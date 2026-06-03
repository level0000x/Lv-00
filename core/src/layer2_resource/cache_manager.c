/**
 * @file cache_manager.c
 * @brief Lv-00 缓存隔离层管理器实现
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供 LRU 淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 4.0.0
 * @author Lv-00 Project
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#include "lv00/cache_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00/lv00_utils.h"
#include "lv00/circuit_breaker.h"

/* ============================================================
 * 内部常量
 * ============================================================ */

/** 默认哈希桶数量（素数，减少冲突） */
#define CACHE_DEFAULT_BUCKET_COUNT 257

/** 默认上下文初始容量 */
#define CACHE_DEFAULT_CONTEXT_CAPACITY 8

/* ============================================================
 * 内部辅助函数（前向声明）
 * ============================================================ */

static uint32_t hash_key(const char *key);
static Lv00CacheEntry *find_entry(Lv00CacheManager *manager, const char *key);
static void lru_move_to_head(Lv00CacheManager *manager, Lv00CacheEntry *entry);
static void lru_remove(Lv00CacheManager *manager, Lv00CacheEntry *entry);
static void lru_push_front(Lv00CacheManager *manager, Lv00CacheEntry *entry);
static bool evict_one(Lv00CacheManager *manager);
static void free_entry_data(Lv00CacheEntry *entry);
static void free_entry(Lv00CacheEntry *entry);
static Lv00CacheContext *find_context(Lv00CacheManager *manager, uint32_t context_id);

/* ============================================================
 * 哈希函数 —— FNV-1a（32位）
 * ============================================================ */

/**
 * @brief 使用 FNV-1a 算法计算字符串哈希值
 *
 * FNV-1a 是一种简单高效的哈希算法，具有良好的分布特性。
 * 适合用于缓存键的哈希映射。
 *
 * @param key 输入字符串
 * @return 32位哈希值
 */
static uint32_t hash_key(const char *key)
{
    if (!key) return 0;

    uint32_t hash = 2166136261u;  /* FNV offset basis */
    for (const char *p = key; *p != '\0'; p++) {
        hash ^= (uint8_t)*p;
        hash *= 16777619u;        /* FNV prime */
    }
    return hash;
}

/* ============================================================
 * 哈希表查找
 * ============================================================ */

/**
 * @brief 在哈希表中查找指定键的缓存条目
 *
 * 通过哈希值定位桶，然后在冲突链中线性查找匹配的键。
 *
 * @param manager 缓存管理器
 * @param key 要查找的键
 * @return 找到的条目指针，未找到返回 NULL
 */
static Lv00CacheEntry *find_entry(Lv00CacheManager *manager, const char *key)
{
    if (!manager || !key) return NULL;

    uint32_t idx = hash_key(key) % (uint32_t)manager->bucket_count;
    Lv00CacheEntry *entry = manager->buckets[idx];

    while (entry) {
        if (strncmp(entry->key, key, sizeof(entry->key)) == 0) {
            return entry;
        }
        entry = entry->hash_next;
    }

    return NULL;
}

/* ============================================================
 * LRU 双向链表操作
 * ============================================================ */

/**
 * @brief 将条目从 LRU 链表中移除
 *
 * 从双向链表中摘除指定节点，不释放内存。
 *
 * @param manager 缓存管理器
 * @param entry 要移除的条目
 */
static void lru_remove(Lv00CacheManager *manager, Lv00CacheEntry *entry)
{
    if (!manager || !entry) return;

    if (entry->lru_prev) {
        entry->lru_prev->lru_next = entry->lru_next;
    } else {
        /* entry 是头节点 */
        manager->lru_head = entry->lru_next;
    }

    if (entry->lru_next) {
        entry->lru_next->lru_prev = entry->lru_prev;
    } else {
        /* entry 是尾节点 */
        manager->lru_tail = entry->lru_prev;
    }

    entry->lru_prev = NULL;
    entry->lru_next = NULL;
}

/**
 * @brief 将条目插入到 LRU 链表头部（最近使用位置）
 *
 * @param manager 缓存管理器
 * @param entry 要插入的条目
 */
static void lru_push_front(Lv00CacheManager *manager, Lv00CacheEntry *entry)
{
    if (!manager || !entry) return;

    entry->lru_prev = NULL;
    entry->lru_next = manager->lru_head;

    if (manager->lru_head) {
        manager->lru_head->lru_prev = entry;
    }
    manager->lru_head = entry;

    if (!manager->lru_tail) {
        manager->lru_tail = entry;
    }
}

