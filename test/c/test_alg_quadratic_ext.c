/**
 * @file test_alg_quadratic_ext.c
 * @brief 代数数域契约测试（批次 C-㊺续13：algebraic_number.h 二次代数数族 18 个零覆盖 API）
 *
 * 覆盖 quadratic 族：
 *   create / from_rational / sqrt / add / sub / mul / div / neg / conj /
 *   norm / cmp / cmp_exact / to_double / to_string / is_rational /
 *   rational_part / error_string（17 个 + 跨层 quadratic_to_interval 在
 *   interval 测试文件）。
 *
 * 契约要点（与头注释核对）：
 *   - d < 0 → ERR_INVALID。
 *   - add/sub/mul/div 要求 x->d == y->d，否则 ERR_DOMAIN。
 *   - div 除数范数为零 → ERR_INVALID；NULL → ERR_NULL。
 *   - cmp_exact 声称精确比较（修复点：diff_b != 0 时不得回退 double 近似）。
 *   - to_string 格式 "a + b*sqrt(d)"，d==0 时值退化纯有理（修复点）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/algebraic_number.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：构造 ============== */

static void test_quadratic_create_api(void) {
    AlgQuadraticError err = lv_alg_quadratic_OK;

    /* create：a=2/4=1/2, b=3/9=1/3, d=5 */
    AlgQuadratic q = lv_alg_quadratic_create(2, 4, 3, 9, 5, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_OK);
    TEST_ASSERT_EQ(q.a.num, 1);
    TEST_ASSERT_EQ(q.a.den, 2);
    TEST_ASSERT_EQ(q.b.num, 1);
    TEST_ASSERT_EQ(q.b.den, 3);
    TEST_ASSERT_EQ(q.d, 5);

    /* d < 0 → ERR_INVALID */
    q = lv_alg_quadratic_create(1, 1, 1, 1, -3, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_INVALID);
    TEST_ASSERT_EQ(q.d, 0);

    /* 分母为零 → ERR_INVALID（修复点：原实现静默忽略 r_err） */
    q = lv_alg_quadratic_create(1, 0, 1, 1, 2, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_INVALID);
    q = lv_alg_quadratic_create(1, 1, 1, 0, 2, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_INVALID);

    /* from_rational */
    AlgRational r = lv_alg_rational_create(7, 2, NULL);
    q = lv_alg_quadratic_from_rational(&r, 3);
    TEST_ASSERT_EQ(q.a.num, 7);
    TEST_ASSERT_EQ(q.a.den, 2);
    TEST_ASSERT_EQ(q.b.num, 0);
    TEST_ASSERT_EQ(q.d, 3);

    /* sqrt：0 + (2/3)*sqrt(5) */
    q = lv_alg_quadratic_sqrt(2, 3, 5, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_OK);
    TEST_ASSERT_EQ(q.a.num, 0);
    TEST_ASSERT_EQ(q.b.num, 2);
    TEST_ASSERT_EQ(q.b.den, 3);
    TEST_ASSERT_EQ(q.d, 5);
    /* sqrt d<0 → ERR_INVALID */
    q = lv_alg_quadratic_sqrt(1, 1, -1, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_INVALID);

    printf("  test_quadratic_create_api: PASSED\n");
}

/* ============== 测试：算术 ============== */

static void test_quadratic_arith_api(void) {
    AlgQuadraticError err = lv_alg_quadratic_OK;
    AlgQuadratic x = lv_alg_quadratic_create(1, 1, 2, 1, 2, NULL);  /* 1 + 2√2 */
    AlgQuadratic y = lv_alg_quadratic_create(2, 1, 3, 1, 2, NULL);  /* 2 + 3√2 */

    /* add：(1+2√2)+(2+3√2) = 3+5√2 */
    AlgQuadratic s = lv_alg_quadratic_add(&x, &y, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_OK);
    TEST_ASSERT_EQ(s.a.num, 3);
    TEST_ASSERT_EQ(s.b.num, 5);
    TEST_ASSERT_EQ(s.d, 2);

    /* sub：(1+2√2)-(2+3√2) = -1-√2 */
    s = lv_alg_quadratic_sub(&x, &y, &err);
    TEST_ASSERT_EQ(s.a.num, -1);
    TEST_ASSERT_EQ(s.b.num, -1);

    /* mul：(1+2√2)(1+√2) = (1+4) + (1+2)√2 = 5+3√2 */
    AlgQuadratic z = lv_alg_quadratic_create(1, 1, 1, 1, 2, NULL);  /* 1 + √2 */
    s = lv_alg_quadratic_mul(&x, &z, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_OK);
    TEST_ASSERT_EQ(s.a.num, 5);
    TEST_ASSERT_EQ(s.b.num, 3);

    /* div：(2+√2)/(1+√2) = √2 */
    AlgQuadratic p = lv_alg_quadratic_create(2, 1, 1, 1, 2, NULL);
    s = lv_alg_quadratic_div(&p, &z, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_OK);
    TEST_ASSERT_EQ(s.a.num, 0);
    TEST_ASSERT_EQ(s.b.num, 1);
    TEST_ASSERT_EQ(s.b.den, 1);

    /* 异域 → ERR_DOMAIN */
    AlgQuadratic w = lv_alg_quadratic_create(1, 1, 1, 1, 3, NULL);
    s = lv_alg_quadratic_add(&x, &w, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_DOMAIN);
    s = lv_alg_quadratic_mul(&x, &w, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_DOMAIN);
    s = lv_alg_quadratic_div(&x, &w, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_DOMAIN);

    /* NULL → ERR_NULL */
    s = lv_alg_quadratic_add(NULL, &y, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_NULL);

    /* 溢出：大数乘法 → ERR_OVERFLOW（修复点：原实现静默丢弃 r_err） */
    AlgQuadratic big = lv_alg_quadratic_create(INT64_MAX, 1, 0, 1, 2, NULL);
    s = lv_alg_quadratic_mul(&big, &big, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_OVERFLOW);

    printf("  test_quadratic_arith_api: PASSED\n");
}

