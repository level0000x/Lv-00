/**
 * @file propagation.c
 * @brief WFC 风格约束传播引擎实现
 *
 * @details 基于 Wave Function Collapse (WFC) 算法的动态约束传播引擎，
 *          用于在几何构造中选择最可能的坐标值。核心组件：
 *
 *          - NodeStateSpace：每个节点的候选坐标状态空间
 *            支持坍缩前候选集的增删与深拷贝
 *          - AC-3 弧相容性（Arc Consistency-3）传播：
 *            利用约束类型（距离/角度/关联/共线等）的相容性断言
 *            迭代剪枝每个节点的候选空间，直至达到不动点
 *          - 熵最小化节点选择：
 *            选择候选坐标数量最少（Shannon 熵最低）的未坍缩节点
 *            优先减少搜索空间
 *          - 坍缩与回溯：
 *            对选定节点随机选择候选坐标（坍缩），
 *            若后续传播失败则回溯到之前的状态快照
 *          - 完整 WFC 求解循环：
 *            AC3 → 熵选择 → 坍缩 → 传播 → 回溯，直至全部变量确定
 *
 *          配置参数（由 lv_config 统一管理）：
 *          - prop_max_iterations: AC-3 最大迭代次数（默认 1000）
 *          - prop_max_backtracks: 最大回溯次数（默认 100）
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 */

#include "lv/propagation.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_config.h"
#include "lv/lv_lifecycle.h"
#include "lv/lv_xmacro.h"

#include "lv/error_codes.h"
#include "lv/lv_utils.h"
#include "lv/lv_internal.h"
#include "lv/lv_numeric.h"

/* ── 运行时配置默认值的边界函数 ── */
int propagation_default_max_iterations(void) {
    return (int) lv_config_current()->propagation.prop_max_iterations;
}
int propagation_default_max_backtracks(void) {
    return (int) lv_config_current()->propagation.prop_max_backtracks;
}
int propagation_wfc_max_collaboration_iterations(void) {
    return (int) lv_config_current()->propagation.prop_max_collaboration_iters;
}

#define MAX_CONSTRAINTS_PER_NODE 128
#define MAX_NEIGHBOR_CONSTRAINTS 64

/** @brief 收集涉及 node_id 的所有活跃约束索引（lvDArray 动态收集，消除定长上限）
 *  @param out 输出数组（函数内先清空）
 *  @return 数量；内存不足返回 -1 */
static int collect_constraints_involving(ConstraintGraph *graph, int node_id, lvDArray *out) {
    lv_darray_clear(out);
    if (!graph)
        return 0;
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || !c->is_active)
            continue;
        for (int j = 0; j < c->participant_count; j++) {
            if (c->participants[j] == node_id) {
                if (lv_darray_push(out, &i) < 0)
                    return -1;
                break;
            }
        }
    }
    return out->count;
}

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/** @brief 创建一个节点状态空间 */
static NodeStateSpace *state_create(int node_id) {
    NodeStateSpace *s = (NodeStateSpace *) lv_calloc(1, sizeof(NodeStateSpace));
    if (!s)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "state_create: calloc failed");
    s->node_id = node_id;
    s->is_collapsed = false;
    s->is_unbounded = false;
    s->collapsed_value = NULL;
    lv_darray_init(&s->candidates_da, sizeof(CoordCandidate));
    return s;
}

/** @brief 销毁节点状态空间 */
static void state_destroy(NodeStateSpace *s) {
    if (!s)
        return;
    if (s->collapsed_value) {
        symbolic_coord_destroy(s->collapsed_value);
        s->collapsed_value = NULL;
    }
    /* 释放候选坐标数组中每个条目的坐标指针 */
    if (s->candidates_da.data && s->candidates_da.count > 0) {
        CoordCandidate *cand = (CoordCandidate *)s->candidates_da.data;
        for (int i = 0; i < s->candidates_da.count; i++) {
            if (cand[i].coord) {
                symbolic_coord_destroy(cand[i].coord);
            }
        }
    }
    lv_darray_free(&s->candidates_da);
    /* 注意：不释放 s 本身，因为它指向 state_spaces 数组的元素 */
}

/** @brief 深拷贝节点状态空间 */
static NodeStateSpace *state_deep_copy(const NodeStateSpace *src) {
    if (!src)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "state_deep_copy: src is NULL");
    NodeStateSpace *dst = (NodeStateSpace *) lv_calloc(1, sizeof(NodeStateSpace));
    if (!dst)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "state_deep_copy: calloc failed");

    dst->node_id = src->node_id;
    dst->is_collapsed = src->is_collapsed;
    dst->is_unbounded = src->is_unbounded;
    lv_darray_init(&dst->candidates_da, sizeof(CoordCandidate));

    if (src->collapsed_value) {
        dst->collapsed_value = symbolic_coord_copy(src->collapsed_value);
    }

    if (src->candidates_da.count > 0 && src->candidates_da.data) {
        CoordCandidate *src_cand = (CoordCandidate *)src->candidates_da.data;
        for (int i = 0; i < src->candidates_da.count; i++) {
            CoordCandidate dc;
            dc.coord = src_cand[i].coord ? symbolic_coord_copy(src_cand[i].coord) : NULL;
            dc.dim = src_cand[i].dim;
            if (lv_darray_push(&dst->candidates_da, &dc) < 0) {
                state_destroy(dst);
                lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "state_deep_copy: lv_darray_push failed");
            }
        }
    }
    return dst;
}

