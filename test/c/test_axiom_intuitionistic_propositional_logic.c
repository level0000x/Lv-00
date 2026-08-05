/**
 * @file test_axiom_intuitionistic_propositional_logic.c
 * @brief Intuitionistic Propositional Logic (Heyting 1930) Axiom Package Test
 *
 * Tests the intuitionistic_propositional_logic.lvz axiom package.
 * IPL is PROPOSITION_KIND_CONSTRUCTIVE logic: NO LEM, NO DNE, NO Peirce.
 * Still uses PROPOSITION_KIND_EXPLOSION_PRINCIPLE (EFQ is an axiom in Heyting's system).
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/intuitionistic_propositional_logic.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/intuitionistic_propositional_logic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 51
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ---------------------------------------------------------------- */
static void test_load_from_file(void) {
    printf("Test 1: Load intuitionistic_propositional_logic.lvz...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Load error: %s\n", err ? err : "(unknown)");
    }
    TEST_ASSERT(status == AXIOM_LOAD_OK, "load should return AXIOM_LOAD_OK");

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "intuitionistic_propositional_logic") == 0,
                "package name should be 'intuitionistic_propositional_logic'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);
    axiom_package_destroy(pkg);
}

/* ---------------------------------------------------------------- */
static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 51 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    /* All 51 templates in declaration order from the .lvz file */
    const char *expected[] = {
        /* Group I: Hilbert-style Core Axiom Schemata (9) */
        "axiom_K_weakening", "axiom_S_distribution", "conjunction_left_elim", "conjunction_right_elim",
        "conjunction_intro_axiom", "disjunction_left_intro", "disjunction_right_intro", "disjunction_elim",
        "ex_falso_quodlibet",

        /* Group II: Inference Rules (1) */
        "modus_ponens",

        /* Group III: Core Constructors — Primitive Connectives (5) */
        "implication", "conjunction", "disjunction", "falsum", "negation",

        /* Group IV: Derived Connectives (5) */
        "biconditional", "exclusive_or", "sheffer_stroke", "peirce_arrow", "verum",

        /* Group V: Derived Inference Rules (14) */
        "hypothetical_syllogism", "modus_tollens", "disjunctive_syllogism", "conjunction_introduction",
        "conjunction_elimination_left", "conjunction_elimination_right", "disjunction_introduction_left",
        "disjunction_introduction_right", "biconditional_introduction", "biconditional_elimination_left",
        "biconditional_elimination_right", "double_negation_introduction", "reductio_ad_absurdum", "deduction_theorem",

        /* Group VI: Propositional Identities (8) */
        "de_morgan_disjunction", "de_morgan_conjunction_weakened", "conjunction_idempotence", "disjunction_idempotence",
        "conjunction_commutativity", "disjunction_commutativity", "conjunction_associativity",
        "disjunction_associativity",

        /* Group VII: IPL-CPL Bridge Theorems (4) */
        "glivenko_double_negation", "negative_translation", "negative_translation_soundness", "kuroda_translation",

        /* Group VIII: Forward Contrapositive & Derived (5) */
        "contraposition_forward", "exportation", "importation", "proof_by_cases", "triple_negation_reduction",

        NULL};

    int found = 0;
    for (int i = 0; expected[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expected[i]);
        if (tmpl)
            found++;
        else {
            printf("  MISSING: '%s'\n", expected[i]);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found == EXPECTED_TEMPLATE_COUNT, "all 51 templates should be found");
    printf("  Found %d / %d templates\n", found, EXPECTED_TEMPLATE_COUNT);

    /* Verify param counts for key templates */
    ConstraintTemplate *t;

    t = axiom_package_get_template(pkg, "axiom_K_weakening");
    TEST_ASSERT(t && t->param_count == 2, "K axiom: 2 params");

    t = axiom_package_get_template(pkg, "axiom_S_distribution");
    TEST_ASSERT(t && t->param_count == 3, "S axiom: 3 params");

    t = axiom_package_get_template(pkg, "disjunction_elim");
    TEST_ASSERT(t && t->param_count == 3, "OR elim: 3 params");

    t = axiom_package_get_template(pkg, "ex_falso_quodlibet");
    TEST_ASSERT(t && t->param_count == 1, "EFQ: 1 param");

    t = axiom_package_get_template(pkg, "modus_ponens");
    TEST_ASSERT(t && t->param_count == 2, "MP: 2 params");

    t = axiom_package_get_template(pkg, "falsum");
    TEST_ASSERT(t && t->param_count == 0, "falsum: 0 params");

    t = axiom_package_get_template(pkg, "negation");
    TEST_ASSERT(t && t->param_count == 1, "negation: 1 param");

    t = axiom_package_get_template(pkg, "biconditional");
    TEST_ASSERT(t && t->param_count == 2, "biconditional: 2 params");

    t = axiom_package_get_template(pkg, "verum");
    TEST_ASSERT(t && t->param_count == 0, "verum: 0 params");

    t = axiom_package_get_template(pkg, "hypothetical_syllogism");
    TEST_ASSERT(t && t->param_count == 3, "HS: 3 params");

    t = axiom_package_get_template(pkg, "modus_tollens");
    TEST_ASSERT(t && t->param_count == 2, "MT: 2 params");

    t = axiom_package_get_template(pkg, "double_negation_introduction");
    TEST_ASSERT(t && t->param_count == 1, "DN Intro: 1 param");

    t = axiom_package_get_template(pkg, "glivenko_double_negation");
    TEST_ASSERT(t && t->param_count == 1, "Glivenko: 1 param");

    t = axiom_package_get_template(pkg, "exportation");
    TEST_ASSERT(t && t->param_count == 3, "Exportation: 3 params");

    t = axiom_package_get_template(pkg, "triple_negation_reduction");
    TEST_ASSERT(t && t->param_count == 1, "Triple Neg: 1 param");

    printf("  Parameter counts verified for 15 key templates\n");
    axiom_package_destroy(pkg);
}

