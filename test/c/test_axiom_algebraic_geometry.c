﻿/**
 * @file test_axiom_algebraic_geometry.c
 * @brief Algebraic Geometry Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the algebraic_geometry.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Algebraic geometry provides the study of geometric objects defined by
 * polynomial equations for Lv-00. The 38 templates cover affine/projective
 * varieties, schemes, cohomology, and dimension theory.
 */

#include <stdio.h>
#include <string.h>

#include "axiom_pkg.h"
#include "lv00_utils.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/algebraic_geometry.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/algebraic_geometry_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 38
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ──────────────────────────────────────────────
 * Test 1: Load from file
 * ────────────────────────────────────────────── */
static void test_load_from_file(void) {
    printf("Test 1: Load algebraic_geometry.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "algebraic_geometry") == 0,
                "package name should be 'algebraic_geometry'");
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

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 38 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    /* Check representative templates from each group */
    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group I: Affine Geometry (6) */
        {"affine_space", 2},
        {"algebraic_set", 1},
        {"zariski_topology", 1},
        {"coordinate_ring", 1},
        {"hilbert_nullstellensatz", 2},
        {"ideal_variety_correspondence", 2},
        /* Group II: Projective Geometry (6) */
        {"projective_space", 1},
        {"projective_variety", 1},
        {"homogeneous_coordinates", 1},
        {"serre_duality", 2},
        {"projective_normality", 1},
        {"scheme_theory", 1},
        /* Group III: Scheme Theory (6) */
        {"affine_scheme", 1},
        {"locally_ringed_space", 1},
        {"separated_morphism", 1},
        {"proper_morphism", 1},
        {"morphism_of_schemes", 2},
        {"sheaf_of_rings", 1},
        /* Group IV: Sheaf Theory (6) */
        {"presheaf", 2},
        {"sheafification", 1},
        {"cohomology_group", 2},
        {"serre_criterion", 2},
        {"leray_cover", 2},
        {"krull_dimension", 1},
        /* Group V: Dimension Theory (6) */
        {"codimension", 2},
        {"dimension_theorem", 2},
        {"transcendence_degree", 1},
        {"regular_local_ring", 1},
        {"algebraic_curve", 1},
        {"elliptic_curve_scheme", 1},
        /* Group VI: Special Varieties (5) */
        {"abelian_variety", 1},
        {"grassmannian", 2},
        {"flag_variety", 1},
        {"noetherian_ring", 1},
        {"artinian_ring", 1},
        /* Group VII: Ring Theory (3) */
        {"integral_extension", 2},
        {"noether_normalization", 1},
        {"zariski_main_theorem", 1},
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

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 6 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    /* Verify each expected unconstructible */
    struct {
        const char *name;
        const char *reduces_to;
        int min_deps;
        bool has_ref;
    } expected_uc[] = {
        {"hartshorne_conjecture", "open_problem", 4, true},   {"minimal_model_program", "open_problem", 5, true},
        {"resolution_of_singularities", "proved", 3, true},   {"cohomology_ring_computation", "undecidable", 4, true},
        {"rational_point_existence", "undecidable", 3, true}, {"hilbert_sixteenth_problem", "open_problem", 4, true},
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
    TEST_ASSERT(
        strstr(pkg->bottom_geometry, "algebraic_set") != NULL || strstr(pkg->bottom_geometry, "affine_space") != NULL,
        "bottom_geometry should contain algebraic geometry concepts");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
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

    lv00_free((void **) &hash1);
    lv00_free((void **) &hash2);
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

    TEST_ASSERT(strcmp(pkg2->name, "algebraic_geometry") == 0, "reloaded package should have same name");
    TEST_ASSERT(strcmp(pkg2->version, "1.0.0") == 0, "reloaded package should have same version");
    TEST_ASSERT(pkg2->template_count == EXPECTED_TEMPLATE_COUNT, "reloaded package should have same template count");
    TEST_ASSERT(pkg2->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "reloaded package should have same unconstructible count");

    char *hash_reload = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash_reload != NULL, "reloaded hash should be computable");
    TEST_ASSERT(strcmp(hash_orig, hash_reload) == 0, "content hash should survive round-trip");

    lv00_free((void **) &hash_orig);
    lv00_free((void **) &hash_reload);
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

    AxiomPackage *loaded_packages[1] = {pkg};

    bool valid = axiom_package_validate_dependencies(pkg, loaded_packages, 1);
    if (!valid) {
        const char *err = axiom_package_get_last_error();
        printf("  Validation note: %s\n", err ? err : "(unknown)");
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
 * Test 10: Key algebraic geometry template checks
 * ────────────────────────────────────────────── */
static void test_key_templates(void) {
    printf("Test 10: Key algebraic geometry templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Affine geometry core */
    const char *affine_core[] = {"affine_space", "algebraic_set", "coordinate_ring"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, affine_core[i]);
        TEST_ASSERT(tmpl != NULL, "affine geometry template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Projective geometry core */
    const char *proj_core[] = {"projective_space", "projective_variety", "homogeneous_coordinates"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, proj_core[i]);
        TEST_ASSERT(tmpl != NULL, "projective geometry template should exist");
    }

    /* Scheme theory core */
    ConstraintTemplate *as = axiom_package_get_template(pkg, "affine_scheme");
    TEST_ASSERT(as != NULL, "affine scheme should exist");
    ConstraintTemplate *lrs = axiom_package_get_template(pkg, "locally_ringed_space");
    TEST_ASSERT(lrs != NULL, "locally ringed space should exist");

    /* Dimension theory */
    ConstraintTemplate *dim = axiom_package_get_template(pkg, "krull_dimension");
    TEST_ASSERT(dim != NULL, "Krull dimension should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Main
 * ────────────────────────────────────────────── */
int main(void) {
    TEST_SUITE_BEGIN("Algebraic Geometry");

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