/* ============== 测试：单目与范数 ============== */

static void test_quadratic_unary_api(void) {
    AlgQuadraticError err = lv_alg_quadratic_OK;
    AlgQuadratic x = lv_alg_quadratic_create(1, 1, 2, 1, 2, NULL);  /* 1 + 2√2 */

    /* neg：-1 - 2√2 */
    AlgQuadratic s = lv_alg_quadratic_neg(&x);
    TEST_ASSERT_EQ(s.a.num, -1);
    TEST_ASSERT_EQ(s.b.num, -2);
    TEST_ASSERT_EQ(s.d, 2);

    /* conj：1 - 2√2 */
    s = lv_alg_quadratic_conj(&x);
    TEST_ASSERT_EQ(s.a.num, 1);
    TEST_ASSERT_EQ(s.b.num, -2);

    /* norm：1² - (2²)*2 = 1 - 8 = -7 */
    AlgRational n = lv_alg_quadratic_norm(&x, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_OK);
    TEST_ASSERT_EQ(n.num, -7);
    TEST_ASSERT_EQ(n.den, 1);

    /* norm NULL → ERR_NULL */
    n = lv_alg_quadratic_norm(NULL, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_NULL);

    /* norm 溢出 → ERR_OVERFLOW（修复点：原实现静默丢弃 r_err） */
    AlgQuadratic big = lv_alg_quadratic_create(INT64_MAX, 1, 1, 1, 2, NULL);
    n = lv_alg_quadratic_norm(&big, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_OVERFLOW);

    /* div 除零范数：norm(y) == 0 → ERR_INVALID */
    /* y = 1 + 1√1：norm = 1-1 = 0（同域 d=1） */
    AlgQuadratic qq = lv_alg_quadratic_create(1, 1, 1, 1, 1, NULL);
    AlgQuadratic dom1 = lv_alg_quadratic_create(2, 1, 0, 1, 1, NULL); /* 2 + 0√1（d=1 同域） */
    s = lv_alg_quadratic_div(&dom1, &qq, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_INVALID);

    printf("  test_quadratic_unary_api: PASSED\n");
}

/* ============== 测试：比较 ============== */

