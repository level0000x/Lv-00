/**
 * @file test_axiom_point_set_topology.c
 * @brief Point-Set Topology Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the point_set_topology.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, external references, and key
 * topology template checks.
 *
 * Point-set topology is the study of topological spaces through their open
 * sets, providing the foundational language for analysis, geometry, and
 * algebraic topology. The 43 templates cover open/closed sets, continuity,
 * connectedness, compactness, separation axioms, and metric spaces.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/point_set_topology.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/point_set_topology_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 43
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Open Set Axioms (4) */
    {"open_set_empty", 1},
    {"open_set_full", 1},
    {"open_set_arbitrary_union", 2},
    {"open_set_finite_intersection", 2},
    /* Group II: Derived Set Operations (8) */
    {"closed_set", 2},
    {"closure", 2},
    {"interior", 2},
    {"boundary", 2},
    {"exterior", 2},
    {"neighborhood", 3},
    {"open_neighborhood", 3},
    {"limit_point", 3},
    /* Group III: Continuous Maps (6) */
    {"continuous_map", 3},
    {"homeomorphism", 3},
    {"continuous_composition", 5},
    {"continuous_identity", 1},
    {"embedding", 3},
    {"quotient_map", 3},
    /* Group IV: Connectedness (6) */
    {"connected_space", 1},
    {"disconnected_space", 1},
    {"path_connected", 3},
    {"connected_component", 2},
    {"locally_connected", 1},
    {"separation_by_open_sets", 3},
    /* Group V: Compactness (6) */
    {"compact_space", 1},
    {"sequentially_compact", 1},
    {"locally_compact", 1},
    {"compact_subset_closed", 2},
    {"heine_borel", 1},
    {"tychonoff_product", 2},
    /* Group VI: Separation Axioms (8) */
    {"T0_kolmogorov", 1},
    {"T1_fréchet", 1},
    {"T2_hausdorff", 1},
    {"T3_regular", 1},
    {"T3half_tychonoff", 1},
    {"T4_normal", 1},
    {"T5_completely_normal", 1},
    {"T6_perfectly_normal", 1},
    /* Group VII: Metric Spaces (5) */
    {"convergent_sequence", 3},
    {"cauchy_sequence", 2},
    {"metric_space", 2},
    {"metric_topology", 1},
    {"complete_metric_space", 1},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"homeomorphism_problem", "undecidable", 4, true},
    {"homotopy_equivalence_problem", "undecidable", 3, true},
    {"topological_isomorphism_problem", "undecidable", 4, true},
    {"compactness_recognition", "undecidable", 3, true},
    {"metrizability_problem", "undecidable", 3, true},
    {"covering_space_classification", "undecidable", 3, true},
    {"fundamental_group_computation", "undecidable", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h。
 * Test 4 逻辑框架与 Test 9 外部引用为文件特有手写体，保留在下方）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "point_set_topology",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params_min 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS_MIN,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 43 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（min_deps 形态） */
        .uc_style = AXIOM_TEST_UC_MIN_DEPS,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 7 unconstructible problems",
        .uc_min_deps = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（文件特有手写，见下方 test_logical_framework） */

        /* Test 5: 内容哈希（确定性形态） */
        .hash_style = AXIOM_TEST_HASH_DETERMINISTIC,
        .hash_free = AXIOM_TEST_FREE_LV_FREE,

        /* Test 6: 往返保存/加载（save_load 形态） */
        .rt_style = AXIOM_TEST_RT_SAVE_LOAD,

        /* Test 7: 依赖验证（note 形态） */
        .dep_style = AXIOM_TEST_DEP_V2,
        .dep_extra = "(identifier references to external concepts are expected)",

        /* Test 8: 负向查找 */
        .neg_style = AXIOM_TEST_NEG_XYZ,

        /* Test 9: 外部引用（文件特有手写，见下方 test_external_references） */
        .ext_style = AXIOM_TEST_EXT_NONE,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

/* Test 4：逻辑框架（文件特有：negation_encoding 用 strstr 检查，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    TEST_ASSERT(strcmp(pkg->bottom_geometry, "topological_space_open_sets") == 0,
                "bottom_geometry should be 'topological_space_open_sets'");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    TEST_ASSERT(strstr(pkg->negation_encoding, "complement") != NULL, "negation_encoding should contain 'complement'");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

/* Test 5/6/7/8 已收敛至 kCases 数据驱动用例（见上） */

/* Test 9：外部引用（文件特有：仅 HTTPS 格式检查，保留原体） */
static void test_external_references(void) {
    printf("Test 9: External reference URLs...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    for (int i = 0; i < axiom_package_get_unconstructible_count(pkg); i++) {
        KnownUnconstructible *uc = axiom_package_get_unconstructible(pkg, i);
        TEST_ASSERT(uc->external_ref != NULL, "each unconstructible should have an external_ref");

        /* Verify it's a valid HTTPS URL */
        int is_https = (strncmp(uc->external_ref, "https://", 8) == 0);
        TEST_ASSERT(is_https, "external_ref should be a valid HTTPS URL");

        printf("  '%s' -> %s\n", uc->name, uc->external_ref);
    }

    axiom_package_destroy(pkg);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_key_templates(void) {
    printf("Test 10: Key topology templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core topology templates that define the discipline */
    const char *key_templates[] = {
        "open_set_empty", "closed_set",   "continuous_map",  "homeomorphism",
        "compact_space",  "T2_hausdorff", "connected_space", "metric_space",
    };

    int key_count = sizeof(key_templates) / sizeof(key_templates[0]);
    for (int i = 0; i < key_count; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, key_templates[i]);
        TEST_ASSERT(tmpl != NULL, "key topology template should exist");
        if (tmpl) {
            TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
            printf("  '%s' (params=%d)\n", key_templates[i], tmpl->param_count);
        }
    }

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Point Set Topology")
    LV_REGISTER_AXIOM_CASES("PointSetTopology", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_external_references);
    TEST_MAIN_RUN(test_key_templates);
TEST_MAIN_END()

