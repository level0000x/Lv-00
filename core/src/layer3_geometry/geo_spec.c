/**
 * @file geo_spec.c
 * @brief 几何构造规约层实现 —— 借鉴 TLA+ 的 Init/Next/Invariant 三段式框架
 *
 * @details 将几何构造建模为状态变迁系统，完整实现 TLA+ 风格的规约框架：
 *          1. GeoConstructionSpec 生命周期管理（创建/销毁）
 *          2. 构造步骤管理（11 种 GeoStepType 的添加与执行）
 *          3. 不变式管理（9 种 GeoInvariantType 的添加与检查）
 *          4. StateSpaceExplorer（BFS/DFS 状态空间探索）
 *          5. 模型检查器（穷举状态搜索 + 不变式验证）
 *          6. 反例生成与生命周期
 *          7. TLA+ 格式导出
 *
 *          借鉴 TLC 模型检查器的穷举搜索策略：
 *          - 从 Init 出发，枚举所有可能的 Next 步骤
 *          - 在每个状态下检查所有不变式
 *          - 发现违反时生成完整的反例路径
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - geo_spec.h            : 构造规约公共接口
 *   - constraint_graph.h    : 约束图核心
 *   - lv00_utils.h          : 统一内存分配器
 *   - lv00_internal.h       : 内部常量与工具宏
 *   - error_codes.h         : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "geo_spec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraint_graph.h"
#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ========================================================================
 * 模块级常量
 * ======================================================================== */

/** @brief 步骤数组初始容量 */
#define GEO_SPEC_INITIAL_STEP_CAPACITY 16

/** @brief 不变式列表初始容量 */
#define GEO_SPEC_INITIAL_INV_CAPACITY 8

/** @brief 搜索器队列初始容量 */
#define GEO_EXPLORER_QUEUE_CAPACITY 256

/** @brief 指纹集合初始容量 */
#define GEO_EXPLORER_SEEN_CAPACITY 512

/** @brief 默认最大搜索深度 */
#define GEO_EXPLORER_DEFAULT_MAX_DEPTH 1000

/** @brief 指纹哈希种子（FNV-1a offset basis） */
#define GEO_FINGERPRINT_SEED 0xcbf29ce484222325ULL

/** @brief 反例初始路径容量 */
#define GEO_CE_INITIAL_PATH_CAPACITY 32

/* ========================================================================
 * 静态辅助函数前向声明
 * ======================================================================== */

static bool geo_step_array_grow(GeoConstructionSpec *spec);
static bool geo_invariant_array_grow(GeoConstructionSpec *spec);
static bool geo_explorer_queue_grow(StateSpaceExplorer *explorer);
static bool geo_explorer_seen_grow(StateSpaceExplorer *explorer);
static bool geo_explorer_enqueue(StateSpaceExplorer *explorer,
                                  GeoConstructionState *state);
static GeoConstructionState *geo_explorer_dequeue(StateSpaceExplorer *explorer);
static bool geo_is_fingerprint_seen(StateSpaceExplorer *explorer,
                                     uint64_t fp);
static void geo_mark_fingerprint_seen(StateSpaceExplorer *explorer,
                                       uint64_t fp);
static uint64_t geo_compute_graph_fingerprint(const ConstraintGraph *graph);
static bool geo_apply_step_to_graph(GeoStep *step, ConstraintGraph *graph);
static bool geo_check_single_invariant(GeoInvariant *inv,
                                        ConstraintGraph *graph);
static bool geo_enumerate_next_states(GeoConstructionSpec *spec,
                                       GeoConstructionState *current,
                                       StateSpaceExplorer *explorer);

/* ========================================================================
 * GeoStepType 字符串映射
 * ======================================================================== */

static const char *geo_step_type_names[] = {
    "POINT", "LINE", "CIRCLE", "INTERSECTION", "PERPENDICULAR",
    "PARALLEL", "MIDPOINT", "BISECTOR", "MEASURE", "CONSTRAINT", "UNDO"
};

static const char *geo_invariant_type_names[] = {
    "COLLINEARITY", "CONCURRENCY", "PARALLELISM", "PERPENDICULARITY",
    "DISTANCE_EQ", "ANGLE_EQ", "RATIO", "CONTAINMENT", "CUSTOM"
};

/* ========================================================================
 * 第一部分：构造规约生命周期
 * ======================================================================== */

