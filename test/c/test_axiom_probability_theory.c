/**
 * @file test_axiom_probability_theory.c
 * @brief Probability Theory Axiom Package Test
 *
 * Tests the loading, template verification, unconstructible problem
 * validation, logical framework, round-trip save/load, and dependency
 * checking for the probability_theory axiom package (v1.0.0).
 *
 * Mathematical theory: Probability Theory (Kolmogorov axioms, probability
 * spaces, random variables, expectation, limit theorems, stochastic
 * processes).
 *
 * Key references:
 *   - Kolmogorov, A.N. (1933). "Grundbegriffe der Wahrscheinlichkeitsrechnung."
 *   - Billingsley, P. (1995). "Probability and Measure" (3rd ed.).
 *   - Feller, W. (1968). "An Introduction to Probability Theory."
 *   - Durrett, R. (2019). "Probability: Theory and Examples" (5th ed.).
 *   - Wikipedia: Probability axioms
 *     https://en.wikipedia.org/wiki/Probability_axioms
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"

#define AXIOM_PKG_PATH "module/axiom_packages/probability_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/probability_theory_test_save.lvz"

/* Template count: 10 groups total
 *   Group I:   Kolmogorov Axioms            = 3
 *   Group II:  Basic Probability Properties = 8
 *   Group III: Conditional Probability      = 6
 *   Group IV:  Independence                 = 6
 *   Group V:   Random Variables             = 8
 *   Group VI:  Expectation and Moments      = 8
 *   Group VII: Limit Theorems               = 6
 *   Group VIII: Common Distributions        = 10
 *   Group IX:  Stochastic Processes         = 6
 *   Group X:   Advanced Topics              = 5
 *   Total = 3+8+6+6+8+8+6+10+6+5 = 66
 */
#define EXPECTED_TEMPLATE_COUNT 66
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

static int g_fail_count = 0;
static int g_pass_count = 0;

#define TEST_ASSERT(cond, msg)           \
    do {                                 \
        if (!(cond)) {                   \
            printf("  FAIL: %s\n", msg); \
            g_fail_count++;              \
        } else {                         \
            g_pass_count++;              \
        }                                \
    } while (0)

