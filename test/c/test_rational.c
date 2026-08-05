/**
 * @file test_rational.c
 * @brief lvRational 模块测试套件
 *
 * 测试 lvRational 的所有公共 API，涵盖：
 * - 生命周期管理（create/destroy/clone）
 * - 赋值操作（set_zero/set_one）
 * - 算术运算（add/sub/mul/div/neg/inv/abs，含原地版本）
 * - 比较操作（cmp）
 * - 谓词函数（is_zero/is_one/is_integer/sgn）
 * - 规范化（simplify）
 * - 转换与序列化（to_double/to_string）
 * - 边界条件（除零）
 *
 * @author Lv-00 Project
 * @date 2026-05-24
 */

/* [QA] Uses double for test assertions against GMP mpq_t via comparison helpers. Acceptable in test code. */

#include <gmp.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "rational.h"
#include "test_helpers.h"

/* ============================================================
 * 全局测试计数器
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 辅助宏：释放 to_string 返回的字符串
 * ============================================================ */
#ifndef SAFE_FREE_STR
#define SAFE_FREE_STR(s) \
    do {                 \
        if (s)           \
            lv_free((void **)&(s)); \
    } while (0)
#endif

/* ============================================================
 * 测试用例
 * ============================================================ */

/**
 * @brief 测试 rational 的创建与销毁
 *
 * 验证 lv_rational_create() 返回非 NULL 指针，
 * 且 lv_rational_destroy() 能正确释放资源。
 */
static void test_rational_create(void) {
    lvRational *r = lv_rational_create();
    TEST_ASSERT_NOT_NULL(r);
    lv_rational_destroy(&r);
    TEST_ASSERT_NULL(r);
}

/**
 * @brief 测试从 C 整数创建有理数
 *
 * 使用 lv_rational_create_from_si(3, 4) 创建 3/4，
 * 验证 lv_rational_to_string() 输出为 "3/4"。
 */
static void test_rational_create_from_si(void) {
    lvRational *r = lv_rational_create_from_si(3, 4);
    TEST_ASSERT_NOT_NULL(r);

    char *s = lv_rational_to_string(r);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "3/4");

    SAFE_FREE_STR(s);
    lv_rational_destroy(&r);
}

/**
 * @brief 测试 set_zero 和 set_one 赋值操作
 *
 * 创建有理数后分别调用 set_zero 和 set_one，
 * 通过 to_string 验证结果。
 */
static void test_rational_set_zero_one(void) {
    lvRational *r = lv_rational_create();
    TEST_ASSERT_NOT_NULL(r);

    /* 测试 set_one */
    lv_rational_set_one(r);
    char *s1 = lv_rational_to_string(r);
    TEST_ASSERT_NOT_NULL(s1);
    TEST_ASSERT_STR_EQ(s1, "1");
    SAFE_FREE_STR(s1);

    /* 测试 set_zero */
    lv_rational_set_zero(r);
    char *s0 = lv_rational_to_string(r);
    TEST_ASSERT_NOT_NULL(s0);
    TEST_ASSERT_STR_EQ(s0, "0");
    SAFE_FREE_STR(s0);

    lv_rational_destroy(&r);
}

/**
 * @brief 测试克隆操作
 *
 * 创建有理数 5/7，克隆后使用 lv_rational_cmp
 * 验证两者相等（返回 0）。
 */
static void test_rational_clone(void) {
    lvRational *orig = lv_rational_create_from_si(5, 7);
    TEST_ASSERT_NOT_NULL(orig);

    lvRational *copy = lv_rational_clone(orig);
    TEST_ASSERT_NOT_NULL(copy);

    int cmp = lv_rational_cmp(orig, copy);
    TEST_ASSERT_EQ(cmp, 0);

    lv_rational_destroy(&orig);
    lv_rational_destroy(&copy);
}

/**
 * @brief 测试精确加法: 1/2 + 1/3 = 5/6
 */
static void test_rational_add(void) {
    lvRational *a = lv_rational_create_from_si(1, 2);
    lvRational *b = lv_rational_create_from_si(1, 3);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    lvRational *sum = lv_rational_add(a, b);
    TEST_ASSERT_NOT_NULL(sum);

    char *s = lv_rational_to_string(sum);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "5/6");

    SAFE_FREE_STR(s);
    lv_rational_destroy(&a);
    lv_rational_destroy(&b);
    lv_rational_destroy(&sum);
}

