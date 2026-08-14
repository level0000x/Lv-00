/**
 * @file sym_expr.c
 * @brief Symbolic expression tree -- implementation
 *
 * Implements tree-based symbolic expressions with simplification,
 * numerical evaluation, string rendering, differentiation, and
 * variable substitution.
 *
 * Reference: SymEngine basic.hpp, GiNaC ex.cpp
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/sym_expr.h"
#include "lv/lv_numeric.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/lv_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv/lv_strbuf.h"

/* ============================================================
 * Internal helpers
 * ============================================================ */

/**
 * @brief Allocate a new expression node with zero-initialized fields
 */
static lvSymExpr *sym_expr_alloc(lvSymExprKind kind) {
    lvSymExpr *expr = (lvSymExpr *) lv_calloc(1, sizeof(lvSymExpr));
    if (!expr)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_alloc: calloc failed");
    expr->kind = kind;
    expr->value = 0.0;
    expr->var_name = NULL;
    expr->children = NULL;
    expr->child_count = 0;
    return expr;
}

/**
 * @brief Deep-copy an expression tree
 */
static lvSymExpr *sym_expr_deep_copy(const lvSymExpr *expr) {
    if (!expr)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "sym_expr_deep_copy: input expr is NULL");

    lvSymExpr *copy = sym_expr_alloc(expr->kind);
    if (!copy)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_deep_copy: alloc copy failed");

    copy->value = expr->value;

    if (expr->var_name) {
        copy->var_name = lv_strdup_safe(expr->var_name);
        if (!copy->var_name) {
            sym_expr_destroy(copy);
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_deep_copy: strdup var_name failed");
        }
    }

    if (expr->child_count > 0 && expr->children) {
        copy->children = (lvSymExpr **) lv_calloc((size_t) expr->child_count, sizeof(lvSymExpr *));
        if (!copy->children) {
            sym_expr_destroy(copy);
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_deep_copy: calloc children failed");
        }
        copy->child_count = expr->child_count;
        for (int i = 0; i < expr->child_count; i++) {
            copy->children[i] = sym_expr_deep_copy(expr->children[i]);
            if (!copy->children[i]) {
                /* Cleanup partially built copy（sym_expr_destroy 对 NULL 子项安全） */
                sym_expr_destroy(copy);
                lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_deep_copy: child copy failed");
            }
        }
    }

    return copy;
}

/**
 * @brief Check if an expression is a constant leaf
 */
static int sym_expr_is_const(const lvSymExpr *expr) {
    return expr && expr->kind == lv_SYM_CONST;
}

/**
 * @brief Check if an expression is a variable leaf
 */
static int sym_expr_is_var(const lvSymExpr *expr) {
    return expr && expr->kind == lv_SYM_VAR;
}

/**
 * @brief Check if an expression is a constant with a specific value
 */
static int sym_expr_is_const_val(const lvSymExpr *expr, double val) {
    return sym_expr_is_const(expr) && expr->value == val;
}

/* ============================================================
 * VTable 类型分发 — 消除 ExprKind switch 反模式
 * ============================================================ */

/** @brief 符号表达式虚函数表条目（每个 kind 一组操作） */
typedef struct {
    /** 数值求值 */
    double (*eval_double)(const lvSymExpr *expr, const char **var_names,
                          const double *var_values, int var_count);
    /** 求导 */
    lvSymExpr *(*diff)(const lvSymExpr *expr, const char *var_name);
    /** 化简（接收已递归化简的子节点，应用代数规则后返回新节点） */
    lvSymExpr *(*simplify)(lvSymExprKind kind, lvSymExpr **simplified_children, int child_count);
    /** 字符串格式（lvStrBuf 动态构建，消除游标式 snprintf） */
    void (*to_string)(const lvSymExpr *expr, lvStrBuf *sb, lvSymExprKind parent_op);
} SymExprVTableEntry;

/* ── 各 kind 的 eval 实现 ── */

