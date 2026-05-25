/**
 * @file memory_pool.c
 * @brief 内存池系统实现
 *
 * @details 实现三种内存管理策略：
 *   1. 固定大小对象池
 *   2. 线性分配器
 *   3. 对象缓存（LRU）
 *
 * 设计说明：本模块作为底层内存基础设施，池结构体本身使用 lv00_malloc/lv00_free
 * 分配，而内部数据块（性能关键路径）保留原生 malloc/free/realloc/calloc，
 * 原因：
 * 1. 避免循环依赖：lv00_malloc 内部可能依赖内存池，而内存池不能依赖 lv00_malloc
 *    进行内部块分配
 * 2. 对象池/线性分配器有独立的内存管理策略，内部块不走 lv00 的统计系统
 * 3. 全局内存统计（lv00_mem_*）是可选的附加功能，不影响核心分配路径
 * 4. 池结构体（Lv00ObjectPool、Lv00LinearAllocator、Lv00ObjectCache 等）
 *    的生命周期管理使用 lv00_malloc/lv00_free，便于统一追踪和调试
 *
 * 【为何内部数据块使用标准 malloc/free 而非 lv00_malloc/lv00_free】
 *
 * memory_pool 是整个项目的底层内存基础设施，位于依赖链的最底层。
 * 如果内部数据块也使用 lv00_malloc/lv00_free，会形成循环依赖：
 *
 *   lv00_malloc() → 内存统计(lv00_mem_record_alloc) → 可能触发内存池分配
 *   → lv00_pool_alloc() → 内部 malloc → lv00_malloc() → ...（无限递归）
 *
 * 因此，内部数据块（对象池中的内存块、线性分配器的 MemoryBlock、
 * 缓存条目的 CacheEntry 等）必须使用标准库的 malloc/free/realloc/calloc，
 * 绕过 lv00 的统计和追踪系统。
 *
 * 而池结构体本身（Lv00ObjectPool、Lv00LinearAllocator 等）的分配/释放
 * 使用 lv00_malloc/lv00_free，因为这些操作发生在池创建/销毁时，
 * 不在性能关键路径上，且便于统一追踪和调试。
 *
 * 使用约定：通过 lv00_pool_alloc 分配的对象必须通过 lv00_pool_free 释放，
 * 严禁混用 lv00_free 或标准 free。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "memory_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "lv00_internal.h"  /* LV00_FNV64_* 哈希常量 */
#include "lv00_utils.h"     /* lv00_strdup, lv00_malloc, lv00_free */

/* ============== 内部常量 ============== */

/** 默认对齐 */
#define LV00_DEFAULT_ALIGNMENT 16

/** 对象池增长因子 */
#define LV00_POOL_GROWTH_FACTOR 2

/** 哈希表初始容量 */
#define LV00_HASH_TABLE_INIT_CAP 64

/* [Bug修复] 溢出检查宏：检测 size_t 乘法是否溢出 */
#define LV00_SIZE_MUL_OVERFLOW(a, b) ((a) != 0 && (b) > SIZE_MAX / (a))

/* ============== 平台抽象层（线程安全） ============== */

#ifdef _WIN32
typedef CRITICAL_SECTION Lv00Mutex;
#define LV00_MUTEX_INIT(m) InitializeCriticalSection(&(m))
#define LV00_MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
#define LV00_MUTEX_LOCK(m) EnterCriticalSection(&(m))
#define LV00_MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))
#else
typedef pthread_mutex_t Lv00Mutex;
#define LV00_MUTEX_INIT(m) pthread_mutex_init(&(m), NULL)
#define LV00_MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))
#define LV00_MUTEX_LOCK(m) pthread_mutex_lock(&(m))
#define LV00_MUTEX_UNLOCK(m) pthread_mutex_unlock(&(m))
#endif

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
struct Lv00ObjectPool {
    /* 配置 */
    size_t object_size;     /**< 单个对象大小 */
    size_t capacity;        /**< 当前容量 */
    bool thread_safe;       /**< 是否线程安全 */
    bool auto_grow;         /**< 是否自动扩展 */
    char name[32];          /**< 池名称 */

