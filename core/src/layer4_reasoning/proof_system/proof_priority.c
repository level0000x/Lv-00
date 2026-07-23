/**
 * @file proof_priority.c
 * @brief 证明优先级管理模块（子目录版本）
 *
 * 实现基于最大堆的证明搜索优先级队列。
 * 每个条目包含节点ID和对应的优先级分数，
 * 分数越高越优先被探索。
 *
 * 用于证明搜索中的节点排序，确保高价值路径优先探索。
 */

#include "lv/proof_priority.h"

#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  内部数据结构
 * ================================================================ */

/**
 * @brief 优先级队列条目
 */
typedef struct {
    int    node_id;   /**< 节点ID */
    double score;     /**< 优先级分数（越高越优先） */
} PQEntry;

/**
 * @brief 证明优先级队列结构体
 *
 * 使用数组实现的最大堆。
 */
struct lvProofPriority {
    PQEntry *entries;   /**< 堆数组 */
    int      capacity;  /**< 数组容量 */
    int      count;     /**< 当前元素数量 */
};

/* ================================================================
 *  内部辅助函数（堆操作）
 * ================================================================ */

/**
 * @brief 交换两个堆条目
 */
static void pq_swap(PQEntry *a, PQEntry *b)
{
    PQEntry tmp = *a;
    *a = *b;
    *b = tmp;
}

/**
 * @brief 上浮操作（插入后维护堆性质）
 */
static void pq_sift_up(lvProofPriority *pq, int idx)
{
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (pq->entries[idx].score > pq->entries[parent].score) {
            pq_swap(&pq->entries[idx], &pq->entries[parent]);
            idx = parent;
        } else {
            break;
        }
    }
}

/**
 * @brief 下沉操作（删除后维护堆性质）
 */
static void pq_sift_down(lvProofPriority *pq, int idx)
{
    while (1) {
        int largest = idx;
        int left  = 2 * idx + 1;
        int right = 2 * idx + 2;

        if (left < pq->count && pq->entries[left].score > pq->entries[largest].score) {
            largest = left;
        }
        if (right < pq->count && pq->entries[right].score > pq->entries[largest].score) {
            largest = right;
        }
        if (largest != idx) {
            pq_swap(&pq->entries[idx], &pq->entries[largest]);
            idx = largest;
        } else {
            break;
        }
    }
}

/**
 * @brief 确保堆数组容量足够
 * @return 0 成功，-1 内存分配失败
 */
static int pq_ensure_capacity(lvProofPriority *pq)
{
    int new_cap;
    PQEntry *new_entries;

    if (pq->count < pq->capacity) return 0;

    new_cap = pq->capacity > 0 ? pq->capacity * 2 : 64;
    new_entries = (PQEntry *)lv_realloc(pq->entries, (size_t)new_cap * sizeof(PQEntry));
    if (!new_entries) return -1;

    pq->entries = new_entries;
    pq->capacity = new_cap;
    return 0;
}

/* ================================================================
 *  公共 API 实现
 * ================================================================ */

/**
 * @brief 创建证明优先级队列
 *
 * @param capacity 初始容量（<=0 使用默认值 64）
 * @return 新分配的优先级队列，失败返回 NULL
 */
lvProofPriority *lv_proof_priority_create(int capacity)
{
    lvProofPriority *pq;

    pq = (lvProofPriority *)calloc(1, sizeof(lvProofPriority));
    if (!pq) return NULL;

    if (capacity <= 0) capacity = 64;
    pq->entries = (PQEntry *)calloc((size_t)capacity, sizeof(PQEntry));
    if (!pq->entries) {
        free(pq);
        return NULL;
    }

    pq->capacity = capacity;
    pq->count = 0;
    return pq;
}

/**
 * @brief 销毁证明优先级队列
 *
 * @param pq 优先级队列（可为 NULL）
 */
void lv_proof_priority_destroy(lvProofPriority *pq)
{
    if (!pq) return;
    free(pq->entries);
    free(pq);
}

/**
 * @brief 向优先级队列推入一个条目
 *
 * @param pq      优先级队列
 * @param node_id 节点ID
 * @param score   优先级分数
 * @return 0 成功，-1 失败
 */
int lv_proof_priority_push(lvProofPriority *pq, int node_id, double score)
{
    int idx;

    if (!pq) return -1;
    if (pq_ensure_capacity(pq) != 0) return -1;

    idx = pq->count;
    pq->entries[idx].node_id = node_id;
    pq->entries[idx].score   = score;
    pq->count++;

    pq_sift_up(pq, idx);
    return 0;
}

/**
 * @brief 从优先级队列弹出最高优先级条目
 *
 * @param pq      优先级队列
 * @param node_id [out] 弹出的节点ID
 * @param score   [out] 弹出的分数
 * @return 0 成功，-1 队列为空或参数错误
 */
int lv_proof_priority_pop(lvProofPriority *pq, int *node_id, double *score)
{
    if (!pq || pq->count <= 0) return -1;
    if (!node_id || !score) return -1;

    /* 取堆顶 */
    *node_id = pq->entries[0].node_id;
    *score   = pq->entries[0].score;

    /* 将最后一个元素移到堆顶 */
    pq->count--;
    if (pq->count > 0) {
        pq->entries[0] = pq->entries[pq->count];
        pq_sift_down(pq, 0);
    }

    return 0;
}

/**
 * @brief 检查优先级队列是否为空
 *
 * @param pq 优先级队列
 * @return 1 为空，0 非空，参数错误返回 1
 */
int lv_proof_priority_empty(const lvProofPriority *pq)
{
    if (!pq) return 1;
    return pq->count == 0 ? 1 : 0;
}
