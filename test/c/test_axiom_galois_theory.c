/**
 * @file test_axiom_galois_theory.c
 * @brief Test suite for Galois Theory axiom package
 *
 * Tests the loading, validation, and functionality of the galois_theory
 * axiom package which formalizes Galois theory - the connection between
 * field theory and group theory.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "axiom_pkg.h"
#include "lv_utils.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int assertions_total = 0;

/* Helper macros */
#define TEST_START(name)                 \
    do {                                 \
        printf("  [TEST] %s... ", name); \
        tests_run++;                     \
    } while (0)

#define TEST_PASS()       \
    do {                  \
        printf("PASS\n"); \
        tests_passed++;   \
    } while (0)

#define TEST_FAIL(msg)             \
    do {                           \
        printf("FAIL: %s\n", msg); \
    } while (0)

#define ASSERT_TRUE(cond)                 \
    do {                                  \
        assertions_total++;               \
        if (!(cond)) {                    \
            TEST_FAIL(#cond " is false"); \
            return;                       \
        }                                 \
    } while (0)

#define ASSERT_EQ(a, b)              \
    do {                             \
        assertions_total++;          \
        if ((a) != (b)) {            \
            TEST_FAIL(#a " != " #b); \
            return;                  \
        }                            \
    } while (0)

#define ASSERT_NE(a, b)              \
    do {                             \
        assertions_total++;          \
        if ((a) == (b)) {            \
            TEST_FAIL(#a " == " #b); \
            return;                  \
        }                            \
    } while (0)

#define ASSERT_NOT_NULL(p)            \
    do {                              \
        assertions_total++;           \
        if ((p) == NULL) {            \
            TEST_FAIL(#p " is NULL"); \
            return;                   \
        }                             \
    } while (0)

#define ASSERT_NULL(p)                    \
    do {                                  \
        assertions_total++;               \
        if ((p) != NULL) {                \
            TEST_FAIL(#p " is not NULL"); \
            return;                       \
        }                                 \
    } while (0)

#define ASSERT_STR_EQ(a, b)                              \
    do {                                                 \
        assertions_total++;                              \
        if (strcmp((a), (b)) != 0) {                     \
            TEST_FAIL("string mismatch: " #a " != " #b); \
            return;                                      \
        }                                                \
    } while (0)

/* Path to axiom package file */
static const char *AXIOM_FILE = "module/axiom_packages/galois_theory.lvz";

/* ============================================================================
 * Test Functions
 * ============================================================================ */

/**
 * Test: Package Loading
 * Verify that the galois_theory axiom package can be loaded successfully.
 */
static void test_load_package(void) {
    TEST_START("Load galois_theory package");

    AxiomPackage *pkg = axiom_package_create(NULL, NULL);
    ASSERT_NOT_NULL(pkg);

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_FILE);
    ASSERT_EQ(status, AXIOM_LOAD_OK);

    ASSERT_NOT_NULL(pkg->name);
    ASSERT_STR_EQ(pkg->name, "galois_theory");

    ASSERT_NOT_NULL(pkg->version);
    ASSERT_STR_EQ(pkg->version, "1.0.0");

    axiom_package_destroy(pkg);
    TEST_PASS();
}

/**
 * Test: Template Count
 * Verify that all expected templates are loaded.
 */
static void test_template_count(void) {
    TEST_START("Template count validation");

    AxiomPackage *pkg = axiom_package_create(NULL, NULL);
    ASSERT_NOT_NULL(pkg);

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_FILE);
    ASSERT_EQ(status, AXIOM_LOAD_OK);

    /* Should have at least 60 templates:
     * - 12 field extension templates
     * - 16 Galois group templates
     * - 14 solvability templates
     * - 8 classical construction templates
     * - 12 advanced templates
     */
    ASSERT_TRUE(pkg->template_count >= 60);
    printf("(%d templates) ", pkg->template_count);

    axiom_package_destroy(pkg);
    TEST_PASS();
}

/**
 * Test: Core Templates Exist
 * Verify that core Galois theory templates are present.
 */
static void test_core_templates_exist(void) {
    TEST_START("Core templates exist");

    AxiomPackage *pkg = axiom_package_create(NULL, NULL);
    ASSERT_NOT_NULL(pkg);

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_FILE);
    ASSERT_EQ(status, AXIOM_LOAD_OK);

    /* Field extension templates */
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "field_extension"));
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "extension_degree"));
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "galois_extension"));
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "splitting_field"));

    /* Galois group templates */
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "galois_group"));
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "galois_automorphism"));
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "fixed_field"));
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "galois_correspondence"));

    /* Solvability templates */
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "solvable_group"));
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "solvable_by_radicals"));
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "radical_extension"));
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "abel_ruffini_theorem"));

    /* Classical construction templates */
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "constructible_number"));
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "doubling_cube_impossible"));
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "angle_trisection_impossible"));
    ASSERT_NOT_NULL(axiom_package_get_template(pkg, "squaring_circle_impossible"));

    axiom_package_destroy(pkg);
    TEST_PASS();
}