    /* 内存块 */
    void **blocks;          /**< 内存块数组 */
    size_t block_count;     /**< 内存块数量 */
    size_t block_capacity;  /**< 内存块数组容量 */
    size_t *block_capacities; /**< [Bug修复] 每块实际分配的对象数量，用于 pool_clear 正确重建空闲链表 */

    /* 空闲链表 */
    FreeNode *free_list;    /**< 空闲对象链表 */

    /* 统计 */
    uint64_t total_allocs;  /**< 总分配次数 */
    uint64_t total_frees;   /**< 总释放次数 */
    size_t current_used;    /**< 当前使用数量 */

    /* 线程安全 */
    Lv00Mutex mutex;        /**< 互斥锁 */
};

/* 对齐辅助函数 */
static inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

Lv00ObjectPool *lv00_pool_create(const Lv00PoolConfig *config) {
    if (!config || config->object_size == 0) {
        return NULL;
    }

    Lv00ObjectPool *pool = (Lv00ObjectPool *)lv00_malloc(sizeof(Lv00ObjectPool));
    if (!pool) {
        return NULL;
    }
    memset(pool, 0, sizeof(Lv00ObjectPool));

    /* 对象大小至少能存放一个 FreeNode 指针 */
    pool->object_size = align_up(config->object_size, sizeof(void *));
    pool->capacity = config->capacity > 0 ? config->capacity : LV00_POOL_DEFAULT_CAPACITY;
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
        LV00_MUTEX_INIT(pool->mutex);
    }

    /* 分配初始内存块数组 */
    pool->block_capacity = 4;
    pool->blocks = (void **)lv00_malloc(pool->block_capacity * sizeof(void *));
    if (!pool->blocks) {
        lv00_free(pool);
        return NULL;
    }

    /* [Bug修复] 分配块容量记录数组 */
    pool->block_capacities = (size_t *)lv00_malloc(pool->block_capacity * sizeof(size_t));
    if (!pool->block_capacities) {
        lv00_free(pool->blocks);
        lv00_free(pool);
        return NULL;
    }

    /* 分配第一个内存块（性能关键路径：保留原生 malloc，避免循环依赖开销） */
    void *block = malloc(pool->object_size * pool->capacity);
    if (!block) {
        lv00_free(pool->blocks);
        lv00_free(pool);
        return NULL;
    }
    pool->blocks[0] = block;
    pool->block_capacities[0] = pool->capacity;  /* [Bug修复] 记录首块容量 */
    pool->block_count = 1;

    /* 初始化空闲链表 */
    pool->free_list = NULL;
    char *ptr = (char *)block;
    for (size_t i = 0; i < pool->capacity; i++) {
        FreeNode *node = (FreeNode *)(ptr + i * pool->object_size);
        node->next = pool->free_list;
        pool->free_list = node;
    }

    return pool;
}

void lv00_pool_destroy(Lv00ObjectPool *pool) {
    if (!pool) {
        return;
    }

    /* 释放所有内存块（性能关键路径：保留原生 free） */
    for (size_t i = 0; i < pool->block_count; i++) {
        free(pool->blocks[i]);
    }
    lv00_free(pool->blocks);
    lv00_free(pool->block_capacities);  /* [Bug修复] 释放块容量记录数组 */

    /* 销毁线程锁 */
    if (pool->thread_safe) {
        LV00_MUTEX_DESTROY(pool->mutex);
    }

    lv00_free(pool);
}

