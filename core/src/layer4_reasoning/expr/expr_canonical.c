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

/* ============================================================
 * VTable 类型分发 — 消除 ExprType switch 反模式
 * ============================================================ */

/** @brief 表达式虚函数表条目（每个 type 一组操作） */
typedef struct {
    /** 释放 type 特有数据（不释放 lvExpr 结构体本身） */
    void (*destroy_data)(lvExpr *e);
    /** 复制 type 特有数据到 dst（已分配好，仅填充 data 字段） */
    int (*copy_data)(const lvExpr *src, lvExpr *dst);
} ExprVTableEntry;

/* ── 各 type 的 destroy 实现 ── */

static void destroy_variable(lvExpr *e) { lv_free((void **)&e->data.variable.name); }
static void destroy_rational(lvExpr *e) { mpq_clear(e->data.rational.value); }
static void destroy_power(lvExpr *e) { (void)e; /* 子表达式由调用者管理 */ }
static void destroy_composite(lvExpr *e) { lv_free((void **)&e->data.composite.operands); }
static void destroy_function(lvExpr *e) { lv_free((void **)&e->data.function.func_name); }

/* ── 各 type 的 copy 实现 ── */

static int copy_variable(const lvExpr *src, lvExpr *dst) {
    dst->data.variable.name = lv_strdup(src->data.variable.name);
    return dst->data.variable.name ? 0 : -1;
}

static int copy_rational(const lvExpr *src, lvExpr *dst) {
    mpq_init(dst->data.rational.value);
    mpq_set(dst->data.rational.value, src->data.rational.value);
    return 0;
}

static int copy_power(const lvExpr *src, lvExpr *dst) {
    dst->data.power.base = src->data.power.base;
    dst->data.power.exponent = src->data.power.exponent;
    return 0;
}

static int copy_composite(const lvExpr *src, lvExpr *dst) {
    dst->data.composite.count = src->data.composite.count;
    dst->data.composite.operands = (lvExpr **)lv_malloc((size_t)src->data.composite.count * sizeof(lvExpr *));
    if (!dst->data.composite.operands) return -1;
    for (uint32_t i = 0; i < src->data.composite.count; i++)
        dst->data.composite.operands[i] = src->data.composite.operands[i];
    return 0;
}

static int copy_function(const lvExpr *src, lvExpr *dst) {
    dst->data.function.func_name = lv_strdup(src->data.function.func_name);
    if (!dst->data.function.func_name) return -1;
    dst->data.function.argument = src->data.function.argument;
    return 0;
}

/* ── VTable 数组 ── */

static const ExprVTableEntry kExprVTables[EXPR_TYPE_FUNCTION + 1] = {
    [EXPR_TYPE_VARIABLE] = { destroy_variable, copy_variable },
    [EXPR_TYPE_RATIONAL] = { destroy_rational, copy_rational },
    [EXPR_TYPE_POWER]    = { destroy_power,    copy_power },
    [EXPR_TYPE_PRODUCT]  = { destroy_composite, copy_composite },
    [EXPR_TYPE_SUM]      = { destroy_composite, copy_composite },
    [EXPR_TYPE_FUNCTION] = { destroy_function,  copy_function },
};

static const ExprVTableEntry *expr_get_vtable(lvExprType type) {
    if (type < 0 || type > EXPR_TYPE_FUNCTION) return NULL;
    return &kExprVTables[type];
}

/* ============== 表达式销毁/复制 ============== */

void lv_expr_destroy(lvExpr **expr) {
    if (!expr || !*expr)
        return;
    lvExpr *e = *expr;

    /* VTable dispatch */
    const ExprVTableEntry *vt = expr_get_vtable(e->type);
    if (vt && vt->destroy_data)
        vt->destroy_data(e);

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

    /* VTable dispatch */
    const ExprVTableEntry *vt = expr_get_vtable(expr->type);
    if (vt && vt->copy_data) {
        if (vt->copy_data(expr, copy) != 0) {
            lv_free((void **) &copy);
            return NULL;
        }
    } else {
        /* 未知类型 */
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
