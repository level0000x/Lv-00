#ifndef LV00_BLOCK_SCHEDULER_H
#define LV00_BLOCK_SCHEDULER_H

#include "lv00/func_block.h"
#include "lv00/effect_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Execution strategy */
typedef enum {
    LV00_SCHED_FULL,
    LV00_SCHED_INCREMENTAL,
    LV00_SCHED_LAZY
} Lv00SchedStrategy;

/* Execution result */
typedef struct Lv00ExecResult {
    int success;
    int blocks_executed;
    int blocks_skipped;
    double elapsed_ms;
    char error_msg[512];
} Lv00ExecResult;

/* Block scheduler */
typedef struct Lv00BlockScheduler {
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
    Lv00EffectTracker *effect_tracker;

    /* Strategy */
    Lv00SchedStrategy strategy;
} Lv00BlockScheduler;

/* Lifecycle */
Lv00BlockScheduler *lv00_block_scheduler_create(void *graph);
void lv00_block_scheduler_destroy(Lv00BlockScheduler *sched);

/* Configuration */
void lv00_block_scheduler_set_strategy(Lv00BlockScheduler *sched, Lv00SchedStrategy strategy);

/* Execution */
Lv00ExecResult lv00_block_scheduler_run(Lv00BlockScheduler *sched);
Lv00ExecResult lv00_block_scheduler_run_incremental(Lv00BlockScheduler *sched, int *dirty, int count);

/* Dirty tracking */
void lv00_block_scheduler_mark_dirty(Lv00BlockScheduler *sched, int block_id);
void lv00_block_scheduler_mark_all_dirty(Lv00BlockScheduler *sched);

#ifdef __cplusplus
}
#endif

#endif /* LV00_BLOCK_SCHEDULER_H */
