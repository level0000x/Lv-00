/**
 * @file test_axiom_number_theory.c
 * @brief Number Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the number_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Number theory provides the foundational arithmetic framework for Lv-00.
 * The 38 templates cover divisibility, prime numbers, algebraic integers,
 * L-functions, elliptic curves, and transcendence theory.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/number_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/number_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 38
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Elementary Number Theory (6) */
    {"divisibility", 2},
    {"congruence", 3},
    {"euler_totient", 2},
    {"fermat_little_theorem", 2},
    {"chinese_remainder", 3},
    {"quadratic_residue", 3},
    /* Group II: Prime Numbers (7) */
    {"prime_number", 1},
    {"prime_distribution", 2},
    {"twin_primes", 2},
    {"goldbach_conjecture", 2},
    {"prime_number_theorem", 2},
    {"dirichlet_theorem", 2},
    {"algebraic_integer", 3},
    /* Group III: Algebraic Number Theory (7) */
    {"ring_of_integers", 2},
    {"ideal_theory", 2},
    {"class_number", 2},
    {"unit_group", 2},
    {"ramification_theory", 3},
    {"dedekind_domain", 1},
    {"riemann_zeta", 2},
    /* Group IV: Analytic Number Theory (6) */
    {"dirichlet_l_function", 3},
    {"modular_form", 2},
    {"l_function", 2},
    {"euler_product", 1},
    {"functional_equation", 1},
    {"diophantine_equation", 2},
    /* Group V: Diophantine Equations (5) */
    {"pell_equation", 2},
    {"elliptic_curve", 3},
    {"mordell_weil_theorem", 2},
    {"faltings_theorem", 2},
    {"p_adic_numbers", 2},
    /* Group VI: Local-Global Principles (4) */
    {"adeles_ideles", 3},
    {"local_global_principle", 3},
    {"hensel_lemma", 3},
    {"algebraic_number", 3},
    /* Group VII: Transcendence (3) */
    {"transcendental_number", 2},
    {"liouville_number", 1},
    {"catalan_constant", 0},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"riemann_hypothesis", "open_problem", 3, true},
    {"goldbach_conjecture_verification", "open_problem", 3, true},
    {"twin_prime_conjecture", "open_problem", 3, true},
    {"class_number_computation", "undecidable", 3, true},
    {"generalized_riemann_hypothesis", "open_problem", 3, true},
    {"ideal_class_group_computation", "undecidable", 3, true},
    {"transcendence_of_constants", "open_problem", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h。
 * Test 4 逻辑框架为本文件特有手写体，保留在下方）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "number_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params_min 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS_MIN,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 38 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（min_deps 形态） */
        .uc_style = AXIOM_TEST_UC_MIN_DEPS,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 7 unconstructible problems",
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
    /* bottom_geometry contains "integer_number_line" */
    TEST_ASSERT(
        strstr(pkg->bottom_geometry, "integer_number_line") != NULL ||
            strstr(pkg->bottom_geometry, "number_line") != NULL,
        "bottom_geometry should contain number theory concepts");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
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
    printf("Test 10: Key number theory templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Elementary number theory core */
    const char *elementary_core[] = {"divisibility", "congruence", "euler_totient", "chinese_remainder",
                                     "quadratic_residue"};

    for (int i = 0; i < 5; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, elementary_core[i]);
        TEST_ASSERT(tmpl != NULL, "elementary template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Algebraic number theory core */
    const char *algebraic_core[] = {"ring_of_integers", "ideal_theory", "class_number"};

    for (int i = 0; i < 3; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, algebraic_core[i]);
        TEST_ASSERT(tmpl != NULL, "algebraic number theory template should exist");
    }

    /* Elliptic curves */
    ConstraintTemplate *ec = axiom_package_get_template(pkg, "elliptic_curve");
    TEST_ASSERT(ec != NULL, "elliptic curve should exist");

    /* Riemann zeta and L-functions */
    ConstraintTemplate *zeta = axiom_package_get_template(pkg, "riemann_zeta");
    TEST_ASSERT(zeta != NULL, "Riemann zeta should exist");
    ConstraintTemplate *lf = axiom_package_get_template(pkg, "l_function");
    TEST_ASSERT(lf != NULL, "L-function should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Number Theory")
    LV_REGISTER_AXIOM_CASES("NumberTheory", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_key_templates);
TEST_MAIN_END()

