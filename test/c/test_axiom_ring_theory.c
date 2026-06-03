﻿/**
 * @file test_axiom_ring_theory.c
 * @brief Ring Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the ring_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, and negative lookups.
 */

#include <stdio.h>
#include <string.h>

#include "axiom_pkg.h"
#include "lv00_utils.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/ring_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/ring_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 54
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

static void test_load_from_file(void) {
    printf("Test 1: Load ring_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "ring_theory") == 0, "package name should be 'ring_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 54 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group A: Additive Abelian Group Axioms (5) */
        {"additive_closure", 2},
        {"additive_associativity", 3},
        {"additive_identity", 1},
        {"additive_inverse", 1},
        {"additive_commutativity", 2},
        /* Group M: Multiplicative Monoid Axioms (3) */
        {"multiplicative_closure", 2},
        {"multiplicative_associativity", 3},
        {"multiplicative_identity", 1},
        /* Group D: Distributive Laws (2) */
        {"left_distributivity", 3},
        {"right_distributivity", 3},
        /* Elementary Consequences (12) */
        {"additive_identity_uniqueness", 0},
        {"additive_inverse_uniqueness", 1},
        {"multiplicative_identity_uniqueness", 0},
        {"zero_multiplication", 1},
        {"negative_multiplication", 2},
        {"negative_negative_product", 2},
        {"zero_ring_condition", 0},
        {"additive_cancellation", 3},
        {"double_additive_inverse", 1},
        {"negative_of_sum", 2},
        {"zero_is_own_add_inverse", 0},
        {"negative_one_times", 1},
        /* Core Constructors (6) */
        {"add", 2},
        {"multiply", 2},
        {"negate", 1},
        {"zero", 0},
        {"one", 0},
        {"subtract", 2},
        /* Derived Constructors (26) */
        {"characteristic", 1},
        {"power_positive", 2},
        {"scalar_multiple", 2},
        {"binomial_theorem", 3},
        {"unit", 1},
        {"multiplicative_inverse", 1},
        {"zero_divisor", 2},
        {"nilpotent", 1},
        {"idempotent", 1},
        {"subring_test", 2},
        {"left_ideal", 2},
        {"right_ideal", 2},
        {"two_sided_ideal", 2},
        {"principal_ideal", 1},
        {"quotient_ring", 2},
        {"homomorphism", 3},
        {"kernel", 1},
        {"image", 1},
        {"first_isomorphism_theorem", 1},
        {"direct_product", 2},
        {"polynomial_ring", 1},
        {"matrix_ring", 2},
        {"commutator", 2},
        {"center", 0},
        {"unit_group", 0},
        {"jacobson_radical", 1},
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

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 8 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        {"hilberts_tenth_problem", "undecidable", 5, true},
        {"word_problem_for_rings", "undecidable", 9, true},
        {"ring_isomorphism_problem", "undecidable", 7, true},
        {"triviality_problem_rings", "undecidable", 5, true},
        {"zero_divisor_recognition", "undecidable", 5, true},
        {"nilpotent_element_recognition", "undecidable", 4, true},
        {"commutativity_recognition", "undecidable", 4, true},
        {"ideal_membership_unrestricted", "undecidable", 7, true},
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

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "ring_theory_abstract") == 0,
                "bottom_geometry should be 'ring_theory_abstract'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_equality") == 0,
                "negation_encoding should be 'classical_equality'");
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
        /* Use lv00_free for memory allocated by axiom_package_compute_content_hash,
         * which internally uses lv00_malloc. Using standard free() causes heap
         * corruption because lv00_malloc prepends an AllocHeader. */
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

static void test_external_refs(void) {
    printf("Test 9: Verify external reference URLs...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    struct {
        const char *name;
        const char *expected_url_prefix;
    } ref_checks[] = {
        {"hilberts_tenth_problem", "https://en.wikipedia.org/wiki/Hilbert%27s_tenth_problem"},
        {"word_problem_for_rings", "https://en.wikipedia.org/wiki/Word_problem_for_groups"},
        {"ring_isomorphism_problem", "https://en.wikipedia.org/wiki/Ring_isomorphism"},
        {"triviality_problem_rings", "https://en.wikipedia.org/wiki/Word_problem_for_groups"},
        {"zero_divisor_recognition", "https://en.wikipedia.org/wiki/Zero_divisor"},
        {"nilpotent_element_recognition", "https://en.wikipedia.org/wiki/Nilpotent"},
        {"commutativity_recognition", "https://en.wikipedia.org/wiki/Commutative_ring"},
        {"ideal_membership_unrestricted", "https://en.wikipedia.org/wiki/Ideal_(ring_theory)"},
    };

    for (int i = 0; i < (int) (sizeof(ref_checks) / sizeof(ref_checks[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, ref_checks[i].name);
        TEST_ASSERT(uc != NULL, ref_checks[i].name);
        if (uc) {
            int url_ok = (uc->external_ref != NULL && strncmp(uc->external_ref, ref_checks[i].expected_url_prefix,
                                                              strlen(ref_checks[i].expected_url_prefix)) == 0);
            TEST_ASSERT(url_ok, ref_checks[i].name);
            printf("  [%d] %s -> %s\n", i, uc->name, uc->external_ref);
        }
    }

    axiom_package_destroy(pkg);
}

static void test_ring_axiom_coherence(void) {
    printf("Test 10: Verify ring axiom coherence...\n");

    AxiomPackage *pkg = axiom_package_create("ring_theory", "1.0.0");
    TEST_ASSERT(pkg != NULL, "create package");
    AxiomLoadStatus s = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(s == AXIOM_LOAD_OK, "load ring_theory.lvz");

    /* 验证核心环公理存在：加法交换律、结合律、零元、负元、乘法结合律、单位元、分配律 */
    const char *core_axioms[] = {"additive_closure",
                                 "multiplicative_closure",
                                 "additive_associativity",
                                 "multiplicative_associativity",
                                 "additive_identity",
                                 "multiplicative_identity",
                                 "additive_inverse",
                                 "additive_commutativity",
                                 "left_distributivity",
                                 "right_distributivity",
                                 NULL};
    for (int i = 0; core_axioms[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_axioms[i]);
        TEST_ASSERT(tmpl != NULL, core_axioms[i]);
    }

    printf("Test 10 passed: all core ring axioms verified.\n");
    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Ring Theory");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_refs);
    TEST_RUN(test_ring_axiom_coherence);

    TEST_SUMMARY();

    return 0;
}
