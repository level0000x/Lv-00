/**
 * @file symbolic_cache.c
 * @brief 蓝图符号坐标缓存实现（TEN_LAYER_OPTIMIZED_PLAN §15.2 落地）
 *
 * 简单线性探测缓存（容量较小，运算键哈希 + 逐输入坐标哈希组合）。
 * 淘汰策略：访问计数最低者优先淘汰（近似 LRU）。
 */

#include "lv/symbolic_cache.h"

#include <stdint.h>
#include <string.h>

#include "lv/lv_thread.h"
#include "lv/lv_utils.h"

/* ============================================================
 * 缓存结构
 * ============================================================ */

typedef struct CacheEntry {
    bool used;                /**< 槽位占用 */
    bool valid;               /**< 有效（invalidate 可只标失效） */
    unsigned int key_hash;    /**< 键哈希 */
    char *operation;          /**< 运算名副本 */
    SymbolicCoord **inputs;   /**< 输入坐标快照（不拷贝坐标，仅指针数组副本——坐标生命周期由调用方保证） */
    int input_count;          /**< 输入数 */
    int node_id;              /**< 关联节点（-1 不关联） */
    SymbolicCoord *result;    /**< 缓存结果（缓存所有） */
    unsigned int access_count; /**< 访问计数（淘汰用） */
} CacheEntry;

struct lvSymbolicCache {
    CacheEntry *entries;      /**< 槽位数组 */
    int capacity;             /**< 槽位总数 */
    int count;                /**< 占用数 */
    unsigned long hits;       /**< 命中 */
    unsigned long misses;     /**< 未命中 */
};

#define DEFAULT_SYMBOLIC_CACHE_CAPACITY 64

/** @brief FNV-1a 组合哈希（运算名 + 输入坐标指针序列） */
static unsigned int cache_key_hash(const char *operation, const SymbolicCoord *const *inputs, int input_count) {
    unsigned int h = 2166136261u;
    const char *p = operation ? operation : "";
    while (*p) {
        h ^= (unsigned char) *p++;
        h *= 16777619u;
    }
    h ^= (unsigned int) input_count;
    h *= 16777619u;
    for (int i = 0; i < input_count; i++) {
        uintptr_t addr = (uintptr_t) (const void *) (inputs ? inputs[i] : NULL);
        h ^= (unsigned int) (addr >> 4);
        h *= 16777619u;
        h ^= (unsigned int) addr;
        h *= 16777619u;
    }
    return h;
}

/** @brief 键匹配（哈希 + 运算名 + 输入序列同指针） */
static bool entry_key_match(const CacheEntry *e, const char *operation, const SymbolicCoord *const *inputs,
                            int input_count, unsigned int hash) {
    if (e->key_hash != hash || e->input_count != input_count)
        return false;
    const char *op = operation ? operation : "";
    if (strcmp(e->operation ? e->operation : "", op) != 0)
        return false;
    for (int i = 0; i < input_count; i++) {
        const SymbolicCoord *a = inputs ? inputs[i] : NULL;
        if (e->inputs[i] != a)
            return false;
    }
    return true;
}

/** @brief 查找槽位（命中返回索引，未命中返回 -1） */
static int entry_find(lvSymbolicCache *cache, const char *operation, const SymbolicCoord *const *inputs,
                      int input_count, unsigned int hash) {
    for (int i = 0; i < cache->capacity; i++) {
        CacheEntry *e = &cache->entries[i];
        if (e->used && e->valid && entry_key_match(e, operation, inputs, input_count, hash))
            return i;
    }
    return -1;
}

/** @brief 释放条目内容 */
static void entry_clear(CacheEntry *e) {
    lv_free((void **) &e->operation);
    lv_free((void **) &e->inputs);
    if (e->result != NULL)
        symbolic_coord_destroy(e->result);
    memset(e, 0, sizeof(*e));
}

/** @brief 选淘汰槽位：访问计数最小（近似 LRU）；无槽返回 -1 */
static int entry_evict_index(lvSymbolicCache *cache) {
    int best = -1;
    unsigned int min_access = 0xFFFFFFFFu;
    for (int i = 0; i < cache->capacity; i++) {
        CacheEntry *e = &cache->entries[i];
        if (e->used && e->access_count < min_access) {
            min_access = e->access_count;
            best = i;
        }
    }
    return best;
}

/** @brief 找空槽（优先 used==false，其次 valid==false 的失效槽） */
static int entry_free_index(lvSymbolicCache *cache) {
    for (int i = 0; i < cache->capacity; i++) {
        if (!cache->entries[i].used || !cache->entries[i].valid)
            return i;
    }
    return -1;
}

