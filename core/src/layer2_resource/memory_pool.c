/**
 * @file memory_pool.c
 * @brief 内存池系统实现
 *
 * @details 实现三种内存管理策略：
 *   1. 固定大小对象池
 *   2. 线性分配器
 *   3. 对象缓存（LRU）
 *
 * 设计说明：本模块作为底层内存基础设施，池结构体本身使用 lv_malloc/lv_free
 * 分配，而内部数据块（性能关键路径）保留原生 malloc/free/realloc/calloc，
 * 原因：
 * 1. 避免循环依赖：lv_malloc 内部可能依赖内存池，而内存池不能依赖 lv_malloc
 *    进行内部块分配
 * 2. 对象池/线性分配器有独立的内存管理策略，内部块不走 lv 的统计系统
 * 3. 全局内存统计（lv_mem_*）是可选的附加功能，不影响核心分配路径
 * 4. 池结构体（lvObjectPool、lvLinearAllocator、lvObjectCache 等）
 *    的生命周期管理使用 lv_malloc/lv_free，便于统一追踪和调试
 *
 * 【为何内部数据块使用标准 malloc/free 而非 lv_malloc/lv_free】
 *
 * memory_pool 是整个项目的底层内存基础设施，位于依赖链的最底层。
 * 如果内部数据块也使用 lv_malloc/lv_free，会形成循环依赖：
 *
 *   lv_malloc() → 内存统计(lv_mem_record_alloc) → 可能触发内存池分配
 *   → lv_pool_alloc() → 内部 malloc → lv_malloc() → ...（无限递归）
 *
 * 因此，内部数据块（对象池中的内存块、线性分配器的 MemoryBlock、
 * 缓存条目的 CacheEntry 等）必须使用标准库的 malloc/free/realloc/calloc，
 * 绕过 lv 的统计和追踪系统。
 *
 * 而池结构体本身（lvObjectPool、lvLinearAllocator 等）的分配/释放
 * 使用 lv_malloc/lv_free，因为这些操作发生在池创建/销毁时，
 * 不在性能关键路径上，且便于统一追踪和调试。
 *
 * 使用约定：通过 lv_pool_alloc 分配的对象必须通过 lv_pool_free 释放，
 * 严禁混用 lv_free 或标准 free。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "memory_pool.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h" /* lv_FNV64_*, lv_CONFIG_POOL_* macros */

#include "lv_internal.h" /* lv_FNV64_* 哈希常量 */
#include "lv_utils.h"    /* lv_strdup, lv_malloc, lv_free */

/* ============== 内部常量 ============== */

/** 默认对齐 */
#define lv_DEFAULT_ALIGNMENT 16

/** 对象池增长因子 */
#define lv_POOL_GROWTH_FACTOR 2

/** 哈希表初始容量 */
#define lv_HASH_TABLE_INIT_CAP 64

/* [Bug修复] 溢出检查宏：检测 size_t 乘法是否溢出 */
#define lv_SIZE_MUL_OVERFLOW(a, b) (((a) != 0 && (b) > SIZE_MAX / (a)))

/* ============== 平台抽象层（线程安全） ============== */
#include "lv/lv_thread.h"

/* ============== 对象池实现 ============== */

/**
 * @brief 空闲链表节点
 */
typedef struct FreeNode {
    struct FreeNode *next;
} FreeNode;

/**
 * @brief 对象池结构
 */
struct lvObjectPool {
    /* 配置 */
    size_t object_size; /**< 单个对象大小 */
    size_t capacity;    /**< 当前容量 */
    bool thread_safe;   /**< 是否线程安全 */
    bool auto_grow;     /**< 是否自动扩展 */
    char name[32];      /**< 池名称 */

    /* 内存块 */
    void **blocks;            /**< 内存块数组 */
    size_t block_count;       /**< 内存块数量 */
    size_t block_capacity;    /**< 内存块数组容量 */
    size_t *block_capacities; /**< [Bug修复] 每块实际分配的对象数量，用于 pool_clear 正确重建空闲链表 */

    /* 空闲链表 */
    FreeNode *free_list; /**< 空闲对象链表 */

    /* 统计 */
    uint64_t total_allocs; /**< 总分配次数 */
    uint64_t total_frees;  /**< 总释放次数 */
    size_t current_used;   /**< 当前使用数量 */

    /* 线程安全 */
    lv_mutex_t mutex; /**< 互斥锁 */
};

/* 对齐辅助函数（共享定义见 lv_internal.h 的 align_up） */