/* ------------------------------------------------------------------ */
/* Test 1: Load from file                                              */
/* ------------------------------------------------------------------ */
static void test_load_from_file(void) {
    printf("Test 1: Load probability_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "probability_theory") == 0,
                "package name should be 'probability_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 2: Verify constraint templates                                 */
/* ------------------------------------------------------------------ */
static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 66 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    const char *expected_templates[] = {
        /* Group I: Kolmogorov Axioms */
        "probability_non_negative", "probability_normalization", "sigma_additivity",
        /* Group II: Basic Probability Properties */
        "empty_set_probability", "complement_rule", "probability_monotonicity", "probability_bounds",
        "finite_additivity", "inclusion_exclusion_2", "inclusion_exclusion_3", "boole_inequality",
        /* Group III: Conditional Probability */
        "conditional_probability_def", "multiplication_rule", "law_total_probability", "bayes_theorem",
        "chain_rule_conditional", "conditional_probability_bounds",
        /* Group IV: Independence */
        "independence_definition", "independence_complement", "mutual_independence", "pairwise_independence",
        "conditional_independence", "sigma_algebra_independence",
        /* Group V: Random Variables */
        "random_variable_measurable", "distribution_function", "probability_mass_function",
        "probability_density_function", "joint_distribution", "marginal_distribution", "random_variable_transformation",
        "random_variable_independence",
        /* Group VI: Expectation and Moments */
        "expected_value_definition", "expectation_linearity", "expectation_of_function", "variance_definition",
        "variance_formula", "covariance_definition", "correlation_coefficient", "moment_generating_function",
        /* Group VII: Limit Theorems */
        "weak_law_large_numbers", "strong_law_large_numbers", "central_limit_theorem", "borel_cantelli_first",
        "borel_cantelli_second", "kolmogorov_zero_one_law",
        /* Group VIII: Common Distributions */
        "bernoulli_distribution", "binomial_distribution", "poisson_distribution", "geometric_distribution",
        "uniform_distribution_discrete", "uniform_distribution_continuous", "exponential_distribution",
        "normal_distribution", "chi_squared_distribution", "student_t_distribution",
        /* Group IX: Stochastic Processes */
        "stochastic_process_definition", "markov_property", "stationary_distribution", "random_walk",
        "martingale_definition", "brownian_motion",
        /* Group X: Advanced Topics */
        "characteristic_function", "convergence_in_probability", "almost_sure_convergence",
        "convergence_in_distribution", "lp_convergence"};

    for (int i = 0; i < (int) (sizeof(expected_templates) / sizeof(expected_templates[0])); i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expected_templates[i]);
        char msg[128];
        snprintf(msg, sizeof(msg), "template '%s' should exist", expected_templates[i]);
        TEST_ASSERT(tmpl != NULL, msg);
    }

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 3: Verify unconstructible problems                             */
/* ------------------------------------------------------------------ */
static void test_unconstructibles(void) {
    printf("Test 3: Verify unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 7 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    const char *expected_unconstructibles[] = {"martin_lof_randomness_test",   "normality_of_pi",
                                               "exact_probability_continuous", "bayesian_exact_inference",
                                               "optimal_stopping_general",     "random_walk_recurrence_general",
                                               "bertrand_paradox_resolution"};

    for (int i = 0; i < (int) (sizeof(expected_unconstructibles) / sizeof(expected_unconstructibles[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected_unconstructibles[i]);
        char msg[128];
        snprintf(msg, sizeof(msg), "unconstructible '%s' should exist", expected_unconstructibles[i]);
        TEST_ASSERT(uc != NULL, msg);
    }

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 4: Verify logical framework                                    */
/* ------------------------------------------------------------------ */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    TEST_ASSERT(strcmp(pkg->bottom_geometry, "probability_space_triple") == 0,
                "bottom_geometry should be 'probability_space_triple'");
    printf("  Bottom geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    TEST_ASSERT(strcmp(pkg->negation_encoding, "event_complement") == 0,
                "negation_encoding should be 'event_complement'");
    printf("  Negation encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 5: Content hash computation                                    */
/* ------------------------------------------------------------------ */
static void test_content_hash(void) {
    printf("Test 5: Compute content hash...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should be computed");
    TEST_ASSERT(strlen(hash) == 64, "hash should be 64 hex characters");

    printf("  Content hash: %.16s...%s\n", hash, hash + 56);

    lv00_free_ptr(hash);
    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 6: Round-trip save/load                                        */
/* ------------------------------------------------------------------ */
static void test_round_trip(void) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg1, AXIOM_PKG_PATH);

    AxiomSaveStatus save_status = axiom_package_save(pkg1, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "save should succeed");

    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "load from saved file should succeed");

    TEST_ASSERT(strcmp(pkg1->name, pkg2->name) == 0, "names should match after round-trip");
    TEST_ASSERT(strcmp(pkg1->version, pkg2->version) == 0, "versions should match after round-trip");
    TEST_ASSERT(pkg1->template_count == pkg2->template_count, "template counts should match after round-trip");
    TEST_ASSERT(pkg1->unconstructible_count == pkg2->unconstructible_count,
                "unconstructible counts should match after round-trip");

    printf("  Round-trip: '%s' v%s, %d templates, %d unconstructibles\n", pkg2->name, pkg2->version,
           pkg2->template_count, pkg2->unconstructible_count);

    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

/* ------------------------------------------------------------------ */
/* Test 7: Dependency validation                                       */
/* ------------------------------------------------------------------ */
static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Probability theory depends on measure_theory and zfc_set_theory */
    AxiomPackage *loaded_packages[] = {pkg};
    bool valid = axiom_package_validate_dependencies(pkg, loaded_packages, 1);
    TEST_ASSERT(valid, "dependency validation should succeed (self-reference)");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 8: Negative lookups                                           */
/* ------------------------------------------------------------------ */
static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "nonexistent template should return NULL");

    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "nonexistent unconstructible should return NULL");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 9: External references                                         */
/* ------------------------------------------------------------------ */
static void test_external_refs(void) {
    printf("Test 9: Verify external references...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Check Martin-Löf randomness test has external reference */
    KnownUnconstructible *uc1 = axiom_package_lookup_unconstructible(pkg, "martin_lof_randomness_test");
    TEST_ASSERT(uc1 != NULL && uc1->external_ref != NULL, "martin_lof_randomness_test should have external_ref");
    TEST_ASSERT(strstr(uc1->external_ref, "wikipedia.org") != NULL, "external_ref should contain wikipedia.org");
    printf("  martin_lof_randomness_test ref: %s\n", uc1->external_ref);

    /* Check normality of pi has external reference */
    KnownUnconstructible *uc2 = axiom_package_lookup_unconstructible(pkg, "normality_of_pi");
    TEST_ASSERT(uc2 != NULL && uc2->external_ref != NULL, "normality_of_pi should have external_ref");
    printf("  normality_of_pi ref: %s\n", uc2->external_ref);

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 10: Key Kolmogorov axioms present                              */
/* ------------------------------------------------------------------ */
static void test_key_axioms(void) {
    printf("Test 10: Verify key Kolmogorov axioms...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* The three Kolmogorov axioms are fundamental */
    const char *kolmogorov_axioms[] = {
        "probability_non_negative",  /* K1: P(E) >= 0 */
        "probability_normalization", /* K2: P(Ω) = 1 */
        "sigma_additivity"           /* K3: σ-additivity */
    };

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, kolmogorov_axioms[i]);
        char msg[128];
        snprintf(msg, sizeof(msg), "Kolmogorov axiom '%s' should exist", kolmogorov_axioms[i]);
        TEST_ASSERT(tmpl != NULL, msg);
    }

    /* Verify key theorems */
    const char *key_theorems[] = {"bayes_theorem", "central_limit_theorem", "weak_law_large_numbers",
                                  "strong_law_large_numbers"};

    for (int i = 0; i < (int) (sizeof(key_theorems) / sizeof(key_theorems[0])); i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, key_theorems[i]);
        char msg[128];
        snprintf(msg, sizeof(msg), "Key theorem '%s' should exist", key_theorems[i]);
        TEST_ASSERT(tmpl != NULL, msg);
    }

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(void) {
    printf("==========================================================\n");
    printf("Lv-00 Axiom Package Test: probability_theory v1.0.0\n");
    printf("==========================================================\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructibles();
    test_logical_framework();
    test_content_hash();
    test_round_trip();
    test_dependency_validation();
    test_negative_lookups();
    test_external_refs();
    test_key_axioms();

    printf("\n==========================================================\n");
    printf("Results: %d passed, %d failed\n", g_pass_count, g_fail_count);
    printf("==========================================================\n");

    return g_fail_count > 0 ? 1 : 0;
}
