/**
 * @file test_geo_topology.c
 * @brief Tests for the geometric topology module.
 *
 * @details Tests cover:
 *          - Simplicial complex creation and destruction
 *          - Edge addition
 *          - Triangle addition (with automatic edge creation)
 *          - Euler characteristic of a single triangle
 *          - Euler characteristic of a tetrahedron surface
 *          - Boundary of a triangle
 *          - Connected components (single component)
 *          - Connected components (disconnected graph)
 *          - NULL safety
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "geo_topology.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Test: Create and destroy
 * ============================================================ */

static void test_simplicial_create_destroy(void) {
    lvSimplicialComplex *sc = geo_simplicial_create(5);
    TEST_ASSERT_NOT_NULL(sc);
    TEST_ASSERT_EQ(sc->n_vertices, 5);
    TEST_ASSERT_EQ(sc->n_edges, 0);
    TEST_ASSERT_EQ(sc->n_triangles, 0);

    geo_simplicial_destroy(sc);

    /* NULL-safe destroy */
    geo_simplicial_destroy(NULL);

    /* Create with zero vertices */
    sc = geo_simplicial_create(0);
    TEST_ASSERT_NOT_NULL(sc);
    TEST_ASSERT_EQ(sc->n_vertices, 0);
    geo_simplicial_destroy(sc);

    /* Create with negative vertices should fail */
    sc = geo_simplicial_create(-1);
    TEST_ASSERT_NULL(sc);
}

/* ============================================================
 * Test: Edge addition
 * ============================================================ */

static void test_add_edge(void) {
    lvSimplicialComplex *sc = geo_simplicial_create(4);
    TEST_ASSERT_NOT_NULL(sc);

    /* Add edge 0-1 */
    bool ok = geo_simplicial_add_edge(sc, 0, 1);
    TEST_ASSERT(ok, "add edge 0-1 should succeed");
    TEST_ASSERT_EQ(sc->n_edges, 1);
    TEST_ASSERT_EQ(sc->edges[0].v0, 0);
    TEST_ASSERT_EQ(sc->edges[0].v1, 1);

    /* Add edge 1-2 */
    ok = geo_simplicial_add_edge(sc, 1, 2);
    TEST_ASSERT(ok, "add edge 1-2 should succeed");
    TEST_ASSERT_EQ(sc->n_edges, 2);

    /* Add edge 2-0 */
    ok = geo_simplicial_add_edge(sc, 2, 0);
    TEST_ASSERT(ok, "add edge 2-0 should succeed");
    TEST_ASSERT_EQ(sc->n_edges, 3);

    /* Duplicate edge should not increase count */
    ok = geo_simplicial_add_edge(sc, 0, 1);
    TEST_ASSERT(ok, "duplicate edge should not fail");
    TEST_ASSERT_EQ(sc->n_edges, 3);

    /* Reverse order should also be detected as duplicate */
    ok = geo_simplicial_add_edge(sc, 1, 0);
    TEST_ASSERT(ok, "reverse edge should not fail");
    TEST_ASSERT_EQ(sc->n_edges, 3);

    /* Self-loop should fail */
    ok = geo_simplicial_add_edge(sc, 0, 0);
    TEST_ASSERT(!ok, "self-loop should fail");

    /* Out-of-range vertex should fail */
    ok = geo_simplicial_add_edge(sc, 0, 10);
    TEST_ASSERT(!ok, "out-of-range vertex should fail");

    /* NULL complex should fail */
    ok = geo_simplicial_add_edge(NULL, 0, 1);
    TEST_ASSERT(!ok, "NULL complex should fail");

    geo_simplicial_destroy(sc);
}

/* ============================================================
 * Test: Triangle addition
 * ============================================================ */

static void test_add_triangle(void) {
    lvSimplicialComplex *sc = geo_simplicial_create(4);
    TEST_ASSERT_NOT_NULL(sc);

    /* Add triangle 0-1-2 */
    bool ok = geo_simplicial_add_triangle(sc, 0, 1, 2);
    TEST_ASSERT(ok, "add triangle 0-1-2 should succeed");
    TEST_ASSERT_EQ(sc->n_triangles, 1);
    /* Adding a triangle should also add its 3 boundary edges */
    TEST_ASSERT_EQ(sc->n_edges, 3);

    /* Duplicate triangle should not increase count */
    ok = geo_simplicial_add_triangle(sc, 2, 1, 0);
    TEST_ASSERT(ok, "duplicate triangle should not fail");
    TEST_ASSERT_EQ(sc->n_triangles, 1);

    /* Degenerate triangle (two same vertices) should fail */
    ok = geo_simplicial_add_triangle(sc, 0, 0, 1);
    TEST_ASSERT(!ok, "degenerate triangle should fail");

    /* Out-of-range triangle should fail */
    ok = geo_simplicial_add_triangle(sc, 0, 1, 10);
    TEST_ASSERT(!ok, "out-of-range triangle should fail");

    geo_simplicial_destroy(sc);
}

/* ============================================================
 * Test: Euler characteristic of a single triangle
 * ============================================================ */

static void test_euler_single_triangle(void) {
    /* A single triangle: V=3, E=3, F=1 => chi = 3 - 3 + 1 = 1 */
    lvSimplicialComplex *sc = geo_simplicial_create(3);
    TEST_ASSERT_NOT_NULL(sc);

    geo_simplicial_add_triangle(sc, 0, 1, 2);

    int chi = geo_simplicial_euler_characteristic(sc);
    TEST_ASSERT_EQ(chi, 1);

    geo_simplicial_destroy(sc);
}

