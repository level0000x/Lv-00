/*
 * @file proof_navigator_equivalence.c
 * @brief Proof navigator module - proposition equivalence
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
#include "lv/engine.h"
#include "lv/proof.h"
#include "lv/proof_trace.h"
#include "lv/smt_backend.h"
#include "lv/trust_color.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "proof_navigator_internal.h"

/* ============== 命题的等价变换 ============== */

/**
 * @note 设计说明：
 * 本函数使用 ProofNavigator 实例的等价表，与 unify.c 中的全局等价表是独立存储。
 * 理想情况下应该统一到一个地方以避免数据不一致，但为保持向后兼容暂不合并。
 * 后续可以考虑让此函数委托给 unify_declare_proposition_equivalence()。
 */

void proof_declare_proposition_equivalence(ProofNavigator *nav, int prop_a_id, int prop_b_id) {
    if (!nav)
        return;

    /* 检查是否已存在相同的等价声明 */
    for (int i = 0; i < nav->equivalences.count; i++) {
        PropositionEquivalence *eq = (PropositionEquivalence *)lv_darray_get(&nav->equivalences, i);
        if ((eq->prop_a_id == prop_a_id && eq->prop_b_id == prop_b_id) ||
            (eq->prop_a_id == prop_b_id && eq->prop_b_id == prop_a_id)) {
            return; /* 已存在，不重复添加 */
        }
    }

    /* 添加等价声明（lv_darray_push 自动扩容） */
    PropositionEquivalence eq;
    eq.prop_a_id = prop_a_id;
    eq.prop_b_id = prop_b_id;
    eq.transformation = NULL; /* 变换规则可后续设置 */
    lv_darray_push(&nav->equivalences, &eq);

    /* 流式事件：等价声明 */
    nav_emit(proof_stream_ctx, STREAM_EVENT_INFO, "命题等价声明: prop_%d <-> prop_%d", prop_a_id, prop_b_id);
}

int proof_find_equivalent_proposition(const ProofNavigator *nav, int prop_id, int *equivalent_ids, int max_count) {
    if (!nav || !equivalent_ids || max_count <= 0)
        return 0;

    int found = 0;
    for (int i = 0; i < nav->equivalences.count && found < max_count; i++) {
        const PropositionEquivalence *eq = (const PropositionEquivalence *)lv_darray_get(&nav->equivalences, i);
        if (eq->prop_a_id == prop_id) {
            equivalent_ids[found++] = eq->prop_b_id;
        } else if (eq->prop_b_id == prop_id) {
            equivalent_ids[found++] = eq->prop_a_id;
        }
    }

    return found;
}
