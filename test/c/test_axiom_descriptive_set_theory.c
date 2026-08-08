/**
 * @file test_axiom_descriptive_set_theory.c
 * @brief Descriptive Set Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the descriptive_set_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Descriptive set theory is formalized through 39 templates covering Polish
 * spaces, Borel sets and hierarchy, analytic/coanalytic sets, projective
 * hierarchy, regularity properties, determinacy, and Borel equivalence relations.
 */

#include <stdio.h>
#include <string.h>

int g_fail_count = 0;
int g_pass_count = 0;

/* 历史私有 TEST_ASSERT 为非返回式语义（失败仅计数、继续执行），
 * 通过 AXIOM_TEST_NON_RETURNING 让骨架头提供兼容变体，保持行为不变 */
#include "test_helpers.h"

#include "axiom_test_common.h"

#define AXIOM_PKG_PATH "module/axiom_packages/descriptive_set_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/descriptive_set_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 39
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Polish Spaces (6) */
    {"polish_space", 1},
    {"baire_space", 0},
    {"cantor_space", 0},
    {"hilbert_cube", 0},
    {"borel_isomorphism", 2},
    {"polish_as_g_delta_in_hilbert_cube", 2},
    /* Group II: Borel Sets and Borel Hierarchy (7) */
    {"borel_sigma_algebra", 2},
    {"sigma_0_1_open", 2},
    {"pi_0_alpha_complement", 3},
    {"sigma_0_delta_union", 3},
    {"delta_0_alpha_intersection", 4},
    {"borel_hierarchy_inclusion", 3},
    {"borel_code", 2},
    /* Group III: Analytic and Coanalytic Sets (5) */
    {"analytic_set_sigma_1_1", 5},
    {"suslin_theorem", 4},
    {"coanalytic_set_pi_1_1", 3},
    {"analytic_separation", 3},
    {"coanalytic_uniformization", 2},
    /* Group IV: Projective Hierarchy (6) */
    {"projective_base_sigma_1_1", 2},
    {"projective_dual_pi_1_n", 3},
    {"projective_projection_sigma_1_n_plus_1", 4},
    {"projective_intersection_delta_1_n", 4},
    {"projective_hierarchy_inclusion", 3},
    {"projective_continuous_preimage", 3},
    /* Group V: Regularity Properties (6) */
    {"property_of_baire", 3},
    {"perfect_set_property", 3},
    {"lebesgue_measurability", 2},
    {"borel_regularity", 4},
    {"analytic_perfect_set_property", 2},
    {"coanalytic_regularity_failure", 2},
    /* Group VI: Determinacy and Infinite Games (5) */
    {"infinite_game", 3},
    {"determinacy", 2},
    {"borel_determinacy", 2},
    {"projective_determinacy", 2},
    {"determinacy_regularity_consequences", 4},
    /* Group VII: Borel Equivalence Relations (4) */
    {"borel_equivalence_relation", 3},
    {"smooth_equivalence_relation", 2},
    {"hyperfinite_equivalence_relation", 2},
    {"borel_reducibility", 3},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"projective_set_determinacy", "independent_of_ZFC", 2, true},
    {"analytic_set_completeness", "coanalytic", 1, true},
    {"coanalytic_uniformization", "requires_choice_axiom", 2, true},
    {"projective_hierarchy_collapse", "independent_of_ZFC", 2, true},
    {"borel_rank_computation", "undecidable", 1, true},
    {"wadge_degree_comparison", "undecidable", 1, true},
    {"borel_equivalence_classification", "undecidable", 1, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "descriptive_set_theory");
}

static void test_templates(void) {
    axiom_test_templates_with_params_min(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT,
                                         "should have 39 constraint templates", k_templates, K_TEMPLATES_COUNT);
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

/* Test 6：往返保存/加载（文件特有：无 remove 清理，保留原体） */
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

    TEST_ASSERT(strcmp(pkg2->name, "descriptive_set_theory") == 0, "reloaded package should have same name");
    TEST_ASSERT(strcmp(pkg2->version, "1.0.0") == 0, "reloaded package should have same version");
    TEST_ASSERT(axiom_package_get_template_count(pkg2) == EXPECTED_TEMPLATE_COUNT,
                "reloaded package should have same template count");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg2) == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "reloaded package should have same unconstructible count");

    char *hash_reload = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash_reload != NULL, "reloaded hash should be computable");
    TEST_ASSERT(strcmp(hash_orig, hash_reload) == 0, "content hash should survive round-trip");

    lv_free((void **) &hash_orig);
    lv_free((void **) &hash_reload);
    axiom_package_destroy(pkg2);
}

/* Test 7：依赖验证（文件特有：NULL 列表 + 期望失败，保留原体） */
static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Validate with no other packages loaded — cross-package deps should fail */
    bool valid = axiom_package_validate_dependencies(pkg, NULL, 0);
    TEST_ASSERT(valid == false, "dependency validation should fail with no loaded packages (cross-package deps)");

    axiom_package_destroy(pkg);
}

/* Test 8：负向查找（文件特有：消息文本不同，保留原体） */
static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Nonexistent template */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "nonexistent template should return NULL");

    /* Nonexistent unconstructible */
    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "nonexistent unconstructible should return NULL");

    axiom_package_destroy(pkg);
}

/* Test 9：外部引用（文件特有：非空 + HTTPS 检查，保留原体） */
static void test_external_references(void) {
    printf("Test 9: External references...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* All unconstructible problems should have external references */
    for (int i = 0; i < axiom_package_get_unconstructible_count(pkg); i++) {
        KnownUnconstructible *uc = axiom_package_get_unconstructible(pkg, i);
        TEST_ASSERT(uc->external_ref != NULL && uc->external_ref[0] != '\0',
                    "every unconstructible should have an external reference");
        /* Verify it starts with https:// */
        TEST_ASSERT(strncmp(uc->external_ref, "https://", 8) == 0, "external reference should be a valid URL");
    }

    axiom_package_destroy(pkg);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_key_templates_present(void) {
    printf("Test 10: Key templates present...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify key DST concepts are represented as templates */
    const char *key_templates[] = {
        "polish_space",           "borel_sigma_algebra",
        "analytic_set_sigma_1_1", "projective_projection_sigma_1_n_plus_1",
        "borel_determinacy",      "borel_equivalence_relation",
    };

    int key_count = sizeof(key_templates) / sizeof(key_templates[0]);
    for (int i = 0; i < key_count; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, key_templates[i]);
        TEST_ASSERT(tmpl != NULL, "key template should exist");
        if (!tmpl) {
            printf("  FAIL: key template '%s' not found\n", key_templates[i]);
        }
    }

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Descriptive Set Theory Axiom Package Tests")
    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructibles);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_save_load_roundtrip);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);
    TEST_MAIN_RUN(test_external_references);
    TEST_MAIN_RUN(test_key_templates_present);
TEST_MAIN_END()
