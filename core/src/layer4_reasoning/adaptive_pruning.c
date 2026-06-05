#include "lv00/adaptive_pruning.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── Complexity Analysis ── */

Lv00ProblemComplexity lv00_analyze_complexity(
    size_t node_count,
    size_t constraint_count,
    size_t edge_count,
    size_t max_poly_degree,
    size_t axiom_count)
{
    Lv00ProblemComplexity c;
    memset(&c, 0, sizeof(c));
    c.node_count = node_count;
    c.constraint_count = constraint_count;
    c.edge_count = edge_count;
    c.max_polynomial_degree = max_poly_degree;
    c.axiom_count = axiom_count;

    /* Estimate search space: O(nodes! * constraints^degree) */
    double factorial = 1.0;
    size_t n = node_count > 20 ? 20 : node_count;
    for (size_t i = 2; i <= n; i++) factorial *= (double)i;
    if (node_count > 20) factorial *= pow((double)node_count, (double)(node_count - 20));

    double constraint_factor = pow((double)(constraint_count + 1), (double)(max_poly_degree + 1));
    c.estimated_search_space = log2(factorial * constraint_factor + 1.0);

    return c;
}

size_t lv00_compute_adaptive_limit(const Lv00ProblemComplexity *complexity,
                                   double target_time_ms)
{
    if (!complexity || complexity->node_count == 0) return 1000;

    /* Adaptive formula: limit = base * log2(nodes+1) * sqrt(constraints) * (1 + degree/10) */
    double base = 100.0;
    double node_factor = log2((double)complexity->node_count + 1.0);
    double constraint_factor = sqrt((double)complexity->constraint_count + 1.0);
    double degree_factor = 1.0 + (double)complexity->max_polynomial_degree / 10.0;

    double limit = base * node_factor * constraint_factor * degree_factor;

    /* Scale by time budget (assume 1ms per 100 iterations baseline) */
    double time_scale = target_time_ms / 1000.0;
    if (time_scale > 0.0 && time_scale < 1.0) {
        limit *= time_scale;
    }

    /* Cap at reasonable maximum */
    if (limit > 100000.0) limit = 100000.0;
    if (limit < 100.0) limit = 100.0;

    return (size_t)limit;
}

/* ── Default Configuration ── */

Lv00AdaptiveConfig lv00_default_adaptive_config(void) {
    Lv00AdaptiveConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.base_iterations = 100.0;
    cfg.time_budget_ms = 30000.0;       /* 30 seconds */
    cfg.progress_threshold = 0.1;       /* 10% minimum progress */
    cfg.solution_likelihood_min = 0.01; /* 1% minimum probability */
    cfg.enable_heuristic_pruning = 1;
    cfg.enable_neural_suggestion = 0;
    return cfg;
}

/* ── Pruner Lifecycle ── */

Lv00AdaptivePruner *lv00_pruner_create(const Lv00AdaptiveConfig *config) {
    Lv00AdaptivePruner *pruner = calloc(1, sizeof(Lv00AdaptivePruner));
    if (!pruner) return NULL;
    pruner->config = config ? *config : lv00_default_adaptive_config();
    pruner->max_iterations = 10000;
    pruner->max_time_ms = pruner->config.time_budget_ms;
    return pruner;
}

void lv00_pruner_destroy(Lv00AdaptivePruner *pruner) {
    free(pruner);
}

int lv00_pruner_set_problem(Lv00AdaptivePruner *pruner,
                            const Lv00ProblemComplexity *complexity) {
    if (!pruner || !complexity) return -1;
    pruner->complexity = *complexity;
    pruner->max_iterations = lv00_compute_adaptive_limit(complexity, pruner->config.time_budget_ms);
    pruner->max_depth = (size_t)(log2((double)complexity->node_count + 1.0) * 3.0);
    return 0;
}

/* ── Pruning Decisions ── */

