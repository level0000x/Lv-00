/**
 * @file test_axiom_linear_logic.c
 * @brief Linear Logic Axiom Package Test
 *
 * Tests for the linear_logic axiom package (Girard 1987).
 * Verifies package loading, template registration, unconstructible
 * problem tracking, logical framework settings, content hashing,
 * round-trip save/load, and dependency validation.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"

#define AXIOM_PKG_PATH "axiom_packages/linear_logic.lvz"
#define SAVE_TEST_PATH "axiom_packages/linear_logic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 53
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 10

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
/*  Test 1: Load from file                                            */
/* ------------------------------------------------------------------ */
static void test_load_from_file(void) {
    printf("Test 1: Load linear_logic.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "linear_logic") == 0, "package name should be 'linear_logic'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 2: Verify constraint templates                               */
/* ------------------------------------------------------------------ */
static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 53 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    const char *expected_templates[] = {
        /* Identity & Structural */
        "identity_init", "cut_rule", "exchange",
        /* Negation */
        "negation_left", "negation_right", "double_negation_involution", "demorgan_tensor_par", "demorgan_par_tensor",
        "demorgan_with_plus", "demorgan_plus_with", "demorgan_bang_quest", "demorgan_quest_bang",
        /* Multiplicative */
        "tensor_left", "tensor_right", "par_left", "par_right", "one_left", "one_right", "bottom_mult_left",
        "bottom_mult_right", "linear_implication_left", "linear_implication_right",
        /* Additive */
        "with_left", "with_right", "plus_left", "plus_right", "top_right", "zero_left",
        /* Exponential */
        "bang_weakening", "bang_contraction", "bang_dereliction", "bang_promotion", "quest_weakening",
        "quest_contraction", "quest_dereliction", "quest_promotion",
        /* Exponential equivalences */
        "bang_distributes_tensor", "bang_top_equivalence", "quest_distributes_par", "quest_zero_equivalence",
        "bang_to_linear", "bang_comultiplication", "bang_counit",
        /* Derived constructors */
        "intuitionistic_implication_encoding", "classical_conjunction_encoding", "classical_disjunction_encoding",
        "excluded_middle_multiplicative", "asynchronous_phase", "synchronous_phase", "focus_decision",
        "linear_to_intuitionistic_translation", "resource_split", "resource_merge", "resource_consume", NULL};

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

    /* Spot-check parameter counts for key templates */
    ConstraintTemplate *t;

    t = axiom_package_get_template(pkg, "identity_init");
    TEST_ASSERT(t && t->param_count == 1, "identity_init should have 1 param (formula B)");

    t = axiom_package_get_template(pkg, "cut_rule");
    TEST_ASSERT(t && t->param_count == 3, "cut_rule should have 3 params");

    t = axiom_package_get_template(pkg, "one_right");
    TEST_ASSERT(t && t->param_count == 0, "one_right should have 0 params (axiom)");

    t = axiom_package_get_template(pkg, "tensor_right");
    TEST_ASSERT(t && t->param_count == 4, "tensor_right should have 4 params (two contexts + two formulas)");

    t = axiom_package_get_template(pkg, "par_right");
    TEST_ASSERT(t && t->param_count == 3, "par_right should have 3 params");

    t = axiom_package_get_template(pkg, "bang_promotion");
    TEST_ASSERT(t && t->param_count == 3, "bang_promotion should have 3 params");

    t = axiom_package_get_template(pkg, "with_right");
    TEST_ASSERT(t && t->param_count == 3, "with_right should have 3 params (context + two formulas)");

    t = axiom_package_get_template(pkg, "plus_right");
    TEST_ASSERT(t && t->param_count == 3, "plus_right should have 3 params");

    t = axiom_package_get_template(pkg, "linear_implication_right");
    TEST_ASSERT(t && t->param_count == 3, "linear_implication_right should have 3 params");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 3: Verify known unconstructible problems                     */
/* ------------------------------------------------------------------ */
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
        {"provability_full_propositional_linear_logic", "undecidable", 5, true},
        {"provability_MELL", "open_problem", 5, false},
        {"proof_net_normalization", "undecidable", 3, true},
        {"type_inhabitation_full_linear_logic", "undecidable", 5, true},
        {"proof_net_equality", "undecidable", 3, true},
        {"provability_noncommutative_linear_logic", "undecidable", 3, true},
        {"additive_excluded_middle", "not_provable", 3, true},
        {"provability_MALL_PSPACE_complete", "PSPACE_complete", 4, true},
        {"provability_MLL_NP_complete", "NP_complete", 3, true},
        {"cut_elimination_termination", "undecidable_for_full_linear_logic", 3, true},
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

/* ------------------------------------------------------------------ */
/*  Test 4: Verify bottom geometry and logical framework              */
/* ------------------------------------------------------------------ */
static void test_logical_framework(void) {
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "linear_resource_multiset") == 0,
                "bottom_geometry should be 'linear_resource_multiset'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "involutive_linear_negation") == 0,
                "negation_encoding should be 'involutive_linear_negation'");
    printf("  negation_encoding: %s\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == CONSTRUCTIVE,
                "contradiction_behavior should be CONSTRUCTIVE (blocking, no explosion)");
    printf("  contradiction_behavior: constructive (blocking)\n");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 5: Content hash computation                                  */
/* ------------------------------------------------------------------ */
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

/* ------------------------------------------------------------------ */
/*  Test 6: Round-trip save/load                                      */
/* ------------------------------------------------------------------ */
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
    TEST_ASSERT(strcmp(pkg2->negation_encoding, pkg1->negation_encoding) == 0,
                "negation_encoding should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg1->contradiction_behavior,
                "contradiction_behavior should match after round-trip");

    printf("  Round-trip: %d templates, %d unconstructibles\n", pkg2->template_count, pkg2->unconstructible_count);

    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

/* ------------------------------------------------------------------ */
/*  Test 7: Dependency validation                                     */
/* ------------------------------------------------------------------ */
static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Self-validation: all reduces_to and dependency references
     * should resolve within the same package */
    AxiomPackage *packages[] = {pkg};
    bool valid = axiom_package_validate_dependencies(pkg, packages, 1);
    TEST_ASSERT(valid, "self-dependency validation should pass");

    /* Verify external_ref URLs are valid */
    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "external_ref should not be empty");
        printf("  [%d] %s ref: %s\n", i, uc->name, uc->external_ref);
    }

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 8: Negative lookups                                          */
/* ------------------------------------------------------------------ */
static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups (non-existent entries)...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *t = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(t == NULL, "nonexistent template should return NULL");

    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "nonexistent unconstructible should return NULL");

    printf("  Negative lookups correctly return NULL\n");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 9: Key structural properties of linear logic                 */