/** @brief 向状态空间添加一个候选坐标 */
static bool state_add_candidate(NodeStateSpace *state, const SymbolicCoord *coord, int dim) {
    if (state->is_collapsed)
        return false;
    CoordCandidate c;
    c.coord = symbolic_coord_copy(coord);
    c.dim = dim;
    return lv_darray_push(&state->candidates_da, &c) >= 0;
}

/** @brief 从状态空间移除指定索引的候选 */
static bool state_remove_at(NodeStateSpace *state, int index) {
    if (!lv_index_in_range(index, state->candidates_da.count))
        return false;
    CoordCandidate *cand = (CoordCandidate *)state->candidates_da.data;
    if (cand[index].coord) {
        symbolic_coord_destroy(cand[index].coord);
    }
    /* 将最后一个元素移到被删除位置 */
    int last = state->candidates_da.count - 1;
    if (index != last) {
        cand[index] = cand[last];
    }
    state->candidates_da.count--;
    return true;
}

/** @brief 检查状态空间中是否已包含某坐标（避免重复）
 *  （坐标相等判定收敛至公共 symbolic_coord_equal；原 static coords_equal
 *   对 NULL 返回 (a==b) 的差异在可达路径上不可见——调用点已保证首参非 NULL，
 *   公共语义"任一 NULL 即不等"行为一致） */
static bool state_contains(const NodeStateSpace *state, const SymbolicCoord *coord) {
    if (!state || !coord)
        return false;
    CoordCandidate *cand = (CoordCandidate *)state->candidates_da.data;
    for (int i = 0; i < state->candidates_da.count; i++) {
        if (cand[i].coord && symbolic_coord_equal(cand[i].coord, coord)) {
            return true;
        }
    }
    return false;
}

/* ── 前向声明（供 VTable handler 使用） ── */
static bool check_incidence_compatible(const SymbolicCoord *point_coord, const GeomNode *point_node,
                                       const GeomNode *line_node);
static bool check_constraint_compatible(const SymbolicCoord *candidate, const Constraint *constraint,
                                        const ConstraintGraph *graph);

/* ================================================================
 * VTable / 查找表模式 — 替代 switch-case 派发
 * ================================================================ */

/* ── Switch 1: 节点类型初始化 handler ── */
typedef void (*InitStateHandler)(PropagationContext *ctx, int node_id, GeomNode *node, NodeStateSpace *ss);

static void init_state_point(PropagationContext *ctx, int node_id, GeomNode *node, NodeStateSpace *ss) {
    (void)ctx;
    (void)node_id;
    /* 点节点：使用当前坐标作为初始状态 */
    if (node->coord_count >= 2 && node->symbolic_coords && node->symbolic_coords[0] &&
        node->symbolic_coords[1]) {
        /* 已有坐标 → 已坍缩 */
        ss->collapsed_value = symbolic_coord_copy(node->symbolic_coords[0]);
        ss->is_collapsed = true;
    } else {
        /* 无坐标 → 检查是否有约束（动态收集，消除 64 上限） */
        lvDArray constraints;
        lv_darray_init(&constraints, sizeof(int));
        int count = collect_constraints_involving(ctx->graph, node_id, &constraints);
        if (count == 0) {
            ss->is_unbounded = true;
        }
        lv_darray_free(&constraints);
        /* 有约束但无坐标 → 由传播引擎后续填充 */
    }
}

static void init_state_line_segment(PropagationContext *ctx, int node_id, GeomNode *node, NodeStateSpace *ss) {
    (void)ctx;
    (void)node_id;
    /* 线段节点：基于端点状态空间 */
    if (node->coord_count >= 4 && node->symbolic_coords) {
        bool all_coords = true;
        for (int j = 0; j < 4; j++) {
            if (!node->symbolic_coords[j]) {
                all_coords = false;
                break;
            }
        }
        if (all_coords) {
            ss->collapsed_value = symbolic_coord_copy(node->symbolic_coords[0]);
            ss->is_collapsed = true;
        }
    }
}

static void init_state_other(PropagationContext *ctx, int node_id, GeomNode *node, NodeStateSpace *ss) {
    (void)ctx;
    (void)node_id;
    (void)node;
    (void)ss;
    /* 其他类型暂不处理状态空间 */
}

/* 查找表：GeomType -> InitStateHandler */
static const InitStateHandler init_state_handlers[] = {
    [GEOM_POINT]         = init_state_point,
    [GEOM_LINE_SEGMENT]  = init_state_line_segment,
    [GEOM_REGION]        = init_state_other,
    [GEOM_CIRCLE]        = init_state_other,
    [GEOM_PORT]          = init_state_other,
    [GEOM_FUNCTION_BLOCK]= init_state_other,
};

/* ── Switch 2: 约束兼容性检查 handler ── */
typedef bool (*CheckConstraintHandler)(const SymbolicCoord *candidate, const Constraint *constraint, const ConstraintGraph *graph);

static bool check_constraint_incidence_handler(const SymbolicCoord *candidate, const Constraint *constraint, const ConstraintGraph *graph) {
    /* 点在线段上：参与者 [point_id, line_id] */
    if (constraint->participant_count < 2)
        return true;
    int point_id = constraint->participants[0];
    int line_id = constraint->participants[1];
    GeomNode *point_node = graph_get_node(graph, point_id);
    GeomNode *line_node = graph_get_node(graph, line_id);
    return check_incidence_compatible(candidate, point_node, line_node);
}

