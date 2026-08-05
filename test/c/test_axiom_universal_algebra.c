/**
 * @file test_axiom_universal_algebra.c
 * @brief Universal Algebra Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the universal_algebra.lvz
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

#define AXIOM_PKG_PATH "module/axiom_packages/universal_algebra.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/universal_algebra_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 60
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Core Axioms — Signature and Algebra Definition (10) */
    {"signature", 1},
    {"term_algebra", 2},
    {"substitution", 3},
    {"equational_satisfaction", 2},
    {"congruence", 2},
    {"quotient_algebra", 2},
    {"homomorphism", 3},
    {"subalgebra", 2},
    {"direct_product", 2},
    {"free_algebra", 2},
    /* Birkhoff's HSP Theorem (3) */
    {"homomorphic_image", 2},
    {"subalgebra_closure", 1},
    {"product_closure", 1},
    /* Congruence Theory (6) */
    {"congruence_identity", 1},
    {"congruence_total", 1},
    {"congruence_meet", 2},
    {"congruence_join", 2},
    {"congruence_lattice", 1},
    {"factor_theorem", 2},
    /* Isomorphism Theorems (4) */
    {"first_isomorphism_theorem", 2},
    {"second_isomorphism_theorem", 2},
    {"third_isomorphism_theorem", 2},
    {"correspondence_theorem", 2},
    /* Variety Theory (6) */
    {"equational_class", 1},
    {"hsp_theorem", 1},
    {"free_algebra_universal_property", 3},
    {"subdirect_representation", 1},
    {"subdirectly_irreducible", 1},
    {"equational_basis", 1},
    /* Mal'cev Conditions (6) */
    {"malcev_term", 1},
    {"congruence_permutability", 2},
    {"congruence_modularity", 3},
    {"congruence_distributivity", 3},
    {"jonsson_terms", 1},
    {"day_terms", 1},
    /* Term Rewriting and Equational Deduction (5) */
    {"equational_deduction", 2},
    {"term_rewriting", 2},
    {"confluence", 1},
    {"termination", 1},
    {"knuth_bendix_completion", 1},
    /* Core Constructors (8) */
    {"apply_operation", 2},
    {"build_term", 2},
    {"evaluate_term", 2},
    {"form_quotient", 2},
    {"form_homomorphism", 3},
    {"form_subalgebra", 2},
    {"form_product", 2},
    {"form_free_algebra", 2},
    /* Derived Constructors (12) */
    {"congruence_generation", 2},
    {"kernel", 1},
    {"image", 1},
    {"isomorphism", 2},
    {"endomorphism", 1},
    {"automorphism", 1},
    {"subdirect_embedding", 2},
    {"ultrafilter_construction", 2},
    {"clone", 1},
    {"polynomial_clone", 1},
    {"variety_membership", 2},
    {"equational_consequence", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"word_problem_for_varieties", "undecidable", 4, true},
    {"equational_theory_equivalence", "undecidable", 4, true},
    {"finite_basis_problem", "undecidable", 3, true},
    {"variety_equivalence", "undecidable", 3, true},
    {"congruence_lattice_recognition", "undecidable", 3, false},
    {"free_algebra_finiteness", "undecidable", 3, false},
    {"knuth_bendix_completion_termination", "undecidable", 4, true},
    {"equational_unification", "undecidable", 4, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* Test 9：期望外部引用 URL 前缀 */
static const AxiomTestExtRefExpectation k_external_refs[] = {
    {"word_problem_for_varieties", "https://en.wikipedia.org/wiki/Word_problem"},
    {"equational_theory_equivalence", "https://en.wikipedia.org/wiki/Word_problem"},
    {"finite_basis_problem", "https://en.wikipedia.org/wiki/Universal_algebra"},
    {"variety_equivalence", "https://en.wikipedia.org/wiki/Variety"},
    {"congruence_lattice_recognition", "https://en.wikipedia.org/wiki/Universal_algebra"},
    {"free_algebra_finiteness", "https://en.wikipedia.org/wiki/Universal_algebra"},
    {"knuth_bendix_completion_termination", "https://en.wikipedia.org/wiki/Word_problem"},
    {"equational_unification", "https://en.wikipedia.org/wiki/Word_problem"},
};
#define K_EXTERNAL_REFS_COUNT (int) (sizeof(k_external_refs) / sizeof(k_external_refs[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "universal_algebra");
}

static void test_templates(void) {
    axiom_test_templates_with_params(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 60 constraint templates",
                                     k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructible_problems(void) {
    axiom_test_unconstructible_problems(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                        "should have 8 unconstructible problems", k_unconstructibles,
                                        K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework(AXIOM_PKG_PATH, "universal_algebra_equational", "equational_equality",
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

static void test_template_categories(void) {
    printf("Test 10: Verify template categories are complete...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify key category templates exist */
    const char *category_templates[] = {
        /* HSP */
        "homomorphic_image",
        "subalgebra_closure",
        "product_closure",
        /* Congruence */
        "congruence_identity",
        "congruence_total",
        "congruence_lattice",
        /* Isomorphism theorems */
        "first_isomorphism_theorem",
        "second_isomorphism_theorem",
        "third_isomorphism_theorem",
        /* Mal'cev conditions */
        "malcev_term",
        "congruence_permutability",
        "congruence_modularity",
        "congruence_distributivity",
        /* Variety */
        "hsp_theorem",
        "subdirect_representation",
        "subdirectly_irreducible",
        "equational_basis",
        /* Term rewriting */
        "confluence",
        "termination",
        "knuth_bendix_completion",
    };

    int cat_total = (int) (sizeof(category_templates) / sizeof(category_templates[0]));
    int cat_found = 0;
    for (int i = 0; i < cat_total; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, category_templates[i]);
        if (tmpl)
            cat_found++;
    }

    TEST_ASSERT(cat_found == cat_total, "all category templates should be found");
    printf("  Category templates: %d / %d found\n", cat_found, cat_total);

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Universal Algebra")

    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructible_problems);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_round_trip);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);
    TEST_MAIN_RUN(test_external_refs);
    TEST_MAIN_RUN(test_template_categories);

TEST_MAIN_END()

