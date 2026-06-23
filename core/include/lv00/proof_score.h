#ifndef LV00_PROOF_SCORE_H
#define LV00_PROOF_SCORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/lv00.h"

double lv00_proof_score_evaluate(int proof_id, void *engine);
const char *lv00_proof_score_grade(double score);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_SCORE_H */
