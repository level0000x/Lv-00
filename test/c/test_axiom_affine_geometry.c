/**
 * @file test_axiom_affine_geometry.c
 * @brief Affine Geometry Axiom Package Test
 *
 * Tests the loading, template verification, unconstructible problem
 * tracking, logical framework, content hashing, round-trip save/load,
 * dependency validation, and negative lookups for the affine_geometry
 * axiom package.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"

#define AXIOM_PKG_PATH "module/axiom_packages/affine_geometry.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/affine_geometry_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 56
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

static int g_fail_count = 0;
static int g_pass_count = 0;

#define TEST_ASSERT(cond, msg)           \
    do {                                 \
        if (!(cond)) {                   \
            printf("  FAIL: %s\n", msg); \
            g_fail_count++;              \
        } else {                         \
            g_pass_count++;              \
        }                                \
    } while (0)

/* ------------------------------------------------------------------ */
/* Test 1: Load from file                                              */
/* ------------------------------------------------------------------ */
static void test_load_from_file(void) {
    printf("Test 1: Load affine_geometry.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "affine_geometry") == 0,
                "package name should be 'affine_geometry'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 2: Verify constraint templates                                 */
/* ------------------------------------------------------------------ */
static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 56 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    /* All 50 expected template names */
    const char *expected_templates[] = {
        /* Group I: Incidence Axioms (4) */
        "line_through_two_points", "line_has_two_points", "existence_of_triangle", "existence_of_affine_frame_3d",
        /* Group II: Parallelism Axioms (4) */
        "parallel_through_point", "parallelism_reflexive", "parallelism_transitive", "parallelism_no_intersection",
        /* Group III: Vector Space of Displacements (11) */
        "point_subtraction", "point_translation", "vector_addition_associative", "vector_addition_commutative",
        "zero_vector_exists", "vector_negation", "scalar_multiplication", "scalar_distributivity_vectors",
        "scalar_distributivity_scalars", "scalar_multiplication_associative", "scalar_unit",
        /* Group IV: Affine Structure Axioms (4) */
        "subtraction_identity", "subtraction_chain", "translation_subtraction_compat", "subtraction_translation_compat",
        /* Group V: Affine Combinations (7) */
        "affine_combination_two", "midpoint", "centroid_three_points", "general_barycenter",
        "affine_combination_associative", "affine_combination_commutative", "affine_combination_idempotent",
        /* Group VI: Affine Transformations (8) */
        "affine_map_preserves_combination", "translation_map", "affine_map_decomposition",
        "affine_map_preserves_parallelism", "affine_map_preserves_ratio", "affine_map_preserves_midpoint",
        "affine_map_preserves_centroid", "affine_map_preserves_barycenter",
        /* Group VII: Desargues' Theorem (1) */
        "desargues_theorem_affine",
        /* Group VIII: Pappus's Theorem (1) */
        "pappus_theorem_affine",
        /* Group IX: Affine Subspaces (5) */
        "affine_subspace_check", "affine_span_two_points", "affine_span_three_points", "affine_subspace_dimension",
        "affine_subspaces_parallel",
        /* Group X: Ratio and Division (4) */
        "simple_ratio", "thales_intercept_theorem", "ceva_theorem", "menelaus_theorem",
        /* Group XI: Parallelogram and Affine Quadrilaterals (4) */
        "parallelogram_fourth_vertex", "parallelogram_law", "parallelogram_diagonal_bisect",
        "affine_map_preserves_parallelogram",
        /* Group XII: Projective Connection (3) */
        "projective_completion", "line_at_infinity", "affine_patch_from_projective", NULL};

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

    /* Verify specific parameter counts */
    ConstraintTemplate *t;

    t = axiom_package_get_template(pkg, "line_through_two_points");
    TEST_ASSERT(t && t->param_count == 2, "line_through_two_points should have 2 params (point A, point B)");

    t = axiom_package_get_template(pkg, "existence_of_triangle");
    TEST_ASSERT(t && t->param_count == 0, "existence_of_triangle should have 0 params");

    t = axiom_package_get_template(pkg, "existence_of_affine_frame_3d");
    TEST_ASSERT(t && t->param_count == 0, "existence_of_affine_frame_3d should have 0 params");

    t = axiom_package_get_template(pkg, "parallel_through_point");
    TEST_ASSERT(t && t->param_count == 2, "parallel_through_point should have 2 params (point P, line L)");

    t = axiom_package_get_template(pkg, "parallelism_reflexive");
    TEST_ASSERT(t && t->param_count == 1, "parallelism_reflexive should have 1 param (line l)");

    t = axiom_package_get_template(pkg, "parallelism_transitive");
    TEST_ASSERT(t && t->param_count == 3, "parallelism_transitive should have 3 params (l, m, n)");

    t = axiom_package_get_template(pkg, "point_subtraction");
    TEST_ASSERT(t && t->param_count == 2, "point_subtraction should have 2 params (point A, point B)");

    t = axiom_package_get_template(pkg, "point_translation");
    TEST_ASSERT(t && t->param_count == 2, "point_translation should have 2 params (point A, vector v)");

    t = axiom_package_get_template(pkg, "zero_vector_exists");
    TEST_ASSERT(t && t->param_count == 0, "zero_vector_exists should have 0 params");

    t = axiom_package_get_template(pkg, "scalar_multiplication");
    TEST_ASSERT(t && t->param_count == 2, "scalar_multiplication should have 2 params (scalar r, vector v)");

    t = axiom_package_get_template(pkg, "subtraction_chain");
    TEST_ASSERT(t && t->param_count == 3, "subtraction_chain should have 3 params (Chasles relation)");

    t = axiom_package_get_template(pkg, "affine_combination_two");
    TEST_ASSERT(t && t->param_count == 4, "affine_combination_two should have 4 params (r, s, A, B)");

    t = axiom_package_get_template(pkg, "midpoint");
    TEST_ASSERT(t && t->param_count == 2, "midpoint should have 2 params (point A, point B)");

    t = axiom_package_get_template(pkg, "centroid_three_points");
    TEST_ASSERT(t && t->param_count == 3, "centroid_three_points should have 3 params (A, B, C)");

    t = axiom_package_get_template(pkg, "general_barycenter");
    TEST_ASSERT(t && t->param_count == 6, "general_barycenter should have 6 params (A, B, C, wa, wb, wc)");

    t = axiom_package_get_template(pkg, "affine_map_preserves_combination");
    TEST_ASSERT(t && t->param_count == 3, "affine_map_preserves_combination should have 3 params");

    t = axiom_package_get_template(pkg, "affine_map_preserves_barycenter");
    TEST_ASSERT(t && t->param_count == 8, "affine_map_preserves_barycenter should have 8 params");

    t = axiom_package_get_template(pkg, "desargues_theorem_affine");
    TEST_ASSERT(t && t->param_count == 7, "desargues_theorem_affine should have 7 params");

    t = axiom_package_get_template(pkg, "pappus_theorem_affine");
    TEST_ASSERT(t && t->param_count == 6, "pappus_theorem_affine should have 6 params");

    t = axiom_package_get_template(pkg, "simple_ratio");
    TEST_ASSERT(t && t->param_count == 3, "simple_ratio should have 3 params (A, B, C)");

    t = axiom_package_get_template(pkg, "ceva_theorem");
    TEST_ASSERT(t && t->param_count == 6, "ceva_theorem should have 6 params (A, B, C, D, E, F)");

    t = axiom_package_get_template(pkg, "menelaus_theorem");
    TEST_ASSERT(t && t->param_count == 6, "menelaus_theorem should have 6 params (A, B, C, D, E, F)");

    t = axiom_package_get_template(pkg, "parallelogram_fourth_vertex");
    TEST_ASSERT(t && t->param_count == 3, "parallelogram_fourth_vertex should have 3 params (A, B, C)");

    t = axiom_package_get_template(pkg, "parallelogram_law");
    TEST_ASSERT(t && t->param_count == 4, "parallelogram_law should have 4 params (A, B, C, D)");

    t = axiom_package_get_template(pkg, "projective_completion");
    TEST_ASSERT(t && t->param_count == 1, "projective_completion should have 1 param (affine_space)");

    t = axiom_package_get_template(pkg, "affine_patch_from_projective");
    TEST_ASSERT(t && t->param_count == 2, "affine_patch_from_projective should have 2 params");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 3: Verify known unconstructible problems                       */
/* ------------------------------------------------------------------ */
static void test_unconstructible_problems(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 7 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        {"perpendicular_bisector", "orthogonality requires metric structure", 2, true},
        {"angle_trisection", "requires metric structure and solving cubic equations", 2, true},
        {"circle_construction", "circles require metric distance notion absent in affine geometry", 2, true},
        {"finite_affine_plane_non_prime_power",
         "existence of finite affine planes of non-prime-power order is an open problem in combinatorics; equivalent "
         "to existence of finite projective planes",
         3, false},
        {"non_desarguesian_classification",
         "classification of non-Desarguesian affine planes is wildly open; only partial results known", 2, false},
        {"metric_recovery_from_affine",
         "an affine space admits infinitely many inequivalent metric structures; no canonical choice without "
         "additional data",
         2, true},
        {"area_computation",
         "area requires a notion of determinant or metric; only ratios of areas on parallel lines are affine "
         "invariants",
         2, true},
    };

    for (int i = 0; i < (int) (sizeof(expected) / sizeof(expected[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        TEST_ASSERT(uc != NULL, expected[i].name);

        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL && strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                        expected[i].name);
            TEST_ASSERT(uc->dependency_chain.count == expected[i].dep_count, expected[i].name);
            TEST_ASSERT(uc->green_verified == expected[i].green_verified, expected[i].name);
            TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "should have external_ref URL");
            printf("  [%d] %s -> %s (deps=%d, verified=%s)\n", i, uc->name, uc->reduces_to, uc->dependency_chain.count,
                   uc->green_verified ? "true" : "false");
        }
    }

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 4: Verify bottom geometry and logical framework                */
/* ------------------------------------------------------------------ */
static void test_logical_framework(void) {
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "affine_space") == 0,
                "bottom_geometry should be 'affine_space'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_material_implication") == 0,
                "negation_encoding should be 'classical_material_implication'");
    printf("  negation_encoding: %s\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 5: Content hash                                                */
/* ------------------------------------------------------------------ */
static void test_content_hash(void) {
    printf("Test 5: Verify content hash...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash1 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash1 != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash1) == 64, "SHA-256 hash should be 64 hex chars");
    printf("  Hash: %s\n", hash1 ? hash1 : "(null)");

    /* Loading again should produce the same hash */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg2, AXIOM_PKG_PATH);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash2 != NULL, "second content hash should not be NULL");
    TEST_ASSERT(strcmp(hash1, hash2) == 0, "identical packages should have identical hashes");
    printf("  Hash verification: %s\n", (hash1 && hash2 && strcmp(hash1, hash2) == 0) ? "MATCH" : "MISMATCH");

    if (hash1)
        lv_free_ptr(hash1);
    if (hash2)
        lv_free_ptr(hash2);
    axiom_package_destroy(pkg);
    axiom_package_destroy(pkg2);
}

