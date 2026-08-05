/**
 * @file test_axiom_number_theory.c
 * @brief Number Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the number_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Number theory provides the foundational arithmetic framework for Lv-00.
 * The 38 templates cover divisibility, prime numbers, algebraic integers,
 * L-functions, elliptic curves, and transcendence theory.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/number_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/number_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 38
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Elementary Number Theory (6) */
    {"divisibility", 2},
    {"congruence", 3},
    {"euler_totient", 2},
    {"fermat_little_theorem", 2},
    {"chinese_remainder", 3},
    {"quadratic_residue", 3},
    /* Group II: Prime Numbers (7) */
    {"prime_number", 1},
    {"prime_distribution", 2},
    {"twin_primes", 2},
    {"goldbach_conjecture", 2},
    {"prime_number_theorem", 2},
    {"dirichlet_theorem", 2},
    {"algebraic_integer", 3},
    /* Group III: Algebraic Number Theory (7) */
    {"ring_of_integers", 2},
    {"ideal_theory", 2},
    {"class_number", 2},
    {"unit_group", 2},
    {"ramification_theory", 3},
    {"dedekind_domain", 1},
    {"riemann_zeta", 2},
    /* Group IV: Analytic Number Theory (6) */
    {"dirichlet_l_function", 3},
    {"modular_form", 2},
    {"l_function", 2},
    {"euler_product", 1},
    {"functional_equation", 1},
    {"diophantine_equation", 2},
    /* Group V: Diophantine Equations (5) */
    {"pell_equation", 2},
    {"elliptic_curve", 3},
    {"mordell_weil_theorem", 2},
    {"faltings_theorem", 2},
    {"p_adic_numbers", 2},
    /* Group VI: Local-Global Principles (4) */
    {"adeles_ideles", 3},
    {"local_global_principle", 3},
    {"hensel_lemma", 3},
    {"algebraic_number", 3},
    /* Group VII: Transcendence (4) */
    {"transcendental_number", 2},
    {"liouville_number", 1},
    {"catalan_constant", 0},
    {"transcendent_numbers", 1},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"riemann_hypothesis", "million_dollar", 5, true},
    {"goldbach_conjecture_verification", "open_problem", 4, true},
    {"twin_prime_conjecture", "open_problem", 4, true},
    {"class_number_computation", "undecidable", 3, true},
    {"generalized_riemann_hypothesis", "open_problem", 5, true},
    {"ideal_class_group_computation", "undecidable", 4, true},
    {"transcendence_of_constants", "open_problem", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "number_theory");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 38 constraint templates", k_templates, K_TEMPLATES_COUNT);
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
    TEST_ASSERT(
        strstr(pkg->bottom_geometry, "divisibility") != NULL || strstr(pkg->bottom_geometry, "prime_number") != NULL,
        "bottom_geometry should contain number theory concepts");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
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
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "number_theory", EXPECTED_TEMPLATE_COUNT,
                                    EXPECTED_UNCONSTRUCTIBLE_COUNT, AXIOM_TEST_FREE_LV_FREE);
}

static void test_dependency_validation(void) {
    axiom_test_dependency_validation_note(AXIOM_PKG_PATH, NULL);
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
    printf("Test 10: Key number theory templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Elementary number theory core */
    const char *elementary_core[] = {"divisibility", "congruence", "euler_totient", "chinese_remainder",
                                     "quadratic_residue"};

    for (int i = 0; i < 5; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, elementary_core[i]);
        TEST_ASSERT(tmpl != NULL, "elementary template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Algebraic number theory core */
    const char *algebraic_core[] = {"ring_of_integers", "ideal_theory", "class_number"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, algebraic_core[i]);
        TEST_ASSERT(tmpl != NULL, "algebraic number theory template should exist");
    }

    /* Elliptic curves */
    ConstraintTemplate *ec = axiom_package_get_template(pkg, "elliptic_curve");
    TEST_ASSERT(ec != NULL, "elliptic curve should exist");

    /* Riemann zeta and L-functions */
    ConstraintTemplate *zeta = axiom_package_get_template(pkg, "riemann_zeta");
    TEST_ASSERT(zeta != NULL, "Riemann zeta should exist");
    ConstraintTemplate *lf = axiom_package_get_template(pkg, "l_function");
    TEST_ASSERT(lf != NULL, "L-function should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Number Theory");

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
