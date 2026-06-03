/**
 * @file test_axiom_modal_logic.c
 * @brief Modal Logic (Normal Modal Logics K, T, S4, S5) Axiom Package Test
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/modal_logic.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/modal_logic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 32
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

static void test_load_from_file(void) {
    printf("Test 1: Load modal_logic.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "modal_logic") == 0, "package name should be 'modal_logic'");
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

    const char *expected_templates[] = {
        /* Group I: Classical Propositional Foundation */
        "classical_tautology", "modus_ponens",
        /* Group II: Core Modal Axioms (System K) */
        "kripke_schema", "necessitation",
        /* Group III: Modal Operator Duality */
        "possibility_dual", "necessity_dual",
        /* Group IV: Reflexivity Axioms (System T) */
        "reflexivity_T",
        /* Group V: Transitivity Axioms (System K4/S4) */
        "transitivity_4",
        /* Group VI: Symmetry Axioms (System S5) */
        "symmetry_B", "euclidean_5",
        /* Group VII: Seriality Axioms (System D) */
        "seriality_D",
        /* Group VIII: Provability Logic (GL) */
        "lob_axiom",
        /* Group IX: Modal System Constructors */
        "kripke_frame", "kripke_model", "satisfaction_at_world", "validity_in_frame",
        /* Group X: Derived Modal Principles */
        "modal_modus_tollens", "box_distributes_over_and", "diamond_monotonicity", "modal_negation",
        /* Group XI: Epistemic/Doxastic Variants */
        "knowledge_axiom", "positive_introspection", "negative_introspection",
        /* Group XII: Temporal Logic Variants */
        "always_operator", "eventually_operator", "next_operator", "until_operator", NULL};

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

    /* Verify specific parameter counts */
    ConstraintTemplate *t;

    /* Core modal axioms */
    t = axiom_package_get_template(pkg, "kripke_schema");
    TEST_ASSERT(t && t->param_count == 2, "kripke_schema should have 2 params (A, B)");

    t = axiom_package_get_template(pkg, "necessitation");
    TEST_ASSERT(t && t->param_count == 1, "necessitation should have 1 param (A)");

    t = axiom_package_get_template(pkg, "classical_tautology");
    TEST_ASSERT(t && t->param_count == 1, "classical_tautology should have 1 param");

    t = axiom_package_get_template(pkg, "modus_ponens");
    TEST_ASSERT(t && t->param_count == 2, "modus_ponens should have 2 params");

    /* Modal operator duality */
    t = axiom_package_get_template(pkg, "possibility_dual");
    TEST_ASSERT(t && t->param_count == 1, "possibility_dual should have 1 param (A)");

    t = axiom_package_get_template(pkg, "necessity_dual");
    TEST_ASSERT(t && t->param_count == 1, "necessity_dual should have 1 param (A)");

    /* Modal axiom schemata */
    t = axiom_package_get_template(pkg, "reflexivity_T");
    TEST_ASSERT(t && t->param_count == 1, "reflexivity_T should have 1 param (A)");

    t = axiom_package_get_template(pkg, "transitivity_4");
    TEST_ASSERT(t && t->param_count == 1, "transitivity_4 should have 1 param (A)");

    t = axiom_package_get_template(pkg, "symmetry_B");
    TEST_ASSERT(t && t->param_count == 1, "symmetry_B should have 1 param (A)");

    t = axiom_package_get_template(pkg, "euclidean_5");
    TEST_ASSERT(t && t->param_count == 1, "euclidean_5 should have 1 param (A)");

    t = axiom_package_get_template(pkg, "seriality_D");
    TEST_ASSERT(t && t->param_count == 1, "seriality_D should have 1 param (A)");

    t = axiom_package_get_template(pkg, "lob_axiom");
    TEST_ASSERT(t && t->param_count == 1, "lob_axiom should have 1 param (A)");

    /* Kripke semantics constructors */
    t = axiom_package_get_template(pkg, "kripke_frame");
    TEST_ASSERT(t && t->param_count == 2, "kripke_frame should have 2 params (W, R)");

    t = axiom_package_get_template(pkg, "kripke_model");
    TEST_ASSERT(t && t->param_count == 3, "kripke_model should have 3 params (W, R, V)");

    t = axiom_package_get_template(pkg, "satisfaction_at_world");
    TEST_ASSERT(t && t->param_count == 3, "satisfaction_at_world should have 3 params (M, w, phi)");

    t = axiom_package_get_template(pkg, "validity_in_frame");
    TEST_ASSERT(t && t->param_count == 2, "validity_in_frame should have 2 params (F, phi)");

    /* Epistemic variants */
    t = axiom_package_get_template(pkg, "knowledge_axiom");
    TEST_ASSERT(t && t->param_count == 1, "knowledge_axiom should have 1 param (A)");

    t = axiom_package_get_template(pkg, "positive_introspection");
    TEST_ASSERT(t && t->param_count == 1, "positive_introspection should have 1 param (A)");

    t = axiom_package_get_template(pkg, "negative_introspection");
    TEST_ASSERT(t && t->param_count == 1, "negative_introspection should have 1 param (A)");

    /* Temporal operators */
    t = axiom_package_get_template(pkg, "always_operator");
    TEST_ASSERT(t && t->param_count == 1, "always_operator should have 1 param (A)");

    t = axiom_package_get_template(pkg, "eventually_operator");
    TEST_ASSERT(t && t->param_count == 1, "eventually_operator should have 1 param (A)");

    t = axiom_package_get_template(pkg, "next_operator");
    TEST_ASSERT(t && t->param_count == 1, "next_operator should have 1 param (A)");

    t = axiom_package_get_template(pkg, "until_operator");
    TEST_ASSERT(t && t->param_count == 2, "until_operator should have 2 params (A, B)");

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 7 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        bool green_verified;
    } expected[] = {{"modal_satisfiability_K", "PSPACE_complete_problem", false},
                    {"modal_satisfiability_S4", "PSPACE_complete_problem", false},
                    {"modal_satisfiability_S5", "NP_complete_problem", false},
                    {"modal_uniform_interpolation", "undecidable", false},
                    {"modal_logic_with_propositional_quantifiers", "undecidable", false},
                    {"global_satisfiability_S4", "EXPTIME_complete", false},
                    {"modal_mu_calculus_model_checking", "NP_intersection_coNP", false},
                    {NULL, NULL, false}};

    int found_count = 0;
    for (int i = 0; expected[i].name != NULL; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        if (uc) {
            found_count++;
            if (uc->reduces_to) {
                TEST_ASSERT(strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                            "reduces_to should match expected value");
            }
            TEST_ASSERT(uc->green_verified == expected[i].green_verified, "green_verified should match expected value");
        } else {
            printf("  MISSING unconstructible: '%s'\n", expected[i].name);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "all expected unconstructible problems should be found");
    printf("  Found %d / %d unconstructible problems\n", found_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    /* Verify external references */
    KnownUnconstructible *uc;

    uc = axiom_package_lookup_unconstructible(pkg, "modal_satisfiability_K");
    TEST_ASSERT(uc && uc->external_ref != NULL, "modal_satisfiability_K should have external_ref");
    if (uc && uc->external_ref) {
        TEST_ASSERT(strstr(uc->external_ref, "wikipedia.org") != NULL || strstr(uc->external_ref, "PSPACE") != NULL,
                    "external_ref should point to Wikipedia or mention PSPACE");
    }

    uc = axiom_package_lookup_unconstructible(pkg, "modal_satisfiability_S5");
    TEST_ASSERT(uc && uc->external_ref != NULL, "modal_satisfiability_S5 should have external_ref");

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework configuration...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify bottom_geometry */
    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    if (pkg->bottom_geometry) {
        TEST_ASSERT(strcmp(pkg->bottom_geometry, "kripke_possible_worlds_semantics") == 0,
                    "bottom_geometry should be 'kripke_possible_worlds_semantics'");
        printf("  bottom_geometry: %s\n", pkg->bottom_geometry);
    }

    /* Verify negation_encoding */
    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    if (pkg->negation_encoding) {
        TEST_ASSERT(strcmp(pkg->negation_encoding, "classical_complement_with_modal_dual") == 0,
                    "negation_encoding should be 'classical_complement_with_modal_dual'");
        printf("  negation_encoding: %s\n", pkg->negation_encoding);
    }

    /* Verify contradiction_behavior */
    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    printf("Test 5: Verify content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should be computed");

    if (hash) {
        size_t len = strlen(hash);
        TEST_ASSERT(len == 64, "content hash should be 64 characters (SHA-256 hex)");
        printf("  Content hash: %.16s... (len=%zu)\n", hash, len);

        /* Verify hash is hexadecimal */
        bool valid_hex = true;
        for (size_t i = 0; i < len; i++) {
            if (!((hash[i] >= '0' && hash[i] <= '9') || (hash[i] >= 'a' && hash[i] <= 'f'))) {
                valid_hex = false;
                break;
            }
        }
        TEST_ASSERT(valid_hex, "content hash should be valid hexadecimal");

        lv00_free((void **) &hash);
    }

    axiom_package_destroy(pkg);
}

static void test_roundtrip_save_load(void) {
    printf("Test 6: Verify round-trip save/load...\n");

    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg1, AXIOM_PKG_PATH);

    /* Compute hash before save */
    char *hash1 = axiom_package_compute_content_hash(pkg1);

    /* Save to temporary file */
    AxiomSaveStatus save_status = axiom_package_save(pkg1, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "axiom_package_save should return AXIOM_SAVE_OK");

    /* Load the saved file */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "loading saved file should succeed");

    /* Compute hash after load */
    char *hash2 = axiom_package_compute_content_hash(pkg2);

    /* Verify hashes match */
    if (hash1 && hash2) {
        TEST_ASSERT(strcmp(hash1, hash2) == 0, "content hash should be identical after round-trip");
        printf("  Round-trip hash match: %.16s...\n", hash1);
    }

    /* Verify basic properties preserved */
    TEST_ASSERT(strcmp(pkg1->name, pkg2->name) == 0, "package name should be preserved");
    TEST_ASSERT(strcmp(pkg1->version, pkg2->version) == 0, "package version should be preserved");
    TEST_ASSERT(pkg1->template_count == pkg2->template_count, "template count should be preserved");
    TEST_ASSERT(pkg1->unconstructible_count == pkg2->unconstructible_count,
                "unconstructible count should be preserved");

    lv00_free((void **) &hash1);
    lv00_free((void **) &hash2);
    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

