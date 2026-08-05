/**
 * @file test_axiom_hyperbolic_geometry.c
 * @brief Hyperbolic Geometry Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the hyperbolic_geometry.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Hyperbolic geometry arises from replacing Euclid's parallel postulate with
 * its negation: through a point not on a given line there exist infinitely
 * many lines parallel to the given line. The 29 templates cover incidence,
 * betweenness, congruence, hyperbolic parallelism, area theory, continuity,
 * and models (Poincare disk, half-plane, Klein).
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/hyperbolic_geometry.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/hyperbolic_geometry_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 29
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Incidence Axioms (3) */
    {"line_through_two_points", 2},
    {"line_has_two_points", 1},
    {"existence_of_triangle", 0},
    /* Group II: Betweenness Axioms (4) */
    {"betweenness_symmetry", 3},
    {"extend_segment", 2},
    {"betweenness_uniqueness", 3},
    {"pasch_axiom", 4},
    /* Group III: Congruence Axioms (6) */
    {"segment_transport", 4},
    {"segment_congruence_reflexive", 2},
    {"segment_congruence_transitive", 6},
    {"angle_transport", 5},
    {"angle_congruence_properties", 6},
    {"SAS_congruence", 6},
    /* Group IV: Hyperbolic Parallelism (3) */
    {"hyperbolic_parallel_existence", 2},
    {"parallel_through_point_not_unique", 2},
    {"limiting_parallel_ray", 3},
    /* Group V: Area Theory (6) */
    {"angle_of_parallelism", 2},
    {"common_perpendicular", 2},
    {"ultraparallel_line", 2},
    {"asymptotic_triangle", 3},
    {"saccheri_quadrilateral", 2},
    {"lambert_quadrilateral", 3},
    /* Group VI: Continuity (3) */
    {"archimedes_axiom", 4},
    {"line_completeness", 0},
    {"hyperbolic_distance", 2},
    /* Group VII: Models (4) */
    {"poincare_disk_model", 2},
    {"poincare_halfplane_model", 2},
    {"klein_model", 2},
    {"hyperbolic_isometry", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "hyperbolic_geometry");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 29 constraint templates", k_templates, K_TEMPLATES_COUNT);
}

/* Test 3：不可构造项（文件特有：仅名称数组 + green_verified 检查，保留原体） */
static void test_unconstructibles(void) {
    printf("Test 3: Verify unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "should have 6 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg),
           EXPECTED_UNCONSTRUCTIBLE_COUNT);

    /* Verify each expected unconstructible */
    const char *expected_uc[] = {
        "squaring_the_circle_hyperbolic", "angle_trisection_hyperbolic", "doubling_the_cube_hyperbolic",
        "regular_polygon_hyperbolic",     "area_of_triangle_trisection", "constructible_angle_characterization",
    };

    int uc_count = sizeof(expected_uc) / sizeof(expected_uc[0]);
    TEST_ASSERT(uc_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "local expected UC count should match");

    for (int i = 0; i < uc_count; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected_uc[i]);
        if (!uc) {
            printf("  FAIL: unconstructible '%s' not found\n", expected_uc[i]);
            g_fail_count++;
            continue;
        }
        TEST_ASSERT(uc->green_verified == true, "unconstructible should be green_verified");
    }

    axiom_package_destroy(pkg);
}

/* Test 4：逻辑框架（文件特有：negation_encoding 用 strstr 检查，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    TEST_ASSERT(strcmp(pkg->bottom_geometry, "hyperbolic_plane") == 0, "bottom_geometry should be 'hyperbolic_plane'");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    TEST_ASSERT(strstr(pkg->negation_encoding, "material_implication") != NULL,
                "negation_encoding should contain 'material_implication'");
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
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "hyperbolic_geometry", EXPECTED_TEMPLATE_COUNT,
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
    printf("Test 10: Key hyperbolic geometry templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Hyperbolic parallel postulate and its consequences */
    const char *parallel_templates[] = {
        "hyperbolic_parallel_existence",
        "parallel_through_point_not_unique",
        "limiting_parallel_ray",
    };

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, parallel_templates[i]);
        TEST_ASSERT(tmpl != NULL, "parallel postulate template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Area theory: angle of parallelism and quadrilaterals */
    const char *area_templates[] = {
        "angle_of_parallelism",
        "saccheri_quadrilateral",
        "lambert_quadrilateral",
    };

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, area_templates[i]);
        TEST_ASSERT(tmpl != NULL, "area theory template should exist");
    }

    /* Models */
    const char *model_templates[] = {
        "poincare_disk_model",
        "poincare_halfplane_model",
        "klein_model",
    };

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, model_templates[i]);
        TEST_ASSERT(tmpl != NULL, "model template should exist");
    }

    /* Congruence: SAS */
    ConstraintTemplate *sas = axiom_package_get_template(pkg, "SAS_congruence");
    TEST_ASSERT(sas != NULL, "SAS_congruence template should exist");
    TEST_ASSERT(sas->param_count == 6, "SAS_congruence should have 6 parameters");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Hyperbolic Geometry");

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