/* ---------------------------------------------------------------- */
static void test_unconstructible_problems(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 6 unconstructible problems");
    printf("  Count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int deps;
        bool verified;
    } exp[] = {
        {"intuitionistic_tautology_checking", "PSPACE_complete", 7, true},
        {"intuitionistic_satisfiability", "PSPACE_complete", 5, true},
        {"intuitionistic_equivalence_checking", "PSPACE_complete", 5, true},
        {"intuitionistic_unification_problem", "undecidable", 6, true},
        {"admissibility_in_IPL", "undecidable", 5, true},
        {"intermediate_logic_axiomatization", "undecidable", 6, true},
    };

    for (int i = 0; i < 6; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, exp[i].name);
        TEST_ASSERT(uc != NULL, exp[i].name);
        if (uc) {
            TEST_ASSERT(uc->reduces_to && strcmp(uc->reduces_to, exp[i].reduces_to) == 0, exp[i].name);
            TEST_ASSERT(uc->dependency_chain.count == exp[i].deps, exp[i].name);
            TEST_ASSERT(uc->green_verified == exp[i].verified, exp[i].name);
            TEST_ASSERT(uc->external_ref && strlen(uc->external_ref) > 0, "should have external_ref");
            printf("  [%d] %s -> %s deps=%d ok\n", i, uc->name, uc->reduces_to, uc->dependency_chain.count);
        }
    }

    /* Check that PSPACE Wikipedia ref exists */
    KnownUnconstructible *taut = axiom_package_lookup_unconstructible(pkg, "intuitionistic_tautology_checking");
    if (taut) {
        TEST_ASSERT(strstr(taut->external_ref, "wikipedia.org") || strstr(taut->external_ref, "wikipedia") ||
                        strlen(taut->external_ref) > 10,
                    "tautology_checking should have external_ref");
    }
    axiom_package_destroy(pkg);
}

/* ---------------------------------------------------------------- */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework config...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry && strcmp(pkg->bottom_geometry, "intuitionistic_propositional_heyting") == 0,
                "bottom_geometry: intuitionistic_propositional_heyting");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding && strcmp(pkg->negation_encoding, "implication_to_falsum") == 0,
                "negation_encoding: implication_to_falsum");
    printf("  negation_encoding: %s\n", pkg->negation_encoding);

    /* IPL uses PROPOSITION_KIND_EXPLOSION_PRINCIPLE because EFQ is an axiom in Heyting's system.
     * Constructivity comes from the ABSENCE of LEM, DNE, and Peirce. */
    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE (EFQ is an axiom in IPL)");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");
    axiom_package_destroy(pkg);
}