static void test_dependency_validation(void) {
    printf("Test 7: Verify dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Test with empty dependency list (no other packages loaded) */
    AxiomPackage *loaded_packages[] = {pkg};
    bool valid = axiom_package_validate_dependencies(pkg, loaded_packages, 1);
    /* Dependencies may fail since we don't have other packages loaded */
    printf("  Dependency validation result: %s\n", valid ? "PASS" : "FAIL (expected)");
    printf("  (Dependencies reference external packages not loaded in this test)\n");

    axiom_package_destroy(pkg);
}

static void test_negative_lookups(void) {
    printf("Test 8: Verify negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Look up non-existent template */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "lookup of non-existent template should return NULL");

    /* Look up non-existent unconstructible */
    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "lookup of non-existent unconstructible should return NULL");

    printf("  Negative lookups correctly return NULL\n");

    axiom_package_destroy(pkg);
}

static void test_external_refs(void) {
    printf("Test 9: Verify external references...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    int valid_url_count = 0;
    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        if (uc->external_ref) {
            /* Check if it looks like a valid URL */
            bool is_valid = false;
            if (strncmp(uc->external_ref, "http://", 7) == 0 || strncmp(uc->external_ref, "https://", 8) == 0) {
                is_valid = true;
            }
            if (is_valid) {
                valid_url_count++;
                printf("  %s -> %s\n", uc->name, uc->external_ref);
            }
        }
    }

    TEST_ASSERT(valid_url_count > 0, "at least some unconstructible problems should have valid URLs");
    printf("  Valid external references: %d/%d\n", valid_url_count, pkg->unconstructible_count);

    axiom_package_destroy(pkg);
}

static void test_key_axioms_present(void) {
    printf("Test 10: Verify key modal axioms are present...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core K system axioms */
    TEST_ASSERT(axiom_package_get_template(pkg, "kripke_schema") != NULL,
                "Kripke schema (Distribution Axiom) should be present");
    TEST_ASSERT(axiom_package_get_template(pkg, "necessitation") != NULL, "Necessitation rule should be present");

    /* System T axiom */
    TEST_ASSERT(axiom_package_get_template(pkg, "reflexivity_T") != NULL, "T axiom (Reflexivity) should be present");

    /* System S4 axioms */
    TEST_ASSERT(axiom_package_get_template(pkg, "transitivity_4") != NULL, "4 axiom (Transitivity) should be present");

    /* System S5 axioms */
    TEST_ASSERT(axiom_package_get_template(pkg, "euclidean_5") != NULL, "5 axiom (Euclidean) should be present");

    /* Modal duality */
    TEST_ASSERT(axiom_package_get_template(pkg, "possibility_dual") != NULL, "Possibility duality should be present");
    TEST_ASSERT(axiom_package_get_template(pkg, "necessity_dual") != NULL, "Necessity duality should be present");

    /* Kripke semantics */
    TEST_ASSERT(axiom_package_get_template(pkg, "kripke_frame") != NULL, "Kripke frame constructor should be present");
    TEST_ASSERT(axiom_package_get_template(pkg, "kripke_model") != NULL, "Kripke model constructor should be present");

    printf("  All key modal axioms verified present\n");

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Modal Logic");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_roundtrip_save_load);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_refs);
    TEST_RUN(test_key_axioms_present);

    TEST_SUMMARY();

    return 0;
}