lvObjectPool *lv_pool_create(const lvPoolConfig *config) {
    if (!config || config->object_size == 0) {
        return NULL;
    }

    lvObjectPool *pool = (lvObjectPool *) malloc(sizeof(lvObjectPool));
    if (!pool) {
        return NULL;
    }
    memset(pool, 0, sizeof(lvObjectPool));

    /* 对象大小至少能存放一个 FreeNode 指针 */
    pool->object_size = align_up(config->object_size, sizeof(void *));
    pool->capacity = config->capacity > 0 ? config->capacity : lv_POOL_DEFAULT_CAPACITY;
    pool->thread_safe = config->thread_safe;
    pool->auto_grow = config->auto_grow;

    if (config->name) {
        strncpy(pool->name, config->name, sizeof(pool->name) - 1);
        pool->name[sizeof(pool->name) - 1] = '\0';
    } else {
        pool->name[0] = '\0';
    }

    /* 初始化线程锁 */
    if (pool->thread_safe) {
        lv_mutex_init(&pool->mutex);
    }

    /* 分配初始内存块数组 */
    pool->block_capacity = 4;
    pool->blocks = (void **) malloc(pool->block_capacity * sizeof(void *));
    if (!pool->blocks) {
        lv_free((void **) &pool);
        return NULL;
    }

    /* [Bug修复] 分配块容量记录数组 */
    pool->block_capacities = (size_t *) malloc(pool->block_capacity * sizeof(size_t));
    if (!pool->block_capacities) {
        if (pool->thread_safe) {
            lv_mutex_destroy(&pool->mutex);
        }
        lv_free((void **) &pool->blocks);
        lv_free((void **) &pool);
        return NULL;
    }

    /* 分配第一个内存块（性能关键路径：保留原生 malloc，避免循环依赖开销） */
    void *block = malloc(pool->object_size * pool->capacity);
    if (!block) {
        if (pool->thread_safe) {
            lv_mutex_destroy(&pool->mutex);
        }
        lv_free((void **) &pool->block_capacities);
        lv_free((void **) &pool->blocks);
        lv_free((void **) &pool);
        return NULL;
    }
    pool->blocks[0] = block;
    pool->block_capacities[0] = pool->capacity; /* [Bug修复] 记录首块容量 */
    pool->block_count = 1;

    /* 初始化空闲链表 */
    pool->free_list = NULL;
    char *ptr = (char *) block;
    for (size_t i = 0; i < pool->capacity; i++) {
        FreeNode *node = (FreeNode *) (ptr + i * pool->object_size);
        node->next = pool->free_list;
        pool->free_list = node;
    }

    return pool;
}

void lv_pool_destroy(lvObjectPool *pool) {
    if (!pool) {
        return;
    }

    /* 释放所有内存块（原生 malloc 分配，用原生 free 释放，避免触发毒模式检测） */
    for (size_t i = 0; i < pool->block_count; i++) {
        free(pool->blocks[i]);
    }
    free(pool->blocks);
    free(pool->block_capacities);

    /* 销毁线程锁 */
    if (pool->thread_safe) {
        lv_mutex_destroy(&pool->mutex);
    }

    lv_free((void **) &pool);
}

