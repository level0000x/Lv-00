/**
 * @file test_sym_expr.c
 * @brief Test suite for the symbolic expression module
 *
 * Tests all public API functions of sym_expr.h:
 * - Construction (const, var, binary, unary)
 * - Simplification (constant folding, identity, zero absorption)
 * - Numerical evaluation
 * - String representation
 * - Symbolic differentiation
 * - Variable substitution
 *
 * @version 3.3.0
 * @date 2026-05-25
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sym_expr.h"
#include "test_helpers.h"

/* ============================================================
 * Global test counters
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Helper: approximate equality for doubles
 * ============================================================ */
static int approx_eq(double a, double b, double eps) {
    return fabs(a - b) < eps;
}

/* ============================================================
 * Test: create constant
 * ============================================================ */

static void test_create_const(void) {
    lvSymExpr *expr = sym_expr_create_const(3.14);
    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_MSG(expr->kind == lv_SYM_CONST, "kind should be SYM_CONST");
    TEST_ASSERT_MSG(approx_eq(expr->value, 3.14, 1e-15), "value should be 3.14");
    sym_expr_destroy(expr);
}

static void test_create_const_zero(void) {
    lvSymExpr *expr = sym_expr_create_const(0.0);
    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_MSG(expr->kind == lv_SYM_CONST, "kind should be SYM_CONST");
    TEST_ASSERT_MSG(expr->value == 0.0, "value should be 0.0");
    sym_expr_destroy(expr);
}

/* ============================================================
 * Test: create variable
 * ============================================================ */

static void test_create_var(void) {
    lvSymExpr *expr = sym_expr_create_var("x");
    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_MSG(expr->kind == lv_SYM_VAR, "kind should be SYM_VAR");
    TEST_ASSERT_STR_EQ(expr->var_name, "x");
    sym_expr_destroy(expr);
}

static void test_create_var_null(void) {
    lvSymExpr *expr = sym_expr_create_var(NULL);
    TEST_ASSERT_NULL(expr);
}

/* ============================================================
 * Test: binary operations
 * ============================================================ */

static void test_add(void) {
    lvSymExpr *a = sym_expr_create_const(2.0);
    lvSymExpr *b = sym_expr_create_const(3.0);
    lvSymExpr *sum = sym_expr_create_binary(lv_SYM_ADD, a, b);
    TEST_ASSERT_NOT_NULL(sum);
    TEST_ASSERT_MSG(sum->kind == lv_SYM_ADD, "kind should be SYM_ADD");
    TEST_ASSERT_MSG(sum->child_count == 2, "should have 2 children");

    const char *names[] = {"x"};
    double vals[] = {0.0};
    double result = sym_expr_eval_double(sum, names, vals, 1);
    TEST_ASSERT_MSG(approx_eq(result, 5.0, 1e-15), "2 + 3 should be 5");

    sym_expr_destroy(sum);
}

static void test_mul(void) {
    lvSymExpr *a = sym_expr_create_const(4.0);
    lvSymExpr *b = sym_expr_create_const(5.0);
    lvSymExpr *prod = sym_expr_create_binary(lv_SYM_MUL, a, b);
    TEST_ASSERT_NOT_NULL(prod);

    const char *names[] = {"x"};
    double vals[] = {0.0};
    double result = sym_expr_eval_double(prod, names, vals, 1);
    TEST_ASSERT_MSG(approx_eq(result, 20.0, 1e-15), "4 * 5 should be 20");

    sym_expr_destroy(prod);
}

static void test_pow(void) {
    lvSymExpr *base = sym_expr_create_const(2.0);
    lvSymExpr *exp = sym_expr_create_const(10.0);
    lvSymExpr *p = sym_expr_create_binary(lv_SYM_POW, base, exp);
    TEST_ASSERT_NOT_NULL(p);

    const char *names[] = {"x"};
    double vals[] = {0.0};
    double result = sym_expr_eval_double(p, names, vals, 1);
    TEST_ASSERT_MSG(approx_eq(result, 1024.0, 1e-10), "2^10 should be 1024");

    sym_expr_destroy(p);
}

