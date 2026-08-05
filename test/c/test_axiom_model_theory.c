/**
 * @file test_axiom_model_theory.c
 * @brief Model Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the model_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Model theory is formalized through 35 templates covering first-order
 * logic, completeness and compactness, Lowenheim-Skolem theorems,
 * ultraproducts, stability theory, and decidability.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/model_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/model_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 35
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Basic Concepts (6) */
    {"first_order_language", 1},
    {"structure_model", 2},
    {"satisfaction_relation", 3},
    {"theory", 2},
    {"theory_consistency", 1},
    {"elementary_substructure", 2},
    /* Group II: Classical Results (7) */
    {"completeness_theorem", 2},
    {"compactness_theorem", 1},
    {"downward_lowenheim_skolem", 3},
    {"upward_lowenheim_skolem", 3},
    {"vaught_test", 2},
    {"omitting_types_theorem", 2},
    {"interpolation_theorem", 3},
    /* Group III: Model Constructions (6) */
    {"ultraproduct", 3},
    {"elementary_chain", 2},
    {"model_completion", 2},
    {"prime_model", 2},
    {"saturated_model", 2},
    {"homogeneous_model", 1},
    /* Group IV: Stability Theory (6) */
    {"complete_type", 3},
    {"type_space", 3},
    {"stability", 2},
    {"forking", 3},
    {"independence_relation", 3},
    {"rank", 3},
    /* Group V: Specific Theories (5) */
    {"algebraically_closed_field", 2},
    {"real_closed_field", 1},
    {"densely_linearly_ordered", 1},
    {"peano_arithmetic", 1},
    {"presburger_arithmetic", 1},
    /* Group VI: Model-Theoretic Properties (5) */
    {"quantifier_elimination", 3},
    {"model_completeness", 2},
    {"decidability_of_theory", 2},
    {"elementary_equivalence", 2},
    {"elementary_embedding", 3},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"first_order_validity", "undecidable", 3, true},
    {"peano_arithmetic_decidability", "undecidable", 4, true},
    {"theory_isomorphism", "undecidable", 4, true},
    {"model_satisfiability", "undecidable", 3, true},
    {"elementary_equivalence_problem", "undecidable", 4, true},
    {"stable_theory_classification", "undecidable", 5, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "model_theory");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 35 constraint templates", k_templates, K_TEMPLATES_COUNT);
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
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "model_theory", EXPECTED_TEMPLATE_COUNT,
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
    printf("Test 10: Key model theory templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* First-order logic basics */
    const char *fol_basics[] = {"first_order_language", "structure_model",        "satisfaction_relation", "theory",
                                "theory_consistency",   "elementary_substructure"};

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, fol_basics[i]);
        TEST_ASSERT(tmpl != NULL, "first-order logic basic template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Classical theorems */
    const char *classical_theorems[] = {"completeness_theorem",    "compactness_theorem", "downward_lowenheim_skolem",
                                        "upward_lowenheim_skolem", "vaught_test",         "omitting_types_theorem",
                                        "interpolation_theorem"};

    for (int i = 0; i < 7; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, classical_theorems[i]);
        TEST_ASSERT(tmpl != NULL, "classical theorem template should exist");
    }

    /* Stability theory */
    const char *stability_core[] = {"complete_type",         "type_space", "stability", "forking",
                                    "independence_relation", "rank"};

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, stability_core[i]);
        TEST_ASSERT(tmpl != NULL, "stability theory template should exist");
    }

    /* Specific theories */
    ConstraintTemplate *acf = axiom_package_get_template(pkg, "algebraically_closed_field");
    TEST_ASSERT(acf != NULL, "algebraically_closed_field template should exist");
    ConstraintTemplate *rcf = axiom_package_get_template(pkg, "real_closed_field");
    TEST_ASSERT(rcf != NULL, "real_closed_field template should exist");
    ConstraintTemplate *pa = axiom_package_get_template(pkg, "peano_arithmetic");
    TEST_ASSERT(pa != NULL, "peano_arithmetic template should exist");
    ConstraintTemplate *qe = axiom_package_get_template(pkg, "quantifier_elimination");
    TEST_ASSERT(qe != NULL, "quantifier_elimination template should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Model Theory");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructibles);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_save_load_roundtrip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_references);
    TEST_RUN(test_key_templates);

    TEST_SUMMARY();

    return 0;
}