static double eval_const(const lvSymExpr *expr, const char **var_names, const double *var_values, int var_count) {
    (void)var_names; (void)var_values; (void)var_count;
    return expr->value;
}

static double eval_var(const lvSymExpr *expr, const char **var_names, const double *var_values, int var_count) {
    for (int i = 0; i < var_count; i++) {
        if (lv_str_eq(expr->var_name, var_names[i]))
            return var_values[i];
    }
    return NAN;
}

static double eval_binary_arith(const lvSymExpr *expr, const char **var_names,
                                 const double *var_values, int var_count, char op) {
    double a = sym_expr_eval_double(expr->children[0], var_names, var_values, var_count);
    double b = sym_expr_eval_double(expr->children[1], var_names, var_values, var_count);
    if (op == '+') return a + b;
    if (op == '*') return a * b;
    /* pow */
    if (isnan(a) || isnan(b)) return NAN;
    if (a < 0.0 && !lv_is_integer_double(b, lv_EPSILON_ULTRA)) return NAN;
    return pow(a, b);
}

static double eval_add(const lvSymExpr *expr, const char **var_names, const double *var_values, int var_count) {
    return eval_binary_arith(expr, var_names, var_values, var_count, '+');
}

static double eval_mul(const lvSymExpr *expr, const char **var_names, const double *var_values, int var_count) {
    return eval_binary_arith(expr, var_names, var_values, var_count, '*');
}

static double eval_pow(const lvSymExpr *expr, const char **var_names, const double *var_values, int var_count) {
    return eval_binary_arith(expr, var_names, var_values, var_count, '^');
}

static double eval_neg(const lvSymExpr *expr, const char **var_names, const double *var_values, int var_count) {
    return -sym_expr_eval_double(expr->children[0], var_names, var_values, var_count);
}

static double eval_sin(const lvSymExpr *expr, const char **var_names, const double *var_values, int var_count) {
    return sin(sym_expr_eval_double(expr->children[0], var_names, var_values, var_count));
}

static double eval_cos(const lvSymExpr *expr, const char **var_names, const double *var_values, int var_count) {
    return cos(sym_expr_eval_double(expr->children[0], var_names, var_values, var_count));
}

static double eval_sqrt(const lvSymExpr *expr, const char **var_names, const double *var_values, int var_count) {
    double val = sym_expr_eval_double(expr->children[0], var_names, var_values, var_count);
    if (isnan(val) || val < 0.0) return NAN;
    return sqrt(val);
}

static double eval_log(const lvSymExpr *expr, const char **var_names, const double *var_values, int var_count) {
    double val = sym_expr_eval_double(expr->children[0], var_names, var_values, var_count);
    if (isnan(val) || val <= 0.0) return NAN;
    return log(val);
}

/* ── 各 kind 的 diff 实现 ── */

static lvSymExpr *diff_const(const lvSymExpr *expr, const char *var_name) {
    (void)expr; (void)var_name;
    return sym_expr_create_const(0.0);
}

static lvSymExpr *diff_var(const lvSymExpr *expr, const char *var_name) {
    return sym_expr_create_const(lv_str_eq(expr->var_name, var_name) ? 1.0 : 0.0);
}

static lvSymExpr *diff_add(const lvSymExpr *expr, const char *var_name) {
    return sym_expr_create_binary(lv_SYM_ADD,
        sym_expr_diff(expr->children[0], var_name),
        sym_expr_diff(expr->children[1], var_name));
}

static lvSymExpr *diff_mul(const lvSymExpr *expr, const char *var_name) {
    lvSymExpr *da = sym_expr_diff(expr->children[0], var_name);
    lvSymExpr *db = sym_expr_diff(expr->children[1], var_name);
    lvSymExpr *a_copy = sym_expr_deep_copy(expr->children[0]);
    lvSymExpr *b_copy = sym_expr_deep_copy(expr->children[1]);
    lvSymExpr *term1 = sym_expr_create_binary(lv_SYM_MUL, da, b_copy);
    lvSymExpr *term2 = sym_expr_create_binary(lv_SYM_MUL, a_copy, db);
    return sym_expr_create_binary(lv_SYM_ADD, term1, term2);
}