/**
 * @brief 测试精确减法: 3/4 - 1/4 = 1/2
 */
static void test_rational_sub(void) {
    lvRational *a = lv_rational_create_from_si(3, 4);
    lvRational *b = lv_rational_create_from_si(1, 4);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    lvRational *diff = lv_rational_sub(a, b);
    TEST_ASSERT_NOT_NULL(diff);

    char *s = lv_rational_to_string(diff);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "1/2");

    SAFE_FREE_STR(s);
    lv_rational_destroy(&a);
    lv_rational_destroy(&b);
    lv_rational_destroy(&diff);
}

/**
 * @brief 测试精确乘法: 2/3 * 3/4 = 1/2
 */
static void test_rational_mul(void) {
    lvRational *a = lv_rational_create_from_si(2, 3);
    lvRational *b = lv_rational_create_from_si(3, 4);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    lvRational *prod = lv_rational_mul(a, b);
    TEST_ASSERT_NOT_NULL(prod);

    char *s = lv_rational_to_string(prod);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "1/2");

    SAFE_FREE_STR(s);
    lv_rational_destroy(&a);
    lv_rational_destroy(&b);
    lv_rational_destroy(&prod);
}

/**
 * @brief 测试精确除法: 1/2 / 1/4 = 2/1
 */
static void test_rational_div(void) {
    lvRational *a = lv_rational_create_from_si(1, 2);
    lvRational *b = lv_rational_create_from_si(1, 4);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    lvRational *quot = lv_rational_div(a, b);
    TEST_ASSERT_NOT_NULL(quot);

    char *s = lv_rational_to_string(quot);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "2");

    SAFE_FREE_STR(s);
    lv_rational_destroy(&a);
    lv_rational_destroy(&b);
    lv_rational_destroy(&quot);
}

/**
 * @brief 测试取相反数: neg(3/4) = -3/4
 */
static void test_rational_neg(void) {
    lvRational *a = lv_rational_create_from_si(3, 4);
    TEST_ASSERT_NOT_NULL(a);

    lvRational *neg_a = lv_rational_neg(a);
    TEST_ASSERT_NOT_NULL(neg_a);

    char *s = lv_rational_to_string(neg_a);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "-3/4");

    SAFE_FREE_STR(s);
    lv_rational_destroy(&a);
    lv_rational_destroy(&neg_a);
}

/**
 * @brief 测试取倒数: inv(2/3) = 3/2
 */
static void test_rational_inv(void) {
    lvRational *a = lv_rational_create_from_si(2, 3);
    TEST_ASSERT_NOT_NULL(a);

    lvRational *inv_a = lv_rational_inv(a);
    TEST_ASSERT_NOT_NULL(inv_a);

    char *s = lv_rational_to_string(inv_a);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "3/2");

    SAFE_FREE_STR(s);
    lv_rational_destroy(&a);
    lv_rational_destroy(&inv_a);
}

/**
 * @brief 测试取绝对值: abs(-3/4) = 3/4
 */
static void test_rational_abs(void) {
    lvRational *a = lv_rational_create_from_si(-3, 4);
    TEST_ASSERT_NOT_NULL(a);

    lvRational *abs_a = lv_rational_abs(a);
    TEST_ASSERT_NOT_NULL(abs_a);

    char *s = lv_rational_to_string(abs_a);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "3/4");

    SAFE_FREE_STR(s);
    lv_rational_destroy(&a);
    lv_rational_destroy(&abs_a);
}

/**
 * @brief 测试比较操作: cmp(1/2, 2/3) < 0
 *
 * 1/2 = 0.5, 2/3 ~ 0.667，因此 1/2 < 2/3，
 * lv_rational_cmp 应返回负值。
 */
static void test_rational_compare(void) {
    lvRational *a = lv_rational_create_from_si(1, 2);
    lvRational *b = lv_rational_create_from_si(2, 3);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    int cmp = lv_rational_cmp(a, b);
    TEST_ASSERT_MSG(cmp < 0, "cmp(1/2, 2/3) should be negative");

    lv_rational_destroy(&a);
    lv_rational_destroy(&b);
}

/**
 * @brief 测试谓词检测函数
 *
 * 验证 is_zero、is_one、is_integer 以及 sgn 的返回值。
 */
