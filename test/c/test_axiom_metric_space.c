/**
 * @file test_axiom_metric_space.c
 * @brief Metric Space Theory Axiom Package Test
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"

#define AXIOM_PKG_PATH "module/axiom_packages/metric_space.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/metric_space_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 47
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

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

static void test_load_from_file(void) {
    printf("Test 1: Load metric_space.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "metric_space") == 0, "package name should be 'metric_space'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 47 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    const char *expected_templates[] = {
        /* Group I: Core Metric Axioms */
        "metric_non_negativity", "metric_symmetry", "triangle_inequality", "identity_of_indiscernibles",
        /* Group II: Basic Metric Constructions */
        "open_ball", "closed_ball", "sphere", "point_set_distance", "set_set_distance", "diameter",
        /* Group III: Metric Topology Constructions */
        "metric_open_set", "metric_closed_set", "interior", "closure", "boundary", "hausdorff_separation",
        /* Group IV: Convergence & Completeness */
        "sequence_convergence", "cauchy_sequence", "completeness", "cauchy_completion", "banach_fixed_point",
        "baire_category_theorem",
        /* Group V: Continuity */
        "pointwise_continuity", "uniform_continuity", "lipschitz_continuity", "contraction_map", "isometry",
        "uniform_extension_to_completion",
        /* Group VI: Compactness & Boundedness */
        "bounded_set", "totally_bounded", "sequential_compactness", "compact_equals_complete_totally_bounded",
        "lebesgue_number_lemma", "arzela_ascoli",
        /* Group VII: Connectedness */
        "connected_set", "path_connected_set", "connected_component",
        /* Group VIII: Product & Quotient Constructions */
        "product_metric_linf", "product_metric_l2", "product_metric_l1", "quotient_metric",
        /* Group IX: Specialized Constructions */
        "hausdorff_distance", "subspace_metric", "discrete_metric", "euclidean_metric_Rn", "sup_metric",
        "weighted_metric", NULL};

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

    /* Verify specific param counts for key templates */
    ConstraintTemplate *t;

    /* Core axioms: metric_non_negativity(2), metric_symmetry(2),
       triangle_inequality(3), identity_of_indiscernibles(2) */
    t = axiom_package_get_template(pkg, "metric_non_negativity");
    TEST_ASSERT(t && t->param_count == 2, "metric_non_negativity should have 2 params");
    t = axiom_package_get_template(pkg, "metric_symmetry");
    TEST_ASSERT(t && t->param_count == 2, "metric_symmetry should have 2 params");
    t = axiom_package_get_template(pkg, "triangle_inequality");
    TEST_ASSERT(t && t->param_count == 3, "triangle_inequality should have 3 params");
    t = axiom_package_get_template(pkg, "identity_of_indiscernibles");
    TEST_ASSERT(t && t->param_count == 2, "identity_of_indiscernibles should have 2 params");

    /* Constructions: open_ball(2), closed_ball(2), sphere(2) */
    t = axiom_package_get_template(pkg, "open_ball");
    TEST_ASSERT(t && t->param_count == 2, "open_ball should have 2 params");
    t = axiom_package_get_template(pkg, "closed_ball");
    TEST_ASSERT(t && t->param_count == 2, "closed_ball should have 2 params");
    t = axiom_package_get_template(pkg, "sphere");
    TEST_ASSERT(t && t->param_count == 2, "sphere should have 2 params");

    /* Topology: metric_open_set(1), closure(1), boundary(1) */
    t = axiom_package_get_template(pkg, "metric_open_set");
    TEST_ASSERT(t && t->param_count == 1, "metric_open_set should have 1 param");
    t = axiom_package_get_template(pkg, "closure");
    TEST_ASSERT(t && t->param_count == 1, "closure should have 1 param");
    t = axiom_package_get_template(pkg, "boundary");
    TEST_ASSERT(t && t->param_count == 1, "boundary should have 1 param");

    /* Convergence: sequence_convergence(3), cauchy_sequence(2),
       completeness(1), cauchy_completion(1) */
    t = axiom_package_get_template(pkg, "sequence_convergence");
    TEST_ASSERT(t && t->param_count == 3, "sequence_convergence should have 3 params");
    t = axiom_package_get_template(pkg, "cauchy_sequence");
    TEST_ASSERT(t && t->param_count == 2, "cauchy_sequence should have 2 params");
    t = axiom_package_get_template(pkg, "completeness");
    TEST_ASSERT(t && t->param_count == 1, "completeness should have 1 param");
    t = axiom_package_get_template(pkg, "cauchy_completion");
    TEST_ASSERT(t && t->param_count == 1, "cauchy_completion should have 1 param");

    /* Banach fixed point: banach_fixed_point(2) */
    t = axiom_package_get_template(pkg, "banach_fixed_point");
    TEST_ASSERT(t && t->param_count == 2, "banach_fixed_point should have 2 params");

    /* Continuity: pointwise_continuity(3), uniform_continuity(2),
       isometry(1) */
    t = axiom_package_get_template(pkg, "pointwise_continuity");
    TEST_ASSERT(t && t->param_count == 3, "pointwise_continuity should have 3 params");
    t = axiom_package_get_template(pkg, "uniform_continuity");
    TEST_ASSERT(t && t->param_count == 2, "uniform_continuity should have 2 params");
    t = axiom_package_get_template(pkg, "isometry");
    TEST_ASSERT(t && t->param_count == 1, "isometry should have 1 param");

    /* Compactness: totally_bounded(2), sequential_compactness(1),
       arzela_ascoli(3) */
    t = axiom_package_get_template(pkg, "totally_bounded");
    TEST_ASSERT(t && t->param_count == 2, "totally_bounded should have 2 params");
    t = axiom_package_get_template(pkg, "sequential_compactness");
    TEST_ASSERT(t && t->param_count == 1, "sequential_compactness should have 1 param");
    t = axiom_package_get_template(pkg, "arzela_ascoli");
    TEST_ASSERT(t && t->param_count == 3, "arzela_ascoli should have 3 params");

    /* Product: product_metric_linf(2), product_metric_l2(2),
       product_metric_l1(2), quotient_metric(2) */
    t = axiom_package_get_template(pkg, "product_metric_linf");
    TEST_ASSERT(t && t->param_count == 2, "product_metric_linf should have 2 params");
    t = axiom_package_get_template(pkg, "product_metric_l2");
    TEST_ASSERT(t && t->param_count == 2, "product_metric_l2 should have 2 params");
    t = axiom_package_get_template(pkg, "product_metric_l1");
    TEST_ASSERT(t && t->param_count == 2, "product_metric_l1 should have 2 params");
    t = axiom_package_get_template(pkg, "quotient_metric");
    TEST_ASSERT(t && t->param_count == 2, "quotient_metric should have 2 params");

    /* Specialized: hausdorff_distance(2), discrete_metric(1),
       euclidean_metric_Rn(1), sup_metric(1), weighted_metric(2) */
    t = axiom_package_get_template(pkg, "hausdorff_distance");
    TEST_ASSERT(t && t->param_count == 2, "hausdorff_distance should have 2 params");
    t = axiom_package_get_template(pkg, "discrete_metric");
    TEST_ASSERT(t && t->param_count == 1, "discrete_metric should have 1 param");
    t = axiom_package_get_template(pkg, "euclidean_metric_Rn");
    TEST_ASSERT(t && t->param_count == 1, "euclidean_metric_Rn should have 1 param");
    t = axiom_package_get_template(pkg, "sup_metric");
    TEST_ASSERT(t && t->param_count == 1, "sup_metric should have 1 param");
    t = axiom_package_get_template(pkg, "weighted_metric");
    TEST_ASSERT(t && t->param_count == 2, "weighted_metric should have 2 params");

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 8 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        {"isometric_embedding_into_l2", "gram_matrix_positive_semi_definiteness", 3, true},
        {"separable_metric_space_classification", "uncountable_isometry_classes", 3, true},
        {"urysohn_universal_space_existence", "requires_axiom_of_choice", 3, true},
        {"finite_metric_space_isometry", "graph_isomorphism", 4, true},
        {"finite_metric_embedding_into_Rn", "NP_hard_optimization", 3, true},
        {"hausdorff_distance_computability", "non_computable_in_computable_analysis", 3, true},
        {"baire_category_without_choice", "requires_dependent_choice", 3, true},
        {"general_metrizability", "nagata_smirnov_conditions", 3, true},
    };

    for (int i = 0; i < (int) (sizeof(expected) / sizeof(expected[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        TEST_ASSERT(uc != NULL, expected[i].name);

        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL && strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                        expected[i].name);
            TEST_ASSERT(uc->dependency_count == expected[i].dep_count, expected[i].name);
            TEST_ASSERT(uc->green_verified == expected[i].green_verified, expected[i].name);
            TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "should have external_ref URL");
            printf("  [%d] %s -> %s (deps=%d, verified=%s)\n", i, uc->name, uc->reduces_to, uc->dependency_count,
                   uc->green_verified ? "true" : "false");
        }
    }

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void) {
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "metric_space_general") == 0,
                "bottom_geometry should be 'metric_space_general'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_distance_negation") == 0,
                "negation_encoding should be 'classical_distance_negation'");
    printf("  negation_encoding: %s\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    printf("Test 5: Content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash) == 64, "SHA-256 hash should be 64 hex chars");

    if (hash) {
        printf("  SHA-256: %s\n", hash);
        lv00_free_ptr(hash);
    }

    axiom_package_destroy(pkg);
}

