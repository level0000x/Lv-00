/**
 * @file test_nt_polynomial.c
 * @brief Test suite for the nt_polynomial (lvPoly) module
 *
 * Minimal smoke tests covering the public API of nt_polynomial.h:
 * - create / destroy lifecycle
 * - set_coeff / get_coeff
 * - degree query
 * - add / mul
 * - mod (polynomial long division)
 * - gcd (Euclidean algorithm)
 * - eval (Horner's method)
 *
 * Keeps the "reserved module" (see nt_polynomial.h design note) under
 * test coverage so its behavior baseline is locked in.
 *
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gmp.h>

#include "nt_polynomial.h"
#include "test_helpers.h"

/* ============================================================
 * Global test counters
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Helpers
 * ============================================================ */

static lvPoly *poly_from_ui(const unsigned long *coeffs, int n) {
    lvPoly *p = nt_poly_create();
    if (!p)
        return NULL;
    for (int i = 0; i < n; i++) {
        mpz_t v;
        mpz_init_set_ui(v, coeffs[i]);
        nt_poly_set_coeff(p, i, v);
        mpz_clear(v);
    }
    return p;
}

/* ============================================================
 * Test: lifecycle
 * ============================================================ */

static void test_poly_create_destroy(void) {
    lvPoly *p = nt_poly_create();
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQ(nt_poly_degree(p), -1); /* 零多项式 */
    nt_poly_destroy(p);
    nt_poly_destroy(NULL); /* NULL 安全 */
}

/* ============================================================
 * Test: coefficient access and degree
 * ============================================================ */

static void test_poly_set_get_coeff(void) {
    lvPoly *p = nt_poly_create();
    TEST_ASSERT_NOT_NULL(p);

    mpz_t v, out;
    mpz_init(out);

    mpz_init_set_ui(v, 5);
    TEST_ASSERT_EQ(nt_poly_set_coeff(p, 0, v), 0);
    TEST_ASSERT_EQ(nt_poly_degree(p), 0);

    mpz_set_ui(v, 3);
    TEST_ASSERT_EQ(nt_poly_set_coeff(p, 2, v), 0);
    TEST_ASSERT_EQ(nt_poly_degree(p), 2);

    TEST_ASSERT_EQ(nt_poly_get_coeff(p, 0, out), 0);
    TEST_ASSERT_MSG(mpz_cmp_ui(out, 5) == 0, "coeff(0) should be 5");
    TEST_ASSERT_EQ(nt_poly_get_coeff(p, 2, out), 0);
    TEST_ASSERT_MSG(mpz_cmp_ui(out, 3) == 0, "coeff(2) should be 3");

    /* 越界 get 返回 -1 且 out 置 0 */
    TEST_ASSERT_EQ(nt_poly_get_coeff(p, 5, out), -1);
    TEST_ASSERT_MSG(mpz_cmp_ui(out, 0) == 0, "out-of-range coeff should read 0");

    /* 把最高次系数清 0 会触发归一化（degree 回退） */
    mpz_set_ui(v, 0);
    TEST_ASSERT_EQ(nt_poly_set_coeff(p, 2, v), 0);
    TEST_ASSERT_EQ(nt_poly_degree(p), 0);

    mpz_clear(v);
    mpz_clear(out);
    nt_poly_destroy(p);
}

/* ============================================================
 * Test: add
 * ============================================================ */

