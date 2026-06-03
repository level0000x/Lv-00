/**
 * @file adaptive_threshold.c
 * @brief 自适应阈值框架实现
 * 
 * 提供基于问题复杂度的动态阈值计算和启发式剪枝功能
 * 
 * @version 1.0.0
 * @date 2026-05-26
 */

#include "lv00/adaptive_threshold.h"
#include "lv00/context.h"
#include <math.h>
#include <string.h>

/* ==================== 默认配置表 ==================== */

/**
 * @brief 各算法的默认配置参数
 * 
 * 基于经验值和理论分析设定
 */
static const Lv00ThresholdConfig g_default_configs[LV00_ALGO_COUNT] = {
    [LV00_ALGO_VF2_MATCH] = {
        .base_threshold = 100.0,
        .scale_factor = 2.0,
        .time_budget_ms = 5000.0,
        .min_threshold = 50.0,
        .max_threshold = 1000.0,
        .enable_time_based = true,
        .enable_progress_tracking = true
    },
    [LV00_ALGO_BUCHBERGER] = {
        .base_threshold = 50000.0,
        .scale_factor = 1.5,
        .time_budget_ms = 30000.0,
        .min_threshold = 10000.0,
        .max_threshold = 200000.0,
        .enable_time_based = true,
        .enable_progress_tracking = false
    },
    [LV00_ALGO_POLY_REDUCE] = {
        .base_threshold = 10000.0,
        .scale_factor = 1.2,
        .time_budget_ms = 10000.0,
        .min_threshold = 5000.0,
        .max_threshold = 50000.0,
        .enable_time_based = true,
        .enable_progress_tracking = false
    },
    [LV00_ALGO_REWRITE_SOLVE] = {
        .base_threshold = 10000.0,
        .scale_factor = 1.8,
        .time_budget_ms = 20000.0,
        .min_threshold = 5000.0,
        .max_threshold = 50000.0,
        .enable_time_based = true,
        .enable_progress_tracking = true
    },
    [LV00_ALGO_NORMALIZATION] = {
        .base_threshold = 1000.0,
        .scale_factor = 1.3,
        .time_budget_ms = 5000.0,
        .min_threshold = 500.0,
        .max_threshold = 10000.0,
        .enable_time_based = false,
        .enable_progress_tracking = false
    },
    [LV00_ALGO_UNIFICATION] = {
        .base_threshold = 500.0,
        .scale_factor = 1.5,
        .time_budget_ms = 2000.0,
        .min_threshold = 200.0,
        .max_threshold = 5000.0,
        .enable_time_based = true,
        .enable_progress_tracking = false
    }
};

/* 全局配置覆盖表 */
static Lv00ThresholdConfig g_global_configs[LV00_ALGO_COUNT];
static bool g_global_config_set[LV00_ALGO_COUNT] = {false};
static bool g_initialized = false;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 获取当前时间戳（微秒）
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief 计算图密度
 * 
 * 密度 = 2 * |E| / (|V| * (|V| - 1))
 */
static double compute_graph_density(size_t node_count, size_t edge_count) {
    if (node_count <= 1) return 0.0;
    size_t max_edges = node_count * (node_count - 1) / 2;
    if (max_edges == 0) return 0.0;
    return (double)(2 * edge_count) / (double)max_edges;
}

/**
 * @brief 使用并查集计算连通分量数量
 */
static size_t count_connected_components(
    const Lv00ConstraintGraph* graph,
    size_t node_count
) {
    if (node_count == 0) return 0;
    
    /* 简单的并查集实现 */
    size_t* parent = (size_t*)lv00_malloc(node_count * sizeof(size_t));
    if (!parent) return 1; /* 内存失败时保守估计 */
    
    for (size_t i = 0; i < node_count; i++) {
        parent[i] = i;
    }
    
    /* 查找函数（带路径压缩） */
    #define FIND(x) ({ \
        size_t _x = (x); \
        while (parent[_x] != _x) { \
            parent[_x] = parent[parent[_x]]; \
            _x = parent[_x]; \
        } \
        _x; \
    })
    
    /* 合并函数 */
    #define UNION(x, y) do { \
        size_t _px = FIND(x); \
        size_t _py = FIND(y); \
        if (_px != _py) parent[_px] = _py; \
    } while(0)
    
    /* 遍历约束，合并节点 */
    for (size_t i = 0; i < graph->constraint_count; i++) {
        const Lv00Constraint* c = &graph->constraints[i];
        for (size_t j = 1; j < c->participant_count && j < LV00_MAX_CONSTRAINT_PARTICIPANTS; j++) {
            size_t u = (size_t)c->participants[0];
            size_t v = (size_t)c->participants[j];
            if (u < node_count && v < node_count) {
                UNION(u, v);
            }
        }
    }
    
    /* 统计连通分量 */
    size_t components = 0;
    for (size_t i = 0; i < node_count; i++) {
        if (FIND(i) == i) components++;
    }
    
    lv00_free(parent);
    return components;
    
    #undef FIND
    #undef UNION
}

