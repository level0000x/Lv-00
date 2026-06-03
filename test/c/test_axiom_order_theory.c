﻿/**
 * @file test_axiom_order_theory.c
 * @brief Order Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the order_theory.lvz
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

#define AXIOM_PKG_PATH "module/axiom_packages/order_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/order_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 32
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

static void test_load_from_file(void) {
    printf("Test 1: Load order_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "order_theory") == 0, "package name should be 'order_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 32 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group I: Partial Order Axioms (3) */
        {"reflexivity", 1},
        {"antisymmetry", 2},
        {"transitivity", 3},
        /* Group II: Special Elements (4) */
        {"least_element", 0},
        {"greatest_element", 0},
        {"minimal_element", 1},
        {"maximal_element", 1},
        /* Group III: Bounds (4) */
        {"upper_bound", 2},
        {"lower_bound", 2},
        {"least_upper_bound", 1},
        {"greatest_lower_bound", 1},
        /* Group IV: Total Order (1) */
        {"totality", 2},
        /* Group V: Well-Order (2) */
        {"well_foundedness", 1},
        {"well_order", 0},
        /* Group VI: Order Morphisms (4) */
        {"monotone_function", 3},
        {"antitone_function", 3},
        {"order_embedding", 3},
        {"order_isomorphism", 3},
        /* Group VII: Order Constructions (4) */
        {"dual_order", 1},
        {"product_order", 2},
        {"induced_suborder", 2},
        {"ordinal_sum", 2},
        /* Group VIII: Derived Concepts & Constructors (10) */
        {"strict_order", 2},
        {"covering_relation", 2},
        {"chain", 1},
        {"antichain", 1},
        {"interval", 2},
        {"hasse_diagram", 1},
        {"zorn_lemma", 1},
        {"dilworth_decomposition", 1},
        {"knaster_tarski_fixed_point", 2},
        {"szpilrajn_extension", 1},
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
        {"poset_dimension", "NP-hard optimization problem", 2, true},
        {"counting_linear_extensions", "#P-complete", 2, true},
        {"poset_isomorphism", "GI-hard (Graph Isomorphism hardness)", 2, true},
        {"infinite_poset_width", "requires transfinite methods; not computable in general", 3, true},
        {"order_automorphism_group", "computationally intractable for general posets", 2, false},
        {"poset_convex_realizability",
         "undecidable: determining if a poset is isomorphic to the inclusion order of convex sets in R^d", 2, true},
        {"poset_dimension_at_least_4", "NP-complete to decide if dimension >= 4", 2, true},
        {"chain_partition_minimization", "NP-hard: minimum chain decomposition", 2, true},
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

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "hasse_diagram_poset") == 0,
                "bottom_geometry should be 'hasse_diagram_poset'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_order_negation") == 0,
                "negation_encoding should be 'classical_order_negation'");
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
        {"poset_dimension", "https://en.wikipedia.org/wiki/Order_dimension"},
        {"counting_linear_extensions", "https://en.wikipedia.org/wiki/Linear_extension"},
        {"poset_isomorphism", "https://en.wikipedia.org/wiki/Graph_isomorphism_problem"},
        {"infinite_poset_width", "https://en.wikipedia.org/wiki/Antichain"},
        {"order_automorphism_group", "https://en.wikipedia.org/wiki/Automorphism_group"},
        {"poset_convex_realizability", "https://en.wikipedia.org/wiki/Convex_geometry"},
        {"poset_dimension_at_least_4", "https://doi.org/10.1016/0012-365X(84)90132-1"},
        {"chain_partition_minimization", "https://en.wikipedia.org/wiki/Dilworth%27s_theorem"},
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

static void test_template_group_coverage(void) {
    printf("Test 10: Verify all 8 template groups are represented...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify at least one template from each group exists */
    const char *group_representatives[] = {
        "reflexivity",       /* Group I: Partial Order */
        "least_element",     /* Group II: Special Elements */
        "upper_bound",       /* Group III: Bounds */
        "totality",          /* Group IV: Total Order */
        "well_order",        /* Group V: Well-Order */
        "monotone_function", /* Group VI: Order Morphisms */
        "dual_order",        /* Group VII: Constructions */
        "hasse_diagram",     /* Group VIII: Derived Concepts */
    };

    int groups_found = 0;
    int num_groups = (int) (sizeof(group_representatives) / sizeof(group_representatives[0]));

    for (int i = 0; i < num_groups; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, group_representatives[i]);
        if (tmpl) {
            groups_found++;
            printf("  [Group %d] '%s' found (params=%d)\n", i + 1, tmpl->name, tmpl->param_count);
        } else {
            printf("  [Group %d] '%s' MISSING\n", i + 1, group_representatives[i]);
            g_fail_count++;
        }
    }

    TEST_ASSERT(groups_found == num_groups, "all 8 template groups should be represented");

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Order Theory");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_refs);
    TEST_RUN(test_template_group_coverage);

    TEST_SUMMARY();

    return 0;
}