static lvSymExpr *diff_pow(const lvSymExpr *expr, const char *var_name) {
    if (sym_expr_is_const(expr->children[1])) {
        double n = expr->children[1]->value;
        lvSymExpr *f_copy = sym_expr_deep_copy(expr->children[0]);
        lvSymExpr *f_prime = sym_expr_diff(expr->children[0], var_name);
        lvSymExpr *n_minus_1 = sym_expr_create_const(n - 1.0);
        lvSymExpr *n_const = sym_expr_create_const(n);
        lvSymExpr *f_pow = sym_expr_create_binary(lv_SYM_POW, f_copy, n_minus_1);
        lvSymExpr *n_f_pow = sym_expr_create_binary(lv_SYM_MUL, n_const, f_pow);
        return sym_expr_create_binary(lv_SYM_MUL, n_f_pow, f_prime);
    }
    lvSymExpr *f_copy = sym_expr_deep_copy(expr->children[0]);
    lvSymExpr *g_copy = sym_expr_deep_copy(expr->children[1]);
    lvSymExpr *f_prime = sym_expr_diff(expr->children[0], var_name);
    lvSymExpr *g_prime = sym_expr_diff(expr->children[1], var_name);
    lvSymExpr *log_f = sym_expr_create_unary(lv_SYM_LOG, f_copy);
    lvSymExpr *g_prime_log_f = sym_expr_create_binary(lv_SYM_MUL, g_prime, log_f);
    lvSymExpr *f_copy2 = sym_expr_deep_copy(expr->children[0]);
    lvSymExpr *g_f_prime = sym_expr_create_binary(lv_SYM_MUL, g_copy, f_prime);
    lvSymExpr *f_div = sym_expr_create_binary(lv_SYM_POW, f_copy2, sym_expr_create_const(-1.0));
    lvSymExpr *g_f_div = sym_expr_create_binary(lv_SYM_MUL, g_f_prime, f_div);
    lvSymExpr *inner = sym_expr_create_binary(lv_SYM_ADD, g_prime_log_f, g_f_div);
    lvSymExpr *fg = sym_expr_create_binary(lv_SYM_POW,
        sym_expr_deep_copy(expr->children[0]), sym_expr_deep_copy(expr->children[1]));
    return sym_expr_create_binary(lv_SYM_MUL, fg, inner);
}

static lvSymExpr *diff_neg(const lvSymExpr *expr, const char *var_name) {
    return sym_expr_create_unary(lv_SYM_NEG, sym_expr_diff(expr->children[0], var_name));
}

static lvSymExpr *diff_sin(const lvSymExpr *expr, const char *var_name) {
    return sym_expr_create_binary(lv_SYM_MUL,
        sym_expr_create_unary(lv_SYM_COS, sym_expr_deep_copy(expr->children[0])),
        sym_expr_diff(expr->children[0], var_name));
}

static lvSymExpr *diff_cos(const lvSymExpr *expr, const char *var_name) {
    return sym_expr_create_binary(lv_SYM_MUL,
        sym_expr_create_unary(lv_SYM_NEG,
            sym_expr_create_unary(lv_SYM_SIN, sym_expr_deep_copy(expr->children[0]))),
        sym_expr_diff(expr->children[0], var_name));
}

static lvSymExpr *diff_sqrt_fn(const lvSymExpr *expr, const char *var_name) {
    lvSymExpr *f_prime = sym_expr_diff(expr->children[0], var_name);
    lvSymExpr *sqrt_f = sym_expr_create_unary(lv_SYM_SQRT, sym_expr_deep_copy(expr->children[0]));
    lvSymExpr *two = sym_expr_create_const(2.0);
    lvSymExpr *denom = sym_expr_create_binary(lv_SYM_MUL, two, sqrt_f);
    return sym_expr_create_binary(lv_SYM_MUL, f_prime,
        sym_expr_create_binary(lv_SYM_POW, denom, sym_expr_create_const(-1.0)));
}