/**
 * @brief 基于复杂度和算法类型计算阈值
 * 
 * 核心公式：threshold = base * f(complexity) * scale_factor
 */
static size_t compute_threshold_formula(
    Lv00AlgorithmType algo_type,
    const Lv00ProblemComplexity* complexity,
    const Lv00ThresholdConfig* config
) {
    double base = config->base_threshold;
    double factor = 1.0;
    
    switch (algo_type) {
        case LV00_ALGO_VF2_MATCH:
            /* VF2: 与节点数和边数相关，使用对数缩放 */
            factor = log2((double)complexity->node_count + 1.0) * 
                     sqrt((double)complexity->edge_count + 1.0);
            break;
            
        case LV00_ALGO_BUCHBERGER:
            /* Buchberger: 与多项式次数和节点数相关，指数级复杂度 */
            factor = pow((double)complexity->max_degree + 1.0, 1.5) * 
                     log2((double)complexity->node_count + 1.0);
            break;
            
        case LV00_ALGO_POLY_REDUCE:
            /* 多项式约化: 与次数线性相关 */
            factor = (double)complexity->max_degree * 
                     sqrt((double)complexity->node_count + 1.0);
            break;
            
        case LV00_ALGO_REWRITE_SOLVE:
            /* 重写-求解: 与约束数量和连通分量相关 */
            factor = log2((double)complexity->constraint_count + 1.0) * 
                     (double)complexity->connected_components * 
                     (1.0 + complexity->density);
            break;
            
        case LV00_ALGO_NORMALIZATION:
            /* 归一化: 与节点数线性相关 */
            factor = (double)complexity->node_count * 
                     log2((double)complexity->connected_components + 1.0);
            break;
            
        case LV00_ALGO_UNIFICATION:
            /* 合一: 与节点数和平均度数相关 */
            factor = sqrt((double)complexity->node_count) * 
                     (double)complexity->avg_node_degree;
            break;
            
        default:
            factor = 1.0;
            break;
    }
    
    /* 应用缩放因子 */
    double threshold = base * factor * config->scale_factor;
    
    /* 应用min/max限制 */
    if (threshold < config->min_threshold) {
        threshold = config->min_threshold;
    }
    if (threshold > config->max_threshold) {
        threshold = config->max_threshold;
    }
    
    return (size_t)threshold;
}

/**
 * @brief 基于时间预算动态调整阈值
 */
static size_t adjust_threshold_by_time(
    size_t current_threshold,
    const Lv00AdaptiveThresholdCtx* ctx
) {
    if (!ctx->config.enable_time_based) {
        return current_threshold;
    }
    
    double time_spent_ms = ctx->progress.time_spent_ms;
    double time_budget_ms = ctx->config.time_budget_ms;
    
    if (time_budget_ms <= 0.0) {
        return current_threshold;
    }
    
    double time_ratio = time_spent_ms / time_budget_ms;
    
    /* 如果时间过半但进度不足10%，大幅降低阈值 */
    if (time_ratio > 0.5 && ctx->progress.progress_estimate < 0.1) {
        return (size_t)(current_threshold * 0.5);
    }
    
    /* 如果时间超过80%，进一步降低阈值 */
    if (time_ratio > 0.8) {
        return (size_t)(current_threshold * 0.3);
    }
    
    return current_threshold;
}

/* ==================== 公共API实现 ==================== */

Lv00Error lv00_adaptive_threshold_init(void) {
    if (g_initialized) {
        return LV00_OK;
    }
    
    /* 初始化全局配置为默认值 */
    memcpy(g_global_configs, g_default_configs, sizeof(g_default_configs));
    memset(g_global_config_set, 0, sizeof(g_global_config_set));
    
    g_initialized = true;
    return LV00_OK;
}

void lv00_adaptive_threshold_cleanup(void) {
    g_initialized = false;
}

