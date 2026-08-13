/*
 * @file prop_verifier_serialize.c
 * @brief Proposition verifier module - formula serialization
 * @details Split from prop_verifier.c
 */

#include "lv/prop_verifier.h"
#include "prop_verifier_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"

/* ============================================================
 * 公式序列化
 * ============================================================ */

/**
 * @brief 获取运算符实例的优先级（用于序列化括号优化）
 *
 * @param f 公式指针
 * @return 优先级数值，越高绑定越紧
 */
/** @brief 公式类型→优先级静态查找表 */
static const int s_precedence_table[] = {
    [PROP_ATOM]         = PROP_PREC_ATOM,
    [PROP_CONJUNCTION]  = PROP_PREC_CONJUNCTION,
    [PROP_DISJUNCTION]  = PROP_PREC_DISJUNCTION,
    [PROP_IMPLICATION]  = PROP_PREC_IMPLICATION,
    [PROP_NEGATION]     = PROP_PREC_NEGATION,
    [PROP_BOTTOM]       = PROP_PREC_ATOM,
    [PROP_TRUE]         = PROP_PREC_ATOM,
};

/** @brief 公式类型→字符串/LaTeX 格式描述（单一事实源，arity/优先级不双份维护） */
typedef struct {
    const char *op_str;    /**< ASCII 运算符字符串（叶子类型为 NULL） */
    const char *latex_str; /**< LaTeX 运算符字符串（叶子类型为 NULL） */
    int arity;             /**< 子公式数：0=叶子, 1=一元, 2=二元 */
    bool right_inc_prec;   /**< 右子公式是否需要提升优先级（仅蕴含） */
} FormatSpec;

/** @brief 公式类型→格式描述查找表（ASCII 与 LaTeX 合并单表） */
static const FormatSpec s_format_spec[] = {
    [PROP_ATOM]         = {NULL,       NULL,          0, false},
    [PROP_CONJUNCTION]  = {" /\\ ",    " \\wedge ",   2, false},
    [PROP_DISJUNCTION]  = {" \\/ ",    " \\vee ",     2, false},
    [PROP_IMPLICATION]  = {" -> ",     " \\to ",      2, true},
    [PROP_NEGATION]     = {"~",        "\\neg ",      1, false},
    [PROP_BOTTOM]       = {"_|_",      "\\bot",       0, false},
    [PROP_TRUE]         = {"T",        "\\top",       0, false},
};

static int formula_precedence(const PropFormula *f) {
    if (!f || (unsigned)f->type >= sizeof(s_precedence_table) / sizeof(s_precedence_table[0]))
        return PROP_PREC_DEFAULT;
    return s_precedence_table[f->type];
}

/* 内部递归序列化（lvStrBuf 版本，无固定缓冲截断风险） */
static void formula_to_string_buf(const PropFormula *f, lvStrBuf *sb, int parent_prec) {
    if (!f)
        return;
    int prec = formula_precedence(f);
    bool need_parens = (parent_prec > prec);

    if (need_parens) {
        lv_strbuf_printf(sb, "(");
    }

    if ((unsigned)f->type < sizeof(s_format_spec) / sizeof(s_format_spec[0])) {
        const FormatSpec *spec = &s_format_spec[f->type];
        switch (spec->arity) {
            case 0: /* 叶子类型 */
                if (f->type == PROP_ATOM) {
                    lv_strbuf_printf(sb, "%s", f->data.atom.name);
                } else if (spec->op_str) {
                    lv_strbuf_printf(sb, "%s", spec->op_str);
                }
                break;
            case 1: /* 一元运算符 */
                if (spec->op_str) {
                    lv_strbuf_printf(sb, "%s", spec->op_str);
                }
                formula_to_string_buf(f->data.unary.operand, sb, prec);
                break;
            case 2: /* 二元运算符 */
                formula_to_string_buf(f->data.binary.left, sb, prec);
                if (spec->op_str) {
                    lv_strbuf_printf(sb, "%s", spec->op_str);
                }
                formula_to_string_buf(f->data.binary.right, sb,
                                      spec->right_inc_prec ? prec + 1 : prec);
                break;
        }
    }

    if (need_parens) {
        lv_strbuf_printf(sb, ")");
    }
}

/**
 * @brief 将命题公式序列化为字符串
 *
 * @param f 公式指针
 * @return 新分配的字符串指针，失败返回 NULL
 */
char *prop_formula_to_string(const PropFormula *f) {
    if (!f)
        return NULL;
    lvStrBuf sb;
    lv_strbuf_init(&sb);
    formula_to_string_buf(f, &sb, 0);
    return lv_strbuf_to_string(&sb);
}

/* LaTeX 序列化（lvStrBuf 版本，自动扩容，无固定缓冲截断风险） */
static void formula_to_latex_buf(const PropFormula *f, lvStrBuf *sb, int parent_prec) {
    if (!f)
        return;
    int prec = formula_precedence(f);
    bool need_parens = (parent_prec > prec);

    if (need_parens) {
        lv_strbuf_printf(sb, "\\left(");
    }

    if ((unsigned)f->type < sizeof(s_format_spec) / sizeof(s_format_spec[0])) {
        const FormatSpec *spec = &s_format_spec[f->type];
        switch (spec->arity) {
            case 0: /* 叶子类型 */
                if (f->type == PROP_ATOM) {
                    lv_strbuf_printf(sb, "%s", f->data.atom.name);
                } else if (spec->latex_str) {
                    lv_strbuf_printf(sb, "%s", spec->latex_str);
                }
                break;
            case 1: /* 一元运算符 */
                if (spec->latex_str) {
                    lv_strbuf_printf(sb, "%s", spec->latex_str);
                }
                formula_to_latex_buf(f->data.unary.operand, sb, prec);
                break;
            case 2: /* 二元运算符 */
                formula_to_latex_buf(f->data.binary.left, sb, prec);
                if (spec->latex_str) {
                    lv_strbuf_printf(sb, "%s", spec->latex_str);
                }
                formula_to_latex_buf(f->data.binary.right, sb,
                                     spec->right_inc_prec ? prec + 1 : prec);
                break;
        }
    }

    if (need_parens) {
        lv_strbuf_printf(sb, "\\right)");
    }
}

/**
 * @brief 将命题公式序列化为 LaTeX 字符串
 *
 * @param f 公式指针
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *prop_formula_to_latex(const PropFormula *f) {
    if (!f)
        return NULL;
    lvStrBuf sb;
    lv_strbuf_init(&sb);
    formula_to_latex_buf(f, &sb, 0);
    return lv_strbuf_to_string(&sb);
}

