/**
 * @file test_axiom_proof_theory.c
 * @brief Proof Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the proof_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/proof_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/proof_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 36
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Sequent Calculus (6) */
    {"sequent", 2},
    {"antecedent_succedent", 2},
    {"initial_sequent", 1},
    {"left_rule", 2},
    {"right_rule", 2},
    {"structural_rule", 2},
    /* Group II: Logical Rules (6) */
    {"negation_left", 2},
    {"negation_right", 2},
    {"conjunction_left", 2},
    {"conjunction_right", 2},
    {"disjunction_left", 2},
    {"disjunction_right", 2},
    /* Group III: Quantifier Rules (5) */
    {"universal_left", 2},
    {"universal_right", 2},
    {"existential_left", 2},
    {"existential_right", 2},
    {"equality_rule", 1},
    /* Group IV: Normalization (5) */
    {"cut_elimination", 1},
    {"proof_normalization", 1},
    {"hauptsatz", 1},
    {"reducibility_candidate", 1},
    {"proof_complexity", 1},
    /* Group V: Ordinal Analysis (5) */
    {"ordinal_notation", 1},
    {"recursive_ordinal", 1},
    {"proof_ordinal_analysis", 2},
    {"buchi_landau_theorem", 0},
    {"first_inaccessible", 0},
    /* Group VI: Provability Logic (4) */
    {"reflection_principle", 1},
    {"provability_logic", 1},
    {"solovay_theorem", 0},
    {"arithmetical_hierarchy", 1},
    /* Group VII: Extended Logics (5) */
    {"arithmetic_proof", 1},
    {"second_order_logic_proof", 1},
    {"intuitionistic_proof", 1},
    {"linear_logic_proof", 1},
    {"modal_proof_logic", 1},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"cut_elimination_complexity", "non_elementary", 3, true},
    {"proof_equality_problem", "undecidable", 3, true},
    {"first_order_validity_proof", "undecidable", 2, true},
    {"ordinal_computation", "undecidable", 3, true},
    {"proof_length_optimal", "open_problem", 3, true},
    {"subsystem_analysis", "undecidable", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h。
 * Test 4 逻辑框架为本文件特有手写体，保留在下方）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "proof_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params_min 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS_MIN,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 36 constraint templates",
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
        .dep_extra = NULL,

        /* Test 8: 负向查找 */
        .neg_style = AXIOM_TEST_NEG_XYZ,

        /* Test 9: 外部引用（遍历全部形态） */
        .ext_style = AXIOM_TEST_EXT_ALL,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

/* Test 4：逻辑框架（文件特有：strstr 概念检查，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    TEST_ASSERT(strstr(pkg->bottom_geometry, "sequent") != NULL || strstr(pkg->bottom_geometry, "proof") != NULL,
                "bottom_geometry should contain proof theory concepts");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

/* Test 5/6/7/8/9 已收敛至 kCases 数据驱动用例（见上） */

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_key_templates(void) {
    printf("Test 10: Key proof theory templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Sequent calculus core */
    const char *sequent_core[] = {"sequent", "left_rule", "right_rule", "structural_rule"};

    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, sequent_core[i]);
        TEST_ASSERT(tmpl != NULL, "sequent calculus template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Logical rules core */
    const char *logic_core[] = {"negation_left", "conjunction_left", "disjunction_left"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, logic_core[i]);
        TEST_ASSERT(tmpl != NULL, "logical rule template should exist");
    }

    /* Cut elimination */
    ConstraintTemplate *ce = axiom_package_get_template(pkg, "cut_elimination");
    TEST_ASSERT(ce != NULL, "cut elimination should exist");
    ConstraintTemplate *pn = axiom_package_get_template(pkg, "proof_normalization");
    TEST_ASSERT(pn != NULL, "proof normalization should exist");

    /* Ordinal analysis */
    ConstraintTemplate *oa = axiom_package_get_template(pkg, "ordinal_notation");
    TEST_ASSERT(oa != NULL, "ordinal notation should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Proof Theory")
    LV_REGISTER_AXIOM_CASES("ProofTheory", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_key_templates);
TEST_MAIN_END()

