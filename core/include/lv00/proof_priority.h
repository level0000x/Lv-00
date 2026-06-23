#ifndef LV00_PROOF_PRIORITY_H
#define LV00_PROOF_PRIORITY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/lv00.h"

typedef struct Lv00ProofPriority Lv00ProofPriority;

Lv00ProofPriority *lv00_proof_priority_create(int capacity);
void lv00_proof_priority_destroy(Lv00ProofPriority *pq);
int lv00_proof_priority_push(Lv00ProofPriority *pq, int node_id, double score);
int lv00_proof_priority_pop(Lv00ProofPriority *pq, int *node_id, double *score);
int lv00_proof_priority_empty(const Lv00ProofPriority *pq);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_PRIORITY_H */
