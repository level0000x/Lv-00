/**
 * @file test_interval_arithmetic.c
 * @brief Test suite for the interval arithmetic module
 *
 * Tests all public API functions of interval_arithmetic.h:
 * - Factory functions (create, point, empty, entire)
 * - Arithmetic operations (add, sub, mul, div, sqrt, sin, cos, exp, log, abs, neg)
 * - Properties (diam, mid, is_empty, contains, is_subset, equals)
 * - Set operations (intersect, union)
 * - Symbolic integration (from_symbolic, to_symbolic)
 * - Verification (verify_solution, verify_adaptive)
 *
 * @version 3.4.0
 * @date 2026-05-25
 */

#include "lv/lv_platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interval_arithmetic.h"
#include "test_helpers.h"

/* ============================================================
 * Global test counters
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Test: interval creation
 * ============================================================ */

static void test_interval_create(void) {
    lvInterval iv = interval_create(1.0, 5.0, 0);
    TEST_ASSERT_MSG(!interval_is_empty(iv), "interval [1,5] should not be empty");
    TEST_ASSERT_MSG(approx_eq_eps(iv.lo, 1.0, 1e-15), "lo should be 1.0");
    TEST_ASSERT_MSG(approx_eq_eps(iv.hi, 5.0, 1e-15), "hi should be 5.0");
    TEST_ASSERT_MSG(iv.is_exact == 0, "should not be exact");
}

static void test_interval_point(void) {
    lvInterval iv = interval_point(3.0);
    TEST_ASSERT_MSG(!interval_is_empty(iv), "point interval should not be empty");
    TEST_ASSERT_MSG(approx_eq_eps(iv.lo, 3.0, 1e-15), "lo should be 3.0");
    TEST_ASSERT_MSG(approx_eq_eps(iv.hi, 3.0, 1e-15), "hi should be 3.0");
    TEST_ASSERT_MSG(iv.is_exact == 1, "point interval should be exact");
}

static void test_interval_empty(void) {
    lvInterval iv = interval_empty();
    TEST_ASSERT_MSG(interval_is_empty(iv), "empty interval should be empty");
}

static void test_interval_entire(void) {
    lvInterval iv = interval_entire();
    TEST_ASSERT_MSG(!interval_is_empty(iv), "entire interval should not be empty");
    TEST_ASSERT_MSG(iv.lo == -INFINITY, "lo should be -inf");
    TEST_ASSERT_MSG(iv.hi == INFINITY, "hi should be +inf");
}

/* ============================================================
 * Test: arithmetic operations
 * ============================================================ */

static void test_interval_add(void) {
    /* [1,2] + [3,4] = [4,6] */
    lvInterval a = interval_create(1.0, 2.0, 0);
    lvInterval b = interval_create(3.0, 4.0, 0);
    lvInterval r = interval_add(a, b);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, 4.0, 1e-15), "[1,2]+[3,4] lo should be 4");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, 6.0, 1e-15), "[1,2]+[3,4] hi should be 6");
}

static void test_interval_sub(void) {
    /* [5,8] - [2,3] = [2,6] */
    lvInterval a = interval_create(5.0, 8.0, 0);
    lvInterval b = interval_create(2.0, 3.0, 0);
    lvInterval r = interval_sub(a, b);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, 2.0, 1e-15), "[5,8]-[2,3] lo should be 2");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, 6.0, 1e-15), "[5,8]-[2,3] hi should be 6");
}

static void test_interval_mul(void) {
    /* [-1,2] * [3,4] = [-4,8] */
    lvInterval a = interval_create(-1.0, 2.0, 0);
    lvInterval b = interval_create(3.0, 4.0, 0);
    lvInterval r = interval_mul(a, b);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, -4.0, 2e-15), "[-1,2]*[3,4] lo should be -4");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, 8.0, 2e-15), "[-1,2]*[3,4] hi should be 8");
}

static void test_interval_mul_positive(void) {
    /* [2,3] * [4,5] = [8,15] */
    lvInterval a = interval_create(2.0, 3.0, 0);
    lvInterval b = interval_create(4.0, 5.0, 0);
    lvInterval r = interval_mul(a, b);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, 8.0, 2e-15), "[2,3]*[4,5] lo should be 8");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, 15.0, 2e-15), "[2,3]*[4,5] hi should be 15");
}

