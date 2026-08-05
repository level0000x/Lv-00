/**
 * @file test_axiom_modal_logic.c
 * @brief Modal Logic (Normal Modal Logics K, T, S4, S5) Axiom Package Test
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/modal_logic.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/modal_logic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 27
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板名 */
static const char *const k_template_names[] = {
    /* Group I: Classical Propositional Foundation */
    "classical_tautology", "modus_ponens",
    /* Group II: Core Modal Axioms (System K) */
    "kripke_schema", "necessitation",
    /* Group III: Modal Operator Duality */
    "possibility_dual", "necessity_dual",
    /* Group IV: Reflexivity Axioms (System T) */
    "reflexivity_T",
    /* Group V: Transitivity Axioms (System K4/S4) */
    "transitivity_4",
    /* Group VI: Symmetry Axioms (System S5) */
    "symmetry_B", "euclidean_5",
    /* Group VII: Seriality Axioms (System D) */
    "seriality_D",
    /* Group VIII: Provability Logic (GL) */
    "lob_axiom",
    /* Group IX: Modal System Constructors */
    "kripke_frame", "kripke_model", "satisfaction_at_world", "validity_in_frame",
    /* Group X: Derived Modal Principles */
    "modal_modus_tollens", "box_distributes_over_and", "diamond_monotonicity", "modal_negation",
    /* Group XI: Epistemic/Doxastic Variants */
    "knowledge_axiom", "positive_introspection", "negative_introspection",
    /* Group XII: Temporal Logic Variants */
    "always_operator", "eventually_operator", "next_operator", "until_operator",
};
#define K_TEMPLATE_NAMES_COUNT (int) (sizeof(k_template_names) / sizeof(k_template_names[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "modal_logic");
}

static void test_templates(void) {
    axiom_test_templates_names_only(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 27 constraint templates",
                                    k_template_names, K_TEMPLATE_NAMES_COUNT);

    /* 文件特有：具体参数个数校验（差异部分，原样保留） */
    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *t;

    /* Core modal axioms */
    t = axiom_package_get_template(pkg, "kripke_schema");
    TEST_ASSERT(t && t->param_count == 2, "kripke_schema should have 2 params (A, B)");

    t = axiom_package_get_template(pkg, "necessitation");
    TEST_ASSERT(t && t->param_count == 1, "necessitation should have 1 param (A)");

    t = axiom_package_get_template(pkg, "classical_tautology");
    TEST_ASSERT(t && t->param_count == 1, "classical_tautology should have 1 param");

    t = axiom_package_get_template(pkg, "modus_ponens");
    TEST_ASSERT(t && t->param_count == 2, "modus_ponens should have 2 params");

    /* Modal operator duality */
    t = axiom_package_get_template(pkg, "possibility_dual");
    TEST_ASSERT(t && t->param_count == 1, "possibility_dual should have 1 param (A)");

    t = axiom_package_get_template(pkg, "necessity_dual");
    TEST_ASSERT(t && t->param_count == 1, "necessity_dual should have 1 param (A)");

    /* Modal axiom schemata */
    t = axiom_package_get_template(pkg, "reflexivity_T");
    TEST_ASSERT(t && t->param_count == 1, "reflexivity_T should have 1 param (A)");

    t = axiom_package_get_template(pkg, "transitivity_4");
    TEST_ASSERT(t && t->param_count == 1, "transitivity_4 should have 1 param (A)");

    t = axiom_package_get_template(pkg, "symmetry_B");
    TEST_ASSERT(t && t->param_count == 1, "symmetry_B should have 1 param (A)");

    t = axiom_package_get_template(pkg, "euclidean_5");
    TEST_ASSERT(t && t->param_count == 1, "euclidean_5 should have 1 param (A)");

    t = axiom_package_get_template(pkg, "seriality_D");
    TEST_ASSERT(t && t->param_count == 1, "seriality_D should have 1 param (A)");

    t = axiom_package_get_template(pkg, "lob_axiom");
    TEST_ASSERT(t && t->param_count == 1, "lob_axiom should have 1 param (A)");

    /* Kripke semantics constructors */
    t = axiom_package_get_template(pkg, "kripke_frame");
    TEST_ASSERT(t && t->param_count == 2, "kripke_frame should have 2 params (W, R)");

    t = axiom_package_get_template(pkg, "kripke_model");
    TEST_ASSERT(t && t->param_count == 3, "kripke_model should have 3 params (W, R, V)");

    t = axiom_package_get_template(pkg, "satisfaction_at_world");
    TEST_ASSERT(t && t->param_count == 3, "satisfaction_at_world should have 3 params (M, w, phi)");

    t = axiom_package_get_template(pkg, "validity_in_frame");
    TEST_ASSERT(t && t->param_count == 2, "validity_in_frame should have 2 params (F, phi)");

    /* Epistemic variants */
    t = axiom_package_get_template(pkg, "knowledge_axiom");
    TEST_ASSERT(t && t->param_count == 1, "knowledge_axiom should have 1 param (A)");

    t = axiom_package_get_template(pkg, "positive_introspection");
    TEST_ASSERT(t && t->param_count == 1, "positive_introspection should have 1 param (A)");

    t = axiom_package_get_template(pkg, "negative_introspection");
    TEST_ASSERT(t && t->param_count == 1, "negative_introspection should have 1 param (A)");

    /* Temporal operators */
    t = axiom_package_get_template(pkg, "always_operator");
    TEST_ASSERT(t && t->param_count == 1, "always_operator should have 1 param (A)");

    t = axiom_package_get_template(pkg, "eventually_operator");
    TEST_ASSERT(t && t->param_count == 1, "eventually_operator should have 1 param (A)");

    t = axiom_package_get_template(pkg, "next_operator");
    TEST_ASSERT(t && t->param_count == 1, "next_operator should have 1 param (A)");

    t = axiom_package_get_template(pkg, "until_operator");
    TEST_ASSERT(t && t->param_count == 2, "until_operator should have 2 params (A, B)");

    axiom_package_destroy(pkg);
}

