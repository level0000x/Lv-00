/**
 * @file adaptive_threshold.c
 * @brief 自适应阈值框架 —— 动态阈值计算与启发式剪枝
 *
 * @details 基于问题复杂度的动态阈值计算：
 *   - 约束图复杂度分析（节点数、约束数、密度、连通分量）
 *   - 按算法类型的自适应阈值计算（VF2、Buchberger、重写求解）
 *   - 进度跟踪与回溯率启发式剪枝
 *   - 时间预算超时检测
 */

#include "lv/lv_platform.h"
#include "lv/config.h"

#include "lv/adaptive_threshold.h"
#include "lv/lv.h"
#include "lv/lv_graph_traversal.h"
#include "lv/lv_thread.h"
#include "lv/lv_check.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv_utils.h"

/* ================================================================
 * 默认配置
 * ================================================================ */

#define VF2_BASE_THRESHOLD 100.0
#define VF2_SCALE_FACTOR 1.5
#define VF2_TIME_BUDGET_MS 1000.0
#define VF2_MIN_THRESHOLD 50.0
#define VF2_MAX_THRESHOLD 1000.0

#define BUCHBERGER_BASE_THRESHOLD 20000.0
#define BUCHBERGER_SCALE_FACTOR 2.0
#define BUCHBERGER_TIME_BUDGET_MS 5000.0
#define BUCHBERGER_MIN_THRESHOLD 10000.0
#define BUCHBERGER_MAX_THRESHOLD 200000.0

#define REWRITE_BASE_THRESHOLD 10000.0
#define REWRITE_SCALE_FACTOR 1.2
#define REWRITE_TIME_BUDGET_MS 2000.0
#define REWRITE_MIN_THRESHOLD 5000.0
#define REWRITE_MAX_THRESHOLD 50000.0

/* 回溯率阈值：超过此比例触发剪枝 */
#define BACKTRACK_PRUNE_RATIO 0.8

/**
 * @brief 算法默认配置查找表
 *
 * 使用指定初始化器按 lvAlgorithmType 枚举值对齐，
 * 编译器可校验枚举与配置的对应关系，避免位置数组因枚举插入而错位。
 */
static const lvThresholdConfig kAlgoDefaults[] = {
    [lv_ALGO_VF2_MATCH] = {
        .base_threshold = VF2_BASE_THRESHOLD,
        .scale_factor = VF2_SCALE_FACTOR,
        .time_budget_ms = VF2_TIME_BUDGET_MS,
        .min_threshold = VF2_MIN_THRESHOLD,
        .max_threshold = VF2_MAX_THRESHOLD,
        .enable_time_based = true,
        .enable_progress_tracking = true,
    },
    [lv_ALGO_BUCHBERGER] = {
        .base_threshold = BUCHBERGER_BASE_THRESHOLD,
        .scale_factor = BUCHBERGER_SCALE_FACTOR,
        .time_budget_ms = BUCHBERGER_TIME_BUDGET_MS,
        .min_threshold = BUCHBERGER_MIN_THRESHOLD,
        .max_threshold = BUCHBERGER_MAX_THRESHOLD,
        .enable_time_based = true,
        .enable_progress_tracking = true,
    },
    [lv_ALGO_REWRITE_SOLVE] = {
        .base_threshold = REWRITE_BASE_THRESHOLD,
        .scale_factor = REWRITE_SCALE_FACTOR,
        .time_budget_ms = REWRITE_TIME_BUDGET_MS,
        .min_threshold = REWRITE_MIN_THRESHOLD,
        .max_threshold = REWRITE_MAX_THRESHOLD,
        .enable_time_based = true,
        .enable_progress_tracking = true,
    },
};

/** @brief 算法枚举遍历表（显式声明，供初始化/清理按枚举值遍历，避免位置数组强转） */
static const lvAlgorithmType kAllAlgos[] = {
    lv_ALGO_VF2_MATCH, lv_ALGO_BUCHBERGER, lv_ALGO_REWRITE_SOLVE
};

/* 编译期断言：默认配置表大小与算法枚举一致，防止枚举插入后表错位 */
_Static_assert(lv_ALGO_REWRITE_SOLVE + 1 == (int) lv_ARRAY_SIZE(kAlgoDefaults),
               "kAlgoDefaults 表大小与 lvAlgorithmType 枚举不一致（新增算法需同步扩展默认配置表）");
_Static_assert(lv_ARRAY_SIZE(kAlgoDefaults) == lv_ARRAY_SIZE(kAllAlgos),
               "kAlgoDefaults 与 kAllAlgos 表大小不一致");

/* ================================================================
 * 模块级全局状态
 * ================================================================ */