static void test_unary_neg(void) {
    lvSymExpr *a = sym_expr_create_const(7.0);
    lvSymExpr *neg = sym_expr_create_unary(lv_SYM_NEG, a);
    TEST_ASSERT_NOT_NULL(neg);

    const char *names[] = {"x"};
    double vals[] = {0.0};
    double result = sym_expr_eval_double(neg, names, vals, 1);
    TEST_ASSERT_MSG(approx_eq(result, -7.0, 1e-15), "neg(7) should be -7");

    sym_expr_destroy(neg);
}

static void test_unary_sin(void) {
    lvSymExpr *a = sym_expr_create_const(0.0);
    lvSymExpr *s = sym_expr_create_unary(lv_SYM_SIN, a);
    TEST_ASSERT_NOT_NULL(s);

    const char *names[] = {"x"};
    double vals[] = {0.0};
    double result = sym_expr_eval_double(s, names, vals, 1);
    TEST_ASSERT_MSG(approx_eq(result, 0.0, 1e-15), "sin(0) should be 0");

    sym_expr_destroy(s);
}

/* ============================================================
 * Test: simplification
 * ============================================================ */

static void test_simplify_const_add(void) {
    /* 2 + 3 -> 5 */
    lvSymExpr *a = sym_expr_create_const(2.0);
    lvSymExpr *b = sym_expr_create_const(3.0);
    lvSymExpr *sum = sym_expr_create_binary(lv_SYM_ADD, a, b);
    lvSymExpr *s = sym_expr_simplify(sum);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_MSG(s->kind == lv_SYM_CONST, "simplified 2+3 should be const");
    TEST_ASSERT_MSG(approx_eq(s->value, 5.0, 1e-15), "simplified 2+3 should be 5");
    sym_expr_destroy(sum);
    sym_expr_destroy(s);
}

static void test_simplify_add_zero(void) {
    /* x + 0 -> x */
    lvSymExpr *x = sym_expr_create_var("x");
    lvSymExpr *z = sym_expr_create_const(0.0);
    lvSymExpr *sum = sym_expr_create_binary(lv_SYM_ADD, x, z);
    lvSymExpr *s = sym_expr_simplify(sum);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_MSG(s->kind == lv_SYM_VAR, "simplified x+0 should be var");
    TEST_ASSERT_STR_EQ(s->var_name, "x");
    sym_expr_destroy(sum);
    sym_expr_destroy(s);
}

static void test_simplify_mul_zero(void) {
    /* x * 0 -> 0 */
    lvSymExpr *x = sym_expr_create_var("x");
    lvSymExpr *z = sym_expr_create_const(0.0);
    lvSymExpr *prod = sym_expr_create_binary(lv_SYM_MUL, x, z);
    lvSymExpr *s = sym_expr_simplify(prod);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_MSG(s->kind == lv_SYM_CONST, "simplified x*0 should be const");
    TEST_ASSERT_MSG(s->value == 0.0, "simplified x*0 should be 0");
    sym_expr_destroy(prod);
    sym_expr_destroy(s);
}

static void test_simplify_mul_one(void) {
    /* x * 1 -> x */
    lvSymExpr *x = sym_expr_create_var("x");
    lvSymExpr *one = sym_expr_create_const(1.0);
    lvSymExpr *prod = sym_expr_create_binary(lv_SYM_MUL, x, one);
    lvSymExpr *s = sym_expr_simplify(prod);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_MSG(s->kind == lv_SYM_VAR, "simplified x*1 should be var");
    TEST_ASSERT_STR_EQ(s->var_name, "x");
    sym_expr_destroy(prod);
    sym_expr_destroy(s);
}

