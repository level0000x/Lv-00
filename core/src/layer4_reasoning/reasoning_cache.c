/**
 * @file reasoning_cache.c
 * @brief 推理结果缓存实现 —— 开放寻址哈希表 + 线性探测
 *
 * @details 使用开放寻址哈希表存储推理中间结果，避免重复计算。
 *          哈希表槽位使用 sentinel 值区分三种状态：
 *          - EMPTY: 从未使用（key = 0, occupied = false）
 *          - OCCUPIED: 正在使用（key != 0, occupied = true）
 *          - DELETED: 已删除（key = 0, occupied = true，探测时跳过）
 *
 *          当缓存满时，使用线性扫描找到最早插入的条目进行替换。
 *
 * @author Lv-00 Project
 * @version 3.5.0
 */

#include "reasoning_cache.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ================================================================
 * 常量定义
 * ================================================================ */

/** @brief 默认缓存容量 */
#define REASONING_CACHE_DEFAULT_CAPACITY 4096

/** @brief 最大负载因子（0.75），超过此值时缓存视为已满 */
#define REASONING_CACHE_MAX_LOAD_NUMERATOR 3
#define REASONING_CACHE_MAX_LOAD_DENOMINATOR 4

/** @brief 空槽位标记（key = 0 且 occupied = false） */
#define SLOT_EMPTY_KEY 0

/* ================================================================
 * 内部数据结构
 * ================================================================ */

/**
 * @brief 哈希表槽位
 */
typedef struct {
    uint64_t key;       /**< 哈希键（0 表示空槽位或已删除） */
    int value;          /**< 推理结果值 */
    bool occupied;      /**< true = 已占用或已删除，false = 空槽位 */
    size_t insert_order; /**< 插入顺序号，用于替换策略 */
} CacheSlot;

/**
 * @brief 推理结果缓存内部结构
 */
struct Lv00ReasoningCache {
    CacheSlot *slots;       /**< 哈希槽位数组 */
    size_t capacity;        /**< 槽位总数（2 的幂） */
    size_t size;            /**< 当前已占用槽位数 */
    size_t next_order;      /**< 下一个插入顺序号 */
    size_t hits;            /**< 缓存命中次数 */
    size_t misses;          /**< 缓存未命中次数 */
};

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/**
 * @brief 计算大于等于 n 的最小 2 的幂
 *
 * @param n 输入值
 * @return 大于等于 n 的最小 2 的幂
 */
static size_t next_power_of_two(size_t n) {
    if (n == 0) {
        return REASONING_CACHE_DEFAULT_CAPACITY;
    }
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
#if SIZE_MAX > 0xFFFFFFFFU
    n |= n >> 32;
#endif
    return n + 1;
}

/**
 * @brief 计算 key 在哈希表中的初始位置
 *
 * 使用乘法哈希（Knuth 变体），利用黄金比例分散键值。
 *
 * @param key      哈希键
 * @param capacity 槽位总数
 * @return 初始槽位索引
 */
static size_t hash_index(uint64_t key, size_t capacity) {
    /* Knuth 乘法哈希：key * 2^64 / golden_ratio */
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33;
    return (size_t)(key & (uint64_t)(capacity - 1));
}

/**
 * @brief 在哈希表中查找指定键的槽位索引
 *
 * @param cache 缓存
 * @param key   哈希键
 * @param out_index 输出：找到的槽位索引
 * @return true 找到，false 未找到
 */
static bool cache_find_slot(const Lv00ReasoningCache *cache, uint64_t key, size_t *out_index) {
    size_t idx = hash_index(key, cache->capacity);
    size_t first_deleted = cache->capacity; /* 记录第一个已删除槽位，用于插入优化 */

    for (size_t probe = 0; probe < cache->capacity; probe++) {
        size_t i = (idx + probe) & (cache->capacity - 1);
        CacheSlot *slot = &cache->slots[i];

        if (!slot->occupied) {
            /* 空槽位，键不存在 */
            return false;
        }

        if (slot->key == key) {
            /* 找到匹配的键 */
            if (out_index) {
                *out_index = i;
            }
            return true;
        }
    }

    return false;
}

/**
 * @brief 找到最早插入的已占用槽位索引（用于替换策略）
 *
 * @param cache 缓存
 * @return 最早插入的槽位索引
 */
