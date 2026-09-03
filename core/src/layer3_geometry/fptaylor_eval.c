/**
 * @file fptaylor_eval.c
 * @brief FPTaylor 浮点误差分析 — 简化实现（静态辅助函数）
 *
 * @details 本文件为 fptaylor_eval 模块辅助与 REAL 高精度复核后端：
 *          浮点误差分析的公共 API（float_interval_*、fptaylor_evaluate_expr、
 *          fptaylor_evaluate_graph、fptaylor_verify_safety 等）在 float_error.c
 *          中有完整实现。
 *
 *          本文件含：
 *          - eval_simple_expr:     简易表达式求值器（递归下降解析，double）
 *          - compute_absolute_error_bound: 一阶泰勒展开绝对误差上界
 *          - compute_taylor_coefficients:  中心值和近似导数计算
 *          - fptaylor_eval_real_expr / fptaylor_eval_expr_double /
 *            fptaylor_verify_expr_real（批次 C1-C3：REAL(MPFR) 高精度复核，
 *            同一文法任意精度重算 + double 中心判定）
 *
 * @version 3.5.0（+C1-C3）
 * @date 2026-05-24
 */

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/float_error.h"
#include "lv/lv_internal.h"
#include "lv/lv_numeric.h" /* 有限差分公共工具（lv_finite_difference / lv_NUMERICAL_DIFF_EPSILON） */
#include "lv/lv_number.h" /* C2：REAL(MPFR) 高精度重算后端 */

/* ============================================================
 * 简易表达式求值器（递归下降解析）
 * ============================================================ */

/** 表达式求值中的词法 token 类型 */
typedef enum {
    FP_TOKEN_END,     /**< 输入结束 */
    FP_TOKEN_NUMBER,  /**< 数值常量 */
    FP_TOKEN_VARIABLE,/**< 变量 x0, x1, ... */
    FP_TOKEN_PLUS,    /**< + */
    FP_TOKEN_MINUS,   /**< - */
    FP_TOKEN_MUL,     /**< * */
    FP_TOKEN_DIV,     /**< / */
    FP_TOKEN_LPAREN,  /**< ( */
    FP_TOKEN_RPAREN,  /**< ) */
    FP_TOKEN_ERROR    /**< 词法错误 */
} FpTokenType;

/** 词法分析器上下文 */
typedef struct {
    const char *p;         /**< 当前解析位置 */
    FpTokenType tok;         /**< 当前 token 类型 */
    double num_val;        /**< 当前 token 的数值（FP_TOKEN_NUMBER 时） */
    int var_idx;           /**< 当前 token 的变量索引（FP_TOKEN_VARIABLE 时） */
    const double *var_vals;/**< 变量值数组 */
    int var_count;         /**< 变量数量 */
} Lexer;

/* ── 单字符运算符查找表 ── */

/**
 * @brief 单字符运算符查找表
 *
 * 按 ASCII 下标索引；表项为零值 FP_TOKEN_END 表示未映射的字符
 * （未命中时落到下方数值/变量解析）。
 */
static const FpTokenType s_operator_tokens[128] = {
    ['+'] = FP_TOKEN_PLUS,
    ['-'] = FP_TOKEN_MINUS,
    ['*'] = FP_TOKEN_MUL,
    ['/'] = FP_TOKEN_DIV,
    ['('] = FP_TOKEN_LPAREN,
    [')'] = FP_TOKEN_RPAREN,
};

/** 向前看一个 token */
static void lex_advance(Lexer *lex) {
    /* 跳过空白 */
    while (*(lex->p) == ' ' || *(lex->p) == '\t') {
        lex->p++;
    }

    char c = *(lex->p);
    if (c == '\0') {
        lex->tok = FP_TOKEN_END;
        return;
    }

    /* 单字符运算符：查表分发，未命中则落到下方数值/变量解析 */
    if ((unsigned char) c < 128) {
        FpTokenType op_tok = s_operator_tokens[(unsigned char) c];
        if (op_tok != FP_TOKEN_END) {
            lex->tok = op_tok;
            lex->p++;
            return;
        }
    }

    /* 数值常量 */
    if ((c >= '0' && c <= '9') || c == '.') {
        char *end = NULL;
        double val = strtod(lex->p, &end);
        if (end == lex->p) {
            lex->tok = FP_TOKEN_ERROR;
            return;
        }
        lex->tok = FP_TOKEN_NUMBER;
        lex->num_val = val;
        lex->p = end;
        return;
    }

    /* 变量：x0, x1, ..., xN */
    if (c == 'x' || c == 'X') {
        const char *digits = lex->p + 1;
        if (*digits < '0' || *digits > '9') {
            lex->tok = FP_TOKEN_ERROR;
            return;
        }
        char *end = NULL;
        long idx = strtol(digits, &end, 10);
        if (idx < 0 || idx >= lex->var_count || end == digits) {
            lex->tok = FP_TOKEN_ERROR;
            return;
        }
        lex->tok = FP_TOKEN_VARIABLE;
        lex->var_idx = (int) idx;
        lex->p = end;
        return;
    }

    lex->tok = FP_TOKEN_ERROR;
}