static void test_simplify_neg_neg(void) {
    /* -(-x) -> x */
    lvSymExpr *x = sym_expr_create_var("x");
    lvSymExpr *neg1 = sym_expr_create_unary(lv_SYM_NEG, x);
    lvSymExpr *neg2 = sym_expr_create_unary(lv_SYM_NEG, neg1);
    lvSymExpr *s = sym_expr_simplify(neg2);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_MSG(s->kind == lv_SYM_VAR, "simplified -(-x) should be var");
    TEST_ASSERT_STR_EQ(s->var_name, "x");
    sym_expr_destroy(neg2);
    sym_expr_destroy(s);
}

static void test_simplify_pow_zero_exp(void) {
    /* x^0 -> 1 */
    lvSymExpr *x = sym_expr_create_var("x");
    lvSymExpr *z = sym_expr_create_const(0.0);
    lvSymExpr *p = sym_expr_create_binary(lv_SYM_POW, x, z);
    lvSymExpr *s = sym_expr_simplify(p);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_MSG(s->kind == lv_SYM_CONST, "simplified x^0 should be const");
    TEST_ASSERT_MSG(approx_eq(s->value, 1.0, 1e-15), "simplified x^0 should be 1");
    sym_expr_destroy(p);
    sym_expr_destroy(s);
}

static void test_simplify_sin_const(void) {
    /* sin(0) -> 0 */
    lvSymExpr *z = sym_expr_create_const(0.0);
    lvSymExpr *s = sym_expr_create_unary(lv_SYM_SIN, z);
    lvSymExpr *simp = sym_expr_simplify(s);
    TEST_ASSERT_NOT_NULL(simp);
    TEST_ASSERT_MSG(simp->kind == lv_SYM_CONST, "simplified sin(0) should be const");
    TEST_ASSERT_MSG(approx_eq(simp->value, 0.0, 1e-15), "simplified sin(0) should be 0");
    sym_expr_destroy(s);
    sym_expr_destroy(simp);
}

/* ============================================================
 * Test: evaluation
 * ============================================================ */

static void test_eval(void) {
    /* Build expression: 2*x + 3 */
    lvSymExpr *x = sym_expr_create_var("x");
    lvSymExpr *two = sym_expr_create_const(2.0);
    lvSymExpr *mul = sym_expr_create_binary(lv_SYM_MUL, two, x);
    lvSymExpr *three = sym_expr_create_const(3.0);
    lvSymExpr *expr = sym_expr_create_binary(lv_SYM_ADD, mul, three);

    const char *names[] = {"x"};
    double vals[] = {5.0};
    double result = sym_expr_eval_double(expr, names, vals, 1);
    TEST_ASSERT_MSG(approx_eq(result, 13.0, 1e-15), "2*5 + 3 should be 13");

    /* Test with different value */
    vals[0] = 0.0;
    result = sym_expr_eval_double(expr, names, vals, 1);
    TEST_ASSERT_MSG(approx_eq(result, 3.0, 1e-15), "2*0 + 3 should be 3");

    sym_expr_destroy(expr);
}

static void test_eval_missing_var(void) {
    /* Evaluate "x + 1" without providing x */
    lvSymExpr *x = sym_expr_create_var("x");
    lvSymExpr *one = sym_expr_create_const(1.0);
    lvSymExpr *expr = sym_expr_create_binary(lv_SYM_ADD, x, one);

    const char *names[] = {"y"};
    double vals[] = {1.0};
    double result = sym_expr_eval_double(expr, names, vals, 1);
    TEST_ASSERT_MSG(isnan(result), "eval with missing variable should return NaN");

    sym_expr_destroy(expr);
}

/* ============================================================
 * Test: string representation
 * ============================================================ */

static void test_to_string(void) {
    lvSymExpr *x = sym_expr_create_var("x");
    lvSymExpr *two = sym_expr_create_const(2.0);
    lvSymExpr *mul = sym_expr_create_binary(lv_SYM_MUL, two, x);
    lvSymExpr *three = sym_expr_create_const(3.0);
    lvSymExpr *expr = sym_expr_create_binary(lv_SYM_ADD, mul, three);

    char *s = sym_expr_to_string(expr);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_MSG(strlen(s) > 0, "string should not be empty");
    TEST_ASSERT_MSG(strstr(s, "2") != NULL, "should contain '2'");
    TEST_ASSERT_MSG(strstr(s, "x") != NULL, "should contain 'x'");
    TEST_ASSERT_MSG(strstr(s, "3") != NULL, "should contain '3'");

    free(s);
    sym_expr_destroy(expr);
}

