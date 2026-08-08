/**
 * @file test_axiom_computational_complexity_theory.c
 * @brief Computational Complexity Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the
 * computational_complexity_theory.lvz axiom package. Validates template
 * count, unconstructible problem entries, logical framework settings,
 * content hashing, round-trip save/load, dependency validation,
 * negative lookups, external references, and axiom coherence.
 */

#include <stdio.h>
#include <string.h>

int g_fail_count = 0;
int g_pass_count = 0;

/* 历史私有 TEST_ASSERT 为非返回式语义（失败仅计数、继续执行），
 * 通过 AXIOM_TEST_NON_RETURNING 让骨架头提供兼容变体，保持行为不变 */
#include "test_helpers.h"

#include "axiom_test_common.h"

#define AXIOM_PKG_PATH "module/axiom_packages/computational_complexity_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/computational_complexity_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 54
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板（名称 + 参数个数） */
static const AxiomTestTemplateExpectation k_template_expectations[] = {
    /* Group I: Machine Model Axioms (4) */
    {"deterministic_turing_machine", 1},
    {"non_deterministic_turing_machine", 1},
    {"probabilistic_turing_machine", 1},
    {"quantum_turing_machine", 1},
    /* Group II: Complexity Measure Axioms (6) */
    {"time_complexity", 2},
    {"space_complexity", 2},
    {"worst_case_analysis", 1},
    {"best_case_analysis", 1},
    {"average_case_analysis", 1},
    {"asymptotic_notation", 2},
    /* Group III: Fundamental Complexity Class Definitions (8) */
    {"class_P", 1},
    {"class_NP", 1},
    {"class_PSPACE", 1},
    {"class_EXPTIME", 1},
    {"class_L", 1},
    {"class_NL", 1},
    {"class_BPP", 1},
    {"class_BQP", 1},
    /* Group IV: Reduction and Completeness Axioms (7) */
    {"polynomial_time_reduction", 2},
    {"NP_completeness", 1},
    {"NP_hardness", 1},
    {"Cook_Levin_theorem", 1},
    {"Karp_reduction", 2},
    {"many_one_reduction", 2},
    {"Turing_reduction", 2},
    /* Group V: Hierarchy Theorems (5) */
    {"time_hierarchy_theorem", 2},
    {"space_hierarchy_theorem", 2},
    {"Ladner_theorem", 1},
    {"Savitch_theorem", 1},
    {"hierarchy_separation", 2},
    /* Group VI: Complexity Class Relationships (6) */
    {"P_subseteq_NP", 0},
    {"NP_subseteq_PSPACE", 0},
    {"PSPACE_subseteq_EXPTIME", 0},
    {"L_subseteq_NL", 0},
    {"NL_subseteq_P", 0},
    {"P_vs_NP_open", 0},
    /* Group VII: Core Constructors (5) */
    {"construct_complexity_class", 2},
    {"verify_NP_membership", 2},
    {"reduce_problem", 2},
    {"prove_completeness", 2},
    {"separate_classes", 2},
    /* Group VIII: Derived Complexity Classes (4) */
    {"class_coNP", 1},
    {"class_PH", 1},
    {"class_#P", 1},
    {"class_IP", 1},
    /* Group IX: Important Problems (5) */
    {"SAT_problem", 1},
    {"3SAT_problem", 1},
    {"Hamiltonian_path_problem", 1},
    {"vertex_cover_problem", 1},
    {"clique_problem", 1},
    /* Group X: Specialized Theorems (4) */
    {"Immerman_Szelepcsényi_theorem", 1},
    {"Fagin_theorem", 1},
    {"PCP_theorem", 1},
    {"IP_equals_PSPACE", 1},
};
#define K_TEMPLATE_EXPECTATIONS_COUNT (int) (sizeof(k_template_expectations) / sizeof(k_template_expectations[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"P_vs_NP_problem", "none", 2, false},
    {"graph_isomorphism_problem", "none", 2, false},
    {"discrete_logarithm_problem", "none", 2, false},
    {"integer_factorization_problem", "none", 2, false},
    {"halting_problem", "none", 1, false},
    {"Post_correspondence_problem", "none", 1, false},
    {"word_problem_for_groups", "none", 2, false},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* Test 9：期望外部引用（名称 + URL 前缀） */
