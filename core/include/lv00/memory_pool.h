/* ========================================================================
 * 模块名称：内存池系统 (memory_pool)
 * 功能概述：提供三种高性能内存管理策略：固定大小对象池、线性分配器
 *          和 LRU 对象缓存。旨在减少内存碎片、提高分配速度，
 *          支持内存使用统计和线程安全（可选）。
 *
 * 主要 API：
 *   - lv00_pool_create / alloc / free / destroy  — 固定大小对象池
 *   - lv00_linear_allocator_create / alloc / reset — 线性分配器
 *   - lv00_cache_create / get / remove / destroy  — LRU 对象缓存
 *   - lv00_mem_register_type / record_alloc/free  — 全局内存统计
 *   - lv00_init_preset_pools / cleanup            — 预定义对象池
 *
 * 使用示例：
 *   Lv00PoolConfig cfg = { .object_size = sizeof(GeomNode), .capacity = 1024 };
 *   Lv00ObjectPool *pool = lv00_pool_create(&cfg);
 *   GeomNode *node = (GeomNode *)lv00_pool_alloc(pool);
 *   lv00_pool_free(pool, node);
 *   lv00_pool_destroy(pool);
 *
 * @version 1.1.0
 * ======================================================================== */

/**
 * @file memory_pool.h
 * @brief 内存池系统 —— 高性能对象复用与批量分配
 */

#ifndef LV00_MEMORY_POOL_H
#define LV00_MEMORY_POOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============== 配置常量 ============== */

/** 对象池默认容量 */
#ifndef LV00_POOL_DEFAULT_CAPACITY
#define LV00_POOL_DEFAULT_CAPACITY 1024
#endif

/** 线性分配器默认块大小 (64KB) */
#ifndef LV00_LINEAR_ALLOCATOR_BLOCK_SIZE
#define LV00_LINEAR_ALLOCATOR_BLOCK_SIZE (64 * 1024)
#endif

/** LRU 缓存默认容量 */
#ifndef LV00_LRU_CACHE_DEFAULT_CAPACITY
#define LV00_LRU_CACHE_DEFAULT_CAPACITY 256
#endif

/** 内存统计最大追踪类型数 */
#ifndef LV00_MEM_STAT_MAX_TYPES
#define LV00_MEM_STAT_MAX_TYPES 64
#endif

/* ============== 前向声明 ============== */

typedef struct Lv00ObjectPool Lv00ObjectPool;
typedef struct Lv00LinearAllocator Lv00LinearAllocator;
typedef struct Lv00ObjectCache Lv00ObjectCache;
typedef struct Lv00MemoryStats Lv00MemoryStats;

/* ============== 对象池（固定大小） ============== */

/**
 * @brief 对象池配置
 */
typedef struct {
    size_t object_size;     /**< 单个对象大小（字节） */
    size_t capacity;        /**< 初始容量 */
    bool thread_safe;       /**< 是否线程安全 */
    bool auto_grow;         /**< 容量不足时是否自动扩展 */
    const char *name;       /**< 池名称（用于调试） */
} Lv00PoolConfig;

/**
 * @brief 创建对象池
 * @param config 配置参数
 * @return 新创建的对象池，失败返回 NULL
 */
Lv00ObjectPool *lv00_pool_create(const Lv00PoolConfig *config);

/**
 * @brief 销毁对象池
 * @param pool 对象池指针
 */
void lv00_pool_destroy(Lv00ObjectPool *pool);

/**
 * @brief 从对象池分配一个对象
 * @param pool 对象池
 * @return 对象指针，失败返回 NULL
 */
void *lv00_pool_alloc(Lv00ObjectPool *pool);

/**
 * @brief 将对象归还到对象池
 * @param pool 对象池
 * @param obj 对象指针
 * @return 是否成功
 */
bool lv00_pool_free(Lv00ObjectPool *pool, void *obj);

/**
 * @brief 获取对象池统计信息
 * @param pool 对象池
 * @param out_total_allocs 输出：总分配次数
 * @param out_total_frees 输出：总释放次数
 * @param out_current_used 输出：当前使用数量
 */
void lv00_pool_get_stats(Lv00ObjectPool *pool,
                         uint64_t *out_total_allocs,
                         uint64_t *out_total_frees,
                         size_t *out_current_used);

/**
 * @brief 清空对象池（不销毁池本身）
 * @param pool 对象池
 */
void lv00_pool_clear(Lv00ObjectPool *pool);

/* ============== 线性分配器 ============== */

/**
 * @brief 创建线性分配器
 * @param block_size 每个内存块的大小（字节）
 * @return 新创建的线性分配器，失败返回 NULL
 */
Lv00LinearAllocator *lv00_linear_allocator_create(size_t block_size);

/**
 * @brief 销毁线性分配器
 * @param allocator 线性分配器指针
 */
void lv00_linear_allocator_destroy(Lv00LinearAllocator *allocator);

