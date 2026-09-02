/**
 * @file symbolic_cache.h
 * @brief 蓝图符号坐标缓存（TEN_LAYER_OPTIMIZED_PLAN §15.2 落地）
 *
 * 缓存 SymbolicCoord 计算结果：键 = operation + 输入坐标序列哈希。
 * LRU 容量淘汰；invalidate 全清 / by_node 定向失效（条目记录关联节点）。
 * 值所有权：lv_cache_insert 不拷贝 result（借用）；淘汰/失效时由缓存
 * destroy 释放（[take] 语义——insert 后 result 归缓存所有，调用方不得
 * 再释放或复用；查询命中返回内部 [borrow] 指针，勿释放）。
 */

#ifndef lv_SYMBOLIC_CACHE_H
#define lv_SYMBOLIC_CACHE_H

#include <stdbool.h>
#include <stddef.h>

#include "symbolic_coord.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 符号缓存句柄（不透明） */
typedef struct lvSymbolicCache lvSymbolicCache;

/**
 * @brief 创建符号缓存（蓝图 lv_cache_create）
 * @param capacity 容量（<=0 使用默认 64）
 * @return 缓存句柄；失败返回 NULL
 */
lv_PUBLIC_API lvSymbolicCache *lv_cache_create(int capacity);

/**
 * @brief 销毁缓存并释放全部缓存结果（蓝图 lv_cache_destroy）
 * NULL 安全。
 */
lv_PUBLIC_API void lv_cache_destroy(lvSymbolicCache *cache);

/**
 * @brief 查询缓存（蓝图 lv_cache_lookup）
 *
 * 命中返回缓存的 [borrow] 结果指针并刷新访问；未命中返回 NULL。
 * 键 = operation + inputs 序列。
 *
 * @param cache       缓存（非 NULL）
 * @param operation   运算名（非 NULL；NULL 视为 "")
 * @param inputs      输入坐标数组（可为 NULL 当 input_count 为 0）
 * @param input_count 输入数
 * @return 缓存结果（[borrow]）；未命中 NULL
 */
lv_PUBLIC_API SymbolicCoord *lv_cache_lookup(lvSymbolicCache *cache, const char *operation,
                                             const SymbolicCoord *const *inputs, int input_count);

/**
 * @brief 插入缓存（蓝图 lv_cache_insert）
 *
 * result 归缓存所有（[take]），淘汰时由缓存释放。
 *
 * @param cache       缓存（非 NULL）
 * @param operation   运算名（非 NULL）
 * @param inputs      输入坐标数组
 * @param input_count 输入数
 * @param result      计算结果（[take] 归缓存所有）
 */
lv_PUBLIC_API void lv_cache_insert(lvSymbolicCache *cache, const char *operation,
                                   const SymbolicCoord *const *inputs, int input_count, SymbolicCoord *result);

/**
 * @brief 插入缓存并关联节点（蓝图 invalidate_by_node 用）
 *
 * 同 lv_cache_insert，额外记录 node_id（-1 表示不关联）。
 *
 * @param cache    缓存
 * @param operation 运算名
 * @param inputs   输入坐标
 * @param input_count 输入数
 * @param node_id  关联节点 ID（-1 不关联）
 * @param result   计算结果（[take]）
 */
lv_PUBLIC_API void lv_cache_insert_for_node(lvSymbolicCache *cache, const char *operation,
                                            const SymbolicCoord *const *inputs, int input_count, int node_id,
                                            SymbolicCoord *result);

/**
 * @brief 全量失效（蓝图 lv_cache_invalidate）
 *
 * 清空全部条目并释放缓存结果。容量与命中计数保留。
 */
lv_PUBLIC_API void lv_cache_invalidate(lvSymbolicCache *cache);

/**
 * @brief 按节点失效（蓝图 lv_cache_invalidate_by_node）
 *
 * 释放并移除所有关联 node_id 的条目。
 */
lv_PUBLIC_API void lv_cache_invalidate_by_node(lvSymbolicCache *cache, int node_id);

/**
 * @brief 命中率（蓝图 lv_cache_hit_rate）
 *
 * @param cache 缓存（NULL 返回 0.0）
 * @return 命中率 [0.0, 1.0]；无访问记录返回 0.0
 */
lv_PUBLIC_API double lv_cache_hit_rate(const lvSymbolicCache *cache);

#ifdef __cplusplus
}
#endif

#endif /* lv_SYMBOLIC_CACHE_H */
