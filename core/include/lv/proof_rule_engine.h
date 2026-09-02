#ifndef lv_PROOF_RULE_ENGINE_H
#define lv_PROOF_RULE_ENGINE_H
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* lvProofRule 前向声明（完整定义见 proof_rule_engine_internal.h） */
typedef struct lvProofRule lvProofRule;

typedef struct ProofRuleConfig {
    int max_rules;
    bool strict;
} ProofRuleConfig;
lv_PUBLIC_API int lv_proof_rule_apply(const char *rule, const void *input, void **output);

/* 数值验证规则：对「仅含实数常量的比较表达式」目标（如 "1+2=3"、"sin(0.5)>0.4"）
 * 做区间算术求值 + FPTaylor 误差界分级（TrustColor）验证。
 * 能力来源：PROOF_STRATEGY_NUMERIC_VERIFICATION（proof_strategy_numeric.c）。 */
#define lv_PROOF_RULE_NAME_NUMERIC_VERIFICATION "numeric_verification"

/* 创建数值验证规则实例（堆分配）。
 * 调用方可经 rule_engine_add_rule 加入规则引擎（lvRuleEngine 销毁时自动释放）；
 * 也可直接经 lv_proof_rule_apply(lv_PROOF_RULE_NAME_NUMERIC_VERIFICATION, ...) 使用。 */
lvProofRule *lv_proof_rule_numeric_verification_create(void);

#ifdef __cplusplus
}
#endif
#endif
