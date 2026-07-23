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

#include "lv/adaptive_threshold.h"
#include "lv_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ================================================================
 * 默认配置
 * ================================================================ */

#define VF2_BASE_THRESHOLD         100.0
#define VF2_SCALE_FACTOR           1.5
#define VF2_TIME_BUDGET_MS         1000.0
#define VF2_MIN_THRESHOLD          50.0
#define VF2_MAX_THRESHOLD          1000.0

#define BUCHBERGER_BASE_THRESHOLD  20000.0
#define BUCHBERGER_SCALE_FACTOR    2.0
#define BUCHBERGER_TIME_BUDGET_MS  5000.0
#define BUCHBERGER_MIN_THRESHOLD   10000.0
#define BUCHBERGER_MAX_THRESHOLD   200000.0

#define REWRITE_BASE_THRESHOLD     10000.0
#define REWRITE_SCALE_FACTOR       1.2
#define REWRITE_TIME_BUDGET_MS     2000.0
#define REWRITE_MIN_THRESHOLD      5000.0
#define REWRITE_MAX_THRESHOLD      50000.0

/* 回溯率阈值：超过此比例触发剪枝 */
#define BACKTRACK_PRUNE_RATIO      0.8

/* ================================================================
 * 模块级全局状态
 * ================================================================ */

static lvThresholdConfig g_global_configs[3]; /* VF2, Buchberger, Rewrite */
static bool g_global_configs_set[3] = { false, false, false };
static bool g_initialized = false;

/* 保留旧版 API 兼容 */
static double s_threshold = 0.5;
static int s_is_adaptive = 0;

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/**
 * @brief 获取算法索引
 */
static int algo_index(lvAlgorithmType algo) {
    switch (algo) {
        case lv_ALGO_VF2_MATCH:    return 0;
        case lv_ALGO_BUCHBERGER:   return 1;
        case lv_ALGO_REWRITE_SOLVE: return 2;
        default: return -1;
    }
}

/**
 * @brief BFS 计算连通分量数
 *
 * 将图中每个约束视为连接其所有参与者的一条超边，
 * 连通分量定义为通过约束可达的节点集合。
 */
static int count_connected_components(const ConstraintGraph *graph) {
    if (!graph || graph->node_count == 0) return 0;

    int n = graph->node_count;

    /* 构建 node_id → 索引 映射的快速查找表 */
    /* 使用 graph 的 node_index 作为查找 */
    /* 为每个节点分配一个 visited 标记 */
    int *visited = (int *)lv_calloc((size_t)n, sizeof(int));
    if (!visited) return 0;

    /* 为 BFS 分配队列 */
    int *queue = (int *)lv_malloc((size_t)n * sizeof(int));
    if (!queue) {
        lv_free((void **)&visited);
        return 0;
    }

    int components = 0;

    for (int i = 0; i < n; i++) {
        if (visited[i]) continue;
        components++;

        /* BFS from node i */
        int qhead = 0, qtail = 0;
        queue[qtail++] = i;
        visited[i] = 1;

        while (qhead < qtail) {
            int cur = queue[qhead++];
            int cur_id = graph->nodes[cur]->id;

            /* 遍历所有约束，找包含当前节点的约束 */
            for (int c = 0; c < graph->constraint_count; c++) {
                const Constraint *con = graph->constraints[c];
                if (!con || !con->is_active) continue;

                /* 检查当前节点是否为该约束的参与者 */
                int has_cur = 0;
                for (int p = 0; p < con->participant_count; p++) {
                    if (con->participants[p] == cur_id) {
                        has_cur = 1;
                        break;
                    }
                }
                if (!has_cur) continue;

                /* 将该约束的所有参与者加入队列 */
                for (int p = 0; p < con->participant_count; p++) {
                    int pid = con->participants[p];
                    /* 查找 pid 对应的数组索引 */
                    for (int j = 0; j < n; j++) {
                        if (graph->nodes[j] && graph->nodes[j]->id == pid && !visited[j]) {
                            visited[j] = 1;
                            queue[qtail++] = j;
                            break;
                        }
                    }
                }
            }
        }
    }

    lv_free((void **)&queue);
    lv_free((void **)&visited);
    return components;
}

/* ================================================================
 * 公共 API
 * ================================================================ */

