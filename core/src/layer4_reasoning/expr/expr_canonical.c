/**
 * @file expr_canonical.c
 * @brief 符号表达式构造与操作实现
 *
 * @details 实现 lvExpr 的创建、销毁和基本组合操作。
 */

#include "expr_canonical.h"

#include <stdlib.h>
#include <string.h>

#include "lv_utils.h"

/* ============== 内部辅助 ============== */

/** 分配并清零一个 lvExpr */
static lvExpr *expr_alloc(void) {
    return (lvExpr *) lv_calloc(1, sizeof(lvExpr));
}

/* ============== 表达式构造 ============== */

lvExpr *lv_expr_create_variable(const char *name) {
    if (!name)
        return NULL;
    lvExpr *e = expr_alloc();
    if (!e)
        return NULL;
    e->type = EXPR_TYPE_VARIABLE;
    e->data.variable.name = lv_strdup(name);
    if (!e->data.variable.name) {
        lv_free((void **) &e);
        return NULL;
    }
    return e;
}

lvExpr *lv_expr_create_rational(int64_t num, uint64_t den) {
    if (den == 0)
        return NULL;
    lvExpr *e = expr_alloc();
    if (!e)
        return NULL;
    e->type = EXPR_TYPE_RATIONAL;
    mpq_init(e->data.rational.value);
    mpq_set_si(e->data.rational.value, (signed long int) num, (unsigned long int) den);
    mpq_canonicalize(e->data.rational.value);
    return e;
}

lvExpr *lv_expr_create_rational_mpq(const mpq_t value) {
    if (!value)
        return NULL;
    lvExpr *e = expr_alloc();
    if (!e)
        return NULL;
    e->type = EXPR_TYPE_RATIONAL;
    mpq_init(e->data.rational.value);
    mpq_set(e->data.rational.value, value);
    return e;
}

lvExpr *lv_expr_power(lvExpr *base, lvExpr *exponent) {
    if (!base || !exponent)
        return NULL;
    lvExpr *e = expr_alloc();
    if (!e)
        return NULL;
    e->type = EXPR_TYPE_POWER;
    e->data.power.base = base;
    e->data.power.exponent = exponent;
    return e;
}

/** 创建二元复合表达式（SUM 或 PRODUCT） */
static lvExpr *expr_composite_binary(lvExprType type, lvExpr *a, lvExpr *b) {
    if (!a || !b)
        return NULL;
    lvExpr *e = expr_alloc();
    if (!e)
        return NULL;
    e->type = type;
    e->data.composite.count = 2;
    e->data.composite.operands = (lvExpr **) lv_malloc(2 * sizeof(lvExpr *));
    if (!e->data.composite.operands) {
        lv_free((void **) &e);
        return NULL;
    }
    e->data.composite.operands[0] = a;
    e->data.composite.operands[1] = b;
    return e;
}

lvExpr *lv_expr_add(lvExpr *a, lvExpr *b) {
    return expr_composite_binary(EXPR_TYPE_SUM, a, b);
}

lvExpr *lv_expr_mul(lvExpr *a, lvExpr *b) {
    return expr_composite_binary(EXPR_TYPE_PRODUCT, a, b);
}

/** 创建 N 元复合表达式 */
static lvExpr *expr_composite_n(lvExprType type, lvExpr **exprs, uint32_t count) {
    if (!exprs || count == 0)
        return NULL;
    for (uint32_t i = 0; i < count; i++) {
        if (!exprs[i])
            return NULL;
    }
    lvExpr *e = expr_alloc();
    if (!e)
        return NULL;
    e->type = type;
    e->data.composite.count = count;
    e->data.composite.operands = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    if (!e->data.composite.operands) {
        lv_free((void **) &e);
        return NULL;
    }
    for (uint32_t i = 0; i < count; i++) {
        e->data.composite.operands[i] = exprs[i];
    }
    return e;
}

lvExpr *lv_expr_sum_n(lvExpr **exprs, uint32_t count) {
    return expr_composite_n(EXPR_TYPE_SUM, exprs, count);
}

