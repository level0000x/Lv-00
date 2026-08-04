/**
 * @file prop_formula_visitor.c
 * @brief Implementation of the PropFormula visitor dispatch.
 */

#include "lv/prop_formula_visitor.h"

/* ================================================================
 * 公式类型 -> 访问器类别 映射表（数据表化，替代 switch）
 * ================================================================ */

/* 访问器类别：标识 PropFormulaVisitor 中四类回调槽位 */
typedef enum {
    PROP_VISIT_NONE = -1,   /* 未知公式类型：不调用任何回调 */
    PROP_VISIT_ATOM,        /* visit_atom */
    PROP_VISIT_BINARY,      /* visit_binary */
    PROP_VISIT_UNARY,       /* visit_unary */
    PROP_VISIT_CONSTANT     /* visit_constant */
} PropFormulaVisitKind;

/** @brief 公式类型 → 访问器类别 映射表（按 PropFormulaType 枚举值升序） */
static const PropFormulaVisitKind s_formula_visit_kinds[] = {
    [PROP_ATOM]        = PROP_VISIT_ATOM,
    [PROP_CONJUNCTION] = PROP_VISIT_BINARY,
    [PROP_DISJUNCTION] = PROP_VISIT_BINARY,
    [PROP_IMPLICATION] = PROP_VISIT_BINARY,
    [PROP_NEGATION]    = PROP_VISIT_UNARY,
    [PROP_BOTTOM]      = PROP_VISIT_CONSTANT,
    [PROP_TRUE]        = PROP_VISIT_CONSTANT,
};

void prop_formula_accept(const PropFormula *f, const PropFormulaVisitor *visitor, void *context) {
    if (!f || !visitor)
        return;

    /* 通过查找表获取访问器类别，越界类型回退为不调用任何回调 */
    PropFormulaVisitKind kind = PROP_VISIT_NONE;
    if ((unsigned) f->type < sizeof(s_formula_visit_kinds) / sizeof(s_formula_visit_kinds[0]))
        kind = s_formula_visit_kinds[f->type];

    switch (kind) {
        case PROP_VISIT_ATOM:
            if (visitor->visit_atom)
                visitor->visit_atom(f, context);
            break;
        case PROP_VISIT_BINARY:
            if (visitor->visit_binary)
                visitor->visit_binary(f, context);
            break;
        case PROP_VISIT_UNARY:
            if (visitor->visit_unary)
                visitor->visit_unary(f, context);
            break;
        case PROP_VISIT_CONSTANT:
            if (visitor->visit_constant)
                visitor->visit_constant(f, context);
            break;
        default:
            break;
    }
}