static bool check_constraint_default_handler(const SymbolicCoord *candidate, const Constraint *constraint, const ConstraintGraph *graph) {
    (void)candidate;
    (void)constraint;
    (void)graph;
    /* 其他约束类型暂不进行候选过滤 */
    return true;
}

/* 查找表：ConstraintType -> CheckConstraintHandler */
static const CheckConstraintHandler check_constraint_handlers[] = {
    [INCIDENCE]     = check_constraint_incidence_handler,
    [BETWEENNESS]   = check_constraint_default_handler,
    [INTERSECTION]  = check_constraint_default_handler,
    [CONTAINMENT]   = check_constraint_default_handler,
    [CONNECTION]    = check_constraint_default_handler,
    [ANGLE]         = check_constraint_default_handler,
    [PARALLEL]      = check_constraint_default_handler,
};

/* ── Switch 3: 节点选择策略 handler ── */
typedef int (*SelectNodeStrategyFn)(PropagationContext *ctx);

static int select_by_min_entropy(PropagationContext *ctx) {
    double min_entropy = lv_HUGE_NUMBER;
    int best_node = -1;
    int best_degree = -1;

    for (int i = 0; i < ctx->state_count; i++) {
        NodeStateSpace *ss = &ctx->state_spaces[i];
        if (ss->is_collapsed || ss->is_unbounded)
            continue;
        if (ss->candidates_da.count <= 0)
            continue;

        double entropy = propagation_compute_entropy(ss);
        if (entropy < 0)
            continue;

        lvDArray constraints;
        lv_darray_init(&constraints, sizeof(int));
        int degree = collect_constraints_involving(ctx->graph, i, &constraints);
        if (degree < 0)
            degree = 0; /* OOM：退化为最低度 */
        lv_darray_free(&constraints);

        if (entropy < min_entropy || (entropy == min_entropy && degree > best_degree)) {
            min_entropy = entropy;
            best_node = i;
            best_degree = degree;
        }
    }
    return best_node;
}

static int select_by_degree(PropagationContext *ctx) {
    double min_entropy = lv_HUGE_NUMBER;
    int best_node = -1;
    int best_degree = -1;

    for (int i = 0; i < ctx->state_count; i++) {
        NodeStateSpace *ss = &ctx->state_spaces[i];
        if (ss->is_collapsed || ss->is_unbounded)
            continue;
        if (ss->candidates_da.count <= 0)
            continue;

        double entropy = propagation_compute_entropy(ss);
        if (entropy < 0)
            continue;

        lvDArray constraints;
        lv_darray_init(&constraints, sizeof(int));
        int degree = collect_constraints_involving(ctx->graph, i, &constraints);
        if (degree < 0)
            degree = 0; /* OOM：退化为最低度 */
        lv_darray_free(&constraints);

        if (degree > best_degree || (degree == best_degree && entropy < min_entropy)) {
            best_degree = degree;
            best_node = i;
            min_entropy = entropy;
        }
    }
    return best_node;
}

static int select_by_topological(PropagationContext *ctx) {
    double min_entropy = lv_HUGE_NUMBER;
    int best_node = -1;
    int best_degree = -1;

    for (int i = 0; i < ctx->state_count; i++) {
        NodeStateSpace *ss = &ctx->state_spaces[i];
        if (ss->is_collapsed || ss->is_unbounded)
            continue;
        if (ss->candidates_da.count <= 0)
            continue;

        double entropy = propagation_compute_entropy(ss);
        if (entropy < 0)
            continue;

        /* 拓扑排序策略：按约束依赖关系选择"被依赖最多"的未坍缩节点 */
        int in_degree = 0;
        lvDArray cids_t;
        lv_darray_init(&cids_t, sizeof(int));
        int nc_t = collect_constraints_involving(ctx->graph, i, &cids_t);
        if (nc_t < 0)
            nc_t = 0; /* OOM：按无约束处理 */
        for (int ci = 0; ci < nc_t; ci++) {
            Constraint *cc = graph_get_constraint(ctx->graph, *(const int *)lv_darray_get(&cids_t, ci));
            if (!cc || !cc->is_active)
                continue;
            for (int p = 0; p < cc->participant_count; p++) {
                int other = cc->participants[p];
                if (other != i && other >= 0 && other < ctx->state_count) {
                    const NodeStateSpace *ss_other = &ctx->state_spaces[other];
                    if (!ss_other->is_collapsed) {
                        in_degree++;
                    }
                }
            }
        }
        lv_darray_free(&cids_t);
        if (in_degree > best_degree || (in_degree == best_degree && entropy < min_entropy)) {
            best_degree = in_degree;
            best_node = i;
            min_entropy = entropy;
        }
    }
    return best_node;
}

/* 查找表：PropagationStrategy -> SelectNodeStrategyFn */
static const SelectNodeStrategyFn select_node_strategies[] = {
    [PROP_STRATEGY_MIN_ENTROPY]  = select_by_min_entropy,
    [PROP_STRATEGY_MRVS]         = select_by_min_entropy,  /* MRVS 与最小熵逻辑相同 */
    [PROP_STRATEGY_DEGREE]       = select_by_degree,
    [PROP_STRATEGY_TOPOLOGICAL]  = select_by_topological,
};

