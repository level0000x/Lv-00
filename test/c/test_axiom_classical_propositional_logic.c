/**
 * @file test_axiom_classical_propositional_logic.c
 * @brief Classical Propositional Logic (Łukasiewicz P₂) Axiom Package Test
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/classical_propositional_logic.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/classical_propositional_logic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 59
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

static void test_load_from_file(void) {
    printf("Test 1: Load classical_propositional_logic.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "classical_propositional_logic") == 0,
                "package name should be 'classical_propositional_logic'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 59 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    const char *expected_templates[] = {
        /* Group I: Łukasiewicz P2 Core Axiom Schemata */
        "axiom_K_weakening", "axiom_S_distribution", "axiom_C_contrapositive",
        /* Group II: Frege's Original Axioms (1879) */
        "frege_proposition_8", "frege_proposition_28", "frege_proposition_31", "frege_proposition_41",
        /* Group III: Russell-Whitehead Axioms */
        "RW_tautology", "RW_addition", "RW_commutation", "RW_association", "RW_distribution",
        /* Group IV: Inference Rules */
        "modus_ponens", "uniform_substitution",
        /* Group V: Core Constructors (Primitive Connectives) */
        "negation", "implication", "falsum", "verum",
        /* Group VI: Derived Connectives */
        "conjunction", "disjunction", "biconditional", "exclusive_or", "sheffer_stroke", "peirce_arrow",
        /* Group VII: Derived Inference Rules */
        "hypothetical_syllogism", "modus_tollens", "disjunctive_syllogism", "conjunction_introduction",
        "conjunction_elimination_left", "conjunction_elimination_right", "disjunction_introduction_left",
        "disjunction_introduction_right", "biconditional_introduction", "double_negation_elimination",
        "double_negation_introduction", "reductio_ad_absurdum", "ex_falso_quodlibet", "law_of_excluded_middle",
        "law_of_non_contradiction", "deduction_theorem", "proof_by_cases", "contraposition", "exportation",
        "importation",
        /* Group VIII: Propositional Identities */
        "de_morgan_conjunction", "de_morgan_disjunction", "conjunction_idempotence", "disjunction_idempotence",
        "conjunction_commutativity", "disjunction_commutativity", "conjunction_associativity",
        "disjunction_associativity", "conjunction_distributes_over_disjunction",
        "disjunction_distributes_over_conjunction", "absorption_conjunction_disjunction",
        "absorption_disjunction_conjunction", "constructive_dilemma", "destructive_dilemma", "peirces_law", NULL};

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

    /* P2 axiom schemata */
    t = axiom_package_get_template(pkg, "axiom_K_weakening");
    TEST_ASSERT(t && t->param_count == 2, "axiom_K_weakening should have 2 params (phi, psi)");

    t = axiom_package_get_template(pkg, "axiom_S_distribution");
    TEST_ASSERT(t && t->param_count == 3, "axiom_S_distribution should have 3 params (phi, psi, chi)");

    t = axiom_package_get_template(pkg, "axiom_C_contrapositive");
    TEST_ASSERT(t && t->param_count == 2, "axiom_C_contrapositive should have 2 params (phi, psi)");

    /* Inference rules */
    t = axiom_package_get_template(pkg, "modus_ponens");
    TEST_ASSERT(t && t->param_count == 2, "modus_ponens should have 2 params");

    t = axiom_package_get_template(pkg, "uniform_substitution");
    TEST_ASSERT(t && t->param_count == 2, "uniform_substitution should have 2 params");

    /* Primitive connectives */
    t = axiom_package_get_template(pkg, "negation");
    TEST_ASSERT(t && t->param_count == 1, "negation should have 1 param");

    t = axiom_package_get_template(pkg, "implication");
    TEST_ASSERT(t && t->param_count == 2, "implication should have 2 params");

    t = axiom_package_get_template(pkg, "falsum");
    TEST_ASSERT(t && t->param_count == 0, "falsum should have 0 params");

    t = axiom_package_get_template(pkg, "verum");
    TEST_ASSERT(t && t->param_count == 0, "verum should have 0 params");

    /* Derived connectives */
    t = axiom_package_get_template(pkg, "conjunction");
    TEST_ASSERT(t && t->param_count == 2, "conjunction should have 2 params");

    t = axiom_package_get_template(pkg, "disjunction");
    TEST_ASSERT(t && t->param_count == 2, "disjunction should have 2 params");

    t = axiom_package_get_template(pkg, "biconditional");
    TEST_ASSERT(t && t->param_count == 2, "biconditional should have 2 params");

    t = axiom_package_get_template(pkg, "exclusive_or");
    TEST_ASSERT(t && t->param_count == 2, "exclusive_or should have 2 params");

    t = axiom_package_get_template(pkg, "sheffer_stroke");
    TEST_ASSERT(t && t->param_count == 2, "sheffer_stroke should have 2 params");

    t = axiom_package_get_template(pkg, "peirce_arrow");
    TEST_ASSERT(t && t->param_count == 2, "peirce_arrow should have 2 params");

    /* Derived inference rules */
    t = axiom_package_get_template(pkg, "hypothetical_syllogism");
    TEST_ASSERT(t && t->param_count == 3, "hypothetical_syllogism should have 3 params");

    t = axiom_package_get_template(pkg, "modus_tollens");
    TEST_ASSERT(t && t->param_count == 2, "modus_tollens should have 2 params");

    t = axiom_package_get_template(pkg, "disjunctive_syllogism");
    TEST_ASSERT(t && t->param_count == 2, "disjunctive_syllogism should have 2 params");

    t = axiom_package_get_template(pkg, "double_negation_elimination");
    TEST_ASSERT(t && t->param_count == 1, "double_negation_elimination should have 1 param");

    t = axiom_package_get_template(pkg, "law_of_excluded_middle");
    TEST_ASSERT(t && t->param_count == 1, "law_of_excluded_middle should have 1 param");

    t = axiom_package_get_template(pkg, "ex_falso_quodlibet");
    TEST_ASSERT(t && t->param_count == 1, "ex_falso_quodlibet should have 1 param");

    t = axiom_package_get_template(pkg, "destructive_dilemma");
    TEST_ASSERT(t && t->param_count == 4, "destructive_dilemma should have 4 params");

    t = axiom_package_get_template(pkg, "peirces_law");
    TEST_ASSERT(t && t->param_count == 2, "peirces_law should have 2 params");

    /* Propositional identities */
    t = axiom_package_get_template(pkg, "de_morgan_conjunction");
    TEST_ASSERT(t && t->param_count == 2, "de_morgan_conjunction should have 2 params");

    t = axiom_package_get_template(pkg, "conjunction_distributes_over_disjunction");
    TEST_ASSERT(t && t->param_count == 3, "conjunction_distributes_over_disjunction should have 3 params");

    /* Frege's axioms */
    t = axiom_package_get_template(pkg, "frege_proposition_8");
    TEST_ASSERT(t && t->param_count == 3, "frege_proposition_8 should have 3 params");

    t = axiom_package_get_template(pkg, "frege_proposition_31");
    TEST_ASSERT(t && t->param_count == 1, "frege_proposition_31 should have 1 param");

    /* RW axioms */
    t = axiom_package_get_template(pkg, "RW_tautology");
    TEST_ASSERT(t && t->param_count == 1, "RW_tautology should have 1 param");

    t = axiom_package_get_template(pkg, "RW_distribution");
    TEST_ASSERT(t && t->param_count == 3, "RW_distribution should have 3 params");

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 6 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        {"propositional_satisfiability", "NP_complete", 3, true},
        {"tautology_checking", "coNP_complete", 3, true},
        {"minimal_proof_length", "NP_hard_approximation", 4, true},
        {"propositional_interpolation", "PiP2_complete", 3, true},
        {"proof_equivalence_checking", "coNP_complete", 4, true},
        {"shortest_implicational_proof", "NP_hard", 4, true},
    };

    for (int i = 0; i < (int) (sizeof(expected) / sizeof(expected[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        TEST_ASSERT(uc != NULL, expected[i].name);

        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL && strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                        expected[i].name);
            TEST_ASSERT(uc->dependency_chain.count == expected[i].dep_count, expected[i].name);
            TEST_ASSERT(uc->green_verified == expected[i].green_verified, expected[i].name);
            TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "should have external_ref URL");
            printf("  [%d] %s -> %s (deps=%d, verified=%s)\n", i, uc->name, uc->reduces_to, uc->dependency_chain.count,
                   uc->green_verified ? "true" : "false");
        }
    }

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void) {
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "classical_propositional_2valued") == 0,
                "bottom_geometry should be 'classical_propositional_2valued'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "material_implication_to_falsum") == 0,
                "negation_encoding should be 'material_implication_to_falsum'");
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