void *lv_pool_alloc(lvObjectPool *pool) {
    if (!pool) {
        return NULL;
    }

    if (pool->thread_safe) {
        lv_mutex_lock(&pool->mutex);
    }

    /* 空闲链表为空，需要扩展 */
    if (!pool->free_list) {
        if (!pool->auto_grow) {
            if (pool->thread_safe) {
                lv_mutex_unlock(&pool->mutex);
            }
            return NULL;
        }

        /* 扩展内存块数组 */
        if (pool->block_count >= pool->block_capacity) {
            /* 溢出检查（双重检查由 lv_ensure_capacity 内部完成） */
            int cap = (int) pool->block_capacity;

            /* 第一次：扩容 blocks */
            if (!lv_ensure_capacity((void **) &pool->blocks, cap, &cap,
                                    sizeof(void *), 1)) {
                if (pool->thread_safe) {
                    lv_mutex_unlock(&pool->mutex);
                }
                return NULL;
            }
            int blocks_cap = cap; /* blocks 的新容量 */

            /* 第二次：扩容 block_capacities 与 blocks 同步。
             * 临时回退容量指针使扩容真实执行；失败时回滚 blocks：
             * 缩回旧大小，若失败则保留新块（仍有效） */
            cap = (int) pool->block_capacity;
            if (!lv_ensure_capacity((void **) &pool->block_capacities, cap, &cap,
                                    sizeof(size_t), blocks_cap - cap)) {
                void **shrunk = (void **) lv_realloc(pool->blocks,
                                                     pool->block_capacity * sizeof(void *));
                if (shrunk) {
                    pool->blocks = shrunk;
                }
                /* pool->blocks 始终有效（要么是 shrunk，要么是新块） */
                if (pool->thread_safe) {
                    lv_mutex_unlock(&pool->mutex);
                }
                return NULL;
            }
            pool->block_capacity = (size_t) blocks_cap;
        }

        /* 分配新内存块 */
        /* [Bug修复] 溢出检查：确保 capacity * GROWTH_FACTOR 不会溢出 */
        if (lv_SIZE_MUL_OVERFLOW(pool->capacity, lv_POOL_GROWTH_FACTOR)) {
            if (pool->thread_safe) {
                lv_mutex_unlock(&pool->mutex);
            }
            return NULL;
        }
        size_t new_capacity = pool->capacity * lv_POOL_GROWTH_FACTOR;

        /* [Bug修复] 溢出检查：确保 object_size * new_capacity 不会溢出 */
        if (lv_SIZE_MUL_OVERFLOW(pool->object_size, new_capacity)) {
            if (pool->thread_safe) {
                lv_mutex_unlock(&pool->mutex);
            }
            return NULL;
        }
        /* 分配新内存块（性能关键路径：保留原生 malloc，避免循环依赖开销） */
        void *block = malloc(pool->object_size * new_capacity);
        if (!block) {
            if (pool->thread_safe) {
                lv_mutex_unlock(&pool->mutex);
            }
            return NULL;
        }
        pool->blocks[pool->block_count] = block;
        pool->block_capacities[pool->block_count] = new_capacity; /* [Bug修复] 记录新块容量 */
        pool->block_count++;

        /* 添加到空闲链表 */
        char *ptr = (char *) block;
        for (size_t i = 0; i < new_capacity; i++) {
            FreeNode *node = (FreeNode *) (ptr + i * pool->object_size);
            node->next = pool->free_list;
            pool->free_list = node;
        }
        pool->capacity += new_capacity;
    }

    /* 从空闲链表取出 */
    FreeNode *node = pool->free_list;
    pool->free_list = node->next;
    pool->total_allocs++;
    pool->current_used++;

    if (pool->thread_safe) {
        lv_mutex_unlock(&pool->mutex);
    }

    /* 清零对象 */
    memset(node, 0, pool->object_size);
    return node;
}

bool lv_pool_free(lvObjectPool *pool, void *obj) {
    if (!pool || !obj) {
        return false;
    }

    if (pool->thread_safe) {
        lv_mutex_lock(&pool->mutex);
    }

    /* 添加到空闲链表 */
    FreeNode *node = (FreeNode *) obj;
    node->next = pool->free_list;
    pool->free_list = node;
    pool->total_frees++;
    if (pool->current_used > 0)
        pool->current_used--;

    if (pool->thread_safe) {
        lv_mutex_unlock(&pool->mutex);
    }

    return true;
}

void lv_pool_get_stats(lvObjectPool *pool, uint64_t *out_total_allocs, uint64_t *out_total_frees,
                       size_t *out_current_used) {
    if (!pool) {
        return;
    }

    if (pool->thread_safe) {
        lv_mutex_lock(&pool->mutex);
    }

    if (out_total_allocs)
        *out_total_allocs = pool->total_allocs;
    if (out_total_frees)
        *out_total_frees = pool->total_frees;
    if (out_current_used)
        *out_current_used = pool->current_used;

    if (pool->thread_safe) {
        lv_mutex_unlock(&pool->mutex);
    }
}

void lv_pool_clear(lvObjectPool *pool) {
    if (!pool) {
        return;
    }

    if (pool->thread_safe) {
        lv_mutex_lock(&pool->mutex);
    }

    /* [Bug修复] 使用 block_capacities 记录的实际容量重建空闲链表，
     * 替代原来基于 lv_POOL_DEFAULT_CAPACITY 的错误计算 */
    pool->free_list = NULL;
    for (size_t b = 0; b < pool->block_count; b++) {
        char *ptr = (char *) pool->blocks[b];
        size_t block_size = pool->block_capacities[b];
        for (size_t i = 0; i < block_size; i++) {
            FreeNode *node = (FreeNode *) (ptr + i * pool->object_size);
            node->next = pool->free_list;
            pool->free_list = node;
        }
    }
    pool->current_used = 0;

    if (pool->thread_safe) {
        lv_mutex_unlock(&pool->mutex);
    }
}

/* ============== 线性分配器实现 ============== */

/**
 * @brief 内存块结构
 */
typedef struct MemoryBlock {
    struct MemoryBlock *next;
    size_t size;
    size_t used;
    char data[]; /**< 柔性数组 */
} MemoryBlock;

/**
 * @brief 线性分配器结构
 */