/**
 * @brief 将已存在的条目移动到 LRU 链表头部
 *
 * 访问缓存条目时调用，标记为最近使用。
 *
 * @param manager 缓存管理器
 * @param entry 要移动的条目
 */
static void lru_move_to_head(Lv00CacheManager *manager, Lv00CacheEntry *entry)
{
    if (!manager || !entry) return;

    /* 如果已经在头部，无需移动 */
    if (manager->lru_head == entry) return;

    lru_remove(manager, entry);
    lru_push_front(manager, entry);
}

/* ============================================================
 * 条目数据释放
 * ============================================================ */

/**
 * @brief 释放缓存条目的数据内容
 *
 * 如果条目设置了自定义析构函数，则调用析构函数；
 * 否则直接释放数据指针。同时释放数据块链表。
 *
 * @param entry 缓存条目
 */
static void free_entry_data(Lv00CacheEntry *entry)
{
    if (!entry) return;

    /* 释放数据块链表 */
    Lv00DataChunk *chunk = entry->chunks;
    while (chunk) {
        Lv00DataChunk *next = chunk->next;
        { void *_tmp = chunk->data; lv00_free(&_tmp); chunk->data = NULL; }
        { void *_tmp = (void *)chunk; lv00_free(&_tmp); chunk = NULL; }
        chunk = next;
    }
    entry->chunks = NULL;
    entry->chunk_count = 0;

    /* 释放主数据 */
    if (entry->data) {
        if (entry->destructor) {
            entry->destructor(entry->data, entry->data_size);
        } else {
            { void *_tmp = entry->data; lv00_free(&_tmp); entry->data = NULL; }
        }
        entry->data = NULL;
        entry->data_size = 0;
    }
}

/**
 * @brief 释放整个缓存条目（包括数据）
 *
 * @param entry 缓存条目
 */
static void free_entry(Lv00CacheEntry *entry)
{
    if (!entry) return;
    free_entry_data(entry);
    { void *_tmp = (void *)entry; lv00_free(&_tmp); entry = NULL; }
}

/* ============================================================
 * 上下文查找
 * ============================================================ */

/**
 * @brief 根据 context_id 查找上下文
 *
 * @param manager 缓存管理器
 * @param context_id 上下文ID
 * @return 找到的上下文指针，未找到返回 NULL
 */
static Lv00CacheContext *find_context(Lv00CacheManager *manager, uint32_t context_id)
{
    if (!manager) return NULL;

    for (int i = 0; i < manager->context_count; i++) {
        if (manager->contexts[i].context_id == context_id) {
            return &manager->contexts[i];
        }
    }
    return NULL;
}

/* ============================================================
 * LRU 淘汰
 * ============================================================ */

/**
 * @brief 淘汰一个最久未使用的缓存条目
 *
 * 从 LRU 链表尾部取出最久未使用的条目，从哈希表中移除，
 * 释放其数据，并更新统计信息。
 *
 * @param manager 缓存管理器
 * @return 成功淘汰返回 true，无条目可淘汰返回 false
 */
