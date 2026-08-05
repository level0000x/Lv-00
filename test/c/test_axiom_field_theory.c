/**
 * @file test_axiom_field_theory.c
 * @brief Field Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the field_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Field theory extends ring theory with multiplicative inverses for
 * non-zero elements. The 37 templates cover field axioms, operations,
 * subfields and extensions, Galois theory, special fields, and
 * polynomial-related constructions.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/field_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/field_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 37
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Field Axioms (11) */
    {"addition_associativity", 3},
    {"addition_commutativity", 2},
    {"additive_identity", 1},
    {"additive_inverse", 1},
    {"multiplication_associativity", 3},
    {"multiplicative_identity", 1},
    {"multiplication_commutativity", 2},
    {"distributivity", 3},
    {"multiplicative_inverse", 1},
    {"zero_not_one", 0},
    {"multiplication_cancellation", 3},
    /* Group II: Field Operations (6) */
    {"addition", 2},
    {"subtraction", 2},
    {"multiplication", 2},
    {"division", 2},
    {"negation", 1},
    {"reciprocal", 1},
    /* Group III: Subfields and Extensions (5) */
    {"subfield_test", 2},
    {"field_extension", 2},
    {"algebraic_element", 2},
    {"transcendental_element", 2},
    {"field_tower", 3},
    /* Group IV: Galois Theory (6) */
    {"automorphism_group", 2},
    {"galois_group", 2},
    {"fixed_field", 2},
    {"galois_extension", 2},
    {"galois_correspondence", 2},
    {"normal_extension", 2},
    /* Group V: Special Fields (4) */
    {"finite_field", 2},
    {"prime_field", 1},
    {"algebraic_closure", 1},
    {"real_closure", 1},
    /* Group VI: Polynomial Constructions (5) */
    {"polynomial_ring", 1},
    {"irreducible_polynomial", 1},
    {"minimal_polynomial", 2},
    {"polynomial_root", 2},
    {"degree_of_extension", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"polynomial_root_by_radicals", "abel_ruffini_theorem", 4, true},
    {"galois_group_computation", "undecidable", 4, true},
    {"field_isomorphism_problem", "undecidable", 3, true},
    {"algebraic_closure_uniqueness", "undecidable", 3, true},
    {"transcendence_degree_basis", "undecidable", 3, true},
    {"field_embedding_existence", "undecidable", 4, true},
    {"inverse_galois_problem", "undecidable", 5, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "field_theory");
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

static void test_logical_framework(void) {
    axiom_test_logical_framework_checked(AXIOM_PKG_PATH, "Test 4: Verify logical framework settings...",
                                         "field_theory_abstract", "classical_equality",
                                         PROPOSITION_KIND_EXPLOSION_PRINCIPLE, "PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
}

static void test_content_hash(void) {
    axiom_test_content_hash_deterministic(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_save_load_roundtrip(void) {
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "field_theory", EXPECTED_TEMPLATE_COUNT,
                                    EXPECTED_UNCONSTRUCTIBLE_COUNT, AXIOM_TEST_FREE_LV_FREE);
}

static void test_dependency_validation(void) {
    axiom_test_dependency_validation_note(AXIOM_PKG_PATH,
                                          "(identifier references to external concepts are expected)");
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
    printf("Test 10: Key field theory templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core field axioms: additive and multiplicative structure */
    const char *core_axioms[] = {"addition_associativity",
                                 "addition_commutativity",
                                 "additive_identity",
                                 "additive_inverse",
                                 "multiplication_associativity",
                                 "multiplicative_identity",
                                 "multiplication_commutativity",
                                 "distributivity",
                                 "multiplicative_inverse",
                                 "zero_not_one"};

    for (int i = 0; i < 10; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_axioms[i]);
        TEST_ASSERT(tmpl != NULL, "core field axiom template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Field operations */
    const char *core_ops[] = {"addition", "subtraction", "multiplication", "division"};

    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_ops[i]);
        TEST_ASSERT(tmpl != NULL, "core field operation template should exist");
    }

    /* Galois theory essentials */
    ConstraintTemplate *gg = axiom_package_get_template(pkg, "galois_group");
    TEST_ASSERT(gg != NULL, "galois_group template should exist");
    ConstraintTemplate *ge = axiom_package_get_template(pkg, "galois_extension");
    TEST_ASSERT(ge != NULL, "galois_extension template should exist");
    ConstraintTemplate *gc = axiom_package_get_template(pkg, "galois_correspondence");
    TEST_ASSERT(gc != NULL, "galois_correspondence template should exist");

    /* Polynomial constructions */
    ConstraintTemplate *mp = axiom_package_get_template(pkg, "minimal_polynomial");
    TEST_ASSERT(mp != NULL, "minimal_polynomial template should exist");
    ConstraintTemplate *de = axiom_package_get_template(pkg, "degree_of_extension");
    TEST_ASSERT(de != NULL, "degree_of_extension template should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Field Theory")

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

