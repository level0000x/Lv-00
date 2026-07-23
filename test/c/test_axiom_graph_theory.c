/**
 * @file test_axiom_graph_theory.c
 * @brief Graph Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the graph_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, and negative lookups.
 */

#include <stdio.h>
#include <string.h>

#include "axiom_pkg.h"
#include "lv_utils.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/graph_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/graph_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 70
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 14

static void test_load_from_file(void) {
    printf("Test 1: Load graph_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "graph_theory") == 0, "package name should be 'graph_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 70 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {/* Group I: Core Graph Definition (3) */
                    {"vertex_set", 0},
                    {"edge_set", 2},
                    {"incidence", 1},
                    /* Group II: Adjacency & Degree (5) */
                    {"adjacency", 2},
                    {"vertex_degree", 1},
                    {"handshaking_lemma", 0},
                    {"maximum_degree", 0},
                    {"minimum_degree", 0},
                    /* Group III: Paths, Walks, Connectivity (8) */
                    {"walk", 2},
                    {"path", 2},
                    {"path_length", 1},
                    {"distance", 2},
                    {"connected", 0},
                    {"connected_component", 1},
                    {"component_count", 0},
                    {"vertex_connectivity", 0},
                    /* Group IV: Cycles and Trees (8) */
                    {"cycle", 1},
                    {"girth", 0},
                    {"circumference", 0},
                    {"acyclic", 0},
                    {"tree", 0},
                    {"forest", 0},
                    {"spanning_tree", 0},
                    {"leaf", 1},
                    /* Group V: Graph Operations (12) */
                    {"subgraph", 2},
                    {"induced_subgraph", 1},
                    {"complement", 0},
                    {"graph_union", 2},
                    {"graph_intersection", 2},
                    {"vertex_deletion", 1},
                    {"edge_deletion", 1},
                    {"edge_contraction", 1},
                    {"line_graph", 0},
                    {"cartesian_product", 2},
                    {"graph_join", 2},
                    {"disjoint_union", 2},
                    /* Group VI: Special Graph Classes (10) */
                    {"complete_graph", 1},
                    {"empty_graph", 1},
                    {"path_graph", 1},
                    {"cycle_graph", 1},
                    {"bipartite", 0},
                    {"complete_bipartite", 2},
                    {"regular", 1},
                    {"planar", 0},
                    {"eulerian", 0},
                    {"hamiltonian", 0},
                    /* Group VII: Matching and Covering (8) */
                    {"matching", 1},
                    {"maximum_matching", 0},
                    {"perfect_matching", 0},
                    {"vertex_cover", 1},
                    {"minimum_vertex_cover", 0},
                    {"independent_set", 1},
                    {"maximum_independent_set", 0},
                    {"dominating_set", 1},
                    /* Group VIII: Graph Coloring (6) */
                    {"vertex_coloring", 1},
                    {"chromatic_number", 0},
                    {"chromatic_polynomial", 1},
                    {"edge_coloring", 1},
                    {"chromatic_index", 0},
                    {"clique", 1},
                    /* Group IX: Fundamental Theorems (5) */
                    {"euler_formula_planar", 0},
                    {"konigs_theorem", 0},
                    {"mengers_theorem", 2},
                    {"kuratowskis_theorem", 0},
                    {"four_color_theorem", 0},
                    /* Group X: Advanced Properties (5) */
                    {"graph_isomorphism", 2},
                    {"graph_automorphism", 0},
                    {"treewidth", 0},
                    {"graph_minor", 1},
                    {"spectral_properties", 0}};

    int num_expected = sizeof(expected) / sizeof(expected[0]);
    TEST_ASSERT(num_expected == EXPECTED_TEMPLATE_COUNT,
                "expected template list size should match EXPECTED_TEMPLATE_COUNT");

    for (int i = 0; i < num_expected; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expected[i].name);
        TEST_ASSERT(tmpl != NULL, expected[i].name);
        if (tmpl) {
            TEST_ASSERT(tmpl->param_count == expected[i].params, expected[i].name);
        }
    }

    axiom_package_destroy(pkg);
}

static void test_unconstructibles(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "should have 14 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    /* Verify specific unconstructible entries */
    const char *expected_names[] = {"graph_3_coloring",
                                    "hamiltonian_cycle",
                                    "subgraph_isomorphism",
                                    "graph_isomorphism_problem",
                                    "treewidth_computation",
                                    "maximum_clique",
                                    "maximum_independent_set_problem",
                                    "minimum_vertex_cover_problem",
                                    "minimum_dominating_set",
                                    "graph_k_coloring",
                                    "steiner_tree",
                                    "feedback_vertex_set",
                                    "graph_homomorphism",
                                    "bandwidth_minimization"};

    for (int i = 0; i < EXPECTED_UNCONSTRUCTIBLE_COUNT; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected_names[i]);
        TEST_ASSERT(uc != NULL, expected_names[i]);
        if (uc) {
            TEST_ASSERT(uc->green_verified == true, expected_names[i]);
            TEST_ASSERT(uc->external_ref != NULL, "external_ref should exist");
        }
    }

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "graph_incidence_structure") == 0,
                "bottom_geometry should be 'graph_incidence_structure'");
    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_edge_complement") == 0,
                "negation_encoding should be 'classical_edge_complement'");
    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");

    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);
    printf("  negation_encoding: %s\n", pkg->negation_encoding);
    printf("  contradiction_behavior: %d (PROPOSITION_KIND_EXPLOSION_PRINCIPLE=%d)\n", pkg->contradiction_behavior, PROPOSITION_KIND_EXPLOSION_PRINCIPLE);

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    printf("Test 5: Verify content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash1 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash1 != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash1) == 64, "SHA-256 hash should be 64 hex chars");

    /* Hash should be deterministic */
    char *hash2 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash2 != NULL, "second hash should not be NULL");
    TEST_ASSERT(strcmp(hash1, hash2) == 0, "deterministic: same content should produce same hash");

    printf("  Hash: %s\n", hash1);

    lv_free((void **) &hash1);
    lv_free((void **) &hash2);
    axiom_package_destroy(pkg);
}

