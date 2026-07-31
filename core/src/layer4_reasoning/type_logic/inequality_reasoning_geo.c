/**
 * @file inequality_reasoning_geo.c
 * @brief 不等式推理系统 —— 几何不等式
 */

#include "inequality_reasoning_internal.h"


bool lv_ineq_triangle_area(lvExpr *a, lvExpr *b, lvExpr *c, lvExpr *area, lvInequality **out_ineq) {
    if (!a || !b || !c || !area)
        return false;

    if (!out_ineq)
        return true;
    *out_ineq = NULL;

    /* 构造半周长: p = (a + b + c) / 2 */
    lvExpr *a_plus_b = lv_expr_add(a, b);
    if (!a_plus_b)
        return false;
    lvExpr *sum_abc = lv_expr_add(a_plus_b, c);
    if (!sum_abc)
        return false;

    lvExpr *half = lv_expr_create_rational(1, 2);
    if (!half)
        return false;
    lvExpr *p = lv_expr_mul(sum_abc, half);
    if (!p)
        return false;

    /* 构造 p-a, p-b, p-c */
    lvExpr *minus_one = lv_expr_create_rational(-1, 1);
    if (!minus_one)
        return false;

    lvExpr *neg_a = lv_expr_mul(a, minus_one);
    lvExpr *neg_b = lv_expr_mul(b, minus_one);
    lvExpr *neg_c = lv_expr_mul(c, minus_one);
    if (!neg_a || !neg_b || !neg_c)
        return false;

    lvExpr *p_minus_a = lv_expr_add(p, neg_a);
    lvExpr *p_minus_b = lv_expr_add(p, neg_b);
    lvExpr *p_minus_c = lv_expr_add(p, neg_c);
    if (!p_minus_a || !p_minus_b || !p_minus_c)
        return false;

    /* 构造 Heron 表达式: p * (p-a) * (p-b) * (p-c) */
    lvExpr *heron_ab = lv_expr_mul(p_minus_a, p_minus_b);
    if (!heron_ab)
        return false;
    lvExpr *heron_abc = lv_expr_mul(heron_ab, p_minus_c);
    if (!heron_abc)
        return false;
    lvExpr *heron = lv_expr_mul(p, heron_abc);
    if (!heron)
        return false;

    /* 构造 area² */
    lvExpr *two = lv_expr_create_rational(2, 1);
    if (!two)
        return false;
    lvExpr *area_sq = lv_expr_power(area, two);
    if (!area_sq)
        return false;

    /* area² >= p(p-a)(p-b)(p-c) —— Heron 公式约束 */
    *out_ineq = lv_ineq_create(area_sq, INEQ_GREATER_EQUAL, heron);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Triangle-Area-Heron");
    }

    return (*out_ineq != NULL);
}

/**
 * Weitzenbock 不等式：
 * a² + b² + c² >= 4√3 * S
 *
 * （注：由于 GMP 仅支持有理数，4√3 不可精确表示为有理数。
 *  此处构造符号形式的右端，实际 sqrt(3) 因子由调用者在更高层验证。）
 */
bool lv_ineq_weitzenbock(lvExpr *a, lvExpr *b, lvExpr *c, lvExpr *area, lvInequality **out_ineq) {
    if (!a || !b || !c || !area)
        return false;

    if (!out_ineq)
        return true;
    *out_ineq = NULL;

    /* 构造左端: a² + b² + c² */
    lvExpr *two = lv_expr_create_rational(2, 1);
    if (!two)
        return false;

    lvExpr *a_sq = lv_expr_power(a, two);
    lvExpr *b_sq = lv_expr_power(b, two);
    lvExpr *c_sq = lv_expr_power(c, two);
    if (!a_sq || !b_sq || !c_sq)
        return false;

    lvExpr *a_sq_plus_b_sq = lv_expr_add(a_sq, b_sq);
    if (!a_sq_plus_b_sq)
        return false;
    lvExpr *left = lv_expr_add(a_sq_plus_b_sq, c_sq);
    if (!left)
        return false;

    /* 构造右端: 4 * sqrt(3) * S
     * 由于 sqrt(3) 非常数有理数，此处构造 sqrt3 变量符号:
     *   right = 4 * sqrt3_var * area
     * sqrt3_var 作为符号变量保留，调用者可绑定为 sqrt(3) 的有理逼近
     */
    lvExpr *four = lv_expr_create_rational(4, 1);
    if (!four)
        return false;

    /* 使用变量 "sqrt3" 作为占位符 */
    lvExpr *sqrt3_var = lv_expr_create_variable("sqrt3");
    if (!sqrt3_var)
        return false;

    lvExpr *four_sqrt3 = lv_expr_mul(four, sqrt3_var);
    if (!four_sqrt3)
        return false;
    lvExpr *right = lv_expr_mul(four_sqrt3, area);
    if (!right)
        return false;

    *out_ineq = lv_ineq_create(left, INEQ_GREATER_EQUAL, right);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Weitzenbock");
    }

    return (*out_ineq != NULL);
}

/**
 * Erdos-Mordell 不等式：
 * 对于三角形 ABC 内点 P，设 P 到三边距离为 p, q, r：
 * PA + PB + PC >= 2(p + q + r)
 */
bool lv_ineq_erdos_mordell(lvExpr *pa, lvExpr *pb, lvExpr *pc, lvExpr *p, lvExpr *q, lvExpr *r,
                           lvInequality **out_ineq) {
    if (!pa || !pb || !pc || !p || !q || !r)
        return false;

    if (!out_ineq)
        return true;
    *out_ineq = NULL;

    /* 构造左端: PA + PB + PC */
    lvExpr *pa_plus_pb = lv_expr_add(pa, pb);
    if (!pa_plus_pb)
        return false;
    lvExpr *left = lv_expr_add(pa_plus_pb, pc);
    if (!left)
        return false;

    /* 构造右端: 2 * (p + q + r) */
    lvExpr *p_plus_q = lv_expr_add(p, q);
    if (!p_plus_q)
        return false;
    lvExpr *sum_dist = lv_expr_add(p_plus_q, r);
    if (!sum_dist)
        return false;

    lvExpr *two = lv_expr_create_rational(2, 1);
    if (!two)
        return false;
    lvExpr *right = lv_expr_mul(two, sum_dist);
    if (!right)
        return false;

    *out_ineq = lv_ineq_create(left, INEQ_GREATER_EQUAL, right);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Erdos-Mordell");
    }

    return (*out_ineq != NULL);
}

/* ============== 不等式序列化 ============== */

