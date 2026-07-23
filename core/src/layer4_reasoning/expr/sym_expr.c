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

#include "sym_expr.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Internal helpers
 * ============================================================ */

/**
 * @brief Allocate a new expression node with zero-initialized fields
 */
static lvSymExpr *sym_expr_alloc(lvSymExprKind kind) {
    lvSymExpr *expr = (lvSymExpr *)calloc(1, sizeof(lvSymExpr));
    if (!expr) return NULL;
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
    if (!expr) return NULL;

    lvSymExpr *copy = sym_expr_alloc(expr->kind);
    if (!copy) return NULL;

    copy->value = expr->value;

    if (expr->var_name) {
        copy->var_name = strdup(expr->var_name);
        if (!copy->var_name) {
            free(copy);
            return NULL;
        }
    }

    if (expr->child_count > 0 && expr->children) {
        copy->children = (lvSymExpr **)calloc(
            (size_t)expr->child_count, sizeof(lvSymExpr *));
        if (!copy->children) {
            free(copy->var_name);
            free(copy);
            return NULL;
        }
        copy->child_count = expr->child_count;
        for (int i = 0; i < expr->child_count; i++) {
            copy->children[i] = sym_expr_deep_copy(expr->children[i]);
            if (!copy->children[i]) {
                /* Cleanup partially built copy */
                for (int j = 0; j < i; j++) {
                    sym_expr_destroy(copy->children[j]);
                }
                free(copy->children);
                free(copy->var_name);
                free(copy);
                return NULL;
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
 * Construction
 * ============================================================ */

lv_PUBLIC_API lvSymExpr *sym_expr_create_const(double value) {
    lvSymExpr *expr = sym_expr_alloc(lv_SYM_CONST);
    if (!expr) return NULL;
    expr->value = value;
    return expr;
}

lv_PUBLIC_API lvSymExpr *sym_expr_create_var(const char *var_name) {
    if (!var_name) return NULL;

    lvSymExpr *expr = sym_expr_alloc(lv_SYM_VAR);
    if (!expr) return NULL;

    expr->var_name = strdup(var_name);
    if (!expr->var_name) {
        free(expr);
        return NULL;
    }
    return expr;
}

lv_PUBLIC_API lvSymExpr *sym_expr_create_binary(lvSymExprKind kind,
                                                    lvSymExpr *left,
                                                    lvSymExpr *right) {
    if (kind != lv_SYM_ADD && kind != lv_SYM_MUL && kind != lv_SYM_POW) {
        return NULL;
    }
    if (!left || !right) {
        sym_expr_destroy(left);
        sym_expr_destroy(right);
        return NULL;
    }

    lvSymExpr *expr = sym_expr_alloc(kind);
    if (!expr) {
        sym_expr_destroy(left);
        sym_expr_destroy(right);
        return NULL;
    }

    expr->child_count = 2;
    expr->children = (lvSymExpr **)calloc(2, sizeof(lvSymExpr *));
    if (!expr->children) {
        free(expr);
        sym_expr_destroy(left);
        sym_expr_destroy(right);
        return NULL;
    }
    expr->children[0] = left;
    expr->children[1] = right;
    return expr;
}

lv_PUBLIC_API lvSymExpr *sym_expr_create_unary(lvSymExprKind kind,
                                                   lvSymExpr *operand) {
    if (kind != lv_SYM_NEG && kind != lv_SYM_SIN && kind != lv_SYM_COS &&
        kind != lv_SYM_SQRT && kind != lv_SYM_LOG) {
        return NULL;
    }
    if (!operand) return NULL;

    lvSymExpr *expr = sym_expr_alloc(kind);
    if (!expr) {
        sym_expr_destroy(operand);
        return NULL;
    }

    expr->child_count = 1;
    expr->children = (lvSymExpr **)calloc(1, sizeof(lvSymExpr *));
    if (!expr->children) {
        free(expr);
        sym_expr_destroy(operand);
        return NULL;
    }
    expr->children[0] = operand;
    return expr;
}

/* ============================================================
 * Destruction
 * ============================================================ */

lv_PUBLIC_API void sym_expr_destroy(lvSymExpr *expr) {
    if (!expr) return;
    if (expr->children) {
        for (int i = 0; i < expr->child_count; i++) {
            sym_expr_destroy(expr->children[i]);
        }
        free(expr->children);
    }
    free(expr->var_name);
    free(expr);
}

/* ============================================================
 * Simplification
 * ============================================================ */

lv_PUBLIC_API lvSymExpr *sym_expr_simplify(const lvSymExpr *expr) {
    if (!expr) return NULL;

    /* Leaf nodes: constants and variables are already simplified */
    if (expr->kind == lv_SYM_CONST || expr->kind == lv_SYM_VAR) {
        return sym_expr_deep_copy(expr);
    }

    /* Recursively simplify children first */
    lvSymExpr **simplified_children = NULL;
    if (expr->child_count > 0) {
        simplified_children = (lvSymExpr **)calloc(
            (size_t)expr->child_count, sizeof(lvSymExpr *));
        if (!simplified_children) return NULL;
        for (int i = 0; i < expr->child_count; i++) {
            simplified_children[i] = sym_expr_simplify(expr->children[i]);
            if (!simplified_children[i]) {
                for (int j = 0; j < i; j++) sym_expr_destroy(simplified_children[j]);
                free(simplified_children);
                return NULL;
            }
        }
    }

    lvSymExpr *result = NULL;

    switch (expr->kind) {
    case lv_SYM_ADD: {
        lvSymExpr *left = simplified_children[0];
        lvSymExpr *right = simplified_children[1];

        /* 0 + x -> x */
        if (sym_expr_is_const_val(left, 0.0)) {
            result = right;
            sym_expr_destroy(left);
            free(simplified_children);
            return result;
        }
        /* x + 0 -> x */
        if (sym_expr_is_const_val(right, 0.0)) {
            result = left;
            sym_expr_destroy(right);
            free(simplified_children);
            return result;
        }
        /* const + const -> const */
        if (sym_expr_is_const(left) && sym_expr_is_const(right)) {
            result = sym_expr_create_const(left->value + right->value);
            sym_expr_destroy(left);
            sym_expr_destroy(right);
            free(simplified_children);
            return result;
        }
        /* Default: build simplified add node */
        result = sym_expr_create_binary(lv_SYM_ADD, left, right);
        break;
    }

    case lv_SYM_MUL: {
        lvSymExpr *left = simplified_children[0];
        lvSymExpr *right = simplified_children[1];

        /* 0 * x -> 0 */
        if (sym_expr_is_const_val(left, 0.0) || sym_expr_is_const_val(right, 0.0)) {
            result = sym_expr_create_const(0.0);
            sym_expr_destroy(left);
            sym_expr_destroy(right);
            free(simplified_children);
            return result;
        }
        /* 1 * x -> x */
        if (sym_expr_is_const_val(left, 1.0)) {
            result = right;
            sym_expr_destroy(left);
            free(simplified_children);
            return result;
        }
        /* x * 1 -> x */
        if (sym_expr_is_const_val(right, 1.0)) {
            result = left;
            sym_expr_destroy(right);
            free(simplified_children);
            return result;
        }
        /* const * const -> const */
        if (sym_expr_is_const(left) && sym_expr_is_const(right)) {
            result = sym_expr_create_const(left->value * right->value);
            sym_expr_destroy(left);
            sym_expr_destroy(right);
            free(simplified_children);
            return result;
        }
        /* Default: build simplified mul node */
        result = sym_expr_create_binary(lv_SYM_MUL, left, right);
        break;
    }

    case lv_SYM_POW: {
        lvSymExpr *base = simplified_children[0];
        lvSymExpr *exp = simplified_children[1];

        /* x^0 -> 1 */
        if (sym_expr_is_const_val(exp, 0.0)) {
            result = sym_expr_create_const(1.0);
            sym_expr_destroy(base);
            sym_expr_destroy(exp);
            free(simplified_children);
            return result;
        }
        /* x^1 -> x */
        if (sym_expr_is_const_val(exp, 1.0)) {
            result = base;
            sym_expr_destroy(exp);
            free(simplified_children);
            return result;
        }
        /* const^const -> const */
        if (sym_expr_is_const(base) && sym_expr_is_const(exp)) {
            /* Guard: pow(negative, non-integer) is undefined in reals.
             * Check if exponent is effectively an integer; if not and base < 0,
             * the result would be NaN. */
            double val;
            if (base->value < 0.0 && fabs(exp->value - round(exp->value)) > 1e-12) {
                val = NAN;
            } else {
                val = pow(base->value, exp->value);
            }
            result = sym_expr_create_const(val);
            sym_expr_destroy(base);
            sym_expr_destroy(exp);
            free(simplified_children);
            return result;
        }
        /* Default: build simplified pow node */
        result = sym_expr_create_binary(lv_SYM_POW, base, exp);
        break;
    }

    case lv_SYM_NEG: {
        lvSymExpr *operand = simplified_children[0];

        /* -const -> const */
        if (sym_expr_is_const(operand)) {
            result = sym_expr_create_const(-operand->value);
            sym_expr_destroy(operand);
            free(simplified_children);
            return result;
        }
        /* -(-x) -> x */
        if (operand->kind == lv_SYM_NEG && operand->child_count == 1) {
            result = operand->children[0];
            operand->children[0] = NULL; /* prevent double-free */
            sym_expr_destroy(operand);
            free(simplified_children);
            return result;
        }
        /* Default: build simplified neg node */
        result = sym_expr_create_unary(lv_SYM_NEG, operand);
        break;
    }

    case lv_SYM_SIN:
    case lv_SYM_COS:
    case lv_SYM_SQRT:
    case lv_SYM_LOG: {
        lvSymExpr *operand = simplified_children[0];

        /* f(const) -> const */
        if (sym_expr_is_const(operand)) {
            double val = 0.0;
            switch (expr->kind) {
            case lv_SYM_SIN:  val = sin(operand->value); break;
            case lv_SYM_COS:  val = cos(operand->value); break;
            case lv_SYM_SQRT: val = (operand->value >= 0.0) ? sqrt(operand->value) : NAN; break;
            case lv_SYM_LOG:  val = (operand->value > 0.0)  ? log(operand->value)  : NAN; break;
            default: break;
            }
            result = sym_expr_create_const(val);
            sym_expr_destroy(operand);
            free(simplified_children);
            return result;
        }
        /* Default: build simplified node */
        result = sym_expr_create_unary(expr->kind, operand);
        break;
    }

    default:
        /* Unknown kind: return deep copy */
        result = sym_expr_deep_copy(expr);
        break;
    }

    free(simplified_children);
    return result;
}

/* ============================================================
 * Evaluation
 * ============================================================ */

lv_PUBLIC_API double sym_expr_eval_double(const lvSymExpr *expr,
                                            const char **var_names,
                                            const double *var_values,
                                            int var_count) {
    if (!expr) return NAN;

    switch (expr->kind) {
    case lv_SYM_CONST:
        return expr->value;

    case lv_SYM_VAR:
        for (int i = 0; i < var_count; i++) {
            if (strcmp(expr->var_name, var_names[i]) == 0) {
                return var_values[i];
            }
        }
        return NAN; /* variable not found */

    case lv_SYM_ADD:
        return sym_expr_eval_double(expr->children[0], var_names, var_values, var_count) +
               sym_expr_eval_double(expr->children[1], var_names, var_values, var_count);

    case lv_SYM_MUL:
        return sym_expr_eval_double(expr->children[0], var_names, var_values, var_count) *
               sym_expr_eval_double(expr->children[1], var_names, var_values, var_count);

    case lv_SYM_POW: {
        double base = sym_expr_eval_double(expr->children[0], var_names, var_values, var_count);
        double exp  = sym_expr_eval_double(expr->children[1], var_names, var_values, var_count);
        /* Guard: NaN propagation and domain check for negative base with non-integer exponent */
        if (isnan(base) || isnan(exp)) return NAN;
        if (base < 0.0 && fabs(exp - round(exp)) > 1e-12) return NAN;
        return pow(base, exp);
    }

    case lv_SYM_NEG:
        return -sym_expr_eval_double(expr->children[0], var_names, var_values, var_count);

    case lv_SYM_SIN:
        return sin(sym_expr_eval_double(expr->children[0], var_names, var_values, var_count));

    case lv_SYM_COS:
        return cos(sym_expr_eval_double(expr->children[0], var_names, var_values, var_count));

    case lv_SYM_SQRT: {
        double val = sym_expr_eval_double(expr->children[0], var_names, var_values, var_count);
        if (isnan(val) || val < 0.0) return NAN;
        return sqrt(val);
    }

    case lv_SYM_LOG: {
        double val = sym_expr_eval_double(expr->children[0], var_names, var_values, var_count);
        if (isnan(val) || val <= 0.0) return NAN;
        return log(val);
    }

    default:
        return NAN;
    }
}

/* ============================================================
 * String representation
 * ============================================================ */

/**
 * @brief Internal recursive string builder
 *
 * @param expr      Expression node
 * @param buf       Output buffer
 * @param bufsize   Buffer size
 * @param parent_op Kind of parent operation (for parenthesization)
 * @return Number of characters written (excluding null terminator),
 *         or number of characters that would have been written if bufsize is 0
 */
static int sym_expr_to_string_impl(const lvSymExpr *expr, char *buf,
                                   int bufsize, lvSymExprKind parent_op) {
    if (!expr) {
        if (buf && bufsize > 0) buf[0] = '\0';
        return 0;
    }

    char tmp[256];
    int len = 0;

    switch (expr->kind) {
    case lv_SYM_CONST:
        /* Format integer-valued constants without decimal point */
        if (expr->value == (double)(long long)expr->value &&
            fabs(expr->value) < 1e15) {
            len = snprintf(tmp, sizeof(tmp), "%lld", (long long)expr->value);
        } else {
            len = snprintf(tmp, sizeof(tmp), "%.6g", expr->value);
        }
        break;

    case lv_SYM_VAR:
        len = snprintf(tmp, sizeof(tmp), "%s", expr->var_name ? expr->var_name : "?");
        break;

    case lv_SYM_ADD: {
        int l = sym_expr_to_string_impl(expr->children[0], NULL, 0, lv_SYM_ADD);
        int r = sym_expr_to_string_impl(expr->children[1], NULL, 0, lv_SYM_ADD);
        len = l + 3 + r; /* "left + right" */
        if (buf && bufsize > 0) {
            int pos = 0;
            pos += sym_expr_to_string_impl(expr->children[0], buf + pos, bufsize - pos, lv_SYM_ADD);
            if (pos < bufsize) pos += snprintf(buf + pos, bufsize - pos, " + ");
            pos += sym_expr_to_string_impl(expr->children[1], buf + pos, bufsize - pos, lv_SYM_ADD);
        }
        return len;
    }

    case lv_SYM_MUL: {
        int l = sym_expr_to_string_impl(expr->children[0], NULL, 0, lv_SYM_MUL);
        int r = sym_expr_to_string_impl(expr->children[1], NULL, 0, lv_SYM_MUL);
        /* Add parentheses for non-atomic children when parent is add/pow */
        int need_paren_l = (expr->children[0]->kind == lv_SYM_ADD);
        int need_paren_r = (expr->children[1]->kind == lv_SYM_ADD);
        len = l + (need_paren_l ? 2 : 0) + 3 + r + (need_paren_r ? 2 : 0);
        if (buf && bufsize > 0) {
            int pos = 0;
            if (need_paren_l && pos < bufsize) buf[pos++] = '(';
            pos += sym_expr_to_string_impl(expr->children[0], buf + pos, bufsize - pos, lv_SYM_MUL);
            if (need_paren_l && pos < bufsize) buf[pos++] = ')';
            if (pos < bufsize) pos += snprintf(buf + pos, bufsize - pos, " * ");
            if (need_paren_r && pos < bufsize) buf[pos++] = '(';
            pos += sym_expr_to_string_impl(expr->children[1], buf + pos, bufsize - pos, lv_SYM_MUL);
            if (need_paren_r && pos < bufsize) buf[pos++] = ')';
        }
        return len;
    }

    case lv_SYM_POW: {
        int l = sym_expr_to_string_impl(expr->children[0], NULL, 0, lv_SYM_POW);
        int r = sym_expr_to_string_impl(expr->children[1], NULL, 0, lv_SYM_POW);
        int need_paren_l = (expr->children[0]->kind != lv_SYM_CONST &&
                           expr->children[0]->kind != lv_SYM_VAR);
        len = l + (need_paren_l ? 2 : 0) + 3 + r;
        if (buf && bufsize > 0) {
            int pos = 0;
            if (need_paren_l && pos < bufsize) buf[pos++] = '(';
            pos += sym_expr_to_string_impl(expr->children[0], buf + pos, bufsize - pos, lv_SYM_POW);
            if (need_paren_l && pos < bufsize) buf[pos++] = ')';
            if (pos < bufsize) pos += snprintf(buf + pos, bufsize - pos, " ^ ");
            pos += sym_expr_to_string_impl(expr->children[1], buf + pos, bufsize - pos, lv_SYM_POW);
        }
        return len;
    }

    case lv_SYM_NEG:
        len = 1 + sym_expr_to_string_impl(expr->children[0], NULL, 0, lv_SYM_NEG);
        if (buf && bufsize > 0) {
            int pos = 0;
            buf[pos++] = '-';
            pos += sym_expr_to_string_impl(expr->children[0], buf + pos, bufsize - pos, lv_SYM_NEG);
        }
        return len;

    case lv_SYM_SIN:
        len = 5 + sym_expr_to_string_impl(expr->children[0], NULL, 0, lv_SYM_SIN) + 1;
        if (buf && bufsize > 0) {
            int pos = 0;
            pos += snprintf(buf + pos, bufsize - pos, "sin(");
            pos += sym_expr_to_string_impl(expr->children[0], buf + pos, bufsize - pos, lv_SYM_SIN);
            if (pos < bufsize) buf[pos++] = ')';
        }
        return len;

    case lv_SYM_COS:
        len = 5 + sym_expr_to_string_impl(expr->children[0], NULL, 0, lv_SYM_COS) + 1;
        if (buf && bufsize > 0) {
            int pos = 0;
            pos += snprintf(buf + pos, bufsize - pos, "cos(");
            pos += sym_expr_to_string_impl(expr->children[0], buf + pos, bufsize - pos, lv_SYM_COS);
            if (pos < bufsize) buf[pos++] = ')';
        }
        return len;

    case lv_SYM_SQRT:
        len = 6 + sym_expr_to_string_impl(expr->children[0], NULL, 0, lv_SYM_SQRT) + 1;
        if (buf && bufsize > 0) {
            int pos = 0;
            pos += snprintf(buf + pos, bufsize - pos, "sqrt(");
            pos += sym_expr_to_string_impl(expr->children[0], buf + pos, bufsize - pos, lv_SYM_SQRT);
            if (pos < bufsize) buf[pos++] = ')';
        }
        return len;

    case lv_SYM_LOG:
        len = 5 + sym_expr_to_string_impl(expr->children[0], NULL, 0, lv_SYM_LOG) + 1;
        if (buf && bufsize > 0) {
            int pos = 0;
            pos += snprintf(buf + pos, bufsize - pos, "log(");
            pos += sym_expr_to_string_impl(expr->children[0], buf + pos, bufsize - pos, lv_SYM_LOG);
            if (pos < bufsize) buf[pos++] = ')';
        }
        return len;
    }

    /* Copy from tmp to buf if needed */
    if (buf && bufsize > 0) {
        int copy_len = (len < bufsize) ? len : bufsize - 1;
        memcpy(buf, tmp, (size_t)copy_len);
        buf[copy_len] = '\0';
    }

    return len;
}

lv_PUBLIC_API char *sym_expr_to_string(const lvSymExpr *expr) {
    if (!expr) return NULL;

    /* First pass: compute required length */
    int len = sym_expr_to_string_impl(expr, NULL, 0, lv_SYM_CONST);
    if (len <= 0) {
        char *s = (char *)malloc(1);
        if (s) s[0] = '\0';
        return s;
    }

    /* Allocate and second pass: fill buffer */
    char *buf = (char *)malloc((size_t)(len + 1));
    if (!buf) return NULL;
    sym_expr_to_string_impl(expr, buf, len + 1, lv_SYM_CONST);
    buf[len] = '\0';
    return buf;
}

/* ============================================================
 * Differentiation
 * ============================================================ */

lv_PUBLIC_API lvSymExpr *sym_expr_diff(const lvSymExpr *expr,
                                           const char *var_name) {
    if (!expr || !var_name) return NULL;

    switch (expr->kind) {
    case lv_SYM_CONST:
        /* d/dx(c) = 0 */
        return sym_expr_create_const(0.0);

    case lv_SYM_VAR:
        /* d/dx(x) = 1, d/dx(y) = 0 */
        return sym_expr_create_const(strcmp(expr->var_name, var_name) == 0 ? 1.0 : 0.0);

    case lv_SYM_ADD:
        /* d/dx(a + b) = da/dx + db/dx */
        return sym_expr_create_binary(
            lv_SYM_ADD,
            sym_expr_diff(expr->children[0], var_name),
            sym_expr_diff(expr->children[1], var_name));

    case lv_SYM_MUL: {
        /* Product rule: d/dx(a * b) = a' * b + a * b' */
        lvSymExpr *da = sym_expr_diff(expr->children[0], var_name);
        lvSymExpr *db = sym_expr_diff(expr->children[1], var_name);
        lvSymExpr *a_copy = sym_expr_deep_copy(expr->children[0]);
        lvSymExpr *b_copy = sym_expr_deep_copy(expr->children[1]);

        lvSymExpr *term1 = sym_expr_create_binary(lv_SYM_MUL, da, b_copy);
        lvSymExpr *term2 = sym_expr_create_binary(lv_SYM_MUL, a_copy, db);

        return sym_expr_create_binary(lv_SYM_ADD, term1, term2);
    }

    case lv_SYM_POW: {
        /* Power rule: d/dx(f^g) = f^g * (g' * ln(f) + g * f'/f)
         * Special case for constant exponent: d/dx(f^n) = n * f^(n-1) * f' */
        if (sym_expr_is_const(expr->children[1])) {
            /* d/dx(f^n) = n * f^(n-1) * f' */
            double n = expr->children[1]->value;
            lvSymExpr *f_copy = sym_expr_deep_copy(expr->children[0]);
            lvSymExpr *f_prime = sym_expr_diff(expr->children[0], var_name);
            lvSymExpr *n_minus_1 = sym_expr_create_const(n - 1.0);
            lvSymExpr *n_const = sym_expr_create_const(n);

            lvSymExpr *f_pow = sym_expr_create_binary(lv_SYM_POW, f_copy, n_minus_1);
            lvSymExpr *n_f_pow = sym_expr_create_binary(lv_SYM_MUL, n_const, f_pow);
            return sym_expr_create_binary(lv_SYM_MUL, n_f_pow, f_prime);
        } else {
            /* General case: d/dx(f^g) = f^g * (g' * log(f) + g * f'/f) */
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
                sym_expr_deep_copy(expr->children[0]),
                sym_expr_deep_copy(expr->children[1]));
            return sym_expr_create_binary(lv_SYM_MUL, fg, inner);
        }
    }

    case lv_SYM_NEG:
        /* d/dx(-f) = -f' */
        return sym_expr_create_unary(lv_SYM_NEG,
                                     sym_expr_diff(expr->children[0], var_name));

    case lv_SYM_SIN:
        /* d/dx(sin(f)) = cos(f) * f' */
        return sym_expr_create_binary(
            lv_SYM_MUL,
            sym_expr_create_unary(lv_SYM_COS, sym_expr_deep_copy(expr->children[0])),
            sym_expr_diff(expr->children[0], var_name));

    case lv_SYM_COS:
        /* d/dx(cos(f)) = -sin(f) * f' */
        return sym_expr_create_binary(
            lv_SYM_MUL,
            sym_expr_create_unary(lv_SYM_NEG,
                sym_expr_create_unary(lv_SYM_SIN, sym_expr_deep_copy(expr->children[0]))),
            sym_expr_diff(expr->children[0], var_name));

    case lv_SYM_SQRT: {
        /* d/dx(sqrt(f)) = f' / (2 * sqrt(f)) */
        lvSymExpr *f_prime = sym_expr_diff(expr->children[0], var_name);
        lvSymExpr *sqrt_f = sym_expr_create_unary(lv_SYM_SQRT,
                                                     sym_expr_deep_copy(expr->children[0]));
        lvSymExpr *two = sym_expr_create_const(2.0);
        lvSymExpr *denom = sym_expr_create_binary(lv_SYM_MUL, two, sqrt_f);
        return sym_expr_create_binary(lv_SYM_MUL, f_prime,
            sym_expr_create_binary(lv_SYM_POW, denom, sym_expr_create_const(-1.0)));
    }

    case lv_SYM_LOG:
        /* d/dx(log(f)) = f' / f */
        return sym_expr_create_binary(
            lv_SYM_MUL,
            sym_expr_diff(expr->children[0], var_name),
            sym_expr_create_binary(lv_SYM_POW,
                sym_expr_deep_copy(expr->children[0]),
                sym_expr_create_const(-1.0)));

    default:
        return sym_expr_create_const(0.0);
    }
}

/* ============================================================
 * Substitution
 * ============================================================ */

lv_PUBLIC_API lvSymExpr *sym_expr_substitute(const lvSymExpr *expr,
                                                 const char *var_name,
                                                 const lvSymExpr *replacement) {
    if (!expr || !var_name || !replacement) return NULL;

    /* Leaf: variable matches -> return deep copy of replacement */
    if (expr->kind == lv_SYM_VAR) {
        if (strcmp(expr->var_name, var_name) == 0) {
            return sym_expr_deep_copy(replacement);
        }
        return sym_expr_deep_copy(expr);
    }

    /* Leaf: constant -> return copy */
    if (expr->kind == lv_SYM_CONST) {
        return sym_expr_create_const(expr->value);
    }

    /* Internal node: recursively substitute in children, then rebuild */
    lvSymExpr **new_children = (lvSymExpr **)calloc(
        (size_t)expr->child_count, sizeof(lvSymExpr *));
    if (!new_children) return NULL;

    for (int i = 0; i < expr->child_count; i++) {
        new_children[i] = sym_expr_substitute(expr->children[i], var_name, replacement);
        if (!new_children[i]) {
            for (int j = 0; j < i; j++) sym_expr_destroy(new_children[j]);
            free(new_children);
            return NULL;
        }
    }

    /* Rebuild node with substituted children */
    lvSymExpr *result = sym_expr_alloc(expr->kind);
    if (!result) {
        for (int i = 0; i < expr->child_count; i++) sym_expr_destroy(new_children[i]);
        free(new_children);
        return NULL;
    }
    result->children = new_children;
    result->child_count = expr->child_count;
    return result;
}
