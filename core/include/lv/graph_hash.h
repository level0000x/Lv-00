#ifndef lv_GRAPH_HASH_H
#define lv_GRAPH_HASH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 图哈希结构体 */
typedef struct GraphHash {
    uint64_t hash;             /**< 整体哈希值 */
    int node_count;            /**< 节点数量（用于完整图哈希） */
    uint64_t *node_hashes;     /**< 各节点独立哈希数组（用于完整图哈希比较） */
} GraphHash;

#ifdef __cplusplus
}
#endif

#endif /* lv_GRAPH_HASH_H */