static void test_rational_is_functions(void) {
    /* 测试零 */
    lvRational *zero = lv_rational_create();
    TEST_ASSERT_NOT_NULL(zero);
    lv_rational_set_zero(zero);

    TEST_ASSERT_MSG(lv_rational_is_zero(zero), "0 should be is_zero");
    TEST_ASSERT_MSG(!lv_rational_is_one(zero), "0 should not be is_one");
    TEST_ASSERT_MSG(lv_rational_is_integer(zero), "0 should be is_integer");
    TEST_ASSERT_EQ(lv_rational_sgn(zero), 0);

    /* 测试一 */
    lvRational *one = lv_rational_create();
    TEST_ASSERT_NOT_NULL(one);
    lv_rational_set_one(one);

    TEST_ASSERT_MSG(!lv_rational_is_zero(one), "1 should not be is_zero");
    TEST_ASSERT_MSG(lv_rational_is_one(one), "1 should be is_one");
    TEST_ASSERT_MSG(lv_rational_is_integer(one), "1 should be is_integer");
    TEST_ASSERT_MSG(lv_rational_sgn(one) > 0, "1 should have positive sign");

    /* 测试正分数 3/2 */
    lvRational *pos = lv_rational_create_from_si(3, 2);
    TEST_ASSERT_NOT_NULL(pos);

    TEST_ASSERT_MSG(!lv_rational_is_zero(pos), "3/2 should not be is_zero");
    TEST_ASSERT_MSG(!lv_rational_is_one(pos), "3/2 should not be is_one");
    TEST_ASSERT_MSG(!lv_rational_is_integer(pos), "3/2 should not be is_integer");
    TEST_ASSERT_MSG(lv_rational_sgn(pos) > 0, "3/2 should have positive sign");

    /* 测试负分数 -3/2 */
    lvRational *neg = lv_rational_create_from_si(-3, 2);
    TEST_ASSERT_NOT_NULL(neg);

    TEST_ASSERT_MSG(!lv_rational_is_zero(neg), "-3/2 should not be is_zero");
    TEST_ASSERT_MSG(!lv_rational_is_one(neg), "-3/2 should not be is_one");
    TEST_ASSERT_MSG(!lv_rational_is_integer(neg), "-3/2 should not be is_integer");
    TEST_ASSERT_MSG(lv_rational_sgn(neg) < 0, "-3/2 should have negative sign");

    /* 测试整数 4/1 */
    lvRational *intval = lv_rational_create_from_si(4, 1);
    TEST_ASSERT_NOT_NULL(intval);

    TEST_ASSERT_MSG(!lv_rational_is_zero(intval), "4 should not be is_zero");
    TEST_ASSERT_MSG(!lv_rational_is_one(intval), "4 should not be is_one");
    TEST_ASSERT_MSG(lv_rational_is_integer(intval), "4 should be is_integer");

    lv_rational_destroy(&zero);
    lv_rational_destroy(&one);
    lv_rational_destroy(&pos);
    lv_rational_destroy(&neg);
    lv_rational_destroy(&intval);
}

/**
 * @brief 测试有理数化简
 *
 * 创建一个分子分母未化简的有理数 2/4（直接操作 mpz_t 字段绕过自动化简），
 * 调用 lv_rational_simplify() 后验证约分为 1/2。
 */
static void test_rational_simplify(void) {
    lvRational *r = lv_rational_create();
    TEST_ASSERT_NOT_NULL(r);

    /* 直接设置分子为 2，分母为 4（未经化简） */
    mpq_set_si(r->value, 2, 4);

    lv_rational_simplify(r);

    char *s = lv_rational_to_string(r);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "1/2");

    SAFE_FREE_STR(s);
    lv_rational_destroy(&r);
}

/**
 * @brief 测试转换为 double: 1/2 约等于 0.5
 *
 * 使用 lv_rational_to_double 输出近似值，
 * 验证误差在合理范围内。
 */
static void test_rational_to_double(void) {
    lvRational *r = lv_rational_create_from_si(1, 2);
    TEST_ASSERT_NOT_NULL(r);

    double val = 0.0;
    int loss_bits = 0;
    bool ok = lv_rational_to_double(r, &val, &loss_bits);
    TEST_ASSERT_MSG(ok, "to_double should succeed");

    TEST_ASSERT_MSG(fabs(val - 0.5) < 1e-15, "1/2 should be approximately 0.5");

    lv_rational_destroy(&r);
}

/**
 * @brief 测试除零边界条件
 *
 * 验证 lv_rational_div(a, zero) 返回 NULL，
 * 且 lv_rational_create_from_si(num, 0) 返回 NULL。
 */