static lvSymExpr *diff_log(const lvSymExpr *expr, const char *var_name) {
    return sym_expr_create_binary(lv_SYM_MUL,
        sym_expr_diff(expr->children[0], var_name),
        sym_expr_create_binary(lv_SYM_POW,
            sym_expr_deep_copy(expr->children[0]), sym_expr_create_const(-1.0)));
}

/* ── 各 kind 的 simplify 实现 ── */

static lvSymExpr *simplify_add(lvSymExprKind kind, lvSymExpr **children, int child_count) {
    (void)kind;
    lvSymExpr *left = children[0], *right = children[1];
    if (sym_expr_is_const_val(left, 0.0)) { sym_expr_destroy(left); return right; }
    if (sym_expr_is_const_val(right, 0.0)) { sym_expr_destroy(right); return left; }
    if (sym_expr_is_const(left) && sym_expr_is_const(right)) {
        lvSymExpr *r = sym_expr_create_const(left->value + right->value);
        sym_expr_destroy(left); sym_expr_destroy(right); return r;
    }
    return sym_expr_create_binary(lv_SYM_ADD, left, right);
}

static lvSymExpr *simplify_mul(lvSymExprKind kind, lvSymExpr **children, int child_count) {
    (void)kind;
    lvSymExpr *left = children[0], *right = children[1];
    if (sym_expr_is_const_val(left, 0.0) || sym_expr_is_const_val(right, 0.0)) {
        sym_expr_destroy(left); sym_expr_destroy(right);
        return sym_expr_create_const(0.0);
    }
    if (sym_expr_is_const_val(left, 1.0)) { sym_expr_destroy(left); return right; }
    if (sym_expr_is_const_val(right, 1.0)) { sym_expr_destroy(right); return left; }
    if (sym_expr_is_const(left) && sym_expr_is_const(right)) {
        lvSymExpr *r = sym_expr_create_const(left->value * right->value);
        sym_expr_destroy(left); sym_expr_destroy(right); return r;
    }
    return sym_expr_create_binary(lv_SYM_MUL, left, right);
}

static lvSymExpr *simplify_pow(lvSymExprKind kind, lvSymExpr **children, int child_count) {
    (void)kind;
    lvSymExpr *base = children[0], *exp = children[1];
    if (sym_expr_is_const_val(exp, 0.0)) { sym_expr_destroy(base); sym_expr_destroy(exp); return sym_expr_create_const(1.0); }
    if (sym_expr_is_const_val(exp, 1.0)) { sym_expr_destroy(exp); return base; }
    if (sym_expr_is_const(base) && sym_expr_is_const(exp)) {
        double val = (base->value < 0.0 && !lv_is_integer_double(exp->value, lv_EPSILON_ULTRA))
                     ? NAN : pow(base->value, exp->value);
        lvSymExpr *r = sym_expr_create_const(val);
        sym_expr_destroy(base); sym_expr_destroy(exp); return r;
    }
    return sym_expr_create_binary(lv_SYM_POW, base, exp);
}

static lvSymExpr *simplify_neg(lvSymExprKind kind, lvSymExpr **children, int child_count) {
    (void)kind; (void)child_count;
    lvSymExpr *operand = children[0];
    if (sym_expr_is_const(operand)) {
        lvSymExpr *r = sym_expr_create_const(-operand->value);
        sym_expr_destroy(operand); return r;
    }
    if (operand->kind == lv_SYM_NEG && operand->child_count == 1) {
        lvSymExpr *r = operand->children[0];
        operand->children[0] = NULL;
        sym_expr_destroy(operand); return r;
    }
    return sym_expr_create_unary(lv_SYM_NEG, operand);
}

