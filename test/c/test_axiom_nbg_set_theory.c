/**
 * @file test_axiom_nbg_set_theory.c
 * @brief NBG Set Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the nbg_set_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Von Neumann-Bernays-Godel (NBG) set theory is a conservative extension
 * of ZFC that provides a finitely axiomatizable set theory by introducing
 * classes as first-class objects. The 32 templates cover class axioms,
 * set axioms, choice principles, limitation of size, class comprehension,
 * proper class distinctions, set operations, and metatheoretic properties.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/nbg_set_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/nbg_set_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 32
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Class Existence Axioms (5) */
    {"class_extensionality", 2},
    {"class_pair_existence", 2},
    {"class_intersection", 2},
    {"class_complement", 1},
    {"class_domain", 1},
    /* Group II: Set Existence Axioms (5) */
    {"infinity", 0},
    {"union", 1},
    {"power_set", 1},
    {"replacement", 2},
    {"regularity", 1},
    /* Group III: Choice Principles (2) */
    {"local_choice", 1},
    {"global_choice", 0},
    /* Group IV: Limitation of Size (2) */
    {"limitation_of_size", 1},
    {"foundation", 1},
    /* Group V: Class Comprehension (2) */
    {"class_comprehension", 2},
    {"class_union", 1},
    /* Group VI: Proper Class Distinctions (6) */
    {"proper_class", 1},
    {"set_class_distinction", 1},
    {"ordinal_class", 1},
    {"cardinal_class", 1},
    {"universal_class_v", 0},
    {"cumulative_hierarchy", 1},
    /* Group VII: Set Operations (6) */
    {"set_pairing", 2},
    {"set_difference", 2},
    {"set_cartesian_product", 2},
    {"set_relation", 3},
    {"set_function", 3},
    {"set_image", 2},
    /* Group VIII: Metatheoretic Properties (4) */
    {"nbg_conservative_over_zfc", 1},
    {"class_comprehension_schema", 1},
    {"global_choice_implies_ac", 1},
    {"nbg_finite_axiomatizability", 0},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"nbg_continuum_hypothesis", "independent_of_nbg", 3, true},
    {"nbg_global_choice_consistency", "equiconsistent_with_zfc", 3, true},
    {"proper_class_cardinality", "undefined", 2, true},
    {"class_membership_decision", "undecidable", 2, true},
    {"nbg_incompleteness", "godel_incompleteness", 3, true},
    {"definable_class_characterization", "undecidable", 2, true},
    {"nbg_conservativity_verification", "undecidable", 2, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "nbg_set_theory");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 32 constraint templates", k_templates, K_TEMPLATES_COUNT);
}

static void test_unconstructibles(void) {
    axiom_test_unconstructibles_min_deps(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                         "should have 7 unconstructible problems", k_unconstructibles,
                                         K_UNCONSTRUCTIBLES_COUNT);
}

/* Test 4：逻辑框架（文件特有：bottom_geometry 双重 strstr 检查，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    /* bottom_geometry contains "cumulative_hierarchy" and "proper_class" */
    TEST_ASSERT(strstr(pkg->bottom_geometry, "cumulative_hierarchy") != NULL,
                "bottom_geometry should contain 'cumulative_hierarchy'");
    TEST_ASSERT(strstr(pkg->bottom_geometry, "proper_class") != NULL, "bottom_geometry should contain 'proper_class'");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    /* negation_encoding contains "complement" */
    TEST_ASSERT(strstr(pkg->negation_encoding, "complement") != NULL, "negation_encoding should contain 'complement'");
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
    axiom_test_round_trip_save_load(AXIOM_PKG_PATH, SAVE_TEST_PATH, "nbg_set_theory", EXPECTED_TEMPLATE_COUNT,
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
    printf("Test 10: Key NBG set theory templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core class existence axioms */
    const char *class_axioms[] = {"class_extensionality", "class_pair_existence", "class_intersection",
                                  "class_complement", "class_domain"};

    for (int i = 0; i < 5; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, class_axioms[i]);
        TEST_ASSERT(tmpl != NULL, "class existence axiom template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Core set existence axioms */
    const char *set_axioms[] = {"infinity", "union", "power_set", "replacement", "regularity"};

    for (int i = 0; i < 5; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, set_axioms[i]);
        TEST_ASSERT(tmpl != NULL, "set existence axiom template should exist");
    }

    /* Limitation of size and foundation */
    ConstraintTemplate *los = axiom_package_get_template(pkg, "limitation_of_size");
    TEST_ASSERT(los != NULL, "limitation_of_size template should exist");
    ConstraintTemplate *fnd = axiom_package_get_template(pkg, "foundation");
    TEST_ASSERT(fnd != NULL, "foundation template should exist");

    /* Choice principles */
    ConstraintTemplate *lc = axiom_package_get_template(pkg, "local_choice");
    TEST_ASSERT(lc != NULL, "local_choice template should exist");
    ConstraintTemplate *gc = axiom_package_get_template(pkg, "global_choice");
    TEST_ASSERT(gc != NULL, "global_choice template should exist");

    /* Proper class distinctions */
    ConstraintTemplate *pc = axiom_package_get_template(pkg, "proper_class");
    TEST_ASSERT(pc != NULL, "proper_class template should exist");
    ConstraintTemplate *scd = axiom_package_get_template(pkg, "set_class_distinction");
    TEST_ASSERT(scd != NULL, "set_class_distinction template should exist");

    /* Metatheoretic properties */
    ConstraintTemplate *con = axiom_package_get_template(pkg, "nbg_conservative_over_zfc");
    TEST_ASSERT(con != NULL, "nbg_conservative_over_zfc template should exist");
    ConstraintTemplate *fin = axiom_package_get_template(pkg, "nbg_finite_axiomatizability");
    TEST_ASSERT(fin != NULL, "nbg_finite_axiomatizability template should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("NBG Set Theory")

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