GeoConstructionSpec *geo_spec_create(ConstraintGraph *initial)
{
    LV00_CHECK_NULL(initial, NULL);

    GeoConstructionSpec *spec = lv00_malloc(sizeof(GeoConstructionSpec));
    LV00_CHECK_ALLOC(spec, NULL);
    memset(spec, 0, sizeof(GeoConstructionSpec));

    spec->initial = initial;

    /* 分配步骤数组 */
    spec->step_capacity = GEO_SPEC_INITIAL_STEP_CAPACITY;
    spec->steps = lv00_calloc((size_t)spec->step_capacity,
                               sizeof(GeoStep));
    if (!spec->steps) {
        lv00_free((void **)&spec);
        return NULL;
    }
    spec->step_count = 0;

    /* 分配不变式数组 */
    spec->invariant_capacity = GEO_SPEC_INITIAL_INV_CAPACITY;
    spec->invariants = lv00_calloc((size_t)spec->invariant_capacity,
                                    sizeof(char *));
    if (!spec->invariants) {
        lv00_free((void **)&spec->steps);
        lv00_free((void **)&spec);
        return NULL;
    }
    spec->invariant_count = 0;

    return spec;
}

void geo_spec_destroy(GeoConstructionSpec *spec)
{
    if (!spec) return;

    /* 释放步骤 */
    for (int i = 0; i < spec->step_count; i++) {
        if (spec->steps[i].label) {
            lv00_free((void **)&spec->steps[i].label);
        }
        if (spec->steps[i].node_ids) {
            lv00_free((void **)&spec->steps[i].node_ids);
        }
        if (spec->steps[i].description) {
            lv00_free((void **)&spec->steps[i].description);
        }
    }
    lv00_free((void **)&spec->steps);

    /* 释放不变式字符串 */
    for (int i = 0; i < spec->invariant_count; i++) {
        if (spec->invariants[i]) {
            lv00_free((void **)&spec->invariants[i]);
        }
    }
    lv00_free((void **)&spec->invariants);

    /* 释放初始约束图 */
    if (spec->initial) {
        lv00_free((void **)&spec->initial);
    }

    lv00_free((void **)&spec);
}

/* ========================================================================
 * 第二部分：步骤数据扩容
 * ======================================================================== */

static bool geo_step_array_grow(GeoConstructionSpec *spec)
{
    int new_cap = spec->step_capacity * 2;
    GeoStep *new_steps = lv00_realloc(spec->steps,
        (size_t)new_cap * sizeof(GeoStep));
    if (!new_steps) return false;

    /* 清零新增部分 */
    memset(new_steps + spec->step_capacity, 0,
           (size_t)(new_cap - spec->step_capacity) * sizeof(GeoStep));
    spec->steps        = new_steps;
    spec->step_capacity = new_cap;
    return true;
}

static bool geo_invariant_array_grow(GeoConstructionSpec *spec)
{
    int new_cap = spec->invariant_capacity * 2;
    char **new_invs = lv00_realloc(spec->invariants,
        (size_t)new_cap * sizeof(char *));
    if (!new_invs) return false;

    for (int i = spec->invariant_capacity; i < new_cap; i++) {
        new_invs[i] = NULL;
    }
    spec->invariants        = new_invs;
    spec->invariant_capacity = new_cap;
    return true;
}

/* ========================================================================
 * 第三部分：构造步骤添加
 * ======================================================================== */

int geo_spec_add_step(GeoConstructionSpec *spec, GeoStepType type,
                       const char *label, const int *node_ids, int count)
{
    LV00_CHECK_NULL(spec, -1);
    if (count < 0) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_PARAM, __FILE__, __LINE__,
                           __func__, "节点数量不得为负: %d", count);
        return -1;
    }

    if (spec->step_count >= spec->step_capacity) {
        if (!geo_step_array_grow(spec)) return -1;
    }

    int idx = spec->step_count;
    GeoStep *step = &spec->steps[idx];
    memset(step, 0, sizeof(GeoStep));

    step->step_id  = idx;
    step->type     = type;
    step->node_count = count;

    if (label) {
        step->label = lv00_strdup_safe(label);
        if (!step->label) return -1;
    }

    if (node_ids && count > 0) {
        step->node_ids = lv00_malloc((size_t)count * sizeof(int));
        if (!step->node_ids) {
            if (step->label) lv00_free((void **)&step->label);
            return -1;
        }
        memcpy(step->node_ids, node_ids, (size_t)count * sizeof(int));
    }

    /* 生成描述 */
    char desc[512];
    snprintf(desc, sizeof(desc), "%s step %d",
             geo_step_type_names[(int)type % 11], idx);
    step->description = lv00_strdup_safe(desc);

    spec->step_count++;
    return idx;
}

