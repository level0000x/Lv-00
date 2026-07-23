#include "lv/proof_priority.h"

#include <stdlib.h>

#include "lv/lv.h"
#include "lv/lv_utils.h"

struct lvProofPriority {
    int capacity;
    int count;
};

/** @brief 创建证明优先级队列 */
lvProofPriority *lv_proof_priority_create(int capacity) {
    lvProofPriority *pq = (lvProofPriority *) lv_malloc(sizeof(lvProofPriority));
    if (!pq)
        return NULL;
    pq->capacity = capacity > 0 ? capacity : 64;
    pq->count = 0;
    return pq;
}

/** @brief 销毁证明优先级队列 */
void lv_proof_priority_destroy(lvProofPriority *pq) {
    lv_free((void **) &pq);
}

/** @brief 向优先级队列中压入一个证明节点 */
int lv_proof_priority_push(lvProofPriority *pq, int node_id, double score) {
    (void) node_id;
    (void) score;
    if (!pq)
        return -1;
    return 0;
}

/** @brief 从优先级队列中弹出最高优先级的节点 */
int lv_proof_priority_pop(lvProofPriority *pq, int *node_id, double *score) {
    (void) node_id;
    (void) score;
    if (!pq)
        return -1;
    return -1;
}

/** @brief 检查优先级队列是否为空 */
int lv_proof_priority_empty(const lvProofPriority *pq) {
    if (!pq)
        return 1;
    return pq->count == 0 ? 1 : 0;
}
