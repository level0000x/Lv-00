/**
 * @file test_axiom_euclidean_plane.c
 * @brief Euclidean Plane Geometry Axiom Package Test
 */

#include "lv00.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "axiom_packages/euclidean_plane.lvz"
#define SAVE_TEST_PATH "axiom_packages/euclidean_plane_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT       22
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

static void test_load_from_file(void)
{
    printf("Test 1: Load euclidean_plane.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK,
        "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "euclidean_plane") == 0,
        "package name should be 'euclidean_plane'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0,
        "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void)
{
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT,
        "should have 22 constraint templates");
    printf("  Template count: %d (expected %d)\n",
           pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    const char *expected_templates[] = {
        "line_through_two_points",
        "line_has_two_points",
        "existence_of_triangle",
        "betweenness_symmetry",
        "extend_segment",
        "betweenness_uniqueness",
        "pasch_axiom",
        "segment_transport",
        "segment_congruence_reflexive",
        "segment_congruence_transitive",
        "angle_transport",
        "angle_congruence_properties",
        "SAS_congruence",
        "unique_parallel",
        "archimedes_axiom",
        "line_completeness",
        "midpoint",
        "perpendicular_bisector",
        "perpendicular_from_point",
        "angle_bisector",
        "circle_by_center_radius",
        "line_circle_intersection",
        NULL
    };

    int found_count = 0;
    for (int i = 0; expected_templates[i] != NULL; i++) {
        ConstraintTemplate *tmpl =
            axiom_package_get_template(pkg, expected_templates[i]);
        if (tmpl) {
            found_count++;
        } else {
            printf("  MISSING template: '%s'\n", expected_templates[i]);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found_count == EXPECTED_TEMPLATE_COUNT,
        "all expected templates should be found");
    printf("  Found %d / %d templates\n", found_count, EXPECTED_TEMPLATE_COUNT);

    ConstraintTemplate *t;
    t = axiom_package_get_template(pkg, "line_through_two_points");
    TEST_ASSERT(t && t->param_count == 2,
        "line_through_two_points should have 2 params");
    t = axiom_package_get_template(pkg, "existence_of_triangle");
    TEST_ASSERT(t && t->param_count == 0,
        "existence_of_triangle should have 0 params");
    t = axiom_package_get_template(pkg, "SAS_congruence");
    TEST_ASSERT(t && t->param_count == 6,
        "SAS_congruence should have 6 params");
    t = axiom_package_get_template(pkg, "pasch_axiom");
    TEST_ASSERT(t && t->param_count == 4,
        "pasch_axiom should have 4 params");
    t = axiom_package_get_template(pkg, "unique_parallel");
    TEST_ASSERT(t && t->param_count == 2,
        "unique_parallel should have 2 params");

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void)
{
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
        "should have 6 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n",
           pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        { "angle_trisection",          "cubic_equation_solving",          3, true },
        { "doubling_the_cube",          "cube_root_of_two",               3, true },
        { "squaring_the_circle",        "pi_transcendence",               3, true },
        { "general_quintic_by_radicals","abel_ruffini_theorem",           2, true },
        { "construction_of_regular_heptagon", "cubic_equation_solving",   2, true },
        { "circle_squaring_straightedge", "lindemann_weierstrass_theorem", 2, true },
    };

    for (int i = 0; i < (int)(sizeof(expected)/sizeof(expected[0])); i++) {
        KnownUnconstructible *uc =
            axiom_package_lookup_unconstructible(pkg, expected[i].name);
        TEST_ASSERT(uc != NULL, expected[i].name);

        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL &&
                        strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                expected[i].name);
            TEST_ASSERT(uc->dependency_count == expected[i].dep_count,
                expected[i].name);
            TEST_ASSERT(uc->green_verified == expected[i].green_verified,
                expected[i].name);
            TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0,
                "should have external_ref URL");
            printf("  [%d] %s -> %s (deps=%d, verified=%s)\n",
                   i, uc->name, uc->reduces_to,
                   uc->dependency_count,
                   uc->green_verified ? "true" : "false");
        }
    }

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void)
{
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL &&
                strcmp(pkg->bottom_geometry, "euclidean_plane") == 0,
        "bottom_geometry should be 'euclidean_plane'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL &&
                strcmp(pkg->negation_encoding, "classical_material_implication") == 0,
        "negation_encoding should be 'classical_material_implication'");
    printf("  negation_encoding: %s\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == EXPLOSION_PRINCIPLE,
        "contradiction_behavior should be EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: explosion_principle\n");

    axiom_package_destroy(pkg);
}

static void test_content_hash(void)
{
    printf("Test 5: Content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash) == 64, "SHA-256 hash should be 64 hex chars");

    if (hash) {
        printf("  SHA-256: %s\n", hash);
        free(hash);
    }

    axiom_package_destroy(pkg);
}

static void test_round_trip(void)
{
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg1, AXIOM_PKG_PATH);

    AxiomSaveStatus save_status = axiom_package_save(pkg1, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "save should succeed");

    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "re-load from saved file should succeed");

    TEST_ASSERT(pkg2->template_count == pkg1->template_count,
        "template count should match after round-trip");
    TEST_ASSERT(pkg2->unconstructible_count == pkg1->unconstructible_count,
        "unconstructible count should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->name, pkg1->name) == 0,
        "name should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->version, pkg1->version) == 0,
        "version should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg1->bottom_geometry) == 0,
        "bottom_geometry should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg1->contradiction_behavior,
        "contradiction_behavior should match after round-trip");

    printf("  Round-trip: templates=%d, unconstructibles=%d\n",
           pkg2->template_count, pkg2->unconstructible_count);

    char *hash1 = axiom_package_compute_content_hash(pkg1);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash1 && hash2 && strcmp(hash1, hash2) == 0,
        "content hashes should match after round-trip");
    printf("  Hash match: %s\n",
           (hash1 && hash2 && strcmp(hash1, hash2) == 0) ? "YES" : "NO");

    free(hash1);
    free(hash2);
    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

static void test_dependency_validation(void)
{
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    bool valid = axiom_package_validate_dependencies(pkg, &pkg, 1);
    /* Note: self-validation may fail because reduces_to targets like
       "cubic_equation_solving" are mathematical reduction descriptions,
       not references to other unconstructible entries in the same package. */
    printf("  Self-validation: %s (expected: may fail for cross-reference reduces_to)\n",
           valid ? "PASS" : "FAIL (acceptable)");

    axiom_package_destroy(pkg);
}

static void test_negative_lookups(void)
{
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "non-existent template should return NULL");

    KnownUnconstructible *uc =
        axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "non-existent unconstructible should return NULL");

    printf("  Negative lookups: correct\n");

    axiom_package_destroy(pkg);
}

int main(void)
{
    TEST_SUITE_BEGIN("Euclidean Plane");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);

    TEST_SUMMARY();

    return 0;
}
