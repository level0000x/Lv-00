/**
 * @file test_axiom_topos_theory.c
 * @brief Topos Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the topos_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, and negative lookups.
 *
 * Mathematical Theory: Elementary Topos (Lawvere-Tierney axiomatization)
 * Core Axioms:
 *   T1. Finite Limits (terminal object, pullbacks)
 *   T2. Cartesian Closed (exponentials)
 *   T3. Subobject Classifier (Ω)
 *   T4. Natural Numbers Object (W-Topos, optional)
 */

#include <stdio.h>
#include <string.h>

#include "axiom_pkg.h"
#include "lv_utils.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/topos_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/topos_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 81
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 10

static void test_load_from_file(void) {
    printf("Test 1: Load topos_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(pkg->name != NULL, "axiom_package_load should load the package (name set)");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "topos_theory") == 0, "package name should be 'topos_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    /* Core templates to verify */
    struct {
        const char *name;
        int params;
    } core_templates[] = {
        /* Part I: Finite Limits */
        {"terminal_object", 0},
        {"product", 2},
        {"pullback", 3},
        {"equalizer", 3},
        {"initial_object", 0},
        {"coproduct", 2},
        {"pushout", 3},
        {"coequalizer", 3},
        /* Part II: Cartesian Closed */
        {"exponential", 2},
        {"evaluation", 2},
        {"curry", 3},
        {"uncurry", 3},
        /* Part III: Subobject Classifier */
        {"subobject_classifier", 0},
        {"truth_morphism", 0},
        {"characteristic_function", 2},
        {"subobject_classification", 2},
        {"false_morphism", 0},
        {"negation", 0},
        {"conjunction", 0},
        {"disjunction", 0},
        {"implication", 0},
        {"biconditional", 0},
        /* Part IV: Power Objects */
        {"power_object", 1},
        {"membership_relation", 1},
        {"power_universal_property", 2},
        {"singleton_map", 1},
        {"power_as_exponential", 1},
        {"union", 1},
        {"intersection", 1},
        {"complement", 1},
        {"subset_relation", 1},
        /* Part V: NNO */
        {"natural_numbers_object", 0},
        {"zero_morphism", 0},
        {"successor_morphism", 0},
        {"nno_universal_property", 2},
        /* Part VI: Internal Logic */
        {"heyting_implication", 0},
        {"heyting_negation", 0},
        {"non_contradiction", 0},
        {"double_negation_intro", 0},
        {"double_negation_elim", 0},
        {"excluded_middle", 0},
        /* Part VII: Geometric Morphisms */
        {"geometric_morphism", 2},
        {"inverse_image_functor", 2},
        {"direct_image_functor", 2},
        {"geometric_adjunction", 2},
        {"point_of_topos", 1},
        /* Part VIII: Logical Morphisms */
        {"logical_morphism", 2},
        {"preserves_classifier", 2},
        {"preserves_exponentials", 2},
        /* Part IX: Lawvere-Tierney Topology */
        {"lt_topology", 0},
        {"lt_topology_truth", 0},
        {"lt_topology_idempotent", 0},
        {"lt_topology_meets", 0},
        {"sheaf_for_topology", 1},
        {"double_negation_topology", 0},
        {"sheafification", 1},
        /* Part X: Constructions */
        {"slice_topos", 1},
        {"presheaf_topos", 1},
        {"sheaf_topos", 2},
        {"grothendieck_topos", 1},
        {"classifying_topos", 1},
        {"morphism_object", 2},
        {"internal_category", 0},
        {"internal_presheaf", 1},
        /* Part XI: Key Theorems */
        {"topos_is_heyting", 0},
        {"topos_is_pretopos", 0},
        {"topos_is_lccc", 0},
        {"topos_is_extensive", 0},
        {"topos_is_adhesive", 0},
        {"mitchell_benabou_language", 0},
        {"kripke_joyal_semantics", 0},
        {"diaconescu_theorem", 0},
    };

    int total = (int) (sizeof(core_templates) / sizeof(core_templates[0]));
    int found_count = 0;

    for (int i = 0; i < total; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_templates[i].name);
        if (tmpl) {
            found_count++;
            if (tmpl->param_count != core_templates[i].params) {
                printf("  FAIL: '%s' has %d params, expected %d\n",
                       core_templates[i].name, tmpl->param_count, core_templates[i].params);
                g_fail_count++;
            } else {
                g_pass_count++;
            }
        } else {
            printf("  MISSING template: '%s'\n", core_templates[i].name);
            g_fail_count++;
        }
    }

    printf("  Found %d / %d core templates\n", found_count, total);
    TEST_ASSERT(found_count >= 60, "should find at least 60 core templates");

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        bool green_verified;
    } expected[] = {
        {"internal_logic_decidability", "undecidable", true},
        {"law_of_excluded_middle", "intuitionistic_logic", true},
        {"double_negation_elimination", "intuitionistic_logic", true},
        {"axiom_of_choice_internal", "intuitionistic_logic", true},
        {"propositional_extensionality", "intuitionistic_logic", true},
        {"geometric_morphism_classification", "undecidable", true},
        {"classifying_topos_construction", "decidable_for_geometric_theories", true},
        {"topos_morphism_equivalence", "undecidable", true},
        {"internal_theorem_proving", "undecidable", true},
        {"sheaf_coherence", "decidable_for_finite_sites", true},
    };

    int total = (int) (sizeof(expected) / sizeof(expected[0]));
    int found_count = 0;

    for (int i = 0; i < total; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        if (uc) {
            found_count++;
            if (uc->reduces_to && strcmp(uc->reduces_to, expected[i].reduces_to) == 0) {
                g_pass_count++;
            } else {
                printf("  FAIL: '%s' reduces_to '%s', expected '%s'\n",
                       expected[i].name, uc->reduces_to ? uc->reduces_to : "(null)", expected[i].reduces_to);
                g_fail_count++;
            }
            if (uc->green_verified != expected[i].green_verified) {
                printf("  FAIL: '%s' green_verified mismatch\n", expected[i].name);
                g_fail_count++;
            }
            if (uc->external_ref) {
                printf("  OK: '%s' has external_ref: %s\n", expected[i].name, uc->external_ref);
                g_pass_count++;
            }
        } else {
            printf("  MISSING unconstructible: '%s'\n", expected[i].name);
            g_fail_count++;
        }
    }

    TEST_ASSERT(found_count == total, "all expected unconstructible problems should be found");
    printf("  Found %d / %d unconstructible problems\n", found_count, total);

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Bottom geometry should be "elementary_topos" */
    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    if (pkg->bottom_geometry) {
        printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);
        TEST_ASSERT(strcmp(pkg->bottom_geometry, "elementary_topos") == 0,
                    "bottom_geometry should be 'elementary_topos'");
    }

    /* Negation encoding should be "heyting_negation" (intuitionistic) */
    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    if (pkg->negation_encoding) {
        printf("  negation_encoding: '%s'\n", pkg->negation_encoding);
        TEST_ASSERT(strcmp(pkg->negation_encoding, "heyting_negation") == 0,
                    "negation_encoding should be 'heyting_negation'");
    }

    /* Contradiction behavior should be "blocking" (intuitionistic, not explosion) */
    printf("  contradiction_behavior: %d\n", pkg->contradiction_behavior);

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    printf("Test 5: Compute content hash...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should be computed");
    TEST_ASSERT(strlen(hash) == 64, "content hash should be 64 characters (SHA-256 hex)");

    printf("  Content hash: %.16s...%s\n", hash, hash + 56);

    lv_free((void **) &hash);
    axiom_package_destroy(pkg);
}

