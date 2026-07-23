#ifndef lv_PROOF_SCORE_H
#define lv_PROOF_SCORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv.h"

double lv_proof_score_evaluate(int proof_id, void *engine);
const char *lv_proof_score_grade(double score);

#ifdef __cplusplus
}
#endif

#endif /* lv_PROOF_SCORE_H */