Lv00Error lv00_compute_complexity(
    const Lv00ConstraintGraph* graph,
    Lv00ProblemComplexity* complexity
) {
    LV00_CHECK_NULL(graph);
    LV00_CHECK_NULL(complexity);
    
    memset(complexity, 0, sizeof(Lv00ProblemComplexity));
    
    /* 基础统计 */
    complexity->node_count = (size_t)graph->node_count;
    complexity->constraint_count = (size_t)graph->constraint_count;
    
    /* 计算边数（从约束推导） */
    size_t edge_count = 0;
    size_t total_degree = 0;
    
    for (size_t i = 0; i < graph->constraint_count; i++) {
        const Lv00Constraint* c = &graph->constraints[i];
        /* 每个约束贡献 (participant_count choose 2) 条边 */
        size_t pcount = c->participant_count;
        if (pcount > 1) {
            edge_count += (pcount * (pcount - 1)) / 2;
            total_degree += pcount;
        }
    }
    
    complexity->edge_count = edge_count;
    
    /* 平均度数 */
    if (complexity->node_count > 0) {
        complexity->avg_node_degree = total_degree / complexity->node_count;
    }
    
    /* 图密度 */
    complexity->density = compute_graph_density(complexity->node_count, edge_count);
    
    /* 连通分量 */
    complexity->connected_components = count_connected_components(graph, complexity->node_count);
    
    /* 多项式次数估计（从约束类型推断） */
    complexity->max_degree = 2; /* 默认二次 */
    for (size_t i = 0; i < graph->constraint_count; i++) {
        const Lv00Constraint* c = &graph->constraints[i];
        /* 距离约束通常产生二次方程 */
        if (c->type == LV00_CONSTRAINT_DISTANCE ||
            c->type == LV00_CONSTRAINT_CIRCLE) {
            if (complexity->max_degree < 2) {
                complexity->max_degree = 2;
            }
        }
        /* 角度约束可能产生更高次 */
        if (c->type == LV00_CONSTRAINT_ANGLE) {
            if (complexity->max_degree < 3) {
                complexity->max_degree = 3;
            }
        }
    }
    
    return LV00_OK;
}

Lv00Error lv00_adaptive_threshold_create(
    Lv00AlgorithmType algo_type,
    const Lv00ConstraintGraph* graph,
    const Lv00ThresholdConfig* config,
    Lv00AdaptiveThresholdCtx** ctx
) {
    LV00_CHECK_NULL(ctx);
    
    if (algo_type < 0 || algo_type >= LV00_ALGO_COUNT) {
        return LV00_ERROR_INVALID_ARGUMENT;
    }
    
    if (!g_initialized) {
        lv00_adaptive_threshold_init();
    }
    
    /* 分配上下文 */
    *ctx = (Lv00AdaptiveThresholdCtx*)lv00_malloc(sizeof(Lv00AdaptiveThresholdCtx));
    if (!*ctx) {
        return LV00_ERROR_OUT_OF_MEMORY;
    }
    
    memset(*ctx, 0, sizeof(Lv00AdaptiveThresholdCtx));
    
    /* 设置算法类型 */
    (*ctx)->algo_type = algo_type;
    
    /* 设置配置 */
    if (config) {
        memcpy(&(*ctx)->config, config, sizeof(Lv00ThresholdConfig));
    } else if (g_global_config_set[algo_type]) {
        memcpy(&(*ctx)->config, &g_global_configs[algo_type], sizeof(Lv00ThresholdConfig));
    } else {
        memcpy(&(*ctx)->config, &g_default_configs[algo_type], sizeof(Lv00ThresholdConfig));
    }
    
    /* 计算问题复杂度 */
    if (graph) {
        Lv00Error err = lv00_compute_complexity(graph, &(*ctx)->complexity);
        if (err != LV00_OK) {
            lv00_free(*ctx);
            *ctx = NULL;
            return err;
        }
    }
    
    /* 初始化进度 */
    (*ctx)->progress.progress_estimate = 0.0;
    (*ctx)->progress.solution_likelihood = 1.0; /* 初始乐观估计 */
    (*ctx)->progress.time_spent_ms = 0.0;
    (*ctx)->progress.iterations_done = 0;
    (*ctx)->progress.backtracks = 0;
    
    /* 记录开始时间 */
    (*ctx)->start_time_us = get_time_us();
    
    /* 计算初始阈值 */
    (*ctx)->current_threshold = lv00_adaptive_threshold_compute(*ctx);
    
    return LV00_OK;
}

void lv00_adaptive_threshold_destroy(Lv00AdaptiveThresholdCtx** ctx) {
    if (!ctx || !*ctx) {
        return;
    }
    
    lv00_free(*ctx);
    *ctx = NULL;
}