struct lvLinearAllocator {
    MemoryBlock *blocks;   /**< 内存块链表 */
    size_t block_size;     /**< 默认块大小 */
    size_t total_blocks;   /**< 总块数 */
    size_t total_used;     /**< 总使用字节数 */
    size_t total_capacity; /**< 总容量 */
};

lvLinearAllocator *lv_linear_allocator_create(size_t block_size) {
    lvLinearAllocator *allocator = (lvLinearAllocator *) malloc(sizeof(lvLinearAllocator));
    if (!allocator) {
        return NULL;
    }
    memset(allocator, 0, sizeof(lvLinearAllocator));

    allocator->block_size = block_size > 0 ? block_size : lv_LINEAR_ALLOCATOR_BLOCK_SIZE;

    /* 预分配第一个块（性能关键路径：保留原生 malloc，避免循环依赖开销） */
    MemoryBlock *block = (MemoryBlock *) malloc(sizeof(MemoryBlock) + allocator->block_size);
    if (!block) {
        lv_free((void **) &allocator);
        return NULL;
    }
    block->next = NULL;
    block->size = allocator->block_size;
    block->used = 0;

    allocator->blocks = block;
    allocator->total_blocks = 1;
    allocator->total_capacity = allocator->block_size;

    return allocator;
}

void lv_linear_allocator_destroy(lvLinearAllocator *allocator) {
    if (!allocator) {
        return;
    }

    /* 释放所有内存块（原生 malloc 分配，用原生 free 释放，避免触发毒模式检测） */
    MemoryBlock *block = allocator->blocks;
    while (block) {
        MemoryBlock *next = block->next;
        free(block);
        block = next;
    }

    lv_free((void **) &allocator);
}

void *lv_linear_alloc(lvLinearAllocator *allocator, size_t size, size_t alignment) {
    if (!allocator || size == 0) {
        return NULL;
    }

    size_t align = alignment > 0 ? alignment : lv_DEFAULT_ALIGNMENT;
    align = align < sizeof(void *) ? sizeof(void *) : align;

    /* 查找有足够空间的块 */
    MemoryBlock *block = allocator->blocks;
    while (block) {
        /* 计算对齐后的起始位置 */
        uintptr_t start = (uintptr_t) (block->data + block->used);
        uintptr_t aligned = (start + align - 1) & ~(align - 1);
        size_t padding = aligned - start;
        /* overflow check */
        if (size > SIZE_MAX - padding) {
            return NULL;
        }
        size_t total_needed = padding + size;

        if (block->used + total_needed <= block->size) {
            /* 找到足够空间 */
            void *ptr = (void *) aligned;
            block->used += total_needed;
            allocator->total_used += total_needed;
            return ptr;
        }
        block = block->next;
    }

    /* 需要新块（性能关键路径：保留原生 malloc，避免循环依赖开销） */
    /* 溢出检查：确保 size + align 不会溢出 */
    if (size > SIZE_MAX - align) {
        return NULL;
    }
    size_t new_block_size = size + align;
    if (new_block_size < allocator->block_size) {
        new_block_size = allocator->block_size;
    }

    MemoryBlock *new_block = (MemoryBlock *) malloc(sizeof(MemoryBlock) + new_block_size);
    if (!new_block) {
        return NULL;
    }

    new_block->next = allocator->blocks;
    new_block->size = new_block_size;
    new_block->used = 0;
    allocator->blocks = new_block;
    allocator->total_blocks++;
    allocator->total_capacity += new_block_size;

    /* 在新块中分配 */
    uintptr_t start = (uintptr_t) new_block->data;
    uintptr_t aligned = (start + align - 1) & ~(align - 1);
    size_t padding = aligned - start;
    void *ptr = (void *) aligned;
    new_block->used = padding + size;
    allocator->total_used += padding + size;

    return ptr;
}

void lv_linear_allocator_reset(lvLinearAllocator *allocator) {
    if (!allocator) {
        return;
    }

    /* 重置所有块的使用量 */
    MemoryBlock *block = allocator->blocks;
    while (block) {
        block->used = 0;
        block = block->next;
    }
    allocator->total_used = 0;
}

void lv_linear_allocator_get_stats(const lvLinearAllocator *allocator, size_t *out_total_blocks, size_t *out_used_bytes,
                                   size_t *out_capacity_bytes) {
    if (!allocator) {
        return;
    }
    if (out_total_blocks)
        *out_total_blocks = allocator->total_blocks;
    if (out_used_bytes)
        *out_used_bytes = allocator->total_used;
    if (out_capacity_bytes)
        *out_capacity_bytes = allocator->total_capacity;
}

