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
    /* Group III: Curvature (6) */
    {"riemann_curvature_tensor", 2},
    {"ricci_curvature", 2},
    {"scalar_curvature", 2},
    {"sectional_curvature", 3},
    {"gauss_bonnet_theorem", 2},
    {"gauss_curvature", 3},
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
    {"geodesic_completeness_decision", "undecidable", 3, true},
    {"positive_mass_theorem", "open_problem", 3, true},
    {"exotic_sphere_existence", "undecidable", 3, true},
    {"poincare_conjecture_higher", "solved", 3, true},
    {"curvature_bounded_below", "undecidable", 3, true},
    {"symplectic_embedding", "undecidable", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h。
 * Test 4 逻辑框架为本文件特有手写体，保留在下方）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "differential_geometry",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params_min 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS_MIN,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 39 constraint templates",
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

/* Test 5/6/7/8/9 已收敛至 kCases 数据驱动用例（见上） */

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

TEST_MAIN_BEGIN("Differential Geometry")
    LV_REGISTER_AXIOM_CASES("DifferentialGeometry", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_key_templates);
TEST_MAIN_END()