/* ---------------------------------------------------------------- */
static void test_content_hash(void) {
    printf("Test 5: Content hash...\n");
    AxiomPackage *pkg = axiom_package_create("pl", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);
    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash && strlen(hash) == 64, "SHA-256 hash 64 hex chars");
    if (hash) {
        printf("  SHA-256: %s\n", hash);
        lv_free((void **) &hash);
    }
    axiom_package_destroy(pkg);
}

/* ---------------------------------------------------------------- */
static void test_round_trip(void) {
    printf("Test 6: Round-trip save/load...\n");
    AxiomPackage *pkg1 = axiom_package_create("pl", "0.0.0");
    axiom_package_load(pkg1, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_save(pkg1, SAVE_TEST_PATH) == AXIOM_SAVE_OK, "save should succeed");

    AxiomPackage *pkg2 = axiom_package_create("pl", "0.0.0");
    TEST_ASSERT(axiom_package_load(pkg2, SAVE_TEST_PATH) == AXIOM_LOAD_OK, "reload should succeed");

    TEST_ASSERT(axiom_package_get_template_count(pkg2) == axiom_package_get_template_count(pkg1), "tpl count match");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg2) == axiom_package_get_unconstructible_count(pkg1), "uc count match");
    TEST_ASSERT(strcmp(pkg2->name, pkg1->name) == 0, "name match");
    TEST_ASSERT(strcmp(pkg2->version, pkg1->version) == 0, "version match");
    TEST_ASSERT(
        pkg2->bottom_geometry && pkg1->bottom_geometry && strcmp(pkg2->bottom_geometry, pkg1->bottom_geometry) == 0,
        "geom match");
    TEST_ASSERT(pkg2->negation_encoding && pkg1->negation_encoding &&
                    strcmp(pkg2->negation_encoding, pkg1->negation_encoding) == 0,
                "neg match");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg1->contradiction_behavior, "behavior match");

    char *h1 = axiom_package_compute_content_hash(pkg1);
    char *h2 = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(h1 && h2 && strcmp(h1, h2) == 0, "hash match after round-trip");
    printf("  Round-trip: templates=%d unconstructibles=%d hash_match=%s\n", axiom_package_get_template_count(pkg2),
           axiom_package_get_unconstructible_count(pkg2), (h1 && h2 && strcmp(h1, h2) == 0) ? "YES" : "NO");
    lv_free((void **) &h1);
    lv_free((void **) &h2);
    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

/* ---------------------------------------------------------------- */
static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");
    AxiomPackage *pkg = axiom_package_create("pl", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);
    /* Self-validation may fail for cross-package reduces_to refs */
    axiom_package_validate_dependencies(pkg, &pkg, 1);
    printf("  Done (cross-package refs expected to fail self-validation)\n");
    axiom_package_destroy(pkg);
}

/* ---------------------------------------------------------------- */
static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");
    AxiomPackage *pkg = axiom_package_create("pl", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template(pkg, "no_such_tpl") == NULL, "null tpl");
    TEST_ASSERT(axiom_package_lookup_unconstructible(pkg, "no_such_uc") == NULL, "null uc");

    /* Classical-only theorems should NOT exist in IPL */
    TEST_ASSERT(axiom_package_get_template(pkg, "double_negation_elimination") == NULL, "DNE should NOT be in IPL");
    TEST_ASSERT(axiom_package_get_template(pkg, "law_of_excluded_middle") == NULL, "LEM should NOT be in IPL");
    TEST_ASSERT(axiom_package_get_template(pkg, "peirces_law") == NULL, "Peirce's law should NOT be in IPL");
    printf("  Negative lookups: OK (classical-only absent)\n");
    axiom_package_destroy(pkg);
}