/* ============== 对象缓存（LRU）实现 ============== */

/**
 * @brief 缓存条目
 */
typedef struct CacheEntry {
    lvCacheKey key;
    void *value;
    struct CacheEntry *prev;
    struct CacheEntry *next;
    struct CacheEntry *hash_next; /**< 哈希表链 */
} CacheEntry;

/**
 * @brief 对象缓存结构
 */
struct lvObjectCache {
    /* 配置 */
    size_t capacity;
    lvCacheCreateFunc create_func;
    lvCacheDestroyFunc destroy_func;
    void *user_data;

    /* LRU 双向链表 */
    CacheEntry *head; /**< 最近使用 */
    CacheEntry *tail; /**< 最少使用 */

    /* 哈希表 */
    CacheEntry **hash_table;
    size_t hash_capacity;

    /* 统计 */
    uint64_t hits;
    uint64_t misses;
    size_t current_size;
};

/* FNV-1a 哈希函数（使用项目统一的 lv_FNV64_* 常量，定义在 lv_internal.h） */
static inline size_t hash_key(lvCacheKey key, size_t capacity) {
    uint64_t hash = lv_FNV64_OFFSET_BASIS;
    /* 对 8 字节 key 逐字节进行 FNV-1a 混合 */
    const unsigned char *bytes = (const unsigned char *) &key;
    for (size_t i = 0; i < sizeof(key); i++) {
        hash ^= bytes[i];
        hash *= lv_FNV64_PRIME;
    }
    return (size_t) (hash % capacity);
}

lvObjectCache *lv_cache_create(size_t capacity, lvCacheCreateFunc create_func, lvCacheDestroyFunc destroy_func,
                               void *user_data) {
    if (!create_func) {
        return NULL;
    }

    lvObjectCache *cache = (lvObjectCache *) malloc(sizeof(lvObjectCache));
    if (!cache) {
        return NULL;
    }
    memset(cache, 0, sizeof(lvObjectCache));

    cache->capacity = capacity > 0 ? capacity : lv_LRU_CACHE_DEFAULT_CAPACITY;
    cache->create_func = create_func;
    cache->destroy_func = destroy_func;
    cache->user_data = user_data;

    /* 分配哈希表 */
    cache->hash_capacity = lv_HASH_TABLE_INIT_CAP;
    cache->hash_table = (CacheEntry **) calloc(cache->hash_capacity, sizeof(CacheEntry *));
    if (!cache->hash_table) {
        free(cache);
        return NULL;
    }

    return cache;
}

/**
 * 释放单个缓存条目（destroy_func 回调 + 原生 free）。
 * 注意：CacheEntry 使用原生 malloc 分配，必须用 free 而非 lv_free。
 */
static void cache_entry_free(lvObjectCache *cache, CacheEntry *entry) {
    if (cache->destroy_func && entry->value) {
        cache->destroy_func(entry->value, cache->user_data);
    }
    free(entry);
}

void lv_cache_destroy(lvObjectCache *cache) {
    if (!cache) {
        return;
    }

    /* 释放所有条目（复用 clear 的遍历循环），再释放容器本身 */
    lv_cache_clear(cache);

    /* 注意：cache->hash_table 使用 calloc 分配，CacheEntry 使用 malloc 分配，
     * 此处用标准 free 释放而非 lv_free，因为这些内存在 cache 模块中
     * 使用原生 calloc/malloc 分配，未经过 lv 的追踪系统 */
    free(cache->hash_table);
    free(cache);
}

/* 将条目移到链表头部 */
static void move_to_head(lvObjectCache *cache, CacheEntry *entry) {
    if (entry == cache->head) {
        return;
    }

    /* 从当前位置移除 */
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    if (entry == cache->tail) {
        cache->tail = entry->prev;
    }

    /* 插入头部 */
    entry->prev = NULL;
    entry->next = cache->head;
    if (cache->head) {
        cache->head->prev = entry;
    }
    cache->head = entry;

    if (!cache->tail) {
        cache->tail = entry;
    }
}

/* 驱逐最少使用的条目 */
static void evict_lru(lvObjectCache *cache) {
    if (!cache->tail) {
        return;
    }

    CacheEntry *entry = cache->tail;

    /* 从 LRU 链表移除 */
    cache->tail = entry->prev;
    if (cache->tail) {
        cache->tail->next = NULL;
    } else {
        cache->head = NULL;
    }

    /* 从哈希表移除 */
    size_t idx = hash_key(entry->key, cache->hash_capacity);
    CacheEntry **pp = &cache->hash_table[idx];
    while (*pp) {
        if (*pp == entry) {
            *pp = entry->hash_next;
            break;
        }
        pp = &(*pp)->hash_next;
    }

    /* 释放 */
    cache_entry_free(cache, entry);
    cache->current_size--;
}

