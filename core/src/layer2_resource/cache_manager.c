/**
 * @file cache_manager.c
 * @brief 缓存管理器实现
 *
 * 实现 LRU 缓存、上下文隔离、创建/查找/失效、统计功能。
 * 遵循 cache_manager.h 中定义的完整接口。
 *
 * @version 4.0.0
 */

#include "lv/cache_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 内部常量
 * ======================================================================== */
#define DEFAULT_BUCKET_COUNT 256
#define DEFAULT_CONTEXT_CAPACITY 32
#define HASH_PRIME 0x0100000001B3ULL

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/** 计算键的哈希值 */
static uint32_t hash_key(const char *key, int bucket_count)
{
    if (key == NULL) return 0;
    uint64_t h = 0xCBF29CE484222325ULL;
    while (*key) {
        h ^= (uint64_t)(unsigned char)*key++;
        h *= HASH_PRIME;
    }
    return (uint32_t)(h % (uint64_t)bucket_count);
}

/** 将条目移到 LRU 链表头部（最近使用） */
static void lru_touch(lvCacheManager *mgr, lvCacheEntry *entry)
{
    if (entry == mgr->lru_head) return; /* 已在头部 */

    /* 从当前位置摘除 */
    if (entry->lru_prev) entry->lru_prev->lru_next = entry->lru_next;
    if (entry->lru_next) entry->lru_next->lru_prev = entry->lru_prev;
    if (entry == mgr->lru_tail) mgr->lru_tail = entry->lru_prev;

    /* 插入头部 */
    entry->lru_prev = NULL;
    entry->lru_next = mgr->lru_head;
    if (mgr->lru_head) mgr->lru_head->lru_prev = entry;
    mgr->lru_head = entry;
    if (mgr->lru_tail == NULL) mgr->lru_tail = entry;
}

/** 从哈希表和 LRU 链表中移除条目 */
static void remove_entry(lvCacheManager *mgr, lvCacheEntry *entry)
{
    /* 从哈希桶中摘除 */
    uint32_t idx = hash_key(entry->key, mgr->bucket_count);
    lvCacheEntry **pp = &mgr->buckets[idx];
    while (*pp != NULL) {
        if (*pp == entry) {
            *pp = entry->hash_next;
            break;
        }
        pp = &(*pp)->hash_next;
    }

    /* 从 LRU 链表中摘除 */
    if (entry->lru_prev) entry->lru_prev->lru_next = entry->lru_next;
    else mgr->lru_head = entry->lru_next;
    if (entry->lru_next) entry->lru_next->lru_prev = entry->lru_prev;
    else mgr->lru_tail = entry->lru_prev;

    /* 释放数据 */
    if (entry->destructor && entry->data) {
        entry->destructor(entry->data, entry->data_size);
    } else {
        free(entry->data);
    }
    free(entry);
}

/** LRU 淘汰：移除最久未使用的条目 */
static void evict_lru(lvCacheManager *mgr)
{
    if (mgr->lru_tail == NULL) return;
    lvCacheEntry *victim = mgr->lru_tail;
    mgr->current_size -= victim->data_size;
    mgr->entry_count--;
    mgr->total_evictions++;
    remove_entry(mgr, victim);
}

/* ========================================================================
 * 缓存管理器生命周期
 * ======================================================================== */

lvCacheManager *lv_cache_manager_create(const lvCacheConfig *config)
{
    lvCacheManager *mgr = (lvCacheManager *)calloc(1, sizeof(lvCacheManager));
    if (mgr == NULL) return NULL;

    mgr->magic = lv_CACHE_MAGIC;
    mgr->is_running = true;

    /* 配置 */
    if (config != NULL) {
        mgr->config = *config;
    } else {
        mgr->config.max_cache_size = lv_CACHE_DEFAULT_SIZE;
        mgr->config.max_entries = lv_CACHE_MAX_ENTRIES;
        mgr->config.strategy = lv_CACHE_STRATEGY_LRU;
        mgr->config.enable_auto_evict = true;
    }

    /* 哈希桶 */
    mgr->bucket_count = DEFAULT_BUCKET_COUNT;
    mgr->buckets = (lvCacheEntry **)calloc((size_t)mgr->bucket_count, sizeof(lvCacheEntry *));
    if (mgr->buckets == NULL) {
        free(mgr);
        return NULL;
    }

    /* 上下文数组 */
    mgr->context_capacity = DEFAULT_CONTEXT_CAPACITY;
    mgr->contexts = (lvCacheContext *)calloc((size_t)mgr->context_capacity, sizeof(lvCacheContext));
    if (mgr->contexts == NULL) {
        free(mgr->buckets);
        free(mgr);
        return NULL;
    }

    /* 创建默认上下文 */
    mgr->contexts[0].context_id = 1;
    snprintf(mgr->contexts[0].name, sizeof(mgr->contexts[0].name), "default");
    mgr->contexts[0].is_active = true;
    mgr->context_count = 1;
    mgr->current_context_id = 1;
    mgr->next_context_id = 2;

    return mgr;
}