static size_t cache_find_oldest_slot(const Lv00ReasoningCache *cache) {
    size_t oldest_idx = 0;
    size_t oldest_order = cache->slots[0].insert_order;

    for (size_t i = 1; i < cache->capacity; i++) {
        if (cache->slots[i].occupied && cache->slots[i].key != SLOT_EMPTY_KEY) {
            if (cache->slots[i].insert_order < oldest_order) {
                oldest_order = cache->slots[i].insert_order;
                oldest_idx = i;
            }
        }
    }

    return oldest_idx;
}

/**
 * @brief 检查缓存是否已达到最大负载
 */
static bool cache_is_full(const Lv00ReasoningCache *cache) {
    return (cache->size * REASONING_CACHE_MAX_LOAD_DENOMINATOR) >=
           (cache->capacity * REASONING_CACHE_MAX_LOAD_NUMERATOR);
}

/* ================================================================
 * 公开 API 实现
 * ================================================================ */

Lv00ReasoningCache *lv00_reasoning_cache_create(size_t capacity) {
    Lv00ReasoningCache *cache = (Lv00ReasoningCache *)calloc(1, sizeof(Lv00ReasoningCache));
    if (!cache) {
        return NULL;
    }

    cache->capacity = next_power_of_two(capacity);
    cache->slots = (CacheSlot *)calloc(cache->capacity, sizeof(CacheSlot));
    if (!cache->slots) {
        free(cache);
        return NULL;
    }

    cache->size = 0;
    cache->next_order = 1;
    cache->hits = 0;
    cache->misses = 0;

    return cache;
}

static void lv00_reasoning_cache_destroy(Lv00ReasoningCache *cache) {
    if (!cache) {
        return;
    }
    free(cache->slots);
    cache->slots = NULL;
    free(cache);
}

static bool lv00_reasoning_cache_has(Lv00ReasoningCache *cache, uint64_t key) {
    if (!cache) {
        return false;
    }

    size_t idx;
    bool found = cache_find_slot(cache, key, &idx);
    if (found) {
        cache->hits++;
    } else {
        cache->misses++;
    }
    return found;
}

static void lv00_reasoning_cache_put(Lv00ReasoningCache *cache, uint64_t key, int result) {
    if (!cache) {
        return;
    }

    /* key = 0 是特殊值，避免与空槽位混淆 */
    if (key == SLOT_EMPTY_KEY) {
        key = 1; /* 将 0 映射到 1 */
    }

    /* 检查是否已存在（更新） */
    size_t existing_idx;
    if (cache_find_slot(cache, key, &existing_idx)) {
        cache->slots[existing_idx].value = result;
        return;
    }

    /* 如果缓存已满，替换最早插入的条目 */
    if (cache_is_full(cache)) {
        size_t oldest = cache_find_oldest_slot(cache);
        cache->slots[oldest].key = key;
        cache->slots[oldest].value = result;
        cache->slots[oldest].insert_order = cache->next_order++;
        /* size 不变（替换而非新增） */
        return;
    }

    /* 找到空槽位插入 */
    size_t idx = hash_index(key, cache->capacity);
    for (size_t probe = 0; probe < cache->capacity; probe++) {
        size_t i = (idx + probe) & (cache->capacity - 1);
        if (!cache->slots[i].occupied) {
            cache->slots[i].key = key;
            cache->slots[i].value = result;
            cache->slots[i].occupied = true;
            cache->slots[i].insert_order = cache->next_order++;
            cache->size++;
            return;
        }
    }
}

static int lv00_reasoning_cache_get(Lv00ReasoningCache *cache, uint64_t key) {
    if (!cache) {
        return 0;
    }

    /* key = 0 被映射到 1 */
    if (key == SLOT_EMPTY_KEY) {
        key = 1;
    }

    size_t idx;
    if (cache_find_slot(cache, key, &idx)) {
        cache->hits++;
        return cache->slots[idx].value;
    }

    cache->misses++;
    return 0;
}

static void lv00_reasoning_cache_clear(Lv00ReasoningCache *cache) {
    if (!cache) {
        return;
    }

    memset(cache->slots, 0, cache->capacity * sizeof(CacheSlot));
    cache->size = 0;
    cache->next_order = 1;
    cache->hits = 0;
    cache->misses = 0;
}

static void lv00_reasoning_cache_get_stats(const Lv00ReasoningCache *cache,
                                     size_t *hits, size_t *misses, size_t *size) {
    if (!cache) {
        if (hits) *hits = 0;
        if (misses) *misses = 0;
        if (size) *size = 0;
        return;
    }

    if (hits) *hits = cache->hits;
    if (misses) *misses = cache->misses;
    if (size) *size = cache->size;
}
