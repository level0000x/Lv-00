/**
 * @file autodiff.h
 * @brief Automatic differentiation engine (forward and reverse mode)
 *
 * @details Provides a lightweight automatic differentiation (AD) system
 *          supporting both forward mode and reverse mode (backpropagation).
 *          Inspired by Enzyme (LLVM-based AD) and PyTorch's autograd.
 *
 *          Expression kinds:
 *          - Constants and variables
 *          - Arithmetic: ADD, MUL, NEG, POW
 *          - Transcendental: SIN, COS
 *
 *          Forward mode: Propagates derivatives alongside values (efficient for
 *          functions with few inputs and many outputs).
 *
 *          Reverse mode: Builds a computation graph, then propagates gradients
 *          backwards (efficient for functions with many inputs and few outputs).
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#ifndef LV00_AUTODIFF_H
#define LV00_AUTODIFF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv00.h"

/* ============================================================
 * AD mode enumeration
 * ============================================================ */

/**
 * @brief Automatic differentiation mode.
 *
 * - AD_FORWARD: Forward mode (tangent propagation). Efficient for f: R^n -> R^m
 *   where n is small. Computes derivatives alongside the primal computation.
 *
 * - AD_REVERSE: Reverse mode (gradient backpropagation). Efficient for
 *   f: R^n -> R where n is large. Builds a computation graph and propagates
 *   adjoints backwards.
 */
typedef enum Lv00ADMode {
    AD_FORWARD = 0,
    AD_REVERSE = 1
} Lv00ADMode;

/* ============================================================
 * AD expression kind enumeration
 * ============================================================ */

/**
 * @brief Kinds of AD expression nodes.
 */
typedef enum Lv00ADExprKind {
    AD_CONST = 0,   /**< Constant value */
    AD_VAR   = 1,   /**< Variable (identified by var_index) */
    AD_ADD   = 2,   /**< Addition: children[0] + children[1] */
    AD_MUL   = 3,   /**< Multiplication: children[0] * children[1] */
    AD_NEG   = 4,   /**< Negation: -children[0] */
    AD_SIN   = 5,   /**< Sine: sin(children[0]) */
    AD_COS   = 6,   /**< Cosine: cos(children[0]) */
    AD_POW   = 7    /**< Power: children[0] ^ children[1] */
} Lv00ADExprKind;

/* ============================================================
 * AD expression node
 * ============================================================ */

/**
 * @brief A node in the AD expression tree / computation graph.
 *
 * For leaf nodes (AD_CONST, AD_VAR), children and child_count are empty.
 * For unary nodes (AD_NEG, AD_SIN, AD_COS), child_count = 1.
 * For binary nodes (AD_ADD, AD_MUL, AD_POW), child_count = 2.
 *
 * The gradient field is used during reverse mode to accumulate gradients.
 */
typedef struct Lv00ADExpr {
    Lv00ADExprKind   kind;         /**< Node kind */
    double           value;        /**< Primal value (set during evaluation) */
    int              var_index;    /**< Variable index (only for AD_VAR, -1 otherwise) */
    struct Lv00ADExpr **children;  /**< Child expression nodes */
    size_t           child_count;  /**< Number of children */
    double           gradient;     /**< Accumulated gradient (used in reverse mode) */
} Lv00ADExpr;

/* ============================================================
 * AD engine
 * ============================================================ */

/**
 * @brief Automatic differentiation engine.
 *
 * Holds the AD mode and manages expression allocation.
 */
typedef struct Lv00ADEngine {
    Lv00ADMode mode;  /**< Differentiation mode */
} Lv00ADEngine;

/* ============================================================
 * API: Engine lifecycle
 * ============================================================ */

/**
 * @brief Create a new AD engine.
 *
 * @param mode  The differentiation mode (AD_FORWARD or AD_REVERSE)
 * @return Pointer to the new engine, or NULL on failure
 */
LV00_PUBLIC_API Lv00ADEngine *ad_engine_create(Lv00ADMode mode);

/**
 * @brief Destroy an AD engine.
 *
 * @param engine  The engine to destroy (may be NULL)
 */
LV00_PUBLIC_API void ad_engine_destroy(Lv00ADEngine *engine);

/* ============================================================
 * API: Expression construction
 * ============================================================ */

/**
 * @brief Create a constant expression node.
 *
 * @param value  The constant value
 * @return Pointer to the new expression node, or NULL on failure
 */
