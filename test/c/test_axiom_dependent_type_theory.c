/**
 * @file test_axiom_dependent_type_theory.c
 * @brief Dependent Type Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the dependent_type_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Dependent type theory is formalized through 33 templates covering pi/sigma
 * types, identity types, natural numbers, computation rules, metatheoretic
 * properties, inductive families, and the Curry-Howard correspondence.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/dependent_type_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/dependent_type_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 33
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Type Formers (5) */
    {"pi_type", 2},
    {"sigma_type", 2},
    {"identity_type", 3},
    {"natural_number_type", 0},
    {"universe_type", 1},
    /* Group II: Introduction Rules (5) */
    {"lambda_abstraction_dependent", 2},
    {"pair_dependent", 2},
    {"refl", 1},
    {"zero", 0},
    {"successor", 1},
    /* Group III: Elimination Rules (5) */
    {"application_dependent", 2},
    {"projection_first", 1},
    {"projection_second", 1},
    {"induction_natural", 3},
    {"path_induction", 3},
    /* Group IV: Computation Rules (4) */
    {"beta_reduction_dependent", 2},
    {"eta_expansion_dependent", 1},
    {"computation_natural", 3},
    {"computation_identity", 2},
    /* Group V: Metatheoretic Properties (5) */
    {"canonicity", 1},
    {"normalization", 1},
    {"decidable_type_checking", 3},
    {"undecidable_type_inhabitation", 1},
    {"cumulativity", 2},
    /* Group VI: Advanced Features (5) */
    {"inductive_family", 3},
    {"pattern_matching", 3},
    {"universe_polymorphism", 1},
    {"coercions", 2},
    {"type_class", 2},
    /* Group VII: Curry-Howard (4) */
    {"propositions_as_types", 1},
    {"proof_relevance", 2},
    {"curry_howard_dependent", 2},
    {"constructive_existential", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"type_inhabitation_dependent", "undecidable", 4, true},
    {"type_equality_decidability", "undecidable", 4, true},
    {"normalization_order", "undecidable", 3, true},
    {"universe_consistency", "undecidable", 3, true},
    {"parametricity_verification", "undecidable", 4, true},
    {"termination_checking_dependent", "undecidable", 4, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "dependent_type_theory");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 33 constraint templates", k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructibles(void) {
    axiom_test_unconstructibles_min_deps(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                         "should have 6 unconstructible problems", k_unconstructibles,
                                         K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework_presence(AXIOM_PKG_PATH, PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                                          "PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
}

static void test_content_hash(void) {
    axiom_test_content_hash_deterministic(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_save_load_roundtrip(void) {
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "dependent_type_theory", EXPECTED_TEMPLATE_COUNT,
                                    EXPECTED_UNCONSTRUCTIBLE_COUNT, AXIOM_TEST_FREE_LV_FREE);
}

static void test_dependency_validation(void) {
    axiom_test_dependency_validation_note(AXIOM_PKG_PATH,
                                          "(identifier references to external concepts are expected)");
}

static void test_negative_lookups(void) {
    axiom_test_negative_lookups(AXIOM_PKG_PATH, AXIOM_TEST_NEG_XYZ);
}

static void test_external_references(void) {
    axiom_test_external_refs_all(AXIOM_PKG_PATH);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_key_templates(void) {
    printf("Test 10: Key dependent type theory templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core type formers */
    const char *type_formers[] = {"pi_type", "sigma_type", "identity_type", "natural_number_type", "universe_type"};

    for (int i = 0; i < 5; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, type_formers[i]);
        TEST_ASSERT(tmpl != NULL, "core type former template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Introduction and elimination rules */
    const char *intro_elim[] = {"lambda_abstraction_dependent",
                                "pair_dependent",
                                "refl",
                                "zero",
                                "successor",
                                "application_dependent",
                                "projection_first",
                                "projection_second",
                                "induction_natural",
                                "path_induction"};

    for (int i = 0; i < 10; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, intro_elim[i]);
        TEST_ASSERT(tmpl != NULL, "introduction/elimination rule template should exist");
    }

    /* Computation rules */
    const char *computation[] = {"beta_reduction_dependent", "eta_expansion_dependent", "computation_natural",
                                 "computation_identity"};

    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, computation[i]);
        TEST_ASSERT(tmpl != NULL, "computation rule template should exist");
    }

    /* Curry-Howard correspondence */
    ConstraintTemplate *pat = axiom_package_get_template(pkg, "propositions_as_types");
    TEST_ASSERT(pat != NULL, "propositions_as_types template should exist");
    ConstraintTemplate *chd = axiom_package_get_template(pkg, "curry_howard_dependent");
    TEST_ASSERT(chd != NULL, "curry_howard_dependent template should exist");
    ConstraintTemplate *ce = axiom_package_get_template(pkg, "constructive_existential");
    TEST_ASSERT(ce != NULL, "constructive_existential template should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Dependent Type Theory")

    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructibles);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_save_load_roundtrip);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);
    TEST_MAIN_RUN(test_external_references);
    TEST_MAIN_RUN(test_key_templates);

TEST_MAIN_END()