static void test_interval_div(void) {
    /* [6,12] / [2,3] = [2,6] */
    lvInterval a = interval_create(6.0, 12.0, 0);
    lvInterval b = interval_create(2.0, 3.0, 0);
    lvInterval r = interval_div(a, b);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, 2.0, 1e-14), "[6,12]/[2,3] lo should be 2");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, 6.0, 1e-14), "[6,12]/[2,3] hi should be 6");
}

static void test_interval_div_by_zero(void) {
    /* [1,2] / [-1,1] should return empty (divisor contains 0) */
    lvInterval a = interval_create(1.0, 2.0, 0);
    lvInterval b = interval_create(-1.0, 1.0, 0);
    lvInterval r = interval_div(a, b);
    TEST_ASSERT_MSG(interval_is_empty(r), "division by interval containing 0 should be empty");
}

static void test_interval_div_negative(void) {
    /* [6,12] / [-3,-2] = [-6,-2] */
    lvInterval a = interval_create(6.0, 12.0, 0);
    lvInterval b = interval_create(-3.0, -2.0, 0);
    lvInterval r = interval_div(a, b);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, -6.0, 1e-14), "[6,12]/[-3,-2] lo should be -6");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, -2.0, 1e-14), "[6,12]/[-3,-2] hi should be -2");
}

static void test_interval_sqrt(void) {
    /* sqrt([4,9]) = [2,3] */
    lvInterval a = interval_create(4.0, 9.0, 0);
    lvInterval r = interval_sqrt(a);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, 2.0, 1e-15), "sqrt([4,9]) lo should be 2");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, 3.0, 1e-15), "sqrt([4,9]) hi should be 3");
}

static void test_interval_sqrt_negative(void) {
    /* sqrt([-1,4]) should return empty (lo < 0) */
    lvInterval a = interval_create(-1.0, 4.0, 0);
    lvInterval r = interval_sqrt(a);
    TEST_ASSERT_MSG(interval_is_empty(r), "sqrt of interval with negative lo should be empty");
}

static void test_interval_sin(void) {
    /* sin([0, pi/2]) should be [0, 1] */
    lvInterval a = interval_create(0.0, M_PI / 2.0, 0);
    lvInterval r = interval_sin(a);
    TEST_ASSERT_MSG(r.lo >= -1e-15, "sin([0,pi/2]) lo should be ~0");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, 1.0, 1e-15), "sin([0,pi/2]) hi should be 1");
}

static void test_interval_cos(void) {
    /* cos([0, pi]) should be [-1, 1] */
    lvInterval a = interval_create(0.0, M_PI, 0);
    lvInterval r = interval_cos(a);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, -1.0, 1e-15), "cos([0,pi]) lo should be -1");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, 1.0, 1e-15), "cos([0,pi]) hi should be 1");
}

static void test_interval_exp(void) {
    /* exp([0, 1]) should be [1, e] */
    lvInterval a = interval_create(0.0, 1.0, 0);
    lvInterval r = interval_exp(a);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, 1.0, 1e-15), "exp([0,1]) lo should be 1");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, M_E, 1e-14), "exp([0,1]) hi should be e");
}

static void test_interval_log(void) {
    /* log([1, e]) should be [0, 1] */
    lvInterval a = interval_create(1.0, M_E, 0);
    lvInterval r = interval_log(a);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, 0.0, 1e-15), "log([1,e]) lo should be 0");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, 1.0, 1e-14), "log([1,e]) hi should be 1");
}

static void test_interval_log_negative(void) {
    /* log([-1, 1]) should return empty (lo <= 0) */
    lvInterval a = interval_create(-1.0, 1.0, 0);
    lvInterval r = interval_log(a);
    TEST_ASSERT_MSG(interval_is_empty(r), "log of interval with non-positive lo should be empty");
}

static void test_interval_abs(void) {
    /* abs([-3, 2]) = [0, 3] */
    lvInterval a = interval_create(-3.0, 2.0, 0);
    lvInterval r = interval_abs(a);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, 0.0, 1e-15), "abs([-3,2]) lo should be 0");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, 3.0, 1e-15), "abs([-3,2]) hi should be 3");
}

static void test_interval_neg(void) {
    /* neg([1, 3]) = [-3, -1] */
    lvInterval a = interval_create(1.0, 3.0, 0);
    lvInterval r = interval_neg(a);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, -3.0, 1e-15), "neg([1,3]) lo should be -3");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, -1.0, 1e-15), "neg([1,3]) hi should be -1");
}

/* ============================================================
 * Test: properties
 * ============================================================ */

static void test_interval_diam(void) {
    lvInterval iv = interval_create(2.0, 7.0, 0);
    TEST_ASSERT_MSG(approx_eq_eps(interval_diam(iv), 5.0, 1e-15), "diam([2,7]) should be 5");
}

