/**
 * @file test_axiom_cartesian_closed_category.c
 * @brief Cartesian Closed Category Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the
 * cartesian_closed_category.lvz axiom package. Validates template count,
 * unconstructible problem entries, logical framework settings, content
 * hashing, round-trip save/load, dependency validation, and negative lookups.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/cartesian_closed_category.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/cartesian_closed_category_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 55
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Foundational Category Structure (7) */
    {"object", 0},
    {"morphism", 2},
    {"identity_morphism", 1},
    {"composition", 3},
    {"associativity", 3},
    {"left_identity", 2},
    {"right_identity", 2},
    /* Group II: Terminal Object (3) */
    {"terminal_object", 0},
    {"terminal_morphism", 1},
    {"terminal_uniqueness", 2},
    /* Group III: Finite Products (12) */
    {"binary_product", 2},
    {"projection_left", 2},
    {"projection_right", 2},
    {"pairing", 2},
    {"projection_left_law", 2},
    {"projection_right_law", 2},
    {"pairing_uniqueness", 3},
    {"product_associator", 3},
    {"product_commutator", 2},
    {"product_unitor_left", 1},
    {"product_unitor_right", 1},
    {"diagonal", 1},
    {"swap", 2},
    /* Group IV: Exponential Objects (7) */
    {"exponential", 2},
    {"evaluation", 2},
    {"currying", 1},
    {"uncurrying", 1},
    {"beta_reduction", 1},
    {"eta_expansion", 1},
    {"exponential_uniqueness", 2},
    /* Group V: Internal Composition (6) */
    {"internal_composition", 3},
    {"identity_element", 1},
    {"precomposition", 2},
    {"postcomposition", 2},
    {"partial_application", 3},
    {"constant_function", 2},
    /* Group VI: Functorial Structure (5) */
    {"product_functor", 2},
    {"exponential_functor_contravariant", 2},
    {"exponential_functor_covariant", 2},
    {"currying_naturality", 3},
    {"currying_naturality_base", 3},
    /* Group VII: CCC Functor (4) */
    {"ccc_functor", 2},
    {"ccc_functor_preserves_terminal", 1},
    {"ccc_functor_preserves_products", 3},
    {"ccc_functor_preserves_exponentials", 3},
    /* Group VIII: Special Objects (3) */
    {"initial_object", 0},
    {"zero_object", 0},
    {"natural_numbers_object", 0},
    /* Group IX: Limits and Colimits (4) */
    {"equalizer", 2},
    {"coequalizer", 2},
    {"pullback", 2},
    {"pushout", 2},
    /* Group X: Internal Logic (6) */
    {"conjunction", 2},
    {"implication", 2},
    {"truth", 0},
    {"weakening", 2},
    {"contraction", 1},
    {"exchange", 2},
    /* Group XI: Sections (1) */
    {"object_of_sections", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"ccc_recognition_problem", "undecidable", 5, true},
    {"morphism_equality_free_ccc", "undecidable", 6, true},
    {"ccc_functor_preservation", "undecidable", 5, true},
    {"exponential_existence", "undecidable", 5, true},
    {"local_cartesian_closedness", "exponential_existence", 5, true},
    {"word_problem_free_ccc", "morphism_equality_free_ccc", 6, true},
    {"nno_existence", "undecidable", 4, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* Test 9：期望外部引用 URL 前缀 */
static const AxiomTestExtRefExpectation k_external_refs[] = {
    {"ccc_recognition_problem", "https://ncatlab.org/nlab/show/cartesian+closed+category"},
    {"morphism_equality_free_ccc", "https://en.wikipedia.org/wiki/Cartesian_closed_category"},
    {"ccc_functor_preservation", "https://ncatlab.org/nlab/show/cartesian+closed+functor"},
    {"exponential_existence", "https://ncatlab.org/nlab/show/exponential+object"},
    {"local_cartesian_closedness", "https://ncatlab.org/nlab/show/locally+cartesian+closed+category"},
    {"word_problem_free_ccc", "https://en.wikipedia.org/wiki/Curry"},
    {"nno_existence", "https://ncatlab.org/nlab/show/natural+numbers+object"},
};
#define K_EXTERNAL_REFS_COUNT (int) (sizeof(k_external_refs) / sizeof(k_external_refs[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "cartesian_closed_category");
}

static void test_templates(void) {
    axiom_test_templates_with_params(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 55 constraint templates",
                                     k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructible_problems(void) {
    axiom_test_unconstructible_problems(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                        "should have 7 unconstructible problems", k_unconstructibles,
                                        K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    /* CCC uses PROPOSITION_KIND_CONSTRUCTIVE contradiction behavior (minimal logic,
     * no ex falso quodlibet) */
    axiom_test_logical_framework(AXIOM_PKG_PATH, "directed_multigraph_with_products_and_exponentials",
                                 "exponential_to_terminal_A_implies_false", PROPOSITION_KIND_CONSTRUCTIVE,
                                 "PROPOSITION_KIND_CONSTRUCTIVE");
}

static void test_content_hash(void) {
    axiom_test_content_hash(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_round_trip(void) {
    axiom_test_round_trip(AXIOM_PKG_PATH, SAVE_TEST_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_dependency_validation(void) {
    axiom_test_dependency_validation(AXIOM_PKG_PATH, "FAIL (may occur for cross-reference reduces_to)", "");
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
    printf("Test 10: Verify template group coverage...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify key representative templates from each group exist */
    const char *key_templates[] = {
        /* Group I */ "object",
        "composition",
        "associativity",
        /* Group II */ "terminal_object",
        "terminal_morphism",
        /* Group III */ "binary_product",
        "projection_left",
        "pairing",
        /* Group IV */ "exponential",
        "evaluation",
        "currying",
        "beta_reduction",
        /* Group V */ "internal_composition",
        "precomposition",
        /* Group VI */ "product_functor",
        "exponential_functor_covariant",
        /* Group VII */ "ccc_functor",
        "ccc_functor_preserves_exponentials",
        /* Group VIII */ "natural_numbers_object",
        /* Group IX */ "equalizer",
        "pullback",
        /* Group X */ "conjunction",
        "implication",
        "truth",
        /* Group XI */ "object_of_sections",
    };

    int num_groups = (int) (sizeof(key_templates) / sizeof(key_templates[0]));
    int groups_found = 0;

    for (int i = 0; i < num_groups; i++) {
        ConstraintTemplate *t = axiom_package_get_template(pkg, key_templates[i]);
        if (t) {
            groups_found++;
        } else {
            printf("  MISSING key template: '%s'\n", key_templates[i]);
            g_fail_count++;
        }
    }

    TEST_ASSERT(groups_found == num_groups, "all key group representatives should be found");
    printf("  Key template coverage: %d / %d groups represented\n", groups_found, num_groups);

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Cartesian Closed Category");

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
