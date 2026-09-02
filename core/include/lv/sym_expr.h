#ifndef lv_SYM_EXPR_H
#define lv_SYM_EXPR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/**
 * @brief Symbolic expression node kind
 */
typedef enum {
    lv_SYM_CONST = 0,
    lv_SYM_VAR,
    lv_SYM_ADD,
    lv_SYM_MUL,
    lv_SYM_POW,
    lv_SYM_NEG,
    lv_SYM_SIN,
    lv_SYM_COS,
    lv_SYM_SQRT,
    lv_SYM_LOG
} lvSymExprKind;

/**
 * @brief Symbolic expression tree node
 */
typedef struct lvSymExpr {
    lvSymExprKind kind;
    double value;
    char *var_name;
    struct lvSymExpr **children;
    int child_count;
} lvSymExpr;

/* ============================================================
 * Construction
 * ============================================================ */

lv_PUBLIC_API lvSymExpr *sym_expr_create_const(double value);

lv_PUBLIC_API lvSymExpr *sym_expr_create_var(const char *var_name);

lv_PUBLIC_API lvSymExpr *sym_expr_create_binary(lvSymExprKind kind, lvSymExpr *left, lvSymExpr *right);

lv_PUBLIC_API lvSymExpr *sym_expr_create_unary(lvSymExprKind kind, lvSymExpr *operand);

/* ============================================================
 * Destruction
 * ============================================================ */

lv_PUBLIC_API void sym_expr_destroy(lvSymExpr *expr);

/* ============================================================
 * Simplification
 * ============================================================ */

lv_PUBLIC_API lvSymExpr *sym_expr_simplify(const lvSymExpr *expr);

/* ============================================================
 * Evaluation
 * ============================================================ */

lv_PUBLIC_API double sym_expr_eval_double(const lvSymExpr *expr, const char **var_names, const double *var_values,
                                          int var_count);

/* ============================================================
 * String representation
 * ============================================================ */

lv_PUBLIC_API char *sym_expr_to_string(const lvSymExpr *expr);

/* ============================================================
 * Differentiation
 * ============================================================ */

lv_PUBLIC_API lvSymExpr *sym_expr_diff(const lvSymExpr *expr, const char *var_name);

/* ============================================================
 * Substitution
 * ============================================================ */

lv_PUBLIC_API lvSymExpr *sym_expr_substitute(const lvSymExpr *expr, const char *var_name, const lvSymExpr *replacement);

#ifdef __cplusplus
}
#endif

#endif
