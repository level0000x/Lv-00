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
#include "ga_multivector.h"
#include "ga_interface.h"
#include "test_helpers.h"

/* Blade index definitions */
#define GA_BLADE_1    0
#define GA_BLADE_E0   1
#define GA_BLADE_E1   2
#define GA_BLADE_E2   3
#define GA_BLADE_E3   4
#define GA_BLADE_E01  5
#define GA_BLADE_E02  6
#define GA_BLADE_E03  7
#define GA_BLADE_E12  8
#define GA_BLADE_E13  9
#define GA_BLADE_E23  10
#define GA_BLADE_E012 11
#define GA_BLADE_E013 12
#define GA_BLADE_E023 13
#define GA_BLADE_E123 14
#define GA_BLADE_E0123 15

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
        TEST_ASSERT(approx_eq(ga_mv_get(mv, i), 0.0),
                    "All components of zero multivector should be zero");
    }

    ga_mv_destroy(mv);
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
    TEST_ASSERT(approx_eq(ga_mv_get(mv, GA_BLADE_1), 42.0),
                "Scalar component should be 42.0");

    /* All other components should be zero */
    for (int i = 1; i < GA_MV_DIM; i++) {
        TEST_ASSERT(approx_eq(ga_mv_get(mv, i), 0.0),
                    "Non-scalar components should be zero");
    }

    ga_mv_destroy(mv);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Geometric product basic properties
 * ======================================================================== */