void *lv_cache_get(lvObjectCache *cache, lvCacheKey key) {
    if (!cache) {
        return NULL;
    }

    /* 查找哈希表 */
    size_t idx = hash_key(key, cache->hash_capacity);
    CacheEntry *entry = cache->hash_table[idx];
    while (entry) {
        if (entry->key == key) {
            /* 命中 */
            cache->hits++;
            move_to_head(cache, entry);
            return entry->value;
        }
        entry = entry->hash_next;
    }

    /* 未命中 */
    cache->misses++;

    /* 创建新条目 */
    void *value = cache->create_func(key, cache->user_data);
    if (!value) {
        return NULL;
    }

    /* 检查容量 */
    if (cache->current_size >= cache->capacity) {
        evict_lru(cache);
    }

    /* 创建条目（性能关键路径：保留原生 malloc） */
    entry = (CacheEntry *) malloc(sizeof(CacheEntry));
    if (!entry) {
        if (cache->destroy_func) {
            cache->destroy_func(value, cache->user_data);
        }
        return NULL;
    }
    entry->key = key;
    entry->value = value;
    entry->prev = NULL;
    entry->next = NULL;
    entry->hash_next = NULL;

    /* 添加到哈希表 */
    entry->hash_next = cache->hash_table[idx];
    cache->hash_table[idx] = entry;

    /* 添加到 LRU 链表头部 */
    entry->next = cache->head;
    if (cache->head) {
        cache->head->prev = entry;
    }
    cache->head = entry;
    if (!cache->tail) {
        cache->tail = entry;
    }

    cache->current_size++;

    return value;
}

bool lv_cache_remove(lvObjectCache *cache, lvCacheKey key) {
    if (!cache) {
        return false;
    }

    /* 查找 */
    size_t idx = hash_key(key, cache->hash_capacity);
    CacheEntry **pp = &cache->hash_table[idx];
    while (*pp) {
        if ((*pp)->key == key) {
            CacheEntry *entry = *pp;
            *pp = entry->hash_next;

            /* 从 LRU 链表移除 */
            if (entry->prev) {
                entry->prev->next = entry->next;
            } else {
                cache->head = entry->next;
            }
            if (entry->next) {
                entry->next->prev = entry->prev;
            } else {
                cache->tail = entry->prev;
            }

            /* 释放 */
            cache_entry_free(cache, entry);
            cache->current_size--;

            return true;
        }
        pp = &(*pp)->hash_next;
    }

    return false;
}

void lv_cache_clear(lvObjectCache *cache) {
    if (!cache) {
        return;
    }

    /* 释放所有条目 */
    CacheEntry *entry = cache->head;
    while (entry) {
        CacheEntry *next = entry->next;
        cache_entry_free(cache, entry);
        entry = next;
    }

    /* 清空哈希表 */
    memset(cache->hash_table, 0, cache->hash_capacity * sizeof(CacheEntry *));
    cache->head = NULL;
    cache->tail = NULL;
    cache->current_size = 0;
}

void lv_cache_get_stats(const lvObjectCache *cache, uint64_t *out_hits, uint64_t *out_misses,
                        size_t *out_current_size) {
    if (!cache) {
        return;
    }
    if (out_hits)
        *out_hits = cache->hits;
    if (out_misses)
        *out_misses = cache->misses;
    if (out_current_size)
        *out_current_size = cache->current_size;
}

/* ============== 全局内存统计 ============== */

/**
 * @brief 内存池模块全局状态
 *
 * 将所有模块级全局变量归并到单一上下文结构体中，
 * 降低模块耦合度，提高可维护性。
 */
typedef struct MemoryPoolState {
    /* 统计信息 */
    lvMemoryStats global_stats;
    lv_mutex_t stats_mutex;
    lv_once_t stats_once;

    /* 对象池 */
    lvObjectPool *node_pool;
    lvObjectPool *constraint_pool;
    lvObjectPool *symbolic_coord_pool;
    lvObjectPool *proof_step_pool;
} MemoryPoolState;

/** 模块级唯一状态实例 */
static MemoryPoolState s_mem_state = {0};

static void stats_mutex_init_func(void) {
    lv_mutex_init(&s_mem_state.stats_mutex);
}