/**
 * @brief 从线性分配器分配内存
 * @param allocator 线性分配器
 * @param size 分配大小
 * @param alignment 对齐要求（必须为 2 的幂，0 表示默认对齐）
 * @return 内存指针，失败返回 NULL
 */
void *lv00_linear_alloc(Lv00LinearAllocator *allocator, size_t size, size_t alignment);

/**
 * @brief 重置线性分配器（释放所有分配，保留内存块）
 * @param allocator 线性分配器
 */
void lv00_linear_allocator_reset(Lv00LinearAllocator *allocator);

/**
 * @brief 获取线性分配器内存使用统计
 * @param allocator 线性分配器
 * @param out_total_blocks 输出：总块数
 * @param out_used_bytes 输出：已使用字节数
 * @param out_capacity_bytes 输出：总容量字节数
 */
void lv00_linear_allocator_get_stats(const Lv00LinearAllocator *allocator,
                                     size_t *out_total_blocks,
                                     size_t *out_used_bytes,
                                     size_t *out_capacity_bytes);

/* ============== 对象缓存（LRU） ============== */

/**
 * @brief 对象缓存键类型
 */
typedef uint64_t Lv00CacheKey;

/**
 * @brief 对象创建回调
 * @param key 对象键
 * @param user_data 用户数据
 * @return 新创建的对象
 */
typedef void *(*Lv00CacheCreateFunc)(Lv00CacheKey key, void *user_data);

/**
 * @brief 对象销毁回调
 * @param obj 对象指针
 * @param user_data 用户数据
 */
typedef void (*Lv00CacheDestroyFunc)(void *obj, void *user_data);

/**
 * @brief 创建对象缓存
 * @param capacity 最大容量
 * @param create_func 创建回调
 * @param destroy_func 销毁回调
 * @param user_data 用户数据
 * @return 新创建的对象缓存，失败返回 NULL
 */
Lv00ObjectCache *lv00_cache_create(size_t capacity,
                                   Lv00CacheCreateFunc create_func,
                                   Lv00CacheDestroyFunc destroy_func,
                                   void *user_data);

/**
 * @brief 销毁对象缓存
 * @param cache 对象缓存指针
 */
void lv00_cache_destroy(Lv00ObjectCache *cache);

/**
 * @brief 从缓存获取对象
 * @param cache 对象缓存
 * @param key 对象键
 * @return 对象指针，不存在则自动创建
 */
void *lv00_cache_get(Lv00ObjectCache *cache, Lv00CacheKey key);

/**
 * @brief 从缓存移除对象
 * @param cache 对象缓存
 * @param key 对象键
 * @return 是否成功移除
 */
bool lv00_cache_remove(Lv00ObjectCache *cache, Lv00CacheKey key);

/**
 * @brief 清空缓存
 * @param cache 对象缓存
 */
void lv00_cache_clear(Lv00ObjectCache *cache);

/**
 * @brief 获取缓存统计信息
 * @param cache 对象缓存
 * @param out_hits 输出：命中次数
 * @param out_misses 输出：未命中次数
 * @param out_current_size 输出：当前大小
 */
void lv00_cache_get_stats(const Lv00ObjectCache *cache,
                          uint64_t *out_hits,
                          uint64_t *out_misses,
                          size_t *out_current_size);

/* ============== 全局内存统计 ============== */

/**
 * @brief 内存类型统计条目
 */
typedef struct {
    const char *name;           /**< 类型名称 */
    uint64_t total_allocs;      /**< 总分配次数 */
    uint64_t total_frees;       /**< 总释放次数 */
    uint64_t current_bytes;     /**< 当前使用字节数 */
    uint64_t peak_bytes;        /**< 峰值使用字节数 */
} Lv00MemTypeStat;

/**
 * @brief 全局内存统计
 */
struct Lv00MemoryStats {
    /**
     * @brief 各类型统计数组
     *
     * 数组大小由 LV00_MEM_STAT_MAX_TYPES（当前为 64）控制。
     * 仅 types[0..type_count-1] 区间内的条目是有效的，
     * types[type_count..LV00_MEM_STAT_MAX_TYPES-1] 为零初始化的未使用条目。
     *
     * 【边界检查建议】
     *   遍历此数组时，应使用 type_count 作为上界，而非 LV00_MEM_STAT_MAX_TYPES：
     *     for (int i = 0; i < stats.type_count; i++) { ... }
     *   如果通过 lv00_mem_register_type() 注册的类型数超过 LV00_MEM_STAT_MAX_TYPES，
     *   注册将失败并返回 -1。建议在系统初始化阶段检查注册返回值。
     */
    Lv00MemTypeStat types[LV00_MEM_STAT_MAX_TYPES]; /**< 各类型统计 */
    int type_count;                                 /**< 已注册类型数 */
    uint64_t total_bytes;                           /**< 总使用字节数 */
    uint64_t peak_bytes;                            /**< 总峰值字节数 */
};

/**
 * @brief 注册内存类型
 * @param name 类型名称
 * @return 类型 ID，失败返回 -1
 */
