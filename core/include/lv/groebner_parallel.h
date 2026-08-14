#ifndef lv_GROEBNER_PARALLEL_H
#define lv_GROEBNER_PARALLEL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Thread pool configuration for Groebner basis computation */
typedef struct lvGroebnerConfig {
    int max_threads;
    int chunk_size;                /* Polynomial pairs per chunk */
    double load_balance_threshold; /* Trigger rebalance when imbalance exceeds this */
    int enable_inter_reduction;    /* Inter-thread reduction */
    int enable_cache;              /* Cache intermediate S-polynomials */
} lvGroebnerConfig;

/* Parallel computation state */
typedef struct lvGroebnerState {
    int total_pairs;
    int completed_pairs;
    int remaining_pairs;
    double elapsed_ms;
    int active_threads;
} lvGroebnerState;

/* Parallel Groebner engine */
typedef struct lvGroebnerParallel {
    lvGroebnerConfig config;
    lvGroebnerState state;

    /* Work queue */
    void *pair_queue;
    int queue_size;

    /* Thread pool */
    void *thread_pool;
    int thread_count; /**< 实际使用的线程数 */

    /* Result */
    void **groebner_basis;
    int basis_size;
} lvGroebnerParallel;

/* Default configuration */
lvGroebnerConfig lv_groebner_default_config(void);

/* Lifecycle */
lvGroebnerParallel *lv_groebner_parallel_create(const lvGroebnerConfig *config);
void lv_groebner_parallel_destroy(lvGroebnerParallel *engine);

/* Execution */
int lv_groebner_parallel_compute(lvGroebnerParallel *engine, void *polynomials, int poly_count);

/* Progress query */
lvGroebnerState lv_groebner_parallel_state(const lvGroebnerParallel *engine);

/* Basis inspection */
bool lv_groebner_poly_is_nonzero_constant(void *poly);

/* Performance target: 50%+ improvement over sequential computation */
/* Baseline: sequential Buchberger algorithm */
/* Target: parallel with work-stealing + inter-reduction */

#ifdef __cplusplus
}
#endif

#endif /* lv_GROEBNER_PARALLEL_H */
