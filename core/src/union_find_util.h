/**
 * @file union_find_util.h
 * @brief 并查集（union-find）工具：static inline 实现，供多模块共享
 *
 * 消除 geo_topology.c / normalization.c 等文件中重复的并查集实现。
 * 裸数组模式：parent[i] 为 i 的父节点，rank[i] 为集合秩。
 */
#ifndef lv_UNION_FIND_UTIL_H
#define lv_UNION_FIND_UTIL_H

#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/**
 * @brief 创建并查集（parent/rank 数组）
 * @param n        元素数量
 * @param out_rank 输出：rank 数组（与 parent 配对，须由 uf_destroy 释放）
 * @return parent 数组（parent[i] = i，失败返回 NULL）
 */
static inline int *uf_create(int n, int **out_rank) {
    int *parent = lv_calloc((size_t) n, sizeof(int));
    int *rank = lv_calloc((size_t) n, sizeof(int));
    if (!parent || !rank) {
        lv_free((void **) &parent);
        lv_free((void **) &rank);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "uf_create: calloc failed for %d elements", n);
    }
    for (int i = 0; i < n; i++)
        parent[i] = i;
    *out_rank = rank;
    return parent;
}

/**
 * @brief 释放由 uf_create 创建的 parent 和 rank 数组
 */
static inline void uf_destroy(int *parent, int *rank) {
    lv_free((void **) &parent);
    lv_free((void **) &rank);
}

/**
 * @brief 查找 x 所在集合的根（路径压缩）
 */
static inline int uf_find(int *parent, int x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]]; /* Path compression */
        x = parent[x];
    }
    return x;
}

/**
 * @brief 合并 x、y 所在集合（按秩合并）
 */
static inline void uf_union(int *parent, int *rank, int x, int y) {
    int rx = uf_find(parent, x);
    int ry = uf_find(parent, y);
    if (rx == ry)
        return;
    if (rank[rx] < rank[ry]) {
        parent[rx] = ry;
    } else if (rank[rx] > rank[ry]) {
        parent[ry] = rx;
    } else {
        parent[ry] = rx;
        rank[rx]++;
    }
}

#endif /* lv_UNION_FIND_UTIL_H */
