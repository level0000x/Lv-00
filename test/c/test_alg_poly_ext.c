/**
 * @file test_alg_poly_ext.c
 * @brief 代数数域契约测试（批次 C-㊺续13：algebraic_number.h 多项式族零覆盖 API）
 *
 * 覆盖 poly 族：
 *   zero / const / linear / quadratic / x / eval_int / eval_rational / add /
 *   sub / mul / neg / lead_coef / const_coef / is_zero / is_const /
 *   discriminant / rational_roots / derivative / to_string / error_string
 *   （20 个）+ 跨层 has_real_roots。
 *
 * 契约要点（与头注释核对）：
 *   - 系数不变量：coef[degree] != 0，degree <= MAX_DEGREE。
 *   - mul 超限 → ERR_DEGREE；eval 溢出 → ERR_OVERFLOW。
 *   - rational_roots 返回去重后的有理根（修复点：原实现可能返回重复根）。
 *   - neg 对 INT64_MIN 必须安全（修复点：原实现 -coef UB）。
 *   - eval_rational 溢出必须上报（修复点：原实现静默丢弃 r_err）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/algebraic_number.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：构造 ============== */

static void test_poly_construct_api(void) {
    AlgPoly z = lv_alg_poly_zero();
    TEST_ASSERT(lv_alg_poly_is_zero(&z), "零多项式");
    TEST_ASSERT_EQ(z.degree, 0);

    AlgPoly c = lv_alg_poly_const(5);
    TEST_ASSERT_EQ(c.degree, 0);
    TEST_ASSERT_EQ(c.coef[0], 5);
    TEST_ASSERT(lv_alg_poly_is_const(&c), "常数多项式");
    TEST_ASSERT(!lv_alg_poly_is_zero(&c), "非零常数");

    AlgPoly l = lv_alg_poly_linear(3, 2);  /* 3x + 2 */
    TEST_ASSERT_EQ(l.degree, 1);
    TEST_ASSERT_EQ(l.coef[1], 3);
    TEST_ASSERT_EQ(l.coef[0], 2);
    TEST_ASSERT(!lv_alg_poly_is_const(&l), "一次非常数");

    /* 首项零 → 降次 */
    AlgPoly l0 = lv_alg_poly_linear(0, 7); /* 7 */
    TEST_ASSERT_EQ(l0.degree, 0);

    AlgPoly q = lv_alg_poly_quadratic(2, 3, 4);  /* 2x^2 + 3x + 4 */
    TEST_ASSERT_EQ(q.degree, 2);
    TEST_ASSERT_EQ(q.coef[2], 2);
    TEST_ASSERT_EQ(q.coef[1], 3);
    TEST_ASSERT_EQ(q.coef[0], 4);

    AlgPoly x = lv_alg_poly_x();  /* x */
    TEST_ASSERT_EQ(x.degree, 1);
    TEST_ASSERT_EQ(x.coef[1], 1);
    TEST_ASSERT_EQ(x.coef[0], 0);

    printf("  test_poly_construct_api: PASSED\n");
}

/* ============== 测试：求值 ============== */

static void test_poly_eval_api(void) {
    AlgPolyError err = lv_alg_poly_OK;
    AlgPoly p = lv_alg_poly_quadratic(1, -3, 2); /* x^2 - 3x + 2 */

    /* eval_int：25-15+2 = 12 */
    TEST_ASSERT_EQ(lv_alg_poly_eval_int(&p, 5, &err), 12);
    TEST_ASSERT_EQ(err, lv_alg_poly_OK);
    /* eval_int 根：x=1 → 0，x=2 → 0 */
    TEST_ASSERT_EQ(lv_alg_poly_eval_int(&p, 1, &err), 0);
    TEST_ASSERT_EQ(lv_alg_poly_eval_int(&p, 2, &err), 0);

    /* eval_rational：p(1/2) = 1/4 - 3/2 + 2 = 3/4 */
    AlgRational half = lv_alg_rational_create(1, 2, NULL);
    AlgRational r = lv_alg_poly_eval_rational(&p, &half, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_OK);
    TEST_ASSERT_EQ(r.num, 3);
    TEST_ASSERT_EQ(r.den, 4);

    /* eval_int 溢出 → ERR_OVERFLOW */
    AlgPoly big = lv_alg_poly_quadratic(INT64_MAX, 0, 0); /* INT64_MAX*x^2 */
    int64_t v = lv_alg_poly_eval_int(&big, 2, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_ERR_OVERFLOW);
    (void) v;

    /* eval_rational 溢出 → ERR_OVERFLOW（修复点：原实现静默丢弃 r_err） */
    AlgRational two = lv_alg_rational_from_int(2);
    r = lv_alg_poly_eval_rational(&big, &two, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_ERR_OVERFLOW);

    /* NULL → ERR_NULL */
    TEST_ASSERT_EQ(lv_alg_poly_eval_int(NULL, 1, &err), 0);
    TEST_ASSERT_EQ(err, lv_alg_poly_ERR_NULL);
    r = lv_alg_poly_eval_rational(NULL, &half, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_ERR_NULL);
    r = lv_alg_poly_eval_rational(&p, NULL, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_ERR_NULL);

    printf("  test_poly_eval_api: PASSED\n");
}

