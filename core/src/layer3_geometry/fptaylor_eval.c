/**
 * @file fptaylor_eval.c
 * @brief FPTaylor 浮点误差分析 — 简化实现（静态辅助函数）
 *
 * @details 本文件仅包含 fptaylor_eval 模块的静态辅助函数。
 *          所有公共 API（float_interval_*、fptaylor_evaluate_expr、
 *          fptaylor_evaluate_graph、fptaylor_verify_safety 等）
 *          在 float_error.c 中有完整实现。
 *
 *          包含的静态函数：
 *          - eval_simple_expr:     简易表达式求值器（递归下降解析）
 *          - compute_absolute_error_bound: 一阶泰勒展开绝对误差上界
 *          - compute_taylor_coefficients:  中心值和近似导数计算
 *
 * @version 3.5.0
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
            if (fabs(rhs) < 1e-308) return NAN;
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

    double h = 1e-8;

    /* 数值偏导数近似：对每个变量独立扰动，独立计算偏导 */
    for (int i = 0; i < var_count; i++) {
        double *x_plus = lv_calloc((size_t) var_count, sizeof(double));
        double *x_minus = lv_calloc((size_t) var_count, sizeof(double));
        if (!x_plus || !x_minus) {
            lv_free((void **) &x_plus);
            lv_free((void **) &x_minus);
            out_derivs[i] = 1.0; /* 保守默认值 */
            continue;
        }

        for (int j = 0; j < var_count; j++) {
            x_plus[j] = var_values[j];
            x_minus[j] = var_values[j];
        }
        x_plus[i] += h;
        x_minus[i] -= h;

        /* 使用表达式求值器计算扰动后的值 */
        double f_plus = eval_simple_expr(expr, x_plus, var_count);
        double f_minus = eval_simple_expr(expr, x_minus, var_count);

        lv_free((void **) &x_plus);
        lv_free((void **) &x_minus);

        if (isnan(f_plus) || isnan(f_minus)) {
            out_derivs[i] = 1.0; /* 求值失败，保守默认值 */
        } else {
            out_derivs[i] = fabs((f_plus - f_minus) / (2.0 * h));
        }
    }
    return true;
}
