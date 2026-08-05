/**
 * @file test_axiom_classical_propositional_logic.c
 * @brief Classical Propositional Logic (Łukasiewicz P₂) Axiom Package Test
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/classical_propositional_logic.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/classical_propositional_logic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 59
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板名 */
static const char *const k_template_names[] = {
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
    "absorption_disjunction_conjunction", "constructive_dilemma", "destructive_dilemma", "peirces_law",
};
#define K_TEMPLATE_NAMES_COUNT (int) (sizeof(k_template_names) / sizeof(k_template_names[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"propositional_satisfiability", "NP_complete", 3, true},
    {"tautology_checking", "coNP_complete", 3, true},
    {"minimal_proof_length", "NP_hard_approximation", 4, true},
    {"propositional_interpolation", "PiP2_complete", 3, true},
    {"proof_equivalence_checking", "coNP_complete", 4, true},
    {"shortest_implicational_proof", "NP_hard", 4, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "classical_propositional_logic");
}

static void test_templates(void) {
    axiom_test_templates_names_only(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 59 constraint templates",
                                    k_template_names, K_TEMPLATE_NAMES_COUNT);

    /* 文件特有：具体参数个数校验（差异部分，原样保留） */
    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

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
    axiom_test_unconstructible_problems(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                        "should have 6 unconstructible problems", k_unconstructibles,
                                        K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework(AXIOM_PKG_PATH, "classical_propositional_2valued", "material_implication_to_falsum",
                                 PROPOSITION_KIND_EXPLOSION_PRINCIPLE, "PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
}

static void test_content_hash(void) {
    axiom_test_content_hash(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_round_trip(void) {
    axiom_test_round_trip(AXIOM_PKG_PATH, SAVE_TEST_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_dependency_validation(void) {
    axiom_test_dependency_validation(AXIOM_PKG_PATH, "FAIL (acceptable)",
                                     " (expected: may fail for cross-reference reduces_to)");
}

static void test_negative_lookups(void) {
    axiom_test_negative_lookups(AXIOM_PKG_PATH, AXIOM_TEST_NEG_BASIC);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

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
    ConstraintTemplate *xor_t = axiom_package_get_template(pkg, "exclusive_or");
    TEST_ASSERT(xor_t != NULL && xor_t->param_count == 2, "Exclusive or (XOR) should exist with 2 params");

    /* Peirce's law (characterizes classical logic) */
    ConstraintTemplate *peirce = axiom_package_get_template(pkg, "peirces_law");
    TEST_ASSERT(peirce != NULL && peirce->param_count == 2, "Peirce's law should exist with 2 params");

    printf("  Functional completeness: NAND, NOR, XOR, Peirce's law all present\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Classical Propositional Logic")

    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructible_problems);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_round_trip);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);
    TEST_MAIN_RUN(test_axiom_systems_coverage);
    TEST_MAIN_RUN(test_functional_completeness);

TEST_MAIN_END()

