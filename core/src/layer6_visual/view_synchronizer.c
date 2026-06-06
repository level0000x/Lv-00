#include "lv00/visual_editor.h"
#include "lv00/lv00_utils.h"
#include <stdlib.h>
#include <string.h>

/* View synchronizer - keeps all 4 views in sync */

typedef struct Lv00ViewSynchronizer {
    int sync_enabled;
    void *source_graph;
    int conflict_count;
    char last_conflict[512];

    /* 脏视图追踪 */
    int *dirty_views;       /* 脏视图ID数组 */
    int dirty_count;
    int dirty_capacity;

    /* 待处理的变更记录 */
    struct {
        int source_view_id;
        char change_type[128];
    } *pending_changes;
    int pending_count;
    int pending_capacity;
} Lv00ViewSynchronizer;

Lv00ViewSynchronizer *lv00_view_sync_create(void) {
    Lv00ViewSynchronizer *sync = lv00_calloc(1, sizeof(Lv00ViewSynchronizer));
    if (!sync) return NULL;
    sync->sync_enabled = 1;
    sync->dirty_capacity = 8;
    sync->dirty_views = lv00_calloc(sync->dirty_capacity, sizeof(int));
    sync->pending_capacity = 8;
    sync->pending_changes = lv00_calloc(sync->pending_capacity, sizeof(sync->pending_changes[0]));
    return sync;
}

void lv00_view_sync_destroy(Lv00ViewSynchronizer *sync) {
    if (!sync) return;
    lv00_free((void **)&sync->dirty_views);
    lv00_free((void **)&sync->pending_changes);
    lv00_free((void **)&sync);
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

/* 标记视图为脏并传播到依赖视图 */
int lv00_view_sync_propagate(Lv00ViewSynchronizer *sync, int source_view_id,
                              const char *change_type) {
    if (!sync || !change_type) return -1;
    if (!sync->sync_enabled) return 0;

    /* 添加待处理变更记录 */
    if (sync->pending_count >= sync->pending_capacity) {
        int new_cap = sync->pending_capacity * 2;
        void *new_arr = lv00_realloc(sync->pending_changes,
                                new_cap * sizeof(sync->pending_changes[0]));
        if (!new_arr) return -1;
        sync->pending_changes = new_arr;
        sync->pending_capacity = new_cap;
    }
    sync->pending_changes[sync->pending_count].source_view_id = source_view_id;
    strncpy(sync->pending_changes[sync->pending_count].change_type,
            change_type, sizeof(sync->pending_changes[0].change_type) - 1);
    sync->pending_changes[sync->pending_count].change_type[
        sizeof(sync->pending_changes[0].change_type) - 1] = '\0';
    sync->pending_count++;

    /* 将源视图标记为脏（如果尚未标记） */
    int already_dirty = 0;
    for (int i = 0; i < sync->dirty_count; i++) {
        if (sync->dirty_views[i] == source_view_id) {
            already_dirty = 1;
            break;
        }
    }
    if (!already_dirty) {
        if (sync->dirty_count >= sync->dirty_capacity) {
            int new_cap = sync->dirty_capacity * 2;
            void *new_arr = lv00_realloc(sync->dirty_views, new_cap * sizeof(int));
            if (!new_arr) return -1;
            sync->dirty_views = new_arr;
            sync->dirty_capacity = new_cap;
        }
        sync->dirty_views[sync->dirty_count++] = source_view_id;
    }

    return 0;
}

/* 处理所有待处理的同步 */
int lv00_view_sync_flush(Lv00ViewSynchronizer *sync) {
    if (!sync) return -1;
    if (!sync->sync_enabled) return 0;

    int processed = 0;
    /* 按顺序处理每个待处理变更 */
    for (int i = 0; i < sync->pending_count; i++) {
        int src = sync->pending_changes[i].source_view_id;
        /* 在实际实现中，这里会调用对应视图的更新回调 */
        /* 当前为简单实现：将所有其他视图标记为脏 */
        (void)src;
        processed++;
    }

    /* 清空待处理队列 */
    sync->pending_count = 0;
    /* 清空脏视图列表 */
    sync->dirty_count = 0;

    return processed;
}