int lv00_mem_register_type(const char *name);

/**
 * @brief 记录内存分配
 * @param type_id 类型 ID
 * @param size 分配大小
 */
void lv00_mem_record_alloc(int type_id, size_t size);

/**
 * @brief 记录内存释放
 * @param type_id 类型 ID
 * @param size 释放大小
 */
void lv00_mem_record_free(int type_id, size_t size);

/**
 * @brief 获取全局内存统计
 * @param stats 输出统计结构
 */
void lv00_mem_get_global_stats(Lv00MemoryStats *stats);

/**
 * @brief 重置全局内存统计
 */
void lv00_mem_reset_stats(void);

/**
 * @brief 打印内存统计报告
 *
 * @param stream 输出流（如 stdout）
 *
 * 【参数类型说明 —— 为什么使用 void* 而非 FILE*】
 *   此函数的 stream 参数类型为 void* 而非 FILE*，原因如下：
 *   1. 跨平台兼容性：在 Windows 平台上，stdout/stderr 的实际类型可能与
 *      标准 C 的 FILE* 不同（特别是当使用 MSVC 的调试 CRT 时）。
 *   2. 避免头文件依赖：使用 void* 可以避免在此头文件中包含 <stdio.h>，
 *      减少编译依赖和编译时间。
 *   3. 扩展性：未来可能支持非标准输出流（如自定义日志回调、文件句柄等），
 *      void* 提供了更大的灵活性。
 *
 *   典型用法：
 *     lv00_mem_print_stats(stdout);   // 输出到标准输出
 *     lv00_mem_print_stats(stderr);   // 输出到标准错误
 */
void lv00_mem_print_stats(void *stream);

/* ============== 通用内存管理函数 ============== */

#define LV00_STRDUP_AS_FUNCTION

/**
 * @brief 复制字符串（安全包装）
 *
 * 为指定字符串分配新内存并复制内容。等价于标准 C 的 strdup()，
 * 但提供跨平台一致性（MSVC 下使用 _strdup）。
 *
 * @param str 源字符串（不可为 NULL）
 * @return 新分配的字符串副本（调用者负责 free 或 lv00_free 释放），失败返回 NULL
 *
 * @note 此函数在 lv00.h 中通过宏定义为 lv00_strdup（映射到 strdup 或 _strdup）。
 *       此处提供显式的函数声明，便于不包含 lv00.h 的模块使用。
 *       调用者获得返回指针的所有权，负责在不再使用时释放。
 */
char *lv00_strdup(const char *str);

/**
 * @brief 安全内存分配（声明，定义见 lv00_utils.h）
 *
 * 自动检查返回值并设置错误码的 malloc 包装。
 *
 * @param size 分配大小（字节）
 * @return 分配的内存指针，失败返回 NULL 并设置错误码
 *
 * @note 完整声明和文档见 lv00_utils.h。
 *       调用者获得返回指针的所有权，负责使用 lv00_free() 释放。
 */
void *lv00_malloc(size_t size);

/**
 * @brief 安全内存分配并清零（声明，定义见 lv00_utils.h）
 *
 * @param nmemb 元素个数
 * @param size 每个元素大小
 * @return 分配的内存指针，失败返回 NULL 并设置错误码
 *
 * @note 完整声明和文档见 lv00_utils.h。
 */
void *lv00_calloc(size_t nmemb, size_t size);

/**
 * @brief 安全内存重新分配（声明，定义见 lv00_utils.h）
 *
 * @param ptr 原指针
 * @param size 新大小
 * @return 重新分配的内存指针，失败返回 NULL 并设置错误码
 *
 * @note 完整声明和文档见 lv00_utils.h。
 */
void *lv00_realloc(void *ptr, size_t size);

/**
 * @brief 释放内存并将指针置 NULL（声明，定义见 lv00_utils.h）
 *
 * @param ptr 指向指针的指针
 *
 * @note 完整声明和文档见 lv00_utils.h。
 */
void lv00_free(void **ptr);

/* ============== 预定义对象池 ============== */

/**
 * @brief 初始化预定义对象池
 *
 * 为常用对象类型创建全局对象池：
 *   - ConstraintNode
 *   - Constraint
 *   - SymbolicCoord
 *   - ProofStep
 *
 * @return 是否成功
 */
bool lv00_init_preset_pools(void);

/**
 * @brief 清理预定义对象池
 */
void lv00_cleanup_preset_pools(void);

/**
 * @brief 获取 ConstraintNode 对象池
 */
Lv00ObjectPool *lv00_get_node_pool(void);

/**
 * @brief 获取 Constraint 对象池
 */
Lv00ObjectPool *lv00_get_constraint_pool(void);

/**
 * @brief 获取 SymbolicCoord 对象池
 */
Lv00ObjectPool *lv00_get_symbolic_coord_pool(void);

/**
 * @brief 获取 ProofStep 对象池
 */
Lv00ObjectPool *lv00_get_proof_step_pool(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_MEMORY_POOL_H */
