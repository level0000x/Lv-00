/**
 * @file inequality_reasoning_serialize.c
 * @brief 不等式推理系统 —— 序列化
 */

#include "inequality_reasoning_internal.h"

#include "lv/lv_strbuf.h"


/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief ineq_type_str 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_ineq_type_str_entries[] = {
    {"<", INEQ_LESS_THAN},
    {"<=", INEQ_LESS_EQUAL},
    {">", INEQ_GREATER_THAN},
    {">=", INEQ_GREATER_EQUAL},
    {"!=", INEQ_NOT_EQUAL},
};

static const char *ineq_type_str(lvInequalityType type) {
    return lv_enum_to_str(s_ineq_type_str_entries, lv_ARRAY_SIZE(s_ineq_type_str_entries), (int) type, "?");
}

/** @brief lv_ineq_proof_to_string 状态名称表（按枚举值升序） */
static const lvStrToEnumEntry s_ineq_status_str_entries[] = {
    {"PROVED", INEQ_STATUS_PROVED},
    {"DISPROVED", INEQ_STATUS_DISPROVED},
    {"CONDITIONAL", INEQ_STATUS_CONDITIONAL},
};

char *lv_ineq_to_string(const lvInequality *ineq) {
    if (!ineq) {
        char *s = (char *) lv_malloc(1);
        if (s)
            s[0] = '\0';
        return s;
    }

    size_t buf_size = 256;
    char *s = (char *) lv_malloc(buf_size);
    if (!s)
        return NULL;

    const char *label = ineq->label ? ineq->label : "ineq";
    lv_snprintf(s, buf_size, "%s: left %s right", label, ineq_type_str(ineq->type));
    return s;
}

char *lv_ineq_proof_to_string(const lvInequalityProof *proof) {
    if (!proof) {
        char *s = (char *) lv_malloc(1);
        if (s)
            s[0] = '\0';
        return s;
    }

    size_t buf_size = 512;
    char *s = (char *) lv_malloc(buf_size);
    if (!s)
        return NULL;

    const char *status_str = lv_enum_to_str(s_ineq_status_str_entries, lv_ARRAY_SIZE(s_ineq_status_str_entries),
                                            (int) proof->status, "UNKNOWN");

    lv_snprintf(s, buf_size, "Proof: %s, %d steps", status_str, proof->step_count);
    return s;
}

char *lv_ineq_proof_to_latex(const lvInequalityProof *proof) {
    if (!proof) {
        char *s = (char *) lv_malloc(1);
        if (s)
            s[0] = '\0';
        return s;
    }

    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb, "\\begin{proof}\n");
    for (int i = 0; i < proof->step_count; i++) {
        const char *just = proof->steps[i].justification ? proof->steps[i].justification : "unknown";
        lv_strbuf_printf(&sb, "  Step %d: %s\n", i + 1, just);
    }
    lv_strbuf_printf(&sb, "\\end{proof}\n");

    return lv_strbuf_to_string(&sb);
}
