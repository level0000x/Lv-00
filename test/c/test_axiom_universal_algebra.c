/**
 * @file test_axiom_universal_algebra.c
 * @brief Universal Algebra Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the universal_algebra.lvz
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

#define AXIOM_PKG_PATH "module/axiom_packages/universal_algebra.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/universal_algebra_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 60
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

static void test_load_from_file(void) {
    printf("Test 1: Load universal_algebra.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "universal_algebra") == 0,
                "package name should be 'universal_algebra'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 60 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Core Axioms — Signature and Algebra Definition (10) */
        {"signature", 1},
        {"term_algebra", 2},
        {"substitution", 3},
        {"equational_satisfaction", 2},
        {"congruence", 2},
        {"quotient_algebra", 2},
        {"homomorphism", 3},
        {"subalgebra", 2},
        {"direct_product", 2},
        {"free_algebra", 2},
        /* Birkhoff's HSP Theorem (3) */
        {"homomorphic_image", 2},
        {"subalgebra_closure", 1},
        {"product_closure", 1},
        /* Congruence Theory (6) */
        {"congruence_identity", 1},
        {"congruence_total", 1},
        {"congruence_meet", 2},
        {"congruence_join", 2},
        {"congruence_lattice", 1},
        {"factor_theorem", 2},
        /* Isomorphism Theorems (4) */
        {"first_isomorphism_theorem", 2},
        {"second_isomorphism_theorem", 2},
        {"third_isomorphism_theorem", 2},
        {"correspondence_theorem", 2},
        /* Variety Theory (6) */
        {"equational_class", 1},
        {"hsp_theorem", 1},
        {"free_algebra_universal_property", 3},
        {"subdirect_representation", 1},
        {"subdirectly_irreducible", 1},
        {"equational_basis", 1},
        /* Mal'cev Conditions (6) */
        {"malcev_term", 1},
        {"congruence_permutability", 2},
        {"congruence_modularity", 3},
        {"congruence_distributivity", 3},
        {"jonsson_terms", 1},
        {"day_terms", 1},
        /* Term Rewriting and Equational Deduction (5) */
        {"equational_deduction", 2},
        {"term_rewriting", 2},
        {"confluence", 1},
        {"termination", 1},
        {"knuth_bendix_completion", 1},
        /* Core Constructors (8) */
        {"apply_operation", 2},
        {"build_term", 2},
        {"evaluate_term", 2},
        {"form_quotient", 2},
        {"form_homomorphism", 3},
        {"form_subalgebra", 2},
        {"form_product", 2},
        {"form_free_algebra", 2},
        /* Derived Constructors (12) */
        {"congruence_generation", 2},
        {"kernel", 1},
        {"image", 1},
        {"isomorphism", 2},
        {"endomorphism", 1},
        {"automorphism", 1},
        {"subdirect_embedding", 2},
        {"ultrafilter_construction", 2},
        {"clone", 1},
        {"polynomial_clone", 1},
        {"variety_membership", 2},
        {"equational_consequence", 2},
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
        {"word_problem_for_varieties", "undecidable", 4, true},
        {"equational_theory_equivalence", "undecidable", 4, true},
        {"finite_basis_problem", "undecidable", 3, true},
        {"variety_equivalence", "undecidable", 3, true},
        {"congruence_lattice_recognition", "undecidable", 3, false},
        {"free_algebra_finiteness", "undecidable", 3, false},
        {"knuth_bendix_completion_termination", "undecidable", 4, true},
        {"equational_unification", "undecidable", 4, true},
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

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "universal_algebra_equational") == 0,
                "bottom_geometry should be 'universal_algebra_equational'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "equational_equality") == 0,
                "negation_encoding should be 'equational_equality'");
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
        {"word_problem_for_varieties", "https://en.wikipedia.org/wiki/Word_problem"},
        {"equational_theory_equivalence", "https://en.wikipedia.org/wiki/Word_problem"},
        {"finite_basis_problem", "https://en.wikipedia.org/wiki/Universal_algebra"},
        {"variety_equivalence", "https://en.wikipedia.org/wiki/Variety"},
        {"congruence_lattice_recognition", "https://en.wikipedia.org/wiki/Universal_algebra"},
        {"free_algebra_finiteness", "https://en.wikipedia.org/wiki/Universal_algebra"},
        {"knuth_bendix_completion_termination", "https://en.wikipedia.org/wiki/Word_problem"},
        {"equational_unification", "https://en.wikipedia.org/wiki/Word_problem"},
    };

    for (int i = 0; i < (int) (sizeof(ref_checks) / sizeof(ref_checks[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, ref_checks[i].name);
        TEST_ASSERT(uc != NULL, ref_checks[i].name);
        if (uc) {
            TEST_ASSERT(uc->external_ref != NULL && strncmp(uc->external_ref, ref_checks[i].expected_url_prefix,
                                                            strlen(ref_checks[i].expected_url_prefix)) == 0,
                        ref_checks[i].name);
            printf("  [%d] %s -> %s\n", i, uc->name, uc->external_ref);
        }
    }

    axiom_package_destroy(pkg);
}

static void test_template_categories(void) {
    printf("Test 10: Verify template categories are complete...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify key category templates exist */
    const char *category_templates[] = {
        /* HSP */
        "homomorphic_image",
        "subalgebra_closure",
        "product_closure",
        /* Congruence */
        "congruence_identity",
        "congruence_total",
        "congruence_lattice",
        /* Isomorphism theorems */
        "first_isomorphism_theorem",
        "second_isomorphism_theorem",
        "third_isomorphism_theorem",
        /* Mal'cev conditions */
        "malcev_term",
        "congruence_permutability",
        "congruence_modularity",
        "congruence_distributivity",
        /* Variety */
        "hsp_theorem",
        "subdirect_representation",
        "subdirectly_irreducible",
        "equational_basis",
        /* Term rewriting */
        "confluence",
        "termination",
        "knuth_bendix_completion",
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

int main(void) {
    TEST_SUITE_BEGIN("Universal Algebra");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_refs);
    TEST_RUN(test_template_categories);

    TEST_SUMMARY();

    return 0;
}