/* ── Switch 4: 坍缩策略 handler ── */
typedef int (*CollapseStrategyFn)(PropagationContext *ctx, NodeStateSpace *ss);

static int collapse_first(PropagationContext *ctx, NodeStateSpace *ss) {
    (void)ctx;
    (void)ss;
    return 0;
}

static int collapse_weighted(PropagationContext *ctx, NodeStateSpace *ss) {
    CoordCandidate *cand = (CoordCandidate *)ss->candidates_da.data;
    int selected_index = 0;

    /* 基于约束兼容性的加权随机选择 */
    double *weights = (double *) lv_calloc((size_t) ss->candidates_da.count, sizeof(double));
    if (weights && ss->candidates_da.count > 0) {
        lvDArray cids;
        lv_darray_init(&cids, sizeof(int));
        int nc = collect_constraints_involving(ctx->graph, ss->node_id, &cids);
        if (nc < 0)
            nc = 0; /* OOM：按无约束处理 */
        for (int k = 0; k < ss->candidates_da.count; k++) {
            double w = 1.0;
            for (int ci = 0; ci < nc; ci++) {
                Constraint *c = graph_get_constraint(ctx->graph, *(const int *)lv_darray_get(&cids, ci));
                if (c && c->is_active && check_constraint_compatible(cand[k].coord, c, ctx->graph)) {
                    w += 1.0;
                }
            }
            weights[k] = w;
        }
        lv_darray_free(&cids);
        double total = 0.0;
        for (int k = 0; k < ss->candidates_da.count; k++)
            total += weights[k];
        if (total > 0.0) {
            double r = lv_random_double(0.0, total);
            double accum = 0.0;
            for (int k = 0; k < ss->candidates_da.count; k++) {
                accum += weights[k];
                if (r <= accum) {
                    selected_index = k;
                    break;
                }
            }
        }
        lv_free((void **) &weights);
    }
    return selected_index;
}

/* 查找表：CollapseStrategy -> CollapseStrategyFn */
static const CollapseStrategyFn collapse_strategies[] = {
    [PROP_COLLAPSE_FIRST]    = collapse_first,
    [PROP_COLLAPSE_WEIGHTED] = collapse_weighted,
};

/* ================================================================
 * 传播队列（环形缓冲区）
 * ================================================================ */

static bool queue_init(PropagationContext *ctx) {
    ctx->queue_capacity = PROP_DEFAULT_QUEUE_CAPACITY;
    ctx->propagation_queue = (int *) lv_calloc((size_t) ctx->queue_capacity, sizeof(int));
    if (!ctx->propagation_queue)
        return false;
    ctx->queue_head = 0;
    ctx->queue_tail = 0;
    ctx->queue_size = 0;
    return true;
}

static void queue_destroy(PropagationContext *ctx) {
    lv_free((void **) &ctx->propagation_queue);
    ctx->propagation_queue = NULL;
}

static bool queue_ensure_capacity(PropagationContext *ctx) {
    if (ctx->queue_size < ctx->queue_capacity)
        return true;
    /* 记录旧容量：环形展开依赖旧容量计算段长（lv_ensure_capacity 成功后
     * ctx->queue_capacity 已更新为新值，故需在调用前保存） */
    int old_cap = ctx->queue_capacity;
    if (!lv_ensure_capacity((void **) &ctx->propagation_queue, ctx->queue_size, &ctx->queue_capacity,
                            sizeof(int), 1))
        return false;

    /* 将数据从环形缓冲区展开到线性数组 */
    int *new_q = ctx->propagation_queue;
    if (ctx->queue_tail > ctx->queue_head) {
        memmove(new_q, new_q + ctx->queue_head, (size_t) ctx->queue_size * sizeof(int));
    } else if (ctx->queue_tail < ctx->queue_head) {
        /* 两段数据：head..end 和 0..tail */
        int seg1 = old_cap - ctx->queue_head;
        int seg2 = ctx->queue_tail;
        memmove(new_q, new_q + ctx->queue_head, (size_t) seg1 * sizeof(int));
        memmove(new_q + seg1, ctx->propagation_queue, (size_t) seg2 * sizeof(int));
    }
    ctx->queue_head = 0;
    ctx->queue_tail = ctx->queue_size;
    return true;
}

static bool queue_push(PropagationContext *ctx, int value) {
    if (!queue_ensure_capacity(ctx))
        return false;
    ctx->propagation_queue[ctx->queue_tail] = value;
    ctx->queue_tail = (ctx->queue_tail + 1) % ctx->queue_capacity;
    ctx->queue_size++;
    return true;
}

static bool queue_pop(PropagationContext *ctx, int *out_value) {
    if (ctx->queue_size == 0)
        return false;
    *out_value = ctx->propagation_queue[ctx->queue_head];
    ctx->queue_head = (ctx->queue_head + 1) % ctx->queue_capacity;
    ctx->queue_size--;
    return true;
}

static void queue_clear(PropagationContext *ctx) {
    ctx->queue_head = 0;
    ctx->queue_tail = 0;
    ctx->queue_size = 0;
}

/* ================================================================
 * 生命周期管理
 * ================================================================ */

