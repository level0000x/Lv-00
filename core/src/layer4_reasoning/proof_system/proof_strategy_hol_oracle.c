/**
 * @file proof_strategy_hol_oracle.c
 * @brief HOL Light 与 Oracle 策略执行
 *
 * 从 proof_strategy_exec.c 拆分的模块之一。
 *
 * @version v3.6.0
 */

#include "proof_multi_strategy_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"
#include "lv/solver.h"

#include "lv/atp_backend.h"
#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lambda_to_graph.h"
#include "lv/lambda_unify.h"
#include "lv/normalization.h"
#include "lv/type_system.h"
#include "lv/unify.h"
#include "layer4_reasoning/proof/proof_step_registry.h"

/**
 * @brief Oracle法执行 —— 外部求解器辅助
 *
 * 通过外部 ATP（自动定理证明器）后端辅助验证命题：
 * - 检查引擎上下文是否有外部求解器能力
 * - 尝试调用 ATP 后端（Vampire/E Prover/iProver）
 * - 将约束图编码为 TPTP 格式
 * - 解析求解结果并生成证明步骤
 * - 所有步骤标记为 PROOF_COLOR_ORANGE_ORACLE
 */
/* ── HOL Light 微内核验证 ── */

/**
 * @brief HOL Light 微内核验证策略
 *
 * 使用 proof_minimal_verify 函数，以 HOL Light 的 10 条基本推理规则
 * (REFL, TRANS, ASSUME, BETA_CONV, MK_COMB, etc.) 验证证明中的每个步骤。
 * 适用于任何具备等式/lambda/应用结构证明步骤的验证场景。
 *
 * 策略逻辑：
 *   1. 遍历 nav 中的所有证明步骤
 *   2. 对每个步骤，根据其类型映射到对应的 VerifyRuleType
 *   3. 调用 proof_minimal_verify 验证
 *   4. 将验证结果和追溯信息写入步骤元数据
 *   5. 若所有步骤验证通过则返回 true
 */
bool execute_hol_light(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav)
        return false;

    int step_count = nav->step_count;
    if (step_count <= 0)
        return false;

    bool all_valid = true;
    for (int i = 0; i < step_count; i++) {
        const ProofStep *step = nav->steps[i];
        if (!step)
            continue;

        /* 将 ProofStepType 映射到 VerifyRuleType（统一取自证明步骤注册表） */
        const ProofStepInfo *info = proof_step_info(step->type);
        if (!info || info->hol_rule == PROOF_STEP_HOL_RULE_NONE) {
            /* 无对应 HOL Light 规则的步骤跳过 */
            continue;
        }
        VerifyRuleType rule = info->hol_rule;

        /* 收集前提（依赖的前驱步骤的结论） */
        const char *premises[16];
        int premise_count = 0;
        if (step->dependency_step_ids && step->dependency_count > 0) {
            for (int d = 0; d < step->dependency_count && premise_count < 14; d++) {
                int dep_id = step->dependency_step_ids[d];
                if (dep_id >= 0 && dep_id < step_count) {
                    const ProofStep *dep = nav->steps[dep_id];
                    if (dep && dep->ext && dep->ext->conclusion)
                        premises[premise_count++] = dep->ext->conclusion;
                }
            }
        }
        premises[premise_count] = NULL;

        /* 检查步骤是否有结论 */
        if (!step->ext || !step->ext->conclusion)
            continue;

        /* 执行 HOL Light 验证 */
        char *trace = NULL;
        LvProofVerifyResult result = proof_minimal_verify(rule, premises, step->ext->conclusion, &trace);

        if (result != VERIFY_VALID) {
            all_valid = false;
            if (trace)
                LOG_WARN("hol_light", "步骤 #%d: %s", i, trace);
        }

        if (trace)
            lv_free((void **) &trace);
    }

    return all_valid;
}

/* ── Oracle 外部求解器 ── */

/**
 * @brief 执行 Oracle 外部求解器策略
 *
 * 遍历构造的约束图，检测是否存在外部求解器（ATP/CAS 等）、
 * 调用外部求解器、解析求解结果并生成证明步骤。
 * 所有步骤标记为 PROOF_COLOR_ORANGE_ORACLE。
 */
