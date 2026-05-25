/**
 * @file test_autodiff.c
 * @brief Tests for the automatic differentiation engine.
 *
 * @details Tests cover:
 *          - Engine lifecycle
 *          - Expression construction
 *          - Forward differentiation of x^2 (d/dx = 2x)
 *          - Reverse differentiation of x^2 (d/dx = 2x)
 *          - Forward differentiation of sin(x) (d/dx = cos(x))
 *          - Chain rule: d/dx sin(x^2) = 2x*cos(x^2)
 *          - Expression evaluation
 *          - Gradient query after reverse mode
 *          - NULL safety
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"
#include "autodiff.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/** Tolerance for floating-point comparisons */
#define AD_TOLERANCE 1e-9

/**
 * @brief Assert that two doubles are approximately equal.
 */
#define TEST_ASSERT_NEAR(actual, expected, tol, msg)                                        \
    do {                                                                                    \
        double _ad_actual = (double)(actual);                                                \
        double _ad_expected = (double)(expected);                                            \
        double _ad_diff = _ad_actual - _ad_expected;                                        \
        if (_ad_diff < 0.0) _ad_diff = -_ad_diff;                                           \
        if (_ad_diff > (tol)) {                                                              \
            fprintf(stderr, "  FAIL [%s:%d] %s (actual=%.12f, expected=%.12f, diff=%.12e)\n", \
                    __FILE__, __LINE__, (msg), _ad_actual, _ad_expected, _ad_diff);          \
            g_fail_count++;                                                                  \
            return;                                                                          \
        }                                                                                    \
        g_pass_count++;                                                                      \
    } while (0)

/* ============================================================
 * Test: Engine lifecycle
 * ============================================================ */

static void test_ad_engine_lifecycle(void) {
    Lv00ADEngine *engine = ad_engine_create(AD_FORWARD);
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQ(engine->mode, AD_FORWARD);
    ad_engine_destroy(engine);

    engine = ad_engine_create(AD_REVERSE);
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQ(engine->mode, AD_REVERSE);
    ad_engine_destroy(engine);

    /* NULL-safe destroy */
    ad_engine_destroy(NULL);
}

/* ============================================================
 * Test: Expression construction
 * ============================================================ */

static void test_expr_construction(void) {
    /* Constant */
    Lv00ADExpr *c = ad_expr_create_const(3.14);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQ(c->kind, AD_CONST);
    TEST_ASSERT_NEAR(c->value, 3.14, AD_TOLERANCE, "const value");
    ad_expr_destroy(c);

    /* Variable */
    Lv00ADExpr *v = ad_expr_create_var(0);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQ(v->kind, AD_VAR);
    TEST_ASSERT_EQ(v->var_index, 0);
    ad_expr_destroy(v);

    /* Addition */
    Lv00ADExpr *a = ad_expr_create_const(1.0);
    Lv00ADExpr *b = ad_expr_create_const(2.0);
    Lv00ADExpr *add = ad_expr_add(a, b);
    TEST_ASSERT_NOT_NULL(add);
    TEST_ASSERT_EQ(add->kind, AD_ADD);
    TEST_ASSERT_EQ(add->child_count, 2);
    ad_expr_destroy(add); /* Destroys a and b recursively */

    /* Multiplication */
    a = ad_expr_create_const(3.0);
    b = ad_expr_create_const(4.0);
    Lv00ADExpr *mul = ad_expr_mul(a, b);
    TEST_ASSERT_NOT_NULL(mul);
    TEST_ASSERT_EQ(mul->kind, AD_MUL);
    ad_expr_destroy(mul);

    /* Sin */
    Lv00ADExpr *x = ad_expr_create_var(0);
    Lv00ADExpr *s = ad_expr_sin(x);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ(s->kind, AD_SIN);
    ad_expr_destroy(s);

    /* Cos */
    x = ad_expr_create_var(0);
    Lv00ADExpr *co = ad_expr_cos(x);
    TEST_ASSERT_NOT_NULL(co);
    TEST_ASSERT_EQ(co->kind, AD_COS);
    ad_expr_destroy(co);

    /* Power */
    Lv00ADExpr *base = ad_expr_create_var(0);
    Lv00ADExpr *exp = ad_expr_create_const(2.0);
    Lv00ADExpr *p = ad_expr_pow(base, exp);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQ(p->kind, AD_POW);
    ad_expr_destroy(p);

    /* NULL safety */
    TEST_ASSERT_NULL(ad_expr_add(NULL, NULL));
    TEST_ASSERT_NULL(ad_expr_mul(NULL, NULL));
    TEST_ASSERT_NULL(ad_expr_sin(NULL));
    TEST_ASSERT_NULL(ad_expr_cos(NULL));
    TEST_ASSERT_NULL(ad_expr_pow(NULL, NULL));

    /* NULL-safe destroy */
    ad_expr_destroy(NULL);
}

