/**
 * @file proof_step_registry.h
 * @brief 证明步骤类型（ProofStepType）统一注册表
 *
 * 收敛 ProofStepType 在多个模块间的分散分发：
 *   - proof_navigator_utils.c  英文类型名（proof_step_type_to_string）
 *   - proof_dependency.c       中/英文动词与"为什么"解释文案
 *   - proof_strategy_hol_oracle.c  HOL Light 验证规则映射
 *
 * @author Lv-00 Project
 */

#ifndef lv_PROOF_STEP_REGISTRY_H
#define lv_PROOF_STEP_REGISTRY_H

#include "lv/proof.h" /* ProofStepType / VerifyRuleType */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 无 HOL 映射时 hol_rule 的哨兵值（区别于任何合法 VerifyRuleType） */
#define PROOF_STEP_HOL_RULE_NONE ((VerifyRuleType) - 1)

/**
 * @brief 证明步骤类型注册表条目
 */
typedef struct {
    ProofStepType type;     /**< 步骤类型枚举值 */
    const char *name_en;    /**< 英文类型名（proof_step_type_to_string 语义） */
    const char *verb_zh;    /**< 中文动词（自然语言输出用） */
    const char *verb_en;    /**< 英文动词（自然语言输出用） */
    const char *why_zh;     /**< 中文"为什么"解释（无文案为 ""） */
    const char *why_en;     /**< 英文"为什么"解释（无文案为 ""） */
    VerifyRuleType hol_rule; /**< HOL Light 验证规则（无映射为 PROOF_STEP_HOL_RULE_NONE） */
} ProofStepInfo;

/**
 * @brief 查询证明步骤类型注册表条目
 *
 * @param type 步骤类型枚举值
 * @return 指向静态注册表条目的指针；未知（越界）类型返回 NULL，
 *         由调用方按各自场景回退默认文案
 */
const ProofStepInfo *proof_step_info(ProofStepType type);

#ifdef __cplusplus
}
#endif

#endif /* lv_PROOF_STEP_REGISTRY_H */