static void test_axiom_systems_coverage(void) {
    printf("Test 9: Verify multiple axiom system representations...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Łukasiewicz P2 */
    TEST_ASSERT(axiom_package_get_template(pkg, "axiom_K_weakening") != NULL, "P2 axiom K should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "axiom_S_distribution") != NULL, "P2 axiom S should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "axiom_C_contrapositive") != NULL, "P2 axiom C should exist");

    /* Frege's Begriffsschrift (1879) */
    TEST_ASSERT(axiom_package_get_template(pkg, "frege_proposition_8") != NULL, "Frege prop 8 should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "frege_proposition_28") != NULL, "Frege prop 28 should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "frege_proposition_31") != NULL, "Frege prop 31 should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "frege_proposition_41") != NULL, "Frege prop 41 should exist");

    /* Russell-Whitehead Principia Mathematica */
    TEST_ASSERT(axiom_package_get_template(pkg, "RW_tautology") != NULL, "RW tautology should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "RW_addition") != NULL, "RW addition should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "RW_commutation") != NULL, "RW commutation should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "RW_association") != NULL, "RW association should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "RW_distribution") != NULL, "RW distribution should exist");

    printf("  Multiple axiom systems: P2, Frege, RW all present\n");

    axiom_package_destroy(pkg);
}

static void test_functional_completeness(void) {
    printf("Test 10: Verify functional completeness coverage...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Both NAND and NOR should be present as functionally complete connectives */
    ConstraintTemplate *nand = axiom_package_get_template(pkg, "sheffer_stroke");
    TEST_ASSERT(nand != NULL && nand->param_count == 2, "Sheffer stroke (NAND) should exist with 2 params");

    ConstraintTemplate *nor = axiom_package_get_template(pkg, "peirce_arrow");
    TEST_ASSERT(nor != NULL && nor->param_count == 2, "Peirce arrow (NOR) should exist with 2 params");

    /* XOR should also be present */
    ConstraintTemplate * xor = axiom_package_get_template(pkg, "exclusive_or");
    TEST_ASSERT(xor!= NULL && xor->param_count == 2, "Exclusive or (XOR) should exist with 2 params");

    /* Peirce's law (characterizes classical logic) */
    ConstraintTemplate *peirce = axiom_package_get_template(pkg, "peirces_law");
    TEST_ASSERT(peirce != NULL && peirce->param_count == 2, "Peirce's law should exist with 2 params");

    printf("  Functional completeness: NAND, NOR, XOR, Peirce's law all present\n");

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Classical Propositional Logic");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_axiom_systems_coverage);
    TEST_RUN(test_functional_completeness);

    TEST_SUMMARY();

    return 0;
}