/** @brief 自适应阈值模块全局状态 */
typedef struct {
    lvThresholdConfig configs[3]; /**< 各算法配置（VF2, Buchberger, Rewrite） */
    bool configs_set[3];          /**< 各算法配置是否已设置 */
    bool initialized;             /**< 是否已初始化 */
    double threshold;             /**< 旧版 API 兼容：默认阈值 */
    int is_adaptive;              /**< 旧版 API 兼容：是否自适应 */
} ThresholdState;

/** @brief 自适应阈值模块全局单例 */
static ThresholdState s_threshold_state = {0};

/** @brief 保护 s_threshold_state 并发读写的互斥锁（惰性初始化，首次加锁时自动完成） */
lv_LAZY_LOCK_DEFINE(s_threshold_lock);

#define THRESHOLD_LOCK() lv_lazy_lock_lock(&s_threshold_lock, s_threshold_lock_init_once)
#define THRESHOLD_UNLOCK() lv_lazy_lock_unlock(&s_threshold_lock)

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/**
 * @brief 连通分量计数回调数据
 *
 * lv_graph_traverse 的回调拿不到"新分量开始"事件，但 visit_all 模式下
 * 每个连通分量的起始节点 depth == 0（其余节点 depth >= 1），
 * 因此统计 depth == 0 的回调次数即为连通分量数。
 */
typedef struct {
    int components; /**< depth==0 回调次数，即连通分量数 */
} ConnectedComponentCounter;

/** @brief 连通分量计数访问器：depth==0 即新分量的起始节点 */
static lvTraversalResult count_components_visitor(GeomNode *node, int depth, void *user_data) {
    (void)node;
    ConnectedComponentCounter *counter = (ConnectedComponentCounter *)user_data;
    if (depth == 0)
        counter->components++;
    return lv_TRAVERSAL_CONTINUE;
}

/**
 * @brief BFS 计算连通分量数
 *
 * 将图中每个约束视为连接其所有参与者的一条超边，
 * 连通分量定义为通过活跃约束可达的节点集合。
 *
 * 复用 lv_graph_traverse（BFS + visit_all + skip_disabled=false）：
 * 与原手写 BFS 语义等价 —— 均以活跃约束为超边、对全部节点（含 disabled）
 * 划分连通分量。visited 语义采用 lv 的按 node->id 索引（get_max_node_id
 * 取 max(node_count, next_node_id)）；节点 id 与数组下标在删除节点或
 * 反序列化（graph_add_node_with_id 指定 ID）后可能错位，但分量划分只依赖
 * 约束参与者关系，与索引方式无关，两种实现计数结果一致。
 */
static int count_connected_components(const ConstraintGraph *graph) {
    if (!graph || graph->node_count == 0)
        return 0;

    /* BFS + visit_all：遍历所有连通分量；
     * skip_disabled=false：lvGraphTraversal 默认跳过 disabled 节点，
     * 原实现不跳过，须在配置中显式关闭。 */
    lvGraphTraversalConfig cfg = lv_GRAPH_TRAVERSAL_DEFAULT_CONFIG;
    cfg.order = lv_TRAVERSAL_BFS;
    cfg.max_depth = 0;
    cfg.visit_all = true;
    cfg.reverse_edges = false;
    cfg.skip_disabled = false;

    ConnectedComponentCounter counter = {0};
    int err = lv_graph_traverse((ConstraintGraph *)graph, count_components_visitor, &counter, &cfg);
    if (err != lv_OK)
        return 0; /* 与原有分配失败返回 0 的行为一致 */
    return counter.components;
}

/* ================================================================
 * 公共 API
 * ================================================================ */

/* exempt: 惰性守卫豁免 —— 自适应阈值为"init→cleanup 可重入"模式：
 * lv_adaptive_threshold_cleanup 将 s_threshold_state.initialized 置 false，
 * 允许再次 init（lv_once 不可重置，转换后 cleanup 无法恢复）；
 * 线程安全已由 THRESHOLD_LOCK（lv_lazy_lock，惰性锁）保证，
 * 且初始化内含运行时配置读取（lv_config_get_double），非纯一次性。
 * L301/L307 的 c->initialized 为对象实例字段（每 ctx 生命周期），
 * 均非 lv_once 适用对象，故保留手写标志检查，不迁移。 */
lvError lv_adaptive_threshold_init(void) {
    THRESHOLD_LOCK();

    if (s_threshold_state.initialized) {
        THRESHOLD_UNLOCK();
        return lv_OK;
    }

    /* 初始化全局默认配置：按算法枚举显式遍历，避免位置数组强转错位 */
    for (size_t i = 0; i < lv_ARRAY_SIZE(kAllAlgos); i++) {
        lvAlgorithmType algo = kAllAlgos[i];
        if (!s_threshold_state.configs_set[algo]) {
            s_threshold_state.configs[algo] = kAlgoDefaults[algo];
            /* Buchberger 时间预算支持运行时配置覆盖（默认 5000ms） */
            if (algo == lv_ALGO_BUCHBERGER) {
                s_threshold_state.configs[algo].time_budget_ms =
                    lv_config_get_double(LV_CFG_BUCHBERGER_TIME_BUDGET_MS, BUCHBERGER_TIME_BUDGET_MS);
            }
        }
    }

    s_threshold_state.initialized = true;
    THRESHOLD_UNLOCK();
    return lv_OK;
}

