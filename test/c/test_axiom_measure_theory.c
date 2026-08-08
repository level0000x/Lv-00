/**
 * @file test_axiom_measure_theory.c
 * @brief Measure Theory Axiom Package Test
 *
 * Tests the measure_theory.lvz axiom package, verifying:
 *   - File loading and metadata
 *   - All 52 constraint templates across 14 groups:
 *       Group I:   σ-Algebra Axioms (7 templates)
 *       Group II:  Core Measure Axioms (8 templates)
 *       Group III: Outer Measure Axioms (3 templates)
 *       Group IV:  Carathéodory Measurability (3 templates)
 *       Group V:   Null Sets and Completeness (4 templates)
 *       Group VI:  σ-Finite and Semifinite Measures (3 templates)
 *       Group VII: Signed and Complex Measures (5 templates)
 *       Group VIII: Absolute Continuity and Radon-Nikodym (4 templates)
 *       Group IX:  Product Measures and Fubini-Tonelli (4 templates)
 *       Group X:   Convergence Theorems (4 templates)
 *       Group XI:  Carathéodory Extension Theorem (4 templates)
 *       Group XII: Specific Measures (9 templates)
 *       Group XIII: Lᵖ Spaces (5 templates)
 *       Group XIV: Probability Measure (3 templates)
 *   - All 8 known unconstructible problems
 *   - Logical framework (bottom geometry, negation, contradiction)
 *   - Content hash, round-trip save/load, dependency validation
 *
 * Total templates: 7+8+3+3+4+3+5+4+4+4+4+9+5+3 = 66
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_fail_count = 0;
int g_pass_count = 0;

/* 历史私有 TEST_ASSERT 为非返回式语义（失败仅计数、继续执行），
 * 通过 AXIOM_TEST_NON_RETURNING 让骨架头提供兼容变体，保持行为不变 */
#include "test_helpers.h"

#include "axiom_test_common.h"