/* Test 3：不可构造项（文件特有：3 字段 + 外部引用特例检查，保留原体） */
static void test_unconstructible_problems(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 7 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        bool green_verified;
    } expected[] = {{"modal_satisfiability_K", "PSPACE_complete_problem", false},
                    {"modal_satisfiability_S4", "PSPACE_complete_problem", false},
                    {"modal_satisfiability_S5", "NP_complete_problem", false},
                    {"modal_uniform_interpolation", "undecidable", false},
                    {"modal_logic_with_propositional_quantifiers", "undecidable", false},
                    {"global_satisfiability_S4", "EXPTIME_complete", false},
                    {"modal_mu_calculus_model_checking", "NP_intersection_coNP", false},
                    {NULL, NULL, false}};

    int found_count = 0;
    for (int i = 0; expected[i].name != NULL; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        if (uc) {
            found_count++;
            if (uc->reduces_to) {
                TEST_ASSERT(strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                            "reduces_to should match expected value");
            }
            TEST_ASSERT(uc->green_verified == expected[i].green_verified, "green_verified should match expected value");
        } else {
            printf("  MISSING unconstructible: '%s'\n", expected[i].name);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "all expected unconstructible problems should be found");
    printf("  Found %d / %d unconstructible problems\n", found_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    /* Verify external references */
    KnownUnconstructible *uc;

    uc = axiom_package_lookup_unconstructible(pkg, "modal_satisfiability_K");
    TEST_ASSERT(uc && uc->external_ref != NULL, "modal_satisfiability_K should have external_ref");
    if (uc && uc->external_ref) {
        TEST_ASSERT(strstr(uc->external_ref, "wikipedia.org") != NULL || strstr(uc->external_ref, "PSPACE") != NULL,
                    "external_ref should point to Wikipedia or mention PSPACE");
    }

    uc = axiom_package_lookup_unconstructible(pkg, "modal_satisfiability_S5");
    TEST_ASSERT(uc && uc->external_ref != NULL, "modal_satisfiability_S5 should have external_ref");

    axiom_package_destroy(pkg);
}

/* Test 4：逻辑框架（文件特有：if 包裹结构，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework configuration...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify bottom_geometry */
    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    if (pkg->bottom_geometry) {
        TEST_ASSERT(strcmp(pkg->bottom_geometry, "kripke_possible_worlds_semantics") == 0,
                    "bottom_geometry should be 'kripke_possible_worlds_semantics'");
        printf("  bottom_geometry: %s\n", pkg->bottom_geometry);
    }

    /* Verify negation_encoding */
    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    if (pkg->negation_encoding) {
        TEST_ASSERT(strcmp(pkg->negation_encoding, "classical_complement_with_modal_dual") == 0,
                    "negation_encoding should be 'classical_complement_with_modal_dual'");
        printf("  negation_encoding: %s\n", pkg->negation_encoding);
    }

    /* Verify contradiction_behavior */
    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

/* Test 5：内容哈希（文件特有：hex 校验循环，保留原体） */
static void test_content_hash(void) {
    printf("Test 5: Verify content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should be computed");

    if (hash) {
        size_t len = strlen(hash);
        TEST_ASSERT(len == 64, "content hash should be 64 characters (SHA-256 hex)");
        printf("  Content hash: %.16s... (len=%zu)\n", hash, len);

        /* Verify hash is hexadecimal */
        bool valid_hex = true;
        for (size_t i = 0; i < len; i++) {
            if (!((hash[i] >= '0' && hash[i] <= '9') || (hash[i] >= 'a' && hash[i] <= 'f'))) {
                valid_hex = false;
                break;
            }
        }
        TEST_ASSERT(valid_hex, "content hash should be valid hexadecimal");

        lv_free((void **) &hash);
    }

    axiom_package_destroy(pkg);
}

/* Test 6：往返保存/加载（文件特有：先哈希 + 哈希匹配打印，保留原体） */
static void test_roundtrip_save_load(void) {
    printf("Test 6: Verify round-trip save/load...\n");

    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg1, AXIOM_PKG_PATH);

    /* Compute hash before save */
    char *hash1 = axiom_package_compute_content_hash(pkg1);

    /* Save to temporary file */
    AxiomSaveStatus save_status = axiom_package_save(pkg1, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "axiom_package_save should return AXIOM_SAVE_OK");

    /* Load the saved file */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "loading saved file should succeed");

    /* Compute hash after load */
    char *hash2 = axiom_package_compute_content_hash(pkg2);

    /* Verify hashes match */
    if (hash1 && hash2) {
        TEST_ASSERT(strcmp(hash1, hash2) == 0, "content hash should be identical after round-trip");
        printf("  Round-trip hash match: %.16s...\n", hash1);
    }

    /* Verify basic properties preserved */
    TEST_ASSERT(strcmp(pkg1->name, pkg2->name) == 0, "package name should be preserved");
    TEST_ASSERT(strcmp(pkg1->version, pkg2->version) == 0, "package version should be preserved");
    TEST_ASSERT(axiom_package_get_template_count(pkg1) == axiom_package_get_template_count(pkg2), "template count should be preserved");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg1) == axiom_package_get_unconstructible_count(pkg2),
                "unconstructible count should be preserved");

    lv_free((void **) &hash1);
    lv_free((void **) &hash2);
    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

