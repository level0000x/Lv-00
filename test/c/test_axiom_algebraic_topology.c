/**
 * @file test_axiom_algebraic_topology.c
 * @brief Algebraic Topology Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the algebraic_topology.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Algebraic topology is formalized through 38 templates covering fundamental
 * groups, homology, cohomology, homotopy theory, fixed point theorems,
 * and K-theory.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/algebraic_topology.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/algebraic_topology_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 38
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Fundamental Group (6) */
    {"fundamental_group", 2},
    {"fundamental_groupoid", 1},
    {"covering_space", 3},
    {"universal_cover", 3},
    {"monodromy", 3},
    {"van_kampen_theorem", 4},
    /* Group II: Simplicial Homology (7) */
    {"simplicial_complex", 1},
    {"simplicial_homology", 2},
    {"chain_complex", 2},
    {"boundary_operator", 3},
    {"homology_group", 2},
    {"euler_characteristic", 1},
    {"betti_number", 2},
    /* Group III: Singular Homology & Cohomology (6) */
    {"singular_homology", 3},
    {"singular_cohomology", 3},
    {"mayer_vietoris_sequence", 3},
    {"excision_theorem", 3},
    {"universal_coefficient_theorem", 3},
    {"kuenneth_formula", 3},
    /* Group IV: Cohomology Operations (5) */
    {"cup_product", 4},
    {"cohomology_ring", 2},
    {"poincare_duality", 4},
    {"de_rham_cohomology", 2},
    {"sheaf_cohomology", 3},
    /* Group V: Homotopy Theory (6) */
    {"homotopy_group", 3},
    {"hurewicz_theorem", 2},
    {"whitehead_theorem", 3},
    {"fibration", 3},
    {"cofibration", 3},
    {"long_exact_sequence_fibration", 3},
    /* Group VI: Fixed Point & Intersection (4) */
    {"lefschetz_fixed_point", 2},
    {"lefschetz_number", 2},
    {"intersection_theory", 3},
    {"poincare_lemma", 2},
    /* Group VII: K-Theory (4) */
    {"vector_bundle", 3},
    {"k_group", 1},
    {"bott_periodicity", 1},
    {"atiyah_singer_index", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"homotopy_group_computation", "undecidable", 3, true},
    {"homology_isomorphism_problem", "undecidable", 3, true},
    {"knot_classification", "undecidable", 3, true},
    {"homeomorphism_problem_manifolds", "undecidable", 3, true},
    {"simple_homotopy_equivalence", "undecidable", 3, true},
    {"group_presentation_triviality", "undecidable", 3, true},
    {"manifold_triangulation", "undecidable", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "algebraic_topology");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 38 constraint templates", k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructibles(void) {
    axiom_test_unconstructibles_min_deps(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                         "should have 7 unconstructible problems", k_unconstructibles,
                                         K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework_presence(AXIOM_PKG_PATH, PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                                          "PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
}

static void test_content_hash(void) {
    axiom_test_content_hash_deterministic(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_save_load_roundtrip(void) {
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "algebraic_topology", EXPECTED_TEMPLATE_COUNT,
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
    printf("Test 10: Key algebraic topology templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Fundamental group core */
    const char *fundamental_group_core[] = {"fundamental_group", "fundamental_groupoid",
                                            "covering_space",    "universal_cover",
                                            "monodromy",         "van_kampen_theorem"};

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, fundamental_group_core[i]);
        TEST_ASSERT(tmpl != NULL, "fundamental group core template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Homology core */
    const char *homology_core[] = {"simplicial_complex", "simplicial_homology", "chain_complex",
                                   "boundary_operator",  "homology_group",      "singular_homology"};

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, homology_core[i]);
        TEST_ASSERT(tmpl != NULL, "homology core template should exist");
    }

    /* Cohomology operations */
    const char *cohomology_ops[] = {"cup_product", "cohomology_ring", "poincare_duality", "de_rham_cohomology"};

    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, cohomology_ops[i]);
        TEST_ASSERT(tmpl != NULL, "cohomology operation template should exist");
    }

    /* K-theory */
    ConstraintTemplate *vb = axiom_package_get_template(pkg, "vector_bundle");
    TEST_ASSERT(vb != NULL, "vector_bundle template should exist");
    ConstraintTemplate *kg = axiom_package_get_template(pkg, "k_group");
    TEST_ASSERT(kg != NULL, "k_group template should exist");
    ConstraintTemplate *bp = axiom_package_get_template(pkg, "bott_periodicity");
    TEST_ASSERT(bp != NULL, "bott_periodicity template should exist");
    ConstraintTemplate *asi = axiom_package_get_template(pkg, "atiyah_singer_index");
    TEST_ASSERT(asi != NULL, "atiyah_singer_index template should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Algebraic Topology")

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

