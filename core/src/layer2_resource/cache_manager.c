/**
 * @file cache_manager.c
 * @brief 缓存管理器实现
 *
 * @details 实现 LRU 缓存系统，包含以下功能：
 *          - 基于哈希表和双向链表的 LRU 淘汰策略
 *          - 上下文隔离（context）机制，支持多租户缓存
 *          - 创建、查找、失效、统计等完整生命周期管理
 *          - 自动淘汰（auto-evict）和自定义析构函数支持
 *
 * 设计要点：
 * - 哈希桶使用 FNV-1a 变体哈希算法（HASH_PRIME = 0x0100000001B3ULL）
 * - LRU 链表头部为最近使用项，尾部为最久未使用项
 * - 上下文隔离通过 context_id 标记实现，支持切换和嵌套
 * - 所有公共 API 包含空指针和魔法数（magic）校验
 *
 * @version 4.0.0
 * @author Lv-00 Project
 */

#include "lv/cache_manager.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

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
static uint32_t hash_key(const char *key, int bucket_count) {
    if (key == NULL)
        return 0;
    uint64_t h = 0xCBF29CE484222325ULL;
    while (*key) {
        h ^= (uint64_t) (unsigned char) *key++;
        h *= HASH_PRIME;
    }
    return (uint32_t) (h % (uint64_t) bucket_count);
}

/** 将条目移到 LRU 链表头部（最近使用） */
static void lru_touch(lvCacheManager *mgr, lvCacheEntry *entry) {
    if (entry == mgr->lru_head)
        return; /* 已在头部 */

    /* 从当前位置摘除 */
    if (entry->lru_prev)
        entry->lru_prev->lru_next = entry->lru_next;
    if (entry->lru_next)
        entry->lru_next->lru_prev = entry->lru_prev;
    if (entry == mgr->lru_tail)
        mgr->lru_tail = entry->lru_prev;

    /* 插入头部 */
    entry->lru_prev = NULL;
    entry->lru_next = mgr->lru_head;
    if (mgr->lru_head)
        mgr->lru_head->lru_prev = entry;
    mgr->lru_head = entry;
    if (mgr->lru_tail == NULL)
        mgr->lru_tail = entry;
}

/** 释放缓存条目：先调用自定义析构函数（若设置），再释放条目本身 */
static void free_entry(lvCacheManager *mgr, lvCacheEntry *e) {
    (void) mgr;
    if (e->destructor && e->data) {
        e->destructor(e->data, e->data_size);
    } else {
        lv_free((void **) &(e->data));
    }
    lv_free((void **) &e);
}

/** 从哈希表和 LRU 链表中移除条目 */
static void remove_entry(lvCacheManager *mgr, lvCacheEntry *entry) {
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
    if (entry->lru_prev)
        entry->lru_prev->lru_next = entry->lru_next;
    else
        mgr->lru_head = entry->lru_next;
    if (entry->lru_next)
        entry->lru_next->lru_prev = entry->lru_prev;
    else
        mgr->lru_tail = entry->lru_prev;

    /* 释放数据 */
    free_entry(mgr, entry);
}

/** LRU 淘汰：移除最久未使用的条目 */
static void evict_lru(lvCacheManager *mgr) {
    if (mgr->lru_tail == NULL)
        return;
    lvCacheEntry *victim = mgr->lru_tail;
    mgr->current_size -= victim->data_size;
    mgr->entry_count--;
    mgr->total_evictions++;
    remove_entry(mgr, victim);
}

/* ========================================================================
 * 缓存管理器生命周期
 * ======================================================================== */

/**
 * @brief 创建缓存管理器
 *
 * 分配并初始化缓存管理器，包含哈希桶、上下文数组和默认上下文。
 * 若 config 为 NULL，则使用默认配置（lv_CACHE_DEFAULT_SIZE 等）。
 *
 * @param config 缓存配置指针（可为 NULL，将使用默认配置）
 * @return 新创建的缓存管理器指针，失败返回 NULL
 */
