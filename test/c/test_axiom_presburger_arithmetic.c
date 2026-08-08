/**
 * @file test_axiom_presburger_arithmetic.c
 * @brief Presburger Arithmetic Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the presburger_arithmetic.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Mathematical Theory: Presburger Arithmetic
 * Axiomatization: First-order theory of (N, 0, 1, +) with induction schema
 *
 * Key Properties:
 *   - Consistent, complete, and decidable (Presburger 1929)
 *   - No multiplication between variables
 *   - Godel's incompleteness theorems do NOT apply
 *   - Satisfiability is 2-EXPTIME complete
 *
 * References:
 *   - Presburger (1929), Cooper (1972), Ferrante & Rackoff (1979)
 *   - Wikipedia: Presburger arithmetic
 *   - nLab: Presburger arithmetic
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/presburger_arithmetic.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/presburger_arithmetic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 74
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 10

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group 1: Core Axioms (5) */
    {"zero_not_successor", 1},
    {"successor_injective", 2},
    {"additive_identity", 1},
    {"addition_recursion", 2},
    {"induction_schema", 1},
    /* Group 2: Elementary Consequences (12) */
    {"addition_commutative", 2},
    {"addition_associative", 3},
    {"left_cancellation", 3},
    {"right_cancellation", 3},
    {"identity_uniqueness", 1},
    {"zero_left_identity", 1},
    {"successor_definition", 1},
    {"every_number_zero_or_successor", 1},
    {"no_self_successor", 1},
    {"addition_increasing", 2},
    {"zero_minimum", 1},
    {"no_maximum", 1},
    /* Group 3: Order Relation (6) */
    {"order_definition", 2},
    {"order_transitive", 3},
    {"order_irreflexive", 1},
    {"order_trichotomy", 2},
    {"order_asymmetric", 2},
    {"successor_immediate", 2},
    /* Group 4: Multiplication by Constants (6) */
    {"multiply_by_2", 1},
    {"multiply_by_3", 1},
    {"multiply_by_constant", 2},
    {"constant_left_distributive", 3},
    {"constant_right_distributive", 3},
    {"constant_multiply_associative", 3},
    /* Group 5: Parity and Divisibility (8) */
    {"even_or_odd", 1},
    {"even_definition", 1},
    {"odd_definition", 1},
    {"even_plus_even", 2},
    {"even_plus_odd", 2},
    {"divisibility_by_constant", 2},
    {"residue_class_exhaustion", 2},
    {"chinese_remainder_constants", 4},
    /* Group 6: Linear Diophantine (6) */
    {"linear_equation_solvability", 3},
    {"frobenius_coin_problem", 2},
    {"linear_inequality_system", 1},
    {"truncated_subtraction", 2},
    {"absolute_difference", 2},
    {"min_max_definable", 2},
    /* Group 7: Quantifier Elimination (4) */
    {"cooper_existential_elimination", 1},
    {"universal_elimination", 1},
    {"quantifier_free_normal_form", 1},
    {"congruence_relation", 2},
    /* Group 8: Automata-Theoretic (4) */
    {"buchi_automaton_construction", 1},
    {"semilinear_set_representation", 1},
    {"ultimately_periodic_sets", 1},
    {"cobham_semenov_theorem", 1},
    /* Group 9: Model Theory (5) */
    {"standard_model", 0},
    {"nonstandard_models", 0},
    {"elementary_equivalence", 0},
    {"model_completeness", 0},
    {"vaught_test", 0},
    /* Group 10: Core Constructors (6) */
    {"construct_successor", 1},
    {"construct_sum", 2},
    {"construct_constant_multiple", 2},
    {"construct_truncated_difference", 2},
    {"construct_minimum", 2},
    {"construct_maximum", 2},
    /* Group 11: Derived Constructors (8) */
    {"construct_residue", 2},
    {"construct_quotient", 2},
    {"construct_parity", 1},
    {"construct_linear_combination", 4},
    {"construct_semilinear_set", 2},
    {"construct_periodic_set", 2},
    {"construct_less_than", 2},
    {"construct_congruence_class", 3},
    /* Group 12: Applications (4) */
    {"array_bounds_check", 2},
    {"loop_invariant_check", 1},
    {"ilp_feasibility", 1},
    {"smt_integration", 1},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"primality", "inexpressible", 2, true},
    {"general_multiplication", "inexpressible", 2, true},
    {"general_divisibility", "inexpressible", 2, true},
    {"exponentiation", "inexpressible", 2, true},
    {"goldbach_conjecture", "inexpressible", 2, true},
    {"fermat_last_theorem", "inexpressible", 2, true},
    {"goodstein_theorem", "inexpressible", 2, true},
    {"godel_sentence", "inexpressible", 2, true},
    {"satisfiability_2exptime", "2_exptime_complete", 3, true},
    {"bit_vector_arithmetic", "undecidable_extension", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h。
 * Test 9 外部引用为本文件特有手写体，保留在下方）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "presburger_arithmetic",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 74 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（A 形态） */
        .uc_style = AXIOM_TEST_UC_A,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 10 unconstructible problems",
        .uc_expectations = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（S 形态） */
        .lf_style = AXIOM_TEST_LF_S,
        .lf_bottom_geometry = "presburger_natural_numbers",
        .lf_negation_encoding = "classical_first_order",
        .lf_contradiction_behavior = PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
        .lf_contradiction_name = "PROPOSITION_KIND_EXPLOSION_PRINCIPLE",

        /* Test 5: 内容哈希（单次形态） */
        .hash_style = AXIOM_TEST_HASH_SINGLE,
        .hash_free = AXIOM_TEST_FREE_LV_FREE,

        /* Test 6: 往返保存/加载（basic 形态） */
        .rt_style = AXIOM_TEST_RT_BASIC,

        /* Test 7: 依赖验证（V1 形态） */
        .dep_style = AXIOM_TEST_DEP_V1,
        .dep_fail_msg = "FAIL (acceptable for cross-references)",
        .dep_suffix = "",

        /* Test 8: 负向查找 */
        .neg_style = AXIOM_TEST_NEG_BASIC,

        /* Test 9: 外部引用（文件特有手写，见下方 test_external_refs） */
        .ext_style = AXIOM_TEST_EXT_NONE,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

/* Test 9：外部引用（文件特有：域名 strstr 匹配，保留原体） */
static void test_external_refs(void) {
    printf("Test 9: Verify external reference URLs...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    struct {
        const char *name;
        const char *expected_domain;
    } ref_checks[] = {
        {"primality", "wikipedia.org"},
        {"general_multiplication", "wikipedia.org"},
        {"general_divisibility", "wikipedia.org"},
        {"exponentiation", "wikipedia.org"},
        {"goldbach_conjecture", "wikipedia.org"},
        {"fermat_last_theorem", "wikipedia.org"},
        {"goodstein_theorem", "stanford.edu"},
        {"godel_sentence", "stanford.edu"},
        {"satisfiability_2exptime", "wikipedia.org"},
        {"bit_vector_arithmetic", "ncatlab.org"},
    };

    for (int i = 0; i < (int) (sizeof(ref_checks) / sizeof(ref_checks[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, ref_checks[i].name);
        TEST_ASSERT(uc != NULL, ref_checks[i].name);
        if (uc) {
            TEST_ASSERT(uc->external_ref != NULL && strstr(uc->external_ref, ref_checks[i].expected_domain) != NULL,
                        ref_checks[i].name);
            printf("  [%d] %s -> %s\n", i, uc->name, uc->external_ref);
        }
    }

    axiom_package_destroy(pkg);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_no_multiplication(void) {
    printf("Test 10: Verify no general multiplication templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify that general multiplication is NOT a template */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "multiplication");
    TEST_ASSERT(tmpl == NULL, "general 'multiplication' template should NOT exist in Presburger arithmetic");

    /* But multiplication by constants SHOULD exist */
    tmpl = axiom_package_get_template(pkg, "multiply_by_constant");
    TEST_ASSERT(tmpl != NULL, "'multiply_by_constant' template SHOULD exist");

    tmpl = axiom_package_get_template(pkg, "multiply_by_2");
    TEST_ASSERT(tmpl != NULL, "'multiply_by_2' template SHOULD exist");

    printf("  No general multiplication: correct\n");
    printf("  Constant multiplication available: correct\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Presburger Arithmetic")
    LV_REGISTER_AXIOM_CASES("PresburgerArithmetic", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_external_refs);
    TEST_MAIN_RUN(test_no_multiplication);
TEST_MAIN_END()

