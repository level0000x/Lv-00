/**
 * @file test_axiom_intuitionistic_logic.c
 * @brief Intuitionistic Propositional Logic (Heyting's Calculus) Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the intuitionistic_logic.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and the contrast with
 * classical_propositional_logic.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/intuitionistic_logic.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/intuitionistic_logic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 50
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ------------------------------------------------------------------ */
/*  Test 1: Load from file                                             */
/* ------------------------------------------------------------------ */
static void test_load_from_file(void) {
    printf("Test 1: Load intuitionistic_logic.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "intuitionistic_logic") == 0,
                "package name should be 'intuitionistic_logic'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 2: Verify template count and key template names               */
/* ------------------------------------------------------------------ */
static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 50 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    /* Group I: Heyting Core Axioms (10) */
    const char *group_1[] = {
        "axiom_then_1_weakening", "axiom_then_2_distribution", "axiom_and_1_elim_left",
        "axiom_and_2_elim_right", "axiom_and_3_intro",         "axiom_or_1_intro_left",
        "axiom_or_2_intro_right", "axiom_or_3_elim",           "axiom_false_efq",
        "modus_ponens",
    };
    for (int i = 0; i < 10; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, group_1[i]);
        TEST_ASSERT(tmpl != NULL, group_1[i]);
    }

    /* Group II: Primitive Connectives (4) */
    const char *group_2[] = {
        "implication",
        "conjunction",
        "disjunction",
        "falsum",
    };
    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, group_2[i]);
        TEST_ASSERT(tmpl != NULL, group_2[i]);
    }

    /* Group III: PROPOSITION_KIND_CONSTRUCTIVE Negation (4) */
    const char *group_3[] = {
        "double_negation_intro",
        "triple_negation_reduction",
        "negation_intro_schema",
        "verum",
    };
    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, group_3[i]);
        TEST_ASSERT(tmpl != NULL, group_3[i]);
    }

    /* Group VI: Curry-Howard Correspondence (8) — spot-check key ones */
    ConstraintTemplate *ch_k = axiom_package_get_template(pkg, "ch_k_combinator");
    TEST_ASSERT(ch_k != NULL, "ch_k_combinator should exist");
    TEST_ASSERT(ch_k->param_count == 2, "ch_k_combinator should have 2 params");

    ConstraintTemplate *ch_s = axiom_package_get_template(pkg, "ch_s_combinator");
    TEST_ASSERT(ch_s != NULL, "ch_s_combinator should exist");
    TEST_ASSERT(ch_s->param_count == 3, "ch_s_combinator should have 3 params");

    /* Group VII: Boundary Markers — spot-check LEM */
    ConstraintTemplate *lem_b = axiom_package_get_template(pkg, "lem_boundary_marker");
    TEST_ASSERT(lem_b != NULL, "lem_boundary_marker should exist");
    TEST_ASSERT(lem_b->param_count == 1, "lem_boundary_marker should have 1 param");

    printf("  All expected templates verified.\n");
    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 3: Verify unconstructible problem entries                     */
/* ------------------------------------------------------------------ */
static void test_unconstructibles(void) {
    printf("Test 3: Verify unconstructible problem entries...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 7 unconstructible entries");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    /* Check specific entries */
    KnownUnconstructible *uc;

    uc = axiom_package_lookup_unconstructible(pkg, "law_of_excluded_middle_unconstructible");
    TEST_ASSERT(uc != NULL, "LEM unconstructible should exist");
    TEST_ASSERT(uc->reduces_to != NULL && strcmp(uc->reduces_to, "lem_boundary_marker") == 0,
                "LEM should reduce to lem_boundary_marker");
    TEST_ASSERT(uc->green_verified == true, "LEM unprovability in IPL should be green_verified");
    TEST_ASSERT(uc->external_ref != NULL && strstr(uc->external_ref, "wikipedia.org") != NULL,
                "LEM should have Wikipedia external_ref");
    TEST_ASSERT(uc->dependency_chain.count >= 1, "LEM should have at least 1 dependency");

    uc = axiom_package_lookup_unconstructible(pkg, "double_negation_elim_unconstructible");
    TEST_ASSERT(uc != NULL, "DNE unconstructible should exist");
    TEST_ASSERT(uc->green_verified == true, "DNE unprovability in IPL should be green_verified");

    uc = axiom_package_lookup_unconstructible(pkg, "peirce_law_unconstructible");
    TEST_ASSERT(uc != NULL, "Peirce's Law unconstructible should exist");
    TEST_ASSERT(uc->green_verified == true, "Peirce's Law unprovability in IPL should be green_verified");

    uc = axiom_package_lookup_unconstructible(pkg, "ipl_provability_pspace_complete");
    TEST_ASSERT(uc != NULL, "IPL provability PSPACE should exist");

    uc = axiom_package_lookup_unconstructible(pkg, "classical_proof_constructive_translation");
    TEST_ASSERT(uc != NULL, "classical proof translation should exist");

    uc = axiom_package_lookup_unconstructible(pkg, "disjunction_nondefinability");
    TEST_ASSERT(uc != NULL, "disjunction nondefinability should exist");

    uc = axiom_package_lookup_unconstructible(pkg, "admissibility_checking_exptime");
    TEST_ASSERT(uc != NULL, "admissibility checking should exist");

    /* Negative lookup: a nonexistent problem */
    uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "nonexistent problem should return NULL");

    printf("  All 7 unconstructible entries verified with external refs.\n");
    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 4: Verify logical framework settings                          */