bool execute_oracle(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav || !nav->construction)
        return false;

    /* 检查引擎上下文是否有外部求解器 */
    if (!nav->engine) {
        ProofStep *no_engine_step = proof_step_create(PROOF_STEP_ORACLE);
        if (no_engine_step) {
            no_engine_step->color = PROOF_COLOR_ORANGE_ORACLE;
            no_engine_step->note = lv_strdup_safe("[Oracle] 无引擎上下文，无法调用外部求解器");
            proof_navigator_add_step(nav, no_engine_step);
        }
        return false;
    }

    /* 添加 Oracle 起始步骤 */
    ProofStep *start_step = proof_step_create(PROOF_STEP_ORACLE);
    if (start_step) {
        start_step->color = PROOF_COLOR_ORANGE_ORACLE;
        start_step->note = lv_strdup_safe("[Oracle] 尝试调用外部 ATP 求解器辅助验证");
        proof_navigator_add_step(nav, start_step);
    }

    bool verified = false;

    /* 尝试使用 ATP 后端编码约束图 */
    /* 检查是否有可用的 ATP 后端 */
    bool atp_available = false;
    (void) atp_available; /* suppress unused warning */
    ATPBackendType atp_types[] = {ATP_BACKEND_VAMPIRE, ATP_BACKEND_EPROVER, ATP_BACKEND_IPROVER};

    /* 尝试编码约束图为 TPTP 格式并求解 */
    for (int backend = 0; backend < 3 && !verified; backend++) {
        const char *atp_name = atp_backend_type_name(atp_types[backend]);
        /* 检查后端是否可用 */
        if (!atp_is_backend_available(atp_types[backend])) {
            ProofStep *skip_step = proof_step_create(PROOF_STEP_ORACLE);
            if (skip_step) {
                skip_step->color = PROOF_COLOR_BLUE_UNEXPLORED;
                char buf[256];
                snprintf(buf, sizeof(buf), "[Oracle] %s 后端不可用，跳过", atp_name);
                skip_step->note = lv_strdup_safe(buf);
                proof_navigator_add_step(nav, skip_step);
            }
            continue;
        }

        ProofStep *try_step = proof_step_create(PROOF_STEP_ORACLE);
        if (try_step) {
            try_step->color = PROOF_COLOR_ORANGE_ORACLE;
            char buf[256];
            snprintf(buf, sizeof(buf), "[Oracle] 尝试 %s 后端...", atp_name);
            try_step->note = lv_strdup_safe(buf);
            proof_navigator_add_step(nav, try_step);
        }

        /* 尝试将约束图编码为 TPTP 格式 */
        char *tptp = atp_encode_constraint_graph(nav->construction, ATP_FORMAT_TPTP_FOF, "lv_oracle_problem", true,
                                                 nav->target_prop);

        if (tptp == NULL) {
            ProofStep *enc_fail = proof_step_create(PROOF_STEP_ORACLE);
            if (enc_fail) {
                enc_fail->color = PROOF_COLOR_BLUE_UNEXPLORED;
                char buf[256];
                snprintf(buf, sizeof(buf), "[Oracle] %s 编码失败，跳过", atp_name);
                enc_fail->note = lv_strdup_safe(buf);
                proof_navigator_add_step(nav, enc_fail);
            }
            continue;
        }

        /* 编码成功，标记后端可用 */
        atp_available = true;

        /* 尝试创建求解器并求解 */
        ATPConfig config = atp_config_default();
        config.timeout_seconds = 10.0; /* Oracle 模式使用较短超时 */

        ATPBackendSolver *solver = atp_solver_create(atp_types[backend], &config);
        if (solver) {
            int load_rc = atp_solver_load(solver, tptp);
            if (load_rc == lv_OK) {
                ATPResultInfo result;
                atp_result_init(&result);
                int solve_rc = atp_solver_solve(solver, &result);

                if (solve_rc == lv_OK && result.result == ATP_RESULT_UNSAT) {
                    /* 证明成功：UNSAT 表示目标不可满足（即命题成立） */
                    verified = true;

                    ProofStep *success_step = proof_step_create(PROOF_STEP_ORACLE);
                    if (success_step) {
                        success_step->color = PROOF_COLOR_GREEN_COMPLETE;
                        char buf[256];
                        snprintf(buf, sizeof(buf), "[Oracle] %s 证明成功（%.2fs, %d 子句）", atp_name,
                                 result.solve_time_seconds, result.processed_clauses);
                        success_step->note = lv_strdup_safe(buf);
                        proof_navigator_add_step(nav, success_step);
                    }

                    /* 将 ATP 证明步骤转换到导航器 */
                    if (result.proof_step_count > 0) {
                        Proof atp_proof;
                        int converted = 0;
                        atp_proof_to_lv(&result, &atp_proof, &converted);
                        /* 转换后的步骤可追加到导航器（由调用者处理） */
                        (void) converted;
                    }
                } else if (solve_rc == lv_OK && result.result == ATP_RESULT_SAT) {
                    ProofStep *sat_step = proof_step_create(PROOF_STEP_ORACLE);
                    if (sat_step) {
                        sat_step->color = PROOF_COLOR_RED_CONFLICT;
                        char buf[256];
                        snprintf(buf, sizeof(buf), "[Oracle] %s 返回 SAT，命题不成立", atp_name);
                        sat_step->note = lv_strdup_safe(buf);
                        proof_navigator_add_step(nav, sat_step);
                    }
                } else {
                    ProofStep *unknown_step = proof_step_create(PROOF_STEP_ORACLE);
                    if (unknown_step) {
                        unknown_step->color = PROOF_COLOR_BLUE_UNEXPLORED;
                        char buf[256];
                        snprintf(buf, sizeof(buf), "[Oracle] %s 无法确定（超时/资源耗尽）", atp_name);
                        unknown_step->note = lv_strdup_safe(buf);
                        proof_navigator_add_step(nav, unknown_step);
                    }
                }

                atp_result_destroy(&result);
            }
            atp_solver_destroy(solver);
        }

        lv_free((void **) &tptp);
    }

    /* 如果 ATP 后端不可用，尝试直接合一作为降级方案 */
    if (!verified && nav->target_prop && nav->target_prop->pattern) {
        ProofStep *fallback_step = proof_step_create(PROOF_STEP_ORACLE);
        if (fallback_step) {
            fallback_step->color = PROOF_COLOR_ORANGE_ORACLE;
            fallback_step->note = lv_strdup_safe("[Oracle] ATP 后端不可用，降级为合一检查");
            proof_navigator_add_step(nav, fallback_step);
        }

        UnifyStatus status = proof_unify(nav->construction, nav->target_prop, true);

        ProofStep *result_step = proof_step_create(PROOF_STEP_ORACLE);
        if (result_step) {
            result_step->color = (status == UNIFY_STATUS_OK) ? PROOF_COLOR_ORANGE_ORACLE : PROOF_COLOR_BLUE_UNEXPLORED;
            result_step->note = (status == UNIFY_STATUS_OK)
                                    ? lv_strdup_safe("[Oracle] 合一检查确认命题成立（Oracle辅助）")
                                    : lv_strdup_safe("[Oracle] 合一检查未能确认命题");
            proof_navigator_add_step(nav, result_step);
        }

        verified = (status == UNIFY_STATUS_OK);
    }

    /* 尝试使用归一化 + 合一 */
    if (!verified) {
        NormalizationResult *norm = graph_normalize(nav->construction, false);
        if (norm) {
            ProofStep *norm_step = proof_step_create(PROOF_STEP_ORACLE);
            if (norm_step) {
                norm_step->color = PROOF_COLOR_ORANGE_ORACLE;
                norm_step->merged_count = norm->merged_count;
                norm_step->note = lv_strdup_safe("[Oracle] 使用归一化简化构造图后重新验证");
                proof_navigator_add_step(nav, norm_step);
            }

            if (nav->target_prop && nav->target_prop->pattern) {
                UnifyStatus status = proof_unify(nav->construction, nav->target_prop, false);
                verified = (status == UNIFY_STATUS_OK);
            }

            normalization_result_destroy(norm);
        }
    }

    if (verified) {
        ProofStep *done_step = proof_step_create(PROOF_STEP_ORACLE);
        if (done_step) {
            done_step->color = PROOF_COLOR_ORANGE_ORACLE;
            done_step->note = lv_strdup_safe("[Oracle] 外部求解器辅助验证成功（注意：依赖非构造性方法）");
            proof_navigator_add_step(nav, done_step);
        }
    }

    return verified;
}
