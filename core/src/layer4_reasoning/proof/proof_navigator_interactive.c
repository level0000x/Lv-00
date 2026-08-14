/*
 * @file proof_navigator_interactive.c
 * @brief Proof navigator module - interactive proof steps
 * @details Split from proof_navigator.c
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_platform.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"
#include "lv/axiom_pkg.h"
#include "lv/constraint_graph.h"
#include "lv/proof.h"
#include "lv/proof_step_strategy.h"
#include "lv/proof_trace.h"
#include "lv/smt_backend.h"
#include "lv/trust_color.h"

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream.h"
#include "proof_navigator_internal.h"

/* ============== 交互式证明步骤 ============== */

/**
 * 交互式证明步骤数据结构
 * 根据 step_type 不同，step_data 指向不同的数据：
 * - PROOF_STEP_ADD_NODE: 指向 int (node_id)
 * - PROOF_STEP_ADD_CONSTRAINT: 指向 int (constraint_id)
 * - PROOF_STEP_REWRITE: 指向 ProofStep (包含 rule_id)
 * - PROOF_STEP_FUNCTION_APP: 指向 ProofStep (包含 func_block_id)
 * - PROOF_STEP_PACK_FUNCTION: 指向 ProofStep (包含 func_block_id)
 * - PROOF_STEP_NORMALIZATION: NULL
 * - PROOF_STEP_UNIFY: NULL
 * - PROOF_STEP_EX_FALSO: NULL
 * - PROOF_STEP_ORACLE: NULL
 */
bool proof_interactive_step(ProofNavigator *nav, ProofStepType step_type, const void *step_data) {
    if (!nav)
        return false;

    /* 验证 step_type 是否在有效范围内 */
    if (step_type < PROOF_STEP_ADD_NODE || step_type > PROOF_STEP_ORACLE) {
        return false;
    }

    /* 创建证明步骤 */
    ProofStep *step = proof_step_create(step_type);
    if (!step)
        return false;

    /* 根据步骤类型验证并填充步骤数据 */
    const ProofStepStrategy *strategy = proof_step_get_strategy(step_type);
    if (!strategy || !strategy->validate) {
        proof_step_destroy(step);
        return false;
    }
    if (!strategy->validate(step, step_data)) {
        proof_step_destroy(step);
        return false;
    }

    /* 如果当前步骤有前驱步骤，自动添加依赖 */
    if (nav->current_step >= 0 && nav->current_step < nav->step_count) {
        proof_step_add_dependency(step, nav->current_step);
    }

    /* 将步骤添加到导航器 */
    if (!proof_navigator_add_step(nav, step)) {
        proof_step_destroy(step);
        return false;
    }

    /* 标记步骤为已完成 */
    step->is_completed = true;

    return true;
}
