/**
 * @file test_axiom_group_theory.c
 * @brief Group Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the group_theory.lvz
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

#define AXIOM_PKG_PATH "module/axiom_packages/group_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/group_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 34
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Core Axioms (4) */
    {"closure", 2},
    {"associativity", 3},
    {"identity", 1},
    {"inverse", 1},
    /* Elementary Consequences (8) */
    {"identity_uniqueness", 0},
    {"inverse_uniqueness", 1},
    {"left_cancellation", 3},
    {"right_cancellation", 3},
    {"double_inverse", 1},
    {"inverse_of_product", 2},
    {"identity_is_own_inverse", 0},
    {"product_with_identity", 1},
    /* Core Constructors (4) */
    {"multiply", 2},
    {"power_positive", 2},
    {"power_negative", 2},
    {"power_zero", 1},
    /* Derived Constructors (18) */
    {"commutator", 2},
    {"conjugation", 2},
    {"element_order", 1},
    {"center", 0},
    {"centralizer", 1},
    {"subgroup_test", 2},
    {"left_coset", 2},
    {"right_coset", 2},
    {"normal_subgroup_test", 2},
    {"quotient_group", 2},
    {"direct_product", 2},
    {"free_group", 1},
    {"abelianization", 1},
    {"commutator_subgroup", 1},
    {"homomorphism", 3},
    {"kernel", 1},
    {"image", 1},
    {"first_isomorphism_theorem", 1},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"word_problem", "undecidable", 4, true},
    {"conjugacy_problem", "undecidable", 5, true},
    {"group_isomorphism_problem", "undecidable", 5, true},
    {"triviality_problem", "undecidable", 4, true},
    {"finiteness_problem", "undecidable", 5, true},
    {"simple_group_recognition", "undecidable", 5, true},
    {"torsion_freeness_problem", "undecidable", 5, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* Test 9：期望外部引用 URL 前缀 */
static const AxiomTestExtRefExpectation k_external_refs[] = {
    {"word_problem", "https://en.wikipedia.org/wiki/Word_problem_for_groups"},
    {"conjugacy_problem", "https://en.wikipedia.org/wiki/Conjugacy_problem"},
    {"group_isomorphism_problem", "https://en.wikipedia.org/wiki/Group_isomorphism_problem"},
    {"triviality_problem", "https://en.wikipedia.org/wiki/Adian"},
    {"finiteness_problem", "https://en.wikipedia.org/wiki/Adian"},
    {"simple_group_recognition", "https://en.wikipedia.org/wiki/Adian"},
    {"torsion_freeness_problem", "https://en.wikipedia.org/wiki/Adian"},
};
#define K_EXTERNAL_REFS_COUNT (int) (sizeof(k_external_refs) / sizeof(k_external_refs[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "group_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 34 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（A 形态） */
        .uc_style = AXIOM_TEST_UC_A,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 7 unconstructible problems",
        .uc_expectations = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（S 形态） */
        .lf_style = AXIOM_TEST_LF_S,
        .lf_bottom_geometry = "group_theory_abstract",
        .lf_negation_encoding = "classical_equality",
        .lf_contradiction_behavior = PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
        .lf_contradiction_name = "PROPOSITION_KIND_EXPLOSION_PRINCIPLE",

        /* Test 5: 内容哈希（单次形态） */
        .hash_style = AXIOM_TEST_HASH_SINGLE,
        .hash_free = AXIOM_TEST_FREE_LV_FREE,

        /* Test 6: 往返保存/加载（basic 形态） */
        .rt_style = AXIOM_TEST_RT_BASIC,

        /* Test 7: 依赖验证（V1 形态） */
        .dep_style = AXIOM_TEST_DEP_V1,
        .dep_fail_msg = "FAIL (acceptable)",
        .dep_suffix = " (expected: may fail for cross-reference reduces_to)",

        /* Test 8: 负向查找 */
        .neg_style = AXIOM_TEST_NEG_BASIC,

        /* Test 9: 外部引用（表驱动形态） */
        .ext_style = AXIOM_TEST_EXT_E1,
        .ext_refs = k_external_refs, .ext_refs_n = K_EXTERNAL_REFS_COUNT,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

TEST_MAIN_BEGIN("Group Theory")
    LV_REGISTER_AXIOM_CASES("GroupTheory", kCases, K_CASES_COUNT);
TEST_MAIN_END()

