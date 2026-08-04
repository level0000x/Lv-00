/**
 * @file prop_formula_visitor.c
 * @brief Implementation of the PropFormula visitor dispatch.
 */

#include "lv/prop_formula_visitor.h"

void prop_formula_accept(const PropFormula *f, const PropFormulaVisitor *visitor, void *context) {
    if (!f || !visitor)
        return;

    switch (f->type) {
        case PROP_ATOM:
            if (visitor->visit_atom)
                visitor->visit_atom(f, context);
            break;
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            if (visitor->visit_binary)
                visitor->visit_binary(f, context);
            break;
        case PROP_NEGATION:
            if (visitor->visit_unary)
                visitor->visit_unary(f, context);
            break;
        case PROP_BOTTOM:
        case PROP_TRUE:
            if (visitor->visit_constant)
                visitor->visit_constant(f, context);
            break;
        default:
            break;
    }
}