PropagationContext *propagation_context_create(ConstraintGraph *graph) {
    if (!graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "propagation_context_create: graph is NULL");

    PropagationContext *ctx = (PropagationContext *) lv_malloc(sizeof(PropagationContext));
    if (!ctx)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "propagation_context_create: malloc failed");
    memset(ctx, 0, sizeof(PropagationContext));

    ctx->graph = graph;
    ctx->strategy = PROP_STRATEGY_MIN_ENTROPY;
    ctx->collapse_strategy = PROP_COLLAPSE_FIRST;
    ctx->max_iterations = propagation_default_max_iterations();
    ctx->max_backtracks = propagation_default_max_backtracks();

    /* 初始化状态空间数组 */
    ctx->state_count = graph->node_count;
    ctx->state_spaces = (NodeStateSpace *) lv_malloc((size_t) ctx->state_count * sizeof(NodeStateSpace));
    if (!ctx->state_spaces && ctx->state_count > 0) {
        lv_free((void **) &ctx);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "propagation_context_create: state_spaces malloc failed");
    }
    if (ctx->state_spaces)
        memset(ctx->state_spaces, 0, (size_t) ctx->state_count * sizeof(NodeStateSpace));

    /* 初始化传播队列 */
    if (!queue_init(ctx)) {
        lv_free((void **) &ctx->state_spaces);
        lv_free((void **) &ctx);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "propagation_context_create: queue_init failed");
    }

    /* 初始化快照栈 */
    ctx->snapshot_capacity = PROP_DEFAULT_SNAPSHOT_CAPACITY;
    ctx->snapshot_stack =
        (PropagationSnapshot **) lv_malloc((size_t) ctx->snapshot_capacity * sizeof(PropagationSnapshot *));
    if (!ctx->snapshot_stack) {
        queue_destroy(ctx);
        lv_free((void **) &ctx->state_spaces);
        lv_free((void **) &ctx);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "propagation_context_create: snapshot_stack malloc failed");
    }
    memset(ctx->snapshot_stack, 0, (size_t) ctx->snapshot_capacity * sizeof(PropagationSnapshot *));

    return ctx;
}

void propagation_context_destroy(PropagationContext *ctx) {
    if (!ctx)
        return;

    /* 销毁状态空间 */
    for (int i = 0; i < ctx->state_count; i++) {
        state_destroy(&ctx->state_spaces[i]);
    }
    lv_free((void **) &ctx->state_spaces);

    /* 销毁传播队列 */
    queue_destroy(ctx);

    /* 销毁快照栈 */
    for (int i = 0; i < ctx->snapshot_count; i++) {
        if (ctx->snapshot_stack[i]) {
            propagation_snapshot_destroy(ctx->snapshot_stack[i]);
        }
    }
    lv_free((void **) &ctx->snapshot_stack);

    lv_free((void **) &ctx);
}

/* ================================================================
 * 状态空间初始化
 * ================================================================ */

PropagationResult propagation_init_state_spaces(PropagationContext *ctx) {
    if (!ctx || !ctx->graph)
        return PROP_RESULT_CONTRADICTION;

    for (int i = 0; i < ctx->state_count; i++) {
        state_destroy(&ctx->state_spaces[i]);
        memset(&ctx->state_spaces[i], 0, sizeof(NodeStateSpace));
        ctx->state_spaces[i].node_id = i;
    }

    for (int i = 0; i < ctx->graph->node_count; i++) {
        GeomNode *node = graph_get_node(ctx->graph, i);
        if (!node)
            continue;
        if (!node->is_active)
            continue;

        NodeStateSpace *ss = &ctx->state_spaces[i];

        LV_DISPATCH_VOID(init_state_handlers, node->type, ctx, i, node, ss);
    }

    return PROP_RESULT_CONSISTENT;
}

NodeStateSpace *propagation_get_state_space(PropagationContext *ctx, int node_id) {
    if (!ctx || !lv_index_in_range(node_id, ctx->state_count))
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "propagation_get_state_space: invalid ctx or node_id");
    return &ctx->state_spaces[node_id];
}

/* ================================================================
 * AC-3 弧相容性传播
 * ================================================================ */

/**
 * @brief 检查候选坐标是否满足关联约束
 *
 * 对于 INCIDENCE（点在线段上）约束：
 *   计算行列式 (Px-Ax)*(By-Ay) - (Py-Ay)*(Bx-Ax) 是否为零
 *
 * @param point_coord   点节点的 x 坐标（SymbolicCoord）
 * @param point_node    点节点（用于获取 y 坐标）
 * @param line_node     线段节点（含端点坐标）
 */
