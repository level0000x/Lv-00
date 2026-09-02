/**
 * @file lv_lru_cache.h
 * @brief 最近最少使用（LRU）缓存设施 —— 整数键到 void* 值的热点数据缓存
 *
 * @details
 *  数据结构：哈希表（key → 链表节点，O(1) 查找）+ 双向链表（访问顺序，
 *  MRU 在头部、LRU 在尾部），get/put 均为 O(1) 平均。
 *  键为 int；值为 void*（**不拷贝**，所有权始终归调用方）。
 *  缓存满时淘汰「最久未使用」（链表尾部）条目；淘汰仅丢弃引用，
 *  不释放 value（详见下方所有权注释）。
 *
 * @note 线程安全：thread_safe=true 时所有操作由库内部互斥锁保护；
 *       thread_safe=false 时不做任何加锁，由调用方保证串行访问。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef lv_LV_LRU_CACHE_H
#define lv_LV_LRU_CACHE_H

#include <stdbool.h>

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/** LRU 缓存句柄（不透明结构，定义见 lv_lru_cache.c） */
typedef struct lvLRUCache lvLRUCache;

/**
 * @brief 创建 LRU 缓存
 *
 * 中文：capacity<=0 时使用默认容量 64；thread_safe=true 启用内部互斥保护。
 *       返回的句柄 [take]：调用方持有，使用后必须用 lv_lru_destroy 释放。
 *
 * English: Create an LRU cache. capacity<=0 falls back to the default of 64;
 *          thread_safe=true enables an internal mutex. The returned handle is
 *          [take]: caller owns it and must release it with lv_lru_destroy.
 *
 * @param capacity    最大条目数（<=0 时使用默认 64）/ max entries (<=0 → default 64)
 * @param thread_safe 是否线程安全 / whether concurrent access is guarded
 * @return 缓存句柄；分配失败返回 NULL / cache handle, or NULL on allocation failure
 */
lv_PUBLIC_API lvLRUCache *lv_lru_create(int capacity, bool thread_safe);

/**
 * @brief 销毁 LRU 缓存
 *
 * 中文：释放全部内部节点与哈希表，**不释放任何 value**（值所有权归调用方）。
 *       NULL 安全（no-op）。
 *
 * English: Destroy the cache: frees all internal nodes and the hash table but
 *          NEVER frees stored values (value ownership belongs to the caller).
 *          NULL-safe (no-op).
 *
 * @param cache 缓存句柄（可为 NULL）/ cache handle (may be NULL)
 */
lv_PUBLIC_API void lv_lru_destroy(lvLRUCache *cache);

/**
 * @brief 插入或更新条目
 *
 * 中文：键不存在 → 插入新条目（作为最新使用）；键已存在 → 更新其 value 并
 *       提升为最新使用；缓存已满且键为新增 → 淘汰最久未使用条目后插入。
 *       存入的 value 为 [borrow]：缓存仅保存指针，不拷贝、不释放；
 *       淘汰/销毁时仅丢弃引用。
 *
 * English: Insert or update an entry. A new key is inserted as most-recently-
 *          used; an existing key has its value updated and is promoted to
 *          most-recently-used; if the cache is full and the key is new, the
 *          least-recently-used entry is evicted first. The stored value is
 *          [borrow]: the cache keeps only the pointer, never copies or frees
 *          it; eviction/destroy only drops the reference.
 *
 * @param cache 缓存句柄（非 NULL）/ cache handle (non-NULL)
 * @param key   条目键 / entry key
 * @param value 条目值（非 NULL；NULL 与「未命中」语义冲突，拒绝）/ value
 *              (non-NULL; NULL conflicts with the miss sentinel and is rejected)
 * @return true 成功；false 参数无效（cache 或 value 为 NULL）或内部失败 /
 *         true on success; false on invalid args (NULL cache/value) or failure
 */
lv_PUBLIC_API bool lv_lru_put(lvLRUCache *cache, int key, void *value);

/**
 * @brief 查询条目并刷新访问时间
 *
 * 中文：命中返回该键的 value（[borrow]：调用方不得释放，缓存可能在任何
 *       后续淘汰中丢弃该引用），并将条目提升为最新使用；未命中返回 NULL。
 *
 * English: Look up an entry and refresh its access time. On a hit, returns the
 *          stored value ([borrow]: caller must not free it — any later eviction
 *          may drop the reference) and promotes the entry to most-recently-used;
 *          on a miss, returns NULL.
 *
 * @param cache 缓存句柄（非 NULL）/ cache handle (non-NULL)
 * @param key   条目键 / entry key
 * @return 命中的 value 指针；未命中或 cache 为 NULL 返回 NULL /
 *         the value on a hit, or NULL on a miss or NULL cache
 */
lv_PUBLIC_API void *lv_lru_get(lvLRUCache *cache, int key);

/**
 * @brief 当前条目数
 *
 * 中文：返回缓存中现存条目个数（恒 <= 容量）；cache 为 NULL 返回 0。
 *
 * English: Return the current number of entries (always <= capacity);
 *          returns 0 for a NULL cache.
 *
 * @param cache 缓存句柄（可为 NULL）/ cache handle (may be NULL)
 * @return 当前条目数 / current entry count
 */
lv_PUBLIC_API int lv_lru_count(const lvLRUCache *cache);

/**
 * @brief 缓存容量（最大条目数）
 *
 * 中文：返回创建时确定的容量（capacity<=0 时为默认 64）；cache 为 NULL 返回 0。
 *
 * English: Return the capacity set at creation (default 64 when capacity<=0);
 *          returns 0 for a NULL cache.
 *
 * @param cache 缓存句柄（可为 NULL）/ cache handle (may be NULL)
 * @return 容量 / capacity
 */
lv_PUBLIC_API int lv_lru_capacity(const lvLRUCache *cache);

#ifdef __cplusplus
}
#endif

#endif /* lv_LV_LRU_CACHE_H */