int lv_mem_register_type(const char *name) {
    if (!name) {
        return -1;
    }

    lv_once(&s_mem_state.stats_once, stats_mutex_init_func);
    lv_mutex_lock(&s_mem_state.stats_mutex);

    if (s_mem_state.global_stats.type_count >= lv_MEM_STAT_MAX_TYPES) {
        lv_mutex_unlock(&s_mem_state.stats_mutex);
        return -1;
    }

    int id = s_mem_state.global_stats.type_count++;
    s_mem_state.global_stats.types[id].name = lv_strdup(name); /* 复制字符串，避免保存裸指针 */
    s_mem_state.global_stats.types[id].total_allocs = 0;
    s_mem_state.global_stats.types[id].total_frees = 0;
    s_mem_state.global_stats.types[id].current_bytes = 0;
    s_mem_state.global_stats.types[id].peak_bytes = 0;

    lv_mutex_unlock(&s_mem_state.stats_mutex);
    return id;
}

void lv_mem_record_alloc(int type_id, size_t size) {
    if (type_id < 0) {
        return;
    }

    lv_once(&s_mem_state.stats_once, stats_mutex_init_func);
    lv_mutex_lock(&s_mem_state.stats_mutex);

    if ((size_t) type_id < (size_t) s_mem_state.global_stats.type_count) {
        s_mem_state.global_stats.types[type_id].total_allocs++;
        s_mem_state.global_stats.types[type_id].current_bytes += size;
        if (s_mem_state.global_stats.types[type_id].current_bytes > s_mem_state.global_stats.types[type_id].peak_bytes) {
            s_mem_state.global_stats.types[type_id].peak_bytes = s_mem_state.global_stats.types[type_id].current_bytes;
        }
    }
    s_mem_state.global_stats.total_bytes += size;
    if (s_mem_state.global_stats.total_bytes > s_mem_state.global_stats.peak_bytes) {
        s_mem_state.global_stats.peak_bytes = s_mem_state.global_stats.total_bytes;
    }

    lv_mutex_unlock(&s_mem_state.stats_mutex);
}

void lv_mem_record_free(int type_id, size_t size) {
    if (type_id < 0) {
        return;
    }

    lv_once(&s_mem_state.stats_once, stats_mutex_init_func);
    lv_mutex_lock(&s_mem_state.stats_mutex);

    if ((size_t) type_id < (size_t) s_mem_state.global_stats.type_count) {
        s_mem_state.global_stats.types[type_id].total_frees++;
        if (s_mem_state.global_stats.types[type_id].current_bytes >= size) {
            s_mem_state.global_stats.types[type_id].current_bytes -= size;
        }
    }
    if (s_mem_state.global_stats.total_bytes >= size) {
        s_mem_state.global_stats.total_bytes -= size;
    }

    lv_mutex_unlock(&s_mem_state.stats_mutex);
}

void lv_mem_get_global_stats(lvMemoryStats *stats) {
    if (!stats) {
        return;
    }

    lv_once(&s_mem_state.stats_once, stats_mutex_init_func);
    lv_mutex_lock(&s_mem_state.stats_mutex);
    memcpy(stats, &s_mem_state.global_stats, sizeof(lvMemoryStats));
    lv_mutex_unlock(&s_mem_state.stats_mutex);
}

void lv_mem_reset_stats(void) {
    lv_once(&s_mem_state.stats_once, stats_mutex_init_func);
    lv_mutex_lock(&s_mem_state.stats_mutex);
    /* 释放已注册类型的名称字符串，防止内存泄漏 */
    for (int i = 0; i < s_mem_state.global_stats.type_count; i++) {
        if (s_mem_state.global_stats.types[i].name) {
            lv_free((void **) &s_mem_state.global_stats.types[i].name);
        }
    }
    memset(&s_mem_state.global_stats, 0, sizeof(lvMemoryStats));
    lv_mutex_unlock(&s_mem_state.stats_mutex);
}

void lv_mem_print_stats(void *stream) {
    if (!stream) {
        stream = stdout;
    }

    lv_once(&s_mem_state.stats_once, stats_mutex_init_func);
    lv_mutex_lock(&s_mem_state.stats_mutex);

    fprintf((FILE *) stream, "\n========== Lv-00 内存统计 ==========\n");
    fprintf((FILE *) stream, "总使用: %llu 字节, 峰值: %llu 字节\n", (unsigned long long) s_mem_state.global_stats.total_bytes,
            (unsigned long long) s_mem_state.global_stats.peak_bytes);
    fprintf((FILE *) stream, "\n各类型统计:\n");
    fprintf((FILE *) stream, "%-24s %12s %12s %12s %12s\n", "类型", "分配次数", "释放次数", "当前字节", "峰值字节");
    fprintf((FILE *) stream, "------------------------------------------------------------\n");

    for (int i = 0; i < s_mem_state.global_stats.type_count; i++) {
        lvMemTypeStat *s = &s_mem_state.global_stats.types[i];
        fprintf((FILE *) stream, "%-24s %12llu %12llu %12llu %12llu\n", s->name ? s->name : "(unnamed)",
                (unsigned long long) s->total_allocs, (unsigned long long) s->total_frees,
                (unsigned long long) s->current_bytes, (unsigned long long) s->peak_bytes);
    }

    fprintf((FILE *) stream, "====================================\n\n");

    lv_mutex_unlock(&s_mem_state.stats_mutex);
}