static bool check_incidence_compatible(const SymbolicCoord *point_coord, const GeomNode *point_node,
                                       const GeomNode *line_node) {
    if (!point_coord || !line_node || !line_node->symbolic_coords)
        return false;
    if (line_node->coord_count < 4)
        return false;

    /* 线段端点 A(x0,y0), B(x1,y1) */
    const SymbolicCoord *ax = line_node->symbolic_coords[0];
    const SymbolicCoord *ay = line_node->symbolic_coords[1];
    const SymbolicCoord *bx = line_node->symbolic_coords[2];
    const SymbolicCoord *by = line_node->symbolic_coords[3];
    if (!ax || !ay || !bx || !by)
        return false;

    /* 点 P(px, py) — 使用 point_coord 的 x,y 分量 */
    /* 使用 double 近似检查行列式（符号坐标的数值评估） */
    double px = symbolic_coord_to_double(point_coord);
    double ax_d = symbolic_coord_to_double(ax);
    double ay_d = symbolic_coord_to_double(ay);
    double bx_d = symbolic_coord_to_double(bx);
    double by_d = symbolic_coord_to_double(by);

    /* 正确计算行列式：|PA × PB| = (Px-Ax)*(By-Ay) - (Py-Ay)*(Bx-Ax)
     * 这里 point_coord 是单值，需要分别获取 x 和 y */
    double py = 0.0; /* 若 point_coord 只有 x，y 默认为 0 */
    /* 尝试从 point_node 获取 y 坐标 */
    if (point_node && point_node->coord_count >= 2) {
        py = symbolic_coord_to_double(point_node->symbolic_coords[1]);
    }
    double det = (px - ax_d) * (by_d - ay_d) - (py - ay_d) * (bx_d - ax_d);
    /* 使用相对容差判断点是否在线段上。
     * 行列式量级正比于坐标乘积 O(coord²)，对于大坐标（如 1e6），
     * 绝对容差 1e-9 过于严格，会误判实际上在线上的点。 */
    double max_coord = fmax(fmax(fabs(px), fabs(py)), fmax(fmax(fabs(ax_d), fabs(ay_d)), fmax(fabs(bx_d), fabs(by_d))));
    double tol = lv_rel_tol_scale(lv_GEO_COLLINEAR_EPSILON, max_coord * max_coord);
    return (fabs(det) < tol);
}

/**
 * @brief 检查候选坐标是否满足约束
 *
 * 根据约束类型分派到具体的兼容性检查。
 */
static bool check_constraint_compatible(const SymbolicCoord *candidate, const Constraint *constraint,
                                        const ConstraintGraph *graph) {
    if (!candidate || !constraint || !graph)
        return false;

    return LV_DISPATCH(check_constraint_handlers, constraint->type, true, candidate, constraint, graph);
}

bool propagation_arc_reduce(PropagationContext *ctx, int constraint_id) {
    if (!ctx || !ctx->graph)
        return false;

    Constraint *c = graph_get_constraint(ctx->graph, constraint_id);
    if (!c || !c->is_active)
        return false;

    bool changed = false;

    /* 对每个参与者执行弧相容性检查 */
    for (int p = 0; p < c->participant_count; p++) {
        int node_id = c->participants[p];
        if (!lv_index_in_range(node_id, ctx->state_count))
            continue;

        NodeStateSpace *ss = &ctx->state_spaces[node_id];
        if (ss->is_collapsed || ss->is_unbounded || ss->candidates_da.count == 0)
            continue;

        /* 检查每个候选是否与约束兼容 */
        int i = 0;
        CoordCandidate *cand = (CoordCandidate *)ss->candidates_da.data;
        while (i < ss->candidates_da.count) {
            SymbolicCoord *candidate = cand[i].coord;
            if (!candidate || check_constraint_compatible(candidate, c, ctx->graph)) {
                i++; /* 兼容，保留 */
            } else {
                state_remove_at(ss, i);
                changed = true;
                ctx->prune_count++;
            }
        }

        /* 若状态空间为空 → 矛盾 */
        if (ss->candidates_da.count == 0 && !ss->is_unbounded) {
            return true; /* changed = true 表示有问题 */
        }

        /* 若状态空间收缩为单一值 → 自动坍缩 */
        if (ss->candidates_da.count == 1 && !ss->is_collapsed) {
            CoordCandidate *cand_single = (CoordCandidate *)ss->candidates_da.data;
            ss->collapsed_value = symbolic_coord_copy(cand_single[0].coord);
            ss->is_collapsed = true;
            changed = true;
        }
    }

    return changed;
}

PropagationResult propagation_run(PropagationContext *ctx) {
    if (!ctx || !ctx->graph)
        return PROP_RESULT_CONTRADICTION;

    queue_clear(ctx);

    /* 将所有活跃约束的参与者加入传播队列 */
    for (int i = 0; i < ctx->graph->constraint_count; i++) {
        Constraint *c = ctx->graph->constraints[i];
        if (!c || !c->is_active)
            continue;
        for (int p = 0; p < c->participant_count; p++) {
            queue_push(ctx, c->participants[p]);
        }
    }

    int iterations = 0;

    while (ctx->queue_size > 0 && iterations < ctx->max_iterations) {
        int node_id;
        if (!queue_pop(ctx, &node_id))
            break;

        /* 查找涉及该节点的所有约束（动态收集，消除 128 上限） */
        lvDArray constraint_ids;
        lv_darray_init(&constraint_ids, sizeof(int));
        int count = collect_constraints_involving(ctx->graph, node_id, &constraint_ids);
        if (count < 0) {
            lv_darray_free(&constraint_ids);
            return PROP_RESULT_CONTRADICTION; /* OOM：保守终止传播 */
        }

        for (int i = 0; i < count; i++) {
            int cid = *(const int *)lv_darray_get(&constraint_ids, i);
            bool changed = propagation_arc_reduce(ctx, cid);
            ctx->propagation_steps++;

            if (changed) {
                /* 检查是否有节点状态空间为空 */
                for (int j = 0; j < ctx->state_count; j++) {
                    NodeStateSpace *ss = &ctx->state_spaces[j];
                    if (!ss->is_unbounded && ss->candidates_da.count == 0 && !ss->is_collapsed) {
                        lv_darray_free(&constraint_ids);
                        return PROP_RESULT_CONTRADICTION;
                    }
                }

                /* 将受影响的邻域节点重新入队 */
                Constraint *c = graph_get_constraint(ctx->graph, cid);
                if (c && c->is_active) {
                    for (int p = 0; p < c->participant_count; p++) {
                        if (c->participants[p] != node_id) {
                            queue_push(ctx, c->participants[p]);
                        }
                    }
                }
            }
        }
        lv_darray_free(&constraint_ids);

        iterations++;
    }

    if (iterations >= ctx->max_iterations) {
        return PROP_RESULT_TIMEOUT;
    }

    /* 检查是否所有节点都已坍缩 */
    if (propagation_is_fully_collapsed(ctx)) {
        return PROP_RESULT_SATISFIED;
    }

    return PROP_RESULT_STABLE;
}

