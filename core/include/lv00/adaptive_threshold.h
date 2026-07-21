#ifndef LV00_ADAPTIVE_THRESHOLD_H
#define LV00_ADAPTIVE_THRESHOLD_H

#include "lv00/constraint_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct ConstraintGraph Lv00ConstraintGraph;
typedef struct SymbolicCoord Lv00SymbolicCoord;

typedef enum {
    LV00_ALGO_VF2_MATCH = 0,
    LV00_ALGO_BUCHBERGER = 1,
    LV00_ALGO_REWRITE_SOLVE = 2
} Lv00AlgorithmType;

typedef struct {
    int node_count;
    int constraint_count;
    int edge_count;
    double density;
    int connected_components;
} Lv00ProblemComplexity;

typedef struct {
    double base_threshold;
    double scale_factor;
    double time_budget_ms;
    double min_threshold;
    double max_threshold;
    bool enable_time_based;
    bool enable_progress_tracking;
} Lv00ThresholdConfig;

typedef struct Lv00AdaptiveThresholdCtx Lv00AdaptiveThresholdCtx;

LV00_PUBLIC_API Lv00ErrorCode lv00_adaptive_threshold_init(void);
LV00_PUBLIC_API void lv00_adaptive_threshold_cleanup(void);
LV00_PUBLIC_API Lv00ErrorCode lv00_compute_complexity(ConstraintGraph *graph, Lv00ProblemComplexity *complexity);
LV00_PUBLIC_API Lv00ErrorCode lv00_adaptive_threshold_create(Lv00AlgorithmType algo, ConstraintGraph *graph, const Lv00ThresholdConfig *config, Lv00AdaptiveThresholdCtx **ctx);
LV00_PUBLIC_API size_t lv00_adaptive_threshold_compute(Lv00AdaptiveThresholdCtx *ctx);
LV00_PUBLIC_API Lv00ErrorCode lv00_adaptive_threshold_default_config(Lv00AlgorithmType algo, Lv00ThresholdConfig *config);
LV00_PUBLIC_API void lv00_adaptive_threshold_destroy(Lv00AdaptiveThresholdCtx **ctx);
LV00_PUBLIC_API void lv00_adaptive_threshold_update_progress(Lv00AdaptiveThresholdCtx *ctx, size_t depth, size_t backtrack_count);
LV00_PUBLIC_API void lv00_adaptive_threshold_should_prune(Lv00AdaptiveThresholdCtx *ctx, bool *should_prune);
LV00_PUBLIC_API Lv00ErrorCode lv00_adaptive_threshold_set_global_config(Lv00AlgorithmType algo, const Lv00ThresholdConfig *config);
LV00_PUBLIC_API size_t lv00_get_vf2_max_depth(ConstraintGraph *graph);
LV00_PUBLIC_API size_t lv00_get_buchberger_max_steps(ConstraintGraph *graph);
LV00_PUBLIC_API size_t lv00_get_rewrite_solve_max_iterations(ConstraintGraph *graph);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ADAPTIVE_THRESHOLD_H */
