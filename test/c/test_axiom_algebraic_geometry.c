/**
 * @file test_axiom_algebraic_geometry.c
 * @brief Algebraic Geometry Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the algebraic_geometry.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Algebraic geometry provides the study of geometric objects defined by
 * polynomial equations for Lv-00. The 38 templates cover affine/projective
 * varieties, schemes, cohomology, and dimension theory.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/algebraic_geometry.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/algebraic_geometry_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 38
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Affine Geometry (6) */
    {"affine_space", 2},
    {"algebraic_set", 1},
    {"zariski_topology", 1},
    {"coordinate_ring", 1},
    {"hilbert_nullstellensatz", 2},
    {"ideal_variety_correspondence", 2},
    /* Group II: Projective Geometry (6) */
    {"projective_space", 1},
    {"projective_variety", 1},
    {"homogeneous_coordinates", 1},
    {"serre_duality", 2},
    {"projective_normality", 1},
    {"scheme_theory", 1},
    /* Group III: Scheme Theory (6) */
    {"affine_scheme", 1},
    {"locally_ringed_space", 1},
    {"separated_morphism", 1},
    {"proper_morphism", 1},
    {"morphism_of_schemes", 2},
    {"sheaf_of_rings", 1},
    /* Group IV: Sheaf Theory (6) */
    {"presheaf", 2},
    {"sheafification", 1},
    {"cohomology_group", 2},
    {"serre_criterion", 2},
    {"leray_cover", 2},
    {"krull_dimension", 1},
    /* Group V: Dimension Theory (6) */
    {"codimension", 2},
    {"dimension_theorem", 2},
    {"transcendence_degree", 1},
    {"regular_local_ring", 1},
    {"algebraic_curve", 1},
    {"elliptic_curve_scheme", 1},
    /* Group VI: Special Varieties (5) */
    {"abelian_variety", 1},
    {"grassmannian", 2},
    {"flag_variety", 1},
    {"noetherian_ring", 1},
    {"artinian_ring", 1},
    /* Group VII: Ring Theory (3) */
    {"integral_extension", 2},
    {"noether_normalization", 1},
    {"zariski_main_theorem", 1},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"hartshorne_conjecture", "open_problem", 3, true},
    {"minimal_model_program", "open_problem", 4, true},
    {"resolution_of_singularities", "proven_hard", 3, true},
    {"cohomology_ring_computation", "undecidable", 3, true},
    {"rational_point_existence", "undecidable", 3, true},
    {"hilbert_sixteenth_problem", "open_problem", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h。
 * Test 4 逻辑框架为本文件特有手写体，保留在下方）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "algebraic_geometry",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params_min 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS_MIN,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 38 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（min_deps 形态） */
        .uc_style = AXIOM_TEST_UC_MIN_DEPS,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 6 unconstructible problems",
        .uc_min_deps = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（文件特有手写，见下方 test_logical_framework） */

        /* Test 5: 内容哈希（确定性形态） */
        .hash_style = AXIOM_TEST_HASH_DETERMINISTIC,
        .hash_free = AXIOM_TEST_FREE_LV_FREE,

        /* Test 6: 往返保存/加载（save_load 形态） */
        .rt_style = AXIOM_TEST_RT_SAVE_LOAD,

        /* Test 7: 依赖验证（note 形态） */
        .dep_style = AXIOM_TEST_DEP_V2,
        .dep_extra = NULL,

        /* Test 8: 负向查找 */
        .neg_style = AXIOM_TEST_NEG_XYZ,

        /* Test 9: 外部引用（遍历全部形态） */
        .ext_style = AXIOM_TEST_EXT_ALL,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

/* Test 4：逻辑框架（文件特有：bottom_geometry 用 strstr 检查，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    /* bottom_geometry contains "polynomial_equation" or "affine" */
    TEST_ASSERT(
        strstr(pkg->bottom_geometry, "polynomial_equation") != NULL || strstr(pkg->bottom_geometry, "affine") != NULL,
        "bottom_geometry should contain algebraic geometry concepts");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_key_templates(void) {
    printf("Test 10: Key algebraic geometry templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Affine geometry core */
    const char *affine_core[] = {"affine_space", "algebraic_set", "coordinate_ring"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, affine_core[i]);
        TEST_ASSERT(tmpl != NULL, "affine geometry template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Projective geometry core */
    const char *proj_core[] = {"projective_space", "projective_variety", "homogeneous_coordinates"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, proj_core[i]);
        TEST_ASSERT(tmpl != NULL, "projective geometry template should exist");
    }

    /* Scheme theory core */
    ConstraintTemplate *as = axiom_package_get_template(pkg, "affine_scheme");
    TEST_ASSERT(as != NULL, "affine scheme should exist");
    ConstraintTemplate *lrs = axiom_package_get_template(pkg, "locally_ringed_space");
    TEST_ASSERT(lrs != NULL, "locally ringed space should exist");

    /* Dimension theory */
    ConstraintTemplate *dim = axiom_package_get_template(pkg, "krull_dimension");
    TEST_ASSERT(dim != NULL, "Krull dimension should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Algebraic Geometry")
    LV_REGISTER_AXIOM_CASES("AlgebraicGeometry", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_key_templates);
TEST_MAIN_END()

