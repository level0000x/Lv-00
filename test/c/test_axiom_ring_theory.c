/**
 * @file test_axiom_ring_theory.c
 * @brief Ring Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the ring_theory.lvz
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

#define AXIOM_PKG_PATH "module/axiom_packages/ring_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/ring_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 54
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group A: Additive Abelian Group Axioms (5) */
    {"additive_closure", 2},
    {"additive_associativity", 3},
    {"additive_identity", 1},
    {"additive_inverse", 1},
    {"additive_commutativity", 2},
    /* Group M: Multiplicative Monoid Axioms (3) */
    {"multiplicative_closure", 2},
    {"multiplicative_associativity", 3},
    {"multiplicative_identity", 1},
    /* Group D: Distributive Laws (2) */
    {"left_distributivity", 3},
    {"right_distributivity", 3},
    /* Elementary Consequences (12) */
    {"additive_identity_uniqueness", 0},
    {"additive_inverse_uniqueness", 1},
    {"multiplicative_identity_uniqueness", 0},
    {"zero_multiplication", 1},
    {"negative_multiplication", 2},
    {"negative_negative_product", 2},
    {"zero_ring_condition", 0},
    {"additive_cancellation", 3},
    {"double_additive_inverse", 1},
    {"negative_of_sum", 2},
    {"zero_is_own_add_inverse", 0},
    {"negative_one_times", 1},
    /* Core Constructors (6) */
    {"add", 2},
    {"multiply", 2},
    {"negate", 1},
    {"zero", 0},
    {"one", 0},
    {"subtract", 2},
    /* Derived Constructors (26) */
    {"characteristic", 1},
    {"power_positive", 2},
    {"scalar_multiple", 2},
    {"binomial_theorem", 3},
    {"unit", 1},
    {"multiplicative_inverse", 1},
    {"zero_divisor", 2},
    {"nilpotent", 1},
    {"idempotent", 1},
    {"subring_test", 2},
    {"left_ideal", 2},
    {"right_ideal", 2},
    {"two_sided_ideal", 2},
    {"principal_ideal", 1},
    {"quotient_ring", 2},
    {"homomorphism", 3},
    {"kernel", 1},
    {"image", 1},
    {"first_isomorphism_theorem", 1},
    {"direct_product", 2},
    {"polynomial_ring", 1},
    {"matrix_ring", 2},
    {"commutator", 2},
    {"center", 0},
    {"unit_group", 0},
    {"jacobson_radical", 1},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"hilberts_tenth_problem", "undecidable", 5, true},
    {"word_problem_for_rings", "undecidable", 9, true},
    {"ring_isomorphism_problem", "undecidable", 7, true},
    {"triviality_problem_rings", "undecidable", 5, true},
    {"zero_divisor_recognition", "undecidable", 5, true},
    {"nilpotent_element_recognition", "undecidable", 4, true},
    {"commutativity_recognition", "undecidable", 4, true},
    {"ideal_membership_unrestricted", "undecidable", 7, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* Test 9：期望外部引用 URL 前缀 */
static const AxiomTestExtRefExpectation k_external_refs[] = {
    {"hilberts_tenth_problem", "https://en.wikipedia.org/wiki/Hilbert%27s_tenth_problem"},
    {"word_problem_for_rings", "https://en.wikipedia.org/wiki/Word_problem_for_groups"},
    {"ring_isomorphism_problem", "https://en.wikipedia.org/wiki/Ring_isomorphism"},
    {"triviality_problem_rings", "https://en.wikipedia.org/wiki/Word_problem_for_groups"},
    {"zero_divisor_recognition", "https://en.wikipedia.org/wiki/Zero_divisor"},
    {"nilpotent_element_recognition", "https://en.wikipedia.org/wiki/Nilpotent"},
    {"commutativity_recognition", "https://en.wikipedia.org/wiki/Commutative_ring"},
    {"ideal_membership_unrestricted", "https://en.wikipedia.org/wiki/Ideal_(ring_theory)"},
};
#define K_EXTERNAL_REFS_COUNT (int) (sizeof(k_external_refs) / sizeof(k_external_refs[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "ring_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 54 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（A 形态） */
        .uc_style = AXIOM_TEST_UC_A,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 8 unconstructible problems",
        .uc_expectations = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（S 形态） */
        .lf_style = AXIOM_TEST_LF_S,
        .lf_bottom_geometry = "ring_theory_abstract",
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

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_ring_axiom_coherence(void) {
    printf("Test 10: Verify ring axiom coherence...\n");

    AxiomPackage *pkg = axiom_package_create("ring_theory", "1.0.0");
    TEST_ASSERT(pkg != NULL, "create package");
    AxiomLoadStatus s = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(s == AXIOM_LOAD_OK, "load ring_theory.lvz");

    /* 验证核心环公理存在：加法交换律、结合律、零元、负元、乘法结合律、单位元、分配律 */
    const char *core_axioms[] = {"additive_closure",
                                 "multiplicative_closure",
                                 "additive_associativity",
                                 "multiplicative_associativity",
                                 "additive_identity",
                                 "multiplicative_identity",
                                 "additive_inverse",
                                 "additive_commutativity",
                                 "left_distributivity",
                                 "right_distributivity",
                                 NULL};
    for (int i = 0; core_axioms[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_axioms[i]);
        TEST_ASSERT(tmpl != NULL, core_axioms[i]);
    }

    printf("Test 10 passed: all core ring axioms verified.\n");
    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Ring Theory")
    LV_REGISTER_AXIOM_CASES("RingTheory", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_ring_axiom_coherence);
TEST_MAIN_END()