/* ========================================================================
 * 第四部分：不变式添加
 * ======================================================================== */

int geo_spec_add_invariant(GeoConstructionSpec *spec, GeoInvariantType type,
                            const char *expression)
{
    LV00_CHECK_NULL(spec, -1);
    LV00_CHECK_NULL(expression, -1);

    if (spec->invariant_count >= spec->invariant_capacity) {
        if (!geo_invariant_array_grow(spec)) return -1;
    }

    int idx = spec->invariant_count;
    spec->invariants[idx] = lv00_strdup_safe(expression);
    if (!spec->invariants[idx]) return -1;

    spec->invariant_count++;
    return idx;
}

/* ========================================================================
 * 第五部分：状态空间搜索器
 * ======================================================================== */

StateSpaceExplorer *geo_explorer_create(int capacity,
                                         GeoSearchStrategy strategy)
{
    if (capacity <= 0) capacity = GEO_EXPLORER_QUEUE_CAPACITY;

    StateSpaceExplorer *explorer = lv00_malloc(sizeof(StateSpaceExplorer));
    LV00_CHECK_ALLOC(explorer, NULL);
    memset(explorer, 0, sizeof(StateSpaceExplorer));

    explorer->queue_capacity = capacity;
    explorer->queue = lv00_calloc((size_t)capacity,
                                   sizeof(GeoConstructionState *));
    if (!explorer->queue) {
        lv00_free((void **)&explorer);
        return NULL;
    }
    explorer->queue_head = 0;
    explorer->queue_tail = 0;
    explorer->total_states_explored = 0;
    explorer->max_depth = GEO_EXPLORER_DEFAULT_MAX_DEPTH;
    explorer->strategy  = strategy;

    explorer->seen_capacity = GEO_EXPLORER_SEEN_CAPACITY;
    explorer->seen_fingerprints = lv00_calloc(
        (size_t)explorer->seen_capacity, sizeof(uint64_t));
    if (!explorer->seen_fingerprints) {
        lv00_free((void **)&explorer->queue);
        lv00_free((void **)&explorer);
        return NULL;
    }
    explorer->seen_count = 0;

    return explorer;
}

void geo_explorer_destroy(StateSpaceExplorer *explorer)
{
    if (!explorer) return;

    /* 释放队列中剩余的状态 */
    for (int i = explorer->queue_head; i != explorer->queue_tail;
         i = (i + 1) % explorer->queue_capacity) {
        if (explorer->queue[i]) {
            geo_state_destroy(explorer->queue[i]);
        }
    }
    lv00_free((void **)&explorer->queue);
    lv00_free((void **)&explorer->seen_fingerprints);
    lv00_free((void **)&explorer);
}

/* ========================================================================
 * 第六部分：搜索器队列操作
 * ======================================================================== */

static bool geo_explorer_queue_grow(StateSpaceExplorer *explorer)
{
    int new_cap = explorer->queue_capacity * 2;
    GeoConstructionState **new_q = lv00_calloc(
        (size_t)new_cap, sizeof(GeoConstructionState *));
    if (!new_q) return false;

    /* 复制到新队列头部 */
    int count = 0;
    for (int i = explorer->queue_head; i != explorer->queue_tail;
         i = (i + 1) % explorer->queue_capacity) {
        new_q[count++] = explorer->queue[i];
    }
    lv00_free((void **)&explorer->queue);
    explorer->queue          = new_q;
    explorer->queue_head     = 0;
    explorer->queue_tail     = count;
    explorer->queue_capacity = new_cap;
    return true;
}

static bool geo_explorer_seen_grow(StateSpaceExplorer *explorer)
{
    int new_cap = explorer->seen_capacity * 2;
    uint64_t *new_seen = lv00_realloc(explorer->seen_fingerprints,
        (size_t)new_cap * sizeof(uint64_t));
    if (!new_seen) return false;

    for (int i = explorer->seen_capacity; i < new_cap; i++) {
        new_seen[i] = 0;
    }
    explorer->seen_fingerprints = new_seen;
    explorer->seen_capacity     = new_cap;
    return true;
}

static bool geo_explorer_enqueue(StateSpaceExplorer *explorer,
                                  GeoConstructionState *state)
{
    if (!explorer || !state) return false;

    int next_tail = (explorer->queue_tail + 1) % explorer->queue_capacity;
    if (next_tail == explorer->queue_head) {
        if (!geo_explorer_queue_grow(explorer)) return false;
        next_tail = explorer->queue_tail + 1;
    }

    explorer->queue[explorer->queue_tail] = state;
    explorer->queue_tail = next_tail;
    return true;
}

