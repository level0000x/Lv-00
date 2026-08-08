/**
 * @file test_axiom_algebraic_topology.c
 * @brief Algebraic Topology Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the algebraic_topology.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Algebraic topology is formalized through 38 templates covering fundamental
 * groups, homology, cohomology, homotopy theory, fixed point theorems,
 * and K-theory.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/algebraic_topology.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/algebraic_topology_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 38
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Fundamental Group (6) */
    {"fundamental_group", 2},
    {"fundamental_groupoid", 1},
    {"covering_space", 3},
    {"universal_cover", 3},
    {"monodromy", 3},
    {"van_kampen_theorem", 4},
    /* Group II: Simplicial Homology (7) */
    {"simplicial_complex", 1},
    {"simplicial_homology", 2},
    {"chain_complex", 2},
    {"boundary_operator", 3},
    {"homology_group", 2},
    {"euler_characteristic", 1},
    {"betti_number", 2},
    /* Group III: Singular Homology & Cohomology (6) */
    {"singular_homology", 3},
    {"singular_cohomology", 3},
    {"mayer_vietoris_sequence", 3},
    {"excision_theorem", 3},
    {"universal_coefficient_theorem", 3},
    {"kuenneth_formula", 3},
    /* Group IV: Cohomology Operations (5) */
    {"cup_product", 4},
    {"cohomology_ring", 2},
    {"poincare_duality", 4},
    {"de_rham_cohomology", 2},
    {"sheaf_cohomology", 3},
    /* Group V: Homotopy Theory (6) */
    {"homotopy_group", 3},
    {"hurewicz_theorem", 2},
    {"whitehead_theorem", 3},
    {"fibration", 3},
    {"cofibration", 3},
    {"long_exact_sequence_fibration", 3},
    /* Group VI: Fixed Point & Intersection (4) */
    {"lefschetz_fixed_point", 2},
    {"lefschetz_number", 2},
    {"intersection_theory", 3},
    {"poincare_lemma", 2},
    /* Group VII: K-Theory (4) */
    {"vector_bundle", 3},
    {"k_group", 1},
    {"bott_periodicity", 1},
    {"atiyah_singer_index", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"homotopy_group_computation", "undecidable", 3, true},
    {"homology_isomorphism_problem", "undecidable", 3, true},
    {"knot_classification", "undecidable", 3, true},
    {"homeomorphism_problem_manifolds", "undecidable", 3, true},
    {"simple_homotopy_equivalence", "undecidable", 3, true},
    {"group_presentation_triviality", "undecidable", 3, true},
    {"manifold_triangulation", "undecidable", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "algebraic_topology",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params_min 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS_MIN,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 38 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（min_deps 形态） */
        .uc_style = AXIOM_TEST_UC_MIN_DEPS,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 7 unconstructible problems",
        .uc_min_deps = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（presence 形态） */
        .lf_style = AXIOM_TEST_LF_P,
        .lf_contradiction_behavior = PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
        .lf_contradiction_name = "PROPOSITION_KIND_EXPLOSION_PRINCIPLE",

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

        /* Test 9: 外部引用（遍历全部形态） */
        .ext_style = AXIOM_TEST_EXT_ALL,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_key_templates(void) {
    printf("Test 10: Key algebraic topology templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Fundamental group core */
    const char *fundamental_group_core[] = {"fundamental_group", "fundamental_groupoid",
                                            "covering_space",    "universal_cover",
                                            "monodromy",         "van_kampen_theorem"};

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, fundamental_group_core[i]);
        TEST_ASSERT(tmpl != NULL, "fundamental group core template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Homology core */
    const char *homology_core[] = {"simplicial_complex", "simplicial_homology", "chain_complex",
                                   "boundary_operator",  "homology_group",      "singular_homology"};

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, homology_core[i]);
        TEST_ASSERT(tmpl != NULL, "homology core template should exist");
    }

    /* Cohomology operations */
    const char *cohomology_ops[] = {"cup_product", "cohomology_ring", "poincare_duality", "de_rham_cohomology"};

    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, cohomology_ops[i]);
        TEST_ASSERT(tmpl != NULL, "cohomology operation template should exist");
    }

    /* K-theory */
    ConstraintTemplate *vb = axiom_package_get_template(pkg, "vector_bundle");
    TEST_ASSERT(vb != NULL, "vector_bundle template should exist");
    ConstraintTemplate *kg = axiom_package_get_template(pkg, "k_group");
    TEST_ASSERT(kg != NULL, "k_group template should exist");
    ConstraintTemplate *bp = axiom_package_get_template(pkg, "bott_periodicity");
    TEST_ASSERT(bp != NULL, "bott_periodicity template should exist");
    ConstraintTemplate *asi = axiom_package_get_template(pkg, "atiyah_singer_index");
    TEST_ASSERT(asi != NULL, "atiyah_singer_index template should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Algebraic Topology")
    LV_REGISTER_AXIOM_CASES("AlgebraicTopology", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_key_templates);
TEST_MAIN_END()