/* ============== 测试：运算 ============== */

static void test_poly_arith_api(void) {
    AlgPolyError err = lv_alg_poly_OK;
    AlgPoly a = lv_alg_poly_linear(2, 1);          /* 2x + 1 */
    AlgPoly b = lv_alg_poly_linear(3, 2);          /* 3x + 2 */

    /* add：(2x+1)+(3x+2) = 5x+3 */
    AlgPoly s = lv_alg_poly_add(&a, &b, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_OK);
    TEST_ASSERT_EQ(s.degree, 1);
    TEST_ASSERT_EQ(s.coef[1], 5);
    TEST_ASSERT_EQ(s.coef[0], 3);

    /* sub：(2x+1)-(3x+2) = -x-1 */
    s = lv_alg_poly_sub(&a, &b, &err);
    TEST_ASSERT_EQ(s.degree, 1);
    TEST_ASSERT_EQ(s.coef[1], -1);
    TEST_ASSERT_EQ(s.coef[0], -1);

    /* mul：(2x+1)(3x+2) = 6x^2 + 7x + 2 */
    s = lv_alg_poly_mul(&a, &b, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_OK);
    TEST_ASSERT_EQ(s.degree, 2);
    TEST_ASSERT_EQ(s.coef[2], 6);
    TEST_ASSERT_EQ(s.coef[1], 7);
    TEST_ASSERT_EQ(s.coef[0], 2);

    /* mul 超限 → ERR_DEGREE */
    AlgPoly hi = lv_alg_poly_zero();
    hi.degree = lv_alg_poly_MAX_DEGREE;
    hi.coef[lv_alg_poly_MAX_DEGREE] = 1;
    s = lv_alg_poly_mul(&hi, &hi, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_ERR_DEGREE);

    /* add 溢出 → ERR_OVERFLOW */
    AlgPoly c1 = lv_alg_poly_const(INT64_MAX);
    AlgPoly c2 = lv_alg_poly_const(1);
    s = lv_alg_poly_add(&c1, &c2, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_ERR_OVERFLOW);

    /* neg：(2x+1) → -2x-1 */
    s = lv_alg_poly_neg(&a);
    TEST_ASSERT_EQ(s.coef[1], -2);
    TEST_ASSERT_EQ(s.coef[0], -1);

    /* neg INT64_MIN 安全（修复点：原实现 -INT64_MIN UB） */
    AlgPoly cmin = lv_alg_poly_const(INT64_MIN);
    s = lv_alg_poly_neg(&cmin);
    TEST_ASSERT_EQ(s.degree, 0);
    TEST_ASSERT(s.coef[0] != INT64_MIN, "INT64_MIN 取负不得 UB");

    /* NULL → ERR_NULL */
    s = lv_alg_poly_add(NULL, &b, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_ERR_NULL);
    s = lv_alg_poly_mul(&a, NULL, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_ERR_NULL);

    printf("  test_poly_arith_api: PASSED\n");
}

/* ============== 测试：访问器与判别式 ============== */