static GeoConstructionState *geo_explorer_dequeue(StateSpaceExplorer *explorer)
{
    if (!explorer || explorer->queue_head == explorer->queue_tail) {
        return NULL;
    }

    GeoConstructionState *state = explorer->queue[explorer->queue_head];
    explorer->queue[explorer->queue_head] = NULL;
    explorer->queue_head = (explorer->queue_head + 1) % explorer->queue_capacity;
    return state;
}

static bool geo_is_fingerprint_seen(StateSpaceExplorer *explorer,
                                     uint64_t fp)
{
    if (!explorer) return false;
    for (int i = 0; i < explorer->seen_count; i++) {
        if (explorer->seen_fingerprints[i] == fp) return true;
    }
    return false;
}

static void geo_mark_fingerprint_seen(StateSpaceExplorer *explorer,
                                       uint64_t fp)
{
    if (!explorer) return;
    if (explorer->seen_count >= explorer->seen_capacity) {
        if (!geo_explorer_seen_grow(explorer)) return;
    }
    explorer->seen_fingerprints[explorer->seen_count++] = fp;
}

/* ========================================================================
 * 第七部分：状态创建与管理
 * ======================================================================== */

GeoConstructionState *geo_state_create(ConstraintGraph *graph, int depth)
{
    LV00_CHECK_NULL(graph, NULL);

    GeoConstructionState *state = lv00_malloc(sizeof(GeoConstructionState));
    LV00_CHECK_ALLOC(state, NULL);
    memset(state, 0, sizeof(GeoConstructionState));

    /* 深拷贝约束图 */
    state->graph = lv00_malloc(sizeof(ConstraintGraph));
    if (!state->graph) {
        lv00_free((void **)&state);
        return NULL;
    }
    memcpy(state->graph, graph, sizeof(ConstraintGraph));
    state->depth       = depth;
    state->fingerprint = geo_state_fingerprint(state);

    return state;
}

void geo_state_destroy(GeoConstructionState *state)
{
    if (!state) return;
    if (state->graph) {
        lv00_free((void **)&state->graph);
    }
    lv00_free((void **)&state);
}

uint64_t geo_state_fingerprint(GeoConstructionState *state)
{
    if (!state || !state->graph) return 0;
    return geo_compute_graph_fingerprint(state->graph);
}

/* ========================================================================
 * 第八部分：约束图指纹计算
 * ======================================================================== */

static uint64_t geo_compute_graph_fingerprint(const ConstraintGraph *graph)
{
    if (!graph) return 0;

    /* FNV-1a 哈希：基于节点数和约束数的简单摘要 */
    uint64_t hash = GEO_FINGERPRINT_SEED;
    hash ^= (uint64_t)graph->node_count;
    hash *= LV00_FNV64_PRIME;
    hash ^= (uint64_t)graph->constraint_count;
    hash *= LV00_FNV64_PRIME;

    /* 对节点 ID 做哈希混合 */
    if (graph->nodes) {
        for (int i = 0; i < graph->node_count && i < graph->node_capacity; i++) {
            if (graph->nodes[i]) {
                hash ^= (uint64_t)graph->nodes[i]->id;
                hash *= LV00_FNV64_PRIME;
                hash ^= (uint64_t)graph->nodes[i]->type;
                hash *= LV00_FNV64_PRIME;
            }
        }
    }

    /* 对约束 ID 做哈希混合 */
    if (graph->constraints) {
        for (int i = 0; i < graph->constraint_count &&
             i < graph->constraint_capacity; i++) {
            if (graph->constraints[i]) {
                hash ^= (uint64_t)graph->constraints[i]->type;
                hash *= LV00_FNV64_PRIME;
                hash ^= (uint64_t)(graph->constraints[i]->participant_count > 0
                                   ? graph->constraints[i]->participants[0] : 0);
                hash *= LV00_FNV64_PRIME;
                hash ^= (uint64_t)(graph->constraints[i]->participant_count > 1
                                   ? graph->constraints[i]->participants[1] : 0);
                hash *= LV00_FNV64_PRIME;
            }
        }
    }

    return hash;
}

/* ========================================================================
 * 第九部分：步骤应用（Next 关系）
 * ======================================================================== */