void lv_adaptive_threshold_cleanup(void) {
    THRESHOLD_LOCK();
    for (size_t i = 0; i < lv_ARRAY_SIZE(kAllAlgos); i++) {
        s_threshold_state.configs_set[kAllAlgos[i]] = false;
    }
    s_threshold_state.initialized = false;
    THRESHOLD_UNLOCK();
}

lvError lv_compute_complexity(const lvConstraintGraph *graph, lvProblemComplexity *complexity) {
    if (!graph || !complexity)
        return lv_ERROR_INVALID_PARAM;

    const ConstraintGraph *g = (const ConstraintGraph *) graph;

    memset(complexity, 0, sizeof(*complexity));

    complexity->node_count = g->node_count;
    complexity->constraint_count = g->constraint_count;

    /* 计算边数：每个约束的每个参与对算一条边 */
    int edge_count = 0;
    for (int i = 0; i < g->constraint_count; i++) {
        const Constraint *c = g->constraints[i];
        if (!c || !c->is_active)
            continue;
        int pc = c->participant_count;
        if (pc >= 2) {
            edge_count += pc * (pc - 1) / 2; /* 完全图中的边数 */
        }
    }
    complexity->edge_count = edge_count;

    /* 密度 = 2*E / (N*(N-1))（无向图） */
    int n = g->node_count;
    if (n > 1) {
        complexity->density = (double) (2 * edge_count) / (double) (n * (n - 1));
        if (complexity->density > 1.0)
            complexity->density = 1.0;
    } else {
        complexity->density = 0.0;
    }

    complexity->connected_components = count_connected_components(g);

    return lv_OK;
}

lvError lv_adaptive_threshold_create(lvAlgorithmType algo, const lvConstraintGraph *graph,
                                     const lvThresholdConfig *config, lvAdaptiveThresholdCtx **ctx) {
    if (!graph || !ctx)
        return lv_ERROR_INVALID_PARAM;

    /* 校验算法类型在默认配置表范围内 */
    lv_CHECK_ENUM(algo, lv_ARRAY_SIZE(kAlgoDefaults));

    /* 自动初始化 */
    lv_adaptive_threshold_init();

    lvAdaptiveThresholdCtx *c = (lvAdaptiveThresholdCtx *) lv_malloc(sizeof(lvAdaptiveThresholdCtx));
    if (!c)
        return lv_ERROR_OUT_OF_MEMORY;

    memset(c, 0, sizeof(*c));
    c->algo = algo;

    /* 捕获开始时间 */
    {
        uint64_t ns = lv_get_time_ns();
        c->start_time.tv_sec = (time_t)(ns / lv_NS_PER_S);
        c->start_time.tv_nsec = (long)(ns % lv_NS_PER_S);
    }

    /* 分析复杂度 */
    lvError err = lv_compute_complexity(graph, &c->complexity);
    if (err != lv_OK) {
        lv_free((void **) &c);
        return err;
    }

    /* 使用传入配置或全局默认 */
    if (config) {
        c->config = *config;
    } else {
        THRESHOLD_LOCK();
        c->config = s_threshold_state.configs[algo];
        THRESHOLD_UNLOCK();
    }

    c->initialized = true;
    *ctx = c;
    return lv_OK;
}

size_t lv_adaptive_threshold_compute(lvAdaptiveThresholdCtx *ctx) {
    if (!ctx || !ctx->initialized)
        return 0;

    double density = ctx->complexity.density;
    int node_count = ctx->complexity.node_count;
    const lvThresholdConfig *cfg = &ctx->config;

    /* 阈值公式：base + node_count * density * scale_factor */
    double threshold = cfg->base_threshold + (double) node_count * density * cfg->scale_factor;

    /* 钳制到 [min, max] 范围 */
    threshold = lv_CLAMP(threshold, cfg->min_threshold, cfg->max_threshold);

    return (size_t) threshold;
}

lvError lv_adaptive_threshold_default_config(lvAlgorithmType algo, lvThresholdConfig *config) {
    lv_CHECK_ENUM(algo, lv_ARRAY_SIZE(kAlgoDefaults));
    if (!config)
        return lv_ERROR_INVALID_PARAM;

    lv_adaptive_threshold_init();

    THRESHOLD_LOCK();
    *config = s_threshold_state.configs[algo];
    THRESHOLD_UNLOCK();
    return lv_OK;
}