lvExpr *lv_expr_product_n(lvExpr **exprs, uint32_t count) {
    return expr_composite_n(EXPR_TYPE_PRODUCT, exprs, count);
}

lvExpr *lv_expr_function(const char *func_name, lvExpr *argument) {
    if (!func_name || !argument)
        return NULL;
    lvExpr *e = expr_alloc();
    if (!e)
        return NULL;
    e->type = EXPR_TYPE_FUNCTION;
    e->data.function.func_name = lv_strdup(func_name);
    if (!e->data.function.func_name) {
        lv_free((void **) &e);
        return NULL;
    }
    e->data.function.argument = argument;
    return e;
}

/* ============== 表达式销毁/复制 ============== */

void lv_expr_destroy(lvExpr **expr) {
    if (!expr || !*expr)
        return;
    lvExpr *e = *expr;

    switch (e->type) {
        case EXPR_TYPE_VARIABLE:
            lv_free((void **) &e->data.variable.name);
            break;
        case EXPR_TYPE_RATIONAL:
            mpq_clear(e->data.rational.value);
            break;
        case EXPR_TYPE_POWER:
            /* 不递归销毁子表达式，由调用者管理生命周期 */
            break;
        case EXPR_TYPE_PRODUCT:
        case EXPR_TYPE_SUM:
            /* 不递归销毁操作数，由调用者管理 */
            lv_free((void **) &e->data.composite.operands);
            break;
        case EXPR_TYPE_FUNCTION:
            lv_free((void **) &e->data.function.func_name);
            break;
        default:
            /* 未知类型：静默忽略（避免新增枚举值时内存泄漏） */
            break;
    }
    lv_free((void **) &e->label);
    lv_free((void **) expr);
}

lvExpr *lv_expr_copy(const lvExpr *expr) {
    if (!expr)
        return NULL;
    lvExpr *copy = expr_alloc();
    if (!copy)
        return NULL;
    copy->type = expr->type;

    switch (expr->type) {
        case EXPR_TYPE_VARIABLE:
            copy->data.variable.name = lv_strdup(expr->data.variable.name);
            if (!copy->data.variable.name) {
                lv_free((void **) &copy);
                return NULL;
            }
            break;
        case EXPR_TYPE_RATIONAL:
            mpq_init(copy->data.rational.value);
            mpq_set(copy->data.rational.value, expr->data.rational.value);
            break;
        case EXPR_TYPE_POWER:
            copy->data.power.base = expr->data.power.base;
            copy->data.power.exponent = expr->data.power.exponent;
            break;
        case EXPR_TYPE_PRODUCT:
        case EXPR_TYPE_SUM:
            copy->data.composite.count = expr->data.composite.count;
            copy->data.composite.operands =
                (lvExpr **) lv_malloc((size_t) expr->data.composite.count * sizeof(lvExpr *));
            if (!copy->data.composite.operands) {
                lv_free((void **) &copy);
                return NULL;
            }
            for (uint32_t i = 0; i < expr->data.composite.count; i++) {
                copy->data.composite.operands[i] = expr->data.composite.operands[i];
            }
            break;
        case EXPR_TYPE_FUNCTION:
            copy->data.function.func_name = lv_strdup(expr->data.function.func_name);
            if (!copy->data.function.func_name) {
                lv_free((void **) &copy);
                return NULL;
            }
            copy->data.function.argument = expr->data.function.argument;
            break;
        default:
            /* 未知类型：cast 无法复制，返回 NULL 表示失败 */
            lv_free((void **) &copy);
            return NULL;
    }

    if (expr->label) {
        copy->label = lv_strdup(expr->label);
    }
    return copy;
}

/* ============== 表达式查询 ============== */

bool lv_expr_is_constant(const lvExpr *expr) {
    return expr && expr->type == EXPR_TYPE_RATIONAL;
}

bool lv_expr_get_integer(const lvExpr *expr, int64_t *out_val) {
    if (!expr || expr->type != EXPR_TYPE_RATIONAL || !out_val)
        return false;
    if (mpz_cmp_ui(mpq_denref(expr->data.rational.value), 1) != 0)
        return false;
    *out_val = mpz_get_si(mpq_numref(expr->data.rational.value));
    return true;
}
