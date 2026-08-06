/**
 * @file lv_callback_list.c
 * @brief 泛型回调列表 —— 注册/注销/分发公共设施实现
 *
 * @details 从 stream（CallbackEntry 数组）与 event_bus（lvEventSubscription 数组）
 *          提取的公共回调列表实现。语义对齐 stream_context.c 蓝本：
 *          - 注册：末尾追加，返回自增 ID（从 1 开始）
 *          - 注销：前移紧凑（lv_shift_left），保持注册顺序
 *          - 分发：快照 count + 越界检查的迭代安全遍历
 *          - 扩容：lv_ensure_capacity 倍增，硬上限 max_entries
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date   2026-08-06
 */

#include "lv/lv_callback_list.h"
#include "lv/lv_utils.h"

#include <string.h>

/* ==================== 生命周期 ==================== */

void lv_callback_list_init(lvCallbackList *list, int initial_capacity, int max_entries) {
    if (!list)
        return;
    memset(list, 0, sizeof(*list));
    list->next_id = 1;
    list->max_entries = max_entries;

    int cap = initial_capacity > 0 ? initial_capacity : lv_CALLBACK_LIST_DEFAULT_CAPACITY;
    list->entries = (lvCallbackEntry *) lv_calloc(1, (size_t) cap * sizeof(lvCallbackEntry));
    if (list->entries) {
        list->capacity = cap;
    }
    /* 预分配失败不致命：后续注册时 lv_callback_list_add 会惰性扩容兜底 */
}

void lv_callback_list_cleanup(lvCallbackList *list) {
    if (!list)
        return;
    if (list->entries)
        lv_free((void **) &list->entries);
    memset(list, 0, sizeof(*list));
}

void lv_callback_list_clear(lvCallbackList *list) {
    if (!list || !list->entries)
        return;
    memset(list->entries, 0, (size_t) list->count * sizeof(lvCallbackEntry));
    list->count = 0;
}

/* ==================== 注册 / 注销 ==================== */

int lv_callback_list_add(lvCallbackList *list, lvCallbackFn callback, void *user_data, uint64_t filter) {
    if (!list || !callback)
        return -1;
    /* 硬上限检查（max_entries > 0 时最多容纳 max_entries 个回调） */
    if (list->max_entries > 0 && list->count >= list->max_entries)
        return -1;
    /* 动态扩容确保足够容量 */
    if (!lv_ensure_capacity((void **) &list->entries, list->count, &list->capacity,
                            sizeof(lvCallbackEntry), 1))
        return -1;

    int idx = list->count;
    list->entries[idx].callback = callback;
    list->entries[idx].user_data = user_data;
    list->entries[idx].id = list->next_id++;
    list->entries[idx].filter = filter;
    list->count++;
    return list->entries[idx].id;
}

bool lv_callback_list_remove_by_id(lvCallbackList *list, int id) {
    if (!list || id <= 0)
        return false;
    for (int i = 0; i < list->count; i++) {
        if (list->entries[i].id == id) {
            /* 后续回调前移一位（统一走 lv_shift_left 的 memmove 路径） */
            lv_shift_left(list->entries, sizeof(list->entries[0]), (size_t) i, (size_t) list->count);
            list->count--;
            return true;
        }
    }
    return false;
}

bool lv_callback_list_remove_by_fn(lvCallbackList *list, lvCallbackFn callback) {
    if (!list || !callback)
        return false;
    for (int i = 0; i < list->count; i++) {
        if (list->entries[i].callback == callback) {
            /* 后续回调前移一位（统一走 lv_shift_left 的 memmove 路径） */
            lv_shift_left(list->entries, sizeof(list->entries[0]), (size_t) i, (size_t) list->count);
            list->count--;
            return true;
        }
    }
    return false;
}

bool lv_callback_list_set_filter(lvCallbackList *list, int id, uint64_t filter) {
    if (!list || id <= 0)
        return false;
    for (int i = 0; i < list->count; i++) {
        if (list->entries[i].id == id) {
            list->entries[i].filter = filter;
            return true;
        }
    }
    return false;
}

uint64_t lv_callback_list_get_filter(const lvCallbackList *list, int id) {
    if (!list || id <= 0)
        return 0;
    for (int i = 0; i < list->count; i++) {
        if (list->entries[i].id == id) {
            return list->entries[i].filter;
        }
    }
    return 0;
}

/* ==================== 查询 ==================== */

int lv_callback_list_count(const lvCallbackList *list) {
    if (!list)
        return 0;
    return list->count;
}

/* ==================== 分发 ==================== */

void lv_callback_list_dispatch(lvCallbackList *list, const void *dispatch_arg,
                               lvCallbackFilterFn filter, lvCallbackInvokeFn invoke) {
    if (!list || !invoke)
        return;
    /* 快照遍历开始时的回调数量，防止回调函数中注册/注销回调导致迭代器失效 */
    int saved_count = list->count;
    for (int i = 0; i < saved_count; i++) {
        /* 检查索引是否仍然有效（回调可能已被注销导致前移） */
        if (i >= list->count)
            break;
        lvCallbackEntry *entry = &list->entries[i];
        if (filter && !filter(entry, dispatch_arg))
            continue;
        invoke(entry, dispatch_arg);
    }
}
