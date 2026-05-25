/**
 * @file test_ga_multivector.c
 * @brief Tests for the PGA multivector module
 *
 * Tests cover:
 * - Zero and scalar multivector creation
 * - Geometric product properties (e1*e1=1, e1*e2=e12)
 * - Outer product (e1^e2=e12, e1^e1=0)
 * - Reverse (e12 reverse = -e12)
 * - Point embedding and extraction round-trip
 * - Collinearity and coplanarity detection
 * - Norm squared computation
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"
#include "lv00/ga_multivector.h"
#include "lv00/ga_interface.h"
#include "test_helpers.h"

/* Global test counters */
int g_pass_count = 0;
int g_fail_count = 0;

/* ========================================================================
 * Helper: check if a multivector component is approximately equal
 * ======================================================================== */

static int approx_eq(double a, double b) {
    return fabs(a - b) < 1e-10;
}

/* ========================================================================
 * Test: Zero multivector
 * ======================================================================== */

void test_ga_mv_zero(void) {
    printf("Testing ga_mv_zero...\n");

    Lv00MultiVector *mv = ga_mv_zero();
    TEST_ASSERT_NOT_NULL(mv);

    /* All components should be zero */
    for (int i = 0; i < GA_MV_DIM; i++) {
        TEST_ASSERT(approx_eq(mv->components[i], 0.0),
                    "All components of zero multivector should be zero");
    }

    /* Trust should be 1.0 */
    TEST_ASSERT(approx_eq(mv->trust, 1.0), "Trust should be 1.0 for zero mv");

    ga_mv_free(mv);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Scalar multivector
 * ======================================================================== */

void test_ga_mv_scalar(void) {
    printf("Testing ga_mv_scalar...\n");

    Lv00MultiVector *mv = ga_mv_scalar(42.0);
    TEST_ASSERT_NOT_NULL(mv);

    /* Scalar component (index 0) should be 42.0 */
    TEST_ASSERT(approx_eq(mv->components[GA_BLADE_1], 42.0),
                "Scalar component should be 42.0");

    /* All other components should be zero */
    for (int i = 1; i < GA_MV_DIM; i++) {
        TEST_ASSERT(approx_eq(mv->components[i], 0.0),
                    "Non-scalar components should be zero");
    }

    ga_mv_free(mv);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Geometric product basic properties
 * ======================================================================== */

void test_ga_geometric_product(void) {
    printf("Testing ga_geometric_product...\n");

    /* Test 1: e1 * e1 = 1 (scalar) */
    Lv00MultiVector *e1 = ga_mv_zero();
    e1->components[GA_BLADE_E1] = 1.0;

    Lv00MultiVector *e1_sq = ga_geometric_product(e1, e1);
    TEST_ASSERT_NOT_NULL(e1_sq);
    TEST_ASSERT(approx_eq(e1_sq->components[GA_BLADE_1], 1.0),
                "e1 * e1 should equal 1 (scalar)");

    /* Other components should be zero */
    for (int i = 1; i < GA_MV_DIM; i++) {
        TEST_ASSERT(approx_eq(e1_sq->components[i], 0.0),
                    "e1*e1 should have no other components");
    }

    /* Test 2: e1 * e2 = e12 (bivector) */
    Lv00MultiVector *e2 = ga_mv_zero();
    e2->components[GA_BLADE_E2] = 1.0;

    Lv00MultiVector *e1_e2 = ga_geometric_product(e1, e2);
    TEST_ASSERT_NOT_NULL(e1_e2);
    TEST_ASSERT(approx_eq(e1_e2->components[GA_BLADE_E12], 1.0),
                "e1 * e2 should have e12 component = 1");

    /* Test 3: e2 * e1 = -e12 (anti-commutativity) */
    Lv00MultiVector *e2_e1 = ga_geometric_product(e2, e1);
    TEST_ASSERT_NOT_NULL(e2_e1);
    TEST_ASSERT(approx_eq(e2_e1->components[GA_BLADE_E12], -1.0),
                "e2 * e1 should have e12 component = -1");

    /* Test 4: e0 * e0 = 0 (null basis) */
    Lv00MultiVector *e0 = ga_mv_zero();
    e0->components[GA_BLADE_E0] = 1.0;

    Lv00MultiVector *e0_sq = ga_geometric_product(e0, e0);
    TEST_ASSERT_NOT_NULL(e0_sq);
    for (int i = 0; i < GA_MV_DIM; i++) {
        TEST_ASSERT(approx_eq(e0_sq->components[i], 0.0),
                    "e0 * e0 should be zero");
    }

    ga_mv_free(e1);
    ga_mv_free(e2);
    ga_mv_free(e0);
    ga_mv_free(e1_sq);
    ga_mv_free(e1_e2);
    ga_mv_free(e2_e1);
    ga_mv_free(e0_sq);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Outer product
 * ======================================================================== */

void test_ga_outer_product(void) {
    printf("Testing ga_outer_product...\n");

    /* Test 1: e1 ^ e2 = e12 */
    Lv00MultiVector *e1 = ga_mv_zero();
    e1->components[GA_BLADE_E1] = 1.0;

    Lv00MultiVector *e2 = ga_mv_zero();
    e2->components[GA_BLADE_E2] = 1.0;

    Lv00MultiVector *wedge = ga_outer_product(e1, e2);
    TEST_ASSERT_NOT_NULL(wedge);
    TEST_ASSERT(approx_eq(wedge->components[GA_BLADE_E12], 1.0),
                "e1 ^ e2 should have e12 = 1");

    /* Test 2: e1 ^ e1 = 0 */
    Lv00MultiVector *self_wedge = ga_outer_product(e1, e1);
    TEST_ASSERT_NOT_NULL(self_wedge);
    for (int i = 0; i < GA_MV_DIM; i++) {
        TEST_ASSERT(approx_eq(self_wedge->components[i], 0.0),
                    "e1 ^ e1 should be zero");
    }

    ga_mv_free(e1);
    ga_mv_free(e2);
    ga_mv_free(wedge);
    ga_mv_free(self_wedge);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Reverse
 * ======================================================================== */

void test_ga_reverse(void) {
    printf("Testing ga_reverse...\n");

    /* Test 1: reverse of scalar is itself */
    Lv00MultiVector *s = ga_mv_scalar(5.0);
    Lv00MultiVector *s_rev = ga_reverse(s);
    TEST_ASSERT_NOT_NULL(s_rev);
    TEST_ASSERT(approx_eq(s_rev->components[GA_BLADE_1], 5.0),
                "Reverse of scalar should be itself");

    /* Test 2: reverse of e1 (grade 1) is -e1 */
    Lv00MultiVector *e1 = ga_mv_zero();
    e1->components[GA_BLADE_E1] = 1.0;
    Lv00MultiVector *e1_rev = ga_reverse(e1);
    TEST_ASSERT_NOT_NULL(e1_rev);
    TEST_ASSERT(approx_eq(e1_rev->components[GA_BLADE_E1], -1.0),
                "Reverse of e1 should be -e1");

    /* Test 3: reverse of e12 (grade 2) is -e12 */
    Lv00MultiVector *e12 = ga_mv_zero();
    e12->components[GA_BLADE_E12] = 1.0;
    Lv00MultiVector *e12_rev = ga_reverse(e12);
    TEST_ASSERT_NOT_NULL(e12_rev);
    TEST_ASSERT(approx_eq(e12_rev->components[GA_BLADE_E12], -1.0),
                "Reverse of e12 should be -e12");

    /* Test 4: reverse of e123 (grade 3) is -e123 */
    Lv00MultiVector *e123 = ga_mv_zero();
    e123->components[GA_BLADE_E123] = 1.0;
    Lv00MultiVector *e123_rev = ga_reverse(e123);
    TEST_ASSERT_NOT_NULL(e123_rev);
    TEST_ASSERT(approx_eq(e123_rev->components[GA_BLADE_E123], -1.0),
                "Reverse of e123 should be -e123");

    ga_mv_free(s);
    ga_mv_free(s_rev);
    ga_mv_free(e1);
    ga_mv_free(e1_rev);
    ga_mv_free(e12);
    ga_mv_free(e12_rev);
    ga_mv_free(e123);
    ga_mv_free(e123_rev);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Point embedding and extraction round-trip
 * ======================================================================== */

void test_ga_embed_point_extract(void) {
    printf("Testing ga_embed_point / ga_extract_point round-trip...\n");

    double x = 3.0, y = 4.0, z = 5.0;

    Lv00MultiVector *p = ga_embed_point(x, y, z);
    TEST_ASSERT_NOT_NULL(p);

    double ox = 0, oy = 0, oz = 0;
    int rc = ga_extract_point(p, &ox, &oy, &oz);
    TEST_ASSERT(rc == 0, "ga_extract_point should succeed");
    TEST_ASSERT(approx_eq(ox, x), "Extracted X should match input X");
    TEST_ASSERT(approx_eq(oy, y), "Extracted Y should match input Y");
    TEST_ASSERT(approx_eq(oz, z), "Extracted Z should match input Z");

    /* Test origin */
    Lv00MultiVector *origin = ga_embed_point(0.0, 0.0, 0.0);
    TEST_ASSERT_NOT_NULL(origin);
    double ox2 = 0, oy2 = 0, oz2 = 0;
    rc = ga_extract_point(origin, &ox2, &oy2, &oz2);
    TEST_ASSERT(rc == 0, "Extract origin should succeed");
    TEST_ASSERT(approx_eq(ox2, 0.0), "Origin X should be 0");
    TEST_ASSERT(approx_eq(oy2, 0.0), "Origin Y should be 0");
    TEST_ASSERT(approx_eq(oz2, 0.0), "Origin Z should be 0");

    ga_mv_free(p);
    ga_mv_free(origin);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Three points collinear
 * ======================================================================== */

void test_ga_three_points_collinear(void) {
    printf("Testing ga_three_points_collinear...\n");

    /* Three collinear points: (0,0,0), (1,1,0), (2,2,0) */
    Lv00MultiVector *p1 = ga_embed_point(0.0, 0.0, 0.0);
    Lv00MultiVector *p2 = ga_embed_point(1.0, 1.0, 0.0);
    Lv00MultiVector *p3 = ga_embed_point(2.0, 2.0, 0.0);

    bool collinear = ga_three_points_collinear(p1, p2, p3);
    TEST_ASSERT(collinear, "Points (0,0,0), (1,1,0), (2,2,0) should be collinear");

    /* Three non-collinear points: (0,0,0), (1,0,0), (0,1,0) */
    Lv00MultiVector *q1 = ga_embed_point(0.0, 0.0, 0.0);
    Lv00MultiVector *q2 = ga_embed_point(1.0, 0.0, 0.0);
    Lv00MultiVector *q3 = ga_embed_point(0.0, 1.0, 0.0);

    bool not_collinear = ga_three_points_collinear(q1, q2, q3);
    TEST_ASSERT(!not_collinear, "Points (0,0,0), (1,0,0), (0,1,0) should not be collinear");

    ga_mv_free(p1); ga_mv_free(p2); ga_mv_free(p3);
    ga_mv_free(q1); ga_mv_free(q2); ga_mv_free(q3);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Four points coplanar
 * ======================================================================== */

void test_ga_four_points_coplanar(void) {
    printf("Testing ga_four_points_coplanar...\n");

    /* Four coplanar points (z=0 plane): (0,0,0), (1,0,0), (0,1,0), (1,1,0) */
    Lv00MultiVector *p1 = ga_embed_point(0.0, 0.0, 0.0);
    Lv00MultiVector *p2 = ga_embed_point(1.0, 0.0, 0.0);
    Lv00MultiVector *p3 = ga_embed_point(0.0, 1.0, 0.0);
    Lv00MultiVector *p4 = ga_embed_point(1.0, 1.0, 0.0);

    bool coplanar = ga_four_points_coplanar(p1, p2, p3, p4);
    TEST_ASSERT(coplanar, "Four points in z=0 plane should be coplanar");

    /* Four non-coplanar points: (0,0,0), (1,0,0), (0,1,0), (0,0,1) */
    Lv00MultiVector *q1 = ga_embed_point(0.0, 0.0, 0.0);
    Lv00MultiVector *q2 = ga_embed_point(1.0, 0.0, 0.0);
    Lv00MultiVector *q3 = ga_embed_point(0.0, 1.0, 0.0);
    Lv00MultiVector *q4 = ga_embed_point(0.0, 0.0, 1.0);

    bool not_coplanar = ga_four_points_coplanar(q1, q2, q3, q4);
    TEST_ASSERT(!not_coplanar, "Points forming a tetrahedron should not be coplanar");

    ga_mv_free(p1); ga_mv_free(p2); ga_mv_free(p3); ga_mv_free(p4);
    ga_mv_free(q1); ga_mv_free(q2); ga_mv_free(q3); ga_mv_free(q4);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Norm squared
 * ======================================================================== */

void test_ga_norm_squared(void) {
    printf("Testing ga_norm_squared...\n");

    /* Norm of scalar 3.0 should be 9.0 */
    Lv00MultiVector *s = ga_mv_scalar(3.0);
    double ns = ga_norm_squared(s);
    TEST_ASSERT(approx_eq(ns, 9.0), "||3||^2 should be 9.0");

    /* Norm of zero should be 0.0 */
    Lv00MultiVector *z = ga_mv_zero();
    double nz = ga_norm_squared(z);
    TEST_ASSERT(approx_eq(nz, 0.0), "||0||^2 should be 0.0");

    /* Norm of e1 should be 1.0 (e1^2 = 1) */
    Lv00MultiVector *e1 = ga_mv_zero();
    e1->components[GA_BLADE_E1] = 1.0;
    double ne1 = ga_norm_squared(e1);
    TEST_ASSERT(approx_eq(ne1, 1.0), "||e1||^2 should be 1.0");

    /* Norm of 2*e1 should be 4.0 */
    Lv00MultiVector *two_e1 = ga_mv_zero();
    two_e1->components[GA_BLADE_E1] = 2.0;
    double n2e1 = ga_norm_squared(two_e1);
    TEST_ASSERT(approx_eq(n2e1, 4.0), "||2*e1||^2 should be 4.0");

    ga_mv_free(s);
    ga_mv_free(z);
    ga_mv_free(e1);
    ga_mv_free(two_e1);
    printf("  PASSED\n");
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void) {
    TEST_SUITE_BEGIN("ga_multivector");

    TEST_RUN(test_ga_mv_zero);
    TEST_RUN(test_ga_mv_scalar);
    TEST_RUN(test_ga_geometric_product);
    TEST_RUN(test_ga_outer_product);
    TEST_RUN(test_ga_reverse);
    TEST_RUN(test_ga_embed_point_extract);
    TEST_RUN(test_ga_three_points_collinear);
    TEST_RUN(test_ga_four_points_coplanar);
    TEST_RUN(test_ga_norm_squared);

    TEST_SUITE_END();
    return g_fail_count > 0 ? 1 : 0;
}