/* ================================================================
 * WFC 节点选择与坍缩
 * ================================================================ */

double propagation_compute_entropy(const NodeStateSpace *state) {
    if (!state)
        return PROP_ENTROPY_UNBOUNDED;
    if (state->is_unbounded)
        return PROP_ENTROPY_UNBOUNDED;
    if (state->is_collapsed)
        return 0.0;
    if (state->candidates_da.count <= 0)
        return PROP_ENTROPY_UNBOUNDED;
    return log2((double) state->candidates_da.count);
}

int propagation_select_node(PropagationContext *ctx) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "propagation_select_node: ctx is NULL");

    return LV_DISPATCH(select_node_strategies, ctx->strategy, -1, ctx);
}

bool propagation_collapse(PropagationContext *ctx, int node_id) {
    if (!ctx || !lv_index_in_range(node_id, ctx->state_count))
        return false;

    NodeStateSpace *ss = &ctx->state_spaces[node_id];
    if (ss->is_collapsed || ss->is_unbounded)
        return false;
    if (ss->candidates_da.count == 0)
        return false;

    CoordCandidate *cand = (CoordCandidate *)ss->candidates_da.data;
    int selected_index = 0;

    selected_index = LV_DISPATCH(collapse_strategies, ctx->collapse_strategy, 0, ctx, ss);

    /* 保存选中的坐标，释放其余 */
    SymbolicCoord *selected = cand[selected_index].coord;

    /* 释放未选中的候选 */
    for (int i = 0; i < ss->candidates_da.count; i++) {
        if (i != selected_index && cand[i].coord) {
            symbolic_coord_destroy(cand[i].coord);
        }
    }

    ss->collapsed_value = selected;
    ss->is_collapsed = true;
    lv_darray_clear(&ss->candidates_da);
    ctx->collapse_count++;

    return true;
}

/* ================================================================
 * 完整 WFC 求解循环
 * ================================================================ */

PropagationResult propagation_wfc_solve(PropagationContext *ctx) {
    if (!ctx || !ctx->graph)
        return PROP_RESULT_CONTRADICTION;

    int wfc_iterations = 0;

    while (wfc_iterations < propagation_wfc_max_collaboration_iterations()) {
        /* 步骤 1: AC-3 约束传播 */
        PropagationResult prop_result = propagation_run(ctx);

        if (prop_result == PROP_RESULT_CONTRADICTION) {
            /* 死路 → 尝试回溯 */
            if (ctx->snapshot_count > 0 && ctx->backtrack_count < ctx->max_backtracks) {
                PropagationSnapshot *snap = ctx->snapshot_stack[ctx->snapshot_count - 1];
                ctx->snapshot_stack[ctx->snapshot_count - 1] = NULL;
                ctx->snapshot_count--;
                propagation_snapshot_restore(ctx, snap);
                ctx->backtrack_count++;
                wfc_iterations++;
                continue;
            }
            return PROP_RESULT_CONTRADICTION;
        }

        if (prop_result == PROP_RESULT_SATISFIED) {
            return PROP_RESULT_SATISFIED;
        }

        if (prop_result == PROP_RESULT_TIMEOUT) {
            return PROP_RESULT_TIMEOUT;
        }

        /* 步骤 2: 选择下一个要坍缩的节点 */
        int node_id = propagation_select_node(ctx);
        if (node_id < 0) {
            /* 没有可坍缩的节点 → 检查是否全部已确定 */
            if (propagation_is_fully_collapsed(ctx)) {
                return PROP_RESULT_SATISFIED;
            }
            /* 剩余节点都是 unbounded → 稳定但未完全确定 */
            return PROP_RESULT_STABLE;
        }

        /* 步骤 3: 保存快照 */
        PropagationSnapshot *snap = propagation_snapshot_save(ctx);
        if (snap) {
            snap->decision_node_id = node_id;
            if (ctx->state_spaces[node_id].candidates_da.count > 0) {
                snap->decision_coord_index = 0;
            }
            /* 压入快照栈 */
            if (ctx->snapshot_count < ctx->snapshot_capacity) {
                ctx->snapshot_stack[ctx->snapshot_count] = snap;
                ctx->snapshot_count++;
            } else {
                propagation_snapshot_destroy(snap);
            }
        }

        /* 步骤 4: 坍缩 */
        if (!propagation_collapse(ctx, node_id)) {
            return PROP_RESULT_CONTRADICTION;
        }

        wfc_iterations++;
    }

    return PROP_RESULT_TIMEOUT;
}

/* ================================================================
 * 快照与回溯
 * ================================================================ */

