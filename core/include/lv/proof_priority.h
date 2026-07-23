#ifndef lv_PROOF_PRIORITY_H
#define lv_PROOF_PRIORITY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv.h"

typedef struct lvProofPriority lvProofPriority;

lvProofPriority *lv_proof_priority_create(int capacity);
void lv_proof_priority_destroy(lvProofPriority *pq);
int lv_proof_priority_push(lvProofPriority *pq, int node_id, double score);
int lv_proof_priority_pop(lvProofPriority *pq, int *node_id, double *score);
int lv_proof_priority_empty(const lvProofPriority *pq);

#ifdef __cplusplus
}
#endif

#endif /* lv_PROOF_PRIORITY_H */
