#ifndef LV00_PROOF_TRACE_H
#define LV00_PROOF_TRACE_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 不透明类型：完整定义仅在 proof_trace.c 中 */
typedef struct ProofTrace ProofTrace;
ProofTrace *lv00_proof_trace_create(void);
void lv00_proof_trace_destroy(ProofTrace *t);
int lv00_proof_trace_add_step(ProofTrace *t, const char *rule, const void *state);
/* 访问器函数 */
int lv00_proof_trace_get_step_count(const ProofTrace *t);
bool lv00_proof_trace_is_complete(const ProofTrace *t);
#ifdef __cplusplus
}
#endif
#endif
