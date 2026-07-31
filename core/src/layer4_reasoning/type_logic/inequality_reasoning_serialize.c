/**
 * @file inequality_reasoning_serialize.c
 * @brief 不等式推理系统 —— 序列化
 */

#include "inequality_reasoning_internal.h"


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
    snprintf(s, buf_size, "%s: left %s right", label, ineq_type_str(ineq->type));
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

    const char *status_str = "UNKNOWN";
    switch (proof->status) {
        case INEQ_STATUS_PROVED:
            status_str = "PROVED";
            break;
        case INEQ_STATUS_DISPROVED:
            status_str = "DISPROVED";
            break;
        case INEQ_STATUS_CONDITIONAL:
            status_str = "CONDITIONAL";
            break;
        default:
            break;
    }

    snprintf(s, buf_size, "Proof: %s, %d steps", status_str, proof->step_count);
    return s;
}

char *lv_ineq_proof_to_latex(const lvInequalityProof *proof) {
    if (!proof) {
        char *s = (char *) lv_malloc(1);
        if (s)
            s[0] = '\0';
        return s;
    }

    size_t buf_size = 1024;
    char *s = (char *) lv_malloc(buf_size);
    if (!s)
        return NULL;

    int offset = 0;
    if (offset < (int) buf_size)
        offset += snprintf(s + offset, buf_size - (size_t) offset, "\\begin{proof}\n");
    if (offset < 0)
        goto done;

    for (int i = 0; i < proof->step_count && offset < (int) buf_size - 64; i++) {
        const char *just = proof->steps[i].justification ? proof->steps[i].justification : "unknown";
        if (offset < (int) buf_size)
            offset += snprintf(s + offset, buf_size - (size_t) offset, "  Step %d: %s\n", i + 1, just);
        if (offset < 0)
            break;
    }

    if (offset >= 0 && offset < (int) buf_size)
        offset += snprintf(s + offset, buf_size - (size_t) offset, "\\end{proof}\n");

done:

    return s;
}