static lvSymExpr *simplify_unary_fn(lvSymExprKind kind, lvSymExpr **children, int child_count) {
    (void)child_count;
    lvSymExpr *operand = children[0];
    if (sym_expr_is_const(operand)) {
        double val = 0.0;
        if (kind == lv_SYM_SIN) val = sin(operand->value);
        else if (kind == lv_SYM_COS) val = cos(operand->value);
        else if (kind == lv_SYM_SQRT) val = (operand->value >= 0.0) ? sqrt(operand->value) : NAN;
        else if (kind == lv_SYM_LOG) val = (operand->value > 0.0) ? log(operand->value) : NAN;
        lvSymExpr *r = sym_expr_create_const(val);
        sym_expr_destroy(operand); return r;
    }
    return sym_expr_create_unary(kind, operand);
}

/* ── 各 kind 的 to_string 实现 ── */

/* Forward declaration（to_string 的 vtable 处理函数调用的递归格式化函数） */
static void sym_expr_to_string_impl(const lvSymExpr *expr, lvStrBuf *sb, lvSymExprKind parent_op);

static void to_string_const(const lvSymExpr *expr, lvStrBuf *sb, lvSymExprKind parent_op) {
    (void)parent_op;
    if (expr->value == (double)(long long)expr->value && fabs(expr->value) < 1e15)
        lv_strbuf_printf(sb, "%lld", (long long)expr->value);
    else
        lv_strbuf_printf(sb, "%.6g", expr->value);
}

static void to_string_var(const lvSymExpr *expr, lvStrBuf *sb, lvSymExprKind parent_op) {
    (void)parent_op;
    lv_strbuf_printf(sb, "%s", expr->var_name ? expr->var_name : "?");
}

static void to_string_binary_op(const lvSymExpr *expr, lvStrBuf *sb,
                                lvSymExprKind parent_op, const char *op_str) {
    (void)parent_op;
    int need_paren_l = (expr->kind == lv_SYM_MUL && expr->children[0]->kind == lv_SYM_ADD);
    int need_paren_r = (expr->kind == lv_SYM_MUL && expr->children[1]->kind == lv_SYM_ADD);
    if (need_paren_l)
        lv_strbuf_printf(sb, "(");
    sym_expr_to_string_impl(expr->children[0], sb, expr->kind);
    if (need_paren_l)
        lv_strbuf_printf(sb, ")");
    lv_strbuf_printf(sb, "%s", op_str);
    if (need_paren_r)
        lv_strbuf_printf(sb, "(");
    sym_expr_to_string_impl(expr->children[1], sb, expr->kind);
    if (need_paren_r)
        lv_strbuf_printf(sb, ")");
}

static void to_string_add(const lvSymExpr *expr, lvStrBuf *sb, lvSymExprKind parent_op) {
    to_string_binary_op(expr, sb, parent_op, " + ");
}

static void to_string_mul(const lvSymExpr *expr, lvStrBuf *sb, lvSymExprKind parent_op) {
    to_string_binary_op(expr, sb, parent_op, " * ");
}

static void to_string_pow(const lvSymExpr *expr, lvStrBuf *sb, lvSymExprKind parent_op) {
    (void)parent_op;
    int need_paren_l = (expr->children[0]->kind != lv_SYM_CONST && expr->children[0]->kind != lv_SYM_VAR);
    if (need_paren_l)
        lv_strbuf_printf(sb, "(");
    sym_expr_to_string_impl(expr->children[0], sb, lv_SYM_POW);
    if (need_paren_l)
        lv_strbuf_printf(sb, ")");
    lv_strbuf_printf(sb, " ^ ");
    sym_expr_to_string_impl(expr->children[1], sb, lv_SYM_POW);
}

static void to_string_neg(const lvSymExpr *expr, lvStrBuf *sb, lvSymExprKind parent_op) {
    (void)parent_op;
    int need_paren = (expr->children[0]->kind == lv_SYM_ADD);
    lv_strbuf_printf(sb, "-");
    if (need_paren)
        lv_strbuf_printf(sb, "(");
    sym_expr_to_string_impl(expr->children[0], sb, lv_SYM_NEG);
    if (need_paren)
        lv_strbuf_printf(sb, ")");
}

