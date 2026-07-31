/**
 * @file inequality_reasoning_transform.c
 * @brief 不等式推理系统 —— 不等式变换（加减乘/传递/合并）
 */

#include "inequality_reasoning_internal.h"


lvInequality *lv_ineq_add(lvInequality *ineq, lvExpr *expr) {
    if (!ineq || !expr)
        return NULL;

    /* 不等式方向不变，两边都加上 expr */
    lvExpr *new_left = lv_expr_add(ineq->left, expr);
    lvExpr *new_right = lv_expr_add(ineq->right, expr);
    if (!new_left || !new_right) {
        lv_expr_destroy(&new_left);
        lv_expr_destroy(&new_right);
        return NULL;
    }

    lvInequality *result = lv_ineq_create(new_left, ineq->type, new_right);
    if (result)
        result->status = ineq->status;
    else {
        lv_expr_destroy(&new_left);
        lv_expr_destroy(&new_right);
    }
    return result;
}

/**
 * 不等式两边乘表达式：
 * - expr_sign > 0: 方向不变
 * - expr_sign < 0: 方向翻转
 * - expr_sign == 0: 返回 NULL（无效操作）
 */
lvInequality *lv_ineq_mul(lvInequality *ineq, lvExpr *expr, int expr_sign) {
    if (!ineq || !expr)
        return NULL;
    if (expr_sign == 0)
        return NULL;

    lvInequalityType new_type = ineq->type;
    if (expr_sign < 0) {
        new_type = ineq_negate_type(ineq->type);
    }

    /* 不等式两边都乘以 expr */
    lvExpr *new_left = lv_expr_mul(ineq->left, expr);
    lvExpr *new_right = lv_expr_mul(ineq->right, expr);
    if (!new_left || !new_right) {
        lv_expr_destroy(&new_left);
        lv_expr_destroy(&new_right);
        return NULL;
    }

    lvInequality *result = lv_ineq_create(new_left, new_type, new_right);
    if (result)
        result->status = ineq->status;
    else {
        lv_expr_destroy(&new_left);
        lv_expr_destroy(&new_right);
    }
    return result;
}

/**
 * 不等式取反：
 * left < right => left >= right
 * left <= right => left > right
 */
lvInequality *lv_ineq_negate(lvInequality *ineq) {
    if (!ineq)
        return NULL;

    lvInequalityType new_type = ineq_negate_type(ineq->type);
    lvInequality *result = lv_ineq_create(ineq->left, new_type, ineq->right);
    if (result) {
        result->status = ineq->status;
        if (result->status == INEQ_STATUS_PROVED)
            result->status = INEQ_STATUS_DISPROVED;
        else if (result->status == INEQ_STATUS_DISPROVED)
            result->status = INEQ_STATUS_PROVED;
    }
    return result;
}

/**
 * 不等式传递：
 * a < b, b < c => a < c
 * a <= b, b <= c => a <= c
 * a < b, b <= c => a < c
 * a <= b, b < c => a < c
 */
bool lv_ineq_transitive(lvInequality **ineqs, uint32_t count, lvInequality **out_result) {
    if (!ineqs || count < 2 || !out_result)
        return false;

    *out_result = NULL;

    /* 检查所有不等式是否同向 */
    for (uint32_t i = 1; i < count; i++) {
        if (!ineqs[i - 1] || !ineqs[i])
            return false;
        if (!ineq_same_direction(ineqs[i - 1]->type, ineqs[i]->type))
            return false;
    }

    /* 检查链式连接：ineqs[i].right == ineqs[i+1].left */
    for (uint32_t i = 0; i < count - 1; i++) {
        if (ineqs[i]->right != ineqs[i + 1]->left)
            return false;
    }

    /* 确定结果类型：如果任一为严格不等式，结果为严格 */
    lvInequalityType result_type = ineqs[0]->type;
    for (uint32_t i = 1; i < count; i++) {
        if (ineq_is_strict(ineqs[i]->type)) {
            if (ineq_is_less_family(ineqs[i]->type))
                result_type = INEQ_LESS_THAN;
            else
                result_type = INEQ_GREATER_THAN;
            break;
        }
    }

    *out_result = lv_ineq_create(ineqs[0]->left, result_type, ineqs[count - 1]->right);
    return (*out_result != NULL);
}

/**
 * 合并同向不等式：
 * a < c, b < d => a + b < c + d
 */
bool lv_ineq_merge(lvInequality **ineqs, uint32_t count, lvInequality **out_result) {
    if (!ineqs || count < 2 || !out_result)
        return false;

    *out_result = NULL;

    /* 检查所有不等式是否同向 */
    for (uint32_t i = 1; i < count; i++) {
        if (!ineqs[i - 1] || !ineqs[i])
            return false;
        if (!ineq_same_direction(ineqs[i - 1]->type, ineqs[i]->type))
            return false;
    }

    /* 确定结果类型 */
    lvInequalityType result_type = ineqs[0]->type;
    for (uint32_t i = 1; i < count; i++) {
        if (ineq_is_strict(ineqs[i]->type)) {
            if (ineq_is_less_family(ineqs[i]->type))
                result_type = INEQ_LESS_THAN;
            else
                result_type = INEQ_GREATER_THAN;
            break;
        }
    }

    /* 结果：left = sum of all lefts, right = sum of all rights */
    lvExpr **left_exprs = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    lvExpr **right_exprs = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    if (!left_exprs || !right_exprs) {
        lv_free((void **) &left_exprs);
        lv_free((void **) &right_exprs);
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        left_exprs[i] = ineqs[i]->left;
        right_exprs[i] = ineqs[i]->right;
    }
    lvExpr *left_sum = lv_expr_sum_n(left_exprs, count);
    lvExpr *right_sum = lv_expr_sum_n(right_exprs, count);
    lv_free((void **) &left_exprs);
    lv_free((void **) &right_exprs);
    if (!left_sum || !right_sum)
        return false;

    *out_result = lv_ineq_create(left_sum, result_type, right_sum);
    return (*out_result != NULL);
}

/* ============== 表达式符号判定 ============== */

/**
 * 通过不等式系统中的约束判定表达式符号
 *
 * 策略：
 * 1. 检查系统中的变量约束（x > 0, x >= 0, x < 0, x <= 0）
 * 2. 传播符号信息（正*正=正，正*负=负，负*负=正）
 * 3. 平方项总是非负
 */
