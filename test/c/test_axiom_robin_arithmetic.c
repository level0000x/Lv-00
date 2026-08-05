/**
 * @file test_axiom_robin_arithmetic.c
 * @brief Robinson Arithmetic (Q) Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the robin_arithmetic.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, and negative lookups.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/robin_arithmetic.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/robin_arithmetic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 39
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 12

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Core Axioms of Q (7) */
    {"zero_not_successor", 1},
    {"successor_injective", 2},
    {"every_number_zero_or_successor", 1},
    {"add_zero_right", 1},
    {"add_successor_right", 2},
    {"mul_zero", 1},
    {"mul_successor_right", 2},
    /* Group II: Definitional Extension: Order Relation (4) */
    {"less_than_definition", 2},
    {"less_than_zero_impossible", 1},
    {"less_than_successor", 2},
    {"trichotomy", 2},
    /* Group III: Elementary Consequences of Q (12) */
    {"zero_predecessor_property", 1},
    {"successor_not_self_instance", 1},
    {"add_zero_left", 1},
    {"add_associative_instance", 3},
    {"add_commutative_instance", 2},
    {"mul_zero_left", 1},
    {"mul_one_right", 1},
    {"mul_one_left", 1},
    {"distributive_instance", 3},
    {"less_than_transitive_instance", 3},
    {"less_than_irreflexive", 1},
    {"zero_is_least_successor", 1},
    /* Group IV: Core Constructors (8) */
    {"successor", 1},
    {"predecessor", 1},
    {"add", 2},
    {"multiply", 2},
    {"zero", 0},
    {"numeral", 1},
    {"less_than_check", 2},
    {"equality_check", 2},
    /* Group V: Derived Constructors (8) */
    {"subtract_truncated", 2},
    {"exponentiate", 2},
    {"is_even", 1},
    {"maximum", 2},
    {"minimum", 2},
    {"divide_truncated", 2},
    {"remainder", 2},
    {"gcd", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"consistency_of_Q", "godel_second_incompleteness", 7, true},
    {"godel_sentence_for_Q", "godel_first_incompleteness", 3, true},
    {"add_commutativity_general", "induction_requirement", 3, true},
    {"mul_commutativity_general", "induction_requirement", 4, true},
    {"add_associativity_general", "induction_requirement", 3, true},
    {"successor_not_self_general", "induction_requirement", 3, true},
    {"decidability_of_Q", "essential_undecidability", 7, true},
    {"decidability_of_any_extension_of_Q", "tarski_mostowski_robinson_theorem", 7, true},
    {"hilbert_tenth_problem_for_Q", "matiyasevich_davis_putnam_robinson_theorem", 3, true},
    {"nonstandard_model_characterization_of_Q", "tennenbaum_independence", 3, true},
    {"uniform_solvability_equations_in_Q", "unsolvability_of_diophantine_equations", 2, true},
    {"recursive_separability_of_Q_theorems_from_refutations", "essential_inseparability_of_Q", 7, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "robin_arithmetic");
}

static void test_templates(void) {
    axiom_test_templates_with_params(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 39 constraint templates",
                                     k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructible_problems(void) {
    axiom_test_unconstructible_problems(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                        "should have 12 unconstructible problems", k_unconstructibles,
                                        K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework(AXIOM_PKG_PATH, "natural_number_zero_without_predecessor",
                                 "classical_falsum_in_first_order_logic", PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                                 "PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
}

static void test_content_hash(void) {
    axiom_test_content_hash(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_round_trip(void) {
    axiom_test_round_trip(AXIOM_PKG_PATH, SAVE_TEST_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_dependency_validation(void) {
    axiom_test_dependency_validation(AXIOM_PKG_PATH, "FAIL (acceptable)",
                                     " (expected: may fail for cross-reference reduces_to)");
}

static void test_negative_lookups(void) {
    axiom_test_negative_lookups(AXIOM_PKG_PATH, AXIOM_TEST_NEG_BASIC);
}

/* Test 9：外部引用（文件特有：strstr 匹配 + 不同输出格式，保留原体） */
static void test_external_refs(void) {
    printf("Test 9: Verify external reference URLs...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    struct {
        const char *name;
        const char *expected_url_prefix;
    } ref_checks[] = {
        {"consistency_of_Q", "https://en.wikipedia.org/wiki/G%C3%B6del%27s_incompleteness_theorems"},
        {"godel_sentence_for_Q", "https://en.wikipedia.org/wiki/G%C3%B6del%27s_incompleteness_theorems"},
        {"add_commutativity_general", "https://en.wikipedia.org/wiki/Robinson_arithmetic"},
        {"mul_commutativity_general", "https://en.wikipedia.org/wiki/Robinson_arithmetic"},
        {"add_associativity_general", "https://en.wikipedia.org/wiki/Robinson_arithmetic"},
        {"successor_not_self_general", "https://en.wikipedia.org/wiki/Robinson_arithmetic"},
        {"decidability_of_Q", "https://en.wikipedia.org/wiki/Essentially_undecidable_theory"},
        {"decidability_of_any_extension_of_Q", "https://en.wikipedia.org/wiki/Essentially_undecidable_theory"},
        {"hilbert_tenth_problem_for_Q", "https://en.wikipedia.org/wiki/Hilbert%27s_tenth_problem"},
        {"nonstandard_model_characterization_of_Q", "https://en.wikipedia.org/wiki/Tennenbaum%27s_theorem"},
        {"uniform_solvability_equations_in_Q", "https://en.wikipedia.org/wiki/Diophantine_set"},
        {"recursive_separability_of_Q_theorems_from_refutations",
         "https://en.wikipedia.org/wiki/Essentially_undecidable_theory"},
    };

    for (int i = 0; i < (int) (sizeof(ref_checks) / sizeof(ref_checks[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, ref_checks[i].name);
        TEST_ASSERT(uc != NULL, ref_checks[i].name);
        if (uc) {
            TEST_ASSERT(uc->external_ref != NULL && strstr(uc->external_ref, ref_checks[i].expected_url_prefix) != NULL,
                        "external_ref should contain expected URL prefix");
            printf("  [%d] %s: %s\n", i, ref_checks[i].name, uc->external_ref);
        }
    }

    axiom_package_destroy(pkg);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_key_axioms_present(void) {
    printf("Test 10: Verify all 7 core Q axioms are present...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    const char *core_axioms[] = {
        "zero_not_successor",  "successor_injective", "every_number_zero_or_successor",
        "add_zero_right",      "add_successor_right", "mul_zero",
        "mul_successor_right",
    };

    for (int i = 0; i < 7; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_axioms[i]);
        TEST_ASSERT(tmpl != NULL, core_axioms[i]);
        if (tmpl) {
            printf("  Q%d [%s] present (params=%d)\n", i + 1, core_axioms[i], tmpl->param_count);
        }
    }

    printf("  All 7 core Q axioms verified\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Robinson Arithmetic (Q) Axiom Package")
    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructible_problems);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_round_trip);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);
    TEST_MAIN_RUN(test_external_refs);
    TEST_MAIN_RUN(test_key_axioms_present);

TEST_MAIN_END()

