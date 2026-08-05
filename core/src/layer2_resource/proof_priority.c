#include "lv/proof_priority.h"

#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/lv_utils.h"
#include "lv/lv_hashtable.h"
#include "lv/lv_heap.h"
#include "lv/lv_internal.h"

struct lvProofPriority {
    int capacity;
    int count;
    int *node_ids;
    double *scores;
};

/** @brief 创建证明优先级队列（最大堆） */
lvProofPriority *lv_proof_priority_create(int capacity) {
    if (capacity <= 0)
        capacity = 64;
    lvProofPriority *pq = (lvProofPriority *) lv_malloc(sizeof(lvProofPriority));
    if (!pq)
        return NULL;
    pq->node_ids = (int *) lv_malloc((size_t) capacity * sizeof(int));
    pq->scores = (double *) lv_malloc((size_t) capacity * sizeof(double));
    if (!pq->node_ids || !pq->scores) {
        lv_free((void **) &pq->node_ids);
        lv_free((void **) &pq->scores);
        lv_free((void **) &pq);
        return NULL;
    }
    pq->capacity = capacity;
    pq->count = 0;
    return pq;
}

/** @brief 销毁证明优先级队列 */
void lv_proof_priority_destroy(lvProofPriority *pq) {
    if (!pq)
        return;
    lv_free((void **) &pq->node_ids);
    lv_free((void **) &pq->scores);
    lv_free((void **) &pq);
}

/* ---- 最大堆辅助函数 ---- */

static void swap_entries(lvProofPriority *pq, int i, int j) {
    int tmp_id = pq->node_ids[i];
    pq->node_ids[i] = pq->node_ids[j];
    pq->node_ids[j] = tmp_id;

    double tmp_sc = pq->scores[i];
    pq->scores[i] = pq->scores[j];
    pq->scores[j] = tmp_sc;
}

static void sift_up(lvProofPriority *pq, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (pq->scores[idx] <= pq->scores[parent])
            break;
        swap_entries(pq, idx, parent);
        idx = parent;
    }
}

static void sift_down(lvProofPriority *pq, int idx) {
    int n = pq->count;
    while (1) {
        int largest = idx;
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;
        if (left < n && pq->scores[left] > pq->scores[largest])
            largest = left;
        if (right < n && pq->scores[right] > pq->scores[largest])
            largest = right;
        if (largest == idx)
            break;
        swap_entries(pq, idx, largest);
        idx = largest;
    }
}

static int ensure_capacity(lvProofPriority *pq) {
    if (pq->count < pq->capacity)
        return 0;

    int old_cap = pq->capacity;
    /* 第一次：扩容 node_ids（溢出检查由 lv_ensure_capacity 内部完成） */
    if (!lv_ensure_capacity((void **) &pq->node_ids, old_cap,
                            &pq->capacity, sizeof(int), 1))
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "ensure_capacity: lv_ensure_capacity failed for node_ids");

    /* 第二次：扩容 scores。临时回退容量指针使扩容真实执行，保持双数组容量一致 */
    pq->capacity = old_cap;
    if (!lv_ensure_capacity((void **) &pq->scores, old_cap,
                            &pq->capacity, sizeof(double), 1))
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "ensure_capacity: lv_ensure_capacity failed for scores");
    return 0;
}

/** @brief 向优先级队列中压入一个证明节点（最大堆插入） */
int lv_proof_priority_push(lvProofPriority *pq, int node_id, double score) {
    if (!pq)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_proof_priority_push: pq is NULL");
    if (ensure_capacity(pq) != 0)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lv_proof_priority_push: ensure_capacity failed");

    int pos = pq->count++;
    pq->node_ids[pos] = node_id;
    pq->scores[pos] = score;
    sift_up(pq, pos);
    return 0;
}

/** @brief 从优先级队列中弹出最高优先级的节点（最大堆提取） */
int lv_proof_priority_pop(lvProofPriority *pq, int *node_id, double *score) {
    if (!pq || pq->count == 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_proof_priority_pop: pq is NULL or empty");
    if (!node_id || !score)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_proof_priority_pop: node_id or score is NULL");

    *node_id = pq->node_ids[0];
    *score = pq->scores[0];

    pq->count--;
    if (pq->count > 0) {
        pq->node_ids[0] = pq->node_ids[pq->count];
        pq->scores[0] = pq->scores[pq->count];
        sift_down(pq, 0);
    }
    return 0;
}

/** @brief 检查优先级队列是否为空 */
int lv_proof_priority_empty(const lvProofPriority *pq) {
    if (!pq)
        return 1;
    return pq->count == 0 ? 1 : 0;
}
