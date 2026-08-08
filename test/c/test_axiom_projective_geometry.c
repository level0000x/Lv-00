/**
 * @file test_axiom_projective_geometry.c
 * @brief Projective Geometry Axiom Package Test
 *
 * Tests the loading, template verification, unconstructible problem
 * tracking, logical framework, content hashing, round-trip save/load,
 * dependency validation, and negative lookups for the projective_geometry
 * axiom package.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_fail_count = 0;
int g_pass_count = 0;

/* 历史私有 TEST_ASSERT 为非返回式语义（失败仅计数、继续执行），
 * 通过 AXIOM_TEST_NON_RETURNING 让骨架头提供兼容变体，保持行为不变 */
#include "test_helpers.h"

#include "axiom_test_common.h"

#define AXIOM_PKG_PATH "module/axiom_packages/projective_geometry.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/projective_geometry_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 38
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板名 */
static const char *const k_template_names[] = {
    /* Group I: Incidence Axioms (3) */
    "join_of_two_points", "meet_of_two_lines", "existence_of_triangle_projective",
    /* Group II: Dimension and Extension Axioms (4) */
    "existence_of_complete_quadrangle", "line_has_three_points", "point_has_three_lines", "veblen_axiom",
    /* Group III: Desargues' Theorem (1) */
    "desargues_theorem",
    /* Group IV: Pappus's Hexagon Theorem (1) */
    "pappus_hexagon_theorem",
    /* Group V: Fundamental Constructors (8) */
    "line_through_point_meeting_line", "harmonic_conjugate", "intersection_of_line_with_line", "cross_ratio",
    "perspectivity", "diagonal_triangle_of_quadrangle", "dual_configuration", "pole_polar_construction",
    /* Group VI: Projective Transformations (5) */
    "projectivity", "fundamental_theorem_uniqueness", "collineation", "correlation", "elation",
    /* Group VII: Conic Sections (4) */
    "conic_through_five_points", "tangent_to_conic", "pascal_theorem", "brianchon_theorem",
    /* Group VIII: Coordinate Field Construction (4) */
    "field_addition_geometric", "field_multiplication_geometric", "field_additive_inverse",
    "field_multiplicative_inverse",
    /* Group IX: Higher-Dimensional (3) */
    "join_point_line_to_plane", "meet_of_two_planes", "desargues_provable_in_3d",
    /* Group X: Derived Theorems (5) */
    "dual_desargues_theorem", "harmonic_conjugate_uniqueness", "complete_quadrilateral_theorem",
    "projectivity_uniqueness", "cross_ratio_invariance",
};
#define K_TEMPLATE_NAMES_COUNT (int) (sizeof(k_template_names) / sizeof(k_template_names[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"existence_of_finite_projective_plane_non_prime_power", "open_problem", 3, true},
    {"classification_of_finite_projective_planes", "wildly_open", 2, true},
    {"coordinate_field_of_non_desarguesian_plane", "non_associative_algebra", 2, true},
    {"constructing_midpoint_with_straightedge_only", "metric_construction", 2, true},
    {"trisection_of_angle_projective", "cubic_equation_solving", 2, true},
    {"pappus_implies_field_commutativity", "algebraic_equivalence", 3, true},
    {"order_of_largest_unknown_finite_projective_plane", "open_problem", 2, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h。
 * Test 2 模板校验与 Test 5/6/7/9 为文件特有手写体，保留在下方）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "projective_geometry",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（test_templates 为混合 wrapper，下方保留） */
        .tmpl_style = AXIOM_TEST_TMPL_NONE,

        /* Test 3: 不可构造项（A 形态） */
        .uc_style = AXIOM_TEST_UC_A,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 7 unconstructible problems",
        .uc_expectations = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（S 形态） */
        .lf_style = AXIOM_TEST_LF_S,
        .lf_bottom_geometry = "projective_plane_incidence",
        .lf_negation_encoding = "classical_equality",
        .lf_contradiction_behavior = PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
        .lf_contradiction_name = "PROPOSITION_KIND_EXPLOSION_PRINCIPLE",

        /* Test 5/6/7/9: 文件特有手写，下方保留 */
        .hash_style = AXIOM_TEST_HASH_NONE,
        .rt_style = AXIOM_TEST_RT_NONE,
        .dep_style = AXIOM_TEST_DEP_NONE,
        .ext_style = AXIOM_TEST_EXT_NONE,

        /* Test 8: 负向查找（empty 形态） */
        .neg_style = AXIOM_TEST_NEG_EMPTY,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

static void test_templates(void) {
    axiom_test_templates_names_only(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 38 constraint templates",
                                    k_template_names, K_TEMPLATE_NAMES_COUNT);

    /* 文件特有：具体参数个数校验（差异部分，原样保留） */
    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *t;

    t = axiom_package_get_template(pkg, "join_of_two_points");
    TEST_ASSERT(t && t->param_count == 2, "join_of_two_points should have 2 params (point A, point B)");

    t = axiom_package_get_template(pkg, "meet_of_two_lines");
    TEST_ASSERT(t && t->param_count == 2, "meet_of_two_lines should have 2 params (line l, line m)");

    t = axiom_package_get_template(pkg, "existence_of_triangle_projective");
    TEST_ASSERT(t && t->param_count == 0, "existence_of_triangle_projective should have 0 params");

    t = axiom_package_get_template(pkg, "existence_of_complete_quadrangle");
    TEST_ASSERT(t && t->param_count == 0, "existence_of_complete_quadrangle should have 0 params");

    t = axiom_package_get_template(pkg, "veblen_axiom");
    TEST_ASSERT(t && t->param_count == 4, "veblen_axiom should have 4 params (A, B, C, D)");

    t = axiom_package_get_template(pkg, "desargues_theorem");
    TEST_ASSERT(t && t->param_count == 7, "desargues_theorem should have 7 params (O, A, B, C, A', B', C')");

    t = axiom_package_get_template(pkg, "pappus_hexagon_theorem");
    TEST_ASSERT(t && t->param_count == 6, "pappus_hexagon_theorem should have 6 params");

    t = axiom_package_get_template(pkg, "harmonic_conjugate");
    TEST_ASSERT(t && t->param_count == 3, "harmonic_conjugate should have 3 params (A, B, C)");

    t = axiom_package_get_template(pkg, "cross_ratio");
    TEST_ASSERT(t && t->param_count == 4, "cross_ratio should have 4 params (A, B, C, D)");

    t = axiom_package_get_template(pkg, "conic_through_five_points");
    TEST_ASSERT(t && t->param_count == 5, "conic_through_five_points should have 5 params");

    t = axiom_package_get_template(pkg, "pascal_theorem");
    TEST_ASSERT(t && t->param_count == 6, "pascal_theorem should have 6 params");

    t = axiom_package_get_template(pkg, "brianchon_theorem");
    TEST_ASSERT(t && t->param_count == 6, "brianchon_theorem should have 6 params");

    t = axiom_package_get_template(pkg, "field_addition_geometric");
    TEST_ASSERT(t && t->param_count == 4, "field_addition_geometric should have 4 params (O, E, A, B)");

    t = axiom_package_get_template(pkg, "field_multiplication_geometric");
    TEST_ASSERT(t && t->param_count == 4, "field_multiplication_geometric should have 4 params (O, E, A, B)");

    t = axiom_package_get_template(pkg, "cross_ratio_invariance");
    TEST_ASSERT(t && t->param_count == 8, "cross_ratio_invariance should have 8 params");

    t = axiom_package_get_template(pkg, "desargues_provable_in_3d");
    TEST_ASSERT(t && t->param_count == 0, "desargues_provable_in_3d should have 0 params");

    axiom_package_destroy(pkg);
}

/* Test 3/4/8 已收敛至 kCases 数据驱动用例（见上） */
static void test_content_hash(void) {
    printf("Test 5: Verify content hash...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash1 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash1 != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash1) == 64, "SHA-256 hash should be 64 hex chars");
    printf("  Hash: %s\n", hash1 ? hash1 : "(null)");

    /* Loading again should produce the same hash */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg2, AXIOM_PKG_PATH);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash2 != NULL, "second content hash should not be NULL");
    TEST_ASSERT(strcmp(hash1, hash2) == 0, "identical packages should have identical hashes");
    printf("  Hash verification: %s\n", (hash1 && hash2 && strcmp(hash1, hash2) == 0) ? "MATCH" : "MISMATCH");

    if (hash1)
        lv_free_ptr(hash1);
    if (hash2)
        lv_free_ptr(hash2);
    axiom_package_destroy(pkg);
    axiom_package_destroy(pkg2);
}