static void test_interval_mid(void) {
    lvInterval iv = interval_create(2.0, 8.0, 0);
    TEST_ASSERT_MSG(approx_eq_eps(interval_mid(iv), 5.0, 1e-15), "mid([2,8]) should be 5");
}

static void test_interval_contains(void) {
    lvInterval iv = interval_create(1.0, 5.0, 0);
    TEST_ASSERT_MSG(interval_contains(iv, 3.0) == 1, "[1,5] should contain 3");
    TEST_ASSERT_MSG(interval_contains(iv, 1.0) == 1, "[1,5] should contain 1");
    TEST_ASSERT_MSG(interval_contains(iv, 5.0) == 1, "[1,5] should contain 5");
    TEST_ASSERT_MSG(interval_contains(iv, 0.0) == 0, "[1,5] should not contain 0");
    TEST_ASSERT_MSG(interval_contains(iv, 6.0) == 0, "[1,5] should not contain 6");
}

static void test_interval_is_subset(void) {
    lvInterval a = interval_create(2.0, 4.0, 0);
    lvInterval b = interval_create(1.0, 5.0, 0);
    lvInterval c = interval_create(3.0, 6.0, 0);
    TEST_ASSERT_MSG(interval_is_subset(a, b) == 1, "[2,4] should be subset of [1,5]");
    TEST_ASSERT_MSG(interval_is_subset(a, c) == 0, "[2,4] should not be subset of [3,6]");
}

static void test_interval_equals(void) {
    lvInterval a = interval_create(1.0, 5.0, 0);
    lvInterval b = interval_create(1.0, 5.0, 0);
    lvInterval c = interval_create(1.0, 6.0, 0);
    TEST_ASSERT_MSG(interval_equals(a, b) == 1, "equal intervals should be equal");
    TEST_ASSERT_MSG(interval_equals(a, c) == 0, "different intervals should not be equal");
}

/* ============================================================
 * Test: set operations
 * ============================================================ */

static void test_interval_intersect(void) {
    /* [1,5] intersect [3,8] = [3,5] */
    lvInterval a = interval_create(1.0, 5.0, 0);
    lvInterval b = interval_create(3.0, 8.0, 0);
    lvInterval r = interval_intersect(a, b);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, 3.0, 1e-15), "intersect lo should be 3");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, 5.0, 1e-15), "intersect hi should be 5");
}

static void test_interval_intersect_disjoint(void) {
    /* [1,3] intersect [5,8] = empty */
    lvInterval a = interval_create(1.0, 3.0, 0);
    lvInterval b = interval_create(5.0, 8.0, 0);
    lvInterval r = interval_intersect(a, b);
    TEST_ASSERT_MSG(interval_is_empty(r), "disjoint intersection should be empty");
}

static void test_interval_union(void) {
    /* [1,3] union [5,8] = [1,8] */
    lvInterval a = interval_create(1.0, 3.0, 0);
    lvInterval b = interval_create(5.0, 8.0, 0);
    lvInterval r = interval_union(a, b);
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, 1.0, 1e-15), "union lo should be 1");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, 8.0, 1e-15), "union hi should be 8");
}

/* ============================================================
 * Test: symbolic integration
 * ============================================================ */

static void test_interval_from_symbolic(void) {
    const char *names[] = {"x", "y"};
    lvInterval bounds[] = {interval_create(1.0, 2.0, 0), interval_create(3.0, 4.0, 0)};

    /* x + y with x in [1,2], y in [3,4] => [4,6] */
    lvInterval r = interval_from_symbolic("x + y", names, bounds, 2);
    TEST_ASSERT_MSG(!interval_is_empty(r), "symbolic eval should not be empty");
    TEST_ASSERT_MSG(approx_eq_eps(r.lo, 4.0, 1e-14), "x+y lo should be 4");
    TEST_ASSERT_MSG(approx_eq_eps(r.hi, 6.0, 1e-14), "x+y hi should be 6");
}

static void test_interval_to_symbolic(void) {
    lvInterval iv = interval_create(1.5, 3.5, 0);
    char buf[128];
    int len = interval_to_symbolic(iv, buf, sizeof(buf));
    TEST_ASSERT_MSG(len > 0, "to_symbolic should return positive length");
    TEST_ASSERT_MSG(strstr(buf, "1.5") != NULL, "should contain 1.5");
    TEST_ASSERT_MSG(strstr(buf, "3.5") != NULL, "should contain 3.5");
}

