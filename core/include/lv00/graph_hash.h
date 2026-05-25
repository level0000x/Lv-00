/**
 * @file graph_hash.h
 * @brief 图结构哈希 —— 基于 FNV-1a 的约束图结构指纹
 *
 * @details 提供约束图的紧凑、与节点顺序无关的结构哈希，
 *          用于快速比较图的拓扑等价性以及重写循环检测。
 *
 *          所有权规则：
 *          - compute_complete_graph_hash 返回新分配的 GraphHash*，调用者须调用 graph_hash_destroy() 释放
 *          - graph_hash_destroy 释放 GraphHash 及其内部的 node_hashes 数组
 *          - graph_hash_equal 只读，不转移所有权
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef LV00_GRAPH_HASH_H
#define LV00_GRAPH_HASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

#include "constraint_graph.h"

/* ================================================================
 *  GraphHash 结构体 —— 约束图的 FNV-1a 结构指纹
 * ================================================================ */

typedef struct GraphHash {
    uint64_t hash;         /**< 整个图的聚合 FNV-1a 哈希值 */
    uint64_t *node_hashes; /**< 每个节点的哈希值（已分配数组，共 node_count 个元素） */
    int node_count;        /**< 图中节点的数量 */
} GraphHash;

/**
 * @brief 计算约束图的完整结构哈希
 *
 * @param[in] graph 要计算哈希的约束图
 *
 * @return 新分配的 GraphHash 指针，调用者须通过 graph_hash_destroy() 释放。
 *         若 graph 为 NULL，返回 NULL。
 */
LV00_PUBLIC_API GraphHash *compute_complete_graph_hash(const ConstraintGraph *graph);

/**
 * @brief 比较两个图哈希是否相等
 *
 * @param[in] a 第一个图哈希指针
 * @param[in] b 第二个图哈希指针
 *
 * @return 两个哈希结构相同时返回 true，否则返回 false
 */
LV00_PUBLIC_API bool graph_hash_equal(const GraphHash *a, const GraphHash *b);

/**
 * @brief 销毁 GraphHash 并释放其内部资源
 *
 * @param[in] hash 要销毁的 GraphHash 指针，可为 NULL（无操作）
 */
LV00_PUBLIC_API void graph_hash_destroy(GraphHash *hash);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GRAPH_HASH_H */