/* ============================================================
 * Test: differentiation
 * ============================================================ */

static void test_diff_const(void) {
    /* d/dx(5) = 0 */
    lvSymExpr *c = sym_expr_create_const(5.0);
    lvSymExpr *d = sym_expr_diff(c, "x");
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_MSG(d->kind == lv_SYM_CONST, "d/dx(5) should be const");
    TEST_ASSERT_MSG(approx_eq(d->value, 0.0, 1e-15), "d/dx(5) should be 0");
    sym_expr_destroy(c);
    sym_expr_destroy(d);
}

static void test_diff_var(void) {
    /* d/dx(x) = 1 */
    lvSymExpr *x = sym_expr_create_var("x");
    lvSymExpr *d = sym_expr_diff(x, "x");
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_MSG(d->kind == lv_SYM_CONST, "d/dx(x) should be const");
    TEST_ASSERT_MSG(approx_eq(d->value, 1.0, 1e-15), "d/dx(x) should be 1");
    sym_expr_destroy(x);
    sym_expr_destroy(d);
}

static void test_diff_var_other(void) {
    /* d/dx(y) = 0 */
    lvSymExpr *y = sym_expr_create_var("y");
    lvSymExpr *d = sym_expr_diff(y, "x");
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_MSG(d->kind == lv_SYM_CONST, "d/dx(y) should be const");
    TEST_ASSERT_MSG(approx_eq(d->value, 0.0, 1e-15), "d/dx(y) should be 0");
    sym_expr_destroy(y);
    sym_expr_destroy(d);
}

static void test_diff_add(void) {
    /* d/dx(x + x) = 2 */
    lvSymExpr *x1 = sym_expr_create_var("x");
    lvSymExpr *x2 = sym_expr_create_var("x");
    lvSymExpr *sum = sym_expr_create_binary(lv_SYM_ADD, x1, x2);
    lvSymExpr *d = sym_expr_diff(sum, "x");
    TEST_ASSERT_NOT_NULL(d);

    const char *names[] = {"x"};
    double vals[] = {1.0};
    double result = sym_expr_eval_double(d, names, vals, 1);
    TEST_ASSERT_MSG(approx_eq(result, 2.0, 1e-10), "d/dx(x + x) should be 2");

    sym_expr_destroy(sum);
    sym_expr_destroy(d);
}

static void test_diff_x_squared(void) {
    /* d/dx(x^2) = 2*x */
    lvSymExpr *x = sym_expr_create_var("x");
    lvSymExpr *two = sym_expr_create_const(2.0);
    lvSymExpr *xsq = sym_expr_create_binary(lv_SYM_POW, x, two);
    lvSymExpr *d = sym_expr_diff(xsq, "x");
    TEST_ASSERT_NOT_NULL(d);

    /* Evaluate derivative at x=3: should be 2*3 = 6 */
    const char *names[] = {"x"};
    double vals[] = {3.0};
    double result = sym_expr_eval_double(d, names, vals, 1);
    TEST_ASSERT_MSG(approx_eq(result, 6.0, 1e-8), "d/dx(x^2) at x=3 should be 6");

    sym_expr_destroy(xsq);
    sym_expr_destroy(d);
}

static void test_diff_sin(void) {
    /* d/dx(sin(x)) = cos(x) */
    lvSymExpr *x = sym_expr_create_var("x");
    lvSymExpr *sinx = sym_expr_create_unary(lv_SYM_SIN, x);
    lvSymExpr *d = sym_expr_diff(sinx, "x");
    TEST_ASSERT_NOT_NULL(d);

    /* Evaluate at x=0: cos(0) = 1 */
    const char *names[] = {"x"};
    double vals[] = {0.0};
    double result = sym_expr_eval_double(d, names, vals, 1);
    TEST_ASSERT_MSG(approx_eq(result, 1.0, 1e-10), "d/dx(sin(x)) at x=0 should be 1");

    sym_expr_destroy(sinx);
    sym_expr_destroy(d);
}

