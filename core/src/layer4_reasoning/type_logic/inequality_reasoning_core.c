/**
 * @file inequality_reasoning_core.c
 * @brief 不等式推理系统 —— 内部辅助与不等式创建/销毁
 */

#include "inequality_reasoning_internal.h"
#include "lv/expr_vtable.h"


/* ============== 内部辅助函数 ============== */

/** 不等式类型翻转映射 */
lvInequalityType ineq_negate_type(lvInequalityType t) {
    switch (t) {
        case INEQ_LESS_THAN:
            return INEQ_GREATER_THAN;
        case INEQ_LESS_EQUAL:
            return INEQ_GREATER_EQUAL;
        case INEQ_GREATER_THAN:
            return INEQ_LESS_THAN;
        case INEQ_GREATER_EQUAL:
            return INEQ_LESS_EQUAL;
        default:
            return t;
    }
}

/** 判断两个不等式是否同向（可合并） */
bool ineq_same_direction(lvInequalityType a, lvInequalityType b) {
    return (a == INEQ_LESS_THAN || a == INEQ_LESS_EQUAL) == (b == INEQ_LESS_THAN || b == INEQ_LESS_EQUAL);
}

/** 判断不等式是否为严格不等式 */
bool ineq_is_strict(lvInequalityType t) {
    return (t == INEQ_LESS_THAN || t == INEQ_GREATER_THAN);
}

/** 判断不等式是否为 <= 或 >= */
bool ineq_is_non_strict(lvInequalityType t) {
    return (t == INEQ_LESS_EQUAL || t == INEQ_GREATER_EQUAL);
}

/** 判断不等式类型是否为 <= 或 < */
bool ineq_is_less_family(lvInequalityType t) {
    return (t == INEQ_LESS_THAN || t == INEQ_LESS_EQUAL);
}

/* ============== lvExpr VTable 实现 ============== */

/* ---- 结构性比较（per-type） ---- */

static bool var_structurally_equal(const lvExpr *a, const lvExpr *b) {
    return (a->data.variable.name && b->data.variable.name &&
            strcmp(a->data.variable.name, b->data.variable.name) == 0);
}

static bool rational_structurally_equal(const lvExpr *a, const lvExpr *b) {
    return (mpq_equal(a->data.rational.value, b->data.rational.value) != 0);
}

static bool power_structurally_equal(const lvExpr *a, const lvExpr *b) {
    return (lv_expr_structurally_equal(a->data.power.base, b->data.power.base) &&
            lv_expr_structurally_equal(a->data.power.exponent, b->data.power.exponent));
}

static bool composite_structurally_equal(const lvExpr *a, const lvExpr *b) {
    if (a->data.composite.count != b->data.composite.count)
        return false;
    for (uint32_t i = 0; i < a->data.composite.count; i++) {
        if (!lv_expr_structurally_equal(a->data.composite.operands[i], b->data.composite.operands[i]))
            return false;
    }
    return true;
}

static bool function_structurally_equal(const lvExpr *a, const lvExpr *b) {
    return (a->data.function.func_name && b->data.function.func_name &&
            strcmp(a->data.function.func_name, b->data.function.func_name) == 0 &&
            lv_expr_structurally_equal(a->data.function.argument, b->data.function.argument));
}

/* ---- 符号判定（per-type） ---- */

static lvSign var_sign(const lvExpr *expr, const lvInequalitySystem *sys) {
    (void)expr;
    (void)sys;
    return SIGN_UNKNOWN;
}

static lvSign rational_sign(const lvExpr *expr, const lvInequalitySystem *sys) {
    (void)sys;
    int cmp = mpq_sgn(expr->data.rational.value);
    if (cmp > 0) return SIGN_POSITIVE;
    if (cmp < 0) return SIGN_NEGATIVE;
    return SIGN_ZERO;
}

static lvSign power_sign(const lvExpr *expr, const lvInequalitySystem *sys) {
    (void)expr;
    (void)sys;
    /* 简化：幂的符号由底数和指数共同决定，暂返回 UNKNOWN */
    return SIGN_UNKNOWN;
}

static lvSign product_sign(const lvExpr *expr, const lvInequalitySystem *sys) {
    (void)expr;
    (void)sys;
    /* 简化：乘积的符号需要递归分析各因子，暂返回 UNKNOWN */
    return SIGN_UNKNOWN;
}

static lvSign sum_sign(const lvExpr *expr, const lvInequalitySystem *sys) {
    (void)expr;
    (void)sys;
    /* 简化：和的符号需要分析各项，暂返回 UNKNOWN */
    return SIGN_UNKNOWN;
}

static lvSign function_sign(const lvExpr *expr, const lvInequalitySystem *sys) {
    (void)expr;
    (void)sys;
    return SIGN_UNKNOWN;
}

/* ---- VTable 静态实例 ---- */

static const lvExprOps var_ops = {
    .structurally_equal = var_structurally_equal,
    .sign = var_sign
};

static const lvExprOps rational_ops = {
    .structurally_equal = rational_structurally_equal,
    .sign = rational_sign
};

static const lvExprOps power_ops = {
    .structurally_equal = power_structurally_equal,
    .sign = power_sign
};

static const lvExprOps product_ops = {
    .structurally_equal = composite_structurally_equal,
    .sign = product_sign
};

static const lvExprOps sum_ops = {
    .structurally_equal = composite_structurally_equal,
    .sign = sum_sign
};

static const lvExprOps function_ops = {
    .structurally_equal = function_structurally_equal,
    .sign = function_sign
};