static bool evict_one(Lv00CacheManager *manager)
{
    if (!manager || !manager->lru_tail) return false;

    Lv00CacheEntry *victim = manager->lru_tail;

    /* 从哈希表中移除 */
    uint32_t idx = hash_key(victim->key) % (uint32_t)manager->bucket_count;
    Lv00CacheEntry **pp = &manager->buckets[idx];

    while (*pp) {
        if (*pp == victim) {
            *pp = victim->hash_next;
            break;
        }
        pp = &(*pp)->hash_next;
    }

    /* 从 LRU 链表中移除 */
    lru_remove(manager, victim);

    /* 更新上下文统计 */
    Lv00CacheContext *ctx = find_context(manager, victim->context_id);
    if (ctx) {
        if (ctx->total_size >= victim->data_size) {
            ctx->total_size -= victim->data_size;
        }
    }

    /* 更新管理器统计 */
    if (manager->current_size >= victim->data_size) {
        manager->current_size -= victim->data_size;
    }
    manager->entry_count--;
    manager->total_evictions++;

    /* 释放条目 */
    free_entry(victim);

    return true;
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * 根据配置创建缓存管理器实例。如果 config 为 NULL，则使用默认配置：
 * - 最大缓存大小：64KB
 * - 最大条目数：1024
 * - 淘汰策略：LRU
 * - 自动淘汰：启用
 *
 * @param config 配置参数，可为 NULL（使用默认值）
 * @return 新创建的管理器指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(
    const Lv00CacheConfig *config)
{
    Lv00CacheManager *manager = (Lv00CacheManager *)lv00_calloc(1, sizeof(Lv00CacheManager));
    if (!manager) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                        "cache_manager_create: 无法分配管理器内存");
        return NULL;
    }

    /* 初始化配置 */
    if (config) {
        manager->config = *config;
    } else {
        manager->config.max_cache_size = LV00_CACHE_DEFAULT_SIZE;
        manager->config.max_entries = LV00_CACHE_MAX_ENTRIES;
        manager->config.strategy = LV00_CACHE_STRATEGY_LRU;
        manager->config.enable_auto_evict = true;
    }

    /* 设置魔法数和初始状态 */
    manager->magic = LV00_CACHE_MAGIC;
    manager->is_running = true;

    /* 初始化哈希表 */
    manager->bucket_count = CACHE_DEFAULT_BUCKET_COUNT;
    manager->buckets = (Lv00CacheEntry **)lv00_calloc(
        (size_t)manager->bucket_count, sizeof(Lv00CacheEntry *));
    if (!manager->buckets) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                        "cache_manager_create: 无法分配哈希桶数组");
        { void *_tmp = (void *)manager; lv00_free(&_tmp); manager = NULL; }
        return NULL;
    }

    /* LRU 链表初始为空 */
    manager->lru_head = NULL;
    manager->lru_tail = NULL;

    /* 初始化上下文数组 */
    manager->context_capacity = CACHE_DEFAULT_CONTEXT_CAPACITY;
    manager->contexts = (Lv00CacheContext *)lv00_calloc(
        (size_t)manager->context_capacity, sizeof(Lv00CacheContext));
    if (!manager->contexts) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                        "cache_manager_create: 无法分配上下文数组");
        { void *_tmp = manager->buckets; lv00_free(&_tmp); manager->buckets = NULL; }
        { void *_tmp = (void *)manager; lv00_free(&_tmp); manager = NULL; }
        return NULL;
    }
    manager->context_count = 0;
    manager->next_context_id = 1;

    /* 自动创建默认上下文（ID=0，名称 "default"） */
    lv00_cache_context_create(manager, "default", 0);

    /* 设置当前活跃上下文为默认上下文 */
    manager->current_context_id = 0;

    return manager;
}

/**
 * @brief 销毁缓存管理器
 *
 * 释放所有缓存条目、上下文数组、哈希桶数组和管理器本身。
 * 调用后 manager 指针将不再有效。
 *
 * @param manager 缓存管理器
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager)
{
    if (!manager) return;

    /* 释放所有缓存条目 */
    lv00_unified_cache_clear(manager);

    /* 释放上下文数组 */
    { void *_tmp = manager->contexts; lv00_free(&_tmp); manager->contexts = NULL; }

    /* 释放哈希桶数组 */
    { void *_tmp = manager->buckets; lv00_free(&_tmp); manager->buckets = NULL; }

    /* 清除魔法数，防止 use-after-free */
    manager->magic = 0;
    manager->is_running = false;

    /* 释放管理器本身 */
    { void *_tmp = (void *)manager; lv00_free(&_tmp); manager = NULL; }
}

/**
 * @brief 检查缓存管理器是否有效
 *
 * 通过魔法数和运行状态判断管理器是否处于可用状态。
 *
 * @param manager 缓存管理器
 * @return 有效返回 true，无效返回 false
 */
LV00_PUBLIC_API bool lv00_cache_manager_is_valid(const Lv00CacheManager *manager)
{
    if (!manager) return false;
    return manager->magic == LV00_CACHE_MAGIC && manager->is_running;
}

/**
 * @brief 重置缓存管理器
 *
 * 清空所有缓存条目，重置统计信息，但保留配置和上下文。
 *
 * @param manager 缓存管理器
 * @return LV00_OK 成功，错误码失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_cache_manager_reset(Lv00CacheManager *manager)
{
    if (!manager) {
        lv00_set_error(LV00_ERROR_NULL_POINTER,
                        "cache_manager_reset: manager 为 NULL");
        return LV00_ERROR_NULL_POINTER;
    }

    if (!lv00_cache_manager_is_valid(manager)) {
        lv00_set_error(LV00_ERROR_INVALID_STATE,
                        "cache_manager_reset: 管理器无效");
        return LV00_ERROR_INVALID_STATE;
    }

    /* 清空所有缓存条目 */
    lv00_unified_cache_clear(manager);

    /* 重置统计信息 */
    manager->total_hits = 0;
    manager->total_misses = 0;
    manager->total_evictions = 0;
    manager->current_size = 0;
    manager->entry_count = 0;

    /* 重置上下文统计 */
    for (int i = 0; i < manager->context_count; i++) {
        manager->contexts[i].total_size = 0;
        manager->contexts[i].hit_count = 0;
        manager->contexts[i].miss_count = 0;
    }

    return LV00_OK;
}