lvError lv_adaptive_threshold_init(void) {
    if (g_initialized) return lv_OK;

    /* 初始化全局默认配置 */
    for (int i = 0; i < 3; i++) {
        if (!g_global_configs_set[i]) {
            lvThresholdConfig *cfg = &g_global_configs[i];
            switch ((lvAlgorithmType)i) {
                case lv_ALGO_VF2_MATCH:
                    cfg->base_threshold    = VF2_BASE_THRESHOLD;
                    cfg->scale_factor      = VF2_SCALE_FACTOR;
                    cfg->time_budget_ms    = VF2_TIME_BUDGET_MS;
                    cfg->min_threshold     = VF2_MIN_THRESHOLD;
                    cfg->max_threshold     = VF2_MAX_THRESHOLD;
                    cfg->enable_time_based = true;
                    cfg->enable_progress_tracking = true;
                    break;
                case lv_ALGO_BUCHBERGER:
                    cfg->base_threshold    = BUCHBERGER_BASE_THRESHOLD;
                    cfg->scale_factor      = BUCHBERGER_SCALE_FACTOR;
                    cfg->time_budget_ms    = BUCHBERGER_TIME_BUDGET_MS;
                    cfg->min_threshold     = BUCHBERGER_MIN_THRESHOLD;
                    cfg->max_threshold     = BUCHBERGER_MAX_THRESHOLD;
                    cfg->enable_time_based = true;
                    cfg->enable_progress_tracking = true;
                    break;
                case lv_ALGO_REWRITE_SOLVE:
                    cfg->base_threshold    = REWRITE_BASE_THRESHOLD;
                    cfg->scale_factor      = REWRITE_SCALE_FACTOR;
                    cfg->time_budget_ms    = REWRITE_TIME_BUDGET_MS;
                    cfg->min_threshold     = REWRITE_MIN_THRESHOLD;
                    cfg->max_threshold     = REWRITE_MAX_THRESHOLD;
                    cfg->enable_time_based = true;
                    cfg->enable_progress_tracking = true;
                    break;
                default:
                    break;
            }
        }
    }

    g_initialized = true;
    return lv_OK;
}

void lv_adaptive_threshold_cleanup(void) {
    for (int i = 0; i < 3; i++) {
        g_global_configs_set[i] = false;
    }
    g_initialized = false;
}

lvError lv_compute_complexity(const lvConstraintGraph *graph,
                                   lvProblemComplexity *complexity) {
    if (!graph || !complexity) return lv_ERROR_INVALID_PARAM;

    const ConstraintGraph *g = (const ConstraintGraph *)graph;

    memset(complexity, 0, sizeof(*complexity));

    complexity->node_count = g->node_count;
    complexity->constraint_count = g->constraint_count;

    /* 计算边数：每个约束的每个参与对算一条边 */
    int edge_count = 0;
    for (int i = 0; i < g->constraint_count; i++) {
        const Constraint *c = g->constraints[i];
        if (!c || !c->is_active) continue;
        int pc = c->participant_count;
        if (pc >= 2) {
            edge_count += pc * (pc - 1) / 2; /* 完全图中的边数 */
        }
    }
    complexity->edge_count = edge_count;

    /* 密度 = 2*E / (N*(N-1))（无向图） */
    int n = g->node_count;
    if (n > 1) {
        complexity->density = (double)(2 * edge_count) / (double)(n * (n - 1));
        if (complexity->density > 1.0) complexity->density = 1.0;
    } else {
        complexity->density = 0.0;
    }

    complexity->connected_components = count_connected_components(g);

    return lv_OK;
}

lvError lv_adaptive_threshold_create(lvAlgorithmType algo,
                                          const lvConstraintGraph *graph,
                                          const lvThresholdConfig *config,
                                          lvAdaptiveThresholdCtx **ctx) {
    if (!graph || !ctx) return lv_ERROR_INVALID_PARAM;

    int idx = algo_index(algo);
    if (idx < 0) return lv_ERROR_INVALID_PARAM;

    /* 自动初始化 */
    lv_adaptive_threshold_init();

    lvAdaptiveThresholdCtx *c = (lvAdaptiveThresholdCtx *)malloc(sizeof(lvAdaptiveThresholdCtx));
    if (!c) return lv_ERROR_OUT_OF_MEMORY;

    memset(c, 0, sizeof(*c));
    c->algo = algo;

    /* 捕获开始时间 */
#ifdef __APPLE__
    clock_gettime(CLOCK_MONOTONIC, &c->start_time);
#elif defined(_WIN32)
    {
        LARGE_INTEGER freq, count;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&count);
        c->start_time.tv_sec = (time_t)(count.QuadPart / freq.QuadPart);
        c->start_time.tv_nsec = (long)((count.QuadPart % freq.QuadPart) * 1000000000LL / freq.QuadPart);
    }
#else
    clock_gettime(CLOCK_MONOTONIC, &c->start_time);
#endif

    /* 分析复杂度 */
    lvError err = lv_compute_complexity(graph, &c->complexity);
    if (err != lv_OK) {
        free(c);
        return err;
    }

    /* 使用传入配置或全局默认 */
    if (config) {
        c->config = *config;
    } else {
        c->config = g_global_configs[idx];
    }

    c->initialized = true;
    *ctx = c;
    return lv_OK;
}

