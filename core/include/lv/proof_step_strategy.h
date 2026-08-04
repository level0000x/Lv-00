/**
 * @file proof_step_strategy.h
 * @brief 证明步骤策略模式 — 将 switch 分发替换为 vtable 策略分发
 *
 * 定义 ProofStepStrategy vtable，包含验证（validate）和 Coq 导出（export_coq）
 * 两个函数指针。通过 proof_step_get_strategy() 获取指定步骤类型的策略实例。
 */

#ifndef lv_PROOF_STEP_STRATEGY_H
#define lv_PROOF_STEP_STRATEGY_H

#include <stdbool.h>
#include <stdio.h>
#include "lv/proof.h"

/**
 * @brief 验证函数指针
 * @param step      已创建的证明步骤（由调用者填充字段）
 * @param step_data 步骤数据（类型取决于步骤类型）
 * @return true 验证通过，false 验证失败
 */
typedef bool (*ProofStepValidateFn)(ProofStep *step, const void *step_data);

/**
 * @brief Coq 导出函数指针
 * @param step  证明步骤
 * @param f     输出文件句柄
 */
typedef void (*ProofStepExportCoqFn)(const ProofStep *step, FILE *f);

/**
 * @brief 证明步骤策略 vtable
 *
 * 每个 PROOF_STEP_* 类型对应一个策略实例。
 * 未使用的回调可为 NULL（调用者需检查）。
 */
typedef struct {
    ProofStepValidateFn validate;   /**< 验证回调（可为 NULL） */
    ProofStepExportCoqFn export_coq; /**< Coq 导出回调（可为 NULL） */
} ProofStepStrategy;

/**
 * @brief 获取指定步骤类型的策略实例
 * @param type 证明步骤类型
 * @return 策略实例指针，若类型无效返回 NULL
 */
extern const ProofStepStrategy *proof_step_get_strategy(ProofStepType type);

#endif /* lv_PROOF_STEP_STRATEGY_H */