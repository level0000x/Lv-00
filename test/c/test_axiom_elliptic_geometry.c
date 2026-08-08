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
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h。
 * Test 3/4 为文件特有手写体，保留在下方）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "elliptic_geometry",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params_min 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS_MIN,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 30 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3/4: 文件特有手写，下方保留 */
        .uc_style = AXIOM_TEST_UC_NONE,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .lf_style = AXIOM_TEST_LF_NONE,

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

/* Test 5/6/7/8/9 已收敛至 kCases 数据驱动用例（见上） */

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
    LV_REGISTER_AXIOM_CASES("EllipticGeometry", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_unconstructibles);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_key_templates);
TEST_MAIN_END()