Lv00PruningDecision lv00_pruner_evaluate(Lv00AdaptivePruner *pruner,
                                           const Lv00SearchHeuristics *heuristics) {
    Lv00PruningDecision decision = {0, 0.0, ""};

    if (!pruner || !heuristics) {
        decision.should_prune = 0;
        strncpy(decision.reason, "Invalid arguments", sizeof(decision.reason) - 1);
        return decision;
    }

    pruner->heuristics = *heuristics;

    /* Rule 1: Time-based pruning */
    if (heuristics->time_spent_ms > pruner->config.time_budget_ms * 0.8) {
        decision.should_prune = 1;
        decision.confidence = 0.9;
        strncpy(decision.reason, "Time budget nearly exhausted (>80%)", sizeof(decision.reason) - 1);
        pruner->total_pruned++;
        return decision;
    }

    /* Rule 2: Low progress + significant time spent */
    if (pruner->config.enable_heuristic_pruning &&
        heuristics->time_spent_ms > pruner->config.time_budget_ms * 0.5 &&
        heuristics->progress_estimate < pruner->config.progress_threshold) {
        decision.should_prune = 1;
        decision.confidence = 0.7;
        strncpy(decision.reason, "Low progress with >50% time spent", sizeof(decision.reason) - 1);
        pruner->total_pruned++;
        return decision;
    }

    /* Rule 3: Very low solution likelihood */
    if (heuristics->solution_likelihood < pruner->config.solution_likelihood_min) {
        decision.should_prune = 1;
        decision.confidence = 0.8;
        strncpy(decision.reason, "Solution likelihood below threshold", sizeof(decision.reason) - 1);
        pruner->total_pruned++;
        return decision;
    }

    /* Rule 4: Depth limit exceeded */
    if (heuristics->branches_explored > pruner->max_depth * 10) {
        decision.should_prune = 1;
        decision.confidence = 0.6;
        strncpy(decision.reason, "Depth limit exceeded", sizeof(decision.reason) - 1);
        pruner->total_pruned++;
        return decision;
    }

    /* No pruning */
    decision.should_prune = 0;
    decision.confidence = 0.8;
    strncpy(decision.reason, "Continue exploring", sizeof(decision.reason) - 1);
    pruner->total_explored++;
    return decision;
}

int lv00_pruner_time_exceeded(const Lv00AdaptivePruner *pruner, double elapsed_ms) {
    return pruner ? (elapsed_ms >= pruner->max_time_ms) : 0;
}

int lv00_pruner_iterations_exceeded(const Lv00AdaptivePruner *pruner, size_t iterations) {
    return pruner ? (iterations >= pruner->max_iterations) : 0;
}

/* ── Heuristic Scoring ── */

double lv00_estimate_progress(int branches_explored, int total_branches,
                                double time_spent_ms, double time_budget_ms) {
    if (total_branches <= 0) return 0.0;
    if (time_budget_ms <= 0.0) return 0.0;

    /* Weighted combination of branch progress and time progress */
    double branch_progress = (double)branches_explored / (double)total_branches;
    double time_progress = time_spent_ms / time_budget_ms;

    /* If we've explored many branches but made little overall progress,
       the progress estimate should be low */
    return 0.6 * branch_progress + 0.4 * (1.0 - time_progress);
}

double lv00_estimate_solution_likelihood(const Lv00ProblemComplexity *complexity,
                                           int depth, int conflicts_found) {
    if (!complexity) return 0.5;

    /* Base probability decreases with search space size */
    double base_prob = 1.0 / (1.0 + complexity->estimated_search_space * 0.01);

    /* Adjust for depth: deeper = less likely to find solution */
    double depth_factor = 1.0 / (1.0 + depth * 0.1);

    /* Adjust for conflicts: more conflicts = less likely */
    double conflict_factor = 1.0 / (1.0 + conflicts_found * 0.5);

    double likelihood = base_prob * depth_factor * conflict_factor;

    /* Clamp to [0, 1] */
    if (likelihood < 0.0) likelihood = 0.0;
    if (likelihood > 1.0) likelihood = 1.0;

    return likelihood;
}

/* ── Neural Suggestion (Heuristic-based) ── */