/* 前向声明 */
static double parse_expr(Lexer *lex);

/** 解析基本因子：number | variable | '(' expr ')' */
static double parse_primary(Lexer *lex) {
    if (lex->tok == FP_TOKEN_NUMBER) {
        double v = lex->num_val;
        lex_advance(lex);
        return v;
    }
    if (lex->tok == FP_TOKEN_VARIABLE) {
        double v = lex->var_vals[lex->var_idx];
        lex_advance(lex);
        return v;
    }
    if (lex->tok == FP_TOKEN_LPAREN) {
        lex_advance(lex);
        double v = parse_expr(lex);
        if (lex->tok != FP_TOKEN_RPAREN) {
            return NAN;
        }
        lex_advance(lex);
        return v;
    }
    /* 一元负号 */
    if (lex->tok == FP_TOKEN_MINUS) {
        lex_advance(lex);
        return -parse_primary(lex);
    }
    /* 一元正号 */
    if (lex->tok == FP_TOKEN_PLUS) {
        lex_advance(lex);
        return parse_primary(lex);
    }
    return NAN;
}

/** 解析乘除：primary { '*'|'/' primary } */
static double parse_term(Lexer *lex) {
    double v = parse_primary(lex);
    if (isnan(v)) return NAN;

    while (lex->tok == FP_TOKEN_MUL || lex->tok == FP_TOKEN_DIV) {
        FpTokenType op = lex->tok;
        lex_advance(lex);
        double rhs = parse_primary(lex);
        if (isnan(rhs)) return NAN;
        if (op == FP_TOKEN_MUL) {
            v *= rhs;
        } else {
            if (fabs(rhs) < lv_SAFE_MIN_POSITIVE) return NAN;
            v /= rhs;
        }
    }
    return v;
}

/** 解析加减：term { '+'|'-' term } */
static double parse_expr(Lexer *lex) {
    double v = parse_term(lex);
    if (isnan(v)) return NAN;

    while (lex->tok == FP_TOKEN_PLUS || lex->tok == FP_TOKEN_MINUS) {
        FpTokenType op = lex->tok;
        lex_advance(lex);
        double rhs = parse_term(lex);
        if (isnan(rhs)) return NAN;
        if (op == FP_TOKEN_PLUS) {
            v += rhs;
        } else {
            v -= rhs;
        }
    }
    return v;
}

/**
 * @brief 简易数学表达式求值器
 *
 * 使用递归下降解析算法，支持：
 *   - 二元运算符：+ - * /
 *   - 一元正负号
 *   - 括号 ()
 *   - 变量：x0, x1, ..., xN
 *   - 数值常量（整数和浮点数）
 *
 * @param expr       表达式字符串
 * @param var_values 变量值数组
 * @param var_count  变量数量
 * @return 表达式值，解析失败返回 NAN
 */
static double eval_simple_expr(const char *expr, const double *var_values, int var_count) {
    if (!expr || !var_values || var_count <= 0) {
        return NAN;
    }

    Lexer lex;
    lex.p = expr;
    lex.var_vals = var_values;
    lex.var_count = var_count;
    lex_advance(&lex);

    double result = parse_expr(&lex);
    if (lex.tok != FP_TOKEN_END) {
        return NAN;
    }
    return result;
}

/* ============================================================
 * REAL(MPFR) 后端：同一文法的任意精度重算（C2，批次 256）
 * ============================================================ */
/** 二元合并：结果 = a op b，随后销毁 a,b（失败返回 NULL，a/b 仍销毁） */
static lvNumber *fpexpr_bin2(lvNumber *a, lvNumber *b, int is_add, int is_mul, int is_div) {
    lvNumber *r = NULL;
    if (is_add) r = lv_number_add(a, b);
    else if (is_mul) r = lv_number_mul(a, b);
    else if (is_div) r = lv_number_div(a, b);
    else r = lv_number_sub(a, b);
    lv_number_destroy(a);
    lv_number_destroy(b);
    return r;
}

static lvNumber *fpexpr_expr(Lexer *lex, int prec); /* 前向 */