static void test_poly_add(void) {
    /* (3x + 1) + (2x^2 + 2x) = 2x^2 + 5x + 1 */
    const unsigned long a_c[] = {1, 3};
    const unsigned long b_c[] = {0, 2, 2};
    lvPoly *a = poly_from_ui(a_c, 2);
    lvPoly *b = poly_from_ui(b_c, 3);
    lvPoly *r = nt_poly_create();
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(r);

    TEST_ASSERT_EQ(nt_poly_add(r, a, b), 0);
    TEST_ASSERT_EQ(nt_poly_degree(r), 2);

    mpz_t out;
    mpz_init(out);
    TEST_ASSERT_EQ(nt_poly_get_coeff(r, 0, out), 0);
    TEST_ASSERT_MSG(mpz_cmp_ui(out, 1) == 0, "const term should be 1");
    TEST_ASSERT_EQ(nt_poly_get_coeff(r, 1, out), 0);
    TEST_ASSERT_MSG(mpz_cmp_ui(out, 5) == 0, "x term should be 5");
    TEST_ASSERT_EQ(nt_poly_get_coeff(r, 2, out), 0);
    TEST_ASSERT_MSG(mpz_cmp_ui(out, 2) == 0, "x^2 term should be 2");
    mpz_clear(out);

    nt_poly_destroy(a);
    nt_poly_destroy(b);
    nt_poly_destroy(r);
}

/* ============================================================
 * Test: mul
 * ============================================================ */

static void test_poly_mul(void) {
    /* (x + 1) * (x - 1) = x^2 - 1 */
    const unsigned long a_c[] = {1, 1};
    lvPoly *a = poly_from_ui(a_c, 2);
    lvPoly *r = nt_poly_create();
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(r);

    /* 构造 b = (-1) + 1*x */
    lvPoly *b = nt_poly_create();
    TEST_ASSERT_NOT_NULL(b);
    mpz_t v;
    mpz_init(v);
    mpz_set_si(v, -1);
    nt_poly_set_coeff(b, 0, v);
    mpz_set_ui(v, 1);
    nt_poly_set_coeff(b, 1, v);

    TEST_ASSERT_EQ(nt_poly_mul(r, a, b), 0);
    TEST_ASSERT_EQ(nt_poly_degree(r), 2);

    mpz_t out;
    mpz_init(out);
    TEST_ASSERT_EQ(nt_poly_get_coeff(r, 0, out), 0);
    TEST_ASSERT_MSG(mpz_cmp_si(out, -1) == 0, "const term should be -1");
    TEST_ASSERT_EQ(nt_poly_get_coeff(r, 1, out), 0);
    TEST_ASSERT_MSG(mpz_cmp_ui(out, 0) == 0, "x term should be 0 (normalized away)");
    TEST_ASSERT_EQ(nt_poly_get_coeff(r, 2, out), 0);
    TEST_ASSERT_MSG(mpz_cmp_ui(out, 1) == 0, "x^2 term should be 1");
    mpz_clear(out);
    mpz_clear(v);

    nt_poly_destroy(a);
    nt_poly_destroy(b);
    nt_poly_destroy(r);
}

/* ============================================================
 * Test: mod
 * ============================================================ */

static void test_poly_mod(void) {
    /* (x^2 + 1) mod (x) = 1； (x^2 + 2x + 1) mod (x + 1) = 0 */
    const unsigned long f_c[] = {1, 2, 1}; /* (x+1)^2 */
    lvPoly *f = poly_from_ui(f_c, 3);
    lvPoly *m = poly_from_ui((const unsigned long[]) {1, 1}, 2); /* x + 1 */
    lvPoly *r = nt_poly_create();
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_NOT_NULL(r);

    TEST_ASSERT_EQ(nt_poly_mod(r, f, m), 0);
    TEST_ASSERT_EQ(nt_poly_degree(r), -1); /* (x+1)^2 mod (x+1) = 0 */

    /* (x^2 + 1) mod (x) = 1 */
    lvPoly *f2 = poly_from_ui((const unsigned long[]) {1, 0, 1}, 3);
    lvPoly *m2 = poly_from_ui((const unsigned long[]) {0, 1}, 2); /* x */
    lvPoly *r2 = nt_poly_create();
    TEST_ASSERT_NOT_NULL(f2);
    TEST_ASSERT_NOT_NULL(m2);
    TEST_ASSERT_NOT_NULL(r2);

    TEST_ASSERT_EQ(nt_poly_mod(r2, f2, m2), 0);
    TEST_ASSERT_EQ(nt_poly_degree(r2), 0);
    mpz_t out;
    mpz_init(out);
    TEST_ASSERT_EQ(nt_poly_get_coeff(r2, 0, out), 0);
    TEST_ASSERT_MSG(mpz_cmp_ui(out, 1) == 0, "x^2+1 mod x should be 1");
    mpz_clear(out);

    nt_poly_destroy(f);
    nt_poly_destroy(m);
    nt_poly_destroy(r);
    nt_poly_destroy(f2);
    nt_poly_destroy(m2);
    nt_poly_destroy(r2);
}