/**
 * @brief 存储数据到缓存
 *
 * 将指定键值对存入缓存。如果键已存在，则更新数据。
 * 当缓存达到上限且启用自动淘汰时，自动淘汰最久未使用的条目。
 * 新条目自动关联到当前活跃上下文。
 *
 * @param manager 缓存管理器
 * @param key 缓存键（最长63字符）
 * @param data 数据指针（会被复制）
 * @param size 数据大小（字节）
 * @return 成功返回 true，失败返回 false
 */
LV00_PUBLIC_API bool lv00_cache_put(Lv00CacheManager *manager,
                                     const char *key,
                                     const void *data,
                                     size_t size)
{
    if (!manager || !key) {
        lv00_set_error(LV00_ERROR_NULL_POINTER,
                        "cache_put: manager 或 key 为 NULL");
        return false;
    }

    if (!lv00_cache_manager_is_valid(manager)) {
        lv00_set_error(LV00_ERROR_INVALID_STATE,
                        "cache_put: 管理器无效");
        return false;
    }

    /* 检查键长度 */
    size_t key_len = strlen(key);
    if (key_len == 0 || key_len >= sizeof(((Lv00CacheEntry *)0)->key)) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM,
                        "cache_put: 键为空或过长（最大%zu字符）",
                        sizeof(((Lv00CacheEntry *)0)->key) - 1);
        return false;
    }

    /* 检查是否已存在同名键 */
    Lv00CacheEntry *existing = find_entry(manager, key);
    if (existing) {
        /* 更新已有条目：释放旧数据，写入新数据 */
        size_t old_size = existing->data_size;

        /* 更新上下文统计 */
        Lv00CacheContext *ctx = find_context(manager, existing->context_id);
        if (ctx) {
            if (ctx->total_size >= old_size) {
                ctx->total_size -= old_size;
            }
        }
        if (manager->current_size >= old_size) {
            manager->current_size -= old_size;
        }

        free_entry_data(existing);

        /* 复制新数据 */
        if (data && size > 0) {
            existing->data = lv00_malloc(size);
            if (!existing->data) {
                lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                                "cache_put: 无法分配数据内存（%zu字节）", size);
                return false;
            }
            memcpy(existing->data, data, size);
            existing->data_size = size;
        } else {
            existing->data = NULL;
            existing->data_size = 0;
        }

        /* 更新访问信息 */
        existing->last_access_time = (int64_t)lv00_circuit_breaker_now_us();
        existing->access_count++;

        /* 更新上下文统计 */
        if (ctx) {
            ctx->total_size += existing->data_size;
        }
        manager->current_size += existing->data_size;

        /* 移动到 LRU 头部 */
        lru_move_to_head(manager, existing);

        return true;
    }

    /* 检查条目数限制 */
    if (manager->entry_count >= manager->config.max_entries) {
        if (!evict_one(manager)) {
            lv00_set_error(LV00_ERROR_RESOURCE_EXHAUSTED,
                            "cache_put: 缓存条目数已达上限且无法淘汰");
            return false;
        }
    }

    /* 检查缓存大小限制，必要时多次淘汰 */
    while (manager->config.enable_auto_evict &&
           manager->current_size + size > manager->config.max_cache_size &&
           manager->lru_tail) {
        if (!evict_one(manager)) {
            break;
        }
    }

    /* 再次检查大小限制 */
    if (size > manager->config.max_cache_size) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM,
                        "cache_put: 单条数据（%zu字节）超过最大缓存大小（%zu字节）",
                        size, manager->config.max_cache_size);
        return false;
    }

    /* 创建新条目 */
    Lv00CacheEntry *entry = (Lv00CacheEntry *)lv00_calloc(1, sizeof(Lv00CacheEntry));
    if (!entry) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                        "cache_put: 无法分配缓存条目");
        return false;
    }

    /* 复制键 */
    strncpy(entry->key, key, sizeof(entry->key) - 1);
    entry->key[sizeof(entry->key) - 1] = '\0';

    /* 复制数据 */
    if (data && size > 0) {
        entry->data = lv00_malloc(size);
        if (!entry->data) {
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                            "cache_put: 无法分配数据内存（%zu字节）", size);
            { void *_tmp = (void *)entry; lv00_free(&_tmp); entry = NULL; }
            return false;
        }
        memcpy(entry->data, data, size);
        entry->data_size = size;
    }

    /* 设置元数据 */
    entry->entry_id = (uint32_t)(manager->entry_count + 1);
    entry->context_id = manager->current_context_id;
    entry->create_time = (int64_t)lv00_circuit_breaker_now_us();
    entry->last_access_time = entry->create_time;
    entry->access_count = 1;
    entry->ttl_ms = 0;  /* 默认永久有效 */

    /* 插入哈希表 */
    uint32_t idx = hash_key(key) % (uint32_t)manager->bucket_count;
    entry->hash_next = manager->buckets[idx];
    manager->buckets[idx] = entry;

    /* 插入 LRU 链表头部 */
    lru_push_front(manager, entry);

    /* 更新统计 */
    manager->entry_count++;
    manager->current_size += entry->data_size;

    /* 更新上下文统计 */
    Lv00CacheContext *ctx = find_context(manager, entry->context_id);
    if (ctx) {
        ctx->total_size += entry->data_size;
    }

    return true;
}