static void test_rational_div_by_zero(void) {
    /* 测试 create_from_si 除法分母为 0 的情况 */
    lvRational *bad = lv_rational_create_from_si(1, 0);
    TEST_ASSERT_NULL(bad);

    /* 测试除法运算除数为零的情况 */
    lvRational *a = lv_rational_create_from_si(1, 2);
    TEST_ASSERT_NOT_NULL(a);

    lvRational *zero = lv_rational_create();
    TEST_ASSERT_NOT_NULL(zero);

    lvRational *result = lv_rational_div(a, zero);
    TEST_ASSERT_NULL(result);

    /* 测试原地除法除数为零应返回 false */
    lvRational *b = lv_rational_create_from_si(3, 4);
    TEST_ASSERT_NOT_NULL(b);
    bool div_ok = lv_rational_div_inplace(b, zero);
    TEST_ASSERT_MSG(!div_ok, "div_inplace by zero should return false");

    lv_rational_destroy(&a);
    lv_rational_destroy(&zero);
    lv_rational_destroy(&b);
}

/**
 * @brief 测试原地算术运算
 *
 * 综合验证 add_inplace、sub_inplace、mul_inplace、
 * div_inplace 和 neg_inplace 的正确性。
 */
static void test_rational_inplace_ops(void) {
    /* add_inplace: 1/2 + 1/3 = 5/6 */
    lvRational *a = lv_rational_create_from_si(1, 2);
    lvRational *b = lv_rational_create_from_si(1, 3);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    lv_rational_add_inplace(a, b);
    char *s = lv_rational_to_string(a);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "5/6");
    SAFE_FREE_STR(s);

    /* sub_inplace: 5/6 - 2/6 (=1/3) = 1/2 */
    lvRational *c = lv_rational_create_from_si(2, 6);
    TEST_ASSERT_NOT_NULL(c);
    lv_rational_sub_inplace(a, c);
    s = lv_rational_to_string(a);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "1/2");
    SAFE_FREE_STR(s);

    /* mul_inplace: 1/2 * 2/3 = 1/3 */
    lvRational *d = lv_rational_create_from_si(2, 3);
    TEST_ASSERT_NOT_NULL(d);
    lv_rational_mul_inplace(a, d);
    s = lv_rational_to_string(a);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "1/3");
    SAFE_FREE_STR(s);

    /* div_inplace: (1/3) / (1/2) = 2/3 */
    lvRational *e = lv_rational_create_from_si(1, 2);
    TEST_ASSERT_NOT_NULL(e);
    bool div_ok = lv_rational_div_inplace(a, e);
    TEST_ASSERT_MSG(div_ok, "div_inplace should succeed");
    s = lv_rational_to_string(a);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "2/3");
    SAFE_FREE_STR(s);

    /* neg_inplace: neg(2/3) = -2/3 */
    lv_rational_neg_inplace(a);
    s = lv_rational_to_string(a);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "-2/3");
    SAFE_FREE_STR(s);

    lv_rational_destroy(&a);
    lv_rational_destroy(&b);
    lv_rational_destroy(&c);
    lv_rational_destroy(&d);
    lv_rational_destroy(&e);
}

/* ============================================================
 * 主入口
 * ============================================================ */
TEST_MAIN_BEGIN("lvRational")

    TEST_MAIN_RUN(test_rational_create);
    TEST_MAIN_RUN(test_rational_create_from_si);
    TEST_MAIN_RUN(test_rational_set_zero_one);
    TEST_MAIN_RUN(test_rational_clone);
    TEST_MAIN_RUN(test_rational_add);
    TEST_MAIN_RUN(test_rational_sub);
    TEST_MAIN_RUN(test_rational_mul);
    TEST_MAIN_RUN(test_rational_div);
    TEST_MAIN_RUN(test_rational_neg);
    TEST_MAIN_RUN(test_rational_inv);
    TEST_MAIN_RUN(test_rational_abs);
    TEST_MAIN_RUN(test_rational_compare);
    TEST_MAIN_RUN(test_rational_is_functions);
    TEST_MAIN_RUN(test_rational_simplify);
    TEST_MAIN_RUN(test_rational_to_double);
    TEST_MAIN_RUN(test_rational_div_by_zero);
    TEST_MAIN_RUN(test_rational_inplace_ops);

TEST_MAIN_END()
