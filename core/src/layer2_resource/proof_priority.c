#include "lv00/lv00.h"
#include "lv00/proof_priority.h"
#include <stdlib.h>

struct Lv00ProofPriority {
    int capacity;
    int count;
};

static Lv00ProofPriority *lv00_proof_priority_create(int capacity)
{
    Lv00ProofPriority *pq = (Lv00ProofPriority *)malloc(sizeof(Lv00ProofPriority));
    if (!pq) return NULL;
    pq->capacity = capacity > 0 ? capacity : 64;
    pq->count = 0;
    return pq;
}

static void lv00_proof_priority_destroy(Lv00ProofPriority *pq)
{
    free(pq);
}

static int lv00_proof_priority_push(Lv00ProofPriority *pq, int node_id, double score)
{
    (void)node_id; (void)score;
    if (!pq) return -1;
    return 0;
}

static int lv00_proof_priority_pop(Lv00ProofPriority *pq, int *node_id, double *score)
{
    (void)node_id; (void)score;
    if (!pq) return -1;
    return -1;
}

static int lv00_proof_priority_empty(const Lv00ProofPriority *pq)
{
    if (!pq) return 1;
    return pq->count == 0 ? 1 : 0;
}