/* ------------------------------------------------------------------ */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Bottom geometry: should be intuitionistic_heyting_algebra_open_topology */
    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    TEST_ASSERT(
        strstr(pkg->bottom_geometry, "heyting") != NULL || strstr(pkg->bottom_geometry, "intuitionistic") != NULL,
        "bottom_geometry should reference Heyting/algebra topology");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    /* Negation encoding: BHK interpretation */
    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    TEST_ASSERT(strstr(pkg->negation_encoding, "brouwer") != NULL || strstr(pkg->negation_encoding, "bhk") != NULL ||
                    strstr(pkg->negation_encoding, "falsum") != NULL,
                "negation_encoding should reference BHK/falsum");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    /* Contradiction behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE
     * (ex falso is an axiom in intuitionistic logic too!) */
    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf(
        "  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE (ex falso is PROPOSITION_KIND_CONSTRUCTIVE)\n");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 5: Content hash computation                                   */
/* ------------------------------------------------------------------ */
static void test_content_hash(void) {
    printf("Test 5: Compute content hash...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should be computed");
    TEST_ASSERT(strlen(hash) == 64, "content hash should be 64 hex chars");
    printf("  SHA-256 content hash: %.16s...\n", hash);

    /* Verify deterministic: compute again */
    char *hash2 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash2 != NULL, "second hash computation should succeed");
    TEST_ASSERT(strcmp(hash, hash2) == 0, "content hash should be deterministic");
    printf("  Hash is deterministic: YES\n");

    lv_free_ptr(hash);
    lv_free_ptr(hash2);
    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 6: Round-trip save/load                                       */
/* ------------------------------------------------------------------ */
static void test_round_trip(void) {
    printf("Test 6: Round-trip save and load...\n");

    /* Load original package */
    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Save to test file */
    AxiomSaveStatus save_status = axiom_package_save(pkg, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "package save should succeed");

    /* Load the saved file */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "saved file should load successfully");

    if (load_status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Load error: %s\n", err ? err : "(unknown)");
    }

    /* Verify name and version preserved */
    TEST_ASSERT(strcmp(pkg2->name, "intuitionistic_logic") == 0, "saved/loaded package name should be preserved");
    TEST_ASSERT(strcmp(pkg2->version, "1.0.0") == 0, "saved/loaded package version should be preserved");

    /* Verify template count preserved */
    TEST_ASSERT(axiom_package_get_template_count(pkg2) == EXPECTED_TEMPLATE_COUNT, "saved/loaded template count should match");

    /* Verify unconstructible count preserved */
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg2) == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "saved/loaded unconstructible count should match");

    /* Verify key logical framework settings preserved */
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg->bottom_geometry) == 0,
                "bottom_geometry should be preserved after round-trip");
    TEST_ASSERT(strcmp(pkg2->negation_encoding, pkg->negation_encoding) == 0,
                "negation_encoding should be preserved after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg->contradiction_behavior,
                "contradiction_behavior should be preserved after round-trip");

    printf("  Round-trip successful: template count=%d, unconstructible count=%d\n", axiom_package_get_template_count(pkg2),
           axiom_package_get_unconstructible_count(pkg2));

    axiom_package_destroy(pkg);
    axiom_package_destroy(pkg2);
}

