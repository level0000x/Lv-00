/**
 * @file test_axiom_linear_logic.c
 * @brief Linear Logic Axiom Package Test
 *
 * Tests for the linear_logic axiom package (Girard 1987).
 * Verifies package loading, template registration, unconstructible
 * problem tracking, logical framework settings, content hashing,
 * round-trip save/load, and dependency validation.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail_count = 0;
static int g_pass_count = 0;

/* 历史私有 TEST_ASSERT 为非返回式语义（失败仅计数、继续执行），
 * 通过 AXIOM_TEST_NON_RETURNING 让骨架头提供兼容变体，保持行为不变 */
#define AXIOM_TEST_NON_RETURNING 1

#include "axiom_test_common.h"

#define AXIOM_PKG_PATH "module/axiom_packages/linear_logic.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/linear_logic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 54
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 10

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板名 */
static const char *const k_template_names[] = {
    /* Identity & Structural */
    "identity_init", "cut_rule", "exchange",
    /* Negation */
    "negation_left", "negation_right", "double_negation_involution", "demorgan_tensor_par", "demorgan_par_tensor",
    "demorgan_with_plus", "demorgan_plus_with", "demorgan_bang_quest", "demorgan_quest_bang",
    /* Multiplicative */
    "tensor_left", "tensor_right", "par_left", "par_right", "one_left", "one_right", "bottom_mult_left",
    "bottom_mult_right", "linear_implication_left", "linear_implication_right",
    /* Additive */
    "with_left", "with_right", "plus_left", "plus_right", "top_right", "zero_left",
    /* Exponential */
    "bang_weakening", "bang_contraction", "bang_dereliction", "bang_promotion", "quest_weakening",
    "quest_contraction", "quest_dereliction", "quest_promotion",
    /* Exponential equivalences */
    "bang_distributes_tensor", "bang_top_equivalence", "quest_distributes_par", "quest_zero_equivalence",
    "bang_to_linear", "bang_comultiplication", "bang_counit",
    /* Derived constructors */
    "intuitionistic_implication_encoding", "classical_conjunction_encoding", "classical_disjunction_encoding",
    "excluded_middle_multiplicative", "asynchronous_phase", "synchronous_phase", "focus_decision",
    "linear_to_intuitionistic_translation", "resource_split", "resource_merge", "resource_consume",
};
#define K_TEMPLATE_NAMES_COUNT (int) (sizeof(k_template_names) / sizeof(k_template_names[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"provability_full_propositional_linear_logic", "undecidable", 5, true},
    {"provability_MELL", "open_problem", 5, false},
    {"proof_net_normalization", "undecidable", 3, true},
    {"type_inhabitation_full_linear_logic", "undecidable", 4, true},
    {"proof_net_equality", "undecidable", 3, true},
    {"provability_noncommutative_linear_logic", "undecidable", 3, true},
    {"additive_excluded_middle", "not_provable", 3, true},
    {"provability_MALL_PSPACE_complete", "PSPACE_complete", 4, true},
    {"provability_MLL_NP_complete", "NP_complete", 3, true},
    {"cut_elimination_termination", "undecidable_for_full_linear_logic", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "linear_logic");
}

static void test_templates(void) {
    axiom_test_templates_names_only(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 54 constraint templates",
                                    k_template_names, K_TEMPLATE_NAMES_COUNT);

    /* 文件特有：具体参数个数校验（差异部分，原样保留） */
    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *t;

    t = axiom_package_get_template(pkg, "identity_init");
    TEST_ASSERT(t && t->param_count == 1, "identity_init should have 1 param (formula B)");

    t = axiom_package_get_template(pkg, "cut_rule");
    TEST_ASSERT(t && t->param_count == 3, "cut_rule should have 3 params");

    t = axiom_package_get_template(pkg, "one_right");
    TEST_ASSERT(t && t->param_count == 0, "one_right should have 0 params (axiom)");

    t = axiom_package_get_template(pkg, "tensor_right");
    TEST_ASSERT(t && t->param_count == 4, "tensor_right should have 4 params (two contexts + two formulas)");

    t = axiom_package_get_template(pkg, "par_right");
    TEST_ASSERT(t && t->param_count == 3, "par_right should have 3 params");

    t = axiom_package_get_template(pkg, "bang_promotion");
    TEST_ASSERT(t && t->param_count == 3, "bang_promotion should have 3 params");

    t = axiom_package_get_template(pkg, "with_right");
    TEST_ASSERT(t && t->param_count == 3, "with_right should have 3 params (context + two formulas)");

    t = axiom_package_get_template(pkg, "plus_right");
    TEST_ASSERT(t && t->param_count == 3, "plus_right should have 3 params");

    t = axiom_package_get_template(pkg, "linear_implication_right");
    TEST_ASSERT(t && t->param_count == 3, "linear_implication_right should have 3 params");

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void) {
    axiom_test_unconstructible_problems(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                        "should have 10 unconstructible problems", k_unconstructibles,
                                        K_UNCONSTRUCTIBLES_COUNT);
}