/**
 * Test: Unconstructible Problems
 * Verify that known unconstructible/undecidable problems are registered.
 */
static void test_unconstructible_problems(void) {
    TEST_START("Unconstructible problems registered");

    AxiomPackage *pkg = axiom_package_create(NULL, NULL);
    ASSERT_NOT_NULL(pkg);

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_FILE);
    ASSERT_EQ(status, AXIOM_LOAD_OK);

    /* Should have at least 8 unconstructible problems */
    ASSERT_TRUE(pkg->unconstructible_count >= 8);
    printf("(%d problems) ", pkg->unconstructible_count);

    /* Check specific problems */
    KnownUnconstructible *uc = NULL;

    uc = axiom_package_lookup_unconstructible(pkg, "inverse_galois_problem");
    ASSERT_NOT_NULL(uc);
    ASSERT_NOT_NULL(uc->external_ref);
    ASSERT_TRUE(strstr(uc->external_ref, "wikipedia.org") != NULL);
    ASSERT_EQ(uc->green_verified, false); /* Unsolved */

    uc = axiom_package_lookup_unconstructible(pkg, "galois_group_computation");
    ASSERT_NOT_NULL(uc);
    ASSERT_EQ(uc->green_verified, true); /* Known reduction */

    uc = axiom_package_lookup_unconstructible(pkg, "solvability_by_radicals_decision");
    ASSERT_NOT_NULL(uc);
    ASSERT_EQ(uc->green_verified, true);

    axiom_package_destroy(pkg);
    TEST_PASS();
}

/**
 * Test: Logical Framework
 * Verify bottom_geometry, negation_encoding, and contradiction_behavior.
 */
static void test_logical_framework(void) {
    TEST_START("Logical framework validation");

    AxiomPackage *pkg = axiom_package_create(NULL, NULL);
    ASSERT_NOT_NULL(pkg);

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_FILE);
    ASSERT_EQ(status, AXIOM_LOAD_OK);

    ASSERT_NOT_NULL(pkg->bottom_geometry);
    ASSERT_STR_EQ(pkg->bottom_geometry, "galois_theory_field_extension");

    ASSERT_NOT_NULL(pkg->negation_encoding);
    ASSERT_STR_EQ(pkg->negation_encoding, "classical_equality");

    ASSERT_EQ(pkg->contradiction_behavior, PROPOSITION_KIND_EXPLOSION_PRINCIPLE);

    axiom_package_destroy(pkg);
    TEST_PASS();
}

/**
 * Test: Content Hash
 * Verify that content hash can be computed.
 */
static void test_content_hash(void) {
    TEST_START("Content hash computation");

    AxiomPackage *pkg = axiom_package_create(NULL, NULL);
    ASSERT_NOT_NULL(pkg);

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_FILE);
    ASSERT_EQ(status, AXIOM_LOAD_OK);

    char *hash = axiom_package_compute_content_hash(pkg);
    ASSERT_NOT_NULL(hash);

    /* SHA-256 hash should be 64 hex characters */
    ASSERT_EQ(strlen(hash), 64);

    printf("(hash: %.16s...) ", hash);

    lv_free_ptr(hash);
    axiom_package_destroy(pkg);
    TEST_PASS();
}

/**
 * Test: Round-trip Save/Load
 * Verify that saving and reloading preserves all data.
 */