static lvNumber *fpexpr_primary(Lexer *lex, int prec) {
    if (lex->tok == FP_TOKEN_NUMBER) {
        lvNumber *n = lv_number_real_from_double(lex->num_val, prec);
        lex_advance(lex);
        return n;
    }
    if (lex->tok == FP_TOKEN_VARIABLE) {
        lvNumber *n = lv_number_real_from_double(lex->var_vals[lex->var_idx], prec);
        lex_advance(lex);
        return n;
    }
    if (lex->tok == FP_TOKEN_LPAREN) {
        lex_advance(lex);
        lvNumber *v = fpexpr_expr(lex, prec);
        if (!v) return NULL;
        if (lex->tok != FP_TOKEN_RPAREN) { lv_number_destroy(v); return NULL; }
        lex_advance(lex);
        return v;
    }
    if (lex->tok == FP_TOKEN_MINUS) {
        lex_advance(lex);
        lvNumber *v = fpexpr_primary(lex, prec);
        if (!v) return NULL;
        lvNumber *n = lv_number_neg(v);
        lv_number_destroy(v);
        return n;
    }
    if (lex->tok == FP_TOKEN_PLUS) {
        lex_advance(lex);
        return fpexpr_primary(lex, prec);
    }
    return NULL;
}

static lvNumber *fpexpr_term(Lexer *lex, int prec) {
    lvNumber *v = fpexpr_primary(lex, prec);
    if (!v) return NULL;
    while (lex->tok == FP_TOKEN_MUL || lex->tok == FP_TOKEN_DIV) {
        FpTokenType op = lex->tok;
        lex_advance(lex);
        lvNumber *rhs = fpexpr_primary(lex, prec);
        if (!rhs) { lv_number_destroy(v); return NULL; }
        if (op == FP_TOKEN_DIV && lv_number_is_zero(rhs)) {
            lv_number_destroy(v); lv_number_destroy(rhs); return NULL;
        }
        lvNumber *r = (op == FP_TOKEN_MUL) ? fpexpr_bin2(v, rhs, 0, 1, 0)
                                           : fpexpr_bin2(v, rhs, 0, 0, 1);
        if (!r) return NULL;
        v = r;
    }
    return v;
}

static lvNumber *fpexpr_expr(Lexer *lex, int prec) {
    lvNumber *v = fpexpr_term(lex, prec);
    if (!v) return NULL;
    while (lex->tok == FP_TOKEN_PLUS || lex->tok == FP_TOKEN_MINUS) {
        FpTokenType op = lex->tok;
        lex_advance(lex);
        lvNumber *rhs = fpexpr_term(lex, prec);
        if (!rhs) { lv_number_destroy(v); return NULL; }
        lvNumber *r = (op == FP_TOKEN_PLUS) ? fpexpr_bin2(v, rhs, 1, 0, 0)
                                            : fpexpr_bin2(v, rhs, 0, 0, 0);
        if (!r) return NULL;
        v = r;
    }
    return v;
}

/**
 * @brief 用 REAL(MPFR) 以任意精度重算简易算术表达式（C2）
 *
 * 文法与 double 版 eval_simple_expr 一致（数值/+ - * //括号/变量 x0..）。
 * @param prec_bits 目标精度（≤0 用默认 REAL 精度）
 * @return REAL 结果（[take] 调用者 lv_number_destroy），失败 NULL
 */
lv_PUBLIC_API lvNumber *fptaylor_eval_real_expr(const char *expr, const double *var_values, int var_count,
                                                int prec_bits) {
    if (!expr || !var_values || var_count <= 0)
        return NULL;
    Lexer lex;
    lex.p = expr;
    lex.var_vals = var_values;
    lex.var_count = var_count;
    lex_advance(&lex);
    lvNumber *r = fpexpr_expr(&lex, prec_bits);
    if (!r) return NULL;
    if (lex.tok != FP_TOKEN_END) { lv_number_destroy(r); return NULL; }
    return r;
}

/** C3：double 求值（供「double 中心 vs REAL 真值」复核）；失败返回 NAN */
lv_PUBLIC_API double fptaylor_eval_expr_double(const char *expr, const double *var_values, int var_count) {
    return eval_simple_expr(expr, var_values, var_count);
}

/** C3：复核 double 中心是否落在 REAL 高精度真值的 abs/rel 容差内（opt-in 旁路） */
lv_PUBLIC_API bool fptaylor_verify_expr_real(const char *expr, const double *var_values, int var_count,
                                             int prec_bits, double rel_tol, double abs_tol) {
    double dc = eval_simple_expr(expr, var_values, var_count);
    if (isnan(dc))
        return false;
    lvNumber *ref = fptaylor_eval_real_expr(expr, var_values, var_count, prec_bits);
    if (!ref)
        return false;
    lvNumber *approx = lv_number_from_double(dc);
    bool ok = approx != NULL && lv_number_real_verify(approx, ref, rel_tol, abs_tol);
    lv_number_destroy(approx);
    lv_number_destroy(ref);
    return ok;
}