/* ============================================================
 * Test: Forward differentiation of x^2
 * ============================================================ */

static void test_forward_diff_x_squared(void) {
    /* Build expression: x * x (which is x^2) */
    Lv00ADExpr *x = ad_expr_create_var(0);
    Lv00ADExpr *x2 = ad_expr_mul(x, x);

    double value, deriv;

    /* At x = 3.0: f(3) = 9, f'(3) = 6 */
    bool ok = ad_forward_diff(x2, 0, 3.0, &value, &deriv);
    TEST_ASSERT(ok, "forward_diff should succeed");
    TEST_ASSERT_NEAR(value, 9.0, AD_TOLERANCE, "x^2 at x=3 should be 9");
    TEST_ASSERT_NEAR(deriv, 6.0, AD_TOLERANCE, "d/dx(x^2) at x=3 should be 6");

    /* At x = 0.0: f(0) = 0, f'(0) = 0 */
    ok = ad_forward_diff(x2, 0, 0.0, &value, &deriv);
    TEST_ASSERT(ok, "forward_diff should succeed");
    TEST_ASSERT_NEAR(value, 0.0, AD_TOLERANCE, "x^2 at x=0 should be 0");
    TEST_ASSERT_NEAR(deriv, 0.0, AD_TOLERANCE, "d/dx(x^2) at x=0 should be 0");

    /* At x = -2.0: f(-2) = 4, f'(-2) = -4 */
    ok = ad_forward_diff(x2, 0, -2.0, &value, &deriv);
    TEST_ASSERT(ok, "forward_diff should succeed");
    TEST_ASSERT_NEAR(value, 4.0, AD_TOLERANCE, "x^2 at x=-2 should be 4");
    TEST_ASSERT_NEAR(deriv, -4.0, AD_TOLERANCE, "d/dx(x^2) at x=-2 should be -4");

    ad_expr_destroy(x2);
}

/* ============================================================
 * Test: Reverse differentiation of x^2
 * ============================================================ */

static void test_reverse_diff_x_squared(void) {
    /* Build expression: x * x (which is x^2) */
    Lv00ADExpr *x = ad_expr_create_var(0);
    Lv00ADExpr *x2 = ad_expr_mul(x, x);

    double value;
    double gradients[1];

    /* At x = 3.0: f(3) = 9, grad = 6 */
    bool ok = ad_reverse_diff(x2, (double[]){3.0}, 1, &value, gradients);
    TEST_ASSERT(ok, "reverse_diff should succeed");
    TEST_ASSERT_NEAR(value, 9.0, AD_TOLERANCE, "x^2 at x=3 should be 9");

    /* Query gradient for variable 0 */
    double grad = ad_grad(x2, 0);
    TEST_ASSERT_NEAR(grad, 6.0, AD_TOLERANCE, "d/dx(x^2) at x=3 should be 6");

    /* At x = 5.0: f(5) = 25, grad = 10 */
    ok = ad_reverse_diff(x2, (double[]){5.0}, 1, &value, gradients);
    TEST_ASSERT(ok, "reverse_diff should succeed");
    TEST_ASSERT_NEAR(value, 25.0, AD_TOLERANCE, "x^2 at x=5 should be 25");
    grad = ad_grad(x2, 0);
    TEST_ASSERT_NEAR(grad, 10.0, AD_TOLERANCE, "d/dx(x^2) at x=5 should be 10");

    ad_expr_destroy(x2);
}

