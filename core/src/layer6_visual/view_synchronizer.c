#include "lv00/visual_editor.h"
#include <stdlib.h>
#include <string.h>

/* View synchronizer - keeps all 4 views in sync */

typedef struct Lv00ViewSynchronizer {
    int sync_enabled;
    void *source_graph;
    int conflict_count;
    char last_conflict[512];
} Lv00ViewSynchronizer;

Lv00ViewSynchronizer *lv00_view_sync_create(void) {
    Lv00ViewSynchronizer *sync = calloc(1, sizeof(Lv00ViewSynchronizer));
    if (!sync) return NULL;
    sync->sync_enabled = 1;
    return sync;
}

void lv00_view_sync_destroy(Lv00ViewSynchronizer *sync) {
    free(sync);
}

int lv00_view_sync_enable(Lv00ViewSynchronizer *sync) {
    if (!sync) return -1;
    sync->sync_enabled = 1;
    return 0;
}

int lv00_view_sync_disable(Lv00ViewSynchronizer *sync) {
    if (!sync) return -1;
    sync->sync_enabled = 0;
    return 0;
}

int lv00_view_sync_conflicts(const Lv00ViewSynchronizer *sync) {
    return sync ? sync->conflict_count : 0;
}