void test_ga_geometric_product(void) {
    printf("Testing ga_geometric_product...\n");

    /* Test 1: e1 * e1 = 1 (scalar) */
    Lv00MultiVector *e1 = ga_mv_zero();
    ga_mv_set(e1, GA_BLADE_E1, 1.0);

    Lv00MultiVector *e1_sq = ga_geometric_product(e1, e1);
    TEST_ASSERT_NOT_NULL(e1_sq);
    TEST_ASSERT(approx_eq(ga_mv_get(e1_sq, GA_BLADE_1), 1.0),
                "e1 * e1 should equal 1 (scalar)");

    /* Other components should be zero */
    for (int i = 1; i < GA_MV_DIM; i++) {
        TEST_ASSERT(approx_eq(ga_mv_get(e1_sq, i), 0.0),
                    "e1*e1 should have no other components");
    }

    /* Test 2: e1 * e2 = e12 (bivector) */
    Lv00MultiVector *e2 = ga_mv_zero();
    ga_mv_set(e2, GA_BLADE_E2, 1.0);

    Lv00MultiVector *e1_e2 = ga_geometric_product(e1, e2);
    TEST_ASSERT_NOT_NULL(e1_e2);
    TEST_ASSERT(approx_eq(ga_mv_get(e1_e2, GA_BLADE_E12), 1.0),
                "e1 * e2 should have e12 component = 1");

    /* Test 3: e2 * e1 = -e12 (anti-commutativity) */
    Lv00MultiVector *e2_e1 = ga_geometric_product(e2, e1);
    TEST_ASSERT_NOT_NULL(e2_e1);
    TEST_ASSERT(approx_eq(ga_mv_get(e2_e1, GA_BLADE_E12), -1.0),
                "e2 * e1 should have e12 component = -1");

    /* Test 4: e0 * e0 = 0 (null basis) */
    Lv00MultiVector *e0 = ga_mv_zero();
    ga_mv_set(e0, GA_BLADE_E0, 1.0);

    Lv00MultiVector *e0_sq = ga_geometric_product(e0, e0);
    TEST_ASSERT_NOT_NULL(e0_sq);
    for (int i = 0; i < GA_MV_DIM; i++) {
        TEST_ASSERT(approx_eq(ga_mv_get(e0_sq, i), 0.0),
                    "e0 * e0 should be zero");
    }

    ga_mv_destroy(e1);
    ga_mv_destroy(e2);
    ga_mv_destroy(e0);
    ga_mv_destroy(e1_sq);
    ga_mv_destroy(e1_e2);
    ga_mv_destroy(e2_e1);
    ga_mv_destroy(e0_sq);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Outer product
 * ======================================================================== */

void test_ga_outer_product(void) {
    printf("Testing ga_outer_product...\n");

    /* Test 1: e1 ^ e2 = e12 */
    Lv00MultiVector *e1 = ga_mv_zero();
    ga_mv_set(e1, GA_BLADE_E1, 1.0);

    Lv00MultiVector *e2 = ga_mv_zero();
    ga_mv_set(e2, GA_BLADE_E2, 1.0);

    Lv00MultiVector *wedge = ga_outer_product(e1, e2);
    TEST_ASSERT_NOT_NULL(wedge);
    TEST_ASSERT(approx_eq(ga_mv_get(wedge, GA_BLADE_E12), 1.0),
                "e1 ^ e2 should have e12 = 1");

    /* Test 2: e1 ^ e1 = 0 */
    Lv00MultiVector *self_wedge = ga_outer_product(e1, e1);
    TEST_ASSERT_NOT_NULL(self_wedge);
    for (int i = 0; i < GA_MV_DIM; i++) {
        TEST_ASSERT(approx_eq(ga_mv_get(self_wedge, i), 0.0),
                    "e1 ^ e1 should be zero");
    }

    ga_mv_destroy(e1);
    ga_mv_destroy(e2);
    ga_mv_destroy(wedge);
    ga_mv_destroy(self_wedge);
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
    TEST_ASSERT(approx_eq(ga_mv_get(s_rev, GA_BLADE_1), 5.0),
                "Reverse of scalar should be itself");

    /* Test 2: reverse of e1 (grade 1) is e1 (unchanged) */
    Lv00MultiVector *e1 = ga_mv_zero();
    ga_mv_set(e1, GA_BLADE_E1, 1.0);
    Lv00MultiVector *e1_rev = ga_reverse(e1);
    TEST_ASSERT_NOT_NULL(e1_rev);
    TEST_ASSERT(approx_eq(ga_mv_get(e1_rev, GA_BLADE_E1), 1.0),
                "Reverse of e1 should be e1");

    /* Test 3: reverse of e12 (grade 2) is -e12 */
    Lv00MultiVector *e12 = ga_mv_zero();
    ga_mv_set(e12, GA_BLADE_E12, 1.0);
    Lv00MultiVector *e12_rev = ga_reverse(e12);
    TEST_ASSERT_NOT_NULL(e12_rev);
    TEST_ASSERT(approx_eq(ga_mv_get(e12_rev, GA_BLADE_E12), -1.0),
                "Reverse of e12 should be -e12");

    /* Test 4: reverse of e123 (grade 3) is -e123 */
    Lv00MultiVector *e123 = ga_mv_zero();
    ga_mv_set(e123, GA_BLADE_E123, 1.0);
    Lv00MultiVector *e123_rev = ga_reverse(e123);
    TEST_ASSERT_NOT_NULL(e123_rev);
    TEST_ASSERT(approx_eq(ga_mv_get(e123_rev, GA_BLADE_E123), -1.0),
                "Reverse of e123 should be -e123");

    ga_mv_destroy(s);
    ga_mv_destroy(s_rev);
    ga_mv_destroy(e1);
    ga_mv_destroy(e1_rev);
    ga_mv_destroy(e12);
    ga_mv_destroy(e12_rev);
    ga_mv_destroy(e123);
    ga_mv_destroy(e123_rev);
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

    ga_mv_destroy(p);
    ga_mv_destroy(origin);
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

    ga_mv_destroy(p1); ga_mv_destroy(p2); ga_mv_destroy(p3);
    ga_mv_destroy(q1); ga_mv_destroy(q2); ga_mv_destroy(q3);
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

    ga_mv_destroy(p1); ga_mv_destroy(p2); ga_mv_destroy(p3); ga_mv_destroy(p4);
    ga_mv_destroy(q1); ga_mv_destroy(q2); ga_mv_destroy(q3); ga_mv_destroy(q4);
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
    ga_mv_set(e1, GA_BLADE_E1, 1.0);
    double ne1 = ga_norm_squared(e1);
    TEST_ASSERT(approx_eq(ne1, 1.0), "||e1||^2 should be 1.0");

    /* Norm of 2*e1 should be 4.0 */
    Lv00MultiVector *two_e1 = ga_mv_zero();
    ga_mv_set(two_e1, GA_BLADE_E1, 2.0);
    double n2e1 = ga_norm_squared(two_e1);
    TEST_ASSERT(approx_eq(n2e1, 4.0), "||2*e1||^2 should be 4.0");

    ga_mv_destroy(s);
    ga_mv_destroy(z);
    ga_mv_destroy(e1);
    ga_mv_destroy(two_e1);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Outer product comprehensive - all grade combinations
 * ======================================================================== */

void test_ga_outer_product_comprehensive(void) {
    printf("Testing ga_outer_product comprehensive (all grade combinations)...\n");

    /* ---- grade-1 ^ grade-1 -> grade-2: e1 ^ e2 = e12 ---- */
    {
        Lv00MultiVector *e1 = ga_mv_zero();
        Lv00MultiVector *e2 = ga_mv_zero();
        ga_mv_set(e1, GA_BLADE_E1, 1.0);
        ga_mv_set(e2, GA_BLADE_E2, 1.0);

        Lv00MultiVector *r = ga_outer_product(e1, e2);
        TEST_ASSERT_NOT_NULL(r);
        TEST_ASSERT(approx_eq(ga_mv_get(r, GA_BLADE_E12), 1.0),
                    "e1 ^ e2: e12 component should be 1");

        /* Verify all other components are zero */
        for (int i = 0; i < GA_MV_DIM; i++) {
            if (i == GA_BLADE_E12) continue;
            TEST_ASSERT(approx_eq(ga_mv_get(r, i), 0.0),
                        "e1 ^ e2: non-e12 components should be zero");
        }

        ga_mv_destroy(e1); ga_mv_destroy(e2); ga_mv_destroy(r);
    }

    /* ---- grade-1 ^ grade-2 -> grade-3: e0 ^ e12 = e012 ---- */
    {
        Lv00MultiVector *e0 = ga_mv_zero();
        Lv00MultiVector *e12 = ga_mv_zero();
        ga_mv_set(e0, GA_BLADE_E0, 1.0);
        ga_mv_set(e12, GA_BLADE_E12, 1.0);

        Lv00MultiVector *r = ga_outer_product(e0, e12);
        TEST_ASSERT_NOT_NULL(r);
        TEST_ASSERT(approx_eq(ga_mv_get(r, GA_BLADE_E012), 1.0),
                    "e0 ^ e12: e012 component should be 1");

        for (int i = 0; i < GA_MV_DIM; i++) {
            if (i == GA_BLADE_E012) continue;
            TEST_ASSERT(approx_eq(ga_mv_get(r, i), 0.0),
                        "e0 ^ e12: non-e012 components should be zero");
        }

        ga_mv_destroy(e0); ga_mv_destroy(e12); ga_mv_destroy(r);
    }

    /* ---- grade-2 ^ grade-1 -> grade-3: e12 ^ e3 = e123 ---- */
    {
        Lv00MultiVector *e12 = ga_mv_zero();
        Lv00MultiVector *e3 = ga_mv_zero();
        ga_mv_set(e12, GA_BLADE_E12, 1.0);
        ga_mv_set(e3, GA_BLADE_E3, 1.0);

        Lv00MultiVector *r = ga_outer_product(e12, e3);
        TEST_ASSERT_NOT_NULL(r);
        TEST_ASSERT(approx_eq(ga_mv_get(r, GA_BLADE_E123), 1.0),
                    "e12 ^ e3: e123 component should be 1");

        for (int i = 0; i < GA_MV_DIM; i++) {
            if (i == GA_BLADE_E123) continue;
            TEST_ASSERT(approx_eq(ga_mv_get(r, i), 0.0),
                        "e12 ^ e3: non-e123 components should be zero");
        }

        ga_mv_destroy(e12); ga_mv_destroy(e3); ga_mv_destroy(r);
    }

    /* ---- grade-2 ^ grade-2 -> grade-4: e01 ^ e23 = e0123 ---- */
    {
        Lv00MultiVector *e01 = ga_mv_zero();
        Lv00MultiVector *e23 = ga_mv_zero();
        ga_mv_set(e01, GA_BLADE_E01, 1.0);
        ga_mv_set(e23, GA_BLADE_E23, 1.0);

        Lv00MultiVector *r = ga_outer_product(e01, e23);
        TEST_ASSERT_NOT_NULL(r);
        TEST_ASSERT(approx_eq(ga_mv_get(r, GA_BLADE_E0123), 1.0),
                    "e01 ^ e23: e0123 component should be 1");

        for (int i = 0; i < GA_MV_DIM; i++) {
            if (i == GA_BLADE_E0123) continue;
            TEST_ASSERT(approx_eq(ga_mv_get(r, i), 0.0),
                        "e01 ^ e23: non-e0123 components should be zero");
        }

        ga_mv_destroy(e01); ga_mv_destroy(e23); ga_mv_destroy(r);
    }

    /* ---- scalar ^ anything = anything: 3.0 ^ e1 = 3*e1 ---- */
    {
        Lv00MultiVector *s = ga_mv_scalar(3.0);
        Lv00MultiVector *e1 = ga_mv_zero();
        ga_mv_set(e1, GA_BLADE_E1, 1.0);

        Lv00MultiVector *r = ga_outer_product(s, e1);
        TEST_ASSERT_NOT_NULL(r);
        TEST_ASSERT(approx_eq(ga_mv_get(r, GA_BLADE_E1), 3.0),
                    "3.0 ^ e1: e1 component should be 3.0");

        for (int i = 0; i < GA_MV_DIM; i++) {
            if (i == GA_BLADE_E1) continue;
            TEST_ASSERT(approx_eq(ga_mv_get(r, i), 0.0),
                        "3.0 ^ e1: non-e1 components should be zero");
        }

        ga_mv_destroy(s); ga_mv_destroy(e1); ga_mv_destroy(r);
    }

    /* ---- anything ^ scalar = anything: e1 ^ 3.0 = 3*e1 ---- */
    {
        Lv00MultiVector *e1 = ga_mv_zero();
        ga_mv_set(e1, GA_BLADE_E1, 1.0);
        Lv00MultiVector *s = ga_mv_scalar(3.0);

        Lv00MultiVector *r = ga_outer_product(e1, s);
        TEST_ASSERT_NOT_NULL(r);
        TEST_ASSERT(approx_eq(ga_mv_get(r, GA_BLADE_E1), 3.0),
                    "e1 ^ 3.0: e1 component should be 3.0");

        for (int i = 0; i < GA_MV_DIM; i++) {
            if (i == GA_BLADE_E1) continue;
            TEST_ASSERT(approx_eq(ga_mv_get(r, i), 0.0),
                        "e1 ^ 3.0: non-e1 components should be zero");
        }

        ga_mv_destroy(e1); ga_mv_destroy(s); ga_mv_destroy(r);
    }

    /* ---- same basis element ^ itself = 0: e1 ^ e1 = 0 ---- */
    {
        Lv00MultiVector *e1a = ga_mv_zero();
        Lv00MultiVector *e1b = ga_mv_zero();
        ga_mv_set(e1a, GA_BLADE_E1, 1.0);
        ga_mv_set(e1b, GA_BLADE_E1, 1.0);

        Lv00MultiVector *r = ga_outer_product(e1a, e1b);
        TEST_ASSERT_NOT_NULL(r);
        for (int i = 0; i < GA_MV_DIM; i++) {
            TEST_ASSERT(approx_eq(ga_mv_get(r, i), 0.0),
                        "e1 ^ e1: all components should be zero");
        }

        ga_mv_destroy(e1a); ga_mv_destroy(e1b); ga_mv_destroy(r);
    }

    /* ---- additional: e2 ^ e2 = 0 ---- */
    {
        Lv00MultiVector *e2a = ga_mv_zero();
        Lv00MultiVector *e2b = ga_mv_zero();
        ga_mv_set(e2a, GA_BLADE_E2, 1.0);
        ga_mv_set(e2b, GA_BLADE_E2, 1.0);

        Lv00MultiVector *r = ga_outer_product(e2a, e2b);
        TEST_ASSERT_NOT_NULL(r);
        for (int i = 0; i < GA_MV_DIM; i++) {
            TEST_ASSERT(approx_eq(ga_mv_get(r, i), 0.0),
                        "e2 ^ e2: all components should be zero");
        }

        ga_mv_destroy(e2a); ga_mv_destroy(e2b); ga_mv_destroy(r);
    }

    /* ---- additional: e0 ^ e0 = 0 (null basis) ---- */
    {
        Lv00MultiVector *e0a = ga_mv_zero();
        Lv00MultiVector *e0b = ga_mv_zero();
        ga_mv_set(e0a, GA_BLADE_E0, 1.0);
        ga_mv_set(e0b, GA_BLADE_E0, 1.0);

        Lv00MultiVector *r = ga_outer_product(e0a, e0b);
        TEST_ASSERT_NOT_NULL(r);
        for (int i = 0; i < GA_MV_DIM; i++) {
            TEST_ASSERT(approx_eq(ga_mv_get(r, i), 0.0),
                        "e0 ^ e0: all components should be zero");
        }

        ga_mv_destroy(e0a); ga_mv_destroy(e0b); ga_mv_destroy(r);
    }

    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Outer product anticommutativity
 * ======================================================================== */

void test_ga_outer_product_anticommutativity(void) {
    printf("Testing ga_outer_product anticommutativity...\n");

    /* e1 ^ e2 = -(e2 ^ e1) */
    Lv00MultiVector *e1 = ga_mv_zero();
    Lv00MultiVector *e2 = ga_mv_zero();
    ga_mv_set(e1, GA_BLADE_E1, 1.0);
    ga_mv_set(e2, GA_BLADE_E2, 1.0);

    Lv00MultiVector *r12 = ga_outer_product(e1, e2);
    Lv00MultiVector *r21 = ga_outer_product(e2, e1);
    TEST_ASSERT_NOT_NULL(r12);
    TEST_ASSERT_NOT_NULL(r21);

    /* r12 + r21 should be zero for all components */
    for (int i = 0; i < GA_MV_DIM; i++) {
        double sum = ga_mv_get(r12, i) + ga_mv_get(r21, i);
        TEST_ASSERT(approx_eq(sum, 0.0),
                    "e1^e2 + e2^e1: all components should sum to zero (anticommutativity)");
    }

    /* Specifically check e12: e1^e2 = +e12, e2^e1 = -e12 */
    TEST_ASSERT(approx_eq(ga_mv_get(r12, GA_BLADE_E12), 1.0),
                "e1 ^ e2: e12 should be +1");
    TEST_ASSERT(approx_eq(ga_mv_get(r21, GA_BLADE_E12), -1.0),
                "e2 ^ e1: e12 should be -1");

    /* Also test with e0 and e3 to cover more pairs */
    Lv00MultiVector *e0 = ga_mv_zero();
    Lv00MultiVector *e3 = ga_mv_zero();
    ga_mv_set(e0, GA_BLADE_E0, 1.0);
    ga_mv_set(e3, GA_BLADE_E3, 1.0);

    Lv00MultiVector *r03 = ga_outer_product(e0, e3);
    Lv00MultiVector *r30 = ga_outer_product(e3, e0);
    TEST_ASSERT_NOT_NULL(r03);
    TEST_ASSERT_NOT_NULL(r30);

    TEST_ASSERT(approx_eq(ga_mv_get(r03, GA_BLADE_E03), 1.0),
                "e0 ^ e3: e03 should be +1");
    TEST_ASSERT(approx_eq(ga_mv_get(r30, GA_BLADE_E03), -1.0),
                "e3 ^ e0: e03 should be -1");

    for (int i = 0; i < GA_MV_DIM; i++) {
        double sum = ga_mv_get(r03, i) + ga_mv_get(r30, i);
        TEST_ASSERT(approx_eq(sum, 0.0),
                    "e0^e3 + e3^e0: all components should sum to zero (anticommutativity)");
    }

    ga_mv_destroy(e1); ga_mv_destroy(e2);
    ga_mv_destroy(r12); ga_mv_destroy(r21);
    ga_mv_destroy(e0); ga_mv_destroy(e3);
    ga_mv_destroy(r03); ga_mv_destroy(r30);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Outer product associativity: (a^b)^c = a^(b^c)
 * ======================================================================== */

void test_ga_outer_product_associativity(void) {
    printf("Testing ga_outer_product associativity...\n");

    /* Use three grade-1 elements: e0, e1, e2
     * (e0 ^ e1) ^ e2 should equal e0 ^ (e1 ^ e2) = e012 */
    Lv00MultiVector *e0 = ga_mv_zero();
    Lv00MultiVector *e1 = ga_mv_zero();
    Lv00MultiVector *e2 = ga_mv_zero();
    ga_mv_set(e0, GA_BLADE_E0, 1.0);
    ga_mv_set(e1, GA_BLADE_E1, 1.0);
    ga_mv_set(e2, GA_BLADE_E2, 1.0);

    /* Left association: (e0 ^ e1) ^ e2 */
    Lv00MultiVector *e01 = ga_outer_product(e0, e1);
    TEST_ASSERT_NOT_NULL(e01);
    Lv00MultiVector *left = ga_outer_product(e01, e2);
    TEST_ASSERT_NOT_NULL(left);

    /* Right association: e0 ^ (e1 ^ e2) */
    Lv00MultiVector *e12 = ga_outer_product(e1, e2);
    TEST_ASSERT_NOT_NULL(e12);
    Lv00MultiVector *right = ga_outer_product(e0, e12);
    TEST_ASSERT_NOT_NULL(right);

    /* Both should equal e012 */
    TEST_ASSERT(approx_eq(ga_mv_get(left, GA_BLADE_E012), 1.0),
                "(e0^e1)^e2: e012 should be 1");
    TEST_ASSERT(approx_eq(ga_mv_get(right, GA_BLADE_E012), 1.0),
                "e0^(e1^e2): e012 should be 1");

    /* All components of left and right should be equal */
    for (int i = 0; i < GA_MV_DIM; i++) {
        TEST_ASSERT(approx_eq(ga_mv_get(left, i), ga_mv_get(right, i)),
                    "(a^b)^c = a^(b^c): all components should match (associativity)");
    }

    /* Also test with different elements: e1, e2, e3
     * (e1 ^ e2) ^ e3 should equal e1 ^ (e2 ^ e3) = e123 */
    Lv00MultiVector *e3 = ga_mv_zero();
    ga_mv_set(e3, GA_BLADE_E3, 1.0);

    Lv00MultiVector *e12_v2 = ga_outer_product(e1, e2);
    Lv00MultiVector *left2 = ga_outer_product(e12_v2, e3);
    TEST_ASSERT_NOT_NULL(left2);

    Lv00MultiVector *e23 = ga_outer_product(e2, e3);
    Lv00MultiVector *right2 = ga_outer_product(e1, e23);
    TEST_ASSERT_NOT_NULL(right2);

    TEST_ASSERT(approx_eq(ga_mv_get(left2, GA_BLADE_E123), 1.0),
                "(e1^e2)^e3: e123 should be 1");
    TEST_ASSERT(approx_eq(ga_mv_get(right2, GA_BLADE_E123), 1.0),
                "e1^(e2^e3): e123 should be 1");

    for (int i = 0; i < GA_MV_DIM; i++) {
        TEST_ASSERT(approx_eq(ga_mv_get(left2, i), ga_mv_get(right2, i)),
                    "(a^b)^c = a^(b^c): all components should match (e1,e2,e3 case)");
    }

    ga_mv_destroy(e0); ga_mv_destroy(e1); ga_mv_destroy(e2); ga_mv_destroy(e3);
    ga_mv_destroy(e01); ga_mv_destroy(e12); ga_mv_destroy(left); ga_mv_destroy(right);
    ga_mv_destroy(e12_v2); ga_mv_destroy(e23); ga_mv_destroy(left2); ga_mv_destroy(right2);
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
    TEST_RUN(test_ga_outer_product_comprehensive);
    TEST_RUN(test_ga_outer_product_anticommutativity);
    TEST_RUN(test_ga_outer_product_associativity);
    TEST_RUN(test_ga_reverse);
    TEST_RUN(test_ga_embed_point_extract);
    TEST_RUN(test_ga_three_points_collinear);
    TEST_RUN(test_ga_four_points_coplanar);
    TEST_RUN(test_ga_norm_squared);

    TEST_SUITE_END();
    return g_fail_count > 0 ? 1 : 0;
}
