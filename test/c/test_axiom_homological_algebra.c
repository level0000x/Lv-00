/**
 * @file test_axiom_homological_algebra.c
 * @brief Homological Algebra Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the homological_algebra.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Homological algebra is formalized through 36 templates covering abelian
 * categories, chain complexes, derived functors, projective/injective modules,
 * homological dimensions, and cohomology theories.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/homological_algebra.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/homological_algebra_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 36
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Abelian Category (6) */
    {"zero_object", 0},
    {"biproduct", 2},
    {"kernel", 1},
    {"cokernel", 1},
    {"abelian_category", 0},
    {"exact_sequence", 3},
    /* Group II: Chain Complexes (6) */
    {"chain_complex", 2},
    {"cochain_complex", 2},
    {"chain_map", 2},
    {"chain_homotopy", 3},
    {"mapping_cone", 2},
    {"cylinder", 2},
    /* Group III: Homology & Exact Sequences (5) */
    {"homology_group", 2},
    {"cohomology_group", 2},
    {"long_exact_sequence", 3},
    {"snake_lemma", 3},
    {"five_lemma", 3},
    /* Group IV: Derived Functors (6) */
    {"left_derived_functor", 2},
    {"right_derived_functor", 2},
    {"tor_functor", 3},
    {"ext_functor", 3},
    {"spectral_sequence", 3},
    {"derived_category", 1},
    /* Group V: Resolutions (5) */
    {"injective_module", 1},
    {"projective_module", 1},
    {"flat_module", 1},
    {"free_resolution", 2},
    {"injective_resolution", 2},
    /* Group VI: Homological Dimensions (4) */
    {"projective_dimension", 2},
    {"injective_dimension", 2},
    {"global_dimension", 1},
    {"regular_sequence", 2},
    /* Group VII: Cohomology Theories (4) */
    {"group_cohomology", 2},
    {"sheaf_cohomology_application", 2},
    {"hochschild_homology", 2},
    {"cyclic_homology", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"projective_dimension_computation", "undecidable", 4, true},
    {"global_dimension_computation", "undecidable", 4, true},
    {"spectral_sequence_convergence", "undecidable", 4, true},
    {"extension_group_computation", "undecidable", 4, true},
    {"derived_equivalence_problem", "undecidable", 4, true},
    {"homological_conjecture_resolution", "undecidable", 4, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "homological_algebra");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 36 constraint templates", k_templates, K_TEMPLATES_COUNT);
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
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "homological_algebra", EXPECTED_TEMPLATE_COUNT,
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
    printf("Test 10: Key homological algebra templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Abelian category core */
    const char *abelian_core[] = {"zero_object", "biproduct",        "kernel",
                                  "cokernel",    "abelian_category", "exact_sequence"};

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, abelian_core[i]);
        TEST_ASSERT(tmpl != NULL, "abelian category core template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Chain complex basics */
    const char *chain_basics[] = {"chain_complex",  "cochain_complex", "chain_map",
                                  "chain_homotopy", "mapping_cone",    "cylinder"};

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, chain_basics[i]);
        TEST_ASSERT(tmpl != NULL, "chain complex basic template should exist");
    }

    /* Derived functors */
    const char *derived_functors[] = {"left_derived_functor", "right_derived_functor", "tor_functor", "ext_functor",
                                      "spectral_sequence"};

    for (int i = 0; i < 5; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, derived_functors[i]);
        TEST_ASSERT(tmpl != NULL, "derived functor template should exist");
    }

    /* Resolutions */
    ConstraintTemplate *pm = axiom_package_get_template(pkg, "projective_module");
    TEST_ASSERT(pm != NULL, "projective_module template should exist");
    ConstraintTemplate *im = axiom_package_get_template(pkg, "injective_module");
    TEST_ASSERT(im != NULL, "injective_module template should exist");
    ConstraintTemplate *fm = axiom_package_get_template(pkg, "flat_module");
    TEST_ASSERT(fm != NULL, "flat_module template should exist");
    ConstraintTemplate *fr = axiom_package_get_template(pkg, "free_resolution");
    TEST_ASSERT(fr != NULL, "free_resolution template should exist");
    ConstraintTemplate *ir = axiom_package_get_template(pkg, "injective_resolution");
    TEST_ASSERT(ir != NULL, "injective_resolution template should exist");

    /* Cohomology theories */
    ConstraintTemplate *gc = axiom_package_get_template(pkg, "group_cohomology");
    TEST_ASSERT(gc != NULL, "group_cohomology template should exist");
    ConstraintTemplate *hh = axiom_package_get_template(pkg, "hochschild_homology");
    TEST_ASSERT(hh != NULL, "hochschild_homology template should exist");
    ConstraintTemplate *cyh = axiom_package_get_template(pkg, "cyclic_homology");
    TEST_ASSERT(cyh != NULL, "cyclic_homology template should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Homological Algebra")

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

