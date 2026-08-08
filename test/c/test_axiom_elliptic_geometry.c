/**
 * @file test_axiom_elliptic_geometry.c
 * @brief Elliptic Geometry Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the elliptic_geometry.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Elliptic geometry is the third classical non-Euclidean geometry (alongside
 * hyperbolic geometry). It is characterized by the absence of parallel lines:
 * any two distinct lines intersect at exactly one point. The single elliptic
 * plane is obtained from the sphere S^2 by identifying antipodal points.
 *
 * The 30 templates cover elliptic incidence, separation/cyclic order,
 * congruence (bounded segments), the elliptic parallel postulate,
 * pole-polar duality, angle excess, continuity, and models (spherical,
 * projective/Cayley-Klein, gnomonic projection).
 */

#include <stdio.h>
#include <string.h>

int g_fail_count = 0;
int g_pass_count = 0;

/* 历史私有 TEST_ASSERT 为非返回式语义（失败仅计数、继续执行），
 * 通过 AXIOM_TEST_NON_RETURNING 让骨架头提供兼容变体，保持行为不变 */
#include "test_helpers.h"

#include "axiom_test_common.h"

#define AXIOM_PKG_PATH "module/axiom_packages/elliptic_geometry.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/elliptic_geometry_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 30
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Elliptic Incidence Axioms (4) */
    {"line_through_two_points", 2},
    {"line_has_two_points", 1},
    {"existence_of_triangle", 0},
    {"any_two_lines_intersect", 2},
    /* Group II: Separation / Cyclic Order (4) */
    {"separation_relation", 4},
    {"separation_symmetry", 4},
    {"separation_transitivity", 5},
    {"separation_extension", 3},
    /* Group III: Congruence Axioms (6) */
    {"bounded_segment_transport", 4},
    {"segment_congruence_reflexive", 2},
    {"segment_congruence_transitive", 6},
    {"angle_transport", 5},
    {"angle_congruence_properties", 6},
    {"SAS_congruence", 6},
    /* Group IV: Elliptic Parallel Postulate (2) */
    {"no_parallel_lines", 2},
    {"projective_incidence_property", 1},
    /* Group V: Elliptic-Specific Properties (6) */
    {"absolute_polar_line", 1},
    {"absolute_pole", 1},
    {"elliptic_distance", 2},
    {"triangle_angle_excess", 3},
    {"polar_triangle", 3},
    {"similarity_implies_congruence", 6},
    /* Group VI: Continuity & Metric (3) */
    {"elliptic_archimedes_axiom", 4},
    {"elliptic_line_completeness", 0},
    {"elliptic_area", 3},
    /* Group VII: Model Constructions (3) */
    {"spherical_model", 1},
    {"projective_model", 1},
    {"gnomonic_projection", 2},
    /* Group VIII: Derived Constructors (2) */
    {"perpendicular_from_point", 2},
    {"elliptic_midpoint_pair", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "elliptic_geometry");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 30 constraint templates", k_templates, K_TEMPLATES_COUNT);
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
        "squaring_the_circle_elliptic",
        "angle_trisection_elliptic",
        "doubling_the_cube_elliptic",
        "regular_heptagon_elliptic",
        "constructible_length_characterization",
        "triangle_similarity_without_congruence",
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
    TEST_ASSERT(strcmp(pkg->bottom_geometry, "elliptic_plane_RP2") == 0,
                "bottom_geometry should be 'elliptic_plane_RP2'");
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
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "elliptic_geometry", EXPECTED_TEMPLATE_COUNT,
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
    printf("Test 10: Key elliptic geometry templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Elliptic parallel postulate and its consequences */
    const char *parallel_templates[] = {
        "no_parallel_lines",
        "projective_incidence_property",
        "any_two_lines_intersect",
    };

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, parallel_templates[i]);
        TEST_ASSERT(tmpl != NULL, "parallel postulate template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Separation axioms (replace betweenness) */
    const char *separation_templates[] = {
        "separation_relation",
        "separation_symmetry",
        "separation_transitivity",
        "separation_extension",
    };

    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, separation_templates[i]);
        TEST_ASSERT(tmpl != NULL, "separation template should exist");
    }

    /* Pole-polar duality */
    const char *duality_templates[] = {
        "absolute_polar_line",
        "absolute_pole",
        "polar_triangle",
    };

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, duality_templates[i]);
        TEST_ASSERT(tmpl != NULL, "pole-polar duality template should exist");
    }

    /* Elliptic-specific properties */
    const char *specific_templates[] = {
        "elliptic_distance",
        "triangle_angle_excess",
        "similarity_implies_congruence",
        "elliptic_midpoint_pair",
    };

    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, specific_templates[i]);
        TEST_ASSERT(tmpl != NULL, "elliptic-specific template should exist");
    }

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Elliptic Geometry Axiom Package Tests")
    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructibles);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_save_load_roundtrip);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);
    TEST_MAIN_RUN(test_external_references);
    TEST_MAIN_RUN(test_key_templates);
TEST_MAIN_END()