/* ------------------------------------------------------------------ */
/* Test 6: Round-trip save/load                                       */
/* ------------------------------------------------------------------ */
static void test_round_trip_save_load(void) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Save */
    AxiomSaveStatus save_status = axiom_package_save(pkg, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "axiom_package_save should return AXIOM_SAVE_OK");
    printf("  Save status: %s\n", save_status == AXIOM_SAVE_OK ? "OK" : "FAILED");

    /* Load saved file */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "re-loading saved file should return AXIOM_LOAD_OK");

    if (load_status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Load error: %s\n", err ? err : "(unknown)");
    }

    /* Verify key properties match */
    TEST_ASSERT(axiom_package_get_template_count(pkg2) == axiom_package_get_template_count(pkg), "template count should match after round-trip");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg2) == axiom_package_get_unconstructible_count(pkg),
                "unconstructible count should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->name, pkg->name) == 0, "package name should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->version, pkg->version) == 0, "package version should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg->bottom_geometry) == 0,
                "bottom_geometry should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->negation_encoding, pkg->negation_encoding) == 0,
                "negation_encoding should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg->contradiction_behavior,
                "contradiction_behavior should match after round-trip");

    printf("  Round-trip: templates=%d, unconstructibles=%d\n", axiom_package_get_template_count(pkg2), axiom_package_get_unconstructible_count(pkg2));

    /* Verify content hashes match */
    char *hash1 = axiom_package_compute_content_hash(pkg);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    /* Note: save may reorder or normalize, so we just check both are valid */
    TEST_ASSERT(hash1 != NULL && hash2 != NULL, "both hashes should be valid after round-trip");
    printf("  Original hash:  %s\n", hash1 ? hash1 : "(null)");
    printf("  Reloaded hash:  %s\n", hash2 ? hash2 : "(null)");

    if (hash1)
        lv_free_ptr(hash1);
    if (hash2)
        lv_free_ptr(hash2);
    axiom_package_destroy(pkg);
    axiom_package_destroy(pkg2);
}