/* ============================================================
 * Test: substitution
 * ============================================================ */

static void test_substitute(void) {
    /* Substitute x=3 in expression x + 1, expect 4 */
    lvSymExpr *x = sym_expr_create_var("x");
    lvSymExpr *one = sym_expr_create_const(1.0);
    lvSymExpr *expr = sym_expr_create_binary(lv_SYM_ADD, x, one);

    lvSymExpr *three = sym_expr_create_const(3.0);
    lvSymExpr *sub = sym_expr_substitute(expr, "x", three);
    TEST_ASSERT_NOT_NULL(sub);

    const char *names[] = {"x"};
    double vals[] = {0.0};
    double result = sym_expr_eval_double(sub, names, vals, 1);
    TEST_ASSERT_MSG(approx_eq(result, 4.0, 1e-15), "substitute x=3 in x+1 should give 4");

    sym_expr_destroy(expr);
    sym_expr_destroy(three);
    sym_expr_destroy(sub);
}

static void test_substitute_no_match(void) {
    /* Substitute y=5 in expression x + 1, should remain unchanged */
    lvSymExpr *x = sym_expr_create_var("x");
    lvSymExpr *one = sym_expr_create_const(1.0);
    lvSymExpr *expr = sym_expr_create_binary(lv_SYM_ADD, x, one);

    lvSymExpr *five = sym_expr_create_const(5.0);
    lvSymExpr *sub = sym_expr_substitute(expr, "y", five);
    TEST_ASSERT_NOT_NULL(sub);

    const char *names[] = {"x"};
    double vals[] = {10.0};
    double result = sym_expr_eval_double(sub, names, vals, 1);
    TEST_ASSERT_MSG(approx_eq(result, 11.0, 1e-15), "substitute y=5 in x+1 should not change x");

    sym_expr_destroy(expr);
    sym_expr_destroy(five);
    sym_expr_destroy(sub);
}

/* ============================================================
 * Test: destroy NULL (safety)
 * ============================================================ */

static void test_destroy_null(void) {
    /* Should not crash */
    sym_expr_destroy(NULL);
    TEST_ASSERT_MSG(1, "destroy NULL should not crash");
}

/* ============================================================
 * Main
 * ============================================================ */
int main(void) {
    TEST_SUITE_BEGIN("SymExpr");

    /* Construction */
    TEST_RUN(test_create_const);
    TEST_RUN(test_create_const_zero);
    TEST_RUN(test_create_var);
    TEST_RUN(test_create_var_null);
    TEST_RUN(test_add);
    TEST_RUN(test_mul);
    TEST_RUN(test_pow);
    TEST_RUN(test_unary_neg);
    TEST_RUN(test_unary_sin);

    /* Simplification */
    TEST_RUN(test_simplify_const_add);
    TEST_RUN(test_simplify_add_zero);
    TEST_RUN(test_simplify_mul_zero);
    TEST_RUN(test_simplify_mul_one);
    TEST_RUN(test_simplify_neg_neg);
    TEST_RUN(test_simplify_pow_zero_exp);
    TEST_RUN(test_simplify_sin_const);

    /* Evaluation */
    TEST_RUN(test_eval);
    TEST_RUN(test_eval_missing_var);

    /* String */
    TEST_RUN(test_to_string);

    /* Differentiation */
    TEST_RUN(test_diff_const);
    TEST_RUN(test_diff_var);
    TEST_RUN(test_diff_var_other);
    TEST_RUN(test_diff_add);
    TEST_RUN(test_diff_x_squared);
    TEST_RUN(test_diff_sin);

    /* Substitution */
    TEST_RUN(test_substitute);
    TEST_RUN(test_substitute_no_match);

    /* Safety */
    TEST_RUN(test_destroy_null);

    TEST_SUITE_END();
    return (g_fail_count > 0) ? 1 : 0;
}
