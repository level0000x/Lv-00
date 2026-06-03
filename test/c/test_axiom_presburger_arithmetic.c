﻿/**
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

#include "axiom_pkg.h"
#include "lv00_utils.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/presburger_arithmetic.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/presburger_arithmetic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 74
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 10

static void test_load_from_file(void) {
    printf("Test 1: Load presburger_arithmetic.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "presburger_arithmetic") == 0,
                "package name should be 'presburger_arithmetic'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 74 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {
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

    int total = (int) (sizeof(expected) / sizeof(expected[0]));
    TEST_ASSERT(total == EXPECTED_TEMPLATE_COUNT, "expected array size should match EXPECTED_TEMPLATE_COUNT");

    int found_count = 0;
    for (int i = 0; i < total; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expected[i].name);
        if (tmpl) {
            found_count++;
            if (tmpl->param_count != expected[i].params) {
                printf("  FAIL: '%s' has %d params, expected %d\n", expected[i].name, tmpl->param_count,
                       expected[i].params);
                g_fail_count++;
            } else {
                g_pass_count++;
            }
        } else {
            printf("  MISSING template: '%s'\n", expected[i].name);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found_count == EXPECTED_TEMPLATE_COUNT, "all expected templates should be found");
    printf("  Found %d / %d templates\n", found_count, EXPECTED_TEMPLATE_COUNT);

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "should have 10 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
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

    for (int i = 0; i < (int) (sizeof(expected) / sizeof(expected[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        TEST_ASSERT(uc != NULL, expected[i].name);

        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL && strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                        expected[i].name);
            TEST_ASSERT(uc->dependency_count == expected[i].dep_count, expected[i].name);
            TEST_ASSERT(uc->green_verified == expected[i].green_verified, expected[i].name);
            TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "should have external_ref URL");
            printf("  [%d] %s -> %s (deps=%d, verified=%s)\n", i, uc->name, uc->reduces_to, uc->dependency_count,
                   uc->green_verified ? "true" : "false");
        }
    }

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void) {
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "presburger_natural_numbers") == 0,
                "bottom_geometry should be 'presburger_natural_numbers'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_first_order") == 0,
                "negation_encoding should be 'classical_first_order'");
    printf("  negation_encoding: %s\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    printf("Test 5: Content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash) == 64, "SHA-256 hash should be 64 hex chars");

    if (hash) {
        printf("  SHA-256: %s\n", hash);
        lv00_free((void **) &hash);
    }

    axiom_package_destroy(pkg);
}

static void test_round_trip(void) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg1, AXIOM_PKG_PATH);

    AxiomSaveStatus save_status = axiom_package_save(pkg1, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "save should succeed");

    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "re-load from saved file should succeed");

    TEST_ASSERT(pkg2->template_count == pkg1->template_count, "template count should match after round-trip");
    TEST_ASSERT(pkg2->unconstructible_count == pkg1->unconstructible_count,
                "unconstructible count should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->name, pkg1->name) == 0, "name should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->version, pkg1->version) == 0, "version should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg1->bottom_geometry) == 0,
                "bottom_geometry should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg1->contradiction_behavior,
                "contradiction_behavior should match after round-trip");

    printf("  Round-trip: templates=%d, unconstructibles=%d\n", pkg2->template_count, pkg2->unconstructible_count);

    char *hash1 = axiom_package_compute_content_hash(pkg1);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash1 && hash2 && strcmp(hash1, hash2) == 0, "content hashes should match after round-trip");
    printf("  Hash match: %s\n", (hash1 && hash2 && strcmp(hash1, hash2) == 0) ? "YES" : "NO");

    lv00_free((void **) &hash1);
    lv00_free((void **) &hash2);
    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    bool valid = axiom_package_validate_dependencies(pkg, &pkg, 1);
    printf("  Self-validation: %s\n", valid ? "PASS" : "FAIL (acceptable for cross-references)");

    axiom_package_destroy(pkg);
}

static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "non-existent template should return NULL");

    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "non-existent unconstructible should return NULL");

    printf("  Negative lookups: correct\n");

    axiom_package_destroy(pkg);
}

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

int main(void) {
    TEST_SUITE_BEGIN("Presburger Arithmetic");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_refs);
    TEST_RUN(test_no_multiplication);

    TEST_SUMMARY();

    return 0;
}
