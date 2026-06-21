#ifndef LV00_PROOF_TRACE_H
#define LV00_PROOF_TRACE_H
/* TODO: Proof trace module stub */
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ProofTrace { int step_count; bool complete; } ProofTrace;
ProofTrace *lv00_proof_trace_create(void);
int lv00_proof_trace_add_step(ProofTrace *t, const char *rule, const void *state);
#ifdef __cplusplus
}
#endif
#endif
