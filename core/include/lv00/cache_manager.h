/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 4.0.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 配置常量 ============== */

/** 默认缓存大小 (64KB) */
#define LV00_CACHE_DEFAULT_SIZE (64 * 1024)

/** 最大缓存条目数 */
#define LV00_CACHE_MAX_ENTRIES 1024

/** 数据块默认大小 */
#define LV00_CACHE_BLOCK_SIZE 4096

/** 最大上下文嵌套深度 */
#define LV00_CACHE_MAX_CONTEXT_DEPTH 32

/** 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C563043  /* "LV0C" */

/* ============== 前向声明 ============== */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00DataChunk Lv00DataChunk;

/* ============== 缓存策略 ============== */

typedef enum {
    LV00_CACHE_STRATEGY_LRU,      /**< 最近最少使用 */
    LV00_CACHE_STRATEGY_LFU,      /**< 最少使用频率 */
    LV00_CACHE_STRATEGY_FIFO,     /**< 先进先出 */
    LV00_CACHE_STRATEGY_TTL,      /**< 生存时间 */
    LV00_CACHE_STRATEGY_ADAPTIVE  /**< 自适应策略 */
} Lv00CacheStrategy;

/* ============== 缓存条目 ============== */

/**
 * @brief 缓存条目
 */
struct Lv00CacheEntry {
    uint32_t entry_id;           /**< 条目ID */
    char key[64];                /**< 键 */
    void *data;                  /**< 数据指针 */
    size_t data_size;            /**< 数据大小 */

    /* LRU链表 */
    Lv00CacheEntry *lru_prev;     /**< LRU前驱 */
    Lv00CacheEntry *lru_next;     /**< LRU后继 */

    /* 哈希表链 */
    Lv00CacheEntry *hash_next;    /**< 哈希冲突链 */

    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    int64_t last_access_time;    /**< 最后访问时间 */
    int64_t create_time;         /**< 创建时间 */
    uint32_t ttl_ms;             /**< 生存时间（毫秒，0表示永久） */
    uint32_t context_id;         /**< 所属上下文ID */

    /* 数据块（用于大容量数据） */
    Lv00DataChunk *chunks;       /**< 数据块链表 */
    int chunk_count;             /**< 块数量 */

    /* 自定义析构 */
    void (*destructor)(void *data, size_t size);
};

/* ============== 数据块 ============== */

/**
 * @brief 数据块结构
 */
struct Lv00DataChunk {
    uint8_t *data;           /**< 数据指针 */
    size_t size;             /**< 数据大小 */
    size_t capacity;         /**< 容量 */
    uint32_t chunk_id;       /**< 块ID */
    Lv00DataChunk *next;     /**< 下一块 */
};

/* ============== 缓存上下文 ============== */

/**
 * @brief 缓存上下文
 */
struct Lv00CacheContext {
    uint32_t context_id;             /**< 上下文ID */
    char name[64];                   /**< 上下文名称 */
    uint32_t parent_id;              /**< 父上下文ID */
    int depth;                       /**< 嵌套深度 */
    bool is_active;                  /**< 是否活跃 */

    /* 统计 */
    size_t total_size;               /**< 总大小 */
    uint64_t hit_count;              /**< 命中次数 */
    uint64_t miss_count;             /**< 未命中次数 */
};

/* ============== 缓存管理器配置 ============== */

/**
 * @brief 缓存管理器配置
 */
typedef struct {
    size_t max_cache_size;           /**< 最大缓存大小 */
    int max_entries;                 /**< 最大条目数 */
    Lv00CacheStrategy strategy;      /**< 淘汰策略 */
    bool enable_auto_evict;          /**< 是否自动淘汰 */
} Lv00CacheConfig;

/* ============== 缓存管理器 ============== */

/**
 * @brief 缓存管理器
 */
struct Lv00CacheManager {
    uint32_t magic;                  /**< 魔法数 */
    bool is_running;                 /**< 运行状态 */

    Lv00CacheConfig config;          /**< 配置 */

    /* 哈希表 */
    Lv00CacheEntry **buckets;        /**< 哈希桶数组 */
    int bucket_count;                /**< 桶数量 */

    /* LRU链表 */
    Lv00CacheEntry *lru_head;        /**< LRU头（最近使用） */
    Lv00CacheEntry *lru_tail;        /**< LRU尾（最久未使用） */

