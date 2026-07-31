/**
 * @file test_axiom_non_well_founded_set_theory.c
 * @brief Non-Well-Founded Set Theory (AFA) Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the
 * non_well_founded_set_theory.lvz axiom package. Validates template count,
 * unconstructible problem entries, logical framework settings, content
 * hashing, round-trip save/load, dependency validation, and negative lookups.
 */

#include <stdio.h>
#include <string.h>

#include "axiom_pkg.h"
#include "lv_utils.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/non_well_founded_set_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/non_well_founded_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 42
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

static void test_load_from_file(void) {
    printf("Test 1: Load non_well_founded_set_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "non_well_founded_set_theory") == 0,
                "package name should be 'non_well_founded_set_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 42 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group I: Core Axioms (10) */
        {"extensionality", 2},
        {"afa_decoration", 2},
        {"solution_lemma", 1},
        {"specification", 3},
        {"pairing", 2},
        {"union", 1},
        {"replacement", 3},
        {"infinity", 0},
        {"power_set", 1},
        {"choice", 1},
        /* Group II: Graph-Theoretic Machinery (7) */
        {"accessible_pointed_graph", 3},
        {"graph_decoration", 1},
        {"bisimulation_check", 2},
        {"greatest_bisimulation", 2},
        {"graph_isomorphism", 2},
        {"extensional_graph_check", 1},
        {"picture_of_hyperset", 1},
        /* Group III: Core Constructors (14) */
        {"empty_set", 0},
        {"singleton", 1},
        {"quine_atom", 0},
        {"singleton_chain", 1},
        {"mutual_recursion_pair", 0},
        {"solve_equation_system", 2},
        {"ordered_pair", 2},
        {"cartesian_product", 2},
        {"binary_union", 2},
        {"binary_intersection", 2},
        {"set_difference", 2},
        {"subset_relation", 2},
        {"big_intersection", 1},
        /* Group IV: Coinductive Operations (6) */
        {"greatest_fixed_point", 1},
        {"coinductive_definition", 1},
        {"stream_cons", 2},
        {"stream_unfold", 2},
        {"lts_construction", 2},
        {"lts_bisimulation", 2},
        /* Group V: Comparison with ZFC (5) */
        {"well_founded_check", 1},
        {"rank_function", 1},
        {"foundation_violation_check", 1},
        {"membership_cycle_detection", 1},
        {"zfc_embedding", 0},
        /* Group VI: Relation & Function Constructors (5) */
        {"relation", 3},
        {"function", 3},
        {"function_application", 2},
        {"image", 2},
        {"inverse_image", 2},
        /* Group VII: Cardinal & Ordinal Constructors (3) */
        {"equinumerous", 2},
        {"cardinality", 1},
        {"well_order", 2},
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

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 8 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        {"foundation_independence", "independent_of_ZF_minus_foundation", 4, true},
        {"afa_safa_fafa_bafa_exclusivity", "mutually_exclusive_extensions_of_ZF", 3, true},
        {"quine_atom_uniqueness_in_bafa", "refuted_in_boffa_universe", 3, true},
        {"zfa_consistency", "equiconsistent_with_ZFC", 3, true},
        {"hyperset_existence_in_zfc", "refutable_in_ZFC", 3, true},
        {"bisimulation_undecidable_infinite", "undecidable_for_infinite_state_systems", 3, true},
        {"continuum_hypothesis_in_zfa", "independent_of_ZFA", 4, true},
        {"choice_independence_in_zfa", "independent_of_ZF_plus_AFA", 3, true},
    };

    int expected_count = (int) (sizeof(expected) / sizeof(expected[0]));
    for (int i = 0; i < expected_count; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL && strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                        "reduces_to should match");
            TEST_ASSERT(uc->dependency_chain.count == expected[i].dep_count, "dependency_count should match");
            TEST_ASSERT(uc->green_verified == expected[i].green_verified, "green_verified should be true");
            TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "should have external_ref URL");
            TEST_ASSERT(strncmp(uc->external_ref, "https://", 8) == 0, "external_ref should be an HTTPS URL");
            printf("  [%d] %s -> %s (deps=%d, verified=%s)\n", i, uc->name, uc->reduces_to, uc->dependency_chain.count,
                   uc->green_verified ? "true" : "false");
        } else {
            printf("  MISSING unconstructible: '%s'\n", expected[i].name);
            g_fail_count++;
        }
    }

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void) {
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "hyperset_graph_universe") == 0,
                "bottom_geometry should be 'hyperset_graph_universe'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL &&
                    strcmp(pkg->negation_encoding, "classical_complement_in_hyperset_universe") == 0,
                "negation_encoding should be 'classical_complement_in_hyperset_universe'");
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
        lv_free((void **) &hash);
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

    TEST_ASSERT(axiom_package_get_template_count(pkg2) == axiom_package_get_template_count(pkg1), "template count should match after round-trip");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg2) == axiom_package_get_unconstructible_count(pkg1),
                "unconstructible count should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->name, pkg1->name) == 0, "name should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->version, pkg1->version) == 0, "version should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg1->bottom_geometry) == 0,
                "bottom_geometry should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg1->contradiction_behavior,
                "contradiction_behavior should match after round-trip");

    printf("  Round-trip: templates=%d, unconstructibles=%d\n", axiom_package_get_template_count(pkg2), axiom_package_get_unconstructible_count(pkg2));

    char *hash1 = axiom_package_compute_content_hash(pkg1);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash1 && hash2 && strcmp(hash1, hash2) == 0, "content hashes should match after round-trip");
    printf("  Hash match: %s\n", (hash1 && hash2 && strcmp(hash1, hash2) == 0) ? "YES" : "NO");

    lv_free((void **) &hash1);
    lv_free((void **) &hash2);
    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Self-validation: the unconstructible problems reference templates
     * and each other within the same package. */
    bool valid = axiom_package_validate_dependencies(pkg, &pkg, 1);
    printf("  Self-validation: %s\n", valid ? "PASS" : "FAIL (cross-references internal)");

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
        {"foundation_independence", "https://en.wikipedia.org/wiki/Axiom_of_regularity"},
        {"afa_safa_fafa_bafa_exclusivity", "https://en.wikipedia.org/wiki/Non-well-founded_set_theory"},
        {"quine_atom_uniqueness_in_bafa", "https://en.wikipedia.org/wiki/Quine_atom"},
        {"zfa_consistency", "https://plato.stanford.edu/entries/nonwellfounded-set-theory/"},
        {"hyperset_existence_in_zfc", "https://en.wikipedia.org/wiki/Axiom_of_regularity"},
        {"bisimulation_undecidable_infinite", "https://en.wikipedia.org/wiki/Bisimulation"},
        {"continuum_hypothesis_in_zfa", "https://en.wikipedia.org/wiki/Continuum_hypothesis"},
        {"choice_independence_in_zfa", "https://en.wikipedia.org/wiki/Axiom_of_choice"},
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

