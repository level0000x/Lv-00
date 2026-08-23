/**
 * @file test_algebraic_number_ext.c
 * @brief 代数数域契约测试（批次 C-㊺续12：algebraic_number.h 有理数族 20 个零覆盖 API）
 *
 * 覆盖 rational 族 20 个 ctest 零覆盖 API：
 *   - 构造族：lv_alg_rational_create / zero / one / from_int
 *   - 算术族：lv_alg_rational_add / sub / mul / div / neg / abs / inv / pow
 *   - 比较族：lv_alg_rational_cmp / eq / is_zero / is_positive / is_negative
 *   - 转换族：lv_alg_rational_to_double / to_interval / to_string
 *
 * 契约要点（与头注释核对）：
 *   - create 自动约分；分母 0 → 0/1 + ERR_ZERO_DEN。
 *   - div 除零 → 0/1 + ERR_ZERO_DEN。
 *   - 不变量：den > 0、gcd(|p|,q)=1。
 *
 * （quadratic/interval/poly 族 55 个登记后续子批）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/algebraic_number.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：构造 ============== */

static void test_rational_create_api(void) {
    /* create：约分 */
    AlgRationalError err = lv_alg_rational_OK;
    AlgRational r = lv_alg_rational_create(6, 4, &err);
    TEST_ASSERT_EQ(err, lv_alg_rational_OK);
    TEST_ASSERT_EQ(r.num, 3);
    TEST_ASSERT_EQ(r.den, 2);

    /* 负号归一化到分子 */
    r = lv_alg_rational_create(1, -2, &err);
    TEST_ASSERT_EQ(err, lv_alg_rational_OK);
    TEST_ASSERT_EQ(r.num, -1);
    TEST_ASSERT_EQ(r.den, 2);

    /* 零分母 → 0/1 + 错误码 */
    r = lv_alg_rational_create(5, 0, &err);
    TEST_ASSERT_EQ(err, lv_alg_rational_ERR_ZERO_DEN);
    TEST_ASSERT_EQ(r.num, 0);
    TEST_ASSERT_EQ(r.den, 1);

    /* 零值 */
    r = lv_alg_rational_create(0, 7, &err);
    TEST_ASSERT_EQ(err, lv_alg_rational_OK);
    TEST_ASSERT_EQ(r.num, 0);
    TEST_ASSERT_EQ(r.den, 1);

    /* zero / one / from_int */
    AlgRational z = lv_alg_rational_zero();
    TEST_ASSERT_EQ(z.num, 0);
    TEST_ASSERT_EQ(z.den, 1);
    AlgRational o = lv_alg_rational_one();
    TEST_ASSERT_EQ(o.num, 1);
    TEST_ASSERT_EQ(o.den, 1);
    AlgRational n = lv_alg_rational_from_int(7);
    TEST_ASSERT_EQ(n.num, 7);
    TEST_ASSERT_EQ(n.den, 1);

    printf("  test_rational_create_api: PASSED\n");
}

/* ============== 测试：算术 ============== */

static void test_rational_arith_api(void) {
    AlgRational a = lv_alg_rational_create(1, 2, NULL);  /* 1/2 */
    AlgRational b = lv_alg_rational_create(1, 3, NULL);  /* 1/3 */
    AlgRationalError err = lv_alg_rational_OK;

    /* add：1/2 + 1/3 = 5/6 */
    AlgRational r = lv_alg_rational_add(&a, &b, &err);
    TEST_ASSERT_EQ(err, lv_alg_rational_OK);
    TEST_ASSERT_EQ(r.num, 5);
    TEST_ASSERT_EQ(r.den, 6);

    /* sub：1/2 - 1/3 = 1/6 */
    r = lv_alg_rational_sub(&a, &b, &err);
    TEST_ASSERT_EQ(r.num, 1);
    TEST_ASSERT_EQ(r.den, 6);

    /* mul：1/2 * 1/3 = 1/6 */
    r = lv_alg_rational_mul(&a, &b, &err);
    TEST_ASSERT_EQ(r.num, 1);
    TEST_ASSERT_EQ(r.den, 6);

    /* div：1/2 / 1/3 = 3/2 */
    r = lv_alg_rational_div(&a, &b, &err);
    TEST_ASSERT_EQ(r.num, 3);
    TEST_ASSERT_EQ(r.den, 2);

    /* 除零 → 0/1 + 错误码 */
    AlgRational z = lv_alg_rational_zero();
    r = lv_alg_rational_div(&a, &z, &err);
    TEST_ASSERT_EQ(err, lv_alg_rational_ERR_ZERO_DEN);
    TEST_ASSERT_EQ(r.num, 0);

    /* NULL 契约：err NULL 安全 */
    r = lv_alg_rational_add(&a, &b, NULL);
    TEST_ASSERT_EQ(r.num, 5);

    printf("  test_rational_arith_api: PASSED\n");
}

/* ============== 测试：单目运算 ============== */

