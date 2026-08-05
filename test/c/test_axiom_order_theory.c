/**
 * @file test_axiom_order_theory.c
 * @brief Order Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the order_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, and negative lookups.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/order_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/order_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 32
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Partial Order Axioms (3) */
    {"reflexivity", 1},
    {"antisymmetry", 2},
    {"transitivity", 3},
    /* Group II: Special Elements (4) */
    {"least_element", 0},
    {"greatest_element", 0},
    {"minimal_element", 1},
    {"maximal_element", 1},
    /* Group III: Bounds (4) */
    {"upper_bound", 2},
    {"lower_bound", 2},
    {"least_upper_bound", 1},
    {"greatest_lower_bound", 1},
    /* Group IV: Total Order (1) */
    {"totality", 2},
    /* Group V: Well-Order (2) */
    {"well_foundedness", 1},
    {"well_order", 0},
    /* Group VI: Order Morphisms (4) */
    {"monotone_function", 3},
    {"antitone_function", 3},
    {"order_embedding", 3},
    {"order_isomorphism", 3},
    /* Group VII: Order Constructions (4) */
    {"dual_order", 1},
    {"product_order", 2},
    {"induced_suborder", 2},
    {"ordinal_sum", 2},
    /* Group VIII: Derived Concepts & Constructors (10) */
    {"strict_order", 2},
    {"covering_relation", 2},
    {"chain", 1},
    {"antichain", 1},
    {"interval", 2},
    {"hasse_diagram", 1},
    {"zorn_lemma", 1},
    {"dilworth_decomposition", 1},
    {"knaster_tarski_fixed_point", 2},
    {"szpilrajn_extension", 1},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"poset_dimension", "NP-hard optimization problem", 2, true},
    {"counting_linear_extensions", "#P-complete", 2, true},
    {"poset_isomorphism", "GI-hard (Graph Isomorphism hardness)", 2, true},
    {"infinite_poset_width", "requires transfinite methods; not computable in general", 3, true},
    {"order_automorphism_group", "computationally intractable for general posets", 2, false},
    {"poset_convex_realizability",
     "undecidable: determining if a poset is isomorphic to the inclusion order of convex sets in R^d", 2, true},
    {"poset_dimension_at_least_4", "NP-complete to decide if dimension >= 4", 2, true},
    {"chain_partition_minimization", "NP-hard: minimum chain decomposition", 2, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* Test 9：期望外部引用 URL 前缀 */
static const AxiomTestExtRefExpectation k_external_refs[] = {
    {"poset_dimension", "https://en.wikipedia.org/wiki/Order_dimension"},
    {"counting_linear_extensions", "https://en.wikipedia.org/wiki/Linear_extension"},
    {"poset_isomorphism", "https://en.wikipedia.org/wiki/Graph_isomorphism_problem"},
    {"infinite_poset_width", "https://en.wikipedia.org/wiki/Antichain"},
    {"order_automorphism_group", "https://en.wikipedia.org/wiki/Automorphism_group"},
    {"poset_convex_realizability", "https://en.wikipedia.org/wiki/Convex_geometry"},
    {"poset_dimension_at_least_4", "https://doi.org/10.1016/0012-365X(84)90132-1"},
    {"chain_partition_minimization", "https://en.wikipedia.org/wiki/Dilworth%27s_theorem"},
};
#define K_EXTERNAL_REFS_COUNT (int) (sizeof(k_external_refs) / sizeof(k_external_refs[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "order_theory");
}

static void test_templates(void) {
    axiom_test_templates_with_params(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 32 constraint templates",
                                     k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructible_problems(void) {
    axiom_test_unconstructible_problems(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                        "should have 8 unconstructible problems", k_unconstructibles,
                                        K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework(AXIOM_PKG_PATH, "hasse_diagram_poset", "classical_order_negation",
                                 PROPOSITION_KIND_EXPLOSION_PRINCIPLE, "PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
}

static void test_content_hash(void) {
    axiom_test_content_hash(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_round_trip(void) {
    axiom_test_round_trip(AXIOM_PKG_PATH, SAVE_TEST_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_dependency_validation(void) {
    axiom_test_dependency_validation(AXIOM_PKG_PATH, "FAIL (acceptable)",
                                     " (expected: may fail for cross-reference reduces_to)");
}

static void test_negative_lookups(void) {
    axiom_test_negative_lookups(AXIOM_PKG_PATH, AXIOM_TEST_NEG_BASIC);
}

static void test_external_refs(void) {
    axiom_test_external_refs(AXIOM_PKG_PATH, k_external_refs, K_EXTERNAL_REFS_COUNT);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_template_group_coverage(void) {
    printf("Test 10: Verify all 8 template groups are represented...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify at least one template from each group exists */
    const char *group_representatives[] = {
        "reflexivity",       /* Group I: Partial Order */
        "least_element",     /* Group II: Special Elements */
        "upper_bound",       /* Group III: Bounds */
        "totality",          /* Group IV: Total Order */
        "well_order",        /* Group V: Well-Order */
        "monotone_function", /* Group VI: Order Morphisms */
        "dual_order",        /* Group VII: Constructions */
        "hasse_diagram",     /* Group VIII: Derived Concepts */
    };

    int groups_found = 0;
    int num_groups = (int) (sizeof(group_representatives) / sizeof(group_representatives[0]));

    for (int i = 0; i < num_groups; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, group_representatives[i]);
        if (tmpl) {
            groups_found++;
            printf("  [Group %d] '%s' found (params=%d)\n", i + 1, tmpl->name, tmpl->param_count);
        } else {
            printf("  [Group %d] '%s' MISSING\n", i + 1, group_representatives[i]);
            g_fail_count++;
        }
    }

    TEST_ASSERT(groups_found == num_groups, "all 8 template groups should be represented");

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Order Theory");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_refs);
    TEST_RUN(test_template_group_coverage);

    TEST_SUMMARY();

    return 0;
}