static void to_string_unary_fn(const lvSymExpr *expr, lvStrBuf *sb,
                               lvSymExprKind parent_op, const char *fn_name) {
    (void)parent_op;
    lv_strbuf_printf(sb, "%s(", fn_name);
    sym_expr_to_string_impl(expr->children[0], sb, expr->kind);
    lv_strbuf_printf(sb, ")");
}

static void to_string_sin(const lvSymExpr *expr, lvStrBuf *sb, lvSymExprKind parent_op) {
    to_string_unary_fn(expr, sb, parent_op, "sin");
}

static void to_string_cos(const lvSymExpr *expr, lvStrBuf *sb, lvSymExprKind parent_op) {
    to_string_unary_fn(expr, sb, parent_op, "cos");
}

static void to_string_sqrt(const lvSymExpr *expr, lvStrBuf *sb, lvSymExprKind parent_op) {
    to_string_unary_fn(expr, sb, parent_op, "sqrt");
}

static void to_string_log(const lvSymExpr *expr, lvStrBuf *sb, lvSymExprKind parent_op) {
    to_string_unary_fn(expr, sb, parent_op, "log");
}

/* ── VTable 数组 ── */

static const SymExprVTableEntry kSymExprVTables[lv_SYM_LOG + 1] = {
    [lv_SYM_CONST] = { eval_const,  diff_const,    NULL,               to_string_const },
    [lv_SYM_VAR]   = { eval_var,    diff_var,      NULL,               to_string_var },
    [lv_SYM_ADD]   = { eval_add,    diff_add,      simplify_add,       to_string_add },
    [lv_SYM_MUL]   = { eval_mul,    diff_mul,      simplify_mul,       to_string_mul },
    [lv_SYM_POW]   = { eval_pow,    diff_pow,      simplify_pow,       to_string_pow },
    [lv_SYM_NEG]   = { eval_neg,    diff_neg,      simplify_neg,       to_string_neg },
    [lv_SYM_SIN]   = { eval_sin,    diff_sin,      simplify_unary_fn,  to_string_sin },
    [lv_SYM_COS]   = { eval_cos,    diff_cos,      simplify_unary_fn,  to_string_cos },
    [lv_SYM_SQRT]  = { eval_sqrt,   diff_sqrt_fn,  simplify_unary_fn,  to_string_sqrt },
    [lv_SYM_LOG]   = { eval_log,    diff_log,      simplify_unary_fn,  to_string_log },
};

/** @brief 获取指定 kind 的 VTable 条目 */
static const SymExprVTableEntry *sym_expr_get_vtable(lvSymExprKind kind) {
    if (kind < 0 || kind > lv_SYM_LOG)
        return NULL;
    return &kSymExprVTables[kind];
}

/* ============================================================
 * Construction
 * ============================================================ */

lv_PUBLIC_API lvSymExpr *sym_expr_create_const(double value) {
    lvSymExpr *expr = sym_expr_alloc(lv_SYM_CONST);
    if (!expr)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_create_const: alloc failed");
    expr->value = value;
    return expr;
}

lv_PUBLIC_API lvSymExpr *sym_expr_create_var(const char *var_name) {
    if (!var_name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "sym_expr_create_var: var_name is NULL");

    lvSymExpr *expr = sym_expr_alloc(lv_SYM_VAR);
    if (!expr)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_create_var: alloc failed");

    expr->var_name = lv_strdup_safe(var_name);
    if (!expr->var_name) {
        lv_free((void **)&(expr));
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_create_var: strdup failed");
    }
    return expr;
}