size_t lv_adaptive_threshold_compute(lvAdaptiveThresholdCtx *ctx) {
    if (!ctx || !ctx->initialized) return 0;

    double density = ctx->complexity.density;
    int node_count = ctx->complexity.node_count;
    const lvThresholdConfig *cfg = &ctx->config;

    /* 阈值公式：base + node_count * density * scale_factor */
    double threshold = cfg->base_threshold +
                        (double)node_count * density * cfg->scale_factor;

    /* 钳制到 [min, max] 范围 */
    if (threshold < cfg->min_threshold) threshold = cfg->min_threshold;
    if (threshold > cfg->max_threshold) threshold = cfg->max_threshold;

    return (size_t)threshold;
}

lvError lv_adaptive_threshold_default_config(lvAlgorithmType algo,
                                                   lvThresholdConfig *config) {
    int idx = algo_index(algo);
    if (idx < 0 || !config) return lv_ERROR_INVALID_PARAM;

    lv_adaptive_threshold_init();

    *config = g_global_configs[idx];
    return lv_OK;
}

void lv_adaptive_threshold_destroy(lvAdaptiveThresholdCtx **ctx) {
    if (!ctx || !*ctx) return;
    lv_free((void **)&*ctx);
    *ctx = NULL;
}

void lv_adaptive_threshold_update_progress(lvAdaptiveThresholdCtx *ctx,
                                               size_t current,
                                               size_t backtrack_count) {
    if (!ctx) return;
    ctx->current_progress = current;
    ctx->backtrack_count = backtrack_count;
}

void lv_adaptive_threshold_should_prune(lvAdaptiveThresholdCtx *ctx,
                                            bool *should_prune) {
    if (!ctx || !should_prune) {
        if (should_prune) *should_prune = false;
        return;
    }

    *should_prune = false;

    /* 基于时间的剪枝 */
    if (ctx->config.enable_time_based) {
        double elapsed_ms = 0.0;
#ifdef __APPLE__
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
#elif defined(_WIN32)
        struct timespec now;
        {
            LARGE_INTEGER freq, count;
            QueryPerformanceFrequency(&freq);
            QueryPerformanceCounter(&count);
            now.tv_sec = (time_t)(count.QuadPart / freq.QuadPart);
            now.tv_nsec = (long)((count.QuadPart % freq.QuadPart) * 1000000000LL / freq.QuadPart);
        }
#else
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
#endif
        elapsed_ms = (double)(now.tv_sec - ctx->start_time.tv_sec) * 1000.0 +
                     (double)(now.tv_nsec - ctx->start_time.tv_nsec) / 1000000.0;
        if (elapsed_ms > ctx->config.time_budget_ms) {
            *should_prune = true;
            return;
        }
    }

    /* 基于回溯率的剪枝 */
    if (ctx->config.enable_progress_tracking &&
        ctx->current_progress > 0 &&
        (double)ctx->backtrack_count / (double)ctx->current_progress > BACKTRACK_PRUNE_RATIO) {
        *should_prune = true;
    }
}

lvError lv_adaptive_threshold_set_global_config(lvAlgorithmType algo,
                                                      const lvThresholdConfig *config) {
    int idx = algo_index(algo);
    if (idx < 0 || !config) return lv_ERROR_INVALID_PARAM;
    g_global_configs[idx] = *config;
    g_global_configs_set[idx] = true;
    return lv_OK;
}

size_t lv_get_vf2_max_depth(const lvConstraintGraph *graph) {
    lvAdaptiveThresholdCtx *ctx = NULL;
    lvError err = lv_adaptive_threshold_create(lv_ALGO_VF2_MATCH, graph, NULL, &ctx);
    if (err != lv_OK) return 100; /* fallback */

    size_t threshold = lv_adaptive_threshold_compute(ctx);
    lv_adaptive_threshold_destroy(&ctx);
    return threshold;
}

size_t lv_get_buchberger_max_steps(const lvConstraintGraph *graph) {
    lvAdaptiveThresholdCtx *ctx = NULL;
    lvError err = lv_adaptive_threshold_create(lv_ALGO_BUCHBERGER, graph, NULL, &ctx);
    if (err != lv_OK) return 20000; /* fallback */

    size_t threshold = lv_adaptive_threshold_compute(ctx);
    lv_adaptive_threshold_destroy(&ctx);
    return threshold;
}

size_t lv_get_rewrite_solve_max_iterations(const lvConstraintGraph *graph) {
    lvAdaptiveThresholdCtx *ctx = NULL;
    lvError err = lv_adaptive_threshold_create(lv_ALGO_REWRITE_SOLVE, graph, NULL, &ctx);
    if (err != lv_OK) return 10000; /* fallback */

    size_t threshold = lv_adaptive_threshold_compute(ctx);
    lv_adaptive_threshold_destroy(&ctx);
    return threshold;
}

/* ================================================================
 * 旧版兼容 API
 * ================================================================ */

int lv_threshold_is_adaptive(void) {
    return s_is_adaptive;
}

void lv_set_adaptive_threshold(double value) {
    if (value < 0.0) value = 0.0;
    if (value > 1.0) value = 1.0;
    s_threshold = value;
    s_is_adaptive = 1;
}

double lv_get_adaptive_threshold(void) {
    return s_threshold;
}
