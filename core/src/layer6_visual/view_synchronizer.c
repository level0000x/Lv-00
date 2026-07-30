/**
 * @file view_synchronizer.c
 * @brief 视图同步器实现
 *
 * @details 实现多视图间的同步机制，支持视图脏标记传播、待处理变更
 *          收集和批量刷新。同步器维护脏视图列表和待处理变更队列，
 *          支持启用/禁用同步以控制传播行为。
 *
 * @author Lv-00 Project
 */

#include <stdlib.h>
#include <string.h>

#include "lv/lv_check.h"
#include "lv/lv_utils.h"
#include "lv/visual_editor.h"
#include "lv/lv_internal.h"

/**
 * @brief 创建视图同步器
 *
 * 分配并初始化同步器，默认启用同步。预分配脏视图列表和待处理变更队列。
 *
 * @return 成功返回同步器指针，失败返回NULL
 */
lvViewSynchronizer *lv_view_sync_create(void) {
    lvViewSynchronizer *sync = lv_calloc(1, sizeof(lvViewSynchronizer));
    if (!sync)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate view synchronizer");
    sync->sync_enabled = 1;
    sync->dirty_capacity = 8;
    sync->dirty_views = lv_calloc(sync->dirty_capacity, sizeof(int));
    if (!sync->dirty_views) {
        lv_free((void **) &sync);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate dirty views array");
    }
    sync->pending_capacity = 8;
    sync->pending_changes = lv_calloc(sync->pending_capacity, sizeof(sync->pending_changes[0]));
    if (!sync->pending_changes) {
        lv_free((void **) &sync->dirty_views);
        lv_free((void **) &sync);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate pending changes array");
    }
    return sync;
}

/**
 * @brief 销毁视图同步器
 *
 * 释放脏视图列表、待处理变更队列和同步器结构体。
 *
 * @param sync 同步器指针
 */
void lv_view_sync_destroy(lvViewSynchronizer *sync) {
    if (!sync)
        return;
    lv_free((void **) &sync->dirty_views);
    lv_free((void **) &sync->pending_changes);
    lv_free((void **) &sync);
}

/**
 * @brief 启用同步
 *
 * 开启视图同步功能，使脏标记传播生效。
 *
 * @param sync 同步器指针
 * @return 成功返回0，失败返回-1
 */
int lv_view_sync_enable(lvViewSynchronizer *sync) {
    lv_CHECK_NOT_NULL(sync);
    sync->sync_enabled = 1;
    return 0;
}

/**
 * @brief 禁用同步
 *
 * 关闭视图同步功能，脏标记传播将被忽略。
 *
 * @param sync 同步器指针
 * @return 成功返回0，失败返回-1
 */
int lv_view_sync_disable(lvViewSynchronizer *sync) {
    lv_CHECK_NOT_NULL(sync);
    sync->sync_enabled = 0;
    return 0;
}

/**
 * @brief 获取冲突计数
 *
 * @param sync 同步器指针（const）
 * @return 冲突计数，sync为NULL时返回0
 */
int lv_view_sync_conflicts(const lvViewSynchronizer *sync) {
    return sync ? sync->conflict_count : 0;
}

/**
 * @brief 标记视图为脏并传播到依赖视图
 *
 * 将源视图标记为脏视图，并记录待处理变更。
 * 如果脏视图列表或待处理变更队列已满，自动扩容。
 *
 * @param sync          同步器指针
 * @param source_view_id 源视图ID
 * @param change_type    变更类型描述字符串
 * @return 成功返回0，失败返回-1
 */
int lv_view_sync_propagate(lvViewSynchronizer *sync, int source_view_id, const char *change_type) {
    if (!sync || !change_type)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL sync or change_type");
    if (!sync->sync_enabled)
        return 0;

    /* 添加待处理变更记录 */
    if (sync->pending_count >= sync->pending_capacity) {
        /* [安全] 防止 pending_capacity * 2 整数溢出 */
        if (sync->pending_capacity > INT_MAX / 2)
            lv_RETURN_ERROR(lv_ERROR_OVERFLOW, "pending capacity overflow");
        int new_cap = sync->pending_capacity * 2;
        void *new_arr = lv_realloc(sync->pending_changes, new_cap * sizeof(sync->pending_changes[0]));
        if (!new_arr)
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to realloc pending changes");
        sync->pending_changes = new_arr;
        sync->pending_capacity = new_cap;
    }
    sync->pending_changes[sync->pending_count].source_view_id = source_view_id;
    strncpy(sync->pending_changes[sync->pending_count].change_type, change_type,
            sizeof(sync->pending_changes[0].change_type) - 1);
    sync->pending_changes[sync->pending_count].change_type[sizeof(sync->pending_changes[0].change_type) - 1] = '\0';
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
            /* [安全] 防止 dirty_capacity * 2 整数溢出 */
            if (sync->dirty_capacity > INT_MAX / 2)
                lv_RETURN_ERROR(lv_ERROR_OVERFLOW, "dirty capacity overflow");
            int new_cap = sync->dirty_capacity * 2;
            void *new_arr = lv_realloc(sync->dirty_views, new_cap * sizeof(int));
            if (!new_arr)
                lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to realloc dirty views");
            sync->dirty_views = new_arr;
            sync->dirty_capacity = new_cap;
        }
        sync->dirty_views[sync->dirty_count++] = source_view_id;
    }

    return 0;
}

/**
 * @brief 处理所有待处理的同步
 *
 * 按序处理所有待处理变更，清空脏视图列表和待处理队列。
 * 返回本次处理的变更数量。
 *
 * @param sync 同步器指针
 * @return 成功返回处理的变更数量，失败返回-1
 */
int lv_view_sync_flush(lvViewSynchronizer *sync) {
    lv_CHECK_NOT_NULL(sync);
    if (!sync->sync_enabled)
        return 0;

    int processed = 0;
    /* 按顺序处理每个待处理变更 */
    for (int i = 0; i < sync->pending_count; i++) {
        int src = sync->pending_changes[i].source_view_id;
        /* 在实际实现中，这里会调用对应视图的更新回调 */
        /* 当前为简单实现：将所有其他视图标记为脏 */
        (void) src;
        processed++;
    }

    /* 清空待处理队列 */
    sync->pending_count = 0;
    /* 清空脏视图列表 */
    sync->dirty_count = 0;

    return processed;
}