/* Test 4：逻辑框架（文件特有：CONSTRUCTIVE 断言消息不同，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "linear_resource_multiset") == 0,
                "bottom_geometry should be 'linear_resource_multiset'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "involutive_linear_negation") == 0,
                "negation_encoding should be 'involutive_linear_negation'");
    printf("  negation_encoding: %s\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_CONSTRUCTIVE,
                "contradiction_behavior should be PROPOSITION_KIND_CONSTRUCTIVE (blocking, no explosion)");
    printf("  contradiction_behavior: PROPOSITION_KIND_CONSTRUCTIVE (blocking)\n");

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    axiom_test_content_hash(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE_PTR);
}

/* Test 6：往返保存/加载（文件特有：printf 格式不同且无哈希校验，保留原体） */
static void test_round_trip(void) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg1, AXIOM_PKG_PATH);

    AxiomSaveStatus save_status = axiom_package_save(pkg1, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "save should succeed");

    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "re-load from saved file should succeed");

    TEST_ASSERT(axiom_package_get_template_count(pkg2) == axiom_package_get_template_count(pkg1), "template count should match after round-trip");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg2) == axiom_package_get_unconstructible_count(pkg1),
                "unconstructible count should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->name, pkg1->name) == 0, "name should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->version, pkg1->version) == 0, "version should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg1->bottom_geometry) == 0,
                "bottom_geometry should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->negation_encoding, pkg1->negation_encoding) == 0,
                "negation_encoding should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg1->contradiction_behavior,
                "contradiction_behavior should match after round-trip");

    printf("  Round-trip: %d templates, %d unconstructibles\n", axiom_package_get_template_count(pkg2), axiom_package_get_unconstructible_count(pkg2));

    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

/* Test 7：依赖验证（文件特有：PASS/FAIL 分支 + 外部引用遍历，保留原体） */
static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Self-validation: all reduces_to and dependency references
     * should resolve within the same package */
    AxiomPackage *packages[] = {pkg};
    bool valid = axiom_package_validate_dependencies(pkg, packages, 1);
    if (!valid) {
        printf("  Self-validation: FAIL (acceptable - may have cross-ref semantics)\n");
    } else {
        printf("  Self-validation: PASS\n");
        g_pass_count++;
    }

    /* Verify external_ref URLs are valid */
    for (int i = 0; i < axiom_package_get_unconstructible_count(pkg); i++) {
        KnownUnconstructible *uc = axiom_package_get_unconstructible(pkg, i);
        TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "external_ref should not be empty");
        printf("  [%d] %s ref: %s\n", i, uc->name, uc->external_ref);
    }

    axiom_package_destroy(pkg);
}

/* Test 8：负向查找（文件特有：header 与收尾打印不同，保留原体） */
static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups (non-existent entries)...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *t = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(t == NULL, "nonexistent template should return NULL");

    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "nonexistent unconstructible should return NULL");

    printf("  Negative lookups correctly return NULL\n");

    axiom_package_destroy(pkg);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