static void test_quadratic_cmp_api(void) {
    AlgQuadraticError err = lv_alg_quadratic_OK;

    /* cmp 近似：1+√2 ≈ 2.414 > 2 */
    AlgQuadratic x = lv_alg_quadratic_create(1, 1, 1, 1, 2, NULL);
    AlgQuadratic y = lv_alg_quadratic_create(2, 1, 0, 1, 2, NULL);
    TEST_ASSERT(lv_alg_quadratic_cmp(&x, &y) > 0, "1+√2 > 2");

    /* cmp_exact 同域相等 */
    AlgQuadratic x2 = lv_alg_quadratic_create(2, 1, 3, 1, 5, NULL);
    AlgQuadratic y2 = lv_alg_quadratic_create(2, 1, 3, 1, 5, NULL);
    TEST_ASSERT_EQ(lv_alg_quadratic_cmp_exact(&x2, &y2, &err), 0);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_OK);

    /* cmp_exact 异域 → ERR_DOMAIN */
    AlgQuadratic w = lv_alg_quadratic_create(2, 1, 3, 1, 7, NULL);
    TEST_ASSERT_EQ(lv_alg_quadratic_cmp_exact(&x2, &w, &err), 0);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_DOMAIN);

    /* cmp_exact NULL → ERR_NULL */
    TEST_ASSERT_EQ(lv_alg_quadratic_cmp_exact(NULL, &y2, &err), 0);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_ERR_NULL);

    /* cmp_exact 精确符号（修复点：接近值不得用 double 近似误判）：
     * x = 1.4 + 0.999√2 ≈ 2.8128，y = 2 + 0.575√2 ≈ 2.8131
     * 两者接近（~3e-4）且含 √2 分量，必须按代数符号精确判定。 */
    AlgQuadratic a1 = lv_alg_quadratic_create(7, 5, 999, 1000, 2, NULL);
    AlgQuadratic b1 = lv_alg_quadratic_create(2, 1, 575, 1000, 2, NULL);
    int c = lv_alg_quadratic_cmp_exact(&a1, &b1, &err);
    TEST_ASSERT_EQ(err, lv_alg_quadratic_OK);
    TEST_ASSERT(c < 0, "1.4+0.999√2 < 2+0.575√2");

    /* cmp_exact 同号分量精确判定 */
    AlgQuadratic c1 = lv_alg_quadratic_create(3, 1, 1, 1, 2, NULL);  /* 3+√2 ≈ 4.414 */
    AlgQuadratic d1 = lv_alg_quadratic_create(2, 1, 1, 1, 2, NULL);  /* 2+√2 ≈ 3.414 */
    TEST_ASSERT(lv_alg_quadratic_cmp_exact(&c1, &d1, &err) > 0, "3+√2 > 2+√2");

    /* cmp_exact d==0 时退化纯有理（修复点：不得依赖 b 分量） */
    AlgQuadratic r1 = lv_alg_quadratic_create(5, 1, 100, 1, 0, NULL); /* 5+100√0 = 5 */
    AlgQuadratic r2 = lv_alg_quadratic_create(3, 1, 0, 1, 0, NULL);   /* 3 */
    TEST_ASSERT(lv_alg_quadratic_cmp_exact(&r1, &r2, &err) > 0, "5 > 3 (d=0 退化)");

    printf("  test_quadratic_cmp_api: PASSED\n");
}

/* ============== 测试：转换与属性 ============== */

static void test_quadratic_convert_api(void) {
    AlgQuadratic x = lv_alg_quadratic_create(1, 1, 1, 1, 2, NULL); /* 1+√2 */

    /* to_double ≈ 2.41421356 */
    TEST_ASSERT_DOUBLE(lv_alg_quadratic_to_double(&x), 2.414213562373095, 1e-9);

    /* to_string：1/2 + 3/4√5 → "1/2 + 3/4*sqrt(5)" */
    char buf[128];
    AlgQuadratic f = lv_alg_quadratic_create(1, 2, 3, 4, 5, NULL);
    int n = lv_alg_quadratic_to_string(&f, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "to_string 返回正长度");
    TEST_ASSERT(strcmp(buf, "1/2 + 3/4*sqrt(5)") == 0, "分数格式");

    /* 负 b：1 - √2 */
    AlgQuadratic m = lv_alg_quadratic_create(1, 1, 1, 1, 2, NULL);
    AlgQuadratic neg = lv_alg_quadratic_neg(&m);  /* -1 - √2 */
    n = lv_alg_quadratic_to_string(&neg, buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "-1 - sqrt(2)") == 0, "负系数格式");

    /* d==0 且 b!=0：值退化纯有理，只输出 a（修复点：原实现输出畸形 "a + b*"） */
    AlgQuadratic dz = lv_alg_quadratic_create(7, 2, 3, 1, 0, NULL); /* 7/2 + 3√0 = 7/2 */
    n = lv_alg_quadratic_to_string(&dz, buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "7/2") == 0, "d==0 退化只输出有理部分");

    /* is_rational */
    AlgQuadratic rat = lv_alg_quadratic_create(1, 1, 0, 1, 2, NULL);
    TEST_ASSERT(lv_alg_quadratic_is_rational(&rat), "b==0 为有理数");
    TEST_ASSERT(!lv_alg_quadratic_is_rational(&x), "b!=0 非有理数");

    /* rational_part */
    AlgRational rp = lv_alg_quadratic_rational_part(&x);
    TEST_ASSERT_EQ(rp.num, 1);
    TEST_ASSERT_EQ(rp.den, 1);

    /* error_string */
    const char *es = lv_alg_quadratic_error_string(lv_alg_quadratic_OK);
    TEST_ASSERT_NOT_NULL(es);
    TEST_ASSERT(strlen(es) > 0, "错误码字符串非空");
    es = lv_alg_quadratic_error_string(lv_alg_quadratic_ERR_DOMAIN);
    TEST_ASSERT_NOT_NULL(es);

    printf("  test_quadratic_convert_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Algebraic Quadratic Ext Test Suite")
    printf("=== Lv-00 Algebraic Quadratic Ext Test Suite (batch C-㊺续13) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_quadratic_create_api);
    TEST_MAIN_RUN(test_quadratic_arith_api);
    TEST_MAIN_RUN(test_quadratic_unary_api);
    TEST_MAIN_RUN(test_quadratic_cmp_api);
    TEST_MAIN_RUN(test_quadratic_convert_api);

    lv_cleanup();
TEST_MAIN_END()