void lv_adaptive_threshold_destroy(lvAdaptiveThresholdCtx **ctx) {
    if (!ctx || !*ctx)
        return;
    lv_free((void **) &*ctx);
    *ctx = NULL;
}

void lv_adaptive_threshold_update_progress(lvAdaptiveThresholdCtx *ctx, size_t current, size_t backtrack_count) {
    if (!ctx)
        return;
    ctx->current_progress = current;
    ctx->backtrack_count = backtrack_count;
}

void lv_adaptive_threshold_should_prune(lvAdaptiveThresholdCtx *ctx, bool *should_prune) {
    if (!ctx || !should_prune) {
        if (should_prune)
            *should_prune = false;
        return;
    }

    *should_prune = false;

    /* 基于时间的剪枝 */
    if (ctx->config.enable_time_based) {
        double elapsed_ms = 0.0;
        struct timespec now;
        {
            uint64_t ns = lv_get_time_ns();
            now.tv_sec = (time_t)(ns / lv_NS_PER_S);
            now.tv_nsec = (long)(ns % lv_NS_PER_S);
        }
        elapsed_ms = (double) (now.tv_sec - ctx->start_time.tv_sec) * (double) lv_MS_PER_S +
                     (double) (now.tv_nsec - ctx->start_time.tv_nsec) / (double) lv_NS_PER_MS;
        if (elapsed_ms > ctx->config.time_budget_ms) {
            *should_prune = true;
            return;
        }
    }

    /* 基于回溯率的剪枝 */
    if (ctx->config.enable_progress_tracking && ctx->current_progress > 0 &&
        (double) ctx->backtrack_count / (double) ctx->current_progress > BACKTRACK_PRUNE_RATIO) {
        *should_prune = true;
    }
}

lvError lv_adaptive_threshold_set_global_config(lvAlgorithmType algo, const lvThresholdConfig *config) {
    lv_CHECK_ENUM(algo, lv_ARRAY_SIZE(kAlgoDefaults));
    if (!config)
        return lv_ERROR_INVALID_PARAM;
    THRESHOLD_LOCK();
    s_threshold_state.configs[algo] = *config;
    s_threshold_state.configs_set[algo] = true;
    THRESHOLD_UNLOCK();
    return lv_OK;
}

size_t lv_get_vf2_max_depth(const lvConstraintGraph *graph) {
    lvAdaptiveThresholdCtx *ctx = NULL;
    lvError err = lv_adaptive_threshold_create(lv_ALGO_VF2_MATCH, graph, NULL, &ctx);
    if (err != lv_OK)
        return (size_t) lv_config_current()->engine.vf2_max_depth;

    size_t threshold = lv_adaptive_threshold_compute(ctx);
    lv_adaptive_threshold_destroy(&ctx);
    return threshold;
}

size_t lv_get_buchberger_max_steps(const lvConstraintGraph *graph) {
    lvAdaptiveThresholdCtx *ctx = NULL;
    lvError err = lv_adaptive_threshold_create(lv_ALGO_BUCHBERGER, graph, NULL, &ctx);
    if (err != lv_OK)
        return (size_t) lv_config_current()->engine.buchberger_max_steps;

    size_t threshold = lv_adaptive_threshold_compute(ctx);
    lv_adaptive_threshold_destroy(&ctx);
    return threshold;
}

size_t lv_get_rewrite_solve_max_iterations(const lvConstraintGraph *graph) {
    lvAdaptiveThresholdCtx *ctx = NULL;
    lvError err = lv_adaptive_threshold_create(lv_ALGO_REWRITE_SOLVE, graph, NULL, &ctx);
    if (err != lv_OK)
        return (size_t) lv_config_current()->engine.rewrite_default_max_iterations;

    size_t threshold = lv_adaptive_threshold_compute(ctx);
    lv_adaptive_threshold_destroy(&ctx);
    return threshold;
}

/* ================================================================
 * 旧版兼容 API
 * ================================================================ */

int lv_threshold_is_adaptive(void) {
    THRESHOLD_LOCK();
    int is_adaptive = s_threshold_state.is_adaptive;
    THRESHOLD_UNLOCK();
    return is_adaptive;
}

void lv_set_adaptive_threshold(double value) {
    value = lv_CLAMP(value, 0.0, 1.0);
    THRESHOLD_LOCK();
    s_threshold_state.threshold = value;
    s_threshold_state.is_adaptive = 1;
    THRESHOLD_UNLOCK();
}

double lv_get_adaptive_threshold(void) {
    THRESHOLD_LOCK();
    double threshold = s_threshold_state.threshold;
    THRESHOLD_UNLOCK();
    return threshold;
}
