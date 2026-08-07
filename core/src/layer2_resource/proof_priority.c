#include "lv/proof_priority.h"

#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/lv_utils.h"
#include "lv/lv_heap.h"
#include "lv/lv_internal.h"

/* ==================================================================
 * 收敛说明：原实现自写最大堆（sift_up/sift_down + 双数组 node_ids/scores
 * 扩容），现复用 lv_heap.c 的泛型二叉堆（lv_MAX_HEAP + 外置比较器）。
 * 元素为 {int node_id; double score} 对（8 字节，lv_heap 栈交换无动态
 * 分配），比较器按 score 比较，语义与原最大堆完全一致：
 *   - 原 sift_up 以 scores[idx] > scores[parent] 上浮 → lv_MAX_HEAP 的
 *     higher_than（cmp > 0）等价；
 *   - 原 ensure_capacity 双数组扩容 → lv_heap_push 内置几何扩容。
 * ================================================================== */

/** @brief 优先级队列元素：节点 ID + 分数 */
typedef struct {
    int node_id;
    double score;
} PriorityEntry;

/** @brief 按 score 比较（lv_MAX_HEAP 模式下 score 大者优先） */
static int compare_by_score(const void *a, const void *b) {
    double sa = ((const PriorityEntry *) a)->score;
    double sb = ((const PriorityEntry *) b)->score;
    if (sa < sb)
        return -1;
    if (sa > sb)
        return 1;
    return 0;
}

struct lvProofPriority {
    lvHeap heap; /**< 泛型最大堆（元素为 PriorityEntry） */
};

/** @brief 创建证明优先级队列（最大堆） */
lvProofPriority *lv_proof_priority_create(int capacity) {
    if (capacity <= 0)
        capacity = 64;
    lvProofPriority *pq = (lvProofPriority *) lv_malloc(sizeof(lvProofPriority));
    if (!pq)
        return NULL;
    if (!lv_heap_init(&pq->heap, sizeof(PriorityEntry), lv_MAX_HEAP, compare_by_score, (size_t) capacity)) {
        lv_free((void **) &pq);
        return NULL;
    }
    return pq;
}

/** @brief 销毁证明优先级队列 */
void lv_proof_priority_destroy(lvProofPriority *pq) {
    if (!pq)
        return;
    lv_heap_destroy(&pq->heap);
    lv_free((void **) &pq);
}

/** @brief 向优先级队列中压入一个证明节点（最大堆插入） */
int lv_proof_priority_push(lvProofPriority *pq, int node_id, double score) {
    if (!pq)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_proof_priority_push: pq is NULL");

    PriorityEntry entry;
    entry.node_id = node_id;
    entry.score = score;
    if (!lv_heap_push(&pq->heap, &entry))
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lv_proof_priority_push: heap push failed");
    return 0;
}

/** @brief 从优先级队列中弹出最高优先级的节点（最大堆提取） */
int lv_proof_priority_pop(lvProofPriority *pq, int *node_id, double *score) {
    if (!pq || lv_heap_empty(&pq->heap))
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_proof_priority_pop: pq is NULL or empty");
    if (!node_id || !score)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_proof_priority_pop: node_id or score is NULL");

    PriorityEntry entry;
    lv_heap_pop(&pq->heap, &entry);
    *node_id = entry.node_id;
    *score = entry.score;
    return 0;
}

/** @brief 检查优先级队列是否为空 */
int lv_proof_priority_empty(const lvProofPriority *pq) {
    if (!pq)
        return 1;
    return lv_heap_empty(&pq->heap) ? 1 : 0;
}