/* ============================================================
 * Test: Forward differentiation of sin(x)
 * ============================================================ */

static void test_forward_diff_sin(void) {
    /* Build expression: sin(x) */
    Lv00ADExpr *x = ad_expr_create_var(0);
    Lv00ADExpr *s = ad_expr_sin(x);

    double value, deriv;

    /* At x = 0.0: sin(0) = 0, cos(0) = 1 */
    bool ok = ad_forward_diff(s, 0, 0.0, &value, &deriv);
    TEST_ASSERT(ok, "forward_diff should succeed");
    TEST_ASSERT_NEAR(value, 0.0, AD_TOLERANCE, "sin(0) should be 0");
    TEST_ASSERT_NEAR(deriv, 1.0, AD_TOLERANCE, "d/dx(sin(x)) at x=0 should be 1");

    /* At x = PI/2: sin(PI/2) = 1, cos(PI/2) = 0 */
    ok = ad_forward_diff(s, 0, M_PI / 2.0, &value, &deriv);
    TEST_ASSERT(ok, "forward_diff should succeed");
    TEST_ASSERT_NEAR(value, 1.0, AD_TOLERANCE, "sin(PI/2) should be 1");
    TEST_ASSERT_NEAR(deriv, 0.0, AD_TOLERANCE, "d/dx(sin(x)) at x=PI/2 should be 0");

    /* At x = PI: sin(PI) = 0, cos(PI) = -1 */
    ok = ad_forward_diff(s, 0, M_PI, &value, &deriv);
    TEST_ASSERT(ok, "forward_diff should succeed");
    TEST_ASSERT_NEAR(value, sin(M_PI), AD_TOLERANCE, "sin(PI) should be ~0");
    TEST_ASSERT_NEAR(deriv, cos(M_PI), AD_TOLERANCE, "d/dx(sin(x)) at x=PI should be -1");

    ad_expr_destroy(s);
}

/* ============================================================
 * Test: Chain rule - d/dx sin(x^2) = 2x * cos(x^2)
 * ============================================================ */

static void test_chain_rule(void) {
    /* Build expression: sin(x * x) = sin(x^2) */
    Lv00ADExpr *x = ad_expr_create_var(0);
    Lv00ADExpr *x2 = ad_expr_mul(x, x);
    Lv00ADExpr *expr = ad_expr_sin(x2);

    double value, deriv;

    /* At x = 1.0: sin(1) = sin(1), d/dx = 2*1*cos(1) = 2*cos(1) */
    bool ok = ad_forward_diff(expr, 0, 1.0, &value, &deriv);
    TEST_ASSERT(ok, "forward_diff should succeed");
    TEST_ASSERT_NEAR(value, sin(1.0), AD_TOLERANCE, "sin(x^2) at x=1 should be sin(1)");
    TEST_ASSERT_NEAR(deriv, 2.0 * cos(1.0), AD_TOLERANCE,
        "d/dx(sin(x^2)) at x=1 should be 2*cos(1)");

    /* At x = 0.0: sin(0) = 0, d/dx = 2*0*cos(0) = 0 */
    ok = ad_forward_diff(expr, 0, 0.0, &value, &deriv);
    TEST_ASSERT(ok, "forward_diff should succeed");
    TEST_ASSERT_NEAR(value, 0.0, AD_TOLERANCE, "sin(x^2) at x=0 should be 0");
    TEST_ASSERT_NEAR(deriv, 0.0, AD_TOLERANCE,
        "d/dx(sin(x^2)) at x=0 should be 0");

    /* At x = 2.0: sin(4), d/dx = 2*2*cos(4) = 4*cos(4) */
    ok = ad_forward_diff(expr, 0, 2.0, &value, &deriv);
    TEST_ASSERT(ok, "forward_diff should succeed");
    TEST_ASSERT_NEAR(value, sin(4.0), AD_TOLERANCE, "sin(x^2) at x=2 should be sin(4)");
    TEST_ASSERT_NEAR(deriv, 4.0 * cos(4.0), AD_TOLERANCE,
        "d/dx(sin(x^2)) at x=2 should be 4*cos(4)");

    /* Also test with reverse mode */
    double grad_val;
    ok = ad_reverse_diff(expr, (double[]){1.0}, 1, &value, &grad_val);
    TEST_ASSERT(ok, "reverse_diff should succeed");
    TEST_ASSERT_NEAR(value, sin(1.0), AD_TOLERANCE, "sin(x^2) at x=1 (reverse)");
    double grad = ad_grad(expr, 0);
    TEST_ASSERT_NEAR(grad, 2.0 * cos(1.0), AD_TOLERANCE,
        "d/dx(sin(x^2)) at x=1 (reverse) should be 2*cos(1)");

    ad_expr_destroy(expr);
}

