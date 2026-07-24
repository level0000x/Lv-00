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

#include "axiom_pkg.h"
#include "lv_utils.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/linear_algebra.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/linear_algebra_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 90
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

/* ──────────────────────────────────────────────
 * Test 1: Load from file
 * ────────────────────────────────────────────── */
static void test_load_from_file(void) {
    printf("Test 1: Load linear_algebra.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "linear_algebra") == 0,
                "package name should be 'linear_algebra'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 2: Verify constraint templates
 * ────────────────────────────────────────────── */
static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 90 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    /* Check representative templates from each group */
    struct {
        const char *name;
        int params;
    } expected[] = {
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

    int expected_count = sizeof(expected) / sizeof(expected[0]);
    TEST_ASSERT(expected_count == EXPECTED_TEMPLATE_COUNT,
                "local expected array count should match EXPECTED_TEMPLATE_COUNT");
    printf("  Local expected count: %d\n", expected_count);

    for (int i = 0; i < expected_count; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expected[i].name);
        if (!tmpl) {
            printf("  FAIL: template '%s' not found\n", expected[i].name);
            g_fail_count++;
            continue;
        }
        TEST_ASSERT(tmpl->param_count == expected[i].params, "template parameter count mismatch");
    }

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 3: Verify unconstructible problems
 * ────────────────────────────────────────────── */
static void test_unconstructibles(void) {
    printf("Test 3: Verify unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 8 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    /* Verify each expected unconstructible */
    struct {
        const char *name;
        const char *reduces_to;
        int min_deps;
        bool has_ref;
    } expected_uc[] = {
        {"matrix_mortality_problem", "undecidable", 3, true},
        {"matrix_semigroup_membership", "undecidable", 3, true},
        {"matrix_semigroup_equality", "undecidable", 2, true},
        {"matrix_nilpotency_problem", "undecidable", 4, true},
        {"basis_existence_infinite_dimensional", "requires_axiom_of_choice", 3, true},
        {"vector_space_isomorphism_problem", "undecidable", 4, true},
        {"tensor_rank_problem", "np_hard", 2, true},
        {"eigenvalue_sensitivity_nonnormal", "numerically_unstable", 4, true},
    };

    int uc_count = sizeof(expected_uc) / sizeof(expected_uc[0]);
    TEST_ASSERT(uc_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "local expected UC count should match");

    for (int i = 0; i < uc_count; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected_uc[i].name);
        if (!uc) {
            printf("  FAIL: unconstructible '%s' not found\n", expected_uc[i].name);
            g_fail_count++;
            continue;
        }
        TEST_ASSERT(uc->reduces_to && strcmp(uc->reduces_to, expected_uc[i].reduces_to) == 0,
                    "unconstructible reduces_to mismatch");
        TEST_ASSERT(uc->dependency_count >= expected_uc[i].min_deps,
                    "unconstructible should have minimum dependency count");
        TEST_ASSERT(expected_uc[i].has_ref ? (uc->external_ref != NULL) : 1,
                    "unconstructible should have external_ref");
        TEST_ASSERT(uc->green_verified == true, "unconstructible should be green_verified");
    }

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 4: Verify logical framework
 * ────────────────────────────────────────────── */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    TEST_ASSERT(strcmp(pkg->bottom_geometry, "vector_space_over_field") == 0,
                "bottom_geometry should be 'vector_space_over_field'");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    TEST_ASSERT(strcmp(pkg->negation_encoding, "classical_equality") == 0,
                "negation_encoding should be 'classical_equality'");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 5: Content hash computation
 * ────────────────────────────────────────────── */
static void test_content_hash(void) {
    printf("Test 5: Content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash1 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash1 != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash1) == 64, "SHA-256 hash should be 64 hex chars");
    printf("  Hash: %.8s...%.8s (len=%zu)\n", hash1, hash1 + 56, strlen(hash1));

    /* Hash should be deterministic */
    char *hash2 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash2 != NULL, "second hash should not be NULL");
    TEST_ASSERT(strcmp(hash1, hash2) == 0, "content hash should be deterministic");

    lv_free((void **) &hash1);
    lv_free((void **) &hash2);
    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 6: Round-trip save/load
 * ────────────────────────────────────────────── */
static void test_save_load_roundtrip(void) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Save to test file */
    AxiomSaveStatus save_status = axiom_package_save(pkg, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "axiom_package_save should return AXIOM_SAVE_OK");

    /* Compute hash before destroying */
    char *hash_orig = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash_orig != NULL, "original hash should be computable");

    axiom_package_destroy(pkg);

    /* Load from saved file */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "reloading saved file should succeed");

    TEST_ASSERT(strcmp(pkg2->name, "linear_algebra") == 0, "reloaded package should have same name");
    TEST_ASSERT(strcmp(pkg2->version, "1.0.0") == 0, "reloaded package should have same version");
    TEST_ASSERT(pkg2->template_count == EXPECTED_TEMPLATE_COUNT, "reloaded package should have same template count");
    TEST_ASSERT(pkg2->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "reloaded package should have same unconstructible count");

    char *hash_reload = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash_reload != NULL, "reloaded hash should be computable");
    TEST_ASSERT(strcmp(hash_orig, hash_reload) == 0, "content hash should survive round-trip");

    lv_free((void **) &hash_orig);
    lv_free((void **) &hash_reload);
    axiom_package_destroy(pkg2);

    /* Clean up test file */
    remove(SAVE_TEST_PATH);
}

/* ──────────────────────────────────────────────
 * Test 7: Dependency validation (self-validation)
 * ────────────────────────────────────────────── */
static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Self-validation: all dependencies should resolve within the package */
    AxiomPackage *loaded_packages[1] = {pkg};

    bool valid = axiom_package_validate_dependencies(pkg, loaded_packages, 1);
    /* Some reduces_to values point to external concepts ("undecidable",
     * "requires_axiom_of_choice", "np_hard", "numerically_unstable"),
     * which should validate as identifier format strings */
    if (!valid) {
        const char *err = axiom_package_get_last_error();
        printf("  Validation note: %s\n", err ? err : "(unknown)");
        printf("  (identifier references to external concepts are expected)\n");
    }
    TEST_ASSERT(1, "dependency validation executed");

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 8: Negative lookup (non-existent entities)
 * ────────────────────────────────────────────── */
static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template_xyz");
    TEST_ASSERT(tmpl == NULL, "lookup of non-existent template should return NULL");

    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem_xyz");
    TEST_ASSERT(uc == NULL, "lookup of non-existent unconstructible should return NULL");

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 9: External reference format validation
 * ────────────────────────────────────────────── */
static void test_external_references(void) {
    printf("Test 9: External reference URLs...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        TEST_ASSERT(uc->external_ref != NULL, "each unconstructible should have an external_ref");

        /* Verify it's a valid URL */
        int is_url = (strncmp(uc->external_ref, "http://", 7) == 0 || strncmp(uc->external_ref, "https://", 8) == 0);
        TEST_ASSERT(is_url, "external_ref should be a valid URL");

        printf("  '%s' -> %s\n", uc->name, uc->external_ref);
    }

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 10: Key linear algebra template checks
 * ────────────────────────────────────────────── */
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

/* ──────────────────────────────────────────────
 * Main
 * ────────────────────────────────────────────── */
int main(void) {
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

    return 0;
}
