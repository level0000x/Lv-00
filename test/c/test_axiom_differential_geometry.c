/**
 * @file test_axiom_differential_geometry.c
 * @brief Differential Geometry Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the differential_geometry.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Differential geometry provides the calculus on manifolds framework for Lv-00.
 * The 39 templates cover smooth manifolds, tensor fields, connections,
 * curvature, Riemannian geometry, and symplectic structures.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/differential_geometry.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/differential_geometry_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 39
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Manifold Basics (6) */
    {"smooth_manifold", 3},
    {"chart_atlas", 2},
    {"smooth_map", 3},
    {"tangent_space", 3},
    {"vector_field", 2},
    {"tensor_field", 3},
    /* Group II: Riemannian Geometry (6) */
    {"riemannian_metric", 2},
    {"metric_compatibility", 4},
    {"length_of_curve", 2},
    {"geodesic", 2},
    {"exponential_map", 3},
    {"volume_form", 2},
    /* Group III: Curvature (7) */
    {"riemann_curvature_tensor", 2},
    {"ricci_curvature", 2},
    {"scalar_curvature", 2},
    {"sectional_curvature", 3},
    {"gauss_bonnet_theorem", 2},
    {"gauss_curvature", 3},
    {"torsion_tensor", 4},
    /* Group IV: Connections (6) */
    {"levi_civita_connection", 2},
    {"covariant_derivative", 4},
    {"parallel_transport", 3},
    {"christoffel_symbols", 2},
    {"torsion_tensor", 4},
    {"submanifold", 3},
    /* Group V: Submanifolds (6) */
    {"immersion_embedding", 3},
    {"induced_metric", 3},
    {"second_fundamental_form", 2},
    {"mean_curvature", 3},
    {"shape_operator", 3},
    {"riemannian_manifold_complete", 1},
    /* Group VI: Completeness (4) */
    {"hopf_rinow_theorem", 1},
    {"constant_curvature", 2},
    {"maximally_symmetric", 2},
    {"space_form", 3},
    /* Group VII: Special Structures (5) */
    {"pseudo_riemannian_metric", 3},
    {"lorentzian_manifold", 2},
    {"symplectic_manifold", 2},
    {"complex_manifold", 2},
    {"kahler_metric", 3},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"geodesic_completeness_decision", "undecidable", 4, true},
    {"positive_mass_theorem", "open_problem", 5, true},
    {"exotic_sphere_existence", "undecidable", 4, true},
    {"poincare_conjecture_higher", "proved", 3, true},
    {"curvature_bounded_below", "open_problem", 4, true},
    {"symplectic_embedding", "undecidable", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "differential_geometry");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 39 constraint templates", k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructibles(void) {
    axiom_test_unconstructibles_min_deps(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                         "should have 6 unconstructible problems", k_unconstructibles,
                                         K_UNCONSTRUCTIBLES_COUNT);
}

/* Test 4：逻辑框架（文件特有：strstr 概念检查，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    TEST_ASSERT(strstr(pkg->bottom_geometry, "smooth_manifold") != NULL,
                "bottom_geometry should contain 'smooth_manifold'");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    TEST_ASSERT(
        strstr(pkg->negation_encoding, "complement") != NULL || strstr(pkg->negation_encoding, "metric") != NULL,
        "negation_encoding should contain 'complement' or 'metric'");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    axiom_test_content_hash_deterministic(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_save_load_roundtrip(void) {
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "differential_geometry", EXPECTED_TEMPLATE_COUNT,
                                    EXPECTED_UNCONSTRUCTIBLE_COUNT, AXIOM_TEST_FREE_LV_FREE);
}

static void test_dependency_validation(void) {
    axiom_test_dependency_validation_note(AXIOM_PKG_PATH, NULL);
}

static void test_negative_lookups(void) {
    axiom_test_negative_lookups(AXIOM_PKG_PATH, AXIOM_TEST_NEG_XYZ);
}

static void test_external_references(void) {
    axiom_test_external_refs_all(AXIOM_PKG_PATH);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_key_templates(void) {
    printf("Test 10: Key differential geometry templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core manifold templates */
    const char *manifold_core[] = {"smooth_manifold", "chart_atlas",  "smooth_map",
                                   "tangent_space",   "vector_field", "tensor_field"};

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, manifold_core[i]);
        TEST_ASSERT(tmpl != NULL, "manifold template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Riemannian geometry core */
    const char *riemannian_core[] = {"riemannian_metric", "geodesic", "exponential_map"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, riemannian_core[i]);
        TEST_ASSERT(tmpl != NULL, "Riemannian template should exist");
    }

    /* Curvature templates */
    ConstraintTemplate *rc = axiom_package_get_template(pkg, "riemann_curvature_tensor");
    TEST_ASSERT(rc != NULL, "Riemann curvature tensor should exist");
    ConstraintTemplate *gc = axiom_package_get_template(pkg, "gauss_curvature");
    TEST_ASSERT(gc != NULL, "Gauss curvature should exist");

    /* Connection templates */
    ConstraintTemplate *lc = axiom_package_get_template(pkg, "levi_civita_connection");
    TEST_ASSERT(lc != NULL, "Levi-Civita connection should exist");
    ConstraintTemplate *cd = axiom_package_get_template(pkg, "covariant_derivative");
    TEST_ASSERT(cd != NULL, "covariant derivative should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Differential Geometry");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructibles);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_save_load_roundtrip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_references);
    TEST_RUN(test_key_templates);

    TEST_SUMMARY();

    return 0;
}