LV00_PUBLIC_API Lv00ADExpr *ad_expr_create_const(double value);

/**
 * @brief Create a variable expression node.
 *
 * @param var_index  The variable index (0-based)
 * @return Pointer to the new expression node, or NULL on failure
 */
LV00_PUBLIC_API Lv00ADExpr *ad_expr_create_var(int var_index);

/**
 * @brief Create an addition expression: a + b.
 *
 * @param a  Left operand
 * @param b  Right operand
 * @return Pointer to the new expression node, or NULL on failure
 */
LV00_PUBLIC_API Lv00ADExpr *ad_expr_add(Lv00ADExpr *a, Lv00ADExpr *b);

/**
 * @brief Create a multiplication expression: a * b.
 *
 * @param a  Left operand
 * @param b  Right operand
 * @return Pointer to the new expression node, or NULL on failure
 */
LV00_PUBLIC_API Lv00ADExpr *ad_expr_mul(Lv00ADExpr *a, Lv00ADExpr *b);

/**
 * @brief Create a sine expression: sin(x).
 *
 * @param x  The operand
 * @return Pointer to the new expression node, or NULL on failure
 */
LV00_PUBLIC_API Lv00ADExpr *ad_expr_sin(Lv00ADExpr *x);

/**
 * @brief Create a cosine expression: cos(x).
 *
 * @param x  The operand
 * @return Pointer to the new expression node, or NULL on failure
 */
LV00_PUBLIC_API Lv00ADExpr *ad_expr_cos(Lv00ADExpr *x);

/**
 * @brief Create a power expression: base ^ exponent.
 *
 * @param base      The base
 * @param exponent  The exponent
 * @return Pointer to the new expression node, or NULL on failure
 */
LV00_PUBLIC_API Lv00ADExpr *ad_expr_pow(Lv00ADExpr *base, Lv00ADExpr *exponent);

/**
 * @brief Destroy an expression node and all its children (recursive).
 *
 * @param expr  The expression to destroy (may be NULL)
 */
LV00_PUBLIC_API void ad_expr_destroy(Lv00ADExpr *expr);

/* ============================================================
 * API: Differentiation
 * ============================================================ */

/**
 * @brief Evaluate an expression and compute its derivative using forward mode.
 *
 * Forward mode propagates the tangent (derivative) alongside the primal value.
 * For a function f(x), computes both f(x) and f'(x) in a single pass.
 *
 * @param expr       The expression to differentiate
 * @param var_index  The variable index to differentiate with respect to
 * @param var_value  The value of the variable
 * @param[out] value     The evaluated function value
 * @param[out] derivative The computed derivative
 * @return true on success, false on failure
 */
LV00_PUBLIC_API bool ad_forward_diff(Lv00ADExpr *expr, int var_index,
    double var_value, double *value, double *derivative);

/**
 * @brief Compute gradients using reverse mode (backpropagation).
 *
 * Builds the computation graph during evaluation, then propagates
 * gradients backwards from the output to all input variables.
 *
 * @param expr       The expression to differentiate
 * @param var_values Array of variable values (indexed by var_index)
 * @param var_count  Number of variables
 * @param[out] value     The evaluated function value
 * @param[out] gradients Array of gradients (caller-allocated, size >= var_count)
 * @return true on success, false on failure
 */
LV00_PUBLIC_API bool ad_reverse_diff(Lv00ADExpr *expr,
    const double *var_values, size_t var_count,
    double *value, double *gradients);

/* ============================================================
 * API: Evaluation and gradient query
 * ============================================================ */

/**
 * @brief Evaluate an expression given variable values.
 *
 * @param expr       The expression to evaluate
 * @param var_values Array of variable values (indexed by var_index)
 * @param var_count  Number of variables
 * @param[out] result  The evaluated result
 * @return true on success, false on failure
 */
LV00_PUBLIC_API bool ad_eval(Lv00ADExpr *expr,
    const double *var_values, size_t var_count, double *result);

/**
 * @brief Get the gradient of a specific variable after reverse differentiation.
 *
 * Must be called after ad_reverse_diff().
 *
 * @param expr       The expression node
 * @param var_index  The variable index to query
 * @return The gradient value, or 0.0 if not found
 */
LV00_PUBLIC_API double ad_grad(Lv00ADExpr *expr, int var_index);

#ifdef __cplusplus
}
#endif

#endif /* LV00_AUTODIFF_H */
