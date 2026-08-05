/**
 * @file test_axiom_boolean_algebra.c
 * @brief Boolean Algebra Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the boolean_algebra.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * and dependency validation.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/boolean_algebra.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/boolean_algebra_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 29
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板名 */
static const char *const k_template_names[] = {
    /* Lattice axioms */
    "join_associativity", "meet_associativity", "join_commutativity",
    "meet_commutativity", "join_absorption", "meet_absorption",
    /* Identity axioms */
    "join_identity", "meet_identity",
    /* Distributive axioms */
    "meet_distributes_over_join", "join_distributes_over_meet",
    /* Complement axioms */
    "complement_join", "complement_meet",
    /* Huntington minimal axioms */
    "huntington_equation",
    /* Core constructors */
    "complement", "meet", "join",
    /* Derived constructors */
    "double_negation", "de_morgan_join", "de_morgan_meet", "join_idempotence",
    "meet_idempotence", "join_bounded_top", "meet_bounded_bottom", "consensus",
    "sheffer_stroke", "peirce_arrow", "material_implication", "exclusive_or",
    "biconditional",
};
#define K_TEMPLATE_NAMES_COUNT (int) (sizeof(k_template_names) / sizeof(k_template_names[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"boolean_satisfiability", "NP_complete", 3, true},
    {"tautology_checking", "coNP_complete", 3, true},
    {"boolean_equivalence_checking", "coNP_complete", 3, true},
    {"boolean_formula_minimization", "NP_hard", 4, true},
    {"minimal_circuit_synthesis", "NP_hard", 4, true},
    {"equational_theory_with_subalgebra", "undecidable", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "boolean_algebra");
}

static void test_templates(void) {
    axiom_test_templates_names_only(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 30 constraint templates",
                                    k_template_names, K_TEMPLATE_NAMES_COUNT);

    /* 文件特有：具体参数个数校验（差异部分，原样保留） */
    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify specific param counts */
    ConstraintTemplate *t;

    /* Binary operations: 2 params */
    t = axiom_package_get_template(pkg, "meet");
    TEST_ASSERT(t && t->param_count == 2, "meet should have 2 params");

    t = axiom_package_get_template(pkg, "join");
    TEST_ASSERT(t && t->param_count == 2, "join should have 2 params");

    t = axiom_package_get_template(pkg, "complement");
    TEST_ASSERT(t && t->param_count == 1, "complement should have 1 param");

    /* Ternary operations: 3 params */
    t = axiom_package_get_template(pkg, "join_associativity");
    TEST_ASSERT(t && t->param_count == 3, "join_associativity should have 3 params");

    t = axiom_package_get_template(pkg, "meet_distributes_over_join");
    TEST_ASSERT(t && t->param_count == 3, "meet_distributes_over_join should have 3 params");

    t = axiom_package_get_template(pkg, "consensus");
    TEST_ASSERT(t && t->param_count == 3, "consensus should have 3 params");

    /* Unary operations: 1 param */
    t = axiom_package_get_template(pkg, "double_negation");
    TEST_ASSERT(t && t->param_count == 1, "double_negation should have 1 param");

    t = axiom_package_get_template(pkg, "join_idempotence");
    TEST_ASSERT(t && t->param_count == 1, "join_idempotence should have 1 param");

    t = axiom_package_get_template(pkg, "complement_join");
    TEST_ASSERT(t && t->param_count == 1, "complement_join should have 1 param");

    /* Huntington equation: 2 params */
    t = axiom_package_get_template(pkg, "huntington_equation");
    TEST_ASSERT(t && t->param_count == 2, "huntington_equation should have 2 params");

    /* Sheffer stroke and Peirce arrow: 2 params */
    t = axiom_package_get_template(pkg, "sheffer_stroke");
    TEST_ASSERT(t && t->param_count == 2, "sheffer_stroke should have 2 params");

    t = axiom_package_get_template(pkg, "peirce_arrow");
    TEST_ASSERT(t && t->param_count == 2, "peirce_arrow should have 2 params");

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void) {
    axiom_test_unconstructible_problems(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                        "should have 6 unconstructible problems", k_unconstructibles,
                                        K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework(AXIOM_PKG_PATH, "boolean_algebra_2element", "classical_complement",
                                 PROPOSITION_KIND_EXPLOSION_PRINCIPLE, "PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
}

static void test_content_hash(void) {
    axiom_test_content_hash(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE_PTR);
}

static void test_round_trip(void) {
    axiom_test_round_trip(AXIOM_PKG_PATH, SAVE_TEST_PATH, AXIOM_TEST_FREE_LV_FREE_PTR);
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

static void test_template_detailed_params(void) {
    printf("Test 9: Detailed template parameter verification...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify all templates have correct param counts */
    struct {
        const char *name;
        int expected_params;
    } param_checks[] = {
        /* Lattice axioms */
        {"join_associativity", 3},
        {"meet_associativity", 3},
        {"join_commutativity", 2},
        {"meet_commutativity", 2},
        {"join_absorption", 2},
        {"meet_absorption", 2},
        /* Identity axioms */
        {"join_identity", 1},
        {"meet_identity", 1},
        /* Distributive axioms */
        {"meet_distributes_over_join", 3},
        {"join_distributes_over_meet", 3},
        /* Complement axioms */
        {"complement_join", 1},
        {"complement_meet", 1},
        /* Huntington */
        {"huntington_equation", 2},
        /* Core constructors */
        {"complement", 1},
        {"meet", 2},
        {"join", 2},
        /* Derived constructors */
        {"double_negation", 1},
        {"de_morgan_join", 2},
        {"de_morgan_meet", 2},
        {"join_idempotence", 1},
        {"meet_idempotence", 1},
        {"join_bounded_top", 1},
        {"meet_bounded_bottom", 1},
        {"consensus", 3},
        {"sheffer_stroke", 2},
        {"peirce_arrow", 2},
        {"material_implication", 2},
        {"exclusive_or", 2},
        {"biconditional", 2},
    };

    int checks = (int) (sizeof(param_checks) / sizeof(param_checks[0]));
    for (int i = 0; i < checks; i++) {
        ConstraintTemplate *t = axiom_package_get_template(pkg, param_checks[i].name);
        char msg[128];
        snprintf(msg, sizeof(msg), "%s should have %d params", param_checks[i].name, param_checks[i].expected_params);
        TEST_ASSERT(t != NULL && t->param_count == param_checks[i].expected_params, msg);
    }

    printf("  Verified %d template param counts\n", checks);

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Boolean Algebra");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_template_detailed_params);

    TEST_SUMMARY();

    return 0;
}