/* ============================================================
 * Test: Euler characteristic of a tetrahedron surface
 * ============================================================ */

static void test_euler_tetrahedron(void) {
    /* Tetrahedron surface: V=4, E=6, F=4 => chi = 4 - 6 + 4 = 2 */
    lvSimplicialComplex *sc = geo_simplicial_create(4);
    TEST_ASSERT_NOT_NULL(sc);

    /* Four faces of a tetrahedron */
    geo_simplicial_add_triangle(sc, 0, 1, 2);
    geo_simplicial_add_triangle(sc, 0, 1, 3);
    geo_simplicial_add_triangle(sc, 0, 2, 3);
    geo_simplicial_add_triangle(sc, 1, 2, 3);

    TEST_ASSERT_EQ(sc->n_vertices, 4);
    TEST_ASSERT_EQ(sc->n_triangles, 4);
    /* 6 unique edges on a tetrahedron */
    TEST_ASSERT_EQ(sc->n_edges, 6);

    int chi = geo_simplicial_euler_characteristic(sc);
    TEST_ASSERT_EQ(chi, 2);

    geo_simplicial_destroy(sc);
}

/* ============================================================
 * Test: Boundary of a triangle
 * ============================================================ */

static void test_boundary_triangle(void) {
    lvSimplicialComplex *sc = geo_simplicial_create(3);
    TEST_ASSERT_NOT_NULL(sc);

    lvTriangle tri;
    tri.v0 = 0;
    tri.v1 = 1;
    tri.v2 = 2;

    lvBoundary *bnd = geo_simplicial_boundary(sc, &tri);
    TEST_ASSERT_NOT_NULL(bnd);
    TEST_ASSERT_EQ(bnd->n_edges, 3);

    /* Check boundary edges */
    /* Edges should be (0,1), (1,2), (0,2) */
    TEST_ASSERT_EQ(bnd->edges[0].v0, 0);
    TEST_ASSERT_EQ(bnd->edges[0].v1, 1);
    TEST_ASSERT_EQ(bnd->edges[1].v0, 1);
    TEST_ASSERT_EQ(bnd->edges[1].v1, 2);
    TEST_ASSERT_EQ(bnd->edges[2].v0, 0);
    TEST_ASSERT_EQ(bnd->edges[2].v1, 2);

    geo_simplicial_boundary_destroy(bnd);
    geo_simplicial_destroy(sc);

    /* NULL triangle */
    bnd = geo_simplicial_boundary(sc, NULL);
    /* sc is already destroyed, but we test NULL tri with a fresh sc */
    sc = geo_simplicial_create(3);
    bnd = geo_simplicial_boundary(sc, NULL);
    TEST_ASSERT_NULL(bnd);
    geo_simplicial_destroy(sc);

    /* NULL-safe destroy */
    geo_simplicial_boundary_destroy(NULL);
}

/* ============================================================
 * Test: Connected components - single component
 * ============================================================ */

static void test_connected_single(void) {
    lvSimplicialComplex *sc = geo_simplicial_create(3);
    TEST_ASSERT_NOT_NULL(sc);

    geo_simplicial_add_edge(sc, 0, 1);
    geo_simplicial_add_edge(sc, 1, 2);

    int cc = geo_simplicial_connected_components(sc);
    TEST_ASSERT_EQ(cc, 1);

    geo_simplicial_destroy(sc);
}

/* ============================================================
 * Test: Connected components - disconnected
 * ============================================================ */

static void test_connected_disconnected(void) {
    /* 5 vertices, 2 components: {0,1,2} and {3,4} */
    lvSimplicialComplex *sc = geo_simplicial_create(5);
    TEST_ASSERT_NOT_NULL(sc);

    geo_simplicial_add_edge(sc, 0, 1);
    geo_simplicial_add_edge(sc, 1, 2);
    geo_simplicial_add_edge(sc, 3, 4);

    int cc = geo_simplicial_connected_components(sc);
    TEST_ASSERT_EQ(cc, 2);

    geo_simplicial_destroy(sc);
}

/* ============================================================
 * Test: Connected components - isolated vertices
 * ============================================================ */

static void test_connected_isolated(void) {
    /* 4 vertices, no edges => 4 components */
    lvSimplicialComplex *sc = geo_simplicial_create(4);
    TEST_ASSERT_NOT_NULL(sc);

    int cc = geo_simplicial_connected_components(sc);
    TEST_ASSERT_EQ(cc, 4);

    geo_simplicial_destroy(sc);
}

/* ============================================================
 * Test: NULL safety
 * ============================================================ */

static void test_null_safety(void) {
    /* Euler characteristic of NULL */
    int chi = geo_simplicial_euler_characteristic(NULL);
    TEST_ASSERT_EQ(chi, 0);

    /* Connected components of NULL */
    int cc = geo_simplicial_connected_components(NULL);
    TEST_ASSERT_EQ(cc, 0);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void) {
    TEST_SUITE_BEGIN("Geo Topology");

    TEST_RUN(test_simplicial_create_destroy);
    TEST_RUN(test_add_edge);
    TEST_RUN(test_add_triangle);
    TEST_RUN(test_euler_single_triangle);
    TEST_RUN(test_euler_tetrahedron);
    TEST_RUN(test_boundary_triangle);
    TEST_RUN(test_connected_single);
    TEST_RUN(test_connected_disconnected);
    TEST_RUN(test_connected_isolated);
    TEST_RUN(test_null_safety);

    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}