/* ============================================================
 * Test: verification
 * ============================================================ */

static void test_interval_verify_solution(void) {
    /* f(x) in [-0.5, 0.5] should verify with tolerance 1.0 */
    lvInterval iv = interval_create(-0.5, 0.5, 0);
    TEST_ASSERT_MSG(interval_verify_solution(iv, 1.0) == 1, "[-0.5,0.5] should verify with tolerance 1.0");

    /* f(x) in [1.0, 2.0] should NOT verify with tolerance 0.5 */
    lvInterval iv2 = interval_create(1.0, 2.0, 0);
    TEST_ASSERT_MSG(interval_verify_solution(iv2, 0.5) == 0, "[1.0,2.0] should not verify with tolerance 0.5");

    /* f(x) in [-0.1, 0.1] should verify with tolerance 0.5 */
    lvInterval iv3 = interval_create(-0.1, 0.1, 0);
    TEST_ASSERT_MSG(interval_verify_solution(iv3, 0.5) == 1, "[-0.1,0.1] should verify with tolerance 0.5");
}

static void test_interval_verify_adaptive(void) {
    /* Verify that x - 0.5 = 0 has a solution in x in [0, 1] */
    const char *names[] = {"x"};
    lvInterval bounds[] = {interval_create(0.0, 1.0, 0)};

    int result = interval_verify_adaptive("x - 0.5", names, bounds, 1, 10, 1e-6);
    TEST_ASSERT_MSG(result == 1, "x - 0.5 should have a solution in [0,1]");
}

/* ============================================================
 * Test: empty interval arithmetic
 * ============================================================ */

static void test_empty_arithmetic(void) {
    lvInterval empty = interval_empty();
    lvInterval a = interval_create(1.0, 2.0, 0);

    TEST_ASSERT_MSG(interval_is_empty(interval_add(empty, a)), "empty + a = empty");
    TEST_ASSERT_MSG(interval_is_empty(interval_sub(empty, a)), "empty - a = empty");
    TEST_ASSERT_MSG(interval_is_empty(interval_mul(empty, a)), "empty * a = empty");
    TEST_ASSERT_MSG(interval_is_empty(interval_div(empty, a)), "empty / a = empty");
}

/* ============================================================
 * Main
 * ============================================================ */
TEST_MAIN_BEGIN("IntervalArithmetic")

    /* Factory */
    TEST_MAIN_RUN(test_interval_create);
    TEST_MAIN_RUN(test_interval_point);
    TEST_MAIN_RUN(test_interval_empty);
    TEST_MAIN_RUN(test_interval_entire);

    /* Arithmetic */
    TEST_MAIN_RUN(test_interval_add);
    TEST_MAIN_RUN(test_interval_sub);
    TEST_MAIN_RUN(test_interval_mul);
    TEST_MAIN_RUN(test_interval_mul_positive);
    TEST_MAIN_RUN(test_interval_div);
    TEST_MAIN_RUN(test_interval_div_by_zero);
    TEST_MAIN_RUN(test_interval_div_negative);
    TEST_MAIN_RUN(test_interval_sqrt);
    TEST_MAIN_RUN(test_interval_sqrt_negative);
    TEST_MAIN_RUN(test_interval_sin);
    TEST_MAIN_RUN(test_interval_cos);
    TEST_MAIN_RUN(test_interval_exp);
    TEST_MAIN_RUN(test_interval_log);
    TEST_MAIN_RUN(test_interval_log_negative);
    TEST_MAIN_RUN(test_interval_abs);
    TEST_MAIN_RUN(test_interval_neg);

    /* Properties */
    TEST_MAIN_RUN(test_interval_diam);
    TEST_MAIN_RUN(test_interval_mid);
    TEST_MAIN_RUN(test_interval_contains);
    TEST_MAIN_RUN(test_interval_is_subset);
    TEST_MAIN_RUN(test_interval_equals);

    /* Set operations */
    TEST_MAIN_RUN(test_interval_intersect);
    TEST_MAIN_RUN(test_interval_intersect_disjoint);
    TEST_MAIN_RUN(test_interval_union);

    /* Symbolic integration */
    TEST_MAIN_RUN(test_interval_from_symbolic);
    TEST_MAIN_RUN(test_interval_to_symbolic);

    /* Verification */
    TEST_MAIN_RUN(test_interval_verify_solution);
    TEST_MAIN_RUN(test_interval_verify_adaptive);

    /* Edge cases */
    TEST_MAIN_RUN(test_empty_arithmetic);

TEST_MAIN_END()
