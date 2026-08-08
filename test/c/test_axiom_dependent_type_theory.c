/**
 * @file test_axiom_dependent_type_theory.c
 * @brief Dependent Type Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the dependent_type_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Dependent type theory is formalized through 33 templates covering pi/sigma
 * types, identity types, natural numbers, computation rules, metatheoretic
 * properties, inductive families, and the Curry-Howard correspondence.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/dependent_type_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/dependent_type_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 33
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 6

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Type Formers (5) */
    {"pi_type", 2},
    {"sigma_type", 2},
    {"identity_type", 3},
    {"natural_number_type", 0},
    {"universe_type", 1},
    /* Group II: Introduction Rules (5) */
    {"lambda_abstraction_dependent", 2},
    {"pair_dependent", 2},
    {"refl", 1},
    {"zero", 0},
    {"successor", 1},
    /* Group III: Elimination Rules (5) */
    {"application_dependent", 2},
    {"projection_first", 1},
    {"projection_second", 1},
    {"induction_natural", 3},
    {"path_induction", 3},
    /* Group IV: Computation Rules (4) */
    {"beta_reduction_dependent", 2},
    {"eta_expansion_dependent", 1},
    {"computation_natural", 3},
    {"computation_identity", 2},
    /* Group V: Metatheoretic Properties (5) */
    {"canonicity", 1},
    {"normalization", 1},
    {"decidable_type_checking", 3},
    {"undecidable_type_inhabitation", 1},
    {"cumulativity", 2},
    /* Group VI: Advanced Features (5) */
    {"inductive_family", 3},
    {"pattern_matching", 3},
    {"universe_polymorphism", 1},
    {"coercions", 2},
    {"type_class", 2},
    /* Group VII: Curry-Howard (4) */
    {"propositions_as_types", 1},
    {"proof_relevance", 2},
    {"curry_howard_dependent", 2},
    {"constructive_existential", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"type_inhabitation_dependent", "undecidable", 4, true},
    {"type_equality_decidability", "undecidable", 4, true},
    {"normalization_order", "undecidable", 3, true},
    {"universe_consistency", "undecidable", 3, true},
    {"parametricity_verification", "undecidable", 4, true},
    {"termination_checking_dependent", "undecidable", 4, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "dependent_type_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params_min 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS_MIN,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 33 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（min_deps 形态） */
        .uc_style = AXIOM_TEST_UC_MIN_DEPS,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 6 unconstructible problems",
        .uc_min_deps = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（presence 形态） */
        .lf_style = AXIOM_TEST_LF_P,
        .lf_contradiction_behavior = PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
        .lf_contradiction_name = "PROPOSITION_KIND_EXPLOSION_PRINCIPLE",

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

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_key_templates(void) {
    printf("Test 10: Key dependent type theory templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core type formers */
    const char *type_formers[] = {"pi_type", "sigma_type", "identity_type", "natural_number_type", "universe_type"};

    for (int i = 0; i < 5; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, type_formers[i]);
        TEST_ASSERT(tmpl != NULL, "core type former template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Introduction and elimination rules */
    const char *intro_elim[] = {"lambda_abstraction_dependent",
                                "pair_dependent",
                                "refl",
                                "zero",
                                "successor",
                                "application_dependent",
                                "projection_first",
                                "projection_second",
                                "induction_natural",
                                "path_induction"};

    for (int i = 0; i < 10; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, intro_elim[i]);
        TEST_ASSERT(tmpl != NULL, "introduction/elimination rule template should exist");
    }

    /* Computation rules */
    const char *computation[] = {"beta_reduction_dependent", "eta_expansion_dependent", "computation_natural",
                                 "computation_identity"};

    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, computation[i]);
        TEST_ASSERT(tmpl != NULL, "computation rule template should exist");
    }

    /* Curry-Howard correspondence */
    ConstraintTemplate *pat = axiom_package_get_template(pkg, "propositions_as_types");
    TEST_ASSERT(pat != NULL, "propositions_as_types template should exist");
    ConstraintTemplate *chd = axiom_package_get_template(pkg, "curry_howard_dependent");
    TEST_ASSERT(chd != NULL, "curry_howard_dependent template should exist");
    ConstraintTemplate *ce = axiom_package_get_template(pkg, "constructive_existential");
    TEST_ASSERT(ce != NULL, "constructive_existential template should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Dependent Type Theory")
    LV_REGISTER_AXIOM_CASES("DependentTypeTheory", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_key_templates);
TEST_MAIN_END()

