/**
 * @file test_axiom_simple_type_theory.c
 * @brief Simple Type Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the simple_type_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Simple type theory (Church-style simply typed lambda calculus) provides
 * a natural bridge between logic and computation via the Curry-Howard
 * correspondence. The 39 templates cover type formation, term construction,
 * typing rules, the propositions-as-types paradigm, metatheoretic properties,
 * and extended constructs.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/simple_type_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/simple_type_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 39
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Type Formation (5) */
    {"base_type", 1},
    {"function_type", 2},
    {"product_type", 2},
    {"sum_type", 2},
    {"unit_type", 0},
    /* Group II: Term Construction (6) */
    {"variable", 1},
    {"lambda_abstraction", 2},
    {"application", 2},
    {"beta_reduction", 2},
    {"eta_expansion", 1},
    {"alpha_conversion", 2},
    /* Group III: Typing Rules (8) */
    {"var_rule", 2},
    {"abs_rule", 3},
    {"app_rule", 3},
    {"conv_rule", 3},
    {"prod_formation", 3},
    {"prod_intro", 3},
    {"prod_elim", 3},
    {"prod_beta", 3},
    /* Group IV: Propositions-as-Types (6) */
    {"proposition_as_type", 1},
    {"proof_as_term", 2},
    {"implication_as_function_type", 2},
    {"conjunction_as_product", 2},
    {"disjunction_as_sum", 2},
    {"negation_as_function_to_false", 1},
    /* Group V: Metatheoretic Properties (5) */
    {"type_safety_progress", 1},
    {"type_safety_preservation", 2},
    {"strong_normalization", 1},
    {"decidability_of_typing", 3},
    {"principal_type_property", 2},
    /* Group VI: Extended Constructs (5) */
    {"let_binding", 2},
    {"fixpoint_for_product", 2},
    {"pair_constructor", 2},
    {"pair_elimination", 2},
    {"inductive_type_sketch", 2},
    /* Group VII: Type Equivalence (4) */
    {"type_equivalence", 2},
    {"definitional_equality", 2},
    {"beta_eta_equivalence", 2},
    {"subtype_relation", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"type_inhabitation_general", "undecidable", 3, true},
    {"beta_normalization_order", "undecidable", 3, true},
    {"type_equivalence_decidability", "undecidable", 4, true},
    {"polymorphic_type_inhabitation", "undecidable", 4, true},
    {"termination_checking", "undecidable", 3, true},
    {"proof_irrelevance", "undecidable", 4, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h。
 * Test 4 逻辑框架为本文件特有手写体，保留在下方）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "simple_type_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params_min 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS_MIN,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 39 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（min_deps 形态） */
        .uc_style = AXIOM_TEST_UC_MIN_DEPS,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 6 unconstructible problems",
        .uc_min_deps = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（文件特有手写，见下方 test_logical_framework） */

        /* Test 5: 内容哈希（确定性形态） */
        .hash_style = AXIOM_TEST_HASH_DETERMINISTIC,
        .hash_free = AXIOM_TEST_FREE_LV_FREE,

        /* Test 6: 往返保存/加载（save_load 形态） */
        .rt_style = AXIOM_TEST_RT_SAVE_LOAD,

        /* Test 7: 依赖验证（note 形态） */
        .dep_style = AXIOM_TEST_DEP_V2,
        .dep_extra = "(identifier references to external concepts are expected)",

        /* Test 8: 负向查找 */
        .neg_style = AXIOM_TEST_NEG_XYZ,

        /* Test 9: 外部引用（遍历全部形态） */
        .ext_style = AXIOM_TEST_EXT_ALL,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

/* Test 4：逻辑框架（文件特有：negation_encoding 用 strstr 概念检查，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    TEST_ASSERT(strcmp(pkg->bottom_geometry, "simply_typed_lambda_calculus_terms") == 0,
                "bottom_geometry should be 'simply_typed_lambda_calculus_terms'");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    /* negation_encoding contains "empty_type" or "false" */
    TEST_ASSERT(strstr(pkg->negation_encoding, "empty_type") != NULL || strstr(pkg->negation_encoding, "false") != NULL,
                "negation_encoding should contain 'empty_type' or 'false'");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_key_templates(void) {
    printf("Test 10: Key simple type theory templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core type formation: base_type, function_type, product_type, sum_type, unit_type */
    const char *core_types[] = {"base_type", "function_type", "product_type", "sum_type", "unit_type"};

    for (int i = 0; i < 5; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_types[i]);
        TEST_ASSERT(tmpl != NULL, "core type template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Core term construction: variable, lambda_abstraction, application */
    const char *core_terms[] = {"variable", "lambda_abstraction", "application"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_terms[i]);
        TEST_ASSERT(tmpl != NULL, "core term template should exist");
    }

    /* Typing rules: var_rule, abs_rule, app_rule */
    const char *typing_rules[] = {"var_rule", "abs_rule", "app_rule"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, typing_rules[i]);
        TEST_ASSERT(tmpl != NULL, "typing rule template should exist");
    }

    /* Curry-Howard bridge */
    ConstraintTemplate *pat = axiom_package_get_template(pkg, "proposition_as_type");
    TEST_ASSERT(pat != NULL, "proposition_as_type template should exist");
    ConstraintTemplate *imp = axiom_package_get_template(pkg, "implication_as_function_type");
    TEST_ASSERT(imp != NULL, "implication_as_function_type template should exist");

    /* Metatheoretic properties */
    ConstraintTemplate *sn = axiom_package_get_template(pkg, "strong_normalization");
    TEST_ASSERT(sn != NULL, "strong_normalization template should exist");
    ConstraintTemplate *dt = axiom_package_get_template(pkg, "decidability_of_typing");
    TEST_ASSERT(dt != NULL, "decidability_of_typing template should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Simple Type Theory")
    LV_REGISTER_AXIOM_CASES("SimpleTypeTheory", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_key_templates);
TEST_MAIN_END()

