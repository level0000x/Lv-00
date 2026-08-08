/**
 * @file test_axiom_linear_algebra.c
 * @brief Linear Algebra Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the linear_algebra.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Linear algebra formalizes vector spaces over a field, linear maps,
 * matrix algebra, determinant theory, eigenvalue theory, inner product
 * spaces, dual spaces, and tensor products. The 90 templates cover 12
 * groups spanning the core axioms through advanced constructions.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/linear_algebra.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/linear_algebra_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 90
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Vector Space Axioms (8) */
    {"vector_addition_associativity", 3},
    {"vector_addition_commutativity", 2},
    {"vector_additive_identity", 1},
    {"vector_additive_inverse", 1},
    {"scalar_multiplication_compatibility", 3},
    {"scalar_identity", 1},
    {"scalar_distributivity_vector", 3},
    {"scalar_distributivity_field", 3},
    /* Group II: Derived Properties (6) */
    {"zero_scalar_annihilates", 1},
    {"zero_vector_annihilates", 1},
    {"negation_via_scalar", 1},
    {"zero_divisor_property", 2},
    {"vector_subtraction", 2},
    {"scalar_subtraction", 3},
    /* Group III: Subspaces and Quotient Spaces (6) */
    {"subspace_test", 2},
    {"span", 1},
    {"linear_independence", 2},
    {"linear_dependence", 2},
    {"quotient_space", 2},
    {"direct_sum", 2},
    /* Group IV: Basis and Dimension (8) */
    {"basis", 1},
    {"dimension", 1},
    {"finite_dimensional", 1},
    {"infinite_dimensional", 1},
    {"dimension_theorem", 2},
    {"rank_nullity_theorem", 2},
    {"basis_extension", 2},
    {"steinitz_exchange", 2},
    /* Group V: Linear Maps (10) */
    {"linear_map", 2},
    {"linear_map_composition", 3},
    {"kernel", 1},
    {"image", 1},
    {"injectivity_criterion", 1},
    {"surjectivity_criterion", 1},
    {"isomorphism", 2},
    {"hom_space", 2},
    {"endomorphism_ring", 1},
    {"general_linear_group", 1},
    /* Group VI: Matrix Algebra (10) */
    {"matrix", 2},
    {"matrix_addition", 2},
    {"matrix_multiplication", 2},
    {"scalar_matrix_multiplication", 2},
    {"matrix_transpose", 1},
    {"identity_matrix", 1},
    {"zero_matrix", 2},
    {"matrix_inverse", 1},
    {"matrix_trace", 1},
    {"matrix_rank", 1},
    /* Group VII: Determinant Theory (6) */
    {"determinant", 1},
    {"determinant_multiplicative", 2},
    {"determinant_transpose", 1},
    {"cofactor_expansion", 1},
    {"cramers_rule", 2},
    {"adjugate_matrix", 1},
    /* Group VIII: Eigenvalue Theory (8) */
    {"eigenvalue", 2},
    {"eigenvector", 2},
    {"characteristic_polynomial", 1},
    {"eigenspace", 2},
    {"algebraic_multiplicity", 2},
    {"geometric_multiplicity", 2},
    {"diagonalizability", 1},
    {"cayley_hamilton", 1},
    /* Group IX: Inner Product Spaces (8) */
    {"inner_product", 2},
    {"conjugate_symmetry", 2},
    {"inner_product_linearity", 3},
    {"positive_definiteness", 1},
    {"norm_from_inner_product", 1},
    {"orthogonality", 2},
    {"orthogonal_complement", 1},
    {"gram_schmidt", 1},
    /* Group X: Dual Spaces and Tensor Products (8) */
    {"dual_space", 1},
    {"dual_basis", 1},
    {"double_dual", 1},
    {"annihilator", 1},
    {"tensor_product", 2},
    {"tensor_universal_property", 3},
    {"exterior_product", 2},
    {"symmetric_product", 2},
    /* Group XI: Canonical Forms (6) */
    {"jordan_normal_form", 1},
    {"rational_canonical_form", 1},
    {"singular_value_decomposition", 1},
    {"qr_decomposition", 1},
    {"lu_decomposition", 1},
    {"spectral_theorem", 1},
    /* Group XII: Advanced Constructions (6) */
    {"direct_product", 2},
    {"external_direct_sum", 2},
    {"induced_quotient_map", 2},
    {"pullback", 3},
    {"pushout", 3},
    {"multilinear_map", 3},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"matrix_mortality_problem", "undecidable", 3, true},
    {"matrix_semigroup_membership", "undecidable", 3, true},
    {"matrix_semigroup_equality", "undecidable", 2, true},
    {"matrix_nilpotency_problem", "undecidable", 4, true},
    {"basis_existence_infinite_dimensional", "requires_axiom_of_choice", 3, true},
    {"vector_space_isomorphism_problem", "undecidable", 4, true},
    {"tensor_rank_problem", "np_hard", 2, true},
    {"eigenvalue_sensitivity_nonnormal", "numerically_unstable", 4, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "linear_algebra");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 90 constraint templates", k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructibles(void) {
    axiom_test_unconstructibles_min_deps(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                         "should have 8 unconstructible problems", k_unconstructibles,
                                         K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework_checked(AXIOM_PKG_PATH, "Test 4: Verify logical framework settings...",
                                         "vector_space_over_field", "classical_equality",
                                         PROPOSITION_KIND_EXPLOSION_PRINCIPLE, "PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
}

static void test_content_hash(void) {
    axiom_test_content_hash_deterministic(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_save_load_roundtrip(void) {
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "linear_algebra", EXPECTED_TEMPLATE_COUNT,
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
    printf("Test 10: Key linear algebra templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core vector space axioms */
    const char *core_axioms[] = {
        "vector_addition_associativity", "vector_addition_commutativity",       "vector_additive_identity",
        "vector_additive_inverse",       "scalar_multiplication_compatibility", "scalar_identity",
        "scalar_distributivity_vector",  "scalar_distributivity_field"};

    for (int i = 0; i < 8; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_axioms[i]);
        TEST_ASSERT(tmpl != NULL, "core vector space axiom template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Linear map essentials */
    const char *map_ops[] = {"linear_map", "kernel", "image", "isomorphism"};
    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, map_ops[i]);
        TEST_ASSERT(tmpl != NULL, "linear map template should exist");
    }

    /* Matrix algebra essentials */
    const char *matrix_ops[] = {"matrix", "matrix_multiplication", "matrix_inverse", "determinant", "matrix_transpose"};
    for (int i = 0; i < 5; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, matrix_ops[i]);
        TEST_ASSERT(tmpl != NULL, "matrix algebra template should exist");
    }

    /* Eigenvalue theory essentials */
    const char *eigen_ops[] = {"eigenvalue", "eigenvector", "characteristic_polynomial", "cayley_hamilton",
                               "diagonalizability"};
    for (int i = 0; i < 5; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, eigen_ops[i]);
        TEST_ASSERT(tmpl != NULL, "eigenvalue theory template should exist");
    }

    /* Inner product space essentials */
    const char *ip_ops[] = {"inner_product", "orthogonality", "gram_schmidt", "norm_from_inner_product"};
    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, ip_ops[i]);
        TEST_ASSERT(tmpl != NULL, "inner product space template should exist");
    }

    /* Tensor product essentials */
    ConstraintTemplate *tp = axiom_package_get_template(pkg, "tensor_product");
    TEST_ASSERT(tp != NULL, "tensor_product template should exist");
    ConstraintTemplate *tup = axiom_package_get_template(pkg, "tensor_universal_property");
    TEST_ASSERT(tup != NULL, "tensor_universal_property template should exist");

    /* Canonical forms */
    const char *cf_ops[] = {"jordan_normal_form", "spectral_theorem", "singular_value_decomposition"};
    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, cf_ops[i]);
        TEST_ASSERT(tmpl != NULL, "canonical form template should exist");
    }

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Linear Algebra Axiom Package Tests")
    TEST_SUITE_BEGIN("Linear Algebra");
    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructibles);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_save_load_roundtrip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_references);
    TEST_RUN(test_key_templates);
    TEST_SUMMARY();
TEST_MAIN_END()