/** 表达式类型到 vtable 的查找表 */
const lvExprOps *lv_expr_get_ops(lvExprType type) {
    static const lvExprOps *const table[6] = {
        [EXPR_TYPE_VARIABLE] = &var_ops,
        [EXPR_TYPE_RATIONAL] = &rational_ops,
        [EXPR_TYPE_POWER]    = &power_ops,
        [EXPR_TYPE_PRODUCT]  = &product_ops,
        [EXPR_TYPE_SUM]      = &sum_ops,
        [EXPR_TYPE_FUNCTION] = &function_ops
    };
    unsigned int idx = (unsigned int)type;
    if (idx >= 6)
        return NULL;
    return table[idx];
}

/** 结构性比较两个表达式（递归深度优先，vtable 分发） */
bool lv_expr_structurally_equal(const lvExpr *a, const lvExpr *b) {
    /* 空指针处理 */
    if (a == b)
        return true;
    if (!a || !b)
        return false;

    /* 类型必须一致 */
    if (a->type != b->type)
        return false;

    const lvExprOps *ops = lv_expr_get_ops(a->type);
    if (!ops)
        return false;
    return ops->structurally_equal(a, b);
}

/** 结构性比较两个不等式 */
bool lv_ineq_structurally_equal(const lvInequality *a, const lvInequality *b) {
    if (!a || !b)
        return false;
    return (a->type == b->type && lv_expr_structurally_equal(a->left, b->left) &&
            lv_expr_structurally_equal(a->right, b->right));
}

/** 创建带错误消息的证明结构 */
lvInequalityProof *lv_ineq_make_proof(lvInequality *target, lvInequalityStatus status, lvInequalityMethod method,
                                             const char *justification, const char *error) {
    lvInequalityProof *p = (lvInequalityProof *) lv_calloc(1, sizeof(lvInequalityProof));
    if (!p)
        return NULL;

    p->target = target;
    p->status = status;
    p->step_count = 1;
    p->step_capacity = 1;
    p->steps = (lvInequalityStep *) lv_calloc(1, sizeof(lvInequalityStep));
    if (!p->steps) {
        lv_free((void **) &p);
        return NULL;
    }

    p->steps[0].method = method;
    p->steps[0].ineq = target;
    if (justification) {
        p->steps[0].justification = lv_strdup(justification);
    }
    if (error) {
        p->error_message = lv_strdup(error);
    }

    return p;
}

/* ============== 不等式创建/销毁 ============== */

lvInequality *lv_ineq_create(lvExpr *left, lvInequalityType type, lvExpr *right) {
    lvInequality *ineq = (lvInequality *) lv_calloc(1, sizeof(lvInequality));
    if (!ineq)
        return NULL;
    ineq->left = left;
    ineq->right = right;
    ineq->type = type;
    ineq->status = INEQ_STATUS_UNPROVED;
    ineq->label = NULL;
    return ineq;
}

void lv_ineq_destroy(lvInequality *ineq) {
    if (!ineq)
        return;
    /* 注意：不释放 left/right 表达式，由调用者管理 */
    lv_free((void **) &ineq->label);
    lv_free((void **) &ineq);
}

lvInequality *lv_ineq_copy(const lvInequality *ineq) {
    if (!ineq)
        return NULL;
    lvInequality *copy = (lvInequality *) lv_calloc(1, sizeof(lvInequality));
    if (!copy)
        return NULL;
    copy->left = ineq->left;
    copy->right = ineq->right;
    copy->type = ineq->type;
    copy->status = ineq->status;
    if (ineq->label) {
        copy->label = (char *) lv_malloc(strlen(ineq->label) + 1);
        if (copy->label)
            snprintf(copy->label, strlen(ineq->label) + 1, "%s", ineq->label);
    }
    return copy;
}

lvInequalitySystem *lv_ineq_system_create(void) {
    lvInequalitySystem *sys = (lvInequalitySystem *) lv_calloc(1, sizeof(lvInequalitySystem));
    if (sys)
        lv_darray_init(&sys->inequalities, sizeof(lvInequality *));
    return sys;
}

void lv_ineq_system_destroy(lvInequalitySystem *sys) {
    if (!sys)
        return;
    for (int i = 0; i < sys->inequalities.count; i++) {
        lvInequality **p = (lvInequality **)lv_darray_get(&sys->inequalities, i);
        if (p && *p)
            lv_ineq_destroy(*p);
    }
    lv_darray_free(&sys->inequalities);
    lv_free((void **) &sys->variables);
    lv_free((void **) &sys);
}

bool lv_ineq_system_add(lvInequalitySystem *sys, lvInequality *ineq) {
    if (!sys || !ineq)
        return false;

    if (lv_darray_push(&sys->inequalities, &ineq) < 0)
        return false;
    return true;
}

bool lv_ineq_system_add_var_constraint(lvInequalitySystem *sys, lvExpr *var, lvInequalityType type, const mpq_t value) {
    if (!sys || !var)
        return false;

    /* 创建不等式: var <type> value */
    lvExpr *val_expr = lv_expr_create_rational_mpq(value);
    if (!val_expr)
        return false;

    lvInequality *ineq = lv_ineq_create(var, type, val_expr);
    if (!ineq) {
        lv_expr_destroy(&val_expr);
        return false;
    }

    return lv_ineq_system_add(sys, ineq);
}

/* ============== 基本不等式证明 ============== */

/**
 * @brief 证明不等式：检查系统中的约束是否足以推导目标不等式
 *
 * 策略：
 * 1. 如果 left == right，则等式成立
 * 2. 遍历系统约束，尝试传递链推导
 * 3. 检查变量约束（如 x > 0）是否支持推导
 */