void *lv00_pool_alloc(Lv00ObjectPool *pool) {
    if (!pool) {
        return NULL;
    }

    if (pool->thread_safe) {
        LV00_MUTEX_LOCK(pool->mutex);
    }

    /* 空闲链表为空，需要扩展 */
    if (!pool->free_list) {
        if (!pool->auto_grow) {
            if (pool->thread_safe) {
                LV00_MUTEX_UNLOCK(pool->mutex);
            }
            return NULL;
        }

        /* 扩展内存块数组 */
        if (pool->block_count >= pool->block_capacity) {
            /* [Bug修复] 溢出检查：确保 block_capacity * GROWTH_FACTOR 不会溢出 */
            if (LV00_SIZE_MUL_OVERFLOW(pool->block_capacity, LV00_POOL_GROWTH_FACTOR)) {
                if (pool->thread_safe) { LV00_MUTEX_UNLOCK(pool->mutex); }
                return NULL;
            }
            size_t new_cap = pool->block_capacity * LV00_POOL_GROWTH_FACTOR;

            /* [Bug修复] 溢出检查：确保 new_cap * sizeof(void*) 不会溢出 */
            if (LV00_SIZE_MUL_OVERFLOW(new_cap, sizeof(void *))) {
                if (pool->thread_safe) { LV00_MUTEX_UNLOCK(pool->mutex); }
                return NULL;
            }
            void **new_blocks = (void **)lv00_realloc(pool->blocks, new_cap * sizeof(void *));
            if (!new_blocks) {
                if (pool->thread_safe) {
                    LV00_MUTEX_UNLOCK(pool->mutex);
                }
                return NULL;
            }
            pool->blocks = new_blocks;

            /* [Bug修复] 同步扩展 block_capacities 数组 */
            size_t *new_bcaps = (size_t *)lv00_realloc(pool->block_capacities, new_cap * sizeof(size_t));
            if (!new_bcaps) {
                /* 回滚 blocks 扩展 */
                pool->blocks = (void **)lv00_realloc(new_blocks, pool->block_capacity * sizeof(void *));
                if (pool->thread_safe) { LV00_MUTEX_UNLOCK(pool->mutex); }
                return NULL;
            }
            pool->block_capacities = new_bcaps;
            pool->block_capacity = new_cap;
        }

        /* 分配新内存块 */
        /* [Bug修复] 溢出检查：确保 capacity * GROWTH_FACTOR 不会溢出 */
        if (LV00_SIZE_MUL_OVERFLOW(pool->capacity, LV00_POOL_GROWTH_FACTOR)) {
            if (pool->thread_safe) { LV00_MUTEX_UNLOCK(pool->mutex); }
            return NULL;
        }
        size_t new_capacity = pool->capacity * LV00_POOL_GROWTH_FACTOR;

        /* [Bug修复] 溢出检查：确保 object_size * new_capacity 不会溢出 */
        if (LV00_SIZE_MUL_OVERFLOW(pool->object_size, new_capacity)) {
            if (pool->thread_safe) { LV00_MUTEX_UNLOCK(pool->mutex); }
            return NULL;
        }
        /* 分配新内存块（性能关键路径：保留原生 malloc，避免循环依赖开销） */
        void *block = malloc(pool->object_size * new_capacity);
        if (!block) {
            if (pool->thread_safe) {
                LV00_MUTEX_UNLOCK(pool->mutex);
            }
            return NULL;
        }
        pool->blocks[pool->block_count] = block;
        pool->block_capacities[pool->block_count] = new_capacity;  /* [Bug修复] 记录新块容量 */
        pool->block_count++;

        /* 添加到空闲链表 */
        char *ptr = (char *)block;
        for (size_t i = 0; i < new_capacity; i++) {
            FreeNode *node = (FreeNode *)(ptr + i * pool->object_size);
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
        LV00_MUTEX_UNLOCK(pool->mutex);
    }

    /* 清零对象 */
    memset(node, 0, pool->object_size);
    return node;
}

bool lv00_pool_free(Lv00ObjectPool *pool, void *obj) {
    if (!pool || !obj) {
        return false;
    }

    if (pool->thread_safe) {
        LV00_MUTEX_LOCK(pool->mutex);
    }

    /* 添加到空闲链表 */
    FreeNode *node = (FreeNode *)obj;
    node->next = pool->free_list;
    pool->free_list = node;
    pool->total_frees++;
    pool->current_used--;

    if (pool->thread_safe) {
        LV00_MUTEX_UNLOCK(pool->mutex);
    }

    return true;
}

void lv00_pool_get_stats(Lv00ObjectPool *pool,
                         uint64_t *out_total_allocs,
                         uint64_t *out_total_frees,
                         size_t *out_current_used) {
    if (!pool) {
        return;
    }

    if (pool->thread_safe) {
        LV00_MUTEX_LOCK(pool->mutex);
    }

    if (out_total_allocs) *out_total_allocs = pool->total_allocs;
    if (out_total_frees) *out_total_frees = pool->total_frees;
    if (out_current_used) *out_current_used = pool->current_used;

    if (pool->thread_safe) {
        LV00_MUTEX_UNLOCK(pool->mutex);
    }
}

void lv00_pool_clear(Lv00ObjectPool *pool) {
    if (!pool) {
        return;
    }

    if (pool->thread_safe) {
        LV00_MUTEX_LOCK(pool->mutex);
    }

    /* [Bug修复] 使用 block_capacities 记录的实际容量重建空闲链表，
     * 替代原来基于 LV00_POOL_DEFAULT_CAPACITY 的错误计算 */
    pool->free_list = NULL;
    for (size_t b = 0; b < pool->block_count; b++) {
        char *ptr = (char *)pool->blocks[b];
        size_t block_size = pool->block_capacities[b];
        for (size_t i = 0; i < block_size; i++) {
            FreeNode *node = (FreeNode *)(ptr + i * pool->object_size);
            node->next = pool->free_list;
            pool->free_list = node;
        }
    }
    pool->current_used = 0;

    if (pool->thread_safe) {
        LV00_MUTEX_UNLOCK(pool->mutex);
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
    char data[];  /**< 柔性数组 */
} MemoryBlock;

/**
 * @brief 线性分配器结构
 */
struct Lv00LinearAllocator {
    MemoryBlock *blocks;        /**< 内存块链表 */
    size_t block_size;          /**< 默认块大小 */
    size_t total_blocks;        /**< 总块数 */
    size_t total_used;          /**< 总使用字节数 */
    size_t total_capacity;      /**< 总容量 */
};

Lv00LinearAllocator *lv00_linear_allocator_create(size_t block_size) {
    Lv00LinearAllocator *allocator = (Lv00LinearAllocator *)lv00_malloc(sizeof(Lv00LinearAllocator));
    if (!allocator) {
        return NULL;
    }
    memset(allocator, 0, sizeof(Lv00LinearAllocator));

    allocator->block_size = block_size > 0 ? block_size : LV00_LINEAR_ALLOCATOR_BLOCK_SIZE;

    /* 预分配第一个块（性能关键路径：保留原生 malloc，避免循环依赖开销） */
    MemoryBlock *block = (MemoryBlock *)malloc(sizeof(MemoryBlock) + allocator->block_size);
    if (!block) {
        lv00_free(allocator);
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

void lv00_linear_allocator_destroy(Lv00LinearAllocator *allocator) {
    if (!allocator) {
        return;
    }

    /* 释放所有内存块（性能关键路径：保留原生 free） */
    MemoryBlock *block = allocator->blocks;
    while (block) {
        MemoryBlock *next = block->next;
        free(block);
        block = next;
    }

    lv00_free(allocator);
}

void *lv00_linear_alloc(Lv00LinearAllocator *allocator, size_t size, size_t alignment) {
    if (!allocator || size == 0) {
        return NULL;
    }

    size_t align = alignment > 0 ? alignment : LV00_DEFAULT_ALIGNMENT;
    align = align < sizeof(void *) ? sizeof(void *) : align;

    /* 查找有足够空间的块 */
    MemoryBlock *block = allocator->blocks;
    while (block) {
        /* 计算对齐后的起始位置 */
        uintptr_t start = (uintptr_t)(block->data + block->used);
        uintptr_t aligned = (start + align - 1) & ~(align - 1);
        size_t padding = aligned - start;
        size_t total_needed = padding + size;

        if (block->used + total_needed <= block->size) {
            /* 找到足够空间 */
            void *ptr = (void *)aligned;
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

    MemoryBlock *new_block = (MemoryBlock *)malloc(sizeof(MemoryBlock) + new_block_size);
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
    uintptr_t start = (uintptr_t)new_block->data;
    uintptr_t aligned = (start + align - 1) & ~(align - 1);
    size_t padding = aligned - start;
    void *ptr = (void *)aligned;
    new_block->used = padding + size;
    allocator->total_used += padding + size;

    return ptr;
}

void lv00_linear_allocator_reset(Lv00LinearAllocator *allocator) {
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

void lv00_linear_allocator_get_stats(const Lv00LinearAllocator *allocator,
                                     size_t *out_total_blocks,
                                     size_t *out_used_bytes,
                                     size_t *out_capacity_bytes) {
    if (!allocator) {
        return;
    }
    if (out_total_blocks) *out_total_blocks = allocator->total_blocks;
    if (out_used_bytes) *out_used_bytes = allocator->total_used;
    if (out_capacity_bytes) *out_capacity_bytes = allocator->total_capacity;
}

/* ============== 对象缓存（LRU）实现 ============== */

/**
 * @brief 缓存条目
 */
typedef struct CacheEntry {
    Lv00CacheKey key;
    void *value;
    struct CacheEntry *prev;
    struct CacheEntry *next;
    struct CacheEntry *hash_next;  /**< 哈希表链 */
} CacheEntry;

/**
 * @brief 对象缓存结构
 */
struct Lv00ObjectCache {
    /* 配置 */
    size_t capacity;
    Lv00CacheCreateFunc create_func;
    Lv00CacheDestroyFunc destroy_func;
    void *user_data;

    /* LRU 双向链表 */
    CacheEntry *head;  /**< 最近使用 */
    CacheEntry *tail;  /**< 最少使用 */

    /* 哈希表 */
    CacheEntry **hash_table;
    size_t hash_capacity;

    /* 统计 */
    uint64_t hits;
    uint64_t misses;
    size_t current_size;
};

/* FNV-1a 哈希函数（使用项目统一的 LV00_FNV64_* 常量，定义在 lv00_internal.h） */
static inline size_t hash_key(Lv00CacheKey key, size_t capacity) {
    uint64_t hash = LV00_FNV64_OFFSET_BASIS;
    /* 对 8 字节 key 逐字节进行 FNV-1a 混合 */
    const unsigned char *bytes = (const unsigned char *)&key;
    for (size_t i = 0; i < sizeof(key); i++) {
        hash ^= bytes[i];
        hash *= LV00_FNV64_PRIME;
    }
    return (size_t)(hash % capacity);
}

Lv00ObjectCache *lv00_cache_create(size_t capacity,
                                   Lv00CacheCreateFunc create_func,
                                   Lv00CacheDestroyFunc destroy_func,
                                   void *user_data) {
    if (!create_func) {
        return NULL;
    }

    Lv00ObjectCache *cache = (Lv00ObjectCache *)lv00_malloc(sizeof(Lv00ObjectCache));
    if (!cache) {
        return NULL;
    }
    memset(cache, 0, sizeof(Lv00ObjectCache));

    cache->capacity = capacity > 0 ? capacity : LV00_LRU_CACHE_DEFAULT_CAPACITY;
    cache->create_func = create_func;
    cache->destroy_func = destroy_func;
    cache->user_data = user_data;

    /* 分配哈希表 */
    cache->hash_capacity = LV00_HASH_TABLE_INIT_CAP;
    cache->hash_table = (CacheEntry **)calloc(cache->hash_capacity, sizeof(CacheEntry *));
    if (!cache->hash_table) {
        lv00_free(cache);
        return NULL;
    }

    return cache;
}

void lv00_cache_destroy(Lv00ObjectCache *cache) {
    if (!cache) {
        return;
    }

    /* 释放所有条目（性能关键路径：保留原生 free） */
    CacheEntry *entry = cache->head;
    while (entry) {
        CacheEntry *next = entry->next;
        if (cache->destroy_func && entry->value) {
            cache->destroy_func(entry->value, cache->user_data);
        }
        free(entry);
        entry = next;
    }

    free(cache->hash_table);
    lv00_free(cache);
}

/* 将条目移到链表头部 */
static void move_to_head(Lv00ObjectCache *cache, CacheEntry *entry) {
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
static void evict_lru(Lv00ObjectCache *cache) {
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
    if (cache->destroy_func && entry->value) {
        cache->destroy_func(entry->value, cache->user_data);
    }
    free(entry);
    cache->current_size--;
}

void *lv00_cache_get(Lv00ObjectCache *cache, Lv00CacheKey key) {
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
    entry = (CacheEntry *)malloc(sizeof(CacheEntry));
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

bool lv00_cache_remove(Lv00ObjectCache *cache, Lv00CacheKey key) {
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
            if (cache->destroy_func && entry->value) {
                cache->destroy_func(entry->value, cache->user_data);
            }
            free(entry);
            cache->current_size--;

            return true;
        }
        pp = &(*pp)->hash_next;
    }

    return false;
}

void lv00_cache_clear(Lv00ObjectCache *cache) {
    if (!cache) {
        return;
    }

    /* 释放所有条目 */
    CacheEntry *entry = cache->head;
    while (entry) {
        CacheEntry *next = entry->next;
        if (cache->destroy_func && entry->value) {
            cache->destroy_func(entry->value, cache->user_data);
        }
        free(entry);
        entry = next;
    }

    /* 清空哈希表 */
    memset(cache->hash_table, 0, cache->hash_capacity * sizeof(CacheEntry *));
    cache->head = NULL;
    cache->tail = NULL;
    cache->current_size = 0;
}

void lv00_cache_get_stats(const Lv00ObjectCache *cache,
                          uint64_t *out_hits,
                          uint64_t *out_misses,
                          size_t *out_current_size) {
    if (!cache) {
        return;
    }
    if (out_hits) *out_hits = cache->hits;
    if (out_misses) *out_misses = cache->misses;
    if (out_current_size) *out_current_size = cache->current_size;
}

/* ============== 全局内存统计 ============== */

static Lv00MemoryStats g_global_stats = {0};
static Lv00Mutex g_stats_mutex;
static volatile long g_stats_initialized = 0; /* 原子标志，用于线程安全的一次性初始化 */

/**
 * @brief 线程安全地确保全局统计互斥锁已初始化
 *
 * 使用 InterlockedCompareExchange（Windows）或 __atomic（POSIX/GCC）保证
 * 只有一个线程执行初始化，避免 ensure_stats_init() 的竞态条件。
 */
static void ensure_stats_init(void) {
#ifdef _WIN32
    if (InterlockedCompareExchange(&g_stats_initialized, 1, 0) == 0) {
        /* 首次调用：执行初始化 */
        LV00_MUTEX_INIT(g_stats_mutex);
    }
    /* 后续调用：g_stats_initialized 已为 1，直接返回 */
#else
    /* 使用 __atomic 内建保证原子比较-交换 */
    if (__atomic_exchange_n(&g_stats_initialized, 1, __ATOMIC_ACQ_REL) == 0) {
        /* 首次调用：执行初始化 */
        LV00_MUTEX_INIT(g_stats_mutex);
    }
#endif
}

int lv00_mem_register_type(const char *name) {
    if (!name) {
        return -1;
    }

    ensure_stats_init();
    LV00_MUTEX_LOCK(g_stats_mutex);

    if (g_global_stats.type_count >= LV00_MEM_STAT_MAX_TYPES) {
        LV00_MUTEX_UNLOCK(g_stats_mutex);
        return -1;
    }

    int id = g_global_stats.type_count++;
    g_global_stats.types[id].name = lv00_strdup(name);  /* 复制字符串，避免保存裸指针 */
    g_global_stats.types[id].total_allocs = 0;
    g_global_stats.types[id].total_frees = 0;
    g_global_stats.types[id].current_bytes = 0;
    g_global_stats.types[id].peak_bytes = 0;

    LV00_MUTEX_UNLOCK(g_stats_mutex);
    return id;
}

void lv00_mem_record_alloc(int type_id, size_t size) {
    if (type_id < 0) {
        return;
    }

    ensure_stats_init();
    LV00_MUTEX_LOCK(g_stats_mutex);

    if ((size_t)type_id < (size_t)g_global_stats.type_count) {
        g_global_stats.types[type_id].total_allocs++;
        g_global_stats.types[type_id].current_bytes += size;
        if (g_global_stats.types[type_id].current_bytes > g_global_stats.types[type_id].peak_bytes) {
            g_global_stats.types[type_id].peak_bytes = g_global_stats.types[type_id].current_bytes;
        }
    }
    g_global_stats.total_bytes += size;
    if (g_global_stats.total_bytes > g_global_stats.peak_bytes) {
        g_global_stats.peak_bytes = g_global_stats.total_bytes;
    }

    LV00_MUTEX_UNLOCK(g_stats_mutex);
}

void lv00_mem_record_free(int type_id, size_t size) {
    if (type_id < 0) {
        return;
    }

    ensure_stats_init();
    LV00_MUTEX_LOCK(g_stats_mutex);

    if ((size_t)type_id < (size_t)g_global_stats.type_count) {
        g_global_stats.types[type_id].total_frees++;
        if (g_global_stats.types[type_id].current_bytes >= size) {
            g_global_stats.types[type_id].current_bytes -= size;
        }
    }
    if (g_global_stats.total_bytes >= size) {
        g_global_stats.total_bytes -= size;
    }

    LV00_MUTEX_UNLOCK(g_stats_mutex);
}

void lv00_mem_get_global_stats(Lv00MemoryStats *stats) {
    if (!stats) {
        return;
    }

    ensure_stats_init();
    LV00_MUTEX_LOCK(g_stats_mutex);
    memcpy(stats, &g_global_stats, sizeof(Lv00MemoryStats));
    LV00_MUTEX_UNLOCK(g_stats_mutex);
}

void lv00_mem_reset_stats(void) {
    ensure_stats_init();
    LV00_MUTEX_LOCK(g_stats_mutex);
    /* 释放已注册类型的名称字符串，防止内存泄漏 */
    for (int i = 0; i < g_global_stats.type_count; i++) {
        if (g_global_stats.types[i].name) {
            lv00_free((void **)&g_global_stats.types[i].name);
        }
    }
    memset(&g_global_stats, 0, sizeof(Lv00MemoryStats));
    LV00_MUTEX_UNLOCK(g_stats_mutex);
}

void lv00_mem_print_stats(void *stream) {
    if (!stream) {
        stream = stdout;
    }

    ensure_stats_init();
    LV00_MUTEX_LOCK(g_stats_mutex);

    fprintf((FILE *)stream, "\n========== Lv-00 内存统计 ==========\n");
    fprintf((FILE *)stream, "总使用: %llu 字节, 峰值: %llu 字节\n",
            (unsigned long long)g_global_stats.total_bytes,
            (unsigned long long)g_global_stats.peak_bytes);
    fprintf((FILE *)stream, "\n各类型统计:\n");
    fprintf((FILE *)stream, "%-24s %12s %12s %12s %12s\n",
            "类型", "分配次数", "释放次数", "当前字节", "峰值字节");
    fprintf((FILE *)stream, "------------------------------------------------------------\n");

    for (int i = 0; i < g_global_stats.type_count; i++) {
        Lv00MemTypeStat *s = &g_global_stats.types[i];
        fprintf((FILE *)stream, "%-24s %12llu %12llu %12llu %12llu\n",
                s->name ? s->name : "(unnamed)",
                (unsigned long long)s->total_allocs,
                (unsigned long long)s->total_frees,
                (unsigned long long)s->current_bytes,
                (unsigned long long)s->peak_bytes);
    }

    fprintf((FILE *)stream, "====================================\n\n");

    LV00_MUTEX_UNLOCK(g_stats_mutex);
}

/* ============== 预定义对象池 ============== */

/* 对象大小定义 —— 集中管理于 config.h，此处引用 */
#define LV00_CONSTRAINT_NODE_SIZE LV00_CONFIG_POOL_CONSTRAINT_NODE_SIZE
#define LV00_CONSTRAINT_SIZE      LV00_CONFIG_POOL_CONSTRAINT_SIZE
#define LV00_SYMBOLIC_COORD_SIZE  LV00_CONFIG_POOL_SYMBOLIC_COORD_SIZE
#define LV00_PROOF_STEP_SIZE      LV00_CONFIG_POOL_PROOF_STEP_SIZE

/* 全局对象池 */
static Lv00ObjectPool *g_node_pool = NULL;
static Lv00ObjectPool *g_constraint_pool = NULL;
static Lv00ObjectPool *g_symbolic_coord_pool = NULL;
static Lv00ObjectPool *g_proof_step_pool = NULL;

bool lv00_init_preset_pools(void) {
    Lv00PoolConfig config = {
        .object_size = 0,
        .capacity = LV00_POOL_DEFAULT_CAPACITY,
        .thread_safe = true,
        .auto_grow = true,
        .name = NULL
    };

    /* ConstraintNode 池 */
    config.object_size = LV00_CONSTRAINT_NODE_SIZE;
    config.name = "ConstraintNode";
    g_node_pool = lv00_pool_create(&config);
    if (!g_node_pool) {
        return false;
    }

    /* Constraint 池 */
    config.object_size = LV00_CONSTRAINT_SIZE;
    config.name = "Constraint";
    g_constraint_pool = lv00_pool_create(&config);
    if (!g_constraint_pool) {
        lv00_pool_destroy(g_node_pool);
        g_node_pool = NULL;
        return false;
    }

    /* SymbolicCoord 池 */
    config.object_size = LV00_SYMBOLIC_COORD_SIZE;
    config.name = "SymbolicCoord";
    g_symbolic_coord_pool = lv00_pool_create(&config);
    if (!g_symbolic_coord_pool) {
        lv00_pool_destroy(g_node_pool);
        lv00_pool_destroy(g_constraint_pool);
        g_node_pool = NULL;
        g_constraint_pool = NULL;
        return false;
    }

    /* ProofStep 池 */
    config.object_size = LV00_PROOF_STEP_SIZE;
    config.name = "ProofStep";
    g_proof_step_pool = lv00_pool_create(&config);
    if (!g_proof_step_pool) {
        lv00_pool_destroy(g_node_pool);
        lv00_pool_destroy(g_constraint_pool);
        lv00_pool_destroy(g_symbolic_coord_pool);
        g_node_pool = NULL;
        g_constraint_pool = NULL;
        g_symbolic_coord_pool = NULL;
        return false;
    }

    return true;
}

void lv00_cleanup_preset_pools(void) {
    if (g_proof_step_pool) {
        lv00_pool_destroy(g_proof_step_pool);
        g_proof_step_pool = NULL;
    }
    if (g_symbolic_coord_pool) {
        lv00_pool_destroy(g_symbolic_coord_pool);
        g_symbolic_coord_pool = NULL;
    }
    if (g_constraint_pool) {
        lv00_pool_destroy(g_constraint_pool);
        g_constraint_pool = NULL;
    }
    if (g_node_pool) {
        lv00_pool_destroy(g_node_pool);
        g_node_pool = NULL;
    }
}

Lv00ObjectPool *lv00_get_node_pool(void) {
    return g_node_pool;
}

Lv00ObjectPool *lv00_get_constraint_pool(void) {
    return g_constraint_pool;
}

Lv00ObjectPool *lv00_get_symbolic_coord_pool(void) {
    return g_symbolic_coord_pool;
}

Lv00ObjectPool *lv00_get_proof_step_pool(void) {
    return g_proof_step_pool;
}
