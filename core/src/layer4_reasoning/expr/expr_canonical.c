/**
 * @file expr_canonical.c
 * @brief 符号表达式构造与操作实现
 *
 * @details 实现 lvExpr 的创建、销毁和基本组合操作。
 */

#include "lv/expr_canonical.h"

#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"
#include "lv/lv_xmacro.h" /* LV_DISPATCH / LV_DISPATCH_VOID */
#include "lv/rational.h" /* lv_rational_from_mpq（create_rational_mpq 互操作） */

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
    e->data.rational.value = lv_number_from_rational(num, den);
    return e;
}

lvExpr *lv_expr_create_rational_mpq(const mpq_t value) {
    if (!value)
        return NULL;
    lvExpr *e = expr_alloc();
    if (!e)
        return NULL;
    e->type = EXPR_TYPE_RATIONAL;
    lvRational *r = lv_rational_from_mpq(value);
    if (!r) {
        lv_free((void **) &e);
        return NULL;
    }
    e->data.rational.value = lv_number_from_lvRational(r);
    lv_rational_destroy(&r);
    if (!e->data.rational.value) {
        lv_free((void **) &e);
        return NULL;
    }
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

/* ── 各 type 的 destroy 实现 ── */

static void destroy_variable(lvExpr *e) { lv_free((void **)&e->data.variable.name); }
static void destroy_rational(lvExpr *e) {
    lv_number_destroy(e->data.rational.value);
    e->data.rational.value = NULL;
}
static void destroy_power(lvExpr *e) {
    (void)e; /* exempt: 浅树生态——公共 lv_expr_destroy 非递归销毁，power 子表达式
               由调用者管理（与 lambda_term 递归 destroy 语义不同，跨模块语义差异） */
}
static void destroy_composite(lvExpr *e) { lv_free((void **)&e->data.composite.operands); }
static void destroy_function(lvExpr *e) { lv_free((void **)&e->data.function.func_name); }

/* ── 各 type 的 copy 实现（深复制：副本子树与原件完全独立） ── */

static int copy_variable(const lvExpr *src, lvExpr *dst) {
    dst->data.variable.name = lv_strdup(src->data.variable.name);
    return dst->data.variable.name ? 0 : -1;
}

static int copy_rational(const lvExpr *src, lvExpr *dst) {
    dst->data.rational.value = lv_number_clone(src->data.rational.value);
    return dst->data.rational.value ? 0 : -1;
}

static int copy_power(const lvExpr *src, lvExpr *dst) {
    dst->data.power.base = lv_expr_copy(src->data.power.base);
    if (!dst->data.power.base) return -1;
    dst->data.power.exponent = lv_expr_copy(src->data.power.exponent);
    if (!dst->data.power.exponent) return -1;
    return 0;
}

static int copy_composite(const lvExpr *src, lvExpr *dst) {
    dst->data.composite.count = src->data.composite.count;
    dst->data.composite.operands = (lvExpr **)lv_malloc((size_t)src->data.composite.count * sizeof(lvExpr *));
    if (!dst->data.composite.operands) return -1;
    for (uint32_t i = 0; i < src->data.composite.count; i++) {
        dst->data.composite.operands[i] = lv_expr_copy(src->data.composite.operands[i]);
        if (!dst->data.composite.operands[i]) return -1;
    }
    return 0;
}

static int copy_function(const lvExpr *src, lvExpr *dst) {
    dst->data.function.func_name = lv_strdup(src->data.function.func_name);
    if (!dst->data.function.func_name) return -1;
    dst->data.function.argument = lv_expr_copy(src->data.function.argument);
    if (!dst->data.function.argument) return -1;
    return 0;
}

/* ── 统一调度表（C1-1：clone/copy/destroy VTable 样板收敛，判据 A） ── */

typedef void (*ExprDestroyFn)(lvExpr *e);
typedef int (*ExprCopyFn)(const lvExpr *src, lvExpr *dst);

static const ExprDestroyFn kExprDestroyTable[EXPR_TYPE_FUNCTION + 1] = {
    [EXPR_TYPE_VARIABLE] = destroy_variable,
    [EXPR_TYPE_RATIONAL] = destroy_rational,
    [EXPR_TYPE_POWER]    = destroy_power,
    [EXPR_TYPE_PRODUCT]  = destroy_composite,
    [EXPR_TYPE_SUM]      = destroy_composite,
    [EXPR_TYPE_FUNCTION] = destroy_function,
};

static const ExprCopyFn kExprCopyTable[EXPR_TYPE_FUNCTION + 1] = {
    [EXPR_TYPE_VARIABLE] = copy_variable,
    [EXPR_TYPE_RATIONAL] = copy_rational,
    [EXPR_TYPE_POWER]    = copy_power,
    [EXPR_TYPE_PRODUCT]  = copy_composite,
    [EXPR_TYPE_SUM]      = copy_composite,
    [EXPR_TYPE_FUNCTION] = copy_function,
};

/* ============== 表达式销毁/复制 ============== */

/**
 * @brief 递归释放整棵 lvExpr 子树（内部辅助，仅供 lv_expr_copy 失败路径清理）
 *
 * 语义契约：自顶向下对子树内每个节点依次调用其 destroy_data、释放 label 并 lv_free。
 * 前置条件：e 指向 lv_expr_create_* 家族分配的有效节点（NULL 安全）。
 * 失败/截断语义：无失败路径。
 * 边界行为：e == NULL → 无操作。
 * exempt: 此处的 type 分支是树结构遍历（不同节点类型的子字段不同），
 *         不属于 VTable 调度样板；公共 lv_expr_destroy 保持非递归浅销毁语义
 *         （浅树生态，子表达式由调用者管理），仅 copy 失败路径用递归清理。
 * 扩展点：新增表达式类型时须在此补充子树递归释放。
 */
static void expr_subtree_destroy(lvExpr *e) {
    if (!e)
        return;
    if (e->type == EXPR_TYPE_POWER) {
        expr_subtree_destroy(e->data.power.base);
        expr_subtree_destroy(e->data.power.exponent);
    } else if (e->type == EXPR_TYPE_PRODUCT || e->type == EXPR_TYPE_SUM) {
        for (uint32_t i = 0; i < e->data.composite.count; i++)
            expr_subtree_destroy(e->data.composite.operands[i]);
    } else if (e->type == EXPR_TYPE_FUNCTION) {
        expr_subtree_destroy(e->data.function.argument);
    }
    LV_DISPATCH_VOID(kExprDestroyTable, e->type, e);
    lv_free((void **) &e->label);
    lv_free((void **) &e);
}

void lv_expr_destroy(lvExpr **expr) {
    if (!expr || !*expr)
        return;
    lvExpr *e = *expr;

    /* 统一调度表分发（LV_DISPATCH_VOID：越界/NULL 槽自动跳过） */
    LV_DISPATCH_VOID(kExprDestroyTable, e->type, e);

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

    /* 统一调度表分发（fallback=-1：未知类型视为复制失败） */
    if (LV_DISPATCH(kExprCopyTable, expr->type, -1, expr, copy) != 0) {
        expr_subtree_destroy(copy);
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
    if (!lv_number_is_integer(expr->data.rational.value))
        return false;
    *out_val = lv_number_to_int(expr->data.rational.value);
    return true;
}