    /* 上下文 */
    Lv00CacheContext *contexts;       /**< 上下文数组 */
    int context_count;               /**< 上下文数量 */
    int context_capacity;            /**< 上下文容量 */
    uint32_t current_context_id;     /**< 当前活跃上下文ID */
    uint32_t next_context_id;        /**< 下一个上下文ID */

    /* 统计 */
    uint64_t total_hits;             /**< 总命中次数 */
    uint64_t total_misses;           /**< 总未命中次数 */
    uint64_t total_evictions;        /**< 总淘汰次数 */
    size_t current_size;             /**< 当前使用大小 */

    /* 条目计数 */
    int entry_count;                 /**< 当前条目数 */

    /* 互斥锁 */
    void *mutex;                     /**< 互斥锁 */
};

/* ============== API 函数声明 ============== */

/**
 * @brief 创建缓存管理器
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(
    const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager);

/**
 * @brief 检查缓存管理器是否有效
 */
LV00_PUBLIC_API bool lv00_cache_manager_is_valid(const Lv00CacheManager *manager);

/**
 * @brief 重置缓存管理器
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_cache_manager_reset(Lv00CacheManager *manager);

/**
 * @brief 存储数据到缓存
 */
LV00_PUBLIC_API bool lv00_cache_put(Lv00CacheManager *manager,
                                     const char *key,
                                     const void *data,
                                     size_t size);

/**
 * @brief 从缓存获取数据
 */
LV00_PUBLIC_API bool lv00_cache_mgr_get(Lv00CacheManager *manager,
                                     const char *key,
                                     void **out_data,
                                     size_t *out_size);

/**
 * @brief 从缓存移除数据
 */
LV00_PUBLIC_API bool lv00_cache_mgr_remove(Lv00CacheManager *manager,
                                        const char *key);

/**
 * @brief 检查键是否存在
 */
LV00_PUBLIC_API bool lv00_cache_contains(Lv00CacheManager *manager,
                                           const char *key);

/**
 * @brief 创建缓存上下文
 */
LV00_PUBLIC_API uint32_t lv00_cache_context_create(Lv00CacheManager *manager,
                                                    const char *name,
                                                    uint32_t parent_id);

/**
 * @brief 切换当前活跃上下文
 */
LV00_PUBLIC_API bool lv00_cache_context_switch(Lv00CacheManager *manager,
                                                 uint32_t context_id);

/**
 * @brief 获取当前活跃上下文ID
 */
LV00_PUBLIC_API uint32_t lv00_cache_context_current(const Lv00CacheManager *manager);

/**
 * @brief 销毁缓存上下文
 */
LV00_PUBLIC_API bool lv00_cache_context_destroy(Lv00CacheManager *manager,
                                                  uint32_t context_id);

/**
 * @brief 获取全局统计信息
 */
LV00_PUBLIC_API void lv00_cache_mgr_get_stats(const Lv00CacheManager *manager,
                                            uint64_t *out_hits,
                                            uint64_t *out_misses,
                                            size_t *out_size);

/**
 * @brief 获取上下文统计信息
 */
LV00_PUBLIC_API void lv00_cache_get_context_stats(const Lv00CacheManager *manager,
                                                   uint32_t context_id,
                                                   uint64_t *out_hits,
                                                   uint64_t *out_misses,
                                                   size_t *out_size);

/**
 * @brief 获取当前条目数
 */
LV00_PUBLIC_API int lv00_cache_entry_count(const Lv00CacheManager *manager);

/**
 * @brief 获取当前缓存大小
 */
LV00_PUBLIC_API size_t lv00_cache_current_size(const Lv00CacheManager *manager);

/**
 * @brief 获取配置
 */
LV00_PUBLIC_API const Lv00CacheConfig *lv00_cache_get_config(
    const Lv00CacheManager *manager);

/**
 * @brief 设置自定义析构函数
 */
LV00_PUBLIC_API void lv00_cache_set_destructor(Lv00CacheManager *manager,
                                               void (*destructor)(void *, size_t));

/**
 * @brief 清空缓存
 */
LV00_PUBLIC_API void lv00_cache_mgr_clear(Lv00CacheManager *manager);

#ifdef __cplusplus
}
#endif

#endif /* LV00_CACHE_MANAGER_H */