/* ============================================================
 * 内部误差计算辅助
 * ============================================================ */

/**
 * @brief 计算 FP32 的机器 epsilon: 2^-24
 */
static double fp32_epsilon(void) {
    return ldexp(1.0, -24);
}

/**
 * @brief 计算 FP64 的机器 epsilon: 2^-53
 */
static double fp64_epsilon(void) {
    return ldexp(1.0, -53);
}

/**
 * @brief 计算一阶泰勒展开的绝对误差上界
 *
 * 对表达式 f(x1,...,xn) 在中心点展开后，
 * 绝对误差上界 = SUM_i |df/dxi| * |xi - center_i| + 高阶余项
 *
 * @param abs_deriv_sum  各偏导数绝对值之和（一阶贡献）
 * @param var_ranges     各变量的区间宽度
 * @param var_count      变量数量
 * @param eps            机器 epsilon
 * @return 绝对误差上界
 */
static double compute_absolute_error_bound(const double *abs_deriv_sum, const double *var_ranges, int var_count,
                                           double eps) {
    double bound = 0.0;
    for (int i = 0; i < var_count; i++) {
        /* 一阶项：|df/dxi| * range(xi) * eps */
        bound += abs_deriv_sum[i] * var_ranges[i] * eps;
    }
    /* 加上舍入误差的保守估计（每步运算最多贡献一个 eps） */
    bound += eps * (double) var_count;
    return fabs(bound);
}

/* 有限差分回调上下文：表达式 + 中心值数组（只读）+ 复用工作缓冲 */
typedef struct {
    const char *expr;          /**< 表达式字符串 */
    const double *var_values;  /**< 变量中心值（只读） */
    int var_count;             /**< 变量数量 */
    int var_idx;               /**< 被求导变量索引 */
    double *work;              /**< 扰动工作缓冲（var_count 个 double） */
} FpTaylorFdCtx;

/** @brief 简易表达式求值回调：将第 var_idx 个变量设为 x 后求值（供 lv_finite_difference 使用） */
static double simple_expr_at(double x, void *userdata) {
    FpTaylorFdCtx *ctx = (FpTaylorFdCtx *) userdata;
    memcpy(ctx->work, ctx->var_values, (size_t) ctx->var_count * sizeof(double));
    ctx->work[ctx->var_idx] = x;
    return eval_simple_expr(ctx->expr, ctx->work, ctx->var_count);
}

/**
 * @brief 计算简单表达式的中心值和近似导数
 *
 * 使用有限差分法近似偏导数：
 *   df/dxi ≈ (f(x+h) - f(x-h)) / (2h)
 *
 * 使用 eval_simple_expr 对表达式字符串进行实际求值，
 * 不假设表达式形式，正确计算每个变量的独立偏导。
 *
 * @param expr        表达式字符串
 * @param var_values  变量中心值
 * @param var_count   变量数量
 * @param out_center  输出中心值
 * @param out_derivs  输出偏导数绝对值数组
 * @return 成功返回 true
 */
static bool compute_taylor_coefficients(const char *expr, const double *var_values, int var_count, double *out_center,
                                        double *out_derivs) {
    if (!expr || !var_values || !out_center || !out_derivs)
        return false;

    /* 使用简易表达式求值器计算中心值 */
    *out_center = eval_simple_expr(expr, var_values, var_count);
    if (isnan(*out_center)) {
        /* 表达式求值失败，回退到保守估计 */
        *out_center = 0.0;
        for (int i = 0; i < var_count; i++) {
            out_derivs[i] = 1.0;
        }
        return false;
    }

    double h = lv_NUMERICAL_DIFF_EPSILON; /* 原硬编码 1e-8，统一引用公共常量（固定绝对步长） */

    /* 数值偏导数近似：对每个变量独立扰动，独立计算偏导 */
    for (int i = 0; i < var_count; i++) {
        /* 回调上下文：在 var_values 副本上扰动第 i 个变量 */
        FpTaylorFdCtx ctx = {expr, var_values, var_count, i, NULL};
        ctx.work = lv_calloc((size_t) var_count, sizeof(double));
        if (!ctx.work) {
            out_derivs[i] = 1.0; /* 保守默认值 */
            continue;
        }

        /* 中心差分导数（绝对值语义，与原实现 fabs 一致）：
         * 求值失败（NaN）时由公共工具返回 NaN，回退保守默认值 */
        double d = lv_finite_difference(simple_expr_at, var_values[i], h, lv_FD_CENTRAL, &ctx);

        lv_free((void **) &ctx.work);

        out_derivs[i] = (isnan(d)) ? 1.0 : fabs(d);
    }
    return true;
}