static void test_poly_accessor_api(void) {
    AlgPolyError err = lv_alg_poly_OK;
    AlgPoly q = lv_alg_poly_quadratic(2, -5, 3); /* 2x^2 - 5x + 3 */

    /* lead/const */
    TEST_ASSERT_EQ(lv_alg_poly_lead_coef(&q), 2);
    TEST_ASSERT_EQ(lv_alg_poly_const_coef(&q), 3);

    /* is_zero / is_const */
    AlgPoly z = lv_alg_poly_zero();
    TEST_ASSERT(lv_alg_poly_is_zero(&z), "零多项式判定");
    TEST_ASSERT(!lv_alg_poly_is_zero(&q), "非零多项式判定");
    AlgPoly z0 = lv_alg_poly_const(0);
    TEST_ASSERT(lv_alg_poly_is_const(&z0), "零常数是常数");

    /* discriminant：deg2 → b^2-4ac = 25-24 = 1 */
    TEST_ASSERT_EQ(lv_alg_poly_discriminant(&q, &err), 1);
    TEST_ASSERT_EQ(err, lv_alg_poly_OK);
    /* deg1 → 1 */
    AlgPoly l = lv_alg_poly_linear(2, 1);
    TEST_ASSERT_EQ(lv_alg_poly_discriminant(&l, &err), 1);
    /* deg0 → 0 */
    AlgPoly c = lv_alg_poly_const(5);
    TEST_ASSERT_EQ(lv_alg_poly_discriminant(&c, &err), 0);
    /* deg>2 → ERR_DEGREE */
    AlgPoly hi = lv_alg_poly_zero();
    hi.degree = 3;
    hi.coef[3] = 1;
    TEST_ASSERT_EQ(lv_alg_poly_discriminant(&hi, &err), 0);
    TEST_ASSERT_EQ(err, lv_alg_poly_ERR_DEGREE);

    /* derivative：2x^2-5x+3 → 4x-5 */
    AlgPoly d = lv_alg_poly_derivative(&q, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_OK);
    TEST_ASSERT_EQ(d.degree, 1);
    TEST_ASSERT_EQ(d.coef[1], 4);
    TEST_ASSERT_EQ(d.coef[0], -5);
    /* 常数导数 → 零 */
    d = lv_alg_poly_derivative(&c, &err);
    TEST_ASSERT(lv_alg_poly_is_zero(&d), "常数导数为零");

    printf("  test_poly_accessor_api: PASSED\n");
}

/* ============== 测试：有理根 ============== */

static void test_poly_roots_api(void) {
    AlgPolyError err = lv_alg_poly_OK;
    AlgRational roots[16];

    /* 一次：3x - 6 → x=2 */
    AlgPoly l = lv_alg_poly_linear(3, -6);
    int n = lv_alg_poly_rational_roots(&l, roots, 16, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_OK);
    TEST_ASSERT_EQ(n, 1);
    TEST_ASSERT_EQ(roots[0].num, 2);

    /* 二次：x^2-5x+6 = (x-2)(x-3)，disc=1 完全平方 → {2,3} */
    AlgPoly q = lv_alg_poly_quadratic(1, -5, 6);
    n = lv_alg_poly_rational_roots(&q, roots, 16, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_OK);
    TEST_ASSERT_EQ(n, 2);
    TEST_ASSERT_EQ(roots[0].num, 2);
    TEST_ASSERT_EQ(roots[1].num, 3);

    /* 二次重根：x^2-2x+1 = (x-1)^2 → {1} */
    AlgPoly dup = lv_alg_poly_quadratic(1, -2, 1);
    n = lv_alg_poly_rational_roots(&dup, roots, 16, &err);
    TEST_ASSERT_EQ(n, 1);
    TEST_ASSERT_EQ(roots[0].num, 1);

    /* 二次无实根：x^2+1 → 0 个 */
    AlgPoly no = lv_alg_poly_quadratic(1, 0, 1);
    n = lv_alg_poly_rational_roots(&no, roots, 16, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_OK);
    TEST_ASSERT_EQ(n, 0);

    /* 二次非完全平方判别式：2x^2+1 → disc=-8 无根 */
    AlgPoly irr = lv_alg_poly_quadratic(2, 0, 1);
    n = lv_alg_poly_rational_roots(&irr, roots, 16, &err);
    TEST_ASSERT_EQ(n, 0);

    /* 高次：x^3 - 6x^2 + 11x - 6 = (x-1)(x-2)(x-3) → {1,2,3} */
    AlgPoly cubic = lv_alg_poly_quadratic(1, -6, 11);
    /* 构造 degree 3：p = (x^2-5x+6)(x-1) = x^3-6x^2+11x-6 */
    AlgPoly x1 = lv_alg_poly_linear(1, -1);
    AlgPoly cubic_full = lv_alg_poly_mul(&q, &x1, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_OK);
    n = lv_alg_poly_rational_roots(&cubic_full, roots, 16, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_OK);
    TEST_ASSERT_EQ(n, 3);
    TEST_ASSERT_EQ(roots[0].num, 1);
    TEST_ASSERT_EQ(roots[1].num, 2);
    TEST_ASSERT_EQ(roots[2].num, 3);
    (void) cubic;

    /* 常数多项式：无根 */
    AlgPoly c = lv_alg_poly_const(7);
    n = lv_alg_poly_rational_roots(&c, roots, 16, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_OK);
    TEST_ASSERT_EQ(n, 0);

    /* NULL / 边界 → ERR_NULL */
    n = lv_alg_poly_rational_roots(NULL, roots, 16, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_ERR_NULL);
    n = lv_alg_poly_rational_roots(&q, NULL, 16, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_ERR_NULL);
    n = lv_alg_poly_rational_roots(&q, roots, 0, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_ERR_NULL);

    /* 重复根去重（修复点）：p = x^2 * (x-1) = x^3 - x^2，
     * 有理根定理枚举 + 降次路径不得返回重复根 → {0, 1} */
    AlgPoly px = lv_alg_poly_x();
    AlgPoly x2 = lv_alg_poly_mul(&px, &px, &err);
    AlgPoly mx1 = lv_alg_poly_linear(1, -1);
    AlgPoly x2_x1 = lv_alg_poly_mul(&x2, &mx1, &err);
    n = lv_alg_poly_rational_roots(&x2_x1, roots, 16, &err);
    TEST_ASSERT_EQ(err, lv_alg_poly_OK);
    TEST_ASSERT_EQ(n, 2);
    TEST_ASSERT_EQ(roots[0].num, 0);
    TEST_ASSERT_EQ(roots[1].num, 1);

    printf("  test_poly_roots_api: PASSED\n");
}

