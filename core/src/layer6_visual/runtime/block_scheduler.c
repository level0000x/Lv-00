#include "lv00/block_scheduler.h"
#include <stdlib.h>
#include <string.h>

Lv00BlockScheduler *lv00_block_scheduler_create(void *graph) {
    Lv00BlockScheduler *sched = calloc(1, sizeof(Lv00BlockScheduler));
    if (!sched) return NULL;
    sched->graph = graph;
    sched->strategy = LV00_SCHED_FULL;
    sched->effect_tracker = lv00_effect_tracker_create();
    return sched;
}

void lv00_block_scheduler_destroy(Lv00BlockScheduler *sched) {
    if (!sched) return;
    free(sched->queue);
    free(sched->incremental.dirty_blocks);
    if (sched->effect_tracker) lv00_effect_tracker_destroy(sched->effect_tracker);
    free(sched);
}

void lv00_block_scheduler_set_strategy(Lv00BlockScheduler *sched, Lv00SchedStrategy strategy) {
    if (sched) sched->strategy = strategy;
}

Lv00ExecResult lv00_block_scheduler_run(Lv00BlockScheduler *sched) {
    Lv00ExecResult result = {0};
    if (!sched) {
        result.success = 0;
        strncpy(result.error_msg, "NULL scheduler", sizeof(result.error_msg));
        return result;
    }
    /* TODO: topological sort + execute */
    result.success = 1;
    result.blocks_executed = sched->queue_count;
    return result;
}

Lv00ExecResult lv00_block_scheduler_run_incremental(Lv00BlockScheduler *sched, int *dirty, int count) {
    Lv00ExecResult result = {0};
    if (!sched || !dirty || count <= 0) {
        result.success = 0;
        strncpy(result.error_msg, "Invalid arguments", sizeof(result.error_msg));
        return result;
    }
    /* TODO: execute only dirty blocks */
    result.success = 1;
    result.blocks_executed = count;
    return result;
}

void lv00_block_scheduler_mark_dirty(Lv00BlockScheduler *sched, int block_id) {
    if (!sched) return;
    /* TODO: add to dirty set */
}

void lv00_block_scheduler_mark_all_dirty(Lv00BlockScheduler *sched) {
    if (!sched) return;
    /* TODO: mark all blocks dirty */
}