/* ============== 预定义对象池 ============== */

/* 对象大小定义 —— 集中管理于 config.h，此处引用 */
#define lv_CONSTRAINT_NODE_SIZE lv_CONFIG_POOL_CONSTRAINT_NODE_SIZE
#define lv_CONSTRAINT_SIZE lv_CONFIG_POOL_CONSTRAINT_SIZE
#define lv_SYMBOLIC_COORD_SIZE lv_CONFIG_POOL_SYMBOLIC_COORD_SIZE
#define lv_PROOF_STEP_SIZE lv_CONFIG_POOL_PROOF_STEP_SIZE

bool lv_init_preset_pools(void) {
    /* 防御性检查：防止二次初始化导致旧池泄漏 */
    if (s_mem_state.node_pool != NULL || s_mem_state.constraint_pool != NULL || s_mem_state.symbolic_coord_pool != NULL ||
        s_mem_state.proof_step_pool != NULL) {
        return true; /* 已经初始化 */
    }

    lvPoolConfig config = {
        .object_size = 0, .capacity = lv_POOL_DEFAULT_CAPACITY, .thread_safe = true, .auto_grow = true, .name = NULL};

    /* ConstraintNode 池 */
    config.object_size = lv_CONSTRAINT_NODE_SIZE;
    config.name = "ConstraintNode";
    s_mem_state.node_pool = lv_pool_create(&config);
    if (!s_mem_state.node_pool) {
        return false;
    }

    /* Constraint 池 */
    config.object_size = lv_CONSTRAINT_SIZE;
    config.name = "Constraint";
    s_mem_state.constraint_pool = lv_pool_create(&config);
    if (!s_mem_state.constraint_pool) {
        lv_pool_destroy(s_mem_state.node_pool);
        s_mem_state.node_pool = NULL;
        return false;
    }

    /* SymbolicCoord 池 */
    config.object_size = lv_SYMBOLIC_COORD_SIZE;
    config.name = "SymbolicCoord";
    s_mem_state.symbolic_coord_pool = lv_pool_create(&config);
    if (!s_mem_state.symbolic_coord_pool) {
        lv_pool_destroy(s_mem_state.node_pool);
        lv_pool_destroy(s_mem_state.constraint_pool);
        s_mem_state.node_pool = NULL;
        s_mem_state.constraint_pool = NULL;
        return false;
    }

    /* ProofStep 池 */
    config.object_size = lv_PROOF_STEP_SIZE;
    config.name = "ProofStep";
    s_mem_state.proof_step_pool = lv_pool_create(&config);
    if (!s_mem_state.proof_step_pool) {
        lv_pool_destroy(s_mem_state.node_pool);
        lv_pool_destroy(s_mem_state.constraint_pool);
        lv_pool_destroy(s_mem_state.symbolic_coord_pool);
        s_mem_state.node_pool = NULL;
        s_mem_state.constraint_pool = NULL;
        s_mem_state.symbolic_coord_pool = NULL;
        return false;
    }

    return true;
}

void lv_cleanup_preset_pools(void) {
    if (s_mem_state.proof_step_pool) {
        lv_pool_destroy(s_mem_state.proof_step_pool);
        s_mem_state.proof_step_pool = NULL;
    }
    if (s_mem_state.symbolic_coord_pool) {
        lv_pool_destroy(s_mem_state.symbolic_coord_pool);
        s_mem_state.symbolic_coord_pool = NULL;
    }
    if (s_mem_state.constraint_pool) {
        lv_pool_destroy(s_mem_state.constraint_pool);
        s_mem_state.constraint_pool = NULL;
    }
    if (s_mem_state.node_pool) {
        lv_pool_destroy(s_mem_state.node_pool);
        s_mem_state.node_pool = NULL;
    }
}

lvObjectPool *lv_get_node_pool(void) {
    return s_mem_state.node_pool;
}

lvObjectPool *lv_get_constraint_pool(void) {
    return s_mem_state.constraint_pool;
}

lvObjectPool *lv_get_symbolic_coord_pool(void) {
    return s_mem_state.symbolic_coord_pool;
}

lvObjectPool *lv_get_proof_step_pool(void) {
    return s_mem_state.proof_step_pool;
}
