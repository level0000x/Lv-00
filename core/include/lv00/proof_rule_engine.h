#ifndef LV00_PROOF_RULE_ENGINE_H
#define LV00_PROOF_RULE_ENGINE_H
#include <stdbool.h>
#include "proof_rule_engine_internal.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ProofRuleConfig { int max_rules; bool strict; } ProofRuleConfig;
int lv00_proof_rule_apply(const char *rule, const void *input, void **output);
#ifdef __cplusplus
}
#endif
#endif