lv_PUBLIC_API lvSymExpr *sym_expr_create_binary(lvSymExprKind kind, lvSymExpr *left, lvSymExpr *right) {
    if (kind != lv_SYM_ADD && kind != lv_SYM_MUL && kind != lv_SYM_POW) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "sym_expr_create_binary: invalid kind %d", (int)kind);
    }
    if (!left || !right) {
        sym_expr_destroy(left);
        sym_expr_destroy(right);
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "sym_expr_create_binary: left or right is NULL");
    }

    lvSymExpr *expr = sym_expr_alloc(kind);
    if (!expr) {
        sym_expr_destroy(left);
        sym_expr_destroy(right);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_create_binary: alloc failed");
    }

    expr->child_count = 2;
    expr->children = (lvSymExpr **) lv_calloc(2, sizeof(lvSymExpr *));
    if (!expr->children) {
        lv_free((void **)&(expr));
        sym_expr_destroy(left);
        sym_expr_destroy(right);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_create_binary: calloc children failed");
    }
    expr->children[0] = left;
    expr->children[1] = right;
    return expr;
}

lv_PUBLIC_API lvSymExpr *sym_expr_create_unary(lvSymExprKind kind, lvSymExpr *operand) {
    if (kind != lv_SYM_NEG && kind != lv_SYM_SIN && kind != lv_SYM_COS && kind != lv_SYM_SQRT && kind != lv_SYM_LOG) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "sym_expr_create_unary: invalid kind %d", (int)kind);
    }
    if (!operand)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "sym_expr_create_unary: operand is NULL");

    lvSymExpr *expr = sym_expr_alloc(kind);
    if (!expr) {
        sym_expr_destroy(operand);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_create_unary: alloc failed");
    }

    expr->child_count = 1;
    expr->children = (lvSymExpr **) lv_calloc(1, sizeof(lvSymExpr *));
    if (!expr->children) {
        lv_free((void **)&(expr));
        sym_expr_destroy(operand);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_create_unary: calloc children failed");
    }
    expr->children[0] = operand;
    return expr;
}

/* ============================================================
 * Destruction
 * ============================================================ */

lv_PUBLIC_API void sym_expr_destroy(lvSymExpr *expr) {
    if (!expr)
        return;
    if (expr->children) {
        for (int i = 0; i < expr->child_count; i++) {
            sym_expr_destroy(expr->children[i]);
        }
        lv_free((void **)&(expr->children));
    }
    lv_free((void **)&(expr->var_name));
    lv_free((void **)&(expr));
}

/* ============================================================
 * Simplification
 * ============================================================ */

lv_PUBLIC_API lvSymExpr *sym_expr_simplify(const lvSymExpr *expr) {
    if (!expr)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "sym_expr_simplify: input expr is NULL");

    /* Leaf nodes: constants and variables are already simplified */
    if (expr->kind == lv_SYM_CONST || expr->kind == lv_SYM_VAR) {
        return sym_expr_deep_copy(expr);
    }

    /* Recursively simplify children first */
    lvSymExpr **simplified_children = NULL;
    if (expr->child_count > 0) {
        simplified_children = (lvSymExpr **) lv_calloc((size_t) expr->child_count, sizeof(lvSymExpr *));
        if (!simplified_children)
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_simplify: calloc simplified_children failed");
        for (int i = 0; i < expr->child_count; i++) {
            simplified_children[i] = sym_expr_simplify(expr->children[i]);
            if (!simplified_children[i]) {
                for (int j = 0; j < i; j++)
                    sym_expr_destroy(simplified_children[j]);
                lv_free((void **)&(simplified_children));
                lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_simplify: child simplify failed");
            }
        }
    }

    lvSymExpr *result = NULL;

    /* VTable dispatch: 使用 kind 对应的 simplify 函数 */
    const SymExprVTableEntry *vt = sym_expr_get_vtable(expr->kind);
    if (vt && vt->simplify) {
        result = vt->simplify(expr->kind, simplified_children, expr->child_count);
    } else {
        /* Leaf (CONST, VAR) 或未知 kind：返回深拷贝 */
        result = sym_expr_deep_copy(expr);
    }

    lv_free((void **)&(simplified_children));
    return result;
}

/* ============================================================
 * Evaluation
 * ============================================================ */