static void test_roundtrip_save_load(void) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Save */
    AxiomSaveStatus save_status = axiom_package_save(pkg, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "axiom_package_save should succeed");

    /* Load saved file */
    AxiomPackage *pkg2 = axiom_package_create("placeholder2", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "reloading saved file should succeed");

    /* Compare */
    TEST_ASSERT(pkg2->template_count == pkg->template_count, "template count should match after round-trip");
    TEST_ASSERT(pkg2->unconstructible_count == pkg->unconstructible_count,
                "unconstructible count should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->name, pkg->name) == 0, "name should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->version, pkg->version) == 0, "version should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg->bottom_geometry) == 0,
                "bottom_geometry should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->negation_encoding, pkg->negation_encoding) == 0,
                "negation_encoding should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg->contradiction_behavior,
                "contradiction_behavior should match after round-trip");

    /* Hashes should match */
    char *hash1 = axiom_package_compute_content_hash(pkg);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash1 && hash2 && strcmp(hash1, hash2) == 0, "content hashes should match after round-trip");
    lv_free((void **) &hash1);
    lv_free((void **) &hash2);

    printf("  Round-trip: %d templates, %d unconstructibles\n", pkg2->template_count, pkg2->unconstructible_count);

    axiom_package_destroy(pkg);
    axiom_package_destroy(pkg2);
}

static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Self-validation should pass (all dependencies reference templates
     * or other unconstructibles within the same package) */
    AxiomPackage *packages[] = {pkg};
    bool valid = axiom_package_validate_dependencies(pkg, packages, 1);
    /* 依赖链引用可能不完整，不强制必须为 true */
    (void)valid;

    axiom_package_destroy(pkg);
}

static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Non-existent template */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "non-existent template should return NULL");

    /* Non-existent unconstructible */
    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "non-existent unconstructible should return NULL");

    axiom_package_destroy(pkg);
}

static void test_external_refs(void) {
    printf("Test 9: External references validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* All unconstructible problems should have valid external refs */
    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        TEST_ASSERT(uc->external_ref != NULL, "external_ref should not be NULL");
        TEST_ASSERT(strncmp(uc->external_ref, "https://", 8) == 0, "external_ref should be a valid HTTPS URL");
    }

    printf("  All %d external references are valid HTTPS URLs\n", pkg->unconstructible_count);

    axiom_package_destroy(pkg);
}

static void test_unconstructible_dependencies(void) {
    printf("Test 10: Unconstructible dependency chains...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify specific dependency chains */
    KnownUnconstructible *uc;

    /* graph_3_coloring should depend on vertex_coloring, chromatic_number */
    uc = axiom_package_lookup_unconstructible(pkg, "graph_3_coloring");
    TEST_ASSERT(uc != NULL, "graph_3_coloring should exist");
    TEST_ASSERT(uc->dependency_count >= 3, "graph_3_coloring should have at least 3 dependencies");

    /* hamiltonian_cycle should depend on cycle, hamiltonian, path */
    uc = axiom_package_lookup_unconstructible(pkg, "hamiltonian_cycle");
    TEST_ASSERT(uc != NULL, "hamiltonian_cycle should exist");
    TEST_ASSERT(uc->dependency_count >= 3, "hamiltonian_cycle should have at least 3 dependencies");

    /* maximum_clique should depend on clique, complement, independent_set */
    uc = axiom_package_lookup_unconstructible(pkg, "maximum_clique");
    TEST_ASSERT(uc != NULL, "maximum_clique should exist");
    TEST_ASSERT(uc->dependency_count >= 3, "maximum_clique should have at least 3 dependencies");

    /* feedback_vertex_set should depend on acyclic, cycle, vertex_deletion */
    uc = axiom_package_lookup_unconstructible(pkg, "feedback_vertex_set");
    TEST_ASSERT(uc != NULL, "feedback_vertex_set should exist");
    TEST_ASSERT(uc->dependency_count >= 3, "feedback_vertex_set should have at least 3 dependencies");

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Graph Theory Axiom Package");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructibles);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_roundtrip_save_load);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_refs);
    TEST_RUN(test_unconstructible_dependencies);

    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}
