/**
 * @file test_axiom_real_analysis.c
 * @brief Real Analysis Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the real_analysis.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/real_analysis.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/real_analysis_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 43
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Ordered Field & Completeness (8) */
    {"ordered_field", 3},
    {"additive_associativity", 3},
    {"multiplicative_associativity", 3},
    {"distributivity", 3},
    {"order_compatibility", 3},
    {"archimedean_property", 1},
    {"dedekind_completeness", 1},
    {"least_upper_bound", 1},
    /* Group II: Sequences & Series (7) */
    {"convergent_sequence", 2},
    {"cauchy_sequence", 1},
    {"monotone_convergence", 1},
    {"bolzano_weierstrass", 1},
    {"sequential_compactness", 1},
    {"limit_superior", 1},
    {"limit_inferior", 1},
    /* Group III: Continuity (6) */
    {"continuous_function", 2},
    {"uniform_continuity", 1},
    {"intermediate_value_theorem", 3},
    {"extreme_value_theorem", 1},
    {"lipschitz_continuity", 2},
    {"uniform_convergence", 2},
    /* Group IV: Differentiation & Integration (6) */
    {"derivative", 2},
    {"riemann_integral", 3},
    {"fundamental_theorem_of_calculus", 3},
    {"chain_rule", 3},
    {"mean_value_theorem", 3},
    {"taylor_expansion", 3},
    /* Group V: Measure Theory (7) */
    {"sigma_algebra", 1},
    {"measurable_set", 1},
    {"measure", 1},
    {"lebesgue_measure", 1},
    {"outer_measure", 1},
    {"caratheodory_extension", 1},
    {"measurable_function", 2},
    /* Group VI: Lebesgue Integration (5) */
    {"lebesgue_integral", 2},
    {"monotone_convergence_theorem", 1},
    {"dominated_convergence_theorem", 2},
    {"fatou_lemma", 1},
    {"fubini_theorem", 3},
    /* Group VII: Lp Spaces (4) */
    {"lp_space", 2},
    {"holder_inequality", 4},
    {"minkowski_inequality", 3},
    {"l2_hilbert_space", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"banach_tarski_paradox", "ac_non_constructive", 3, true},
    {"vitali_set_non_measurable", "ac_non_constructive", 3, true},
    {"lebesgue_measure_borel", "undecidable", 4, true},
    {"riemann_integrability_characterization", "undecidable", 3, true},
    {"improper_integral_convergence", "undecidable", 3, true},
    {"function_space_separability", "undecidable", 3, true},
    {"distribution_generalized_function", "undecidable", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "real_analysis");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 43 constraint templates", k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructibles(void) {
    axiom_test_unconstructibles_min_deps(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                         "should have 7 unconstructible problems", k_unconstructibles,
                                         K_UNCONSTRUCTIBLES_COUNT);
}

/* Test 4：逻辑框架（文件特有：strstr 概念检查，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    /* bottom_geometry contains "dedekind" or "real_number" */
    TEST_ASSERT(strstr(pkg->bottom_geometry, "dedekind") != NULL || strstr(pkg->bottom_geometry, "real_number") != NULL,
                "bottom_geometry should contain 'dedekind' or 'real_number'");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    /* negation_encoding contains "complement" or "measure" */
    TEST_ASSERT(
        strstr(pkg->negation_encoding, "complement") != NULL || strstr(pkg->negation_encoding, "measure") != NULL,
        "negation_encoding should contain 'complement' or 'measure'");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    axiom_test_content_hash_deterministic(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_save_load_roundtrip(void) {
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "real_analysis", EXPECTED_TEMPLATE_COUNT,
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
    printf("Test 10: Key real analysis templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core ordered field definition */
    const char *ordered_field_core[] = {
        "ordered_field",       "additive_associativity", "multiplicative_associativity", "distributivity",
        "order_compatibility", "archimedean_property",   "dedekind_completeness",        "least_upper_bound"};

    for (int i = 0; i < 8; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, ordered_field_core[i]);
        TEST_ASSERT(tmpl != NULL, "core ordered field template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Key analysis theorems */
    const char *key_theorems[] = {"intermediate_value_theorem",   "extreme_value_theorem",
                                  "mean_value_theorem",           "fundamental_theorem_of_calculus",
                                  "monotone_convergence_theorem", "dominated_convergence_theorem"};

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, key_theorems[i]);
        TEST_ASSERT(tmpl != NULL, "key analysis theorem template should exist");
    }

    /* Measure theory core */
    const char *measure_core[] = {"sigma_algebra",    "measurable_set", "measure",
                                  "lebesgue_measure", "outer_measure",  "caratheodory_extension"};

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, measure_core[i]);
        TEST_ASSERT(tmpl != NULL, "measure theory core template should exist");
    }

    /* Lp space and inequalities */
    ConstraintTemplate *lp = axiom_package_get_template(pkg, "lp_space");
    TEST_ASSERT(lp != NULL, "lp_space template should exist");
    ConstraintTemplate *holder = axiom_package_get_template(pkg, "holder_inequality");
    TEST_ASSERT(holder != NULL, "holder_inequality template should exist");
    ConstraintTemplate *minkowski = axiom_package_get_template(pkg, "minkowski_inequality");
    TEST_ASSERT(minkowski != NULL, "minkowski_inequality template should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Real Analysis")

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