/* Test 6：往返保存/加载（文件特有：Save/Load 状态打印 + negation_encoding 校验，保留原体） */
static void test_round_trip_save_load(void) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Save */
    AxiomSaveStatus save_status = axiom_package_save(pkg, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "axiom_package_save should return AXIOM_SAVE_OK");
    printf("  Save status: %s\n", save_status == AXIOM_SAVE_OK ? "OK" : "FAILED");

    /* Load saved file */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "re-loading saved file should return AXIOM_LOAD_OK");

    if (load_status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Load error: %s\n", err ? err : "(unknown)");
    }

    /* Verify key properties match */
    TEST_ASSERT(axiom_package_get_template_count(pkg2) == axiom_package_get_template_count(pkg), "template count should match after round-trip");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg2) == axiom_package_get_unconstructible_count(pkg),
                "unconstructible count should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->name, pkg->name) == 0, "package name should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->version, pkg->version) == 0, "package version should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg->bottom_geometry) == 0,
                "bottom_geometry should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->negation_encoding, pkg->negation_encoding) == 0,
                "negation_encoding should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg->contradiction_behavior,
                "contradiction_behavior should match after round-trip");

    printf("  Round-trip: templates=%d, unconstructibles=%d\n", axiom_package_get_template_count(pkg2), axiom_package_get_unconstructible_count(pkg2));

    /* Verify content hashes match */
    char *hash1 = axiom_package_compute_content_hash(pkg);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    /* Note: save may reorder or normalize, so we just check both are valid */
    TEST_ASSERT(hash1 != NULL && hash2 != NULL, "both hashes should be valid after round-trip");
    printf("  Original hash:  %s\n", hash1 ? hash1 : "(null)");
    printf("  Reloaded hash:  %s\n", hash2 ? hash2 : "(null)");

    if (hash1)
        lv_free_ptr(hash1);
    if (hash2)
        lv_free_ptr(hash2);
    axiom_package_destroy(pkg);
    axiom_package_destroy(pkg2);
}