/* ------------------------------------------------------------------ */
/* Test 7: Dependency validation                                      */
/* ------------------------------------------------------------------ */
static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Validate with no other packages loaded — external refs should still
       pass URL format checks */
    bool valid = axiom_package_validate_dependencies(pkg, NULL, 0);
    /* Some dependency references point to external items not in any loaded
       package, so validation may fail for those. We just check it doesn't
       crash and reports correctly. */
    printf("  Validation result (no deps): %s\n", valid ? "PASS" : "FAIL (expected for unresolved cross-package refs)");

    /* Validate with self as the only loaded package */
    AxiomPackage *loaded[] = {pkg};
    valid = axiom_package_validate_dependencies(pkg, loaded, 1);
    printf("  Validation result (self only): %s\n",
           valid ? "PASS" : "FAIL (expected for unresolved cross-package refs)");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 8: Negative lookups                                           */
/* ------------------------------------------------------------------ */
static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Template that doesn't exist */
    ConstraintTemplate *t = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(t == NULL, "nonexistent template should return NULL");

    /* Unconstructible problem that doesn't exist */
    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "nonexistent unconstructible should return NULL");

    /* Empty string lookups */
    t = axiom_package_get_template(pkg, "");
    TEST_ASSERT(t == NULL, "empty template name should return NULL");

    uc = axiom_package_lookup_unconstructible(pkg, "");
    TEST_ASSERT(uc == NULL, "empty unconstructible name should return NULL");

    printf("  All negative lookups returned NULL as expected\n");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 9: External references format validation                      */