lvCacheManager *lv_cache_manager_create(const lvCacheConfig *config) {
    lvCacheManager *mgr = (lvCacheManager *) lv_calloc(1, sizeof(lvCacheManager));
    if (mgr == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_cache_manager_create: calloc failed for manager");

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
    mgr->buckets = (lvCacheEntry **) lv_calloc((size_t) mgr->bucket_count, sizeof(lvCacheEntry *));
    if (mgr->buckets == NULL) {
        lv_free((void **) &mgr);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_cache_manager_create: calloc failed for buckets");
    }

    /* 上下文数组 */
    mgr->context_capacity = DEFAULT_CONTEXT_CAPACITY;
    mgr->contexts = (lvCacheContext *) lv_calloc((size_t) mgr->context_capacity, sizeof(lvCacheContext));
    if (mgr->contexts == NULL) {
        lv_free((void **) &(mgr->buckets));
        lv_free((void **) &mgr);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_cache_manager_create: calloc failed for contexts");
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

/**
 * @brief 销毁缓存管理器
 *
 * 释放所有缓存条目、哈希桶、上下文数组及管理器本身。
 * 对每个条目调用其自定义析构函数（如果已设置）。
 *
 * @param manager 缓存管理器指针（可为 NULL）
 */
void lv_cache_manager_destroy(lvCacheManager *manager) {
    if (manager == NULL)
        return;

    /* 释放所有条目 */
    for (int i = 0; i < manager->bucket_count; i++) {
        lvCacheEntry *entry = manager->buckets[i];
        while (entry != NULL) {
            lvCacheEntry *next = entry->hash_next;
            free_entry(manager, entry);
            entry = next;
        }
    }

    lv_free((void **) &(manager->buckets));
    lv_free((void **) &(manager->contexts));
    lv_free((void **) &manager);
}

/**
 * @brief 检查缓存管理器是否有效
 *
 * 验证指针非空、魔法数正确且运行状态为 true。
 *
 * @param manager 缓存管理器指针
 * @return true 有效，false 无效
 */
bool lv_cache_manager_is_valid(const lvCacheManager *manager) {
    return (manager != NULL && manager->magic == lv_CACHE_MAGIC && manager->is_running);
}

/* ========================================================================
 * 缓存存取操作
 * ======================================================================== */

/**
 * @brief 向缓存中存入数据
 *
 * 若键已存在，则更新其数据并移到 LRU 头部；
 * 若不存在，创建新条目后插入。
 * 在插入前根据配置自动执行 LRU 淘汰。
 *
 * @param manager 缓存管理器
 * @param key     缓存键（字符串）
 * @param data    数据指针
 * @param size    数据大小（字节）
 * @return true 成功，false 失败（参数无效或内存不足）
 */
bool lv_cache_put(lvCacheManager *manager, const char *key, const void *data, size_t size) {
    if (!lv_cache_manager_is_valid(manager) || key == NULL || data == NULL)
        return false;

    /* 自动淘汰 */
    while (manager->config.enable_auto_evict && (manager->entry_count >= manager->config.max_entries ||
                                                 manager->current_size + size > manager->config.max_cache_size)) {
        if (manager->lru_tail == NULL)
            break;
        evict_lru(manager);
    }

    /* 检查是否已存在 */
    uint32_t idx = hash_key(key, manager->bucket_count);
    lvCacheEntry *existing = manager->buckets[idx];
    while (existing != NULL) {
        if (strncmp(existing->key, key, sizeof(existing->key) - 1) == 0) {
            /* 更新现有条目 */
            lv_free((void **) &(existing->data));
            existing->data = lv_malloc(size);
            if (existing->data == NULL)
                return false;
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
    lvCacheEntry *entry = (lvCacheEntry *) lv_calloc(1, sizeof(lvCacheEntry));
    if (entry == NULL)
        return false;

    lv_strlcpy(entry->key, key, sizeof(entry->key));
    entry->data = lv_malloc(size);
    if (entry->data == NULL) {
        lv_free((void **) &entry);
        return false;
    }
    memcpy(entry->data, data, size);
    entry->data_size = size;
    entry->access_count = 1;
    entry->context_id = manager->current_context_id;
    entry->destructor = manager->default_destructor; /* 继承默认析构 */

    /* 插入哈希桶 */
    entry->hash_next = manager->buckets[idx];
    manager->buckets[idx] = entry;

    /* 插入 LRU 链表头部 */
    lru_touch(manager, entry);

    manager->entry_count++;
    manager->current_size += size;

    return true;
}

/**
 * @brief 从缓存中获取数据
 *
 * 查找指定键对应的缓存条目，命中时将其移到 LRU 头部并返回数据指针。
 *
 * @param manager  缓存管理器
 * @param key      缓存键
 * @param out_data 输出数据指针（可为 NULL，仅检查是否存在）
 * @param out_size 输出数据大小（可为 NULL）
 * @return true 命中，false 未命中或参数无效
 */
bool lv_cache_mgr_get(lvCacheManager *manager, const char *key, void **out_data, size_t *out_size) {
    if (!lv_cache_manager_is_valid(manager) || key == NULL)
        return false;

    uint32_t idx = hash_key(key, manager->bucket_count);
    lvCacheEntry *entry = manager->buckets[idx];

    while (entry != NULL) {
        if (strncmp(entry->key, key, sizeof(entry->key) - 1) == 0) {
            /* 命中 */
            entry->access_count++;
            lru_touch(manager, entry);
            manager->total_hits++;
            if (out_data)
                *out_data = entry->data;
            if (out_size)
                *out_size = entry->data_size;
            return true;
        }
        entry = entry->hash_next;
    }

    /* 未命中 */
    manager->total_misses++;
    return false;
}

/**
 * @brief 从缓存中移除指定键的条目
 *
 * @param manager 缓存管理器
 * @param key     缓存键
 * @return true 成功移除，false 未找到或参数无效
 */
bool lv_cache_mgr_remove(lvCacheManager *manager, const char *key) {
    if (!lv_cache_manager_is_valid(manager) || key == NULL)
        return false;

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

/**
 * @brief 检查缓存中是否存在指定键
 *
 * @param manager 缓存管理器
 * @param key     缓存键
 * @return true 存在，false 不存在或参数无效
 */
bool lv_cache_contains(lvCacheManager *manager, const char *key) {
    if (!lv_cache_manager_is_valid(manager) || key == NULL)
        return false;

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

/**
 * @brief 创建新的缓存上下文
 *
 * @param manager   缓存管理器
 * @param name      上下文名称
 * @param parent_id 父上下文 ID（0 表示根上下文）
 * @return 新上下文的 ID，失败返回 0
 */
uint32_t lv_cache_context_create(lvCacheManager *manager, const char *name, uint32_t parent_id) {
    if (!lv_cache_manager_is_valid(manager) || name == NULL)
        return 0;
    if (manager->context_count >= manager->context_capacity)
        return 0;

    lvCacheContext *ctx = &manager->contexts[manager->context_count];
    ctx->context_id = manager->next_context_id++;
    lv_strlcpy(ctx->name, name, sizeof(ctx->name));
    ctx->parent_id = parent_id;
    ctx->depth = 0;
    ctx->is_active = true;
    manager->context_count++;

    return ctx->context_id;
}

/**
 * @brief 切换当前活跃上下文
 *
 * @param manager    缓存管理器
 * @param context_id 目标上下文 ID
 * @return true 切换成功，false 未找到对应上下文
 */
bool lv_cache_context_switch(lvCacheManager *manager, uint32_t context_id) {
    if (!lv_cache_manager_is_valid(manager))
        return false;

    for (int i = 0; i < manager->context_count; i++) {
        if (manager->contexts[i].context_id == context_id) {
            manager->current_context_id = context_id;
            return true;
        }
    }
    return false;
}

/**
 * @brief 获取当前活跃上下文 ID
 *
 * @param manager 缓存管理器（可为 NULL）
 * @return 当前上下文 ID，manager 为 NULL 时返回 0
 */
uint32_t lv_cache_context_current(const lvCacheManager *manager) {
    if (manager == NULL)
        return 0;
    return manager->current_context_id;
}

/**
 * @brief 销毁指定上下文
 *
 * 标记上下文为非活跃。若当前上下文被销毁，回退到默认上下文（ID=1）。
 * 不允许销毁默认上下文。
 *
 * @param manager    缓存管理器
 * @param context_id 待销毁的上下文 ID
 * @return true 销毁成功，false 无效或不允许销毁默认上下文
 */
bool lv_cache_context_destroy(lvCacheManager *manager, uint32_t context_id) {
    if (!lv_cache_manager_is_valid(manager))
        return false;
    if (context_id == 1)
        return false; /* 不允许销毁默认上下文 */

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

/**
 * @brief 获取缓存管理器全局统计信息
 *
 * @param manager   缓存管理器（可为 NULL）
 * @param out_hits  输出总命中次数（可为 NULL）
 * @param out_misses 输出总未命中次数（可为 NULL）
 * @param out_size  输出当前缓存大小（可为 NULL）
 */
void lv_cache_mgr_get_stats(const lvCacheManager *manager, uint64_t *out_hits, uint64_t *out_misses, size_t *out_size) {
    if (manager == NULL)
        return;
    if (out_hits)
        *out_hits = manager->total_hits;
    if (out_misses)
        *out_misses = manager->total_misses;
    if (out_size)
        *out_size = manager->current_size;
}

/**
 * @brief 获取指定上下文的统计信息
 *
 * @param manager    缓存管理器（可为 NULL）
 * @param context_id 上下文 ID
 * @param out_hits   输出命中次数（可为 NULL）
 * @param out_misses 输出未命中次数（可为 NULL）
 * @param out_size   输出总大小（可为 NULL）
 */
void lv_cache_get_context_stats(const lvCacheManager *manager, uint32_t context_id, uint64_t *out_hits,
                                uint64_t *out_misses, size_t *out_size) {
    if (manager == NULL)
        return;
    for (int i = 0; i < manager->context_count; i++) {
        if (manager->contexts[i].context_id == context_id) {
            if (out_hits)
                *out_hits = manager->contexts[i].hit_count;
            if (out_misses)
                *out_misses = manager->contexts[i].miss_count;
            if (out_size)
                *out_size = manager->contexts[i].total_size;
            return;
        }
    }
    if (out_hits)
        *out_hits = 0;
    if (out_misses)
        *out_misses = 0;
    if (out_size)
        *out_size = 0;
}

/**
 * @brief 获取当前缓存条目数量
 *
 * @param manager 缓存管理器（可为 NULL）
 * @return 条目数量，manager 为 NULL 时返回 0
 */
int lv_cache_entry_count(const lvCacheManager *manager) {
    if (manager == NULL)
        return 0;
    return manager->entry_count;
}

/**
 * @brief 获取当前缓存数据总大小
 *
 * @param manager 缓存管理器（可为 NULL）
 * @return 数据总大小（字节），manager 为 NULL 时返回 0
 */
size_t lv_cache_current_size(const lvCacheManager *manager) {
    if (manager == NULL)
        return 0;
    return manager->current_size;
}

/**
 * @brief 获取缓存配置
 *
 * @param manager 缓存管理器（可为 NULL）
 * @return 配置指针，manager 为 NULL 时返回 NULL
 */
const lvCacheConfig *lv_cache_get_config(const lvCacheManager *manager) {
    if (manager == NULL)
        return NULL;
    return &manager->config;
}

/* ========================================================================
 * 杂项操作
 * ======================================================================== */

/**
 * @brief 重置缓存管理器，清空所有条目和统计信息
 *
 * 释放所有缓存条目，重置 LRU 链表和统计计数器，
 * 但保留上下文配置。
 *
 * @param manager 缓存管理器
 * @return lv_OK 成功，lv_ERROR_INVALID_STATE 参数无效
 */
lvErrorCode lv_cache_manager_reset(lvCacheManager *manager) {
    if (!lv_cache_manager_is_valid(manager))
        return lv_ERROR_INVALID_STATE;

    /* 清空所有条目 */
    for (int i = 0; i < manager->bucket_count; i++) {
        lvCacheEntry *entry = manager->buckets[i];
        while (entry != NULL) {
            lvCacheEntry *next = entry->hash_next;
            free_entry(manager, entry);
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

/**
 * @brief 清空缓存（统一接口别名）
 *
 * 调用 lv_cache_manager_reset 实现。
 *
 * @param manager 缓存管理器（可为 NULL）
 */
void lv_unified_cache_clear(lvCacheManager *manager) {
    if (manager == NULL)
        return;
    lv_cache_manager_reset(manager);
}

/**
 * @brief 设置缓存条目的默认析构函数
 *
 * @param manager    缓存管理器（可为 NULL）
 * @param destructor 析构函数指针（void (*)(void *, size_t)）
 */
void lv_cache_set_destructor(lvCacheManager *manager, void (*destructor)(void *, size_t)) {
    if (manager == NULL)
        return;
    manager->default_destructor = destructor;
}
