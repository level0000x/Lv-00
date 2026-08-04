/**
 * @file autodiff_vtable.h
 * @brief Virtual function table for AD expression operations.
 *
 * @details Provides a vtable-based dispatch mechanism for AD expression
 *          operations, replacing switch-on-kind with polymorphic dispatch.
 *          Each AD expression kind (CONST, VAR, ADD, MUL, NEG, SIN, COS, POW)
 *          has its own vtable instance with specialized implementations.
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date   2026-08-04
 */
#ifndef lv_AUTODIFF_VTABLE_H
#define lv_AUTODIFF_VTABLE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "autodiff.h"
#include <stddef.h>

/* ============================================================
 * Forward mode evaluation result
 * ============================================================ */

/**
 * @brief Result of a forward-mode evaluation: (value, tangent) pair.
 */
typedef struct {
    double value;   /**< Primal value */
    double tangent; /**< Tangent (derivative) value */
} ForwardResult;

/* ============================================================
 * AD expression operation vtable
 * ============================================================ */

/**
 * @brief Virtual function table for AD expression operations.
 *
 * Each lvADExprKind has a corresponding vtable instance with 4
 * function pointers implementing the expression's behavior for:
 * - forward_eval:     Forward-mode evaluation (value + tangent)
 * - reverse_forward:  Reverse-mode forward pass (primal value)
 * - reverse_backward: Reverse-mode backward pass (gradient propagation)
 * - store_values:     Store primal values during reverse-mode forward pass
 */
typedef struct lvADExprOps {
    /** Forward-mode evaluation: compute (value, tangent) pair */
    ForwardResult (*forward_eval)(const lvADExpr *expr, int var_index, double var_value);

    /** Reverse-mode forward pass: compute primal value */
    double (*reverse_forward)(const lvADExpr *expr, const double *var_values, size_t var_count);

    /** Reverse-mode backward pass: propagate gradient adjoint */
    void (*reverse_backward)(lvADExpr *expr, double adjoint);

    /** Store primal values in the expression tree */
    void (*store_values)(lvADExpr *expr, const double *var_values, size_t var_count);
} lvADExprOps;

/* ============================================================
 * Vtable lookup
 * ============================================================ */

/**
 * @brief Get the operation vtable for a given expression kind.
 *
 * @param kind  The expression kind (AD_CONST, AD_VAR, ...)
 * @return Pointer to the vtable, or NULL if kind is invalid
 */
const lvADExprOps *lv_ad_get_ops(lvADExprKind kind);

#ifdef __cplusplus
}
#endif
#endif /* lv_AUTODIFF_VTABLE_H */