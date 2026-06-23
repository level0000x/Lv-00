#include "lv00/lv00.h"
#include "lv00/proof_score.h"

double lv00_proof_score_evaluate(int proof_id, void *engine)
{
    (void)proof_id; (void)engine;
    return 0.0;
}

const char *lv00_proof_score_grade(double score)
{
    (void)score;
    return "unknown";
}
