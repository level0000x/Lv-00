/*
 * @file proof_navigator_utils.c
 * @brief Proof navigator module - helper functions
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

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"
#include "proof_navigator_internal.h"
#include "proof_step_registry.h"
#include "lv/lv_str_utils.h"

/** @brief proposition_type_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_proposition_type_to_string_names[] = {
    {"Atomic", PROPOSITION_TYPE_ATOMIC},
    {"Conjunction", PROPOSITION_TYPE_CONJUNCTION},
    {"Disjunction", PROPOSITION_TYPE_DISJUNCTION},
    {"Implication", PROPOSITION_TYPE_IMPLICATION},
    {"Negation", PROPOSITION_TYPE_NEGATION},
    {"Universal", PROPOSITION_TYPE_UNIVERSAL},
    {"Existential", PROPOSITION_TYPE_EXISTENTIAL},
    {"Bottom", PROPOSITION_TYPE_BOTTOM},
};

/** @brief unify_result_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_unify_result_to_string_names[] = {
    {"OK", UNIFY_STATUS_OK},
    {"Port Mismatch", UNIFY_STATUS_PORT_TYPE_MISMATCH},
    {"Constraint Mismatch", UNIFY_STATUS_CONSTRAINT_MISMATCH},
    {"Coordinate Mismatch", UNIFY_STATUS_COORD_MISMATCH},
    {"Structure Mismatch", UNIFY_STATUS_STRUCTURE_MISMATCH},
    {"Scope Mismatch", UNIFY_STATUS_SCOPE_MISMATCH},
    {"Error", UNIFY_STATUS_FAILED},
};


/* ============== 辅助函数 ============== */

/**
 * @brief 将字符串转义为安全的 HTML 文本
 *
 * 转义 <, >, &, ", ' 等 HTML 特殊字符。
 * 返回线程局部缓冲区，每次调用覆盖前一次结果。
 */
const char *html_escape(const char *s) {
    if (!s)
        return "";
    static lv_THREAD_LOCAL char buf[4096];
    lv_str_html_escape(s, strlen(s), buf, sizeof(buf));
    return buf;
}

const char *proof_color_to_string(ProofColor color) {
    return proof_color_name(color);
}

const char *proposition_type_to_string(PropositionType type) {
    return lv_enum_to_str(s_proposition_type_to_string_names, lv_ARRAY_SIZE(s_proposition_type_to_string_names), (int) type, "Unknown");
}

const char *proof_step_type_to_string(ProofStepType type) {
    /* 英文类型名统一取自证明步骤注册表（proof_step_registry） */
    const ProofStepInfo *info = proof_step_info(type);
    return info ? info->name_en : "Unknown";
}

const char *unify_result_to_string(UnifyStatus result) {
    return lv_enum_to_str(s_unify_result_to_string_names, lv_ARRAY_SIZE(s_unify_result_to_string_names), (int) result, "Unknown");
}
