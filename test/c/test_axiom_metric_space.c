/**
 * @file test_axiom_metric_space.c
 * @brief Metric Space Theory Axiom Package Test
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

#define AXIOM_PKG_PATH "module/axiom_packages/metric_space.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/metric_space_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 47
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"isometric_embedding_into_l2", "gram_matrix_positive_semi_definiteness", 3, true},
    {"separable_metric_space_classification", "uncountable_isometry_classes", 3, true},
    {"urysohn_universal_space_existence", "requires_axiom_of_choice", 3, true},
    {"finite_metric_space_isometry", "graph_isomorphism", 4, true},
    {"finite_metric_embedding_into_Rn", "NP_hard_optimization", 3, true},
    {"hausdorff_distance_computability", "non_computable_in_computable_analysis", 3, true},
    {"baire_category_without_choice", "requires_dependent_choice", 3, true},
    {"general_metrizability", "nagata_smirnov_conditions", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "metric_space");
}

/* Test 2：约束模板（文件特有：仅名称数组 + 尾部参数校验，保留原体） */
static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 47 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    const char *expected_templates[] = {
        /* Group I: Core Metric Axioms */
        "metric_non_negativity", "metric_symmetry", "triangle_inequality", "identity_of_indiscernibles",
        /* Group II: Basic Metric Constructions */
        "open_ball", "closed_ball", "sphere", "point_set_distance", "set_set_distance", "diameter",
        /* Group III: Metric Topology Constructions */
        "metric_open_set", "metric_closed_set", "interior", "closure", "boundary", "hausdorff_separation",
        /* Group IV: Convergence & Completeness */
        "sequence_convergence", "cauchy_sequence", "completeness", "cauchy_completion", "banach_fixed_point",
        "baire_category_theorem",
        /* Group V: Continuity */
        "pointwise_continuity", "uniform_continuity", "lipschitz_continuity", "contraction_map", "isometry",
        "uniform_extension_to_completion",
        /* Group VI: Compactness & Boundedness */
        "bounded_set", "totally_bounded", "sequential_compactness", "compact_equals_complete_totally_bounded",
        "lebesgue_number_lemma", "arzela_ascoli",
        /* Group VII: Connectedness */
        "connected_set", "path_connected_set", "connected_component",
        /* Group VIII: Product & Quotient Constructions */
        "product_metric_linf", "product_metric_l2", "product_metric_l1", "quotient_metric",
        /* Group IX: Specialized Constructions */
        "hausdorff_distance", "subspace_metric", "discrete_metric", "euclidean_metric_Rn", "sup_metric",
        "weighted_metric", NULL};

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

    /* Core axioms: metric_non_negativity(2), metric_symmetry(2),
       triangle_inequality(3), identity_of_indiscernibles(2) */
    t = axiom_package_get_template(pkg, "metric_non_negativity");
    TEST_ASSERT(t && t->param_count == 2, "metric_non_negativity should have 2 params");
    t = axiom_package_get_template(pkg, "metric_symmetry");
    TEST_ASSERT(t && t->param_count == 2, "metric_symmetry should have 2 params");
    t = axiom_package_get_template(pkg, "triangle_inequality");
    TEST_ASSERT(t && t->param_count == 3, "triangle_inequality should have 3 params");
    t = axiom_package_get_template(pkg, "identity_of_indiscernibles");
    TEST_ASSERT(t && t->param_count == 2, "identity_of_indiscernibles should have 2 params");

    /* Constructions: open_ball(2), closed_ball(2), sphere(2) */
    t = axiom_package_get_template(pkg, "open_ball");
    TEST_ASSERT(t && t->param_count == 2, "open_ball should have 2 params");
    t = axiom_package_get_template(pkg, "closed_ball");
    TEST_ASSERT(t && t->param_count == 2, "closed_ball should have 2 params");
    t = axiom_package_get_template(pkg, "sphere");
    TEST_ASSERT(t && t->param_count == 2, "sphere should have 2 params");

    /* Topology: metric_open_set(1), closure(1), boundary(1) */
    t = axiom_package_get_template(pkg, "metric_open_set");
    TEST_ASSERT(t && t->param_count == 1, "metric_open_set should have 1 param");
    t = axiom_package_get_template(pkg, "closure");
    TEST_ASSERT(t && t->param_count == 1, "closure should have 1 param");
    t = axiom_package_get_template(pkg, "boundary");
    TEST_ASSERT(t && t->param_count == 1, "boundary should have 1 param");

    /* Convergence: sequence_convergence(3), cauchy_sequence(2),
       completeness(1), cauchy_completion(1) */
    t = axiom_package_get_template(pkg, "sequence_convergence");
    TEST_ASSERT(t && t->param_count == 3, "sequence_convergence should have 3 params");
    t = axiom_package_get_template(pkg, "cauchy_sequence");
    TEST_ASSERT(t && t->param_count == 2, "cauchy_sequence should have 2 params");
    t = axiom_package_get_template(pkg, "completeness");
    TEST_ASSERT(t && t->param_count == 1, "completeness should have 1 param");
    t = axiom_package_get_template(pkg, "cauchy_completion");
    TEST_ASSERT(t && t->param_count == 1, "cauchy_completion should have 1 param");

    /* Banach fixed point: banach_fixed_point(2) */
    t = axiom_package_get_template(pkg, "banach_fixed_point");
    TEST_ASSERT(t && t->param_count == 2, "banach_fixed_point should have 2 params");

    /* Continuity: pointwise_continuity(3), uniform_continuity(2),
       isometry(1) */
    t = axiom_package_get_template(pkg, "pointwise_continuity");
    TEST_ASSERT(t && t->param_count == 3, "pointwise_continuity should have 3 params");
    t = axiom_package_get_template(pkg, "uniform_continuity");
    TEST_ASSERT(t && t->param_count == 2, "uniform_continuity should have 2 params");
    t = axiom_package_get_template(pkg, "isometry");
    TEST_ASSERT(t && t->param_count == 1, "isometry should have 1 param");

    /* Compactness: totally_bounded(2), sequential_compactness(1),
       arzela_ascoli(3) */
    t = axiom_package_get_template(pkg, "totally_bounded");
    TEST_ASSERT(t && t->param_count == 2, "totally_bounded should have 2 params");
    t = axiom_package_get_template(pkg, "sequential_compactness");
    TEST_ASSERT(t && t->param_count == 1, "sequential_compactness should have 1 param");
    t = axiom_package_get_template(pkg, "arzela_ascoli");
    TEST_ASSERT(t && t->param_count == 3, "arzela_ascoli should have 3 params");

    /* Product: product_metric_linf(2), product_metric_l2(2),
       product_metric_l1(2), quotient_metric(2) */
    t = axiom_package_get_template(pkg, "product_metric_linf");
    TEST_ASSERT(t && t->param_count == 2, "product_metric_linf should have 2 params");
    t = axiom_package_get_template(pkg, "product_metric_l2");
    TEST_ASSERT(t && t->param_count == 2, "product_metric_l2 should have 2 params");
    t = axiom_package_get_template(pkg, "product_metric_l1");
    TEST_ASSERT(t && t->param_count == 2, "product_metric_l1 should have 2 params");
    t = axiom_package_get_template(pkg, "quotient_metric");
    TEST_ASSERT(t && t->param_count == 2, "quotient_metric should have 2 params");

    /* Specialized: hausdorff_distance(2), discrete_metric(1),
       euclidean_metric_Rn(1), sup_metric(1), weighted_metric(2) */
    t = axiom_package_get_template(pkg, "hausdorff_distance");
    TEST_ASSERT(t && t->param_count == 2, "hausdorff_distance should have 2 params");
    t = axiom_package_get_template(pkg, "discrete_metric");
    TEST_ASSERT(t && t->param_count == 1, "discrete_metric should have 1 param");
    t = axiom_package_get_template(pkg, "euclidean_metric_Rn");
    TEST_ASSERT(t && t->param_count == 1, "euclidean_metric_Rn should have 1 param");
    t = axiom_package_get_template(pkg, "sup_metric");
    TEST_ASSERT(t && t->param_count == 1, "sup_metric should have 1 param");
    t = axiom_package_get_template(pkg, "weighted_metric");
    TEST_ASSERT(t && t->param_count == 2, "weighted_metric should have 2 params");

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void) {
    axiom_test_unconstructible_problems(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                        "should have 8 unconstructible problems", k_unconstructibles,
                                        K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework(AXIOM_PKG_PATH, "metric_space_general", "classical_distance_negation",
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

TEST_MAIN_BEGIN("Metric Space Theory Axiom Package Test Suite")
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Testing: axiom_packages/metric_space.lvz ===\n\n");
    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructible_problems);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_round_trip);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);
TEST_MAIN_END()