Lv00NeuralSuggestion lv00_neural_suggest_strategy(
    const Lv00ProblemComplexity *complexity)
{
    Lv00NeuralSuggestion suggestion;
    memset(&suggestion, 0, sizeof(suggestion));

    if (!complexity) return suggestion;

    /* 初始化所有策略权重为低基线 */
    for (int i = 0; i < STRATEGY_COUNT; i++) {
        suggestion.strategy_weights[i] = 0.05f;
    }

    float total_weight = 0.0f;
    int feature_matches = 0;

    /*
     * 启发式策略选择基于问题复杂度分析：
     *
     * - 约束少 (< 5): 问题简单，优先直接构造法和面积法
     * - 约束多 (> 20): 问题复杂，优先代数法和SOS法
     * - 不等式特征 (高多项式次数): 优先不等式策略
     * - 平行/垂直特征 (中等节点数): 优先坐标法
     */

    /* 特征1: 少约束 → 直接构造 + 面积法 */
    if (complexity->constraint_count < 5) {
        float boost = 0.35f;
        suggestion.strategy_weights[STRATEGY_DIRECT_CONSTRUCTION] += boost;
        suggestion.strategy_weights[STRATEGY_AREA_METHOD] += boost * 0.8f;
        feature_matches++;
        total_weight += boost + boost * 0.8f;
    }

    /* 特征2: 多约束 → 代数法 + SOS法 */
    if (complexity->constraint_count > 20) {
        float boost = 0.35f;
        suggestion.strategy_weights[STRATEGY_ALGEBRAIC] += boost;
        suggestion.strategy_weights[STRATEGY_SOS] += boost * 0.8f;
        feature_matches++;
        total_weight += boost + boost * 0.8f;
    }

    /* 特征3: 高多项式次数 → 不等式策略 */
    if (complexity->max_polynomial_degree >= 2) {
        float boost = 0.25f;
        suggestion.strategy_weights[STRATEGY_INEQUALITY] += boost;
        feature_matches++;
        total_weight += boost;
    }

    /* 特征4: 中等节点数 (3-10) → 坐标法 (平行/垂直特征常见) */
    if (complexity->node_count >= 3 && complexity->node_count <= 10) {
        float boost = 0.20f;
        suggestion.strategy_weights[STRATEGY_COORDINATE] += boost;
        feature_matches++;
        total_weight += boost;
    }

    /* 特征5: 大搜索空间 → 向量法和变换法 */
    if (complexity->estimated_search_space > 20.0) {
        float boost = 0.15f;
        suggestion.strategy_weights[STRATEGY_VECTOR] += boost;
        suggestion.strategy_weights[STRATEGY_TRANSFORM] += boost * 0.7f;
        feature_matches++;
        total_weight += boost + boost * 0.7f;
    }

    /* 如果没有特征匹配，回退到均匀分布 */
    if (feature_matches == 0) {
        for (int i = 0; i < STRATEGY_COUNT; i++) {
            suggestion.strategy_weights[i] = 0.125f;
        }
        suggestion.confidence = 0.1f;
        suggestion.valid = 1;
        return suggestion;
    }

    /* 归一化权重，使总和为 1.0 */
    if (total_weight > 0.0f) {
        float sum = 0.0f;
        for (int i = 0; i < STRATEGY_COUNT; i++) {
            sum += suggestion.strategy_weights[i];
        }
        if (sum > 0.0f) {
            for (int i = 0; i < STRATEGY_COUNT; i++) {
                suggestion.strategy_weights[i] /= sum;
            }
        }
    }

    /* 置信度基于特征匹配的清晰度：
     * 匹配越多、特征越明确，置信度越高 */
    suggestion.confidence = (float)feature_matches / 6.0f;
    if (suggestion.confidence > 1.0f) suggestion.confidence = 1.0f;
    if (suggestion.confidence < 0.0f) suggestion.confidence = 0.0f;

    suggestion.valid = 1;

    return suggestion;
}

/* ── Statistics ── */

Lv00PrunerStats lv00_pruner_get_stats(const Lv00AdaptivePruner *pruner) {
    Lv00PrunerStats stats;
    memset(&stats, 0, sizeof(stats));
    if (!pruner) return stats;
    stats.total_pruned = pruner->total_pruned;
    stats.total_explored = pruner->total_explored;
    stats.total_time_saved_ms = pruner->total_time_saved_ms;
    size_t total = stats.total_pruned + stats.total_explored;
    stats.pruning_rate = total > 0 ? (double)stats.total_pruned / (double)total : 0.0;
    return stats;
}
