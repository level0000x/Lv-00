/**
 * @file inequality_reasoning_internal.h
 * @brief Internal shared definitions for the inequality reasoning module.
 */

#ifndef lv_INEQUALITY_REASONING_INTERNAL_H
#define lv_INEQUALITY_REASONING_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "inequality_reasoning.h"
#include "lv/lv_xmacro.h"
#include "lv_utils.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- shared helpers (defined in inequality_reasoning_core.c) ---- */
lvInequalityType ineq_negate_type(lvInequalityType t);
bool ineq_same_direction(lvInequalityType a, lvInequalityType b);
bool ineq_is_strict(lvInequalityType t);
bool ineq_is_non_strict(lvInequalityType t);
bool ineq_is_less_family(lvInequalityType t);
bool lv_expr_structurally_equal(const lvExpr *a, const lvExpr *b);
bool lv_ineq_structurally_equal(const lvInequality *a, const lvInequality *b);
lvInequalityProof *lv_ineq_make_proof(lvInequality *target, lvInequalityStatus status,
                                      lvInequalityMethod method, const char *justification,
                                      const char *error);

#ifdef __cplusplus
}
#endif

#endif /* lv_INEQUALITY_REASONING_INTERNAL_H */
