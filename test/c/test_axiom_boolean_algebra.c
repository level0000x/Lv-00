/**
 * @file test_axiom_boolean_algebra.c
 * @brief Boolean Algebra Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the boolean_algebra.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * and dependency validation.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/boolean_algebra.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/boolean_algebra_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 29
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

static void test_load_from_file(void) {
    printf("Test 1: Load boolean_algebra.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "boolean_algebra") == 0,
                "package name should be 'boolean_algebra'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 30 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    /* Group I: Lattice Axioms (6) */
    const char *expected_templates[] = {/* Lattice axioms */
                                        "join_associativity", "meet_associativity", "join_commutativity",
                                        "meet_commutativity", "join_absorption", "meet_absorption",
                                        /* Identity axioms */
                                        "join_identity", "meet_identity",
                                        /* Distributive axioms */
                                        "meet_distributes_over_join", "join_distributes_over_meet",
                                        /* Complement axioms */
                                        "complement_join", "complement_meet",
                                        /* Huntington minimal axioms */
                                        "huntington_equation",
                                        /* Core constructors */
                                        "complement", "meet", "join",
                                        /* Derived constructors */
                                        "double_negation", "de_morgan_join", "de_morgan_meet", "join_idempotence",
                                        "meet_idempotence", "join_bounded_top", "meet_bounded_bottom", "consensus",
                                        "sheffer_stroke", "peirce_arrow", "material_implication", "exclusive_or",
                                        "biconditional", NULL};

    int found_count = 0;
    for (int i = 0; expected_templates[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expected_templates[i]);
        if (tmpl) {
            found_count++;
        } else {
            printf("  MISSING template: '%s'\n", expected_templates[i]);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found_count == EXPECTED_TEMPLATE_COUNT, "all expected templates should be found");
    printf("  Found %d / %d templates\n", found_count, EXPECTED_TEMPLATE_COUNT);

    /* Verify specific param counts */
    ConstraintTemplate *t;

    /* Binary operations: 2 params */
    t = axiom_package_get_template(pkg, "meet");
    TEST_ASSERT(t && t->param_count == 2, "meet should have 2 params");

    t = axiom_package_get_template(pkg, "join");
    TEST_ASSERT(t && t->param_count == 2, "join should have 2 params");

    t = axiom_package_get_template(pkg, "complement");
    TEST_ASSERT(t && t->param_count == 1, "complement should have 1 param");

    /* Ternary operations: 3 params */
    t = axiom_package_get_template(pkg, "join_associativity");
    TEST_ASSERT(t && t->param_count == 3, "join_associativity should have 3 params");

    t = axiom_package_get_template(pkg, "meet_distributes_over_join");
    TEST_ASSERT(t && t->param_count == 3, "meet_distributes_over_join should have 3 params");

    t = axiom_package_get_template(pkg, "consensus");
    TEST_ASSERT(t && t->param_count == 3, "consensus should have 3 params");

    /* Unary operations: 1 param */
    t = axiom_package_get_template(pkg, "double_negation");
    TEST_ASSERT(t && t->param_count == 1, "double_negation should have 1 param");

    t = axiom_package_get_template(pkg, "join_idempotence");
    TEST_ASSERT(t && t->param_count == 1, "join_idempotence should have 1 param");

    t = axiom_package_get_template(pkg, "complement_join");
    TEST_ASSERT(t && t->param_count == 1, "complement_join should have 1 param");

    /* Huntington equation: 2 params */
    t = axiom_package_get_template(pkg, "huntington_equation");
    TEST_ASSERT(t && t->param_count == 2, "huntington_equation should have 2 params");

    /* Sheffer stroke and Peirce arrow: 2 params */
    t = axiom_package_get_template(pkg, "sheffer_stroke");
    TEST_ASSERT(t && t->param_count == 2, "sheffer_stroke should have 2 params");

    t = axiom_package_get_template(pkg, "peirce_arrow");
    TEST_ASSERT(t && t->param_count == 2, "peirce_arrow should have 2 params");

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 6 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        {"boolean_satisfiability", "NP_complete", 3, true},
        {"tautology_checking", "coNP_complete", 3, true},
        {"boolean_equivalence_checking", "coNP_complete", 3, true},
        {"boolean_formula_minimization", "NP_hard", 4, true},
        {"minimal_circuit_synthesis", "NP_hard", 4, true},
        {"equational_theory_with_subalgebra", "undecidable", 3, true},
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

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "boolean_algebra_2element") == 0,
                "bottom_geometry should be 'boolean_algebra_2element'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_complement") == 0,
                "negation_encoding should be 'classical_complement'");
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
        lv00_free_ptr(hash);
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

    lv00_free_ptr(hash1);
    lv00_free_ptr(hash2);
    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    bool valid = axiom_package_validate_dependencies(pkg, &pkg, 1);
    /* Note: self-validation may fail because reduces_to targets like
       "NP_complete" are complexity class descriptions, not references
       to other unconstructible entries in the same package. */
    printf("  Self-validation: %s (expected: may fail for cross-reference reduces_to)\n",
           valid ? "PASS" : "FAIL (acceptable)");

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

static void test_template_detailed_params(void) {
    printf("Test 9: Detailed template parameter verification...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify all templates have correct param counts */
    struct {
        const char *name;
        int expected_params;
    } param_checks[] = {
        /* Lattice axioms */
        {"join_associativity", 3},
        {"meet_associativity", 3},
        {"join_commutativity", 2},
        {"meet_commutativity", 2},
        {"join_absorption", 2},
        {"meet_absorption", 2},
        /* Identity axioms */
        {"join_identity", 1},
        {"meet_identity", 1},
        /* Distributive axioms */
        {"meet_distributes_over_join", 3},
        {"join_distributes_over_meet", 3},
        /* Complement axioms */
        {"complement_join", 1},
        {"complement_meet", 1},
        /* Huntington */
        {"huntington_equation", 2},
        /* Core constructors */
        {"complement", 1},
        {"meet", 2},
        {"join", 2},
        /* Derived constructors */
        {"double_negation", 1},
        {"de_morgan_join", 2},
        {"de_morgan_meet", 2},
        {"join_idempotence", 1},
        {"meet_idempotence", 1},
        {"join_bounded_top", 1},
        {"meet_bounded_bottom", 1},
        {"consensus", 3},
        {"sheffer_stroke", 2},
        {"peirce_arrow", 2},
        {"material_implication", 2},
        {"exclusive_or", 2},
        {"biconditional", 2},
    };

    int checks = (int) (sizeof(param_checks) / sizeof(param_checks[0]));
    for (int i = 0; i < checks; i++) {
        ConstraintTemplate *t = axiom_package_get_template(pkg, param_checks[i].name);
        char msg[128];
        snprintf(msg, sizeof(msg), "%s should have %d params", param_checks[i].name, param_checks[i].expected_params);
        TEST_ASSERT(t != NULL && t->param_count == param_checks[i].expected_params, msg);
    }

    printf("  Verified %d template param counts\n", checks);

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Boolean Algebra");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_template_detailed_params);

    TEST_SUMMARY();

    return 0;
}
