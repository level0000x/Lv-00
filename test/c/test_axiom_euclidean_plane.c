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
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h。
 * Test 2 模板校验含文件特有参数校验，保留在下方）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "euclidean_plane",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（test_templates 为混合 wrapper，下方保留） */
        .tmpl_style = AXIOM_TEST_TMPL_NONE,

        /* Test 3: 不可构造项（A 形态） */
        .uc_style = AXIOM_TEST_UC_A,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 6 unconstructible problems",
        .uc_expectations = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（S 形态） */
        .lf_style = AXIOM_TEST_LF_S,
        .lf_bottom_geometry = "euclidean_plane",
        .lf_negation_encoding = "classical_material_implication",
        .lf_contradiction_behavior = PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
        .lf_contradiction_name = "PROPOSITION_KIND_EXPLOSION_PRINCIPLE",

        /* Test 5: 内容哈希（单次形态） */
        .hash_style = AXIOM_TEST_HASH_SINGLE,
        .hash_free = AXIOM_TEST_FREE_LV_FREE_PTR,

        /* Test 6: 往返保存/加载（basic 形态） */
        .rt_style = AXIOM_TEST_RT_BASIC,

        /* Test 7: 依赖验证（V1 形态） */
        .dep_style = AXIOM_TEST_DEP_V1,
        .dep_fail_msg = "FAIL (acceptable)",
        .dep_suffix = " (expected: may fail for cross-reference reduces_to)",

        /* Test 8: 负向查找 */
        .neg_style = AXIOM_TEST_NEG_BASIC,

        /* Test 9: 外部引用（无此测试） */
        .ext_style = AXIOM_TEST_EXT_NONE,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

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

/* Test 3/4/5/6/7/8 已收敛至 kCases 数据驱动用例（见上） */

TEST_MAIN_BEGIN("Euclidean Plane")
    LV_REGISTER_AXIOM_CASES("EuclideanPlane", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_templates);
TEST_MAIN_END()

