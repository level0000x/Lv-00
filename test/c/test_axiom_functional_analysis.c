/**
 * @file test_axiom_functional_analysis.c
 * @brief Functional Analysis Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the functional_analysis.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Functional analysis provides infinite-dimensional analysis for Lv-00.
 * The 37 templates cover Banach/Hilbert spaces, operators, spectra,
 * C*-algebras, distributions, and Fourier analysis.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/functional_analysis.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/functional_analysis_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 37
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Normed Spaces (5) */
    {"normed_vector_space", 2},
    {"banach_space", 1},
    {"dual_space", 1},
    {"hahn_banach_theorem", 3},
    {"separation_theorem", 2},
    /* Group II: Inner Product Spaces (6) */
    {"inner_product_space", 2},
    {"hilbert_space", 1},
    {"orthonormal_basis", 1},
    {"projection_theorem", 2},
    {"riesz_representation", 1},
    {"parseval_identity", 2},
    /* Group III: Operator Theory (7) */
    {"bounded_linear_operator", 3},
    {"operator_norm", 1},
    {"operator_adjoint", 1},
    {"compact_operator", 1},
    {"self_adjoint_operator", 1},
    {"unitary_operator", 1},
    {"spectrum_of_operator", 1},
    /* Group IV: Spectral Theory (6) */
    {"spectral_radius", 1},
    {"resolvent_set", 1},
    {"spectral_theorem", 1},
    {"eigenvalue_problem", 1},
    {"spectral_decomposition", 1},
    {"banach_algebra", 1},
    /* Group V: Operator Algebras (5) */
    {"c_star_algebra", 1},
    {"von_neumann_algebra", 1},
    {"gel_fand_naimark", 1},
    {"functional_calculus", 2},
    {"sobolev_space", 3},
    /* Group VI: Sobolev Spaces (4) */
    {"weak_derivative", 2},
    {"trace_theorem", 2},
    {"embedding_theorem", 3},
    {"test_function_space", 1},
    /* Group VII: Distributions (4) */
    {"distribution_theory", 1},
    {"fourier_transform", 1},
    {"convolution", 2},
    {"singularity_theorem", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"invariant_subspace_problem", "open_problem", 3, true},
    {"approximation_property", "undecidable", 3, true},
    {"komornik_loreti_constant", "open_problem", 2, true},
    {"boundedness_of_singular_integrals", "undecidable", 3, true},
    {"spectral_theorem_self_adjoint", "open_problem", 3, true},
    {"existence_of_complement", "undecidable", 2, true},
    {"continuous_function_algebra", "undecidable", 2, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "functional_analysis");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 37 constraint templates", k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructibles(void) {
    axiom_test_unconstructibles_min_deps(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                         "should have 7 unconstructible problems", k_unconstructibles,
                                         K_UNCONSTRUCTIBLES_COUNT);
}

/* Test 4：逻辑框架（文件特有：bottom_geometry 用 strstr 检查，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    TEST_ASSERT(strstr(pkg->bottom_geometry, "normed_vector_space") != NULL ||
                    strstr(pkg->bottom_geometry, "hilbert_space") != NULL,
                "bottom_geometry should contain functional analysis concepts");
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
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "functional_analysis", EXPECTED_TEMPLATE_COUNT,
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
    printf("Test 10: Key functional analysis templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Normed space core */
    const char *normed_core[] = {"normed_vector_space", "banach_space", "dual_space"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, normed_core[i]);
        TEST_ASSERT(tmpl != NULL, "normed space template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Hilbert space core */
    const char *hilbert_core[] = {"inner_product_space", "hilbert_space", "orthonormal_basis"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, hilbert_core[i]);
        TEST_ASSERT(tmpl != NULL, "Hilbert space template should exist");
    }

    /* Operator theory core */
    ConstraintTemplate *blo = axiom_package_get_template(pkg, "bounded_linear_operator");
    TEST_ASSERT(blo != NULL, "bounded linear operator should exist");
    ConstraintTemplate *sa = axiom_package_get_template(pkg, "self_adjoint_operator");
    TEST_ASSERT(sa != NULL, "self-adjoint operator should exist");

    /* Spectral theorem */
    ConstraintTemplate *st = axiom_package_get_template(pkg, "spectral_theorem");
    TEST_ASSERT(st != NULL, "spectral theorem should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Functional Analysis")

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

