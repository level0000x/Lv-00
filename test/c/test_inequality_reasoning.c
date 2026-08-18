/**
 * @file test_inequality_reasoning.c
 * @brief 不等式推理 —— 表达式符号判定（P1-3 桩补齐回归测试）
 *
 * 覆盖 lv_expr_get_ops 的 sign 回调（expr_vtable.h）：
 * - rational_sign：正/负/零
 * - power_sign：正底数任意指数为正；负底数奇/偶指数
 * - product_sign：零因子、负因子奇偶、UNKNOWN 传播
 * - sum_sign：全正/全负/全零/混合/UNKNOWN
 * - function_sign：abs/sqrt 等恒非负函数
 * - var_sign：无系统约束返回 UNKNOWN
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/expr_canonical.h"
#include "lv/expr_vtable.h"
#include "lv/inequality_reasoning.h"

#define TEST_PASS_STATEMENT g_pass_count++
#define TEST_FAIL_STATEMENT g_fail_count++
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 符号判定测试 ============== */

/** 测试有理数符号判定 */
static void test_rational_sign(void) {
    lvExpr *pos = lv_expr_create_rational(3, 2);
    lvExpr *neg = lv_expr_create_rational(-7, 4);
    lvExpr *zero = lv_expr_create_rational(0, 1);

    TEST_ASSERT(lv_expr_get_ops(pos->type)->sign(pos, NULL) == SIGN_POSITIVE, "3/2 sign = POSITIVE");
    TEST_ASSERT(lv_expr_get_ops(neg->type)->sign(neg, NULL) == SIGN_NEGATIVE, "-7/4 sign = NEGATIVE");
    TEST_ASSERT(lv_expr_get_ops(zero->type)->sign(zero, NULL) == SIGN_ZERO, "0 sign = ZERO");

    lv_expr_destroy(&pos);
    lv_expr_destroy(&neg);
    lv_expr_destroy(&zero);
    PASS();
}

/** 测试幂符号判定 */
static void test_power_sign(void) {
    /* 2^3 = 8 > 0 */
    lvExpr *base2 = lv_expr_create_rational(2, 1);
    lvExpr *exp3 = lv_expr_create_rational(3, 1);
    lvExpr *pow_pos = lv_expr_power(base2, exp3);
    TEST_ASSERT(lv_expr_get_ops(pow_pos->type)->sign(pow_pos, NULL) == SIGN_POSITIVE, "2^3 sign = POSITIVE");
    lv_expr_destroy(&pow_pos);

    /* (-2)^3 = -8 < 0 */
    lvExpr *base_neg = lv_expr_create_rational(-2, 1);
    lvExpr *exp3b = lv_expr_create_rational(3, 1);
    lvExpr *pow_neg = lv_expr_power(base_neg, exp3b);
    TEST_ASSERT(lv_expr_get_ops(pow_neg->type)->sign(pow_neg, NULL) == SIGN_NEGATIVE, "(-2)^3 sign = NEGATIVE");
    lv_expr_destroy(&pow_neg);

    /* (-2)^2 = 4 > 0 */
    lvExpr *base_neg2 = lv_expr_create_rational(-2, 1);
    lvExpr *exp2 = lv_expr_create_rational(2, 1);
    lvExpr *pow_even = lv_expr_power(base_neg2, exp2);
    TEST_ASSERT(lv_expr_get_ops(pow_even->type)->sign(pow_even, NULL) == SIGN_POSITIVE, "(-2)^2 sign = POSITIVE");
    lv_expr_destroy(&pow_even);

    /* 变量底数 → UNKNOWN */
    lvExpr *var = lv_expr_create_variable("x");
    lvExpr *exp1 = lv_expr_create_rational(2, 1);
    lvExpr *pow_var = lv_expr_power(var, exp1);
    TEST_ASSERT(lv_expr_get_ops(pow_var->type)->sign(pow_var, NULL) == SIGN_UNKNOWN, "x^2 sign = UNKNOWN");
    lv_expr_destroy(&pow_var);

    PASS();
}

/** 测试乘积符号判定 */
static void test_product_sign(void) {
    /* 2 * 3 = 6 > 0 */
    lvExpr *a = lv_expr_create_rational(2, 1);
    lvExpr *b = lv_expr_create_rational(3, 1);
    lvExpr *prod_pos = lv_expr_mul(a, b);
    TEST_ASSERT(lv_expr_get_ops(prod_pos->type)->sign(prod_pos, NULL) == SIGN_POSITIVE, "2*3 sign = POSITIVE");
    lv_expr_destroy(&prod_pos); /* 释放 a、b */

    /* 2 * (-3) = -6 < 0 */
    lvExpr *a2 = lv_expr_create_rational(2, 1);
    lvExpr *neg3 = lv_expr_create_rational(-3, 1);
    lvExpr *prod_neg = lv_expr_mul(a2, neg3);
    TEST_ASSERT(lv_expr_get_ops(prod_neg->type)->sign(prod_neg, NULL) == SIGN_NEGATIVE, "2*(-3) sign = NEGATIVE");
    lv_expr_destroy(&prod_neg);

    /* 0 * x = 0 */
    lvExpr *zero = lv_expr_create_rational(0, 1);
    lvExpr *var = lv_expr_create_variable("x");
    lvExpr *prod_zero = lv_expr_mul(zero, var);
    TEST_ASSERT(lv_expr_get_ops(prod_zero->type)->sign(prod_zero, NULL) == SIGN_ZERO, "0*x sign = ZERO");
    lv_expr_destroy(&prod_zero);

    /* x * y → UNKNOWN */
    lvExpr *var2 = lv_expr_create_variable("x");
    lvExpr *var3 = lv_expr_create_variable("y");
    lvExpr *prod_var = lv_expr_mul(var2, var3);
    TEST_ASSERT(lv_expr_get_ops(prod_var->type)->sign(prod_var, NULL) == SIGN_UNKNOWN, "x*y sign = UNKNOWN");
    lv_expr_destroy(&prod_var);

    PASS();
}