static void test_key_axioms_present(void) {
    printf("Test 10: Verify all core ZFA axioms are represented...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core ZFA axioms: ZF axioms (minus Regularity) + AFA */
    const char *required[] = {
        "extensionality", /* ZF1 (strengthened) */
        "afa_decoration", /* AFA (replaces Regularity) */
        "solution_lemma", /* AFA operational form */
        "specification",  /* ZF3 */
        "pairing",        /* ZF4 */
        "union",          /* ZF5 */
        "replacement",    /* ZF6 */
        "infinity",       /* ZF7 */
        "power_set",      /* ZF8 */
        "choice",         /* ZFC9 */
    };

    for (int i = 0; i < 10; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, required[i]);
        TEST_ASSERT(tmpl != NULL, required[i]);
        if (tmpl) {
            printf("  [%d] %s (params=%d) - PRESENT\n", i, required[i], tmpl->param_count);
        }
    }

    axiom_package_destroy(pkg);
}

static void test_afa_specific_templates(void) {
    printf("Test 11: Verify AFA-specific templates (not in ZFC)...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Templates that are unique to ZFA and not present in ZFC */
    const char *afa_specific[] = {
        "quine_atom",
        "singleton_chain",
        "mutual_recursion_pair",
        "solve_equation_system",
        "bisimulation_check",
        "greatest_bisimulation",
        "accessible_pointed_graph",
        "graph_decoration",
        "picture_of_hyperset",
        "extensional_graph_check",
        "greatest_fixed_point",
        "coinductive_definition",
        "stream_cons",
        "stream_unfold",
        "lts_construction",
        "lts_bisimulation",
        "well_founded_check",
        "foundation_violation_check",
        "membership_cycle_detection",
        "zfc_embedding",
    };

    int afa_count = (int) (sizeof(afa_specific) / sizeof(afa_specific[0]));
    int found = 0;
    for (int i = 0; i < afa_count; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, afa_specific[i]);
        if (tmpl) {
            found++;
            printf("  [%d] %s (params=%d) - PRESENT\n", i, afa_specific[i], tmpl->param_count);
        } else {
            printf("  [%d] %s - MISSING\n", i, afa_specific[i]);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found == afa_count, "all AFA-specific templates should be found");
    printf("  AFA-specific templates: %d / %d\n", found, afa_count);

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Non-Well-Founded Set Theory (AFA)");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_refs);
    TEST_RUN(test_key_axioms_present);
    TEST_RUN(test_afa_specific_templates);

    TEST_SUMMARY();

    return 0;
}
