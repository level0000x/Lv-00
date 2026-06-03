#include "lv00/block_scheduler.h"
#include <stdlib.h>

/* Incremental execution engine */
/* Tracks which blocks need re-execution based on port value changes */

typedef struct Lv00IncrementalExec {
    void *value_cache;
    int *dependency_graph;
    int node_count;
} Lv00IncrementalExec;

Lv00IncrementalExec *lv00_incremental_exec_create(int node_count) {
    Lv00IncrementalExec *exec = calloc(1, sizeof(Lv00IncrementalExec));
    if (!exec) return NULL;
    exec->node_count = node_count;
    return exec;
}

void lv00_incremental_exec_destroy(Lv00IncrementalExec *exec) {
    if (!exec) return;
    free(exec->dependency_graph);
    free(exec);
}