lv_PUBLIC_API double sym_expr_eval_double(const lvSymExpr *expr, const char **var_names, const double *var_values,
                                          int var_count) {
    if (!expr)
        return NAN;

    /* VTable dispatch: 使用 kind 对应的 eval_double 函数 */
    const SymExprVTableEntry *vt = sym_expr_get_vtable(expr->kind);
    if (vt && vt->eval_double) {
        return vt->eval_double(expr, var_names, var_values, var_count);
    }
    return NAN;
}

/* ============================================================
 * String representation
 * ============================================================ */

/**
 * @brief Internal recursive string builder（lvStrBuf 动态构建，消除两遍法长度预估）
 *
 * @param expr      Expression node
 * @param sb        lvStrBuf 构建器
 * @param parent_op Kind of parent operation (for parenthesization)
 */
static void sym_expr_to_string_impl(const lvSymExpr *expr, lvStrBuf *sb, lvSymExprKind parent_op) {
    if (!expr)
        return;

    /* VTable dispatch: 使用 kind 对应的 to_string 函数 */
    const SymExprVTableEntry *vt = sym_expr_get_vtable(expr->kind);
    if (vt && vt->to_string) {
        vt->to_string(expr, sb, parent_op);
    }
    /* Unknown kind: empty string（不写入） */
}

lv_PUBLIC_API char *sym_expr_to_string(const lvSymExpr *expr) {
    if (!expr)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "sym_expr_to_string: input expr is NULL");

    /* lvStrBuf 单遍构建（自动扩容），替代原"先算长度再写入"的两遍法 */
    lvStrBuf sb;
    lv_strbuf_init(&sb);
    sym_expr_to_string_impl(expr, &sb, lv_SYM_CONST);
    return lv_strbuf_to_string(&sb);
}

/* ============================================================
 * Differentiation
 * ============================================================ */

lv_PUBLIC_API lvSymExpr *sym_expr_diff(const lvSymExpr *expr, const char *var_name) {
    if (!expr || !var_name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "sym_expr_diff: expr or var_name is NULL");

    /* VTable dispatch: 使用 kind 对应的 diff 函数 */
    const SymExprVTableEntry *vt = sym_expr_get_vtable(expr->kind);
    if (vt && vt->diff) {
        return vt->diff(expr, var_name);
    }
    return sym_expr_create_const(0.0);
}

/* ============================================================
 * Substitution
 * ============================================================ */

lv_PUBLIC_API lvSymExpr *sym_expr_substitute(const lvSymExpr *expr, const char *var_name,
                                             const lvSymExpr *replacement) {
    if (!expr || !var_name || !replacement)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "sym_expr_substitute: expr, var_name, or replacement is NULL");

    /* Leaf: variable matches -> return deep copy of replacement */
    if (expr->kind == lv_SYM_VAR) {
        if (lv_str_eq(expr->var_name, var_name)) {
            return sym_expr_deep_copy(replacement);
        }
        return sym_expr_deep_copy(expr);
    }

    /* Leaf: constant -> return copy */
    if (expr->kind == lv_SYM_CONST) {
        return sym_expr_create_const(expr->value);
    }

    /* Internal node: recursively substitute in children, then rebuild */
    lvSymExpr **new_children = (lvSymExpr **) lv_calloc((size_t) expr->child_count, sizeof(lvSymExpr *));
    if (!new_children)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_substitute: calloc new_children failed");

    for (int i = 0; i < expr->child_count; i++) {
        new_children[i] = sym_expr_substitute(expr->children[i], var_name, replacement);
        if (!new_children[i]) {
            for (int j = 0; j < i; j++)
                sym_expr_destroy(new_children[j]);
            lv_free((void **)&(new_children));
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_substitute: child substitute failed");
        }
    }

    /* Rebuild node with substituted children */
    lvSymExpr *result = sym_expr_alloc(expr->kind);
    if (!result) {
        for (int i = 0; i < expr->child_count; i++)
            sym_expr_destroy(new_children[i]);
        lv_free((void **)&(new_children));
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "sym_expr_substitute: alloc result failed");
    }
    result->children = new_children;
    result->child_count = expr->child_count;
    return result;
}