/**
 * @brief 从缓存获取数据
 *
 * 根据键查找缓存条目，返回数据指针和大小。
 * 获取成功后自动将条目标记为最近使用（LRU 更新）。
 *
 * @param manager 缓存管理器
 * @param key 缓存键
 * @param out_data [输出] 数据指针（指向缓存内部数据，勿释放）
 * @param out_size [输出] 数据大小
 * @return 成功返回 true，未找到或失败返回 false
 */
LV00_PUBLIC_API bool lv00_cache_mgr_get(Lv00CacheManager *manager,
                                     const char *key,
                                     void **out_data,
                                     size_t *out_size)
{
    if (!manager || !key) {
        lv00_set_error(LV00_ERROR_NULL_POINTER,
                        "cache_mgr_get: manager 或 key 为 NULL");
        return false;
    }

    if (!lv00_cache_manager_is_valid(manager)) {
        lv00_set_error(LV00_ERROR_INVALID_STATE,
                        "cache_mgr_get: 管理器无效");
        return false;
    }

    Lv00CacheEntry *entry = find_entry(manager, key);
    if (!entry) {
        /* 缓存未命中 */
        manager->total_misses++;

        /* 更新上下文统计 */
        Lv00CacheContext *ctx = find_context(manager, manager->current_context_id);
        if (ctx) {
            ctx->miss_count++;
        }

        if (out_data) *out_data = NULL;
        if (out_size) *out_size = 0;
        return false;
    }

    /* 检查 TTL（0 表示永久有效） */
    if (entry->ttl_ms > 0) {
        int64_t now = (int64_t)lv00_circuit_breaker_now_us();
        int64_t elapsed_us = now - entry->create_time;
        int64_t elapsed_ms = elapsed_us / 1000;
        if (elapsed_ms > (int64_t)entry->ttl_ms) {
            /* TTL 过期，淘汰此条目 */
            evict_one(manager);
            manager->total_misses++;
            if (out_data) *out_data = NULL;
            if (out_size) *out_size = 0;
            return false;
        }
    }

    /* 缓存命中 */
    manager->total_hits++;
    entry->access_count++;
    entry->last_access_time = (int64_t)lv00_circuit_breaker_now_us();

    /* 更新上下文统计 */
    Lv00CacheContext *ctx = find_context(manager, entry->context_id);
    if (ctx) {
        ctx->hit_count++;
    }

    /* 移动到 LRU 头部 */
    lru_move_to_head(manager, entry);

    /* 输出结果 */
    if (out_data) *out_data = entry->data;
    if (out_size) *out_size = entry->data_size;

    return true;
}

/**
 * @brief 从缓存移除数据
 *
 * 根据键移除缓存条目，释放其数据并更新统计信息。
 *
 * @param manager 缓存管理器
 * @param key 缓存键
 * @return 成功移除返回 true，未找到返回 false
 */
