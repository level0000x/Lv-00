/**
 * @file sym_expr.h
 * @brief Symbolic expression tree -- construction, simplification, evaluation, differentiation
 *
 * Provides a tree-based symbolic expression system supporting constants,
 * variables, binary operations (add, mul, pow), unary operations (neg, sin,
 * cos, sqrt, log), simplification, numerical evaluation, string rendering,
 * symbolic differentiation, and variable substitution.
 *
 * Reference: SymEngine (C++ symbolic library), GiNaC (embedded CAS)
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef LV00_SYM_EXPR_H
#define LV00_SYM_EXPR_H

#include "lv00.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Types
 * ============================================================ */

/**
 * @brief Symbolic expression kind (node type)
 */
typedef enum Lv00SymExprKind {
    LV00_SYM_CONST = 0,  /**< Numeric constant */
    LV00_SYM_VAR,        /**< Variable (named) */
    LV00_SYM_ADD,        /**< Addition (binary) */
    LV00_SYM_MUL,        /**< Multiplication (binary) */
    LV00_SYM_POW,        /**< Exponentiation (binary) */
    LV00_SYM_NEG,        /**< Negation (unary) */
    LV00_SYM_SIN,        /**< Sine (unary) */
    LV00_SYM_COS,        /**< Cosine (unary) */
    LV00_SYM_SQRT,       /**< Square root (unary) */
    LV00_SYM_LOG         /**< Natural logarithm (unary) */
} Lv00SymExprKind;

/**
 * @brief Symbolic expression node (tree structure)
 *
 * Each node represents either a leaf (constant or variable) or an
 * internal node (operation with children). The tree is owned by the
 * root node; destroying the root recursively frees all children.
 */
typedef struct Lv00SymExpr {
    Lv00SymExprKind  kind;        /**< Node type */
    double           value;       /**< Numeric value (only for SYM_CONST) */
    char            *var_name;    /**< Variable name (only for SYM_VAR, heap-allocated) */
    struct Lv00SymExpr **children; /**< Child nodes (heap-allocated array) */
    int              child_count; /**< Number of children */
} Lv00SymExpr;

/* ============================================================
 * Construction
 * ============================================================ */

/**
 * @brief Create a constant expression
 *
 * @param value  Numeric value
 * @return New expression node, or NULL on allocation failure
 */
LV00_PUBLIC_API Lv00SymExpr *sym_expr_create_const(double value);

/**
 * @brief Create a variable expression
 *
 * The var_name string is copied; the caller may free the original.
 *
 * @param var_name  Variable name (must not be NULL)
 * @return New expression node, or NULL on allocation failure
 */
LV00_PUBLIC_API Lv00SymExpr *sym_expr_create_var(const char *var_name);

/**
 * @brief Create a binary operation expression
 *
 * @param kind     Operation kind (must be SYM_ADD, SYM_MUL, or SYM_POW)
 * @param left     Left operand (ownership transferred to the new node)
 * @param right    Right operand (ownership transferred to the new node)
 * @return New expression node, or NULL on error
 */
LV00_PUBLIC_API Lv00SymExpr *sym_expr_create_binary(Lv00SymExprKind kind,
                                                    Lv00SymExpr *left,
                                                    Lv00SymExpr *right);

/**
 * @brief Create a unary operation expression
 *
 * @param kind     Operation kind (must be SYM_NEG, SYM_SIN, SYM_COS, SYM_SQRT, or SYM_LOG)
 * @param operand  Operand (ownership transferred to the new node)
 * @return New expression node, or NULL on error
 */
LV00_PUBLIC_API Lv00SymExpr *sym_expr_create_unary(Lv00SymExprKind kind,
                                                   Lv00SymExpr *operand);

/* ============================================================
 * Destruction
 * ============================================================ */

/**
 * @brief Destroy an expression tree and release all resources
 *
 * Recursively frees all children. Safe to call with NULL.
 *
 * @param expr  Expression to destroy
 */
LV00_PUBLIC_API void sym_expr_destroy(Lv00SymExpr *expr);

/* ============================================================
 * Simplification
 * ============================================================ */

/**
 * @brief Simplify an expression tree
 *
 * Applies algebraic simplification rules:
 *   - Constant folding (e.g. 2 + 3 -> 5)
 *   - Identity elimination (e.g. x + 0 -> x, x * 1 -> x)
 *   - Zero absorption (e.g. x * 0 -> 0)
 *   - Negation normalization (e.g. neg(neg(x)) -> x)
 *
 * Returns a new expression tree; the original is not modified.
 *
 * @param expr  Expression to simplify
 * @return Simplified expression (caller must destroy), or NULL on error
 */
LV00_PUBLIC_API Lv00SymExpr *sym_expr_simplify(const Lv00SymExpr *expr);

/* ============================================================
 * Evaluation
 * ============================================================ */

/**
 * @brief Evaluate an expression numerically
 *
 * Variables are looked up in the provided name/value arrays.
 * If a variable is not found, returns NaN.
 *
 * @param expr       Expression to evaluate
 * @param var_names  Array of variable names
 * @param var_values Array of variable values
 * @param var_count  Number of variables
 * @return Numerical result, or NaN on error or missing variable
 */
LV00_PUBLIC_API double sym_expr_eval_double(const Lv00SymExpr *expr,
                                            const char **var_names,
                                            const double *var_values,
                                            int var_count);

/* ============================================================
 * String representation
 * ============================================================ */

/**
 * @brief Convert an expression to a string
 *
 * Returns a heap-allocated string in infix notation.
 * Caller must free() the returned string.
 *
 * @param expr  Expression to convert
 * @return String representation, or NULL on error
 */
LV00_PUBLIC_API char *sym_expr_to_string(const Lv00SymExpr *expr);

/* ============================================================
 * Differentiation
 * ============================================================ */

/**
 * @brief Symbolic differentiation with respect to a variable
 *
 * Returns a new expression tree representing d(expr)/d(var_name).
 * The original expression is not modified.
 *
 * @param expr      Expression to differentiate
 * @param var_name  Variable name to differentiate with respect to
 * @return Derivative expression (caller must destroy), or NULL on error
 */
LV00_PUBLIC_API Lv00SymExpr *sym_expr_diff(const Lv00SymExpr *expr,
                                           const char *var_name);

/* ============================================================
 * Substitution
 * ============================================================ */

/**
 * @brief Substitute a variable with an expression
 *
 * Returns a new expression tree where all occurrences of var_name
 * are replaced with replacement. The original expression is not modified.
 *
 * @param expr         Expression to substitute in
 * @param var_name     Variable name to replace
 * @param replacement  Replacement expression (ownership NOT transferred;
 *                     the tree is deep-copied internally)
 * @return New expression with substitutions applied (caller must destroy),
 *         or NULL on error
 */
LV00_PUBLIC_API Lv00SymExpr *sym_expr_substitute(const Lv00SymExpr *expr,
                                                 const char *var_name,
                                                 const Lv00SymExpr *replacement);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SYM_EXPR_H */
