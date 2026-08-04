#ifndef LV_PROP_FORMULA_OPS_H
#define LV_PROP_FORMULA_OPS_H

#include "lv/prop_verifier.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct PropFormula PropFormula;

/* Operation function pointer types */
typedef bool (*PropFormulaEqualFn)(const PropFormula *a, const PropFormula *b);
typedef uint64_t (*PropFormulaHashFn)(const PropFormula *f);
typedef bool (*PropFormulaIsDescendantFn)(const PropFormula *child, const PropFormula *parent);

/* VTable for PropFormulaType operations */
typedef struct {
    PropFormulaEqualFn equal;
    PropFormulaHashFn hash;
    PropFormulaIsDescendantFn is_descendant;
} PropFormulaOps;

/* Get the VTable for a given PropFormulaType */
const PropFormulaOps *prop_formula_get_ops(PropFormulaType type);

#ifdef __cplusplus
}
#endif

#endif /* LV_PROP_FORMULA_OPS_H */