LV00_PUBLIC_API bool lv00_cache_mgr_remove(Lv00CacheManager *manager,
                                        const char *key)
{
    if (!manager || !key) {
        lv00_set_error(LV00_ERROR_NULL_POINTER,
                        "cache_mgr_remove: manager 或 key 为 NULL");
        return false;
    }

    if (!lv00_cache_manager_is_valid(manager)) {
        lv00_set_error(LV00_ERROR_INVALID_STATE,
                        "cache_mgr_remove: 管理器无效");
        return false;
    }

    Lv00CacheEntry *entry = find_entry(manager, key);
    if (!entry) {
        return false;
    }

    /* 从哈希表中移除 */
    uint32_t idx = hash_key(key) % (uint32_t)manager->bucket_count;
    Lv00CacheEntry **pp = &manager->buckets[idx];

    while (*pp) {
        if (*pp == entry) {
            *pp = entry->hash_next;
            break;
        }
        pp = &(*pp)->hash_next;
    }

    /* 从 LRU 链表中移除 */
    lru_remove(manager, entry);

    /* 更新上下文统计 */
    Lv00CacheContext *ctx = find_context(manager, entry->context_id);
    if (ctx) {
        if (ctx->total_size >= entry->data_size) {
            ctx->total_size -= entry->data_size;
        }
    }

    /* 更新管理器统计 */
    if (manager->current_size >= entry->data_size) {
        manager->current_size -= entry->data_size;
    }
    manager->entry_count--;

    /* 释放条目 */
    free_entry(entry);

    return true;
}

/**
 * @brief 检查键是否存在
 *
 * @param manager 缓存管理器
 * @param key 缓存键
 * @return 存在返回 true，不存在返回 false
 */
LV00_PUBLIC_API bool lv00_cache_contains(Lv00CacheManager *manager,
                                           const char *key)
{
    if (!manager || !key) return false;
    if (!lv00_cache_manager_is_valid(manager)) return false;

    Lv00CacheEntry *entry = find_entry(manager, key);
    if (!entry) return false;

    /* 检查 TTL */
    if (entry->ttl_ms > 0) {
        int64_t now = (int64_t)lv00_circuit_breaker_now_us();
        int64_t elapsed_ms = (now - entry->create_time) / 1000;
        if (elapsed_ms > (int64_t)entry->ttl_ms) {
            return false;
        }
    }

    return true;
}

/* ============================================================
 * 上下文管理 API
 * ============================================================ */

/**
 * @brief 创建缓存上下文
 *
 * 创建一个新的缓存上下文，用于隔离不同运算场景的缓存数据。
 * 每个上下文拥有独立的统计信息。
 *
 * @param manager 缓存管理器
 * @param name 上下文名称（最长63字符）
 * @param parent_id 父上下文ID（0 表示无父上下文）
 * @return 新创建的上下文ID，失败返回 0
 */
LV00_PUBLIC_API uint32_t lv00_cache_context_create(Lv00CacheManager *manager,
                                                    const char *name,
                                                    uint32_t parent_id)
{
    if (!manager) {
        lv00_set_error(LV00_ERROR_NULL_POINTER,
                        "cache_context_create: manager 为 NULL");
        return 0;
    }

    if (!lv00_cache_manager_is_valid(manager)) {
        lv00_set_error(LV00_ERROR_INVALID_STATE,
                        "cache_context_create: 管理器无效");
        return 0;
    }

    /* 检查上下文容量，必要时扩容 */
    if (manager->context_count >= manager->context_capacity) {
        int new_capacity = manager->context_capacity * 2;
        if (new_capacity > LV00_CACHE_MAX_CONTEXT_DEPTH) {
            lv00_set_error(LV00_ERROR_RESOURCE_EXHAUSTED,
                            "cache_context_create: 上下文数量超过最大深度（%d）",
                            LV00_CACHE_MAX_CONTEXT_DEPTH);
            return 0;
        }

        Lv00CacheContext *new_contexts = (Lv00CacheContext *)lv00_realloc(
            manager->contexts, (size_t)new_capacity * sizeof(Lv00CacheContext));
        if (!new_contexts) {
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                            "cache_context_create: 无法扩容上下文数组");
            return 0;
        }

        /* 清零新增部分 */
        memset(&new_contexts[manager->context_count], 0,
               (size_t)(new_capacity - manager->context_capacity) * sizeof(Lv00CacheContext));

        manager->contexts = new_contexts;
        manager->context_capacity = new_capacity;
    }

    /* 创建新上下文 */
    Lv00CacheContext *ctx = &manager->contexts[manager->context_count];
    ctx->context_id = manager->next_context_id++;
    ctx->parent_id = parent_id;

    if (name) {
        strncpy(ctx->name, name, sizeof(ctx->name) - 1);
        ctx->name[sizeof(ctx->name) - 1] = '\0';
    } else {
        ctx->name[0] = '\0';
    }

    /* 计算嵌套深度 */
    if (parent_id == 0) {
        ctx->depth = 0;
    } else {
        Lv00CacheContext *parent = find_context(manager, parent_id);
        ctx->depth = parent ? (parent->depth + 1) : 0;
    }

    ctx->is_active = true;
    ctx->total_size = 0;
    ctx->hit_count = 0;
    ctx->miss_count = 0;

    manager->context_count++;

    return ctx->context_id;
}