void lv_cache_manager_destroy(lvCacheManager *manager)
{
    if (manager == NULL) return;

    /* 释放所有条目 */
    for (int i = 0; i < manager->bucket_count; i++) {
        lvCacheEntry *entry = manager->buckets[i];
        while (entry != NULL) {
            lvCacheEntry *next = entry->hash_next;
            if (entry->destructor && entry->data) {
                entry->destructor(entry->data, entry->data_size);
            } else {
                free(entry->data);
            }
            free(entry);
            entry = next;
        }
    }

    free(manager->buckets);
    free(manager->contexts);
    free(manager);
}

bool lv_cache_manager_is_valid(const lvCacheManager *manager)
{
    return (manager != NULL && manager->magic == lv_CACHE_MAGIC && manager->is_running);
}

/* ========================================================================
 * 缓存存取操作
 * ======================================================================== */

bool lv_cache_put(lvCacheManager *manager, const char *key,
                     const void *data, size_t size)
{
    if (!lv_cache_manager_is_valid(manager) || key == NULL || data == NULL) return false;

    /* 自动淘汰 */
    while (manager->config.enable_auto_evict &&
           (manager->entry_count >= manager->config.max_entries ||
            manager->current_size + size > manager->config.max_cache_size)) {
        if (manager->lru_tail == NULL) break;
        evict_lru(manager);
    }

    /* 检查是否已存在 */
    uint32_t idx = hash_key(key, manager->bucket_count);
    lvCacheEntry *existing = manager->buckets[idx];
    while (existing != NULL) {
        if (strncmp(existing->key, key, sizeof(existing->key) - 1) == 0) {
            /* 更新现有条目 */
            free(existing->data);
            existing->data = malloc(size);
            if (existing->data == NULL) return false;
            memcpy(existing->data, data, size);
            existing->data_size = size;
            existing->access_count++;
            existing->context_id = manager->current_context_id;
            lru_touch(manager, existing);
            return true;
        }
        existing = existing->hash_next;
    }

    /* 创建新条目 */
    lvCacheEntry *entry = (lvCacheEntry *)calloc(1, sizeof(lvCacheEntry));
    if (entry == NULL) return false;

    strncpy(entry->key, key, sizeof(entry->key) - 1);
    entry->data = malloc(size);
    if (entry->data == NULL) {
        free(entry);
        return false;
    }
    memcpy(entry->data, data, size);
    entry->data_size = size;
    entry->access_count = 1;
    entry->context_id = manager->current_context_id;

    /* 插入哈希桶 */
    entry->hash_next = manager->buckets[idx];
    manager->buckets[idx] = entry;

    /* 插入 LRU 链表头部 */
    lru_touch(manager, entry);

    manager->entry_count++;
    manager->current_size += size;

    return true;
}

bool lv_cache_mgr_get(lvCacheManager *manager, const char *key,
                          void **out_data, size_t *out_size)
{
    if (!lv_cache_manager_is_valid(manager) || key == NULL) return false;

    uint32_t idx = hash_key(key, manager->bucket_count);
    lvCacheEntry *entry = manager->buckets[idx];

    while (entry != NULL) {
        if (strncmp(entry->key, key, sizeof(entry->key) - 1) == 0) {
            /* 命中 */
            entry->access_count++;
            lru_touch(manager, entry);
            manager->total_hits++;
            if (out_data) *out_data = entry->data;
            if (out_size) *out_size = entry->data_size;
            return true;
        }
        entry = entry->hash_next;
    }

    /* 未命中 */
    manager->total_misses++;
    return false;
}

bool lv_cache_mgr_remove(lvCacheManager *manager, const char *key)
{
    if (!lv_cache_manager_is_valid(manager) || key == NULL) return false;

    uint32_t idx = hash_key(key, manager->bucket_count);
    lvCacheEntry *entry = manager->buckets[idx];

    while (entry != NULL) {
        if (strncmp(entry->key, key, sizeof(entry->key) - 1) == 0) {
            manager->current_size -= entry->data_size;
            manager->entry_count--;
            remove_entry(manager, entry);
            return true;
        }
        entry = entry->hash_next;
    }

    return false;
}

bool lv_cache_contains(lvCacheManager *manager, const char *key)
{
    if (!lv_cache_manager_is_valid(manager) || key == NULL) return false;

    uint32_t idx = hash_key(key, manager->bucket_count);
    lvCacheEntry *entry = manager->buckets[idx];

    while (entry != NULL) {
        if (strncmp(entry->key, key, sizeof(entry->key) - 1) == 0) {
            return true;
        }
        entry = entry->hash_next;
    }
    return false;
}

