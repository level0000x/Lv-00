/**
 * @file test_axiom_combinatorics.c
 * @brief Combinatorics Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the combinatorics.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Combinatorics is formalized through 39 templates covering counting
 * principles, permutations and combinations, graph theory, Ramsey theory,
 * probabilistic method, and design theory.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/combinatorics.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/combinatorics_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 39
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Counting Principles (6) */
    {"pigeonhole_principle", 2},
    {"inclusion_exclusion", 2},
    {"binomial_coefficient", 2},
    {"multinomial_coefficient", 2},
    {"stars_and_bars", 2},
    {"generating_function", 2},
    /* Group II: Permutations & Combinations (5) */
    {"permutation", 2},
    {"combination", 2},
    {"permutation_with_repetition", 2},
    {"derangement", 1},
    {"stirling_number", 2},
    /* Group III: Graph Theory Basics (8) */
    {"graph_vertex", 1},
    {"graph_edge", 2},
    {"graph_path", 2},
    {"graph_cycle", 1},
    {"graph_tree", 1},
    {"graph_connected", 1},
    {"graph_bipartite", 1},
    {"graph_planar", 1},
    /* Group IV: Graph Theory Advanced (6) */
    {"graph_coloring", 2},
    {"graph_matching", 1},
    {"graph_flow", 3},
    {"eulerian_path", 1},
    {"hamiltonian_path", 1},
    {"graph_isomorphism", 2},
    /* Group V: Ramsey Theory (4) */
    {"ramsey_number", 2},
    {"ramsey_theorem", 3},
    {"schur_theorem", 1},
    {"van_der_waerden_theorem", 2},
    /* Group VI: Probabilistic Method (5) */
    {"probabilistic_method", 2},
    {"markov_inequality", 2},
    {"chebyshev_inequality", 3},
    {"chernoff_bound", 3},
    {"lovasz_local_lemma", 3},
    /* Group VII: Design Theory (5) */
    {"latin_square", 1},
    {"design_theory_block", 4},
    {"error_correcting_code", 3},
    {"matroid", 2},
    {"poset_dilworth", 1},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"graph_isomorphism_problem", "quasi_polynomial", 3, true},
    {"graph_coloring_decision", "np_complete", 3, true},
    {"hamiltonian_cycle_decision", "np_complete", 5, true},
    {"subgraph_isomorphism", "np_complete", 3, true},
    {"ramsey_number_exact", "undecidable", 3, true},
    {"permanent_computation", "sharp_p_hard", 2, true},
    {"satisfiability_3sat", "np_complete", 2, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "combinatorics",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params_min 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS_MIN,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 39 constraint templates",
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
    printf("Test 10: Key combinatorics templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core counting principles */
    const char *counting_core[] = {"pigeonhole_principle",    "inclusion_exclusion", "binomial_coefficient",
                                   "multinomial_coefficient", "stars_and_bars",      "generating_function"};

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, counting_core[i]);
        TEST_ASSERT(tmpl != NULL, "core counting principle template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Graph theory basics */
    const char *graph_basics[] = {"graph_vertex", "graph_edge",      "graph_path",      "graph_cycle",
                                  "graph_tree",   "graph_connected", "graph_bipartite", "graph_planar"};

    for (int i = 0; i < 8; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, graph_basics[i]);
        TEST_ASSERT(tmpl != NULL, "graph theory basic template should exist");
    }

    /* Ramsey theory */
    const char *ramsey_core[] = {"ramsey_number", "ramsey_theorem", "schur_theorem", "van_der_waerden_theorem"};

    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, ramsey_core[i]);
        TEST_ASSERT(tmpl != NULL, "Ramsey theory template should exist");
    }

    /* Probabilistic method */
    ConstraintTemplate *prob = axiom_package_get_template(pkg, "probabilistic_method");
    TEST_ASSERT(prob != NULL, "probabilistic_method template should exist");
    ConstraintTemplate *lll = axiom_package_get_template(pkg, "lovasz_local_lemma");
    TEST_ASSERT(lll != NULL, "lovasz_local_lemma template should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Combinatorics")
    LV_REGISTER_AXIOM_CASES("Combinatorics", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_key_templates);
TEST_MAIN_END()

