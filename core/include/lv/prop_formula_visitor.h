/**
 * @file prop_formula_visitor.h
 * @brief Visitor pattern for PropFormula AST traversal
 *
 * Eliminates multiple switch-on-type dispatches in analysis functions
 * by providing a single dispatch point (prop_formula_accept) that routes
 * to the appropriate visitor method.
 */

#ifndef lv_PROP_FORMULA_VISITOR_H
#define lv_PROP_FORMULA_VISITOR_H

#include "lv/prop_verifier.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Visitor vtable for PropFormula nodes.
 *
 * Each method receives the current formula node and an opaque context pointer.
 * Unused methods can be set to NULL (the accept function will skip NULL methods).
 */
typedef struct PropFormulaVisitor {
    void (*visit_atom)(const PropFormula *f, void *context);
    void (*visit_binary)(const PropFormula *f, void *context);
    void (*visit_unary)(const PropFormula *f, void *context);
    void (*visit_constant)(const PropFormula *f, void *context);
} PropFormulaVisitor;

/**
 * @brief Dispatch a PropFormula node to the appropriate visitor method.
 *
 * Routes f->type to one of the four visitor methods:
 *   - PROP_ATOM        → visit_atom
 *   - PROP_CONJUNCTION / PROP_DISJUNCTION / PROP_IMPLICATION → visit_binary
 *   - PROP_NEGATION    → visit_unary
 *   - PROP_BOTTOM / PROP_TRUE → visit_constant
 *
 * @param f        The formula node to visit (may be NULL; silently ignored).
 * @param visitor  The visitor vtable (may be NULL; silently ignored).
 * @param context  Opaque pointer forwarded to the visitor method.
 */
void prop_formula_accept(const PropFormula *f, const PropFormulaVisitor *visitor, void *context);

#ifdef __cplusplus
}
#endif

#endif /* lv_PROP_FORMULA_VISITOR_H */