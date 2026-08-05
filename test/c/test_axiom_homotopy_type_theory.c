/**
 * @file test_axiom_homotopy_type_theory.c
 * @brief Homotopy Type Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the homotopy_type_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Homotopy Type Theory (HoTT) provides a foundations of mathematics based on
 * the correspondence between types and spaces for Lv-00. The 37 templates
 * cover identity types, equivalences, universes, higher inductive types,
 * and the univalence axiom.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/homotopy_type_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/homotopy_type_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 37
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Identity Types (5) */
    {"identity_type_path", 2},
    {"path_induction", 3},
    {"refl_identity", 1},
    {"transport_identification", 2},
    {"concat_path", 2},
    /* Group II: Equivalences (6) */
    {"homotopy_equivalence", 2},
    {"quasi_inverse", 2},
    {"contr_map", 1},
    {"fiber_construction", 2},
    {"equivalence_induction", 2},
    {"ua_univalence", 1},
    /* Group III: Universes (5) */
    {"universe_type", 1},
    {"univalence_axiom", 2},
    {"ua_equivalence", 1},
    {"is_inhabited_universe", 1},
    {"large_universe", 0},
    /* Group IV: Higher Inductive Types (5) */
    {"circle_type", 0},
    {"sphere_type", 1},
    {"interval_type", 0},
    {"quotient_type", 2},
    {"propositional_truncation", 1},
    {"set_truncation", 2},
    /* Group V: Fibrations (5) */
    {"fiber_type", 2},
    {"total_space", 1},
    {"pullback_fibration", 2},
    {"cofiber_cospace", 1},
    {"suspension_loop", 2},
    /* Group VI: Homotopy Groups (5) */
    {"homotopy_group_type", 2},
    {"connectedness_level", 2},
    {"n_type_deck", 2},
    {"truncation_closure", 2},
    {"equivalences_power", 2},
    /* Group VII: Foundations (5) */
    {"hott_propositions_as_types", 1},
    {"hott_logic_levels", 1},
    {"propositional_resizing", 1},
    {"impredicative_universe", 1},
    {"classical_logic_hoq", 1},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"univalence_proof_checker", "undecidable", 4, true},
    {"canonicity_in_hoq", "open_problem", 5, true},
    {"set_membership_hott", "open_problem", 3, true},
    {"higher_inductive_coherence", "undecidable", 4, true},
    {"univalence_extensionality", "open_problem", 3, true},
    {"constructive_univalence", "open_problem", 4, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "homotopy_type_theory");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 37 constraint templates", k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructibles(void) {
    axiom_test_unconstructibles_min_deps(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                         "should have 6 unconstructible problems", k_unconstructibles,
                                         K_UNCONSTRUCTIBLES_COUNT);
}

/* Test 4：逻辑框架（文件特有：bottom_geometry 用 strstr 检查，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    TEST_ASSERT(
        strstr(pkg->bottom_geometry, "identity_type") != NULL || strstr(pkg->bottom_geometry, "homotopy") != NULL,
        "bottom_geometry should contain HoTT concepts");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    axiom_test_content_hash_deterministic(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_save_load_roundtrip(void) {
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "homotopy_type_theory", EXPECTED_TEMPLATE_COUNT,
                                    EXPECTED_UNCONSTRUCTIBLE_COUNT, AXIOM_TEST_FREE_LV_FREE);
}

static void test_dependency_validation(void) {
    axiom_test_dependency_validation_note(AXIOM_PKG_PATH, NULL);
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
    printf("Test 10: Key HoTT templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Identity types core */
    const char *identity_core[] = {"identity_type_path", "path_induction", "refl_identity"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, identity_core[i]);
        TEST_ASSERT(tmpl != NULL, "identity type template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Equivalence core */
    const char *equiv_core[] = {"homotopy_equivalence", "quasi_inverse", "ua_univalence"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, equiv_core[i]);
        TEST_ASSERT(tmpl != NULL, "equivalence template should exist");
    }

    /* Univalence axiom */
    ConstraintTemplate *ua = axiom_package_get_template(pkg, "univalence_axiom");
    TEST_ASSERT(ua != NULL, "univalence axiom should exist");

    /* Higher inductive types */
    ConstraintTemplate *s1 = axiom_package_get_template(pkg, "sphere_type");
    TEST_ASSERT(s1 != NULL, "sphere type should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Homotopy Type Theory");

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