static void test_rational_ops_api(void) {
    AlgRational a = lv_alg_rational_create(2, 3, NULL); /* 2/3 */
    AlgRationalError err = lv_alg_rational_OK;

    /* neg：-2/3 */
    AlgRational r = lv_alg_rational_neg(&a);
    TEST_ASSERT_EQ(r.num, -2);
    TEST_ASSERT_EQ(r.den, 3);

    /* abs：2/3 */
    r = lv_alg_rational_abs(&a);
    TEST_ASSERT_EQ(r.num, 2);
    TEST_ASSERT_EQ(r.den, 3);
    AlgRational neg_a = lv_alg_rational_neg(&a);
    r = lv_alg_rational_abs(&neg_a);
    TEST_ASSERT_EQ(r.num, 2);

    /* inv：3/2 */
    r = lv_alg_rational_inv(&a, &err);
    TEST_ASSERT_EQ(err, lv_alg_rational_OK);
    TEST_ASSERT_EQ(r.num, 3);
    TEST_ASSERT_EQ(r.den, 2);

    /* pow：2/3 ^ 2 = 4/9 */
    r = lv_alg_rational_pow(&a, 2, &err);
    TEST_ASSERT_EQ(r.num, 4);
    TEST_ASSERT_EQ(r.den, 9);
    /* pow 0 → 1 */
    r = lv_alg_rational_pow(&a, 0, NULL);
    TEST_ASSERT_EQ(r.num, 1);
    TEST_ASSERT_EQ(r.den, 1);
    /* pow 负指数：2/3 ^ -1 = 3/2 */
    r = lv_alg_rational_pow(&a, -1, &err);
    TEST_ASSERT_EQ(err, lv_alg_rational_OK);
    TEST_ASSERT_EQ(r.num, 3);
    TEST_ASSERT_EQ(r.den, 2);

    printf("  test_rational_ops_api: PASSED\n");
}

/* ============== 测试：比较 ============== */

static void test_rational_cmp_api(void) {
    AlgRational a = lv_alg_rational_create(1, 2, NULL);
    AlgRational b = lv_alg_rational_create(1, 3, NULL);
    AlgRational c = lv_alg_rational_create(2, 4, NULL); /* = 1/2 */

    /* cmp：-1/0/1 */
    TEST_ASSERT(lv_alg_rational_cmp(&b, &a) < 0, "1/3 < 1/2");
    TEST_ASSERT_EQ(lv_alg_rational_cmp(&a, &c), 0);
    TEST_ASSERT(lv_alg_rational_cmp(&a, &b) > 0, "1/2 > 1/3");

    /* eq */
    TEST_ASSERT(lv_alg_rational_eq(&a, &c), "1/2 == 2/4");
    TEST_ASSERT(!lv_alg_rational_eq(&a, &b), "1/2 != 1/3");

    /* is_zero / is_positive / is_negative */
    AlgRational z = lv_alg_rational_zero();
    AlgRational neg = lv_alg_rational_create(-1, 2, NULL);
    TEST_ASSERT(lv_alg_rational_is_zero(&z), "0 为零");
    TEST_ASSERT(!lv_alg_rational_is_zero(&a), "1/2 非零");
    TEST_ASSERT(lv_alg_rational_is_positive(&a), "1/2 为正");
    TEST_ASSERT(!lv_alg_rational_is_positive(&neg), "-1/2 非正");
    TEST_ASSERT(lv_alg_rational_is_negative(&neg), "-1/2 为负");
    TEST_ASSERT(!lv_alg_rational_is_negative(&a), "1/2 非负");

    printf("  test_rational_cmp_api: PASSED\n");
}

/* ============== 测试：转换 ============== */

static void test_rational_convert_api(void) {
    AlgRational a = lv_alg_rational_create(3, 4, NULL); /* 3/4 */
    AlgRationalError err = lv_alg_rational_OK;

    /* to_double */
    TEST_ASSERT_DOUBLE(lv_alg_rational_to_double(&a), 0.75, 1e-12);

    /* to_string：缓冲式，q=1 时输出整数形式 */
    char buf[64];
    int n = lv_alg_rational_to_string(&a, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "to_string 返回正长度");
    TEST_ASSERT(strcmp(buf, "3/4") == 0, "3/4 格式化为 \"3/4\"");
    AlgRational one = lv_alg_rational_one();
    n = lv_alg_rational_to_string(&one, buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "1") == 0, "1/1 格式化为 \"1\"");

    /* to_interval：点区间包含该有理数 */
    AlgInterval iv = lv_alg_rational_to_interval(&a);
    TEST_ASSERT(lv_alg_interval_is_point(&iv), "有理数转区间为点区间");
    TEST_ASSERT(lv_alg_interval_contains_rational(&iv, &a), "区间包含该有理数");
    TEST_ASSERT(!lv_alg_interval_is_empty(&iv), "区间非空");

    printf("  test_rational_convert_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Algebraic Number Ext Test Suite")
    printf("=== Lv-00 Algebraic Number Ext Test Suite (batch C-㊺续12) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_rational_create_api);
    TEST_MAIN_RUN(test_rational_arith_api);
    TEST_MAIN_RUN(test_rational_ops_api);
    TEST_MAIN_RUN(test_rational_cmp_api);
    TEST_MAIN_RUN(test_rational_convert_api);

    lv_cleanup();
TEST_MAIN_END()
