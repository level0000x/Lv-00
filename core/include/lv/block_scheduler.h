#ifndef lv_BLOCK_SCHEDULER_H
#define lv_BLOCK_SCHEDULER_H

#include "lv/func_block.h"
#include "lv/effect_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Execution strategy */
typedef enum {
    lv_SCHED_FULL,
    lv_SCHED_INCREMENTAL,
    lv_SCHED_LAZY
} lvSchedStrategy;

/* Execution result */
typedef struct lvExecResult {
    int success;
    int blocks_executed;
    int blocks_skipped;
    double elapsed_ms;
    char error_msg[512];
} lvExecResult;

/* Block scheduler */
typedef struct lvBlockScheduler {
    void *graph;

    /* Execution queue (block IDs in topological order) */
    int *queue;
    int queue_count;

    /* Port values */
    void *port_values;

    /* Incremental state */
    struct {
        int *dirty_blocks;
        int dirty_count;
        void *cached_results;
    } incremental;

    /* Effect tracking */
    lvEffectTracker *effect_tracker;

    /* Strategy */
    lvSchedStrategy strategy;
} lvBlockScheduler;

/* Lifecycle */
lvBlockScheduler *lv_block_scheduler_create(void *graph);
void lv_block_scheduler_destroy(lvBlockScheduler *sched);

/* Configuration */
void lv_block_scheduler_set_strategy(lvBlockScheduler *sched, lvSchedStrategy strategy);

/* Execution */
lvExecResult lv_block_scheduler_run(lvBlockScheduler *sched);
lvExecResult lv_block_scheduler_run_incremental(lvBlockScheduler *sched, int *dirty, int count);

/* Dirty tracking */
void lv_block_scheduler_mark_dirty(lvBlockScheduler *sched, int block_id);
void lv_block_scheduler_mark_all_dirty(lvBlockScheduler *sched);

#ifdef __cplusplus
}
#endif

#endif /* lv_BLOCK_SCHEDULER_H */