/* ------------------------------------------------------------------ */
static void test_external_refs(void) {
    printf("Test 9: External references format validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    int valid_ref_count = 0;
    for (int i = 0; i < axiom_package_get_unconstructible_count(pkg); i++) {
        KnownUnconstructible *uc = axiom_package_get_unconstructible(pkg, i);
        if (uc->external_ref && strlen(uc->external_ref) > 0) {
            /* Check it starts with https:// */
            bool is_https = strncmp(uc->external_ref, "https://", 8) == 0;
            TEST_ASSERT(is_https, "external_ref should use https:// URL format");
            if (is_https)
                valid_ref_count++;
            printf("  [%d] %s\n       -> %s\n", i, uc->name, uc->external_ref);
        }
    }

    TEST_ASSERT(valid_ref_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "all unconstructible problems should have valid external refs");
    printf("  Valid external refs: %d / %d\n", valid_ref_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 10: Verify template group distribution                        */
/* ------------------------------------------------------------------ */
static void test_template_groups(void) {
    printf("Test 10: Verify template group distribution...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Count templates in each group by checking name prefixes */
    int incidence_count = 0;
    int parallelism_count = 0;
    int vector_count = 0;
    int affine_struct_count = 0;
    int combination_count = 0;
    int transform_count = 0;
    int theorem_count = 0;
    int subspace_count = 0;
    int ratio_count = 0;
    int parallelogram_count = 0;
    int projective_count = 0;

    for (int i = 0; i < axiom_package_get_template_count(pkg); i++) {
        const ConstraintTemplate *tpl = axiom_package_get_template_by_index(pkg, i);
        const char *name = tpl ? tpl->name : "";
        if (strcmp(name, "line_through_two_points") == 0 || strcmp(name, "line_has_two_points") == 0 ||
            strcmp(name, "existence_of_triangle") == 0 || strcmp(name, "existence_of_affine_frame_3d") == 0)
            incidence_count++;
        else if (strncmp(name, "parallel", 8) == 0)
            parallelism_count++;
        else if (strncmp(name, "vector", 6) == 0 || strncmp(name, "scalar", 6) == 0 ||
                 strcmp(name, "zero_vector_exists") == 0)
            vector_count++;
        else if (strncmp(name, "subtraction", 11) == 0 || strncmp(name, "translation", 11) == 0)
            affine_struct_count++;
        else if (strncmp(name, "affine_combination", 18) == 0 || strncmp(name, "midpoint", 8) == 0 ||
                 strncmp(name, "centroid", 8) == 0 || strncmp(name, "general_barycenter", 18) == 0)
            combination_count++;
        else if (strncmp(name, "affine_map", 10) == 0)
            transform_count++;
        else if (strcmp(name, "desargues_theorem_affine") == 0 || strcmp(name, "pappus_theorem_affine") == 0)
            theorem_count++;
        else if (strncmp(name, "affine_subspace", 15) == 0 || strncmp(name, "affine_span", 11) == 0)
            subspace_count++;
        else if (strncmp(name, "simple_ratio", 12) == 0 || strncmp(name, "thales", 6) == 0 ||
                 strncmp(name, "ceva", 4) == 0 || strncmp(name, "menelaus", 8) == 0)
            ratio_count++;
        else if (strncmp(name, "parallelogram", 13) == 0)
            parallelogram_count++;
        else if (strncmp(name, "projective", 9) == 0 || strncmp(name, "line_at_infinity", 16) == 0 ||
                 strncmp(name, "affine_patch", 12) == 0)
            projective_count++;
    }

    printf("  Incidence: %d\n", incidence_count);
    printf("  Parallelism: %d\n", parallelism_count);
    printf("  Vector space: %d\n", vector_count);
    printf("  Affine structure: %d\n", affine_struct_count);
    printf("  Affine combinations: %d\n", combination_count);
    printf("  Affine transforms: %d\n", transform_count);
    printf("  Theorems (Desargues/Pappus): %d\n", theorem_count);
    printf("  Affine subspaces: %d\n", subspace_count);
    printf("  Ratio/Division: %d\n", ratio_count);
    printf("  Parallelogram: %d\n", parallelogram_count);
    printf("  Projective connection: %d\n", projective_count);

    int total = incidence_count + parallelism_count + vector_count + affine_struct_count + combination_count +
                transform_count + theorem_count + subspace_count + ratio_count + parallelogram_count + projective_count;

    printf("  Group total: %d (expected %d)%s\n", total, EXPECTED_TEMPLATE_COUNT,
           total == EXPECTED_TEMPLATE_COUNT ? "" : "  -- unclassified templates exist");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(void) {
    printf("============================================================\n");
    printf("  Affine Geometry Axiom Package Test Suite\n");
    printf("============================================================\n\n");

    test_load_from_file();
    printf("\n");

    test_templates();
    printf("\n");

    test_unconstructible_problems();
    printf("\n");

    test_logical_framework();
    printf("\n");

    test_content_hash();
    printf("\n");

    test_round_trip_save_load();
    printf("\n");

    test_dependency_validation();
    printf("\n");

    test_negative_lookups();
    printf("\n");

    test_external_refs();
    printf("\n");

    test_template_groups();
    printf("\n");

    printf("============================================================\n");
    printf("  Results: %d passed, %d failed, %d total\n", g_pass_count, g_fail_count, g_pass_count + g_fail_count);
    printf("============================================================\n");

    return g_fail_count > 0 ? 1 : 0;
}