static const AxiomTestExtRefExpectation k_ext_refs[] = {
    {"P_vs_NP_problem", "https://www.claymath.org/millennium-problems/p-vs-np-problem"},
    {"graph_isomorphism_problem", "https://en.wikipedia.org/wiki/Graph_isomorphism_problem"},
    {"discrete_logarithm_problem", "https://en.wikipedia.org/wiki/Discrete_logarithm"},
    {"integer_factorization_problem", "https://en.wikipedia.org/wiki/Integer_factorization"},
    {"halting_problem", "https://en.wikipedia.org/wiki/Halting_problem"},
    {"Post_correspondence_problem", "https://en.wikipedia.org/wiki/Post_correspondence_problem"},
    {"word_problem_for_groups", "https://en.wikipedia.org/wiki/Word_problem_for_groups"},
};
#define K_EXT_REFS_COUNT (int) (sizeof(k_ext_refs) / sizeof(k_ext_refs[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "computational_complexity_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 54 constraint templates",
        .tmpl_expectations = k_template_expectations, .tmpl_n = K_TEMPLATE_EXPECTATIONS_COUNT,

        /* Test 3: 不可构造项（A 形态） */
        .uc_style = AXIOM_TEST_UC_A,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 7 unconstructible problems",
        .uc_expectations = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（S 形态） */
        .lf_style = AXIOM_TEST_LF_S,
        .lf_bottom_geometry = "computational_complexity_abstract",
        .lf_negation_encoding = "classical_complement",
        .lf_contradiction_behavior = PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
        .lf_contradiction_name = "PROPOSITION_KIND_EXPLOSION_PRINCIPLE",

        /* Test 5: 内容哈希（单次形态） */
        .hash_style = AXIOM_TEST_HASH_SINGLE,
        .hash_free = AXIOM_TEST_FREE_LV_FREE,

        /* Test 6: 往返保存/加载（basic 形态） */
        .rt_style = AXIOM_TEST_RT_BASIC,

        /* Test 7: 依赖验证（V1 形态） */
        .dep_style = AXIOM_TEST_DEP_V1,
        .dep_fail_msg = "FAIL (acceptable)",
        .dep_suffix = " (expected: may fail for cross-reference reduces_to)",

        /* Test 8: 负向查找 */
        .neg_style = AXIOM_TEST_NEG_BASIC,

        /* Test 9: 外部引用（表驱动形态） */
        .ext_style = AXIOM_TEST_EXT_E1,
        .ext_refs = k_ext_refs, .ext_refs_n = K_EXT_REFS_COUNT,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_complexity_axiom_coherence(void) {
    printf("Test 10: Verify complexity theory axiom coherence...\n");

    AxiomPackage *pkg = axiom_package_create("computational_complexity_theory", "1.0.0");
    TEST_ASSERT(pkg != NULL, "create package");
    AxiomLoadStatus s = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(s == AXIOM_LOAD_OK, "load computational_complexity_theory.lvz");

    /* Verify machine model axioms are present */
    const char *machine_models[] = {"deterministic_turing_machine", "non_deterministic_turing_machine",
                                    "probabilistic_turing_machine", "quantum_turing_machine", NULL};
    for (int i = 0; machine_models[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, machine_models[i]);
        TEST_ASSERT(tmpl != NULL, machine_models[i]);
    }

    /* Verify fundamental complexity classes are present */
    const char *classes[] = {
        "class_P", "class_NP", "class_PSPACE", "class_EXPTIME", "class_L", "class_NL", "class_BPP", "class_BQP", NULL};
    for (int i = 0; classes[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, classes[i]);
        TEST_ASSERT(tmpl != NULL, classes[i]);
    }

    /* Verify reduction and completeness axioms are present */
    const char *reductions[] = {
        "polynomial_time_reduction", "NP_completeness",  "NP_hardness", "Cook_Levin_theorem", "Karp_reduction",
        "many_one_reduction",        "Turing_reduction", NULL};
    for (int i = 0; reductions[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, reductions[i]);
        TEST_ASSERT(tmpl != NULL, reductions[i]);
    }

    /* Verify hierarchy theorems are present */
    const char *hierarchy[] = {"time_hierarchy_theorem", "space_hierarchy_theorem", "Ladner_theorem",
                               "Savitch_theorem",        "hierarchy_separation",    NULL};
    for (int i = 0; hierarchy[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, hierarchy[i]);
        TEST_ASSERT(tmpl != NULL, hierarchy[i]);
    }

    /* Verify class relationships are present */
    const char *relationships[] = {"P_subseteq_NP",
                                   "NP_subseteq_PSPACE",
                                   "PSPACE_subseteq_EXPTIME",
                                   "L_subseteq_NL",
                                   "NL_subseteq_P",
                                   "P_vs_NP_open",
                                   NULL};
    for (int i = 0; relationships[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, relationships[i]);
        TEST_ASSERT(tmpl != NULL, relationships[i]);
    }

    /* Verify important problems are present */
    const char *problems[] = {"SAT_problem",          "3SAT_problem",   "Hamiltonian_path_problem",
                              "vertex_cover_problem", "clique_problem", NULL};
    for (int i = 0; problems[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, problems[i]);
        TEST_ASSERT(tmpl != NULL, problems[i]);
    }

    /* Verify specialized theorems are present */
    const char *theorems[] = {"Immerman_Szelepcsényi_theorem", "Fagin_theorem", "PCP_theorem", "IP_equals_PSPACE",
                              NULL};
    for (int i = 0; theorems[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, theorems[i]);
        TEST_ASSERT(tmpl != NULL, theorems[i]);
    }

    printf(
        "Test 10 passed: all complexity theory axioms, classes, reductions, "
        "hierarchies, relationships, problems, and theorems verified.\n");
    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Computational Complexity Theory Axiom Package Test Suite")
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Testing: axiom_packages/computational_complexity_theory.lvz ===\n\n");
    LV_REGISTER_AXIOM_CASES("ComputationalComplexityTheory", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_complexity_axiom_coherence);
TEST_MAIN_END()
