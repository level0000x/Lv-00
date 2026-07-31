#ifndef lv_PROOF_RULE_ENGINE_H
#define lv_PROOF_RULE_ENGINE_H
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct ProofRuleConfig {
    int max_rules;
    bool strict;
} ProofRuleConfig;
int lv_proof_rule_apply(const char *rule, const void *input, void **output);
#ifdef __cplusplus
}
#endif
#endif