PropagationSnapshot *propagation_snapshot_save(PropagationContext *ctx) {
    if (!ctx)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "propagation_snapshot_save: ctx is NULL");

    PropagationSnapshot *snap = (PropagationSnapshot *) lv_calloc(1, sizeof(PropagationSnapshot));
    if (!snap)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "propagation_snapshot_save: snap calloc failed");

    snap->state_count = ctx->state_count;
    snap->states = (NodeStateSpace *) lv_calloc((size_t) snap->state_count, sizeof(NodeStateSpace));
    if (!snap->states) {
        lv_free((void **) &snap);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "propagation_snapshot_save: states calloc failed");
    }

    for (int i = 0; i < snap->state_count; i++) {
        snap->states[i] = *(state_deep_copy(&ctx->state_spaces[i]));
    }

    snap->propagation_steps = ctx->propagation_steps;
    snap->collapse_count = ctx->collapse_count;
    snap->backtrack_count = ctx->backtrack_count;
    snap->prune_count = ctx->prune_count;
    snap->decision_node_id = -1;
    snap->decision_coord_index = -1;

    return snap;
}

void propagation_snapshot_restore(PropagationContext *ctx, PropagationSnapshot *snap) {
    if (!ctx || !snap)
        return;

    /* 销毁当前状态空间 */
    for (int i = 0; i < ctx->state_count; i++) {
        state_destroy(&ctx->state_spaces[i]);
    }

    /* 用快照替换 */
    for (int i = 0; i < snap->state_count && i < ctx->state_count; i++) {
        ctx->state_spaces[i] = snap->states[i];
    }

    ctx->propagation_steps = snap->propagation_steps;
    ctx->collapse_count = snap->collapse_count;
    ctx->backtrack_count = snap->backtrack_count;
    ctx->prune_count = snap->prune_count;

    /* 销毁快照壳 */
    lv_free((void **) &snap->states);
    lv_free((void **) &snap);
}

/* propagation_snapshot_destroy 字段描述表：states 为 NodeStateSpace 值数组
 * （非指针数组，不适用 lv_FIELD_ARRAY 的指针数组语义），逐元素 state_destroy
 * 后释放数组；整体释放顺序与原实现一致 */
static void destroy_snapshot_states(void *obj, void *field_ptr) {
    (void) field_ptr;
    PropagationSnapshot *snap = (PropagationSnapshot *) obj;
    if (snap->states) {
        for (int i = 0; i < snap->state_count; i++) {
            state_destroy(&snap->states[i]);
        }
        lv_free((void **) &snap->states);
    }
}

static const lvFieldDesc s_snapshot_destroy_fields[] = {
    lv_FIELD_CUSTOM(PropagationSnapshot, states, destroy_snapshot_states),
};

void propagation_snapshot_destroy(PropagationSnapshot *snap) {
    if (!snap)
        return;
    lv_obj_destroy_fields(snap, s_snapshot_destroy_fields,
                          sizeof(s_snapshot_destroy_fields) / sizeof(s_snapshot_destroy_fields[0]));
    lv_free((void **) &snap);
}

/* ================================================================
 * 配置
 * ================================================================ */

void propagation_set_strategy(PropagationContext *ctx, PropagationStrategy strategy) {
    if (ctx)
        ctx->strategy = strategy;
}

void propagation_set_collapse_strategy(PropagationContext *ctx, CollapseStrategy strategy) {
    if (ctx)
        ctx->collapse_strategy = strategy;
}

void propagation_set_stream_context(PropagationContext *ctx, StreamContext *stream_ctx) {
    if (ctx)
        ctx->stream_ctx = stream_ctx;
}

void propagation_set_max_iterations(PropagationContext *ctx, int max_iterations) {
    if (ctx && max_iterations > 0)
        ctx->max_iterations = max_iterations;
}

void propagation_set_max_backtracks(PropagationContext *ctx, int max_backtracks) {
    if (ctx && max_backtracks >= 0)
        ctx->max_backtracks = max_backtracks;
}

/* ================================================================
 * 诊断与查询
 * ================================================================ */

void propagation_get_statistics(const PropagationContext *ctx, int64_t *out_steps, int64_t *out_collapses,
                                int64_t *out_backtracks, int64_t *out_prunes) {
    if (!ctx)
        return;
    if (out_steps)
        *out_steps = ctx->propagation_steps;
    if (out_collapses)
        *out_collapses = ctx->collapse_count;
    if (out_backtracks)
        *out_backtracks = ctx->backtrack_count;
    if (out_prunes)
        *out_prunes = ctx->prune_count;
}

int propagation_count_uncollapsed(const PropagationContext *ctx) {
    if (!ctx)
        return 0;
    int count = 0;
    for (int i = 0; i < ctx->state_count; i++) {
        const NodeStateSpace *ss = &ctx->state_spaces[i];
        if (!ss->is_collapsed && !ss->is_unbounded && ss->candidates_da.count > 0) {
            count++;
        }
    }
    return count;
}

bool propagation_is_fully_collapsed(const PropagationContext *ctx) {
    if (!ctx)
        return false;
    for (int i = 0; i < ctx->state_count; i++) {
        const NodeStateSpace *ss = &ctx->state_spaces[i];
        GeomNode *node = graph_get_node(ctx->graph, i);
        if (!node || !node->is_active)
            continue;
        if (node->type != GEOM_POINT)
            continue; /* 只关注点节点 */
        if (!ss->is_collapsed && !ss->is_unbounded) {
            return false;
        }
    }
    return true;
}