/* ------------------------------------------------------------------ */
/*  Test 7: Dependency validation                                      */
/* ------------------------------------------------------------------ */
static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Validate with self-only: dependencies point to templates
     * that exist within the same package */
    AxiomPackage *packages[1];
    packages[0] = pkg;
    bool valid = axiom_package_validate_dependencies(pkg, packages, 1);
    TEST_ASSERT(valid == true, "self-dependency validation should succeed (all deps are in-package)");

    /* Validate with empty package list: cross-package refs would fail,
     * but in-package deps are checked first */
    bool valid_empty = axiom_package_validate_dependencies(pkg, NULL, 0);
    TEST_ASSERT(valid_empty == true, "dependency validation with empty external list should succeed");

    printf("  Dependency validation: OK\n");
    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 8: Negative lookups                                           */
/* ------------------------------------------------------------------ */
static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Template negative lookup */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "nonexistent template should return NULL");

    /* Unconstructible negative lookup */
    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "no_such_problem");
    TEST_ASSERT(uc == NULL, "nonexistent unconstructible should return NULL");

    printf("  Negative lookups: OK\n");
    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 9: Verify external references for all unconstructibles        */
/* ------------------------------------------------------------------ */
static void test_external_references(void) {
    printf("Test 9: Verify external references for all unconstructibles...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    for (int i = 0; i < axiom_package_get_unconstructible_count(pkg); i++) {
        KnownUnconstructible *uc = axiom_package_get_unconstructible(pkg, i);
        TEST_ASSERT(uc->external_ref != NULL, "every unconstructible should have an external_ref");
        TEST_ASSERT(uc->external_ref[0] != '\0', "external_ref should be non-empty");
        TEST_ASSERT(strstr(uc->external_ref, "https://") == uc->external_ref,
                    "external_ref should be a valid HTTPS URL");
    }
    printf("  All %d external references are valid HTTPS URLs.\n", axiom_package_get_unconstructible_count(pkg));

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 10: Contrast with classical logic — confirm PROPOSITION_KIND_CONSTRUCTIVE     */
/*           principles are marked as unconstructible                  */
/* ------------------------------------------------------------------ */
static void test_classical_boundary(void) {
    printf("Test 10: Verify classical/intuitionistic boundary...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Check that the key classical principles have boundary markers
     * as templates (for structural comparison) but are also marked
     * as unconstructible */
    ConstraintTemplate *lem_tmpl = axiom_package_get_template(pkg, "lem_boundary_marker");
    TEST_ASSERT(lem_tmpl != NULL, "LEM boundary marker should exist as template");

    ConstraintTemplate *dne_tmpl = axiom_package_get_template(pkg, "dne_boundary_marker");
    TEST_ASSERT(dne_tmpl != NULL, "DNE boundary marker should exist as template");

    ConstraintTemplate *peirce_tmpl = axiom_package_get_template(pkg, "peirce_boundary_marker");
    TEST_ASSERT(peirce_tmpl != NULL, "Peirce boundary marker should exist as template");

    /* And they must be in the unconstructible list */
    KnownUnconstructible *lem_uc = axiom_package_lookup_unconstructible(pkg, "law_of_excluded_middle_unconstructible");
    TEST_ASSERT(lem_uc != NULL && lem_uc->green_verified, "LEM should be green_verified as unconstructible");

    KnownUnconstructible *dne_uc = axiom_package_lookup_unconstructible(pkg, "double_negation_elim_unconstructible");
    TEST_ASSERT(dne_uc != NULL && dne_uc->green_verified, "DNE should be green_verified as unconstructible");

    /* Also confirm that double negation INTRO is a valid template */
    ConstraintTemplate *dni_tmpl = axiom_package_get_template(pkg, "double_negation_intro");
    TEST_ASSERT(dni_tmpl != NULL, "double_negation_intro should be a valid template (not boundary marker)");

    printf("  Classical/intuitionistic boundary correctly marked.\n");
    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */
int main(void) {
    TEST_SUITE_BEGIN("Intuitionistic Logic");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructibles);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_references);
    TEST_RUN(test_classical_boundary);

    TEST_SUMMARY();

    return 0;
}