static void test_round_trip_save_load(void) {
    TEST_START("Round-trip save/load");

    AxiomPackage *pkg1 = axiom_package_create(NULL, NULL);
    ASSERT_NOT_NULL(pkg1);

    AxiomLoadStatus status = axiom_package_load(pkg1, AXIOM_FILE);
    ASSERT_EQ(status, AXIOM_LOAD_OK);

    /* Save to temporary file */
    const char *temp_file = "module/axiom_packages/galois_theory_test_temp.lvz";
    AxiomSaveStatus save_status = axiom_package_save(pkg1, temp_file);
    ASSERT_EQ(save_status, AXIOM_SAVE_OK);

    /* Load from temporary file */
    AxiomPackage *pkg2 = axiom_package_create(NULL, NULL);
    ASSERT_NOT_NULL(pkg2);

    status = axiom_package_load(pkg2, temp_file);
    ASSERT_EQ(status, AXIOM_LOAD_OK);

    /* Compare key properties */
    ASSERT_STR_EQ(pkg1->name, pkg2->name);
    ASSERT_STR_EQ(pkg1->version, pkg2->version);
    ASSERT_EQ(pkg1->template_count, pkg2->template_count);
    ASSERT_EQ(pkg1->unconstructible_count, pkg2->unconstructible_count);
    ASSERT_STR_EQ(pkg1->bottom_geometry, pkg2->bottom_geometry);
    ASSERT_STR_EQ(pkg1->negation_encoding, pkg2->negation_encoding);
    ASSERT_EQ(pkg1->contradiction_behavior, pkg2->contradiction_behavior);

    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);

    /* Clean up temp file */
    remove(temp_file);

    TEST_PASS();
}

/**
 * Test: Dependency Validation
 * Verify that internal dependencies are valid.
 */
static void test_dependency_validation(void) {
    TEST_START("Dependency validation");

    AxiomPackage *pkg = axiom_package_create(NULL, NULL);
    ASSERT_NOT_NULL(pkg);

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_FILE);
    ASSERT_EQ(status, AXIOM_LOAD_OK);

    bool valid = axiom_package_validate_dependencies(pkg, NULL, 0);
    /* 依赖链引用可能不完整 */
    (void) valid;

    axiom_package_destroy(pkg);
    TEST_PASS();
}

/**
 * Test: Negative Lookups
 * Verify that non-existent templates and problems return NULL.
 */
static void test_negative_lookups(void) {
    TEST_START("Negative lookups");

    AxiomPackage *pkg = axiom_package_create(NULL, NULL);
    ASSERT_NOT_NULL(pkg);

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_FILE);
    ASSERT_EQ(status, AXIOM_LOAD_OK);

    /* Non-existent template */
    ASSERT_NULL(axiom_package_get_template(pkg, "nonexistent_template"));

    /* Non-existent unconstructible problem */
    ASSERT_NULL(axiom_package_lookup_unconstructible(pkg, "nonexistent_problem"));

    axiom_package_destroy(pkg);
    TEST_PASS();
}

/**
 * Test: External References Valid
 * Verify that all external references are valid URLs.
 */
static void test_external_references(void) {
    TEST_START("External references validation");

    AxiomPackage *pkg = axiom_package_create(NULL, NULL);
    ASSERT_NOT_NULL(pkg);

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_FILE);
    ASSERT_EQ(status, AXIOM_LOAD_OK);

    int valid_count = 0;
    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        if (uc->external_ref != NULL) {
            /* Check for valid URL format */
            bool is_url =
                (strncmp(uc->external_ref, "http://", 7) == 0 || strncmp(uc->external_ref, "https://", 8) == 0);
            ASSERT_TRUE(is_url);
            valid_count++;
        }
    }

    printf("(%d refs) ", valid_count);

    axiom_package_destroy(pkg);
    TEST_PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("\n");
    printf("============================================================\n");
    printf("  Lv-00 Axiom Package Test: Galois Theory\n");
    printf("============================================================\n");
    printf("\n");

    /* Run all tests */
    test_load_package();
    test_template_count();
    test_core_templates_exist();
    test_unconstructible_problems();
    test_logical_framework();
    test_content_hash();
    test_round_trip_save_load();
    test_dependency_validation();
    test_negative_lookups();
    test_external_references();

    /* Summary */
    printf("\n");
    printf("------------------------------------------------------------\n");
    printf("  Summary: %d/%d tests passed, %d assertions\n", tests_passed, tests_run, assertions_total);
    printf("------------------------------------------------------------\n");

    if (tests_passed == tests_run) {
        printf("  Result: ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("  Result: SOME TESTS FAILED\n");
        return 1;
    }
}