size_t lv00_adaptive_threshold_compute(Lv00AdaptiveThresholdCtx* ctx) {
    if (!ctx) {
        return 100; /* 保守默认值 */
    }
    
    /* 更新已花费时间 */
    uint64_t now = get_time_us();
    ctx->progress.time_spent_ms = (double)(now - ctx->start_time_us) / 1000.0;
    
    /* 计算基础阈值 */
    size_t threshold = compute_threshold_formula(
        ctx->algo_type,
        &ctx->complexity,
        &ctx->config
    );
    
    /* 基于时间预算调整 */
    threshold = adjust_threshold_by_time(threshold, ctx);
    
    ctx->current_threshold = threshold;
    return threshold;
}

Lv00Error lv00_adaptive_threshold_update_progress(
    Lv00AdaptiveThresholdCtx* ctx,
    size_t iterations_done,
    size_t backtracks
) {
    LV00_CHECK_NULL(ctx);
    
    ctx->progress.iterations_done = iterations_done;
    ctx->progress.backtracks = backtracks;
    
    /* 更新已花费时间 */
    uint64_t now = get_time_us();
    ctx->progress.time_spent_ms = (double)(now - ctx->start_time_us) / 1000.0;
    
    /* 估计进度（简化模型） */
    if (ctx->current_threshold > 0) {
        ctx->progress.progress_estimate = 
            (double)iterations_done / (double)ctx->current_threshold;
        if (ctx->progress.progress_estimate > 1.0) {
            ctx->progress.progress_estimate = 1.0;
        }
    }
    
    /* 估计解存在概率（基于回溯率） */
    if (iterations_done > 0) {
        double backtrack_rate = (double)backtracks / (double)iterations_done;
        /* 高回溯率可能表示搜索空间困难 */
        ctx->progress.solution_likelihood = 1.0 - (backtrack_rate * 0.5);
        if (ctx->progress.solution_likelihood < 0.01) {
            ctx->progress.solution_likelihood = 0.01;
        }
    }
    
    return LV00_OK;
}

Lv00Error lv00_adaptive_threshold_should_prune(
    Lv00AdaptiveThresholdCtx* ctx,
    bool* should_prune
) {
    LV00_CHECK_NULL(ctx);
    LV00_CHECK_NULL(should_prune);
    
    *should_prune = false;
    
    if (!ctx->config.enable_progress_tracking) {
        return LV00_OK;
    }
    
    double time_budget_ms = ctx->config.time_budget_ms;
    double time_spent_ms = ctx->progress.time_spent_ms;
    
    /* 规则1: 时间预算耗尽 */
    if (time_budget_ms > 0.0 && time_spent_ms > time_budget_ms) {
        *should_prune = true;
        return LV00_OK;
    }
    
    /* 规则2: 进度缓慢且时间过半 */
    if (time_budget_ms > 0.0 && 
        time_spent_ms > time_budget_ms * 0.5 && 
        ctx->progress.progress_estimate < 0.1) {
        *should_prune = true;
        return LV00_OK;
    }
    
    /* 规则3: 解存在概率极低 */
    if (ctx->progress.solution_likelihood < 0.01) {
        *should_prune = true;
        return LV00_OK;
    }
    
    /* 规则4: 回溯率过高（可能陷入局部困难） */
    if (ctx->progress.iterations_done > 100) {
        double backtrack_rate = (double)ctx->progress.backtracks / 
                                (double)ctx->progress.iterations_done;
        if (backtrack_rate > 0.9) { /* 90%以上的迭代都在回溯 */
            *should_prune = true;
            return LV00_OK;
        }
    }
    
    return LV00_OK;
}

Lv00Error lv00_adaptive_threshold_default_config(
    Lv00AlgorithmType algo_type,
    Lv00ThresholdConfig* config
) {
    LV00_CHECK_NULL(config);
    
    if (algo_type < 0 || algo_type >= LV00_ALGO_COUNT) {
        return LV00_ERROR_INVALID_ARGUMENT;
    }
    
    memcpy(config, &g_default_configs[algo_type], sizeof(Lv00ThresholdConfig));
    return LV00_OK;
}

Lv00Error lv00_adaptive_threshold_set_global_config(
    Lv00AlgorithmType algo_type,
    const Lv00ThresholdConfig* config
) {
    LV00_CHECK_NULL(config);
    
    if (algo_type < 0 || algo_type >= LV00_ALGO_COUNT) {
        return LV00_ERROR_INVALID_ARGUMENT;
    }
    
    if (!g_initialized) {
        lv00_adaptive_threshold_init();
    }
    
    memcpy(&g_global_configs[algo_type], config, sizeof(Lv00ThresholdConfig));
    g_global_config_set[algo_type] = true;
    
    return LV00_OK;
}