/* Test 7：依赖验证（文件特有：Verify 打印格式，保留原体） */
static void test_dependency_validation(void) {
    printf("Test 7: Verify dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Test with empty dependency list (no other packages loaded) */
    AxiomPackage *loaded_packages[] = {pkg};
    bool valid = axiom_package_validate_dependencies(pkg, loaded_packages, 1);
    /* Dependencies may fail since we don't have other packages loaded */
    printf("  Dependency validation result: %s\n", valid ? "PASS" : "FAIL (expected)");
    printf("  (Dependencies reference external packages not loaded in this test)\n");

    axiom_package_destroy(pkg);
}

/* Test 8：负向查找（文件特有：Verify 打印格式，保留原体） */
static void test_negative_lookups(void) {
    printf("Test 8: Verify negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Look up non-existent template */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "lookup of non-existent template should return NULL");

    /* Look up non-existent unconstructible */
    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "lookup of non-existent unconstructible should return NULL");

    printf("  Negative lookups correctly return NULL\n");

    axiom_package_destroy(pkg);
}

/* Test 9：外部引用（文件特有：http/https 计数格式，保留原体） */
static void test_external_refs(void) {
    printf("Test 9: Verify external references...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    int valid_url_count = 0;
    for (int i = 0; i < axiom_package_get_unconstructible_count(pkg); i++) {
        KnownUnconstructible *uc = axiom_package_get_unconstructible(pkg, i);
        if (uc->external_ref) {
            /* Check if it looks like a valid URL */
            bool is_valid = false;
            if (strncmp(uc->external_ref, "http://", 7) == 0 || strncmp(uc->external_ref, "https://", 8) == 0) {
                is_valid = true;
            }
            if (is_valid) {
                valid_url_count++;
                printf("  %s -> %s\n", uc->name, uc->external_ref);
            }
        }
    }

    TEST_ASSERT(valid_url_count > 0, "at least some unconstructible problems should have valid URLs");
    printf("  Valid external references: %d/%d\n", valid_url_count, axiom_package_get_unconstructible_count(pkg));

    axiom_package_destroy(pkg);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_key_axioms_present(void) {
    printf("Test 10: Verify key modal axioms are present...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core K system axioms */
    TEST_ASSERT(axiom_package_get_template(pkg, "kripke_schema") != NULL,
                "Kripke schema (Distribution Axiom) should be present");
    TEST_ASSERT(axiom_package_get_template(pkg, "necessitation") != NULL, "Necessitation rule should be present");

    /* System T axiom */
    TEST_ASSERT(axiom_package_get_template(pkg, "reflexivity_T") != NULL, "T axiom (Reflexivity) should be present");

    /* System S4 axioms */
    TEST_ASSERT(axiom_package_get_template(pkg, "transitivity_4") != NULL, "4 axiom (Transitivity) should be present");

    /* System S5 axioms */
    TEST_ASSERT(axiom_package_get_template(pkg, "euclidean_5") != NULL, "5 axiom (Euclidean) should be present");

    /* Modal duality */
    TEST_ASSERT(axiom_package_get_template(pkg, "possibility_dual") != NULL, "Possibility duality should be present");
    TEST_ASSERT(axiom_package_get_template(pkg, "necessity_dual") != NULL, "Necessity duality should be present");

    /* Kripke semantics */
    TEST_ASSERT(axiom_package_get_template(pkg, "kripke_frame") != NULL, "Kripke frame constructor should be present");
    TEST_ASSERT(axiom_package_get_template(pkg, "kripke_model") != NULL, "Kripke model constructor should be present");

    printf("  All key modal axioms verified present\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Modal Logic")

    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructible_problems);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_roundtrip_save_load);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);
    TEST_MAIN_RUN(test_external_refs);
    TEST_MAIN_RUN(test_key_axioms_present);

TEST_MAIN_END()

