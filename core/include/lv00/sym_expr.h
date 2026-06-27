#ifndef LV00_SYM_EXPR_H
#define LV00_SYM_EXPR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/**
 * @brief Symbolic expression node kind
 */
typedef enum {
    LV00_SYM_CONST = 0,
    LV00_SYM_VAR,
    LV00_SYM_ADD,
    LV00_SYM_MUL,
    LV00_SYM_POW,
    LV00_SYM_NEG,
    LV00_SYM_SIN,
    LV00_SYM_COS,
    LV00_SYM_SQRT,
    LV00_SYM_LOG
} Lv00SymExprKind;

/**
 * @brief Symbolic expression tree node
 */
typedef struct Lv00SymExpr {
    Lv00SymExprKind kind;
    double value;
    char *var_name;
    struct Lv00SymExpr **children;
    int child_count;
} Lv00SymExpr;

/* ============================================================
 * Construction
 * ============================================================ */

LV00_PUBLIC_API Lv00SymExpr *sym_expr_create_const(double value);

LV00_PUBLIC_API Lv00SymExpr *sym_expr_create_var(const char *var_name);

LV00_PUBLIC_API Lv00SymExpr *sym_expr_create_binary(Lv00SymExprKind kind,
                                                    Lv00SymExpr *left,
                                                    Lv00SymExpr *right);

LV00_PUBLIC_API Lv00SymExpr *sym_expr_create_unary(Lv00SymExprKind kind,
                                                   Lv00SymExpr *operand);

/* ============================================================
 * Destruction
 * ============================================================ */

LV00_PUBLIC_API void sym_expr_destroy(Lv00SymExpr *expr);

/* ============================================================
 * Simplification
 * ============================================================ */

LV00_PUBLIC_API Lv00SymExpr *sym_expr_simplify(const Lv00SymExpr *expr);

/* ============================================================
 * Evaluation
 * ============================================================ */

LV00_PUBLIC_API double sym_expr_eval_double(const Lv00SymExpr *expr,
                                            const char **var_names,
                                            const double *var_values,
                                            int var_count);

/* ============================================================
 * String representation
 * ============================================================ */

LV00_PUBLIC_API char *sym_expr_to_string(const Lv00SymExpr *expr);

/* ============================================================
 * Differentiation
 * ============================================================ */

LV00_PUBLIC_API Lv00SymExpr *sym_expr_diff(const Lv00SymExpr *expr,
                                           const char *var_name);

/* ============================================================
 * Substitution
 * ============================================================ */

LV00_PUBLIC_API Lv00SymExpr *sym_expr_substitute(const Lv00SymExpr *expr,
                                                 const char *var_name,
                                                 const Lv00SymExpr *replacement);

#ifdef __cplusplus
}
#endif

#endif