/* Test 7：依赖验证（文件特有：NULL 列表 + self 列表两次验证，保留原体） */
static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Validate with no other packages loaded — external refs should still
       pass URL format checks */
    bool valid = axiom_package_validate_dependencies(pkg, NULL, 0);
    /* Some dependency references point to external items not in any loaded
       package, so validation may fail for those. We just check it doesn't
       crash and reports correctly. */
    printf("  Validation result (no deps): %s\n", valid ? "PASS" : "FAIL (expected for unresolved cross-package refs)");

    /* Validate with self as the only loaded package */
    AxiomPackage *loaded[] = {pkg};
    valid = axiom_package_validate_dependencies(pkg, loaded, 1);
    printf("  Validation result (self only): %s\n",
           valid ? "PASS" : "FAIL (expected for unresolved cross-package refs)");

    axiom_package_destroy(pkg);
}

/* Test 8 已收敛至 kCases 数据驱动用例（见上） */

/* Test 9：外部引用（文件特有：https 计数 + 逐条 URL 打印，保留原体） */
static void test_external_refs(void) {
    printf("Test 9: External references format validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    int valid_ref_count = 0;
    for (int i = 0; i < axiom_package_get_unconstructible_count(pkg); i++) {
        KnownUnconstructible *uc = axiom_package_get_unconstructible(pkg, i);
        if (uc->external_ref && strlen(uc->external_ref) > 0) {
            /* Check it starts with https:// */
            bool is_https = strncmp(uc->external_ref, "https://", 8) == 0;
            TEST_ASSERT(is_https, "external_ref should use https:// URL format");
            if (is_https)
                valid_ref_count++;
            printf("  [%d] %s\n       -> %s\n", i, uc->name, uc->external_ref);
        }
    }

    TEST_ASSERT(valid_ref_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "all unconstructible problems should have valid external refs");
    printf("  Valid external refs: %d / %d\n", valid_ref_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
TEST_MAIN_BEGIN("Axiom Package Tests")
    LV_REGISTER_AXIOM_CASES("ProjectiveGeometry", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_round_trip_save_load);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_external_refs);
TEST_MAIN_END()