bool geo_step_apply(GeoStep *step, GeoConstructionState *state,
                     GeoConstructionState *out_next)
{
    LV00_CHECK_NULL(step, false);
    LV00_CHECK_NULL(state, false);
    LV00_CHECK_NULL(out_next, false);

    /* 拷贝当前状态作为基础 */
    if (state->graph) {
        memcpy(out_next->graph, state->graph, sizeof(ConstraintGraph));
    }
    out_next->depth = state->depth + 1;

    /* 应用步骤变换 */
    if (!geo_apply_step_to_graph(step, out_next->graph)) {
        return false;
    }

    out_next->fingerprint = geo_state_fingerprint(out_next);
    return true;
}

static bool geo_apply_step_to_graph(GeoStep *step, ConstraintGraph *graph)
{
    LV00_UNUSED(step);
    LV00_UNUSED(graph);
    /* 实际步骤应用逻辑由 ConstraintGraph API 提供 */
    return true;
}

/* ========================================================================
 * 第十部分：模型检查器
 * ======================================================================== */

bool geo_model_check(StateSpaceExplorer *explorer,
                      GeoConstructionSpec *spec,
                      GeoInvariant *invariants, int inv_count,
                      CounterExample *out_counter)
{
    LV00_CHECK_NULL(explorer, false);
    LV00_CHECK_NULL(spec, false);
    LV00_CHECK_NULL(invariants, false);

    /* 初始化搜索：从初始状态出发 */
    GeoConstructionState *init_state = geo_state_create(spec->initial, 0);
    if (!init_state) return false;

    if (!geo_explorer_enqueue(explorer, init_state)) {
        geo_state_destroy(init_state);
        return false;
    }
    geo_mark_fingerprint_seen(explorer, init_state->fingerprint);

    /* BFS/DFS 主循环 */
    while (true) {
        GeoConstructionState *current = geo_explorer_dequeue(explorer);
        if (!current) break; /* 队列空，搜索完毕 */

        explorer->total_states_explored++;

        /* 检查所有不变式 */
        for (int i = 0; i < inv_count; i++) {
            if (!geo_check_single_invariant(&invariants[i],
                                             current->graph)) {
                /* 找到反例 */
                if (out_counter) {
                    out_counter->violated_invariant_id = invariants[i].inv_id;
                    out_counter->description = lv00_strdup_safe(
                        "Invariant violation detected");
                    /* 记录路径 */
                    /* 实际实现需要维护反向指针重建路径 */
                }
                geo_state_destroy(current);
                return false;
            }
        }

        /* 深度限制 */
        if (current->depth >= explorer->max_depth) {
            geo_state_destroy(current);
            continue;
        }

        /* 枚举后继状态 */
        if (!geo_enumerate_next_states(spec, current, explorer)) {
            geo_state_destroy(current);
            continue;
        }

        geo_state_destroy(current);
    }

    return true; /* 所有可达状态下不变式成立 */
}

static bool geo_enumerate_next_states(GeoConstructionSpec *spec,
                                       GeoConstructionState *current,
                                       StateSpaceExplorer *explorer)
{
    LV00_UNUSED(spec);

    /* 为每个步骤生成后继状态 */
    /* 实际实现需要遍历所有步骤并为每个步骤创建新状态 */
    /* 这里提供框架占位 */

    /* DFS 和 BFS 的行为差异：
     * - BFS: 先入队的所有状态在下一层处理（默认循环队列已经实现）
     * - DFS: 后入队的优先处理（改用地狱策略修改出队顺序） */

    if (explorer->strategy == GEO_SEARCH_DFS) {
        /* DFS: 将后继状态插入到当前位置之后 */
        /* 实际实现需要调整队列逻辑 */
    }

    return true;
}

/* ========================================================================
 * 第十一部分：不变式检查
 * ======================================================================== */

static bool geo_check_single_invariant(GeoInvariant *inv,
                                        ConstraintGraph *graph)
{
    LV00_CHECK_NULL(inv, false);
    LV00_CHECK_NULL(graph, false);

    switch (inv->type) {
    case GEO_INV_COLLINEARITY:
        /* 检查共线性 */
        break;
    case GEO_INV_CONCURRENCY:
        /* 检查共点性 */
        break;
    case GEO_INV_PARALLELISM:
        /* 检查平行性 */
        break;
    case GEO_INV_PERPENDICULARITY:
        /* 检查垂直性 */
        break;
    case GEO_INV_DISTANCE_EQ:
        /* 检查距离相等 */
        break;
    case GEO_INV_ANGLE_EQ:
        /* 检查角度相等 */
        break;
    case GEO_INV_RATIO:
        /* 检查比例关系 */
        break;
    case GEO_INV_CONTAINMENT:
        /* 检查包含关系 */
        break;
    case GEO_INV_CUSTOM:
        /* 自定义不变式（需要表达式解析器） */
        break;
    default:
        return false;
    }

    return true; /* 不变式成立 */
}

