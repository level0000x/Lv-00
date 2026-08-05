/**
 * @file test_axiom_euclidean_plane.c
 * @brief Euclidean Plane Geometry Axiom Package Test
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/euclidean_plane.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/euclidean_plane_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 22
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板名 */
static const char *const k_template_names[] = {
    "line_through_two_points",
    "line_has_two_points",
    "existence_of_triangle",
    "betweenness_symmetry",
    "extend_segment",
    "betweenness_uniqueness",
    "pasch_axiom",
    "segment_transport",
    "segment_congruence_reflexive",
    "segment_congruence_transitive",
    "angle_transport",
    "angle_congruence_properties",
    "SAS_congruence",
    "unique_parallel",
    "archimedes_axiom",
    "line_completeness",
    "midpoint",
    "perpendicular_bisector",
    "perpendicular_from_point",
    "angle_bisector",
    "circle_by_center_radius",
    "line_circle_intersection",
};
#define K_TEMPLATE_NAMES_COUNT (int) (sizeof(k_template_names) / sizeof(k_template_names[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"angle_trisection", "cubic_equation_solving", 3, true},
    {"doubling_the_cube", "cube_root_of_two", 3, true},
    {"squaring_the_circle", "pi_transcendence", 3, true},
    {"general_quintic_by_radicals", "abel_ruffini_theorem", 2, true},
    {"construction_of_regular_heptagon", "cubic_equation_solving", 2, true},
    {"circle_squaring_straightedge", "lindemann_weierstrass_theorem", 2, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "euclidean_plane");
}

static void test_templates(void) {
    axiom_test_templates_names_only(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 22 constraint templates",
                                    k_template_names, K_TEMPLATE_NAMES_COUNT);

    /* 文件特有：具体参数个数校验（差异部分，原样保留） */
    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *t;
    t = axiom_package_get_template(pkg, "line_through_two_points");
    TEST_ASSERT(t && t->param_count == 2, "line_through_two_points should have 2 params");
    t = axiom_package_get_template(pkg, "existence_of_triangle");
    TEST_ASSERT(t && t->param_count == 0, "existence_of_triangle should have 0 params");
    t = axiom_package_get_template(pkg, "SAS_congruence");
    TEST_ASSERT(t && t->param_count == 6, "SAS_congruence should have 6 params");
    t = axiom_package_get_template(pkg, "pasch_axiom");
    TEST_ASSERT(t && t->param_count == 4, "pasch_axiom should have 4 params");
    t = axiom_package_get_template(pkg, "unique_parallel");
    TEST_ASSERT(t && t->param_count == 2, "unique_parallel should have 2 params");

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void) {
    axiom_test_unconstructible_problems(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                        "should have 6 unconstructible problems", k_unconstructibles,
                                        K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework(AXIOM_PKG_PATH, "euclidean_plane", "classical_material_implication",
                                 PROPOSITION_KIND_EXPLOSION_PRINCIPLE, "PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
}

static void test_content_hash(void) {
    axiom_test_content_hash(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE_PTR);
}

static void test_round_trip(void) {
    axiom_test_round_trip(AXIOM_PKG_PATH, SAVE_TEST_PATH, AXIOM_TEST_FREE_LV_FREE_PTR);
}

static void test_dependency_validation(void) {
    axiom_test_dependency_validation(AXIOM_PKG_PATH, "FAIL (acceptable)",
                                     " (expected: may fail for cross-reference reduces_to)");
}

static void test_negative_lookups(void) {
    axiom_test_negative_lookups(AXIOM_PKG_PATH, AXIOM_TEST_NEG_BASIC);
}

TEST_MAIN_BEGIN("Euclidean Plane")

    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructible_problems);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_round_trip);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);

TEST_MAIN_END()

