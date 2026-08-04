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

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "proof_navigator_internal.h"

/** @brief proof_color_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_proof_color_to_string_names[] = {
    {"Green", PROOF_COLOR_GREEN},
    {"Blue (unexplored)", PROOF_COLOR_BLUE_UNEXPLORED},
    {"Blue (resource)", PROOF_COLOR_BLUE_RESOURCE},
    {"Blue (out of range)", PROOF_COLOR_BLUE_OUT_OF_RANGE},
    {"Green (verified)", PROOF_COLOR_GREEN_VERIFIED},
    {"Yellow", PROOF_COLOR_YELLOW},
    {"Orange (oracle)", PROOF_COLOR_ORANGE_ORACLE},
    {"Orange (ex falso)", PROOF_COLOR_ORANGE_EX_FALSO},
    {"Amber", PROOF_COLOR_AMBER},
    {"Dark orange", PROOF_COLOR_DARK_ORANGE},
    {"Green (complete)", PROOF_COLOR_GREEN_COMPLETE},
    {"Red (conflict)", PROOF_COLOR_RED_CONFLICT},
};

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

/** @brief proof_step_type_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_proof_step_type_to_string_names[] = {
    {"Add Node", PROOF_STEP_ADD_NODE},
    {"Add Constraint", PROOF_STEP_ADD_CONSTRAINT},
    {"Rewrite", PROOF_STEP_REWRITE},
    {"Function Application", PROOF_STEP_FUNCTION_APP},
    {"Pack Function", PROOF_STEP_PACK_FUNCTION},
    {"Normalization", PROOF_STEP_NORMALIZATION},
    {"Unify", PROOF_STEP_UNIFY},
    {"Ex Falso", PROOF_STEP_EX_FALSO},
    {"Oracle", PROOF_STEP_ORACLE},
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

/** @brief JSON 字符转义查找表（按 ASCII 下标，NULL 表示无需转义） */
static const char *const s_json_escape_table[256] = {
    ['\b'] = "\\b",
    ['\t'] = "\\t",
    ['\n'] = "\\n",
    ['\f'] = "\\f",
    ['\r'] = "\\r",
    ['"'] = "\\\"",
    ['\\'] = "\\\\",
};

/** @brief HTML 字符转义查找表（按 ASCII 下标，NULL 表示无需转义） */
static const char *const s_html_escape_table[256] = {
    ['&'] = "&amp;",
    ['<'] = "&lt;",
    ['>'] = "&gt;",
    ['"'] = "&quot;",
    ['\''] = "&#39;",
};

/**
 * @brief 将字符串转义为安全的 JSON 字符串字面量
 *
 * 转义双引号、反斜杠、换行符、回车符、制表符等 JSON 特殊字符。
 * 返回线程局部缓冲区，每次调用覆盖前一次结果。
 */
static const char *json_escape(const char *s) {
    if (!s)
        return "";
    static lv_THREAD_LOCAL char buf[4096];
    size_t j = 0;
    for (size_t i = 0; s[i] && j < sizeof(buf) - 6; i++) {
        const char *esc = s_json_escape_table[(unsigned char) s[i]];
        if (esc) {
            size_t esc_len = strlen(esc);
            if (j + esc_len >= sizeof(buf))
                break;
            memcpy(buf + j, esc, esc_len);
            j += esc_len;
        } else if ((unsigned char) s[i] < 0x20) {
            j += (size_t) snprintf(buf + j, sizeof(buf) - j, "\\u%04x", (unsigned char) s[i]);
        } else {
            buf[j++] = s[i];
        }
    }
    buf[j] = '\0';
    return buf;
}

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
    size_t j = 0;
    for (size_t i = 0; s[i] && j < sizeof(buf) - 6; i++) {
        const char *esc = s_html_escape_table[(unsigned char) s[i]];
        if (esc) {
            size_t esc_len = strlen(esc);
            if (j + esc_len >= sizeof(buf))
                break;
            memcpy(buf + j, esc, esc_len);
            j += esc_len;
        } else {
            buf[j++] = s[i];
        }
    }
    buf[j] = '\0';
    return buf;
}

const char *proof_color_to_string(ProofColor color) {
    return lv_enum_to_str(s_proof_color_to_string_names, lv_ARRAY_SIZE(s_proof_color_to_string_names), (int) color, "Unknown");
}

const char *proposition_type_to_string(PropositionType type) {
    return lv_enum_to_str(s_proposition_type_to_string_names, lv_ARRAY_SIZE(s_proposition_type_to_string_names), (int) type, "Unknown");
}

const char *proof_step_type_to_string(ProofStepType type) {
    return lv_enum_to_str(s_proof_step_type_to_string_names, lv_ARRAY_SIZE(s_proof_step_type_to_string_names), (int) type, "Unknown");
}

const char *unify_result_to_string(UnifyStatus result) {
    return lv_enum_to_str(s_unify_result_to_string_names, lv_ARRAY_SIZE(s_unify_result_to_string_names), (int) result, "Unknown");
}
