#ifndef LV00_ADAPTIVE_PRUNING_H
#define LV00_ADAPTIVE_PRUNING_H

#include "lv00/type_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Problem complexity metrics */
typedef struct Lv00ProblemComplexity {
    size_t node_count;
    size_t constraint_count;
    size_t edge_count;
    size_t max_polynomial_degree;
    size_t axiom_count;
    double estimated_search_space;  /* log2 of estimated search space */
} Lv00ProblemComplexity;

/* Adaptive threshold configuration */
typedef struct Lv00AdaptiveConfig {
    double base_iterations;        /* Base iteration limit */
    double time_budget_ms;        /* Time budget in milliseconds */
    double progress_threshold;    /* Minimum progress rate to continue */
    double solution_likelihood_min; /* Minimum solution probability */
    int enable_heuristic_pruning;  /* Enable heuristic-based pruning */
    int enable_neural_suggestion;  /* Enable neural strategy suggestion (future) */
} Lv00AdaptiveConfig;

/* Search heuristics for branch evaluation */
typedef struct Lv00SearchHeuristics {
    double progress_estimate;      /* Progress estimate [0, 1] */
    double solution_likelihood;    /* Solution existence probability [0, 1] */
    double time_spent_ms;          /* Time already spent */
    double time_remaining_ms;      /* Estimated remaining time */
    int branches_explored;
    int branches_pruned;
    int branches_remaining;
} Lv00SearchHeuristics;

/* Pruning decision result */
typedef struct Lv00PruningDecision {
    int should_prune;              /* 1 = prune, 0 = continue */
    double confidence;             /* Decision confidence [0, 1] */
    char reason[256];              /* Human-readable reason */
} Lv00PruningDecision;

/* Strategy weight for neural suggestion (future ML integration) */
typedef struct Lv00NeuralSuggestion {
    float strategy_weights[8];      /* Weights for 8 proof strategies */
    float confidence;              /* Model confidence */
    int valid;                     /* Whether suggestion is valid */
} Lv00NeuralSuggestion;

/* Adaptive pruner (main interface) */
typedef struct Lv00AdaptivePruner {
    Lv00AdaptiveConfig config;
    Lv00ProblemComplexity complexity;
    Lv00SearchHeuristics heuristics;

    /* Computed limits */
    size_t max_iterations;
    size_t max_depth;
    double max_time_ms;

    /* Statistics */
    size_t total_pruned;
    size_t total_explored;
    double total_time_saved_ms;
} Lv00AdaptivePruner;

/* ── Complexity Analysis ── */

/* Analyze problem complexity from constraint graph */
Lv00ProblemComplexity lv00_analyze_complexity(
    size_t node_count,
    size_t constraint_count,
    size_t edge_count,
    size_t max_poly_degree,
    size_t axiom_count
);

/* Compute adaptive iteration limit based on complexity */
size_t lv00_compute_adaptive_limit(
    const Lv00ProblemComplexity *complexity,
    double target_time_ms
);

/* ── Pruner Lifecycle ── */

Lv00AdaptivePruner *lv00_pruner_create(const Lv00AdaptiveConfig *config);
void lv00_pruner_destroy(Lv00AdaptivePruner *pruner);

/* Configure pruner for a specific problem */
int lv00_pruner_set_problem(Lv00AdaptivePruner *pruner,
                            const Lv00ProblemComplexity *complexity);

/* ── Pruning Decisions ── */

/* Evaluate whether current branch should be pruned */
Lv00PruningDecision lv00_pruner_evaluate(Lv00AdaptivePruner *pruner,
                                           const Lv00SearchHeuristics *heuristics);

/* Check if time budget is exhausted */
int lv00_pruner_time_exceeded(const Lv00AdaptivePruner *pruner, double elapsed_ms);

/* Check if iteration limit is reached */
int lv00_pruner_iterations_exceeded(const Lv00AdaptivePruner *pruner, size_t iterations);

/* ── Heuristic Scoring ── */

/* Compute progress estimate based on search state */
double lv00_estimate_progress(int branches_explored, int total_branches,
                                double time_spent_ms, double time_budget_ms);

/* Compute solution likelihood based on problem properties */
double lv00_estimate_solution_likelihood(const Lv00ProblemComplexity *complexity,
                                           int depth, int conflicts_found);

/* ── Neural Suggestion (Future ML Integration) ── */

/* Get neural strategy suggestion (stub for future ML integration) */
Lv00NeuralSuggestion lv00_neural_suggest_strategy(
    const Lv00ProblemComplexity *complexity
);

/* ── Default Configuration ── */

Lv00AdaptiveConfig lv00_default_adaptive_config(void);

/* ── Statistics ── */

typedef struct Lv00PrunerStats {
    size_t total_pruned;
    size_t total_explored;
    double total_time_saved_ms;
    double pruning_rate;  /* total_pruned / (total_pruned + total_explored) */
} Lv00PrunerStats;

Lv00PrunerStats lv00_pruner_get_stats(const Lv00AdaptivePruner *pruner);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ADAPTIVE_PRUNING_H */