lvSymbolicCache *lv_cache_create(int capacity) {
    if (capacity <= 0)
        capacity = DEFAULT_SYMBOLIC_CACHE_CAPACITY;
    lvSymbolicCache *cache = (lvSymbolicCache *) lv_calloc(1, sizeof(lvSymbolicCache));
    if (cache == NULL)
        return NULL;
    cache->entries = (CacheEntry *) lv_calloc((size_t) capacity, sizeof(CacheEntry));
    if (cache->entries == NULL) {
        lv_free((void **) &cache);
        return NULL;
    }
    cache->capacity = capacity;
    cache->count = 0;
    return cache;
}

void lv_cache_destroy(lvSymbolicCache *cache) {
    if (cache == NULL)
        return;
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->entries[i].used)
            entry_clear(&cache->entries[i]);
    }
    lv_free((void **) &cache->entries);
    lv_free((void **) &cache);
}

SymbolicCoord *lv_cache_lookup(lvSymbolicCache *cache, const char *operation, const SymbolicCoord *const *inputs,
                               int input_count) {
    if (cache == NULL)
        return NULL;
    unsigned int hash = cache_key_hash(operation, inputs, input_count);
    int idx = entry_find(cache, operation, inputs, input_count, hash);
    if (idx >= 0) {
        CacheEntry *e = &cache->entries[idx];
        e->access_count++;
        cache->hits++;
        return e->result;
    }
    cache->misses++;
    return NULL;
}

/** @brief 内部插入（operation 与 inputs 需 strdup/复制前先拷贝 result 所有权） */
static void cache_insert_impl(lvSymbolicCache *cache, const char *operation, const SymbolicCoord *const *inputs,
                              int input_count, int node_id, SymbolicCoord *result) {
    if (cache == NULL || result == NULL)
        return;
    unsigned int hash = cache_key_hash(operation, inputs, input_count);
    int idx = entry_find(cache, operation, inputs, input_count, hash);
    if (idx >= 0) {
        /* 已存在：替换结果（旧结果释放） */
        CacheEntry *e = &cache->entries[idx];
        if (e->result != result)
            symbolic_coord_destroy(e->result);
        e->result = result;
        e->node_id = node_id;
        e->access_count = 0;
        return;
    }
    /* 新键：找空/失效槽，否则淘汰 */
    idx = entry_free_index(cache);
    if (idx < 0) {
        idx = entry_evict_index(cache);
        if (idx < 0)
            return; /* 无法腾位（不应发生） */
    }
    CacheEntry *e = &cache->entries[idx];
    if (e->used)
        entry_clear(e); /* 释放被淘汰项 */
    memset(e, 0, sizeof(*e));
    e->operation = lv_strdup(operation ? operation : "");
    e->inputs = (SymbolicCoord **) lv_calloc((size_t)(input_count > 0 ? input_count : 1), sizeof(SymbolicCoord *));
    if (e->operation == NULL || (e->inputs == NULL && input_count > 0)) {
        /* 内存不足：回滚，不缓存（result 归调用方？契约 [take]——此处未能接管，调用方不应再用；
         * 按 [take] 语义销毁之避免泄漏） */
        entry_clear(e);
        symbolic_coord_destroy(result);
        return;
    }
    for (int i = 0; i < input_count; i++)
        e->inputs[i] = inputs ? inputs[i] : NULL;
    e->used = true;
    e->valid = true;
    e->key_hash = hash;
    e->input_count = input_count;
    e->node_id = node_id;
    e->result = result;
    e->access_count = 0;
    cache->count++;
}

void lv_cache_insert(lvSymbolicCache *cache, const char *operation, const SymbolicCoord *const *inputs,
                     int input_count, SymbolicCoord *result) {
    cache_insert_impl(cache, operation, inputs, input_count, -1, result);
}

void lv_cache_insert_for_node(lvSymbolicCache *cache, const char *operation, const SymbolicCoord *const *inputs,
                              int input_count, int node_id, SymbolicCoord *result) {
    cache_insert_impl(cache, operation, inputs, input_count, node_id, result);
}

void lv_cache_invalidate(lvSymbolicCache *cache) {
    if (cache == NULL)
        return;
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->entries[i].used && cache->entries[i].valid) {
            entry_clear(&cache->entries[i]);
            cache->count--;
        }
    }
}

void lv_cache_invalidate_by_node(lvSymbolicCache *cache, int node_id) {
    if (cache == NULL)
        return;
    for (int i = 0; i < cache->capacity; i++) {
        CacheEntry *e = &cache->entries[i];
        if (e->used && e->valid && e->node_id == node_id) {
            entry_clear(e);
            cache->count--;
        }
    }
}

double lv_cache_hit_rate(const lvSymbolicCache *cache) {
    if (cache == NULL)
        return 0.0;
    unsigned long total = cache->hits + cache->misses;
    if (total == 0)
        return 0.0;
    return (double) cache->hits / (double) total;
}