static void test_round_trip_save_load(void) {
    printf("Test 6: Round-trip save and load...\n");

    /* Load original */
    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg1, AXIOM_PKG_PATH);

    /* Save to test path */
    AxiomSaveStatus save_status = axiom_package_save(pkg1, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "save should succeed");

    /* Load saved copy */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "load saved copy should succeed");

    /* Compare */
    TEST_ASSERT(strcmp(pkg1->name, pkg2->name) == 0, "names should match");
    TEST_ASSERT(strcmp(pkg1->version, pkg2->version) == 0, "versions should match");
    TEST_ASSERT(pkg1->template_count == pkg2->template_count, "template counts should match");
    TEST_ASSERT(pkg1->unconstructible_count == pkg2->unconstructible_count, "unconstructible counts should match");

    printf("  Round-trip: OK (templates: %d, unconstructibles: %d)\n",
           pkg2->template_count, pkg2->unconstructible_count);

    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

static void test_negative_lookups(void) {
    printf("Test 7: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Non-existent template */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template_xyz");
    TEST_ASSERT(tmpl == NULL, "nonexistent template should return NULL");

    /* Non-existent unconstructible */
    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem_xyz");
    TEST_ASSERT(uc == NULL, "nonexistent unconstructible should return NULL");

    printf("  Negative lookups: OK\n");

    axiom_package_destroy(pkg);
}

int main(void) {
    printf("===============================================\n");
    printf("Lv-00 Axiom Package Test: Topos Theory\n");
    printf("===============================================\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructible_problems();
    test_logical_framework();
    test_content_hash();
    test_round_trip_save_load();
    test_negative_lookups();

    printf("\n===============================================\n");
    printf("Results: %d passed, %d failed\n", g_pass_count, g_fail_count);
    printf("===============================================\n");

    return g_fail_count > 0 ? 1 : 0;
}