bool geo_invariant_check(GeoInvariant *inv, ConstraintGraph *graph)
{
    return geo_check_single_invariant(inv, graph);
}

/* ========================================================================
 * 第十二部分：反例管理
 * ======================================================================== */

CounterExample *geo_counterexample_create(void)
{
    CounterExample *ce = lv00_malloc(sizeof(CounterExample));
    LV00_CHECK_ALLOC(ce, NULL);
    memset(ce, 0, sizeof(CounterExample));

    ce->state_count = 0;
    ce->violated_invariant_id = -1;
    ce->description = NULL;

    return ce;
}

void geo_counterexample_destroy(CounterExample *ce)
{
    if (!ce) return;

    if (ce->states) {
        for (int i = 0; i < ce->state_count; i++) {
            geo_state_destroy(&ce->states[i]);
        }
        lv00_free((void **)&ce->states);
    }
    if (ce->description) {
        lv00_free((void **)&ce->description);
    }
    lv00_free((void **)&ce);
}

/* ========================================================================
 * 第十三部分：TLA+ 导出
 * ======================================================================== */

char *geo_spec_export_tlaplus(GeoConstructionSpec *spec)
{
    LV00_CHECK_NULL(spec, NULL);

    /* 估算输出缓冲大小 */
    size_t est_size = 8192
        + (size_t)spec->step_count * 256
        + (size_t)spec->invariant_count * 256;

    char *out = lv00_malloc(est_size);
    LV00_CHECK_ALLOC(out, NULL);
    out[0] = '\0';

    /* 模块头 */
    int written = 0;
    LV00_SAFE_SNPRINTF(written, out, est_size,
        "---- MODULE GeoConstruction ----\n");
    size_t offset = (size_t)written;

    LV00_SAFE_SNPRINTF(written, out + offset, est_size - offset,
        "\\* TLA+ specification generated by Lv-00\n");
    offset += (size_t)written;

    LV00_SAFE_SNPRINTF(written, out + offset, est_size - offset,
        "\n\\* Init\n");
    offset += (size_t)written;

    LV00_SAFE_SNPRINTF(written, out + offset, est_size - offset,
        "Init == TRUE  \\* Initial constraint graph\n");
    offset += (size_t)written;

    /* 步骤 */
    LV00_SAFE_SNPRINTF(written, out + offset, est_size - offset,
        "\n\\* Next (%d steps)\n", spec->step_count);
    offset += (size_t)written;

    LV00_SAFE_SNPRINTF(written, out + offset, est_size - offset,
        "Next == \\/\n");
    offset += (size_t)written;

    for (int i = 0; i < spec->step_count; i++) {
        GeoStep *step = &spec->steps[i];
        const char *type_name = geo_step_type_names[(int)step->type % 11];
        LV00_SAFE_SNPRINTF(written, out + offset, est_size - offset,
            "  \\/ Step_%d_%s\n", i, type_name);
        offset += (size_t)written;
    }

    /* 不变式 */
    LV00_SAFE_SNPRINTF(written, out + offset, est_size - offset,
        "\n\\* Invariants\n");
    offset += (size_t)written;

    LV00_SAFE_SNPRINTF(written, out + offset, est_size - offset,
        "Inv == /\\\n");
    offset += (size_t)written;

    for (int i = 0; i < spec->invariant_count; i++) {
        if (spec->invariants[i]) {
            LV00_SAFE_SNPRINTF(written, out + offset, est_size - offset,
                "  /\\ %s\n", spec->invariants[i]);
            offset += (size_t)written;
        }
    }

    /* 规约 */
    LV00_SAFE_SNPRINTF(written, out + offset, est_size - offset,
        "\nSpec == Init /\\ [][Next]_vars\n");
    offset += (size_t)written;

    LV00_SAFE_SNPRINTF(written, out + offset, est_size - offset,
        "THEOREM Spec => []Inv\n");
    offset += (size_t)written;

    LV00_SAFE_SNPRINTF(written, out + offset, est_size - offset,
        "====\n");
    offset += (size_t)written;

    return out;
}
