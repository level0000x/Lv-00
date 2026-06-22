#ifndef LV00_GROEBNER_PARALLEL_H
#define LV00_GROEBNER_PARALLEL_H

#include "lv00/type_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Thread pool configuration for Groebner basis computation */
typedef struct Lv00GroebnerConfig {
    int max_threads;
    int chunk_size;           /* Polynomial pairs per chunk */
    double load_balance_threshold; /* Trigger rebalance when imbalance exceeds this */
    int enable_inter_reduction;   /* Inter-thread reduction */
    int enable_cache;             /* Cache intermediate S-polynomials */
} Lv00GroebnerConfig;

/* Parallel computation state */
typedef struct Lv00GroebnerState {
    int total_pairs;
    int completed_pairs;
    int remaining_pairs;
    double elapsed_ms;
    int active_threads;
} Lv00GroebnerState;

/* Parallel Groebner engine */
typedef struct Lv00GroebnerParallel {
    Lv00GroebnerConfig config;
    Lv00GroebnerState state;

    /* Work queue */
    void *pair_queue;
    int queue_size;

    /* Thread pool */
    void *thread_pool;

    /* Result */
    void **groebner_basis;
    int basis_size;
} Lv00GroebnerParallel;

/* Default configuration */
Lv00GroebnerConfig lv00_groebner_default_config(void);

/* Lifecycle */
Lv00GroebnerParallel *lv00_groebner_parallel_create(const Lv00GroebnerConfig *config);
void lv00_groebner_parallel_destroy(Lv00GroebnerParallel *engine);

/* Execution */
int lv00_groebner_parallel_compute(Lv00GroebnerParallel *engine,
                                     void *polynomials, int poly_count);

/* Progress query */
Lv00GroebnerState lv00_groebner_parallel_state(const Lv00GroebnerParallel *engine);

/* Basis inspection */
bool lv00_groebner_poly_is_nonzero_constant(void *poly);

/* Performance target: 50%+ improvement over sequential computation */
/* Baseline: sequential Buchberger algorithm */
/* Target: parallel with work-stealing + inter-reduction */

#ifdef __cplusplus
}
#endif

#endif /* LV00_GROEBNER_PARALLEL_H */
