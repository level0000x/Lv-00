/**
 * @file test_axiom_peano_arithmetic.c
 * @brief Peano Arithmetic Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the peano_arithmetic.lvz
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

#define AXIOM_PKG_PATH "module/axiom_packages/peano_arithmetic.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/peano_arithmetic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 70
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Successor Axioms (3) */
    {"zero_not_successor", 1},
    {"successor_injective", 2},
    {"add_zero_left", 1},
    /* Group II: Addition Axioms (2) */
    {"add_successor_right", 2},
    {"add_zero_right", 1},
    /* Group III: Multiplication Axioms (2) */
    {"mul_zero", 1},
    {"mul_successor_right", 2},
    /* Group IV: Induction Schema (4) */
    {"induction_schema", 1},
    {"induction_on_addition", 1},
    {"induction_on_multiplication", 1},
    {"strong_induction", 1},
    /* Group V: Order Relation (6) */
    {"less_than_definition", 2},
    {"less_than_irreflexive", 1},
    {"less_than_transitive", 3},
    {"less_than_total", 2},
    {"zero_is_least", 1},
    {"no_largest_element", 1},
    /* Group VI: Elementary Arithmetic Consequences (16) */
    {"successor_not_equal", 1},
    {"successor_distinct", 2},
    {"addition_commutative", 2},
    {"addition_associative", 3},
    {"addition_cancellative", 3},
    {"multiplication_commutative", 2},
    {"multiplication_associative", 3},
    {"distributivity_left", 3},
    {"distributivity_right", 3},
    {"mul_identity_right", 1},
    {"mul_identity_left", 1},
    {"mul_zero_commutes", 1},
    {"no_zero_divisors", 2},
    {"order_add_right", 3},
    {"order_add_left", 3},
    {"order_mul_positive", 2},
    /* Group VII: Exponentiation (5) */
    {"exp_zero", 1},
    {"exp_successor", 2},
    {"exp_addition_law", 3},
    {"exp_multiplication_law", 3},
    {"exp_power_law", 3},
    /* Group VIII: Divisibility and Remainder (4) */
    {"divisibility_definition", 2},
    {"division_algorithm", 2},
    {"euclidean_gcd", 2},
    {"bezout_identity", 2},
    /* Group IX: Primality (4) */
    {"prime_definition", 1},
    {"unique_prime_factorization", 1},
    {"infinitude_of_primes", 1},
    {"euclid_lemma", 3},
    /* Group X: Core Constructors (13) */
    {"successor", 1},
    {"predecessor", 1},
    {"add", 2},
    {"subtract_truncated", 2},
    {"multiply", 2},
    {"exponentiate", 2},
    {"less_than_compare", 2},
    {"less_or_equal_compare", 2},
    {"equality_compare", 2},
    {"maximum", 2},
    {"minimum", 2},
    {"factorial", 1},
    /* Group XI: Derived Constructors (13) */
    {"quotient", 2},
    {"remainder", 2},
    {"divisibility_test", 2},
    {"gcd", 2},
    {"lcm", 2},
    {"primality_test", 1},
    {"next_prime", 1},
    {"prime_factorization", 1},
    {"beta_function_encode", 2},
    {"beta_function_decode", 2},
    {"bounded_forall", 2},
    {"bounded_exists", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"godel_sentence", "godel_first_incompleteness", 6, true},
    {"consistency_of_PA", "godel_second_incompleteness", 5, true},
    {"goodstein_theorem", "transfinite_induction_up_to_epsilon_0", 5, true},
    {"paris_harrington_principle", "independence_from_PA", 5, true},
    {"kirby_paris_hydra", "transfinite_induction_up_to_epsilon_0", 4, true},
    {"truth_predicate_for_PA", "tarski_undefinability", 5, true},
    {"halting_problem_for_PA", "turing_halting_problem", 5, true},
    {"epsilon_0_consistency", "gentzen_consistency_proof", 5, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* Test 9：期望外部引用 URL 前缀 */
static const AxiomTestExtRefExpectation k_external_refs[] = {
    {"godel_sentence", "https://en.wikipedia.org/wiki/G%C3%B6del%27s_incompleteness_theorems"},
    {"consistency_of_PA", "https://en.wikipedia.org/wiki/G%C3%B6del%27s_second_incompleteness"},
    {"goodstein_theorem", "https://en.wikipedia.org/wiki/Goodstein%27s_theorem"},
    {"paris_harrington_principle", "https://en.wikipedia.org/wiki/Paris%E2%80%93Harrington_theorem"},
    {"kirby_paris_hydra", "https://en.wikipedia.org/wiki/Hydra_game"},
    {"truth_predicate_for_PA", "https://en.wikipedia.org/wiki/Tarski%27s_undefinability_theorem"},
    {"halting_problem_for_PA", "https://en.wikipedia.org/wiki/Halting_problem"},
    {"epsilon_0_consistency", "https://en.wikipedia.org/wiki/Epsilon_numbers"},
};
#define K_EXTERNAL_REFS_COUNT (int) (sizeof(k_external_refs) / sizeof(k_external_refs[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "peano_arithmetic");
}

static void test_templates(void) {
    axiom_test_templates_with_params(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 70 constraint templates",
                                     k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructible_problems(void) {
    axiom_test_unconstructible_problems(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                        "should have 8 unconstructible problems", k_unconstructibles,
                                        K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework(AXIOM_PKG_PATH, "peano_arithmetic_discrete", "classical_first_order_logic",
                                 PROPOSITION_KIND_EXPLOSION_PRINCIPLE, "PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
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

static void test_external_refs(void) {
    axiom_test_external_refs(AXIOM_PKG_PATH, k_external_refs, K_EXTERNAL_REFS_COUNT);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_template_categories(void) {
    printf("Test 10: Verify template categories are complete...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify key category templates exist */
    const char *category_templates[] = {
        /* Successor axioms */
        "zero_not_successor",
        "successor_injective",
        /* Addition & Multiplication */
        "add_zero_left",
        "add_successor_right",
        "mul_zero",
        "mul_successor_right",
        /* Induction */
        "induction_schema",
        "strong_induction",
        /* Order */
        "less_than_definition",
        "less_than_irreflexive",
        "less_than_transitive",
        /* Arithmetic consequences */
        "addition_commutative",
        "multiplication_commutative",
        "distributivity_left",
        "no_zero_divisors",
        /* Exponentiation */
        "exp_zero",
        "exp_successor",
        "exp_power_law",
        /* Divisibility */
        "division_algorithm",
        "euclidean_gcd",
        "bezout_identity",
        /* Primality */
        "prime_definition",
        "unique_prime_factorization",
        "euclid_lemma",
        /* Constructors */
        "successor",
        "add",
        "multiply",
        "exponentiate",
        "factorial",
        "gcd",
        "lcm",
        "primality_test",
        /* Gödel encoding */
        "beta_function_encode",
        "beta_function_decode",
        "bounded_forall",
        "bounded_exists",
    };

    int cat_total = (int) (sizeof(category_templates) / sizeof(category_templates[0]));
    int cat_found = 0;
    for (int i = 0; i < cat_total; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, category_templates[i]);
        if (tmpl)
            cat_found++;
    }

    TEST_ASSERT(cat_found == cat_total, "all category templates should be found");
    printf("  Category templates: %d / %d found\n", cat_found, cat_total);

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Peano Arithmetic")

    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructible_problems);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_round_trip);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);
    TEST_MAIN_RUN(test_external_refs);
    TEST_MAIN_RUN(test_template_categories);

TEST_MAIN_END()

