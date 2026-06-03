/**
 * @file reasoning_cache.h
 * @brief 推理结果缓存 —— 基于哈希的去重中间结果缓存
 *
 * @details 提供基于开放寻址哈希表（线性探测）的推理结果缓存，
 *          用于避免在证明搜索过程中重复计算相同的推理步骤。
 *
 *          缓存键为 uint64_t 哈希值，通常由命题内容和已应用规则列表
 *          组合计算得出。缓存值为 int 类型的推理结果。
 *
 *          设计特点：
 *          - 开放寻址 + 线性探测，缓存友好
 *          - 固定容量，达到上限后新条目替换最旧条目（近似 LRU）
 *          - 线程不安全（调用者需自行同步）
 *          - 内置命中/未命中统计，用于性能调优
 *
 * @author Lv-00 Project
 * @version 3.5.0
 */

#ifndef LV00_REASONING_CACHE_H
#define LV00_REASONING_CACHE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 推理结果缓存（不透明类型）
 *
 * 内部使用开放寻址哈希表，线性探测解决冲突。
 * 通过 lv00_reasoning_cache_create() 创建，lv00_reasoning_cache_destroy() 销毁。
 */
typedef struct Lv00ReasoningCache Lv00ReasoningCache;

/* ============== 生命周期管理 ============== */

/**
 * @brief 创建推理结果缓存
 *
 * @param capacity 缓存容量（槽位数），0 表示使用默认值 4096。
 *                 实际内部容量会取大于等于 capacity 的最小 2 的幂。
 * @return 新分配的缓存实例，失败返回 NULL
 */
Lv00ReasoningCache *lv00_reasoning_cache_create(size_t capacity);

/**
 * @brief 销毁推理结果缓存并释放所有资源
 * @param cache 缓存指针（可为 NULL，此时无操作）
 */
void lv00_reasoning_cache_destroy(Lv00ReasoningCache *cache);

/* ============== 缓存操作 ============== */

/**
 * @brief 检查缓存中是否存在指定键的结果
 *
 * @param cache 缓存
 * @param key   哈希键（通常由命题 + 已应用规则列表计算得出）
 * @return true 键存在，false 键不存在或参数无效
 */
bool lv00_reasoning_cache_has(Lv00ReasoningCache *cache, uint64_t key);

/**
 * @brief 将结果存入缓存
 *
 * 如果键已存在，则更新其值。
 * 如果缓存已满，则替换最早插入的条目。
 *
 * @param cache  缓存
 * @param key    哈希键
 * @param result 推理结果值
 */
void lv00_reasoning_cache_put(Lv00ReasoningCache *cache, uint64_t key, int result);

/**
 * @brief 从缓存中获取结果
 *
 * @param cache 缓存
 * @param key   哈希键
 * @return 缓存的结果值，键不存在时返回 0
 */
int lv00_reasoning_cache_get(Lv00ReasoningCache *cache, uint64_t key);

/**
 * @brief 清空缓存中的所有条目
 *
 * 重置缓存状态但保留已分配的内存，后续可继续使用。
 * 同时重置命中/未命中统计计数器。
 *
 * @param cache 缓存
 */
void lv00_reasoning_cache_clear(Lv00ReasoningCache *cache);

/* ============== 统计信息 ============== */

/**
 * @brief 获取缓存统计信息
 *
 * @param cache  缓存
 * @param hits   输出：缓存命中次数（可为 NULL）
 * @param misses 输出：缓存未命中次数（可为 NULL）
 * @param size   输出：当前缓存中的条目数量（可为 NULL）
 */
void lv00_reasoning_cache_get_stats(const Lv00ReasoningCache *cache,
                                     size_t *hits, size_t *misses, size_t *size);

#ifdef __cplusplus
}
#endif

#endif /* LV00_REASONING_CACHE_H */
