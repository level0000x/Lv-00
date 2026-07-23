#include "lv/lv.h"
#include "lv/proof_priority.h"
#include <stdlib.h>

struct lvProofPriority {
    int capacity;
    int count;
};

lvProofPriority *lv_proof_priority_create(int capacity)
{
    lvProofPriority *pq = (lvProofPriority *)malloc(sizeof(lvProofPriority));
    if (!pq) return NULL;
    pq->capacity = capacity > 0 ? capacity : 64;
    pq->count = 0;
    return pq;
}

void lv_proof_priority_destroy(lvProofPriority *pq)
{
    free(pq);
}

int lv_proof_priority_push(lvProofPriority *pq, int node_id, double score)
{
    (void)node_id; (void)score;
    if (!pq) return -1;
    return 0;
}

int lv_proof_priority_pop(lvProofPriority *pq, int *node_id, double *score)
{
    (void)node_id; (void)score;
    if (!pq) return -1;
    return -1;
}

int lv_proof_priority_empty(const lvProofPriority *pq)
{
    if (!pq) return 1;
    return pq->count == 0 ? 1 : 0;
}
