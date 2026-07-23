#ifndef lv_GRAPH_HASH_H
#define lv_GRAPH_HASH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "lv/constraint_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 图哈希结构体 */
typedef struct GraphHash {
    uint64_t hash;             /**< 整体哈希值 */
    int node_count;            /**< 节点数量 */
    uint64_t *node_hashes;     /**< 各节点独立哈希数组 */
} GraphHash;

/**
 * @brief 计算约束图的完整哈希（FNV-1a 基）
 *
 * 遍历所有节点（含符号坐标序列化）和所有约束，
 * 生成与节点顺序无关的图结构指纹。
 *
 * @param graph 约束图
 * @return GraphHash* 调用者负责 graph_hash_destroy
 */
GraphHash *compute_complete_graph_hash(const ConstraintGraph *graph);

/**
 * @brief 比较两个图哈希是否相等
 *
 * 先比较整体 hash 和 node_count（快速淘汰），
 * 再逐节点比较哈希数组。
 *
 * @param a, b 图哈希指针
 * @return true 相等
 */
bool graph_hash_equal(const GraphHash *a, const GraphHash *b);

/**
 * @brief 销毁图哈希
 */
void graph_hash_destroy(GraphHash *hash);

#ifdef __cplusplus
}
#endif

#endif /* lv_GRAPH_HASH_H */