/* ============================================================
 * Test: Expression evaluation
 * ============================================================ */

static void test_ad_eval(void) {
    /* Build expression: 2 * x + 3 */
    Lv00ADExpr *two = ad_expr_create_const(2.0);
    Lv00ADExpr *x = ad_expr_create_var(0);
    Lv00ADExpr *three = ad_expr_create_const(3.0);
    Lv00ADExpr *mul = ad_expr_mul(two, x);
    Lv00ADExpr *add = ad_expr_add(mul, three);

    double result;
    bool ok = ad_eval(add, (double[]){5.0}, 1, &result);
    TEST_ASSERT(ok, "eval should succeed");
    TEST_ASSERT_NEAR(result, 13.0, AD_TOLERANCE, "2*5+3 should be 13");

    ok = ad_eval(add, (double[]){-1.0}, 1, &result);
    TEST_ASSERT(ok, "eval should succeed");
    TEST_ASSERT_NEAR(result, 1.0, AD_TOLERANCE, "2*(-1)+3 should be 1");

    ad_expr_destroy(add);
}

/* ============================================================
 * Test: Cosine differentiation
 * ============================================================ */

static void test_forward_diff_cos(void) {
    /* Build expression: cos(x) */
    Lv00ADExpr *x = ad_expr_create_var(0);
    Lv00ADExpr *c = ad_expr_cos(x);

    double value, deriv;

    /* At x = 0.0: cos(0) = 1, -sin(0) = 0 */
    bool ok = ad_forward_diff(c, 0, 0.0, &value, &deriv);
    TEST_ASSERT(ok, "forward_diff should succeed");
    TEST_ASSERT_NEAR(value, 1.0, AD_TOLERANCE, "cos(0) should be 1");
    TEST_ASSERT_NEAR(deriv, 0.0, AD_TOLERANCE, "d/dx(cos(x)) at x=0 should be 0");

    /* At x = PI: cos(PI) = -1, -sin(PI) = 0 */
    ok = ad_forward_diff(c, 0, M_PI, &value, &deriv);
    TEST_ASSERT(ok, "forward_diff should succeed");
    TEST_ASSERT_NEAR(value, cos(M_PI), AD_TOLERANCE, "cos(PI) should be -1");
    TEST_ASSERT_NEAR(deriv, -sin(M_PI), AD_TOLERANCE,
        "d/dx(cos(x)) at x=PI should be ~0");

    ad_expr_destroy(c);
}

/* ============================================================
 * Test: Power differentiation
 * ============================================================ */