#define AXIOM_PKG_PATH "module/axiom_packages/measure_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/measure_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 66
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"vitali_set_non_measurable", "axiom_of_choice", 2, true},
    {"banach_tarski_paradox", "non_measurable_decomposition", 2, true},
    {"universal_measure_on_all_subsets", "non_measurable_set_existence", 2, true},
    {"all_sets_measurable_consistency", "independence_from_ZF_DC", 2, true},
    {"hausdorff_dimension_computation", "algorithmic_undecidability", 2, false},
    {"measure_space_isomorphism_classification", "isomorphism_problem", 2, false},
    {"measure_extension_uniqueness_without_sigma_finite", "non_uniqueness_of_extension", 2, true},
    {"riemann_integrability_decision", "lebesgue_measure_zero_set", 2, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h。
 * Test 2 模板校验与 Test 9 外部引用为文件特有手写体，保留在下方）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "measure_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（test_templates 为文件特有手写，下方保留） */
        .tmpl_style = AXIOM_TEST_TMPL_NONE,

        /* Test 3: 不可构造项（A 形态） */
        .uc_style = AXIOM_TEST_UC_A,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 8 unconstructible problems",
        .uc_expectations = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（S 形态） */
        .lf_style = AXIOM_TEST_LF_S,
        .lf_bottom_geometry = "measure_space_extended_reals",
        .lf_negation_encoding = "classical_equality",
        .lf_contradiction_behavior = PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
        .lf_contradiction_name = "PROPOSITION_KIND_EXPLOSION_PRINCIPLE",

        /* Test 5: 内容哈希（单次形态） */
        .hash_style = AXIOM_TEST_HASH_SINGLE,
        .hash_free = AXIOM_TEST_FREE_LV_FREE_PTR,

        /* Test 6: 往返保存/加载（basic 形态） */
        .rt_style = AXIOM_TEST_RT_BASIC,

        /* Test 7: 依赖验证（V1 形态） */
        .dep_style = AXIOM_TEST_DEP_V1,
        .dep_fail_msg = "FAIL (acceptable)",
        .dep_suffix = " (expected: may fail for cross-reference reduces_to)",

        /* Test 8: 负向查找 */
        .neg_style = AXIOM_TEST_NEG_BASIC,

        /* Test 9: 外部引用（文件特有手写，见下方 test_external_refs） */
        .ext_style = AXIOM_TEST_EXT_NONE,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

/* Test 2：约束模板（文件特有：仅名称数组 + 大量尾部参数校验，保留原体） */
static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 66 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    const char *expected_templates[] = {
        /* Group I: σ-Algebra Axioms (7) */
        "sigma_algebra_contains_X", "sigma_algebra_complement", "sigma_algebra_countable_union",
        "sigma_algebra_countable_intersection", "sigma_algebra_contains_empty", "sigma_algebra_set_difference",
        "sigma_algebra_symmetric_difference",
        /* Group II: Core Measure Axioms (8) */
        "measure_null_empty_set", "measure_non_negativity", "measure_countable_additivity", "measure_finite_additivity",
        "measure_monotonicity", "measure_countable_subadditivity", "measure_continuity_from_below",
        "measure_continuity_from_above",
        /* Group III: Outer Measure Axioms (3) */
        "outer_measure_null_empty", "outer_measure_monotonicity", "outer_measure_countable_subadditivity",
        /* Group IV: Carathéodory Measurability (3) */
        "caratheodory_measurability", "measurable_sets_form_sigma_algebra", "outer_measure_restriction_complete",
        /* Group V: Null Sets and Completeness (4) */
        "null_set", "null_set_subset_measurable", "almost_everywhere", "measure_completion",
        /* Group VI: σ-Finite and Semifinite Measures (3) */
        "sigma_finite_measure", "semifinite_measure", "localizable_measure",
        /* Group VII: Signed and Complex Measures (5) */
        "signed_measure", "hahn_decomposition", "jordan_decomposition", "total_variation_measure", "complex_measure",
        /* Group VIII: Absolute Continuity and Radon-Nikodym (4) */
        "absolute_continuity", "mutual_singularity", "radon_nikodym_theorem", "lebesgue_decomposition",
        /* Group IX: Product Measures and Fubini-Tonelli (4) */
        "product_sigma_algebra", "product_measure", "fubini_theorem", "tonelli_theorem",
        /* Group X: Convergence Theorems (4) */
        "monotone_convergence_theorem", "fatou_lemma", "dominated_convergence_theorem", "uniform_integrability",
        /* Group XI: Carathéodory Extension Theorem (4) */
        "pre_measure", "caratheodory_outer_measure_construction", "caratheodory_extension_theorem",
        "dynkin_pi_lambda_theorem",
        /* Group XII: Specific Measures (9) */
        "counting_measure", "dirac_measure", "lebesgue_measure_interval", "lebesgue_measure_rn", "borel_sigma_algebra",
        "hausdorff_measure", "hausdorff_dimension", "pushforward_measure", "measure_restriction",
        /* Group XIII: Lᵖ Spaces (5) */
        "lp_norm", "linf_norm", "holder_inequality", "minkowski_inequality", "jensen_inequality",
        /* Group XIV: Probability Measure (3) */
        "probability_measure", "conditional_expectation", "sigma_algebra_independence", NULL};

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

    /* Verify specific param counts for key templates */
    ConstraintTemplate *t;

    /* Group I: σ-Algebra Axioms */
    t = axiom_package_get_template(pkg, "sigma_algebra_contains_X");
    TEST_ASSERT(t && t->param_count == 0, "sigma_algebra_contains_X should have 0 params");
    t = axiom_package_get_template(pkg, "sigma_algebra_complement");
    TEST_ASSERT(t && t->param_count == 1, "sigma_algebra_complement should have 1 param");
    t = axiom_package_get_template(pkg, "sigma_algebra_countable_union");
    TEST_ASSERT(t && t->param_count == 1, "sigma_algebra_countable_union should have 1 param");
    t = axiom_package_get_template(pkg, "sigma_algebra_countable_intersection");
    TEST_ASSERT(t && t->param_count == 1, "sigma_algebra_countable_intersection should have 1 param");
    t = axiom_package_get_template(pkg, "sigma_algebra_set_difference");
    TEST_ASSERT(t && t->param_count == 2, "sigma_algebra_set_difference should have 2 params");
    t = axiom_package_get_template(pkg, "sigma_algebra_symmetric_difference");
    TEST_ASSERT(t && t->param_count == 2, "sigma_algebra_symmetric_difference should have 2 params");

    /* Group II: Core Measure Axioms */
    t = axiom_package_get_template(pkg, "measure_null_empty_set");
    TEST_ASSERT(t && t->param_count == 0, "measure_null_empty_set should have 0 params");
    t = axiom_package_get_template(pkg, "measure_non_negativity");
    TEST_ASSERT(t && t->param_count == 1, "measure_non_negativity should have 1 param");
    t = axiom_package_get_template(pkg, "measure_countable_additivity");
    TEST_ASSERT(t && t->param_count == 1, "measure_countable_additivity should have 1 param");
    t = axiom_package_get_template(pkg, "measure_finite_additivity");
    TEST_ASSERT(t && t->param_count == 2, "measure_finite_additivity should have 2 params");
    t = axiom_package_get_template(pkg, "measure_monotonicity");
    TEST_ASSERT(t && t->param_count == 2, "measure_monotonicity should have 2 params");
    t = axiom_package_get_template(pkg, "measure_continuity_from_below");
    TEST_ASSERT(t && t->param_count == 1, "measure_continuity_from_below should have 1 param");
    t = axiom_package_get_template(pkg, "measure_continuity_from_above");
    TEST_ASSERT(t && t->param_count == 1, "measure_continuity_from_above should have 1 param");

    /* Group III: Outer Measure */
    t = axiom_package_get_template(pkg, "outer_measure_null_empty");
    TEST_ASSERT(t && t->param_count == 0, "outer_measure_null_empty should have 0 params");
    t = axiom_package_get_template(pkg, "outer_measure_monotonicity");
    TEST_ASSERT(t && t->param_count == 2, "outer_measure_monotonicity should have 2 params");
    t = axiom_package_get_template(pkg, "outer_measure_countable_subadditivity");
    TEST_ASSERT(t && t->param_count == 1, "outer_measure_countable_subadditivity should have 1 param");

    /* Group IV: Carathéodory */
    t = axiom_package_get_template(pkg, "caratheodory_measurability");
    TEST_ASSERT(t && t->param_count == 2, "caratheodory_measurability should have 2 params");
    t = axiom_package_get_template(pkg, "measurable_sets_form_sigma_algebra");
    TEST_ASSERT(t && t->param_count == 0, "measurable_sets_form_sigma_algebra should have 0 params");
    t = axiom_package_get_template(pkg, "outer_measure_restriction_complete");
    TEST_ASSERT(t && t->param_count == 0, "outer_measure_restriction_complete should have 0 params");

    /* Group V: Null Sets */
    t = axiom_package_get_template(pkg, "null_set");
    TEST_ASSERT(t && t->param_count == 1, "null_set should have 1 param");
    t = axiom_package_get_template(pkg, "null_set_subset_measurable");
    TEST_ASSERT(t && t->param_count == 2, "null_set_subset_measurable should have 2 params");
    t = axiom_package_get_template(pkg, "almost_everywhere");
    TEST_ASSERT(t && t->param_count == 1, "almost_everywhere should have 1 param");
    t = axiom_package_get_template(pkg, "measure_completion");
    TEST_ASSERT(t && t->param_count == 1, "measure_completion should have 1 param");

    /* Group VII: Signed/Complex Measures */
    t = axiom_package_get_template(pkg, "hahn_decomposition");
    TEST_ASSERT(t && t->param_count == 1, "hahn_decomposition should have 1 param");
    t = axiom_package_get_template(pkg, "jordan_decomposition");
    TEST_ASSERT(t && t->param_count == 1, "jordan_decomposition should have 1 param");
    t = axiom_package_get_template(pkg, "complex_measure");
    TEST_ASSERT(t && t->param_count == 1, "complex_measure should have 1 param");

    /* Group VIII: Radon-Nikodym */
    t = axiom_package_get_template(pkg, "absolute_continuity");
    TEST_ASSERT(t && t->param_count == 2, "absolute_continuity should have 2 params");
    t = axiom_package_get_template(pkg, "radon_nikodym_theorem");
    TEST_ASSERT(t && t->param_count == 2, "radon_nikodym_theorem should have 2 params");
    t = axiom_package_get_template(pkg, "lebesgue_decomposition");
    TEST_ASSERT(t && t->param_count == 2, "lebesgue_decomposition should have 2 params");

    /* Group IX: Product Measures */
    t = axiom_package_get_template(pkg, "fubini_theorem");
    TEST_ASSERT(t && t->param_count == 3, "fubini_theorem should have 3 params");
    t = axiom_package_get_template(pkg, "tonelli_theorem");
    TEST_ASSERT(t && t->param_count == 3, "tonelli_theorem should have 3 params");

    /* Group X: Convergence */
    t = axiom_package_get_template(pkg, "dominated_convergence_theorem");
    TEST_ASSERT(t && t->param_count == 2, "dominated_convergence_theorem should have 2 params");

    /* Group XI: Extension */
    t = axiom_package_get_template(pkg, "caratheodory_extension_theorem");
    TEST_ASSERT(t && t->param_count == 1, "caratheodory_extension_theorem should have 1 param");
    t = axiom_package_get_template(pkg, "dynkin_pi_lambda_theorem");
    TEST_ASSERT(t && t->param_count == 2, "dynkin_pi_lambda_theorem should have 2 params");
    t = axiom_package_get_template(pkg, "pre_measure");
    TEST_ASSERT(t && t->param_count == 2, "pre_measure should have 2 params");

    /* Group XII: Specific Measures */
    t = axiom_package_get_template(pkg, "counting_measure");
    TEST_ASSERT(t && t->param_count == 1, "counting_measure should have 1 param");
    t = axiom_package_get_template(pkg, "dirac_measure");
    TEST_ASSERT(t && t->param_count == 2, "dirac_measure should have 2 params");
    t = axiom_package_get_template(pkg, "lebesgue_measure_interval");
    TEST_ASSERT(t && t->param_count == 2, "lebesgue_measure_interval should have 2 params");
    t = axiom_package_get_template(pkg, "lebesgue_measure_rn");
    TEST_ASSERT(t && t->param_count == 2, "lebesgue_measure_rn should have 2 params");
    t = axiom_package_get_template(pkg, "hausdorff_measure");
    TEST_ASSERT(t && t->param_count == 3, "hausdorff_measure should have 3 params");
    t = axiom_package_get_template(pkg, "hausdorff_dimension");
    TEST_ASSERT(t && t->param_count == 1, "hausdorff_dimension should have 1 param");
    t = axiom_package_get_template(pkg, "pushforward_measure");
    TEST_ASSERT(t && t->param_count == 2, "pushforward_measure should have 2 params");
    t = axiom_package_get_template(pkg, "measure_restriction");
    TEST_ASSERT(t && t->param_count == 2, "measure_restriction should have 2 params");

    /* Group XIII: Lᵖ Spaces */
    t = axiom_package_get_template(pkg, "lp_norm");
    TEST_ASSERT(t && t->param_count == 2, "lp_norm should have 2 params");
    t = axiom_package_get_template(pkg, "holder_inequality");
    TEST_ASSERT(t && t->param_count == 4, "holder_inequality should have 4 params");
    t = axiom_package_get_template(pkg, "minkowski_inequality");
    TEST_ASSERT(t && t->param_count == 3, "minkowski_inequality should have 3 params");
    t = axiom_package_get_template(pkg, "jensen_inequality");
    TEST_ASSERT(t && t->param_count == 3, "jensen_inequality should have 3 params");

    /* Group XIV: Probability */
    t = axiom_package_get_template(pkg, "probability_measure");
    TEST_ASSERT(t && t->param_count == 1, "probability_measure should have 1 param");
    t = axiom_package_get_template(pkg, "conditional_expectation");
    TEST_ASSERT(t && t->param_count == 2, "conditional_expectation should have 2 params");
    t = axiom_package_get_template(pkg, "sigma_algebra_independence");
    TEST_ASSERT(t && t->param_count == 2, "sigma_algebra_independence should have 2 params");

    axiom_package_destroy(pkg);
}

/* Test 3/4/5/6/7/8 已收敛至 kCases 数据驱动用例（见上） */

/* Test 9：外部引用（文件特有：URL 精确匹配数组，保留原体） */
static void test_external_refs(void) {
    printf("Test 9: Verify external references...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    const char *expected_urls[] = {"https://en.wikipedia.org/wiki/Vitali_set",
                                   "https://en.wikipedia.org/wiki/Banach%E2%80%93Tarski_paradox",
                                   "https://en.wikipedia.org/wiki/Non-measurable_set",
                                   "https://en.wikipedia.org/wiki/Solovay_model",
                                   "https://en.wikipedia.org/wiki/Hausdorff_dimension",
                                   "https://en.wikipedia.org/wiki/Standard_probability_space",
                                   "https://en.wikipedia.org/wiki/Carath%C3%A9odory%27s_extension_theorem",
                                   "https://en.wikipedia.org/wiki/Riemann_integral#Integrability",
                                   NULL};

    for (int i = 0; expected_urls[i] != NULL; i++) {
        bool found = false;
        for (int j = 0; j < axiom_package_get_unconstructible_count(pkg); j++) {
            KnownUnconstructible *uc = axiom_package_get_unconstructible(pkg, j);
            if (uc->external_ref && strcmp(uc->external_ref, expected_urls[i]) == 0) {
                found = true;
                break;
            }
        }
        TEST_ASSERT(found, expected_urls[i]);
    }

    printf("  All external references verified\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Measure Theory Axiom Package Test Suite")
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Testing: axiom_packages/measure_theory.lvz ===\n\n");
    LV_REGISTER_AXIOM_CASES("MeasureTheory", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_external_refs);
TEST_MAIN_END()