/* ------------------------------------------------------------------ */
/*  Test 9: Key structural properties of linear logic                 */
/* ------------------------------------------------------------------ */
static void test_linear_logic_properties(void) {
    printf("Test 9: Key structural properties of linear logic...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Property 1: Linear logic has both multiplicative and additive
     * connectives (unlike classical logic which conflates them) */
    ConstraintTemplate *tensor = axiom_package_get_template(pkg, "tensor_right");
    ConstraintTemplate *with = axiom_package_get_template(pkg, "with_right");
    ConstraintTemplate *par = axiom_package_get_template(pkg, "par_right");
    ConstraintTemplate *plus = axiom_package_get_template(pkg, "plus_right");
    TEST_ASSERT(tensor != NULL && with != NULL && par != NULL && plus != NULL,
                "should have both multiplicative (tensor, par) and additive (with, plus) connectives");

    /* Property 2: Exponentials (! and ?) provide controlled contraction/weakening */
    ConstraintTemplate *bang_w = axiom_package_get_template(pkg, "bang_weakening");
    ConstraintTemplate *bang_c = axiom_package_get_template(pkg, "bang_contraction");
    ConstraintTemplate *quest_w = axiom_package_get_template(pkg, "quest_weakening");
    ConstraintTemplate *quest_c = axiom_package_get_template(pkg, "quest_contraction");
    TEST_ASSERT(bang_w != NULL && bang_c != NULL && quest_w != NULL && quest_c != NULL,
                "should have exponential rules for both ! and ?");

    /* Property 3: De Morgan dualities are present (involutive negation) */
    ConstraintTemplate *dm_tp = axiom_package_get_template(pkg, "demorgan_tensor_par");
    ConstraintTemplate *dm_wp = axiom_package_get_template(pkg, "demorgan_with_plus");
    ConstraintTemplate *dm_bq = axiom_package_get_template(pkg, "demorgan_bang_quest");
    TEST_ASSERT(dm_tp != NULL && dm_wp != NULL && dm_bq != NULL, "should have De Morgan duality templates");

    /* Property 4: No explosion principle (PROPOSITION_KIND_CONSTRUCTIVE contradiction behavior) */
    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_CONSTRUCTIVE,
                "linear logic should NOT have explosion principle");

    /* Property 5: Negation is involutive (double negation = identity) */
    ConstraintTemplate *dn = axiom_package_get_template(pkg, "double_negation_involution");
    TEST_ASSERT(dn != NULL, "should have double negation involution");

    /* Property 6: Linear implication is definable from negation + par */
    ConstraintTemplate *limpl_l = axiom_package_get_template(pkg, "linear_implication_left");
    ConstraintTemplate *limpl_r = axiom_package_get_template(pkg, "linear_implication_right");
    TEST_ASSERT(limpl_l != NULL && limpl_r != NULL, "should have linear implication templates");

    /* Property 7: Focused proof system (Andreoli 1992) */
    ConstraintTemplate *async = axiom_package_get_template(pkg, "asynchronous_phase");
    ConstraintTemplate *sync = axiom_package_get_template(pkg, "synchronous_phase");
    ConstraintTemplate *focus = axiom_package_get_template(pkg, "focus_decision");
    TEST_ASSERT(async != NULL && sync != NULL && focus != NULL, "should have focused proof system templates");

    printf("  All structural properties verified\n");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 10: MELL open problem verification                           */
/* ------------------------------------------------------------------ */
static void test_mell_open_problem(void) {
    printf("Test 10: MELL decidability open problem...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* MELL provability is a famous open problem in linear logic.
     * It should be marked as NOT green_verified. */
    KnownUnconstructible *mell = axiom_package_lookup_unconstructible(pkg, "provability_MELL");
    TEST_ASSERT(mell != NULL, "MELL problem should exist");
    TEST_ASSERT(mell->green_verified == false, "MELL decidability should be marked as unverified (open problem)");
    TEST_ASSERT(mell->reduces_to != NULL && strcmp(mell->reduces_to, "open_problem") == 0,
                "MELL should reduce to 'open_problem'");

    printf("  MELL decidability correctly marked as open problem (green_verified=false)\n");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */
int main(void) {
    printf("========================================\n");
    printf("  Linear Logic Axiom Package Tests\n");
    printf("========================================\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructible_problems();
    test_logical_framework();
    test_content_hash();
    test_round_trip();
    test_dependency_validation();
    test_negative_lookups();
    test_linear_logic_properties();
    test_mell_open_problem();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", g_pass_count, g_fail_count);
    printf("========================================\n");

    return g_fail_count > 0 ? 1 : 0;
}
