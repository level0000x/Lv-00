/**
 * @file test_axiom_zfc_set_theory.c
 * @brief ZFC Set Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the zfc_set_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, and negative lookups.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/zfc_set_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/zfc_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 29
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 10

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Existence & Construction Axioms (5) */
    {"extensionality", 2},
    {"pairing", 2},
    {"union", 1},
    {"power_set", 1},
    {"infinity", 0},
    /* Group II: Structural & Regularity (1) */
    {"regularity", 1},
    /* Group III: Axiom Schemas (2) */
    {"specification", 3},
    {"replacement", 3},
    /* Group IV: Axiom of Choice (1) */
    {"choice", 1},
    /* Group V: Core Constructors (9) */
    {"empty_set", 0},
    {"singleton", 1},
    {"ordered_pair", 2},
    {"cartesian_product", 2},
    {"binary_union", 2},
    {"binary_intersection", 2},
    {"set_difference", 2},
    {"big_intersection", 1},
    {"subset_relation", 2},
    /* Group VI: Well-Foundedness & Induction (3) */
    {"epsilon_induction", 2},
    {"transitive_closure", 1},
    {"ordinal_successor", 1},
    /* Group VII: Relation & Function Constructors (6) */
    {"relation", 3},
    {"function", 3},
    {"function_application", 2},
    {"image", 2},
    {"inverse_image", 2},
    /* Group VIII: Cardinal & Ordinal Constructors (2) */
    {"equinumerous", 2},
    {"cardinality", 1},
    {"well_order", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"continuum_hypothesis", "independent_of_ZFC", 4, true},
    {"generalized_continuum_hypothesis", "independent_of_ZFC", 3, true},
    {"axiom_of_choice_independence", "independent_of_ZF", 2, true},
    {"inaccessible_cardinal_existence", "equiconsistent_with_ZFC", 3, true},
    {"suslin_hypothesis", "independent_of_ZFC", 3, true},
    {"whitehead_problem", "independent_of_ZFC", 3, true},
    {"zfc_consistency", "unprovable_in_ZFC", 3, true},
    {"measurable_cardinal_existence", "transcends_ZFC", 3, true},
    {"axiom_of_constructibility", "independent_of_ZFC", 3, true},
    {"martins_axiom", "independent_of_ZFC", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* Test 9：期望外部引用 URL 前缀 */
static const AxiomTestExtRefExpectation k_external_refs[] = {
    {"continuum_hypothesis", "https://en.wikipedia.org/wiki/Continuum_hypothesis"},
    {"generalized_continuum_hypothesis", "https://en.wikipedia.org/wiki/Continuum_hypothesis"},
    {"axiom_of_choice_independence", "https://en.wikipedia.org/wiki/Axiom_of_choice"},
    {"inaccessible_cardinal_existence", "https://en.wikipedia.org/wiki/Inaccessible_cardinal"},
    {"suslin_hypothesis", "https://en.wikipedia.org/wiki/Suslin%27s_problem"},
    {"whitehead_problem", "https://en.wikipedia.org/wiki/Whitehead_problem"},
    {"zfc_consistency", "https://en.wikipedia.org/wiki/G%C3%B6del%27s_incompleteness_theorems"},
    {"measurable_cardinal_existence", "https://en.wikipedia.org/wiki/Measurable_cardinal"},
    {"axiom_of_constructibility", "https://en.wikipedia.org/wiki/Axiom_of_constructibility"},
    {"martins_axiom", "https://en.wikipedia.org/wiki/Martin%27s_axiom"},
};
#define K_EXTERNAL_REFS_COUNT (int) (sizeof(k_external_refs) / sizeof(k_external_refs[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "zfc_set_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 29 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（B 形态：HTTPS 检查） */
        .uc_style = AXIOM_TEST_UC_B,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 10 unconstructible problems",
        .uc_expectations = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（S 形态） */
        .lf_style = AXIOM_TEST_LF_S,
        .lf_bottom_geometry = "zfc_cumulative_hierarchy",
        .lf_negation_encoding = "classical_complement_in_set_universe",
        .lf_contradiction_behavior = PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
        .lf_contradiction_name = "PROPOSITION_KIND_EXPLOSION_PRINCIPLE",

        /* Test 5: 内容哈希（单次形态） */
        .hash_style = AXIOM_TEST_HASH_SINGLE,
        .hash_free = AXIOM_TEST_FREE_LV_FREE,

        /* Test 6: 往返保存/加载（basic 形态） */
        .rt_style = AXIOM_TEST_RT_BASIC,

        /* Test 7: 依赖验证（V1 形态） */
        .dep_style = AXIOM_TEST_DEP_V1,
        .dep_fail_msg = "FAIL (cross-references internal)",
        .dep_suffix = "",

        /* Test 8: 负向查找 */
        .neg_style = AXIOM_TEST_NEG_BASIC,

        /* Test 9: 外部引用（表驱动形态） */
        .ext_style = AXIOM_TEST_EXT_E1,
        .ext_refs = k_external_refs, .ext_refs_n = K_EXTERNAL_REFS_COUNT,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_key_axioms_present(void) {
    printf("Test 10: Verify all 9 ZFC axioms are represented...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core ZF axioms that must be present (9) */
    const char *required[] = {
        "extensionality", /* ZF1 */
        "regularity",     /* ZF2 */
        "specification",  /* ZF3 */
        "pairing",        /* ZF4 */
        "union",          /* ZF5 */
        "replacement",    /* ZF6 */
        "infinity",       /* ZF7 */
        "power_set",      /* ZF8 */
        "choice",         /* ZFC9 */
    };

    for (int i = 0; i < 9; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, required[i]);
        TEST_ASSERT(tmpl != NULL, required[i]);
        if (tmpl) {
            printf("  [%d] %s (params=%d) - PRESENT\n", i, required[i], tmpl->param_count);
        }
    }

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("ZFC Set Theory")
    LV_REGISTER_AXIOM_CASES("ZFCSetTheory", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_key_axioms_present);
TEST_MAIN_END()

