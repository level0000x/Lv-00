/**
 * @file adaptive_pruning.h
 * @brief 自适应剪枝与策略建议
 *
 * 提供基于问题复杂度的自适应搜索限制计算、
 * 启发式剪枝决策和策略权重建议。
 */

#ifndef LV00_ADAPTIVE_PRUNING_H
#define LV00_ADAPTIVE_PRUNING_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 问题复杂度 ============== */

/**
 * @brief 问题复杂度分析结果
 */
typedef struct {
    size_t node_count;            /**< 节点数量 */
    size_t constraint_count;      /**< 约束数量 */
    size_t edge_count;            /**< 边数量 */
    size_t max_polynomial_degree;  /**< 最大多项式次数 */
    size_t axiom_count;           /**< 公理数量 */
    double estimated_search_space; /**< 估计搜索空间（log2） */
} Lv00ProblemComplexity;

/* ============== 搜索启发式 ============== */

/**
 * @brief 搜索启发式信息
 */
typedef struct {
    double time_spent_ms;        /**< 已用时间（毫秒） */
    double progress_estimate;    /**< 进度估计 [0, 1] */
    double solution_likelihood;  /**< 解存在的似然度 [0, 1] */
    int branches_explored;       /**< 已探索分支数 */
} Lv00SearchHeuristics;

/* ============== 自适应配置 ============== */

/**
 * @brief 自适应剪枝配置
 */
typedef struct {
    double base_iterations;        /**< 基础迭代次数 */
    double time_budget_ms;         /**< 时间预算（毫秒） */
    double progress_threshold;     /**< 进度阈值 */
    double solution_likelihood_min; /**< 最小解似然度 */
    int enable_heuristic_pruning;  /**< 是否启用启发式剪枝 */
    int enable_neural_suggestion;  /**< 是否启用神经策略建议 */
} Lv00AdaptiveConfig;

/* ============== 剪枝决策 ============== */

/**
 * @brief 剪枝决策结果
 */
typedef struct {
    int should_prune;    /**< 是否应该剪枝 */
    double confidence;   /**< 决策置信度 [0, 1] */
    char reason[256];    /**< 决策理由 */
} Lv00PruningDecision;

/* ============== 神经策略建议 ============== */

/**
 * @brief 策略索引枚举
 */
enum {
    STRATEGY_DIRECT_CONSTRUCTION = 0, /**< 直接构造法 */
    STRATEGY_AREA_METHOD,             /**< 面积法 */
    STRATEGY_ALGEBRAIC,              /**< 代数法 */
    STRATEGY_SOS,                    /**< SOS（平方和）法 */
    STRATEGY_INEQUALITY,             /**< 不等式法 */
    STRATEGY_COORDINATE,             /**< 坐标法 */
    STRATEGY_VECTOR,                 /**< 向量法 */
    STRATEGY_TRANSFORM,              /**< 变换法 */
    STRATEGY_COUNT = 8               /**< 策略总数 */
};

/**
 * @brief 神经策略建议结果
 */
typedef struct {
    int valid;                        /**< 建议是否有效 */
    float strategy_weights[8];         /**< 各策略权重 [0, 1] */
    float confidence;                 /**< 总体置信度 [0, 1] */
} Lv00NeuralSuggestion;

/* ============== 自适应剪枝器 ============== */

/**
 * @brief 自适应剪枝器
 */
struct Lv00AdaptivePruner {
    Lv00AdaptiveConfig config;          /**< 自适应配置 */
    Lv00ProblemComplexity complexity;   /**< 问题复杂度 */
    Lv00SearchHeuristics heuristics;    /**< 搜索启发式信息 */
    size_t max_iterations;              /**< 最大迭代次数 */
    double max_time_ms;                 /**< 最大时间（毫秒） */
    size_t max_depth;                   /**< 最大深度 */
    size_t total_pruned;                /**< 总剪枝数 */
    size_t total_explored;              /**< 总探索数 */
    double total_time_saved_ms;         /**< 节省的总时间（毫秒） */
};

typedef struct Lv00AdaptivePruner Lv00AdaptivePruner;

/* ============== 剪枝统计 ============== */

/**
 * @brief 剪枝器统计信息
 */
typedef struct {
    size_t total_pruned;            /**< 总剪枝数 */
    size_t total_explored;          /**< 总探索数 */
    double total_time_saved_ms;     /**< 节省的总时间（毫秒） */
    double pruning_rate;            /**< 剪枝率 */
} Lv00PrunerStats;

/* ============== 公共 API ============== */

Lv00ProblemComplexity lv00_analyze_complexity(
    size_t node_count, size_t constraint_count, size_t edge_count,
    size_t max_poly_degree, size_t axiom_count);

size_t lv00_compute_adaptive_limit(const Lv00ProblemComplexity *complexity,
                                   double target_time_ms);

Lv00AdaptiveConfig lv00_default_adaptive_config(void);

Lv00AdaptivePruner *lv00_pruner_create(const Lv00AdaptiveConfig *config);
void lv00_pruner_destroy(Lv00AdaptivePruner *pruner);
int lv00_pruner_set_problem(Lv00AdaptivePruner *pruner,
                            const Lv00ProblemComplexity *complexity);

Lv00PruningDecision lv00_pruner_evaluate(Lv00AdaptivePruner *pruner,
                                           const Lv00SearchHeuristics *heuristics);
int lv00_pruner_time_exceeded(const Lv00AdaptivePruner *pruner, double elapsed_ms);
int lv00_pruner_iterations_exceeded(const Lv00AdaptivePruner *pruner, size_t iterations);

double lv00_estimate_progress(int branches_explored, int total_branches,
                                double time_spent_ms, double time_budget_ms);
double lv00_estimate_solution_likelihood(const Lv00ProblemComplexity *complexity,
                                           int depth, int conflicts_found);

Lv00NeuralSuggestion lv00_neural_suggest_strategy(
    const Lv00ProblemComplexity *complexity);

Lv00PrunerStats lv00_pruner_get_stats(const Lv00AdaptivePruner *pruner);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ADAPTIVE_PRUNING_H */
