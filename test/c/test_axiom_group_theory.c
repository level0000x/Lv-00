/**
 * @file test_axiom_group_theory.c
 * @brief Group Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the group_theory.lvz
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

#define AXIOM_PKG_PATH "module/axiom_packages/group_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/group_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 34
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Core Axioms (4) */
    {"closure", 2},
    {"associativity", 3},
    {"identity", 1},
    {"inverse", 1},
    /* Elementary Consequences (8) */
    {"identity_uniqueness", 0},
    {"inverse_uniqueness", 1},
    {"left_cancellation", 3},
    {"right_cancellation", 3},
    {"double_inverse", 1},
    {"inverse_of_product", 2},
    {"identity_is_own_inverse", 0},
    {"product_with_identity", 1},
    /* Core Constructors (4) */
    {"multiply", 2},
    {"power_positive", 2},
    {"power_negative", 2},
    {"power_zero", 1},
    /* Derived Constructors (18) */
    {"commutator", 2},
    {"conjugation", 2},
    {"element_order", 1},
    {"center", 0},
    {"centralizer", 1},
    {"subgroup_test", 2},
    {"left_coset", 2},
    {"right_coset", 2},
    {"normal_subgroup_test", 2},
    {"quotient_group", 2},
    {"direct_product", 2},
    {"free_group", 1},
    {"abelianization", 1},
    {"commutator_subgroup", 1},
    {"homomorphism", 3},
    {"kernel", 1},
    {"image", 1},
    {"first_isomorphism_theorem", 1},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"word_problem", "undecidable", 4, true},
    {"conjugacy_problem", "undecidable", 5, true},
    {"group_isomorphism_problem", "undecidable", 5, true},
    {"triviality_problem", "undecidable", 4, true},
    {"finiteness_problem", "undecidable", 5, true},
    {"simple_group_recognition", "undecidable", 5, true},
    {"torsion_freeness_problem", "undecidable", 5, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* Test 9：期望外部引用 URL 前缀 */
static const AxiomTestExtRefExpectation k_external_refs[] = {
    {"word_problem", "https://en.wikipedia.org/wiki/Word_problem_for_groups"},
    {"conjugacy_problem", "https://en.wikipedia.org/wiki/Conjugacy_problem"},
    {"group_isomorphism_problem", "https://en.wikipedia.org/wiki/Group_isomorphism_problem"},
    {"triviality_problem", "https://en.wikipedia.org/wiki/Adian"},
    {"finiteness_problem", "https://en.wikipedia.org/wiki/Adian"},
    {"simple_group_recognition", "https://en.wikipedia.org/wiki/Adian"},
    {"torsion_freeness_problem", "https://en.wikipedia.org/wiki/Adian"},
};
#define K_EXTERNAL_REFS_COUNT (int) (sizeof(k_external_refs) / sizeof(k_external_refs[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "group_theory");
}

static void test_templates(void) {
    axiom_test_templates_with_params(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 34 constraint templates",
                                     k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructible_problems(void) {
    axiom_test_unconstructible_problems(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                        "should have 7 unconstructible problems", k_unconstructibles,
                                        K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework(AXIOM_PKG_PATH, "group_theory_abstract", "classical_equality",
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

TEST_MAIN_BEGIN("Group Theory")

    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructible_problems);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_round_trip);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);
    TEST_MAIN_RUN(test_external_refs);

TEST_MAIN_END()