/**
 * @brief 切换当前活跃上下文
 *
 * 将指定上下文设为当前活跃上下文。后续的缓存操作将关联到此上下文。
 *
 * @param manager 缓存管理器
 * @param context_id 目标上下文ID
 * @return 成功返回 true，上下文不存在返回 false
 */
LV00_PUBLIC_API bool lv00_cache_context_switch(Lv00CacheManager *manager,
                                                 uint32_t context_id)
{
    if (!manager) {
        lv00_set_error(LV00_ERROR_NULL_POINTER,
                        "cache_context_switch: manager 为 NULL");
        return false;
    }

    if (!lv00_cache_manager_is_valid(manager)) {
        lv00_set_error(LV00_ERROR_INVALID_STATE,
                        "cache_context_switch: 管理器无效");
        return false;
    }

    Lv00CacheContext *ctx = find_context(manager, context_id);
    if (!ctx) {
        lv00_set_error(LV00_ERROR_NOT_FOUND,
                        "cache_context_switch: 上下文 %u 不存在", context_id);
        return false;
    }

    manager->current_context_id = context_id;
    return true;
}

/**
 * @brief 获取当前活跃上下文ID
 *
 * @param manager 缓存管理器
 * @return 当前活跃上下文ID，manager 无效返回 0
 */
LV00_PUBLIC_API uint32_t lv00_cache_context_current(const Lv00CacheManager *manager)
{
    if (!manager) return 0;
    if (!lv00_cache_manager_is_valid(manager)) return 0;
    return manager->current_context_id;
}

/**
 * @brief 销毁缓存上下文
 *
 * 销毁指定上下文，同时移除该上下文下所有缓存条目。
 * 默认上下文（ID=0）不可销毁。
 *
 * @param manager 缓存管理器
 * @param context_id 要销毁的上下文ID
 * @return 成功返回 true，失败返回 false
 */
LV00_PUBLIC_API bool lv00_cache_context_destroy(Lv00CacheManager *manager,
                                                  uint32_t context_id)
{
    if (!manager) {
        lv00_set_error(LV00_ERROR_NULL_POINTER,
                        "cache_context_destroy: manager 为 NULL");
        return false;
    }

    if (!lv00_cache_manager_is_valid(manager)) {
        lv00_set_error(LV00_ERROR_INVALID_STATE,
                        "cache_context_destroy: 管理器无效");
        return false;
    }

    /* 不允许销毁默认上下文 */
    if (context_id == 0) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM,
                        "cache_context_destroy: 不允许销毁默认上下文（ID=0）");
        return false;
    }

    /* 移除属于该上下文的所有缓存条目 */
    for (int i = 0; i < manager->bucket_count; i++) {
        Lv00CacheEntry **pp = &manager->buckets[i];
        while (*pp) {
            Lv00CacheEntry *entry = *pp;
            if (entry->context_id == context_id) {
                *pp = entry->hash_next;
                lru_remove(manager, entry);

                if (manager->current_size >= entry->data_size) {
                    manager->current_size -= entry->data_size;
                }
                manager->entry_count--;

                free_entry(entry);
            } else {
                pp = &(*pp)->hash_next;
            }
        }
    }

    /* 从上下文数组中移除 */
    for (int i = 0; i < manager->context_count; i++) {
        if (manager->contexts[i].context_id == context_id) {
            /* 将最后一个上下文移到当前位置 */
            if (i < manager->context_count - 1) {
                manager->contexts[i] = manager->contexts[manager->context_count - 1];
            }
            memset(&manager->contexts[manager->context_count - 1], 0,
                   sizeof(Lv00CacheContext));
            manager->context_count--;
            break;
        }
    }

    /* 如果当前活跃上下文被销毁，切换回默认上下文 */
    if (manager->current_context_id == context_id) {
        manager->current_context_id = 0;
    }

    return true;
}