/* ============== 测试：字符串 ============== */

static void test_poly_string_api(void) {
    char buf[256];
    int n;

    /* 2x^2 - 5x + 3 → "2*x^2 - 5*x + 3" */
    AlgPoly q = lv_alg_poly_quadratic(2, -5, 3);
    n = lv_alg_poly_to_string(&q, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "to_string 正长度");
    TEST_ASSERT(strcmp(buf, "2*x^2 - 5*x + 3") == 0, "二次格式");

    /* 零多项式 → "0" */
    AlgPoly z = lv_alg_poly_zero();
    n = lv_alg_poly_to_string(&z, buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "0") == 0, "零多项式格式");

    /* 常系数 → "7" */
    AlgPoly c = lv_alg_poly_const(7);
    n = lv_alg_poly_to_string(&c, buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "7") == 0, "常数格式");

    /* NULL → "(null)" */
    n = lv_alg_poly_to_string(NULL, buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "(null)") == 0, "NULL 格式");

    /* has_real_roots 跨层 */
    TEST_ASSERT(lv_alg_has_real_roots(1, -5, 6), "disc>0 有实根");
    TEST_ASSERT(!lv_alg_has_real_roots(1, 0, 1), "disc<0 无实根");
    TEST_ASSERT(lv_alg_has_real_roots(1, 2, 1), "disc=0 有实根");

    /* error_string */
    const char *es = lv_alg_poly_error_string(lv_alg_poly_OK);
    TEST_ASSERT_NOT_NULL(es);
    TEST_ASSERT(strlen(es) > 0, "错误码字符串非空");
    es = lv_alg_poly_error_string(lv_alg_poly_ERR_DEGREE);
    TEST_ASSERT_NOT_NULL(es);
    es = lv_alg_poly_error_string(lv_alg_poly_ERR_DIV_BY_ZERO);
    TEST_ASSERT_NOT_NULL(es);

    printf("  test_poly_string_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Algebraic Poly Ext Test Suite")
    printf("=== Lv-00 Algebraic Poly Ext Test Suite (batch C-㊺续13) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_poly_construct_api);
    TEST_MAIN_RUN(test_poly_eval_api);
    TEST_MAIN_RUN(test_poly_arith_api);
    TEST_MAIN_RUN(test_poly_accessor_api);
    TEST_MAIN_RUN(test_poly_roots_api);
    TEST_MAIN_RUN(test_poly_string_api);

    lv_cleanup();
TEST_MAIN_END()