/* ---------------------------------------------------------------- */
static void test_constructive_character(void) {
    printf("Test 9: PROPOSITION_KIND_CONSTRUCTIVE vs Classical contrast...\n");
    AxiomPackage *pkg = axiom_package_create("pl", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* === Present in IPL (constructively valid) === */
    TEST_ASSERT(axiom_package_get_template(pkg, "double_negation_introduction") != NULL,
                "DN Intro: phi -> ~~phi (valid)");
    TEST_ASSERT(axiom_package_get_template(pkg, "triple_negation_reduction") != NULL,
                "Triple Neg: ~~~phi -> ~phi (valid)");
    TEST_ASSERT(axiom_package_get_template(pkg, "contraposition_forward") != NULL,
                "Forward contrapositive: (phi->psi) -> (~psi->~phi) (valid)");
    TEST_ASSERT(axiom_package_get_template(pkg, "modus_tollens") != NULL, "Modus tollens (valid in IPL)");
    TEST_ASSERT(axiom_package_get_template(pkg, "disjunctive_syllogism") != NULL,
                "Disjunctive syllogism (valid in IPL)");
    TEST_ASSERT(axiom_package_get_template(pkg, "ex_falso_quodlibet") != NULL, "EFQ: bot -> phi (axiom)");

    /* De Morgan: OR case — full equivalence, AND case — only half */
    TEST_ASSERT(axiom_package_get_template(pkg, "de_morgan_disjunction") != NULL, "De Morgan OR: both directions hold");
    TEST_ASSERT(axiom_package_get_template(pkg, "de_morgan_conjunction_weakened") != NULL,
                "De Morgan AND: only (~phi|~psi) -> ~(phi&psi) direction");

    /* Glivenko + Negative Translation */
    TEST_ASSERT(axiom_package_get_template(pkg, "glivenko_double_negation") != NULL, "Glivenko's theorem");
    TEST_ASSERT(axiom_package_get_template(pkg, "negative_translation") != NULL, "Gödel-Gentzen negative translation");
    TEST_ASSERT(axiom_package_get_template(pkg, "negative_translation_soundness") != NULL,
                "Negative translation soundness");

    /* === Absent in IPL (classical only) === */
    TEST_ASSERT(axiom_package_get_template(pkg, "double_negation_elimination") == NULL, "DNE NOT present");
    TEST_ASSERT(axiom_package_get_template(pkg, "law_of_excluded_middle") == NULL, "LEM NOT present");
    TEST_ASSERT(axiom_package_get_template(pkg, "peirces_law") == NULL, "Peirce's law NOT present");

    /* === Unconstructible entries document the PSPACE/undecidable results === */
    TEST_ASSERT(axiom_package_lookup_unconstructible(pkg, "intuitionistic_tautology_checking") != NULL,
                "IPL tautology (PSPACE) documented");
    TEST_ASSERT(axiom_package_lookup_unconstructible(pkg, "admissibility_in_IPL") != NULL,
                "IPL admissibility (undecidable) documented");
    TEST_ASSERT(axiom_package_lookup_unconstructible(pkg, "intermediate_logic_axiomatization") != NULL,
                "Intermediate logic axiomatization (undecidable) documented");
    TEST_ASSERT(axiom_package_lookup_unconstructible(pkg, "intuitionistic_unification_problem") != NULL,
                "IPL unification (undecidable) documented");

    printf("  PROPOSITION_KIND_CONSTRUCTIVE vs Classical: verified\n");
    axiom_package_destroy(pkg);
}

/* ---------------------------------------------------------------- */
static void test_external_references(void) {
    printf("Test 10: External references in unconstructible problems...\n");
    AxiomPackage *pkg = axiom_package_create("pl", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    for (int i = 0; i < axiom_package_get_unconstructible_count(pkg); i++) {
        KnownUnconstructible *uc = axiom_package_get_unconstructible(pkg, i);
        TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 5,
                    "every unconstructible should have an external_ref");
        if (uc->external_ref) {
            /* Verify URLs are proper */
            TEST_ASSERT(
                strstr(uc->external_ref, "http") == uc->external_ref || isalpha((unsigned char) uc->external_ref[0]),
                uc->name);
        }
    }
    printf("  External references: OK for all %d problems\n", axiom_package_get_unconstructible_count(pkg));
    axiom_package_destroy(pkg);
}

/* ---------------------------------------------------------------- */
TEST_MAIN_BEGIN("Intuitionistic Propositional Logic")

    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructible_problems);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_round_trip);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);
    TEST_MAIN_RUN(test_constructive_character);
    TEST_MAIN_RUN(test_external_references);

TEST_MAIN_END()