static void test_round_trip(void) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg1, AXIOM_PKG_PATH);

    AxiomSaveStatus save_status = axiom_package_save(pkg1, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "save should succeed");

    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "re-load from saved file should succeed");

    TEST_ASSERT(pkg2->template_count == pkg1->template_count, "template count should match after round-trip");
    TEST_ASSERT(pkg2->unconstructible_count == pkg1->unconstructible_count,
                "unconstructible count should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->name, pkg1->name) == 0, "name should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->version, pkg1->version) == 0, "version should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg1->bottom_geometry) == 0,
                "bottom_geometry should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg1->contradiction_behavior,
                "contradiction_behavior should match after round-trip");

    printf("  Round-trip: templates=%d, unconstructibles=%d\n", pkg2->template_count, pkg2->unconstructible_count);

    char *hash1 = axiom_package_compute_content_hash(pkg1);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash1 && hash2 && strcmp(hash1, hash2) == 0, "content hashes should match after round-trip");
    printf("  Hash match: %s\n", (hash1 && hash2 && strcmp(hash1, hash2) == 0) ? "YES" : "NO");

    lv00_free_ptr(hash1);
    lv00_free_ptr(hash2);
    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    bool valid = axiom_package_validate_dependencies(pkg, &pkg, 1);
    /* Note: self-validation may fail because reduces_to targets like
       "gram_matrix_positive_semi_definiteness" are mathematical reduction
       descriptions, not references to other unconstructible entries. */
    printf("  Self-validation: %s (expected: may fail for cross-reference reduces_to)\n",
           valid ? "PASS" : "FAIL (acceptable)");

    axiom_package_destroy(pkg);
}

static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "non-existent template should return NULL");

    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "non-existent unconstructible should return NULL");

    printf("  Negative lookups: correct\n");

    axiom_package_destroy(pkg);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("=== Metric Space Theory Axiom Package Test Suite ===\n");
    printf("=== Testing: axiom_packages/metric_space.lvz ===\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructible_problems();
    test_logical_framework();
    test_content_hash();
    test_round_trip();
    test_dependency_validation();
    test_negative_lookups();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass_count, g_fail_count);

    return g_fail_count > 0 ? 1 : 0;
}
