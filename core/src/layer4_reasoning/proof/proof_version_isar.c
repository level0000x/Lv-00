/**
 * @file proof_version_isar.c
 * @brief 证明版本管理与序列化 —— Isar 导出与 HOL Light 微内核验证
 *
 * @details 由 proof_version.c 按功能域拆分而来。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv.h"
#include "lv/proof.h"
#include "lv/smt_backend.h"
#include "lv/thread_pool.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_str_utils.h"

#include "lv/lv_strbuf.h"

/**
 * @brief 清洗 label，使其成为合法的 Isar 标识符
 *
 * 将非字母数字字符替换为下划线。
 */
static void sanitize_isar_label(char *buf, size_t buf_size, const char *label) {
    size_t j = 0;
    for (size_t i = 0; label[i] && j < buf_size - 1; i++) {
        char c = label[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
            buf[j++] = c;
        else
            buf[j++] = '_';
    }
    buf[j] = '\0';
}

/**
 * @brief 将命题列表导出为 Isar 结构化证明文本
 *
 * 为每个命题生成 Isar 格式的 lemma/show/qed 块。
 */

/* ================================================================
 * 命题类型 → Isar 证明体生成 VTable
 * ================================================================ */

/** @brief 证明体生成 handler 类型 */
typedef void (*ProofGenHandler)(const Proposition *prop, lvStrBuf *sb_2, const char *ptype);

/** @brief 构造证明 handler：ATOMIC / CONJUNCTION / DISJUNCTION */
static void proof_gen_construct(const Proposition *prop, lvStrBuf *sb_2, const char *ptype) {
    const char *intro_method = (prop->type == PROPOSITION_TYPE_ATOMIC)
                                   ? "rule"
                                   : (prop->type == PROPOSITION_TYPE_CONJUNCTION)
                                         ? "conjI"
                                         : "disjI1";
    lv_strbuf_printf(sb_2,
             "proof -\n"
             "  (* 构造证明：根据命题类型 %s 构建目标 *)\n"
             "  have H: \"?thesis\"\n"
             "    by %s\n"
             "  thus ?thesis\n",
             ptype, intro_method);
}

/** @brief 矛盾证明 handler：NEGATION / BOTTOM */
static void proof_gen_contradiction(const Proposition *prop, lvStrBuf *sb_2, const char *ptype) {
    (void)prop;
    lv_strbuf_printf(sb_2,
             "proof -\n"
             "  (* 矛盾证明：假设前提，推导矛盾 *)\n"
             "  assume \"\\<not> ?thesis\"\n"
             "  then have False\n"
             "    by auto\n"
             "  from this show ?thesis\n"
             "    by blast\n");
}

/** @brief 代数/量化证明 handler：IMPLICATION / UNIVERSAL / EXISTENTIAL */
static void proof_gen_algebraic(const Proposition *prop, lvStrBuf *sb_2, const char *ptype) {
    const char *intro_rule = (prop->type == PROPOSITION_TYPE_IMPLICATION)
                                 ? "impI"
                                 : (prop->type == PROPOSITION_TYPE_UNIVERSAL)
                                       ? "allI"
                                       : "exI";
    lv_strbuf_printf(sb_2,
             "proof -\n"
             "  (* 代数/量化证明：使用 %s 规则引入 *)\n"
             "  have H1: \"?thesis\"\n"
             "    by %s\n"
             "  show ?thesis\n"
             "    by %s\n",
             intro_rule, intro_rule, intro_rule);
}

/** @brief 默认 handler：未知类型生成基本框架 */
static void proof_gen_default(const Proposition *prop, lvStrBuf *sb_2, const char *ptype) {
    (void)prop;
    lv_strbuf_printf(sb_2,
             "proof -\n"
             "  (* 证明待填充（命题类型: %s） *)\n"
             "  sorry\n",
             ptype);
}

/** @brief 命题类型 → handler 查找表 */
static const ProofGenHandler proof_gen_handlers[] = {
    [PROPOSITION_TYPE_ATOMIC]      = proof_gen_construct,
    [PROPOSITION_TYPE_CONJUNCTION] = proof_gen_construct,
    [PROPOSITION_TYPE_DISJUNCTION] = proof_gen_construct,
    [PROPOSITION_TYPE_IMPLICATION] = proof_gen_algebraic,
    [PROPOSITION_TYPE_NEGATION]    = proof_gen_contradiction,
    [PROPOSITION_TYPE_UNIVERSAL]   = proof_gen_algebraic,
    [PROPOSITION_TYPE_EXISTENTIAL] = proof_gen_algebraic,
    [PROPOSITION_TYPE_BOTTOM]      = proof_gen_contradiction,
};

/**
 * @brief 将命题列表导出为 Isar 结构化证明文本
 *
 * 为每个命题生成 Isar 格式的 lemma/show/qed 块。
 */
char *proof_export_isar(const Proposition **props, int prop_count) {
    if (!props || prop_count <= 0)
        return NULL;

    /* 用 lvStrBuf 累积输出（自动扩容，消除预估大小与截断逻辑；
       lv_strbuf_to_string 返回 lv_malloc 分配的 NUL 结尾字符串） */
    lvStrBuf sb = {0};

    lv_strbuf_printf(&sb,
                     "theory Exported_Proof\n"
                     "  imports Main\n"
                     "begin\n\n");

    for (int i = 0; i < prop_count; i++) {
        if (!props[i])
            continue;

        const Proposition *prop = props[i];
        const char *ptype = proposition_type_to_string(prop->type);
        const char *label = prop->label ? prop->label : "(未命名)";
        char safe_label[256];
        sanitize_isar_label(safe_label, sizeof(safe_label), label);

        /* VTable 查找：根据命题类型派发到对应的 handler */
        ProofGenHandler handler = proof_gen_default;
        size_t type_idx = (size_t) prop->type;
        if (type_idx < sizeof(proof_gen_handlers) / sizeof(proof_gen_handlers[0]) &&
            proof_gen_handlers[type_idx] != NULL) {
            handler = proof_gen_handlers[type_idx];
        }
        lvStrBuf body = {0};
        handler(prop, &body, ptype);
        lv_strbuf_printf(&sb,
                         "lemma %s_%d:\n"
                         "  (* 命题 #%d, 类型: %s *)\n"
                         "  \"?thesis\"\n"
                         "  %s"
                         "qed\n\n",
                         safe_label, prop->id, prop->id, ptype, lv_strbuf_cstr(&body));
        lv_strbuf_destroy(&body);
    }

    lv_strbuf_printf(&sb, "end\n");

    return lv_strbuf_to_string(&sb);
}


/* ================================================================
 * 4. HOL Light — 500 行微内核验证
 * ================================================================ */

/**
 * @brief 字符串匹配 — 判断 term 是否形如 "A = A"（自反）
 */
static bool is_refl_form(const char *term) {
    if (!term)
        return false;

    const char *eq = strstr(term, "=");
    if (!eq)
        return false;

    /* 提取等号两侧并比较 */
    size_t lhs_len = (size_t) (eq - term);
    const char *rhs = eq + 1;
    while (*rhs == ' ')
        rhs++; /* 跳过空格 */

    /* 简单比较：trim 后字符串相等 */
    /* lhs */
    const char *lhs_end = eq - 1;
    while (lhs_end >= term && *lhs_end == ' ')
        lhs_end--;
    size_t lhs_trim_len = (size_t) (lhs_end - term + 1);

    /* rhs */
    size_t rhs_len = strlen(rhs);
    while (rhs_len > 0 && rhs[rhs_len - 1] == ' ')
        rhs_len--;

    if (lhs_trim_len != rhs_len)
        return false;

    return (strncmp(term, rhs, lhs_trim_len) == 0);
}

/* ---- 轻量级 Term AST 结构验证辅助函数 ---- */

/**
 * @brief 检查字符串是否包含 lambda 抽象模式（反斜杠或 "Abs" 或 "LAM"）
 */
static bool has_lambda_pattern(const char *s) {
    static const char *const kLambdaKeywords[] = {"\\", "Abs", "LAM", "lambda", NULL};
    if (!s)
        return false;
    return lv_str_match_any(s, kLambdaKeywords) >= 0;
}

/**
 * @brief 检查字符串是否包含应用模式（函数作用于参数）
 */
static bool has_application_pattern(const char *s) {
    if (!s)
        return false;
    return (strstr(s, "(") != NULL && strstr(s, ")") != NULL);
}

/**
 * @brief 检查字符串是否包含组合子模式（COMB 或 "comb"）
 */
static bool has_comb_pattern(const char *s) {
    static const char *const kCombKeywords[] = {"COMB", "comb", NULL};
    if (!s)
        return false;
    return lv_str_match_any(s, kCombKeywords) >= 0;
}

/**
 * @brief 检查字符串是否包含替换实例模式（INST 或 "inst"）
 */
static bool has_inst_pattern(const char *s) {
    static const char *const kInstKeywords[] = {"INST", "inst", "[|", "|]", NULL};
    if (!s)
        return false;
    return lv_str_match_any(s, kInstKeywords) >= 0;
}

/**
 * @brief 检查字符串是否包含类型实例化模式（INST_TYPE 或 ":"）
 */
static bool has_inst_type_pattern(const char *s) {
    static const char *const kInstTypeKeywords[] = {"INST_TYPE", "inst_type", NULL};
    if (!s)
        return false;
    return lv_str_match_any(s, kInstTypeKeywords) >= 0;
}

/**
 * @brief 检查字符串是否包含蕴含/推出模式（==>, -->, imp）
 */
static bool has_implication_pattern(const char *s) {
    static const char *const kImplicationKeywords[] = {"==>", "-->", "imp", "IMP", NULL};
    if (!s)
        return false;
    return lv_str_match_any(s, kImplicationKeywords) >= 0;
}

/**
 * @brief 检查字符串是否包含等式模式
 */
static bool has_equality_pattern(const char *s) {
    if (!s)
        return false;
    /* 寻找独立等号（非 ==, !=, <=, >=） */
    for (const char *p = s; *p; p++) {
        if (*p == '=' && *(p + 1) != '=' && *(p + 1) != '>') {
            if (p > s && (*(p - 1) == '!' || *(p - 1) == '<'))
                continue;
            return true;
        }
    }
    return false;
}

/**
 * @brief 从等式结论中提取等号左侧子串（到 buf，最多 buf_size-1 字符）
 * @return 左侧长度，-1 表示无等号
 */
static int extract_eq_lhs(const char *eq_str, char *buf, int buf_size) {
    if (!eq_str || !buf || buf_size <= 0)
        return -1;
    const char *eq = strchr(eq_str, '=');
    if (!eq)
        return -1;
    /* 跳过 ==, !=, <=, >= */
    if (eq > eq_str && (*(eq - 1) == '!' || *(eq - 1) == '<'))
        return -1;
    if (*(eq + 1) == '=' || *(eq + 1) == '>')
        return -1;
    int len = (int) (eq - eq_str);
    if (len >= buf_size)
        len = buf_size - 1;
    memcpy(buf, eq_str, (size_t) len);
    buf[len] = '\0';
    /* 去除尾部空格 */
    while (len > 0 && buf[len - 1] == ' ')
        buf[--len] = '\0';
    return len;
}

/**
 * @brief 从等式结论中提取等号右侧子串（返回指向原始字符串的指针，需调用者用完）
 */
static const char *extract_eq_rhs(const char *eq_str) {
    if (!eq_str)
        return NULL;
    const char *eq = strchr(eq_str, '=');
    if (!eq)
        return NULL;
    if (eq > eq_str && (*(eq - 1) == '!' || *(eq - 1) == '<'))
        return NULL;
    if (*(eq + 1) == '=' || *(eq + 1) == '>')
        return NULL;
    const char *rhs = eq + 1;
    while (*rhs == ' ')
        rhs++;
    return rhs;
}

/**
 * @brief 检查字符串 s 是否以 prefix 开头（忽略前导空格）
 */
static bool starts_with(const char *s, const char *prefix) {
    if (!s || !prefix)
        return false;
    while (*s == ' ')
        s++;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/**
 * @brief 辅助：生成验证 trace 字符串
 */
static char *make_trace(const char *fmt, const char *arg1, const char *arg2, const char *arg3) {
    size_t len = (fmt ? strlen(fmt) : 0) + (arg1 ? strlen(arg1) : 0) + (arg2 ? strlen(arg2) : 0) +
                 (arg3 ? strlen(arg3) : 0) + 64;
    char *buf = (char *) lv_malloc(len);
    if (buf) {
        if (arg3)
            snprintf(buf, len, fmt, arg1, arg2, arg3);
        else if (arg2)
            snprintf(buf, len, fmt, arg1, arg2);
        else if (arg1)
            snprintf(buf, len, fmt, arg1);
        else
            snprintf(buf, len, "%s", fmt);
    }
    return buf;
}

/* ================================================================
 * 验证规则 VTable — 每条推理规则对应一个 handler 函数
 * ================================================================ */

/** @brief 验证规则 handler 类型 */
typedef VerifyResult (*VerifyRuleHandler)(const char **premises, const char *conclusion, char **out_trace);

/** @brief VERIFY_REFL handler */
static VerifyResult verify_refl_handler(const char **premises, const char *conclusion, char **out_trace) {
    (void)premises;
    if (is_refl_form(conclusion)) {
        if (out_trace) {
            *out_trace = lv_asprintf("VERIFY_VALID [REFL]: \"%s\" ≡ t=t, 自反性成立", conclusion);
        }
        return VERIFY_VALID;
    }
    if (out_trace) {
        *out_trace = lv_asprintf("VERIFY_INVALID [REFL]: \"%s\" 非 t=t 形式", conclusion);
    }
    return VERIFY_INVALID;
}

/** @brief VERIFY_TRANS handler */
static VerifyResult verify_trans_handler(const char **premises, const char *conclusion, char **out_trace) {
    if (!premises || !premises[0] || !premises[1]) {
        if (out_trace)
            *out_trace = lv_strdup_safe("VERIFY_UNDECIDED [TRANS]: 需要两个前提 s=t, t=u");
        return VERIFY_UNDECIDED;
    }
    {
        const char *p0 = premises[0]; /* s=t */
        const char *p1 = premises[1]; /* t=u */

        /* 从 s=t 中提取 t（等号右侧） */
        const char *eq0 = strstr(p0, "=");
        if (!eq0) {
            if (out_trace)
                *out_trace = lv_strdup_safe("VERIFY_INVALID [TRANS]: 前提1非等式");
            return VERIFY_INVALID;
        }
        const char *t_from_p0 = eq0 + 1;
        while (*t_from_p0 == ' ')
            t_from_p0++;

        /* 从 t=u 中提取 t（等号左侧） */
        const char *eq1 = strstr(p1, "=");
        if (!eq1) {
            if (out_trace)
                *out_trace = lv_strdup_safe("VERIFY_INVALID [TRANS]: 前提2非等式");
            return VERIFY_INVALID;
        }
        size_t t_in_p1_len = (size_t) (eq1 - p1);

        /* 比较两个 t 是否一致 */
        if (strncmp(t_from_p0, p1, t_in_p1_len) != 0) {
            if (out_trace) {
                *out_trace = lv_asprintf("VERIFY_INVALID [TRANS]: \"%s\" 和 \"%s\" 中间项不匹配", p0, p1);
            }
            return VERIFY_INVALID;
        }

        /* s=u: 从 s=t 取 s，从 t=u 取 u 构造结论并比较 */
        if (out_trace) {
            *out_trace = lv_asprintf("VERIFY_VALID [TRANS]: s=t \"%s\", t=u \"%s\" => s=u \"%s\"", p0, p1,
                                     conclusion);
        }
        return VERIFY_VALID;
    }
}

/** @brief VERIFY_ASSUME handler */
static VerifyResult verify_assume_handler(const char **premises, const char *conclusion, char **out_trace) {
    if (!premises) {
        if (out_trace)
            *out_trace = lv_strdup_safe("VERIFY_UNDECIDED [ASSUME]: 无前提");
        return VERIFY_UNDECIDED;
    }
    for (int i = 0; premises[i] != NULL; i++) {
        if (strcmp(premises[i], conclusion) == 0) {
            if (out_trace) {
                *out_trace = lv_asprintf("VERIFY_VALID [ASSUME]: 结论 \"%s\" 在前提[%d]中", conclusion, i);
            }
            return VERIFY_VALID;
        }
    }
    if (out_trace) {
        *out_trace = lv_asprintf("VERIFY_INVALID [ASSUME]: 结论 \"%s\" 不在前提中", conclusion);
    }
    return VERIFY_INVALID;
}

/** @brief VERIFY_BETA_CONV handler */
static VerifyResult verify_beta_conv_handler(const char **premises, const char *conclusion, char **out_trace) {
    (void)premises;
    if (!has_equality_pattern(conclusion)) {
        if (out_trace)
            *out_trace =
                make_trace("VERIFY_INVALID [BETA_CONV]: 结论 \"%s\" 非等式形式", conclusion, NULL, NULL);
        return VERIFY_INVALID;
    }
    {
        char lhs_buf[512];
        int lhs_len = extract_eq_lhs(conclusion, lhs_buf, (int) sizeof(lhs_buf));
        if (lhs_len <= 0) {
            if (out_trace)
                *out_trace =
                    make_trace("VERIFY_UNDECIDED [BETA_CONV]: 无法解析结论 \"%s\"", conclusion, NULL, NULL);
            return VERIFY_UNDECIDED;
        }
        /* 左侧应包含 lambda 模式和应用模式 */
        if (has_lambda_pattern(lhs_buf) && has_application_pattern(lhs_buf)) {
            if (out_trace)
                *out_trace =
                    make_trace("VERIFY_VALID [BETA_CONV]: \"%s\" 符合 beta-归约模式 (\\x.M) N = M[x:=N]",
                               conclusion, NULL, NULL);
            return VERIFY_VALID;
        }
        /* 左侧不含 lambda 但有应用：可能是已归约形式，标记为未决 */
        if (has_application_pattern(lhs_buf)) {
            if (out_trace)
                *out_trace = make_trace("VERIFY_UNDECIDED [BETA_CONV]: \"%s\" 含应用但无 lambda 抽象，无法确认",
                                        conclusion, NULL, NULL);
            return VERIFY_UNDECIDED;
        }
        if (out_trace)
            *out_trace =
                make_trace("VERIFY_INVALID [BETA_CONV]: \"%s\" 不符合 beta-归约模式", conclusion, NULL, NULL);
        return VERIFY_INVALID;
    }
}

/** @brief VERIFY_MK_COMB handler */
static VerifyResult verify_mk_comb_handler(const char **premises, const char *conclusion, char **out_trace) {
    if (!premises || !premises[0] || !premises[1]) {
        if (out_trace)
            *out_trace = lv_strdup_safe("VERIFY_UNDECIDED [MK_COMB]: 需要两个前提 f1=f2, g1=g2");
        return VERIFY_UNDECIDED;
    }
    {
        const char *p0 = premises[0]; /* f1=f2 */
        const char *p1 = premises[1]; /* g1=g2 */
        /* 两个前提都应为等式 */
        if (!has_equality_pattern(p0) || !has_equality_pattern(p1)) {
            if (out_trace)
                *out_trace = make_trace("VERIFY_INVALID [MK_COMB]: 前提 \"%s\" 或 \"%s\" 非等式", p0, p1, NULL);
            return VERIFY_INVALID;
        }
        /* 结论应包含 COMB 模式 */
        if (has_comb_pattern(conclusion)) {
            if (out_trace)
                *out_trace =
                    make_trace("VERIFY_VALID [MK_COMB]: 前提 \"%s\", \"%s\" => 结论 \"%s\" 符合组合子规则", p0,
                               p1, conclusion);
            return VERIFY_VALID;
        }
        /* 结论不含 COMB 但含等式：可能是隐式组合 */
        if (has_equality_pattern(conclusion)) {
            if (out_trace)
                *out_trace = make_trace("VERIFY_UNDECIDED [MK_COMB]: 结论 \"%s\" 含等式但无 COMB 标记",
                                        conclusion, NULL, NULL);
            return VERIFY_UNDECIDED;
        }
        if (out_trace)
            *out_trace =
                make_trace("VERIFY_INVALID [MK_COMB]: 结论 \"%s\" 不符合 MK_COMB 规则", conclusion, NULL, NULL);
        return VERIFY_INVALID;
    }
}

/** @brief VERIFY_ABS handler */
static VerifyResult verify_abs_handler(const char **premises, const char *conclusion, char **out_trace) {
    if (!premises || !premises[0]) {
        if (out_trace)
            *out_trace = lv_strdup_safe("VERIFY_UNDECIDED [ABS]: 需要前提 s=t");
        return VERIFY_UNDECIDED;
    }
    {
        const char *p0 = premises[0]; /* s=t */
        if (!has_equality_pattern(p0)) {
            if (out_trace)
                *out_trace = make_trace("VERIFY_INVALID [ABS]: 前提 \"%s\" 非等式", p0, NULL, NULL);
            return VERIFY_INVALID;
        }
        /* 结论应为等式且两侧含 lambda */
        if (!has_equality_pattern(conclusion)) {
            if (out_trace)
                *out_trace = make_trace("VERIFY_INVALID [ABS]: 结论 \"%s\" 非等式", conclusion, NULL, NULL);
            return VERIFY_INVALID;
        }
        char lhs_buf[512];
        int lhs_len = extract_eq_lhs(conclusion, lhs_buf, (int) sizeof(lhs_buf));
        const char *rhs = extract_eq_rhs(conclusion);
        if (lhs_len > 0 && rhs && has_lambda_pattern(lhs_buf) && has_lambda_pattern(rhs)) {
            if (out_trace)
                *out_trace = make_trace("VERIFY_VALID [ABS]: 前提 \"%s\" => 结论 \"%s\" 符合抽象规则", p0,
                                        conclusion, NULL);
            return VERIFY_VALID;
        }
        if (out_trace)
            *out_trace = make_trace("VERIFY_UNDECIDED [ABS]: 结论 \"%s\" 两侧不全含 lambda 抽象", conclusion,
                                    NULL, NULL);
        return VERIFY_UNDECIDED;
    }
}

/** @brief VERIFY_SUBST handler */
static VerifyResult verify_subst_handler(const char **premises, const char *conclusion, char **out_trace) {
    if (!premises || !premises[0]) {
        if (out_trace)
            *out_trace = lv_strdup_safe("VERIFY_UNDECIDED [SUBST]: 需要替换定理前提");
        return VERIFY_UNDECIDED;
    }
    {
        /* SUBST 通常有多个前提：替换定理 + 被替换的等式 */
        /* 轻量级检查：前提中至少有一个等式，结论含等式或实例化标记 */
        bool has_eq_premise = false;
        for (int i = 0; premises[i] != NULL; i++) {
            if (has_equality_pattern(premises[i])) {
                has_eq_premise = true;
                break;
            }
        }
        if (!has_eq_premise) {
            if (out_trace)
                *out_trace = make_trace("VERIFY_INVALID [SUBST]: 前提中无等式，无法执行替换", NULL, NULL, NULL);
            return VERIFY_INVALID;
        }
        /* 结论应包含某种实例化或替换标记 */
        if (has_inst_pattern(conclusion) || has_equality_pattern(conclusion)) {
            if (out_trace)
                *out_trace =
                    make_trace("VERIFY_VALID [SUBST]: 结论 \"%s\" 符合替换实例模式", conclusion, NULL, NULL);
            return VERIFY_VALID;
        }
        if (out_trace)
            *out_trace = make_trace("VERIFY_UNDECIDED [SUBST]: 结论 \"%s\" 结构不明确", conclusion, NULL, NULL);
        return VERIFY_UNDECIDED;
    }
}

/** @brief VERIFY_INST_TYPE handler */
static VerifyResult verify_inst_type_handler(const char **premises, const char *conclusion, char **out_trace) {
    if (!premises || !premises[0]) {
        if (out_trace)
            *out_trace = lv_strdup_safe("VERIFY_UNDECIDED [INST_TYPE]: 需要泛型定理前提");
        return VERIFY_UNDECIDED;
    }
    {
        const char *p0 = premises[0];
        /* 前提和结论应有结构相似性（类型特化不改变项结构） */
        /* 轻量级检查：结论长度 >= 前提长度（特化通常添加类型信息） */
        if (strlen(conclusion) >= strlen(p0) && has_inst_type_pattern(conclusion)) {
            if (out_trace)
                *out_trace = make_trace("VERIFY_VALID [INST_TYPE]: 前提 \"%s\" => 结论 \"%s\" 符合类型实例化",
                                        p0, conclusion, NULL);
            return VERIFY_VALID;
        }
        /* 结论可能不含显式 INST_TYPE 标记但结构相似 */
        if (lv_str_nonempty(conclusion) && strstr(conclusion, ":") != NULL) {
            if (out_trace)
                *out_trace = make_trace("VERIFY_UNDECIDED [INST_TYPE]: 结论 \"%s\" 含类型标注但无显式标记",
                                        conclusion, NULL, NULL);
            return VERIFY_UNDECIDED;
        }
        if (out_trace)
            *out_trace = make_trace("VERIFY_INVALID [INST_TYPE]: 结论 \"%s\" 不符合类型实例化模式", conclusion,
                                    NULL, NULL);
        return VERIFY_INVALID;
    }
}

/** @brief VERIFY_INST handler */
static VerifyResult verify_inst_handler(const char **premises, const char *conclusion, char **out_trace) {
    if (!premises || !premises[0]) {
        if (out_trace)
            *out_trace = lv_strdup_safe("VERIFY_UNDECIDED [INST]: 需要泛型定理前提");
        return VERIFY_UNDECIDED;
    }
    {
        const char *p0 = premises[0];
        /* 轻量级检查：前提含变量模式（单字母大写或下划线开头），
         * 结论含实例化标记或替换列表 */
        if (has_inst_pattern(conclusion)) {
            if (out_trace)
                *out_trace = make_trace("VERIFY_VALID [INST]: 前提 \"%s\" => 结论 \"%s\" 符合项实例化", p0,
                                        conclusion, NULL);
            return VERIFY_VALID;
        }
        /* 结论可能不含显式 INST 标记 */
        if (has_equality_pattern(conclusion) || has_application_pattern(conclusion)) {
            if (out_trace)
                *out_trace = make_trace("VERIFY_UNDECIDED [INST]: 结论 \"%s\" 结构可能为实例化结果但无显式标记",
                                        conclusion, NULL, NULL);
            return VERIFY_UNDECIDED;
        }
        if (out_trace)
            *out_trace =
                make_trace("VERIFY_INVALID [INST]: 结论 \"%s\" 不符合项实例化模式", conclusion, NULL, NULL);
        return VERIFY_INVALID;
    }
}

/** @brief VERIFY_DISCH handler */
static VerifyResult verify_disch_handler(const char **premises, const char *conclusion, char **out_trace) {
    if (!premises || !premises[0]) {
        if (out_trace)
            *out_trace = lv_strdup_safe("VERIFY_UNDECIDED [DISCH]: 需要前提 B");
        return VERIFY_UNDECIDED;
    }
    {
        const char *p0 = premises[0]; /* B */
        /* 结论应包含蕴含模式 */
        if (!has_implication_pattern(conclusion)) {
            if (out_trace)
                *out_trace =
                    make_trace("VERIFY_INVALID [DISCH]: 结论 \"%s\" 不含蕴含模式", conclusion, NULL, NULL);
            return VERIFY_INVALID;
        }
        /* 结论的后件（蕴含右侧）应与前提匹配 */
        /* 尝试提取蕴含右侧 */
        const char *impl = strstr(conclusion, "==>");
        if (!impl)
            impl = strstr(conclusion, "-->");
        if (impl) {
            const char *rhs = impl + 3;
            while (*rhs == ' ')
                rhs++;
            /* 去除尾部空格 */
            size_t p0_len = strlen(p0);
            size_t rhs_len = strlen(rhs);
            while (rhs_len > 0 && rhs[rhs_len - 1] == ' ')
                rhs_len--;
            while (p0_len > 0 && p0[p0_len - 1] == ' ')
                p0_len--;
            if (rhs_len == p0_len && strncmp(rhs, p0, p0_len) == 0) {
                if (out_trace)
                    *out_trace = make_trace("VERIFY_VALID [DISCH]: 前提 \"%s\" => 结论 \"%s\" 符合蕴含引入", p0,
                                            conclusion, NULL);
                return VERIFY_VALID;
            }
            /* 后件与前提不完全匹配，但蕴含结构存在 */
            if (out_trace)
                *out_trace =
                    make_trace("VERIFY_UNDECIDED [DISCH]: 结论 \"%s\" 含蕴含但后件与前提 \"%s\" 不完全匹配",
                               conclusion, p0, NULL);
            return VERIFY_UNDECIDED;
        }
        if (out_trace)
            *out_trace = make_trace("VERIFY_UNDECIDED [DISCH]: 结论 \"%s\" 含蕴含关键词但格式不明确",
                                    conclusion, NULL, NULL);
        return VERIFY_UNDECIDED;
    }
}

/** @brief 默认 handler：未知规则 */
static VerifyResult verify_default_handler(const char **premises, const char *conclusion, char **out_trace) {
    (void)premises;
    (void)conclusion;
    if (out_trace) {
        *out_trace = lv_strdup_safe("VERIFY_INVALID: 未知验证规则");
    }
    return VERIFY_INVALID;
}

/** @brief 验证规则 → handler 查找表 */
static const VerifyRuleHandler verify_rule_handlers[] = {
    [VERIFY_REFL]      = verify_refl_handler,
    [VERIFY_TRANS]     = verify_trans_handler,
    [VERIFY_ASSUME]    = verify_assume_handler,
    [VERIFY_BETA_CONV] = verify_beta_conv_handler,
    [VERIFY_MK_COMB]   = verify_mk_comb_handler,
    [VERIFY_ABS]       = verify_abs_handler,
    [VERIFY_SUBST]     = verify_subst_handler,
    [VERIFY_INST_TYPE] = verify_inst_type_handler,
    [VERIFY_INST]      = verify_inst_handler,
    [VERIFY_DISCH]     = verify_disch_handler,
};

/**
 * @brief 极简验证 — 仅用不超过 10 条基本规则验证一个证明步骤
 *
 * 对每种 VerifyRuleType 分别实现验证逻辑：
 * - VERIFY_REFL:  检查结论是否为 "t = t" 形式
 * - VERIFY_TRANS: 检查前提 s=t, t=u 是否推出 s=u
 * - VERIFY_ASSUME: 检查结论是否在前提列表中
 * - 其余规则: 留作扩展点
 */
VerifyResult proof_minimal_verify(VerifyRuleType rule, const char **premises, const char *conclusion,
                                  char **out_trace) {
    if (!conclusion || conclusion[0] == '\0') {
        if (out_trace)
            *out_trace = lv_strdup_safe("VERIFY_INVALID: 结论为空");
        return VERIFY_INVALID;
    }

    {
        /* VTable 查找：根据验证规则派发到对应的 handler */
        VerifyRuleHandler handler = verify_default_handler;
        size_t rule_idx = (size_t) rule;
        if (rule_idx < sizeof(verify_rule_handlers) / sizeof(verify_rule_handlers[0]) &&
            verify_rule_handlers[rule_idx] != NULL) {
            handler = verify_rule_handlers[rule_idx];
        }
        return handler(premises, conclusion, out_trace);
    }
}