/* ------------------------------------------------------------------ */
static void test_linear_logic_properties(void) {
    printf("Test 9: Key structural properties of linear logic...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Property 1: Linear logic has both multiplicative and additive
     * connectives (unlike classical logic which conflates them) */
    ConstraintTemplate *tensor = axiom_package_get_template(pkg, "tensor_right");
    ConstraintTemplate *with = axiom_package_get_template(pkg, "with_right");
    ConstraintTemplate *par = axiom_package_get_template(pkg, "par_right");
    ConstraintTemplate *plus = axiom_package_get_template(pkg, "plus_right");
    TEST_ASSERT(tensor != NULL && with != NULL && par != NULL && plus != NULL,
                "should have both multiplicative (tensor, par) and additive (with, plus) connectives");

    /* Property 2: Exponentials (! and ?) provide controlled contraction/weakening */
    ConstraintTemplate *bang_w = axiom_package_get_template(pkg, "bang_weakening");
    ConstraintTemplate *bang_c = axiom_package_get_template(pkg, "bang_contraction");
    ConstraintTemplate *quest_w = axiom_package_get_template(pkg, "quest_weakening");
    ConstraintTemplate *quest_c = axiom_package_get_template(pkg, "quest_contraction");
    TEST_ASSERT(bang_w != NULL && bang_c != NULL && quest_w != NULL && quest_c != NULL,
                "should have exponential rules for both ! and ?");

    /* Property 3: De Morgan dualities are present (involutive negation) */
    ConstraintTemplate *dm_tp = axiom_package_get_template(pkg, "demorgan_tensor_par");
    ConstraintTemplate *dm_wp = axiom_package_get_template(pkg, "demorgan_with_plus");
    ConstraintTemplate *dm_bq = axiom_package_get_template(pkg, "demorgan_bang_quest");
    TEST_ASSERT(dm_tp != NULL && dm_wp != NULL && dm_bq != NULL, "should have De Morgan duality templates");

    /* Property 4: No explosion principle (constructive contradiction behavior) */
    TEST_ASSERT(pkg->contradiction_behavior == CONSTRUCTIVE, "linear logic should NOT have explosion principle");

    /* Property 5: Negation is involutive (double negation = identity) */
    ConstraintTemplate *dn = axiom_package_get_template(pkg, "double_negation_involution");
    TEST_ASSERT(dn != NULL, "should have double negation involution");

    /* Property 6: Linear implication is definable from negation + par */
    ConstraintTemplate *limpl_l = axiom_package_get_template(pkg, "linear_implication_left");
    ConstraintTemplate *limpl_r = axiom_package_get_template(pkg, "linear_implication_right");
    TEST_ASSERT(limpl_l != NULL && limpl_r != NULL, "should have linear implication templates");

    /* Property 7: Focused proof system (Andreoli 1992) */
    ConstraintTemplate *async = axiom_package_get_template(pkg, "asynchronous_phase");
    ConstraintTemplate *sync = axiom_package_get_template(pkg, "synchronous_phase");
    ConstraintTemplate *focus = axiom_package_get_template(pkg, "focus_decision");
    TEST_ASSERT(async != NULL && sync != NULL && focus != NULL, "should have focused proof system templates");

    printf("  All structural properties verified\n");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 10: MELL open problem verification                           */
/* ------------------------------------------------------------------ */
static void test_mell_open_problem(void) {
    printf("Test 10: MELL decidability open problem...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* MELL provability is a famous open problem in linear logic.
     * It should be marked as NOT green_verified. */
    KnownUnconstructible *mell = axiom_package_lookup_unconstructible(pkg, "provability_MELL");
    TEST_ASSERT(mell != NULL, "MELL problem should exist");
    TEST_ASSERT(mell->green_verified == false, "MELL decidability should be marked as unverified (open problem)");
    TEST_ASSERT(mell->reduces_to != NULL && strcmp(mell->reduces_to, "open_problem") == 0,
                "MELL should reduce to 'open_problem'");

    printf("  MELL decidability correctly marked as open problem (green_verified=false)\n");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */
int main(void) {
    printf("========================================\n");
    printf("  Linear Logic Axiom Package Tests\n");
    printf("========================================\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructible_problems();
    test_logical_framework();
    test_content_hash();
    test_round_trip();
    test_dependency_validation();
    test_negative_lookups();
    test_linear_logic_properties();
    test_mell_open_problem();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", g_pass_count, g_fail_count);
    printf("========================================\n");

    return g_fail_count > 0 ? 1 : 0;
}