static void test_forward_diff_pow(void) {
    /* Build expression: x^3 using ad_expr_pow */
    Lv00ADExpr *x = ad_expr_create_var(0);
    Lv00ADExpr *three = ad_expr_create_const(3.0);
    Lv00ADExpr *p = ad_expr_pow(x, three);

    double value, deriv;

    /* At x = 2.0: 2^3 = 8, d/dx(x^3) = 3*x^2 = 12 */
    bool ok = ad_forward_diff(p, 0, 2.0, &value, &deriv);
    TEST_ASSERT(ok, "forward_diff should succeed");
    TEST_ASSERT_NEAR(value, 8.0, AD_TOLERANCE, "x^3 at x=2 should be 8");
    TEST_ASSERT_NEAR(deriv, 12.0, AD_TOLERANCE, "d/dx(x^3) at x=2 should be 12");

    ad_expr_destroy(p);
}

/* ============================================================
 * Test: Multi-variable reverse mode
 * ============================================================ */

static void test_reverse_diff_multi_var(void) {
    /* Build expression: x * y (product of two variables) */
    Lv00ADExpr *x = ad_expr_create_var(0);
    Lv00ADExpr *y = ad_expr_create_var(1);
    Lv00ADExpr *mul = ad_expr_mul(x, y);

    double value;
    double gradients[2];

    /* At x=3, y=4: f = 12, df/dx = 4, df/dy = 3 */
    double vars[] = {3.0, 4.0};
    bool ok = ad_reverse_diff(mul, vars, 2, &value, gradients);
    TEST_ASSERT(ok, "reverse_diff should succeed");
    TEST_ASSERT_NEAR(value, 12.0, AD_TOLERANCE, "x*y at (3,4) should be 12");

    double gx = ad_grad(mul, 0);
    double gy = ad_grad(mul, 1);
    TEST_ASSERT_NEAR(gx, 4.0, AD_TOLERANCE, "d/dx(x*y) at (3,4) should be 4");
    TEST_ASSERT_NEAR(gy, 3.0, AD_TOLERANCE, "d/dy(x*y) at (3,4) should be 3");

    ad_expr_destroy(mul);
}

/* ============================================================
 * Test: NULL safety
 * ============================================================ */

static void test_ad_null_safety(void) {
    double value, deriv;

    /* NULL engine create (mode out of range still works) */
    Lv00ADEngine *engine = ad_engine_create(AD_FORWARD);
    TEST_ASSERT_NOT_NULL(engine);
    ad_engine_destroy(engine);

    /* NULL forward diff */
    bool ok = ad_forward_diff(NULL, 0, 1.0, &value, &deriv);
    TEST_ASSERT(!ok, "forward_diff with NULL expr should fail");

    ok = ad_forward_diff(NULL, 0, 1.0, NULL, &deriv);
    TEST_ASSERT(!ok, "forward_diff with NULL value should fail");

    /* NULL reverse diff */
    ok = ad_reverse_diff(NULL, NULL, 0, NULL, NULL);
    TEST_ASSERT(!ok, "reverse_diff with NULL expr should fail");

    /* NULL eval */
    ok = ad_eval(NULL, NULL, 0, NULL);
    TEST_ASSERT(!ok, "eval with NULL expr should fail");

    /* NULL grad */
    double g = ad_grad(NULL, 0);
    TEST_ASSERT_NEAR(g, 0.0, AD_TOLERANCE, "grad with NULL expr should return 0");
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void) {
    TEST_SUITE_BEGIN("Automatic Differentiation");

    TEST_RUN(test_ad_engine_lifecycle);
    TEST_RUN(test_expr_construction);
    TEST_RUN(test_forward_diff_x_squared);
    TEST_RUN(test_reverse_diff_x_squared);
    TEST_RUN(test_forward_diff_sin);
    TEST_RUN(test_chain_rule);
    TEST_RUN(test_ad_eval);
    TEST_RUN(test_forward_diff_cos);
    TEST_RUN(test_forward_diff_pow);
    TEST_RUN(test_reverse_diff_multi_var);
    TEST_RUN(test_ad_null_safety);

    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}