/** 测试和符号判定 */
static void test_sum_sign(void) {
    /* 2 + 3 = 5 > 0 */
    lvExpr *a = lv_expr_create_rational(2, 1);
    lvExpr *b = lv_expr_create_rational(3, 1);
    lvExpr *sum_pos = lv_expr_add(a, b);
    TEST_ASSERT(lv_expr_get_ops(sum_pos->type)->sign(sum_pos, NULL) == SIGN_POSITIVE, "2+3 sign = POSITIVE");
    lv_expr_destroy(&sum_pos);

    /* -2 + (-3) = -5 < 0 */
    lvExpr *na = lv_expr_create_rational(-2, 1);
    lvExpr *nb = lv_expr_create_rational(-3, 1);
    lvExpr *sum_neg = lv_expr_add(na, nb);
    TEST_ASSERT(lv_expr_get_ops(sum_neg->type)->sign(sum_neg, NULL) == SIGN_NEGATIVE, "-2+-3 sign = NEGATIVE");
    lv_expr_destroy(&sum_neg);

    /* 2 + (-3) 混合 → UNKNOWN */
    lvExpr *a2 = lv_expr_create_rational(2, 1);
    lvExpr *nb2 = lv_expr_create_rational(-3, 1);
    lvExpr *sum_mix = lv_expr_add(a2, nb2);
    TEST_ASSERT(lv_expr_get_ops(sum_mix->type)->sign(sum_mix, NULL) == SIGN_UNKNOWN, "2+(-3) sign = UNKNOWN");
    lv_expr_destroy(&sum_mix);

    /* 2 + x → UNKNOWN */
    lvExpr *a3 = lv_expr_create_rational(2, 1);
    lvExpr *var = lv_expr_create_variable("x");
    lvExpr *sum_var = lv_expr_add(a3, var);
    TEST_ASSERT(lv_expr_get_ops(sum_var->type)->sign(sum_var, NULL) == SIGN_UNKNOWN, "2+x sign = UNKNOWN");
    lv_expr_destroy(&sum_var);

    PASS();
}

/** 测试函数符号判定 */
static void test_function_sign(void) {
    lvExpr *v1 = lv_expr_create_variable("t");
    lvExpr *abs_f = lv_expr_function("abs", v1);
    TEST_ASSERT(lv_expr_get_ops(abs_f->type)->sign(abs_f, NULL) == SIGN_NONNEGATIVE, "abs(t) sign = NONNEGATIVE");
    lv_expr_destroy(&abs_f);

    lvExpr *v2 = lv_expr_create_variable("t");
    lvExpr *sqrt_f = lv_expr_function("sqrt", v2);
    TEST_ASSERT(lv_expr_get_ops(sqrt_f->type)->sign(sqrt_f, NULL) == SIGN_NONNEGATIVE, "sqrt(t) sign = NONNEGATIVE");
    lv_expr_destroy(&sqrt_f);

    lvExpr *v3 = lv_expr_create_variable("t");
    lvExpr *sin_f = lv_expr_function("sin", v3);
    TEST_ASSERT(lv_expr_get_ops(sin_f->type)->sign(sin_f, NULL) == SIGN_UNKNOWN, "sin(t) sign = UNKNOWN");
    lv_expr_destroy(&sin_f);

    PASS();
}

/** 测试变量符号判定 */
static void test_var_sign(void) {
    lvExpr *var = lv_expr_create_variable("x");
    TEST_ASSERT(lv_expr_get_ops(var->type)->sign(var, NULL) == SIGN_UNKNOWN, "var sign without sys = UNKNOWN");
    lv_expr_destroy(&var);
    PASS();
}

/* ============== 主函数 ============== */

TEST_MAIN_BEGIN("Inequality Reasoning Sign Test Suite")
    printf("=== Lv-00 Inequality Reasoning Sign Test Suite ===\n\n");
    TEST_MAIN_RUN(test_rational_sign);
    TEST_MAIN_RUN(test_power_sign);
    TEST_MAIN_RUN(test_product_sign);
    TEST_MAIN_RUN(test_sum_sign);
    TEST_MAIN_RUN(test_function_sign);
    TEST_MAIN_RUN(test_var_sign);
TEST_MAIN_END()