/* ============================================================
 * Test: gcd
 * ============================================================ */

static void test_poly_gcd(void) {
    /* gcd(x^2 - 1, x - 1) = x - 1 */
    lvPoly *a = nt_poly_create();
    lvPoly *b = nt_poly_create();
    lvPoly *r = nt_poly_create();
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(r);

    mpz_t v;
    mpz_init(v);
    mpz_set_si(v, -1);
    nt_poly_set_coeff(a, 0, v);
    mpz_set_ui(v, 1);
    nt_poly_set_coeff(a, 2, v); /* a = x^2 - 1 */

    mpz_set_si(v, -1);
    nt_poly_set_coeff(b, 0, v);
    mpz_set_ui(v, 1);
    nt_poly_set_coeff(b, 1, v); /* b = x - 1 */

    TEST_ASSERT_EQ(nt_poly_gcd(r, a, b), 0);
    TEST_ASSERT_EQ(nt_poly_degree(r), 1);

    mpz_t out;
    mpz_init(out);
    TEST_ASSERT_EQ(nt_poly_get_coeff(r, 0, out), 0);
    TEST_ASSERT_MSG(mpz_cmp_si(out, -1) == 0, "gcd const term should be -1");
    TEST_ASSERT_EQ(nt_poly_get_coeff(r, 1, out), 0);
    TEST_ASSERT_MSG(mpz_cmp_ui(out, 1) == 0, "gcd x term should be 1");
    mpz_clear(out);
    mpz_clear(v);

    nt_poly_destroy(a);
    nt_poly_destroy(b);
    nt_poly_destroy(r);
}

/* ============================================================
 * Test: eval
 * ============================================================ */

static void test_poly_eval(void) {
    /* p = 2x^2 - 3x + 1，p(3) = 18 - 9 + 1 = 10 */
    lvPoly *p = nt_poly_create();
    TEST_ASSERT_NOT_NULL(p);
    mpz_t v, x, out;
    mpz_init(v);
    mpz_init(x);
    mpz_init(out);

    mpz_set_ui(v, 1);
    nt_poly_set_coeff(p, 0, v);
    mpz_set_si(v, -3);
    nt_poly_set_coeff(p, 1, v);
    mpz_set_ui(v, 2);
    nt_poly_set_coeff(p, 2, v);

    mpz_set_ui(x, 3);
    TEST_ASSERT_EQ(nt_poly_eval(p, x, out), 0);
    TEST_ASSERT_MSG(mpz_cmp_ui(out, 10) == 0, "p(3) should be 10");

    /* 零多项式求值返回 0 */
    lvPoly *zero = nt_poly_create();
    TEST_ASSERT_NOT_NULL(zero);
    TEST_ASSERT_EQ(nt_poly_eval(zero, x, out), 0);
    TEST_ASSERT_MSG(mpz_cmp_ui(out, 0) == 0, "zero poly eval should be 0");

    mpz_clear(v);
    mpz_clear(x);
    mpz_clear(out);
    nt_poly_destroy(p);
    nt_poly_destroy(zero);
}

/* ============================================================
 * Main
 * ============================================================ */
TEST_MAIN_BEGIN("NtPolynomial")

    TEST_MAIN_RUN(test_poly_create_destroy);
    TEST_MAIN_RUN(test_poly_set_get_coeff);
    TEST_MAIN_RUN(test_poly_add);
    TEST_MAIN_RUN(test_poly_mul);
    TEST_MAIN_RUN(test_poly_mod);
    TEST_MAIN_RUN(test_poly_gcd);
    TEST_MAIN_RUN(test_poly_eval);

TEST_MAIN_END()
