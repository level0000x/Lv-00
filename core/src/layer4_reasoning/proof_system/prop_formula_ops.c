/*
 * @file prop_formula_ops.c
 * @brief VTable dispatch for PropFormulaType operations
 * @details Centralizes type-specific logic for formula_equal, formula_hash,
 *          and formula_is_descendant into per-type handler functions.
 */

#include "lv/prop_formula_ops.h"
#include "prop_verifier_internal.h"

#include <string.h>

/* ============================================================
 * equal handlers (type-specific comparison)
 * ============================================================ */

static bool equal_atom(const PropFormula *a, const PropFormula *b) {
    (void)b; /* b is guaranteed to be same type as a by caller */
    return strcmp(a->data.atom.name, b->data.atom.name) == 0;
}

static bool equal_binary(const PropFormula *a, const PropFormula *b) {
    return formula_equal(a->data.binary.left, b->data.binary.left) &&
           formula_equal(a->data.binary.right, b->data.binary.right);
}

static bool equal_unary(const PropFormula *a, const PropFormula *b) {
    return formula_equal(a->data.unary.operand, b->data.unary.operand);
}

static bool equal_terminal(const PropFormula *a, const PropFormula *b) {
    (void)a;
    (void)b;
    /* PROP_BOTTOM / PROP_TRUE: type match is sufficient */
    return true;
}

/* ============================================================
 * hash handlers (type-specific hashing)
 * ============================================================ */

static uint64_t hash_atom(const PropFormula *f) {
    uint64_t h = (uint64_t)f->type * PROP_HASH_TYPE_MULTIPLIER;
    for (const char *s = f->data.atom.name; *s; s++)
        h = h * PROP_HASH_STRING_MULTIPLIER + (uint64_t)(unsigned char)*s;
    return h;
}

static uint64_t hash_binary(const PropFormula *f) {
    uint64_t h = (uint64_t)f->type * PROP_HASH_TYPE_MULTIPLIER;
    h ^= formula_hash(f->data.binary.left) * PROP_HASH_LEFT_MULTIPLIER;
    h ^= formula_hash(f->data.binary.right) * PROP_HASH_RIGHT_MULTIPLIER;
    return h;
}

static uint64_t hash_unary(const PropFormula *f) {
    uint64_t h = (uint64_t)f->type * PROP_HASH_TYPE_MULTIPLIER;
    h ^= formula_hash(f->data.unary.operand) * PROP_HASH_RIGHT_MULTIPLIER;
    return h;
}

static uint64_t hash_terminal(const PropFormula *f) {
    return (uint64_t)f->type * PROP_HASH_TYPE_MULTIPLIER;
}

/* ============================================================
 * is_descendant handlers (type-specific)
 * ============================================================ */

/* Forward declaration of the dispatch function used for recursion */
static bool is_descendant_dispatch(const PropFormula *child, const PropFormula *parent);

static bool is_descendant_binary(const PropFormula *child, const PropFormula *parent) {
    return is_descendant_dispatch(child, parent->data.binary.left) ||
           is_descendant_dispatch(child, parent->data.binary.right);
}

static bool is_descendant_unary(const PropFormula *child, const PropFormula *parent) {
    return is_descendant_dispatch(child, parent->data.unary.operand);
}

static bool is_descendant_false(const PropFormula *child, const PropFormula *parent) {
    (void)child;
    (void)parent;
    /* ATOM, BOTTOM, TRUE have no sub-formulas */
    return false;
}

/* Dispatch helper for is_descendant recursion */
static bool is_descendant_dispatch(const PropFormula *child, const PropFormula *parent) {
    if (!child || !parent)
        return false;
    if (child == parent)
        return true;
    const PropFormulaOps *ops = prop_formula_get_ops(parent->type);
    if (ops && ops->is_descendant)
        return ops->is_descendant(child, parent);
    return false;
}

/* ============================================================
 * VTable table definition
 * ============================================================ */

static const PropFormulaOps vtables[] = {
    [PROP_ATOM] = {
        .equal = equal_atom,
        .hash = hash_atom,
        .is_descendant = is_descendant_false,
    },
    [PROP_CONJUNCTION] = {
        .equal = equal_binary,
        .hash = hash_binary,
        .is_descendant = is_descendant_binary,
    },
    [PROP_DISJUNCTION] = {
        .equal = equal_binary,
        .hash = hash_binary,
        .is_descendant = is_descendant_binary,
    },
    [PROP_IMPLICATION] = {
        .equal = equal_binary,
        .hash = hash_binary,
        .is_descendant = is_descendant_binary,
    },
    [PROP_NEGATION] = {
        .equal = equal_unary,
        .hash = hash_unary,
        .is_descendant = is_descendant_unary,
    },
    [PROP_BOTTOM] = {
        .equal = equal_terminal,
        .hash = hash_terminal,
        .is_descendant = is_descendant_false,
    },
    [PROP_TRUE] = {
        .equal = equal_terminal,
        .hash = hash_terminal,
        .is_descendant = is_descendant_false,
    },
};

const PropFormulaOps *prop_formula_get_ops(PropFormulaType type) {
    if (type < 0 || type >= (PropFormulaType)(sizeof(vtables) / sizeof(vtables[0])))
        return NULL;
    /* Check if the vtable entry is initialized (all function pointers set) */
    if (!vtables[type].equal)
        return NULL;
    return &vtables[type];
}