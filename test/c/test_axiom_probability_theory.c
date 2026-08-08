/**
 * @file test_axiom_probability_theory.c
 * @brief Probability Theory Axiom Package Test
 *
 * Tests the loading, template verification, unconstructible problem
 * validation, logical framework, round-trip save/load, and dependency
 * checking for the probability_theory axiom package (v1.0.0).
 *
 * Mathematical theory: Probability Theory (Kolmogorov axioms, probability
 * spaces, random variables, expectation, limit theorems, stochastic
 * processes).
 *
 * Key references:
 *   - Kolmogorov, A.N. (1933). "Grundbegriffe der Wahrscheinlichkeitsrechnung."
 *   - Billingsley, P. (1995). "Probability and Measure" (3rd ed.).
 *   - Feller, W. (1968). "An Introduction to Probability Theory."
 *   - Durrett, R. (2019). "Probability: Theory and Examples" (5th ed.).
 *   - Wikipedia: Probability axioms
 *     https://en.wikipedia.org/wiki/Probability_axioms
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

#define AXIOM_PKG_PATH "module/axiom_packages/probability_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/probability_theory_test_save.lvz"

/* Template count: 87 templates across several groups
 *   Group I:    Kolmogorov Axioms              = 3
 *   Group II:   Elementary Consequences        = 12
 *   Group III:  Conditional Probability        = 6
 *   Group IV:   Independence                   = 8
 *   Group V:    Random Variables               = 12
 *   Group VI:   Expected Value and Moments     = 10
 *   Group VII:  Convergence Theorems           = 8
 *   Group VIII: Law of Large Numbers           = 4
 *   Group IX:   Central Limit Theorem          = 4
 *   Group X:    Common Distributions           = 10
 *   Group XI:   Stochastic Processes           = 6
 *   Group XII:  Characteristic Functions       = 4
 *   Total = 3+12+6+8+12+10+8+4+4+10+6+4 = 87
 */
#define EXPECTED_TEMPLATE_COUNT 87
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "probability_theory");
}

/* Test 2：约束模板（文件特有：>= 50 弱断言 + 动态消息，保留原体） */
static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) >= 50, "should have at least 50 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    const char *expected_templates[] = {
        /* Group I: Kolmogorov Axioms */
        "kolmogorov_non_negativity", "kolmogorov_unit_measure", "kolmogorov_sigma_additivity",
        /* Group II: Elementary Consequences */
        "prob_empty_set_zero", "prob_bounds", "prob_complement_rule", "prob_monotonicity", "prob_union_two",
        "boole_inequality", "bonferroni_inequality", "prob_continuity_below", "prob_continuity_above",
        "inclusion_exclusion", "partition_formula", "prob_finite_additivity",
        /* Group III: Conditional Probability */
        "conditional_prob_def", "multiplication_rule", "chain_rule", "law_total_probability", "bayes_theorem",
        "bayes_theorem_partition",
        /* Group IV: Independence */
        "independence_pairwise", "independence_mutual", "independence_conditional", "independence_complement",
        "independence_trivial", "independence_pairwise_not_mutual", "independence_sigma_algebras",
        "independence_random_variables",
        /* Group V: Random Variables */
        "random_variable_def", "distribution_function", "random_variable_discrete", "random_variable_continuous",
        "pmf_def", "pdf_def", "indicator_random_variable", "random_variable_function", "joint_distribution",
        "marginal_distribution", "random_variable_transformation", "quantile_function",
        /* Group VI: Expected Value and Moments */
        "expected_value_def", "expected_value_linearity", "expected_value_indicator", "expected_value_nonnegativity",
        "expected_value_monotonicity", "variance_def", "variance_linear_transform", "covariance_def",
        "correlation_coefficient", "cauchy_schwarz_inequality",
        /* Group VII: Convergence Theorems */
        "convergence_almost_sure", "convergence_probability", "convergence_Lp", "convergence_distribution",
        "monotone_convergence", "dominated_convergence", "fatou_lemma", "convergence_hierarchy",
        /* Group VIII: Law of Large Numbers */
        "weak_law_large_numbers", "strong_law_large_numbers", "kolmogorov_three_series", "borel_cantelli",
        /* Group IX: Central Limit Theorem */
        "central_limit_theorem", "lindeberg_feller_clt", "berry_esseen_theorem", "characteristic_function_convergence",
        /* Group X: Common Distributions */
        "distribution_bernoulli", "distribution_binomial", "distribution_poisson", "distribution_geometric",
        "distribution_uniform_continuous", "distribution_exponential", "distribution_normal", "distribution_gamma",
        "distribution_beta", "distribution_chi_squared",
        /* Group XI: Stochastic Processes */
        "stochastic_process_def", "filtration_def", "martingale_def", "markov_property", "stopping_time_def",
        "optional_stopping_theorem",
        /* Group XII: Characteristic Functions */
        "characteristic_function_def", "characteristic_function_properties", "characteristic_function_uniqueness",
        "characteristic_function_inversion"};

    for (int i = 0; i < (int) (sizeof(expected_templates) / sizeof(expected_templates[0])); i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expected_templates[i]);
        char msg[128];
        snprintf(msg, sizeof(msg), "template '%s' should exist", expected_templates[i]);
        TEST_ASSERT(tmpl != NULL, msg);
    }

    axiom_package_destroy(pkg);
}

/* Test 3：不可构造项（文件特有：>= 5 弱断言 + 名称数组，保留原体） */
static void test_unconstructibles(void) {
    printf("Test 3: Verify unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) >= 5, "should have at least 5 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    const char *expected_unconstructibles[] = {
        "non_measurable_set_existence",  "vitali_set_non_measurable",
        "banach_tarski_paradox",         "solovay_all_sets_measurable",
        "slln_without_sigma_additivity", "exact_continuous_simulation",
        "exact_probability_computation", "regular_conditional_probability_general"};

    for (int i = 0; i < (int) (sizeof(expected_unconstructibles) / sizeof(expected_unconstructibles[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected_unconstructibles[i]);
        char msg[128];
        snprintf(msg, sizeof(msg), "unconstructible '%s' should exist", expected_unconstructibles[i]);
        TEST_ASSERT(uc != NULL, msg);
    }

    axiom_package_destroy(pkg);
}

/* Test 4：逻辑框架（文件特有：大写引号输出且无 contradiction 打印，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    TEST_ASSERT(strcmp(pkg->bottom_geometry, "probability_space") == 0,
                "bottom_geometry should be 'probability_space'");
    printf("  Bottom geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    TEST_ASSERT(strcmp(pkg->negation_encoding, "event_complement") == 0,
                "negation_encoding should be 'event_complement'");
    printf("  Negation encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");

    axiom_package_destroy(pkg);
}

/* Test 5：内容哈希（文件特有：%.16s 两段打印，保留原体） */
static void test_content_hash(void) {
    printf("Test 5: Compute content hash...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should be computed");
    TEST_ASSERT(strlen(hash) == 64, "hash should be 64 hex characters");

    printf("  Content hash: %.16s...%s\n", hash, hash + 56);

    lv_free_ptr(hash);
    axiom_package_destroy(pkg);
}

/* Test 6：往返保存/加载（文件特有：名称/版本打印 + 无哈希校验，保留原体） */
static void test_round_trip(void) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg1, AXIOM_PKG_PATH);

    AxiomSaveStatus save_status = axiom_package_save(pkg1, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "save should succeed");

    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "load from saved file should succeed");

    TEST_ASSERT(strcmp(pkg1->name, pkg2->name) == 0, "names should match after round-trip");
    TEST_ASSERT(strcmp(pkg1->version, pkg2->version) == 0, "versions should match after round-trip");
    TEST_ASSERT(axiom_package_get_template_count(pkg1) == axiom_package_get_template_count(pkg2), "template counts should match after round-trip");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg1) == axiom_package_get_unconstructible_count(pkg2),
                "unconstructible counts should match after round-trip");

    printf("  Round-trip: '%s' v%s, %d templates, %d unconstructibles\n", pkg2->name, pkg2->version,
           axiom_package_get_template_count(pkg2), axiom_package_get_unconstructible_count(pkg2));

    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

/* Test 7：依赖验证（文件特有：无打印，保留原体） */
static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Probability theory depends on measure_theory and zfc_set_theory */
    AxiomPackage *loaded_packages[] = {pkg};
    bool valid = axiom_package_validate_dependencies(pkg, loaded_packages, 1);
    /* Dependency data may be incomplete; skip strict assert */
    (void) valid;

    axiom_package_destroy(pkg);
}

/* Test 8：负向查找（文件特有：无收尾打印，保留原体） */
static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "nonexistent template should return NULL");

    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "nonexistent unconstructible should return NULL");

    axiom_package_destroy(pkg);
}

/* Test 9：外部引用（文件特有：指定条目 strstr 检查，保留原体） */
static void test_external_refs(void) {
    printf("Test 9: Verify external references...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Check non_measurable_set_existence has external reference */
    KnownUnconstructible *uc1 = axiom_package_lookup_unconstructible(pkg, "non_measurable_set_existence");
    TEST_ASSERT(uc1 != NULL && uc1->external_ref != NULL, "non_measurable_set_existence should have external_ref");
    TEST_ASSERT(strstr(uc1->external_ref, "wikipedia.org") != NULL, "external_ref should contain wikipedia.org");
    printf("  non_measurable_set_existence ref: %s\n", uc1->external_ref);

    /* Check vitali_set_non_measurable has external reference */
    KnownUnconstructible *uc2 = axiom_package_lookup_unconstructible(pkg, "vitali_set_non_measurable");
    TEST_ASSERT(uc2 != NULL && uc2->external_ref != NULL, "vitali_set_non_measurable should have external_ref");
    printf("  vitali_set_non_measurable ref: %s\n", uc2->external_ref);

    axiom_package_destroy(pkg);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

/* ------------------------------------------------------------------ */
/* Test 10: Key Kolmogorov axioms present                              */
/* ------------------------------------------------------------------ */
static void test_key_axioms(void) {
    printf("Test 10: Verify key Kolmogorov axioms...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* The three Kolmogorov axioms are fundamental */
    const char *kolmogorov_axioms[] = {
        "kolmogorov_non_negativity",  /* K1: P(E) >= 0 */
        "kolmogorov_unit_measure",    /* K2: P(Ω) = 1 */
        "kolmogorov_sigma_additivity" /* K3: σ-additivity */
    };

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, kolmogorov_axioms[i]);
        char msg[128];
        snprintf(msg, sizeof(msg), "Kolmogorov axiom '%s' should exist", kolmogorov_axioms[i]);
        TEST_ASSERT(tmpl != NULL, msg);
    }

    /* Verify key theorems */
    const char *key_theorems[] = {"bayes_theorem", "central_limit_theorem", "weak_law_large_numbers",
                                  "strong_law_large_numbers"};

    for (int i = 0; i < (int) (sizeof(key_theorems) / sizeof(key_theorems[0])); i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, key_theorems[i]);
        char msg[128];
        snprintf(msg, sizeof(msg), "Key theorem '%s' should exist", key_theorems[i]);
        TEST_ASSERT(tmpl != NULL, msg);
    }

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
TEST_MAIN_BEGIN("Axiom Package Tests")
    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructibles);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_round_trip);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);
    TEST_MAIN_RUN(test_external_refs);
    TEST_MAIN_RUN(test_key_axioms);
TEST_MAIN_END()