/* ============================================================
 * 统计 API
 * ============================================================ */

/**
 * @brief 获取全局统计信息
 *
 * @param manager 缓存管理器
 * @param out_hits [输出] 总命中次数（可为 NULL）
 * @param out_misses [输出] 总未命中次数（可为 NULL）
 * @param out_size [输出] 当前缓存大小（可为 NULL）
 */
LV00_PUBLIC_API void lv00_unified_cache_get_stats(const Lv00CacheManager *manager,
                                            uint64_t *out_hits,
                                            uint64_t *out_misses,
                                            size_t *out_size)
{
    if (!manager) return;

    if (out_hits)   *out_hits   = manager->total_hits;
    if (out_misses) *out_misses = manager->total_misses;
    if (out_size)   *out_size   = manager->current_size;
}

/**
 * @brief 获取上下文统计信息
 *
 * @param manager 缓存管理器
 * @param context_id 上下文ID
 * @param out_hits [输出] 命中次数（可为 NULL）
 * @param out_misses [输出] 未命中次数（可为 NULL）
 * @param out_size [输出] 缓存大小（可为 NULL）
 */
LV00_PUBLIC_API void lv00_cache_get_context_stats(const Lv00CacheManager *manager,
                                                   uint32_t context_id,
                                                   uint64_t *out_hits,
                                                   uint64_t *out_misses,
                                                   size_t *out_size)
{
    if (!manager) return;

    Lv00CacheContext *ctx = find_context((Lv00CacheManager *)manager, context_id);
    if (!ctx) return;

    if (out_hits)   *out_hits   = ctx->hit_count;
    if (out_misses) *out_misses = ctx->miss_count;
    if (out_size)   *out_size   = ctx->total_size;
}

/**
 * @brief 获取当前条目数
 *
 * @param manager 缓存管理器
 * @return 当前缓存条目数
 */
LV00_PUBLIC_API int lv00_cache_entry_count(const Lv00CacheManager *manager)
{
    if (!manager) return 0;
    return manager->entry_count;
}

/**
 * @brief 获取当前缓存大小
 *
 * @param manager 缓存管理器
 * @return 当前缓存使用大小（字节）
 */
LV00_PUBLIC_API size_t lv00_cache_current_size(const Lv00CacheManager *manager)
{
    if (!manager) return 0;
    return manager->current_size;
}

/**
 * @brief 获取配置
 *
 * @param manager 缓存管理器
 * @return 配置指针（指向管理器内部数据，勿释放），无效时返回 NULL
 */
LV00_PUBLIC_API const Lv00CacheConfig *lv00_cache_get_config(
    const Lv00CacheManager *manager)
{
    if (!manager) return NULL;
    if (!lv00_cache_manager_is_valid(manager)) return NULL;
    return &manager->config;
}

/**
 * @brief 设置自定义析构函数
 *
 * 设置全局析构函数，用于在缓存条目被淘汰或移除时释放关联资源。
 * 析构函数接收数据指针和数据大小两个参数。
 *
 * @param manager 缓存管理器
 * @param destructor 析构函数指针（可为 NULL，表示使用默认的 lv00_free）
 */
LV00_PUBLIC_API void lv00_cache_set_destructor(Lv00CacheManager *manager,
                                               void (*destructor)(void *, size_t))
{
    if (!manager) return;

    /* 遍历所有现有条目，设置析构函数 */
    for (int i = 0; i < manager->bucket_count; i++) {
        Lv00CacheEntry *entry = manager->buckets[i];
        while (entry) {
            entry->destructor = destructor;
            entry = entry->hash_next;
        }
    }
}

/**
 * @brief 清空缓存
 *
 * 移除所有缓存条目，重置条目计数和缓存大小。
 * 保留上下文和配置不变。
 *
 * @param manager 缓存管理器
 */
LV00_PUBLIC_API void lv00_unified_cache_clear(Lv00CacheManager *manager)
{
    if (!manager) return;

    /* 遍历所有桶，释放所有条目 */
    for (int i = 0; i < manager->bucket_count; i++) {
        Lv00CacheEntry *entry = manager->buckets[i];
        while (entry) {
            Lv00CacheEntry *next = entry->hash_next;
            free_entry(entry);
            entry = next;
        }
        manager->buckets[i] = NULL;
    }

    /* 重置 LRU 链表 */
    manager->lru_head = NULL;
    manager->lru_tail = NULL;

    /* 重置计数 */
    manager->entry_count = 0;
    manager->current_size = 0;
}