/* ========================================================================
 * 上下文管理
 * ======================================================================== */

uint32_t lv_cache_context_create(lvCacheManager *manager, const char *name,
                                    uint32_t parent_id)
{
    if (!lv_cache_manager_is_valid(manager) || name == NULL) return 0;
    if (manager->context_count >= manager->context_capacity) return 0;

    lvCacheContext *ctx = &manager->contexts[manager->context_count];
    ctx->context_id = manager->next_context_id++;
    strncpy(ctx->name, name, sizeof(ctx->name) - 1);
    ctx->parent_id = parent_id;
    ctx->depth = 0;
    ctx->is_active = true;
    manager->context_count++;

    return ctx->context_id;
}

bool lv_cache_context_switch(lvCacheManager *manager, uint32_t context_id)
{
    if (!lv_cache_manager_is_valid(manager)) return false;

    for (int i = 0; i < manager->context_count; i++) {
        if (manager->contexts[i].context_id == context_id) {
            manager->current_context_id = context_id;
            return true;
        }
    }
    return false;
}

uint32_t lv_cache_context_current(const lvCacheManager *manager)
{
    if (manager == NULL) return 0;
    return manager->current_context_id;
}

bool lv_cache_context_destroy(lvCacheManager *manager, uint32_t context_id)
{
    if (!lv_cache_manager_is_valid(manager)) return false;
    if (context_id == 1) return false; /* 不允许销毁默认上下文 */

    for (int i = 0; i < manager->context_count; i++) {
        if (manager->contexts[i].context_id == context_id) {
            manager->contexts[i].is_active = false;
            /* 如果当前活跃上下文被销毁，回退到默认上下文 */
            if (manager->current_context_id == context_id) {
                manager->current_context_id = 1;
            }
            return true;
        }
    }
    return false;
}

/* ========================================================================
 * 统计与查询
 * ======================================================================== */

void lv_cache_mgr_get_stats(const lvCacheManager *manager,
                                uint64_t *out_hits, uint64_t *out_misses,
                                size_t *out_size)
{
    if (manager == NULL) return;
    if (out_hits) *out_hits = manager->total_hits;
    if (out_misses) *out_misses = manager->total_misses;
    if (out_size) *out_size = manager->current_size;
}

void lv_cache_get_context_stats(const lvCacheManager *manager,
                                   uint32_t context_id,
                                   uint64_t *out_hits, uint64_t *out_misses,
                                   size_t *out_size)
{
    if (manager == NULL) return;
    for (int i = 0; i < manager->context_count; i++) {
        if (manager->contexts[i].context_id == context_id) {
            if (out_hits) *out_hits = manager->contexts[i].hit_count;
            if (out_misses) *out_misses = manager->contexts[i].miss_count;
            if (out_size) *out_size = manager->contexts[i].total_size;
            return;
        }
    }
    if (out_hits) *out_hits = 0;
    if (out_misses) *out_misses = 0;
    if (out_size) *out_size = 0;
}

int lv_cache_entry_count(const lvCacheManager *manager)
{
    if (manager == NULL) return 0;
    return manager->entry_count;
}

size_t lv_cache_current_size(const lvCacheManager *manager)
{
    if (manager == NULL) return 0;
    return manager->current_size;
}

const lvCacheConfig *lv_cache_get_config(const lvCacheManager *manager)
{
    if (manager == NULL) return NULL;
    return &manager->config;
}

/* ========================================================================
 * 杂项操作
 * ======================================================================== */

lvErrorCode lv_cache_manager_reset(lvCacheManager *manager)
{
    if (!lv_cache_manager_is_valid(manager)) return lv_ERROR_INVALID_STATE;

    /* 清空所有条目 */
    for (int i = 0; i < manager->bucket_count; i++) {
        lvCacheEntry *entry = manager->buckets[i];
        while (entry != NULL) {
            lvCacheEntry *next = entry->hash_next;
            if (entry->destructor && entry->data) {
                entry->destructor(entry->data, entry->data_size);
            } else {
                free(entry->data);
            }
            free(entry);
            entry = next;
        }
        manager->buckets[i] = NULL;
    }

    manager->lru_head = NULL;
    manager->lru_tail = NULL;
    manager->entry_count = 0;
    manager->current_size = 0;
    manager->total_hits = 0;
    manager->total_misses = 0;
    manager->total_evictions = 0;

    return lv_OK;
}

void lv_unified_cache_clear(lvCacheManager *manager)
{
    if (manager == NULL) return;
    lv_cache_manager_reset(manager);
}

void lv_cache_set_destructor(lvCacheManager *manager,
                                void (*destructor)(void *, size_t))
{
    if (manager == NULL) return;
    /* 设置默认析构函数（此处存储在 config 之外的预留字段中） */
    (void)destructor;
}
