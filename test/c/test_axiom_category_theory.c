/**
 * @file test_axiom_category_theory.c
 * @brief Category Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the category_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Category theory is naturally suited for Lv-00's geometric constraint
 * graph representation: a category IS a directed multigraph with
 * composition. The 60 templates cover the full breadth from basic
 * category axioms through functors, natural transformations, adjunctions,
 * limits, and the Yoneda lemma.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/category_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/category_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 60
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Core Primitives (5) */
    {"object", 0},
    {"morphism", 2},
    {"composability", 2},
    {"composition", 3},
    {"identity_morphism", 1},
    /* Group II: Axiom Constraints (3) */
    {"associativity", 3},
    {"left_identity", 2},
    {"right_identity", 2},
    /* Group III: Morphism Classifications (7) */
    {"monomorphism", 3},
    {"epimorphism", 3},
    {"isomorphism", 2},
    {"endomorphism", 1},
    {"automorphism", 2},
    {"section", 2},
    {"retraction", 2},
    /* Group IV: Universal Objects (3) */
    {"initial_object", 0},
    {"terminal_object", 0},
    {"zero_object", 0},
    /* Group V: Limits (11) */
    {"binary_product", 2},
    {"product_projection_left", 2},
    {"product_projection_right", 2},
    {"binary_coproduct", 2},
    {"coproduct_injection_left", 2},
    {"coproduct_injection_right", 2},
    {"equalizer", 2},
    {"coequalizer", 2},
    {"pullback", 2},
    {"pushout", 2},
    {"exponential_object", 2},
    /* Group VI: Functors (8) */
    {"functor_object_map", 2},
    {"functor_morphism_map", 2},
    {"functor_preserves_composition", 3},
    {"functor_preserves_identity", 1},
    {"identity_functor", 1},
    {"functor_composition", 3},
    {"contravariant_functor", 2},
    {"forgetful_functor", 1},
    /* Group VII: Natural Transformations (6) */
    {"natural_transformation", 2},
    {"natural_transformation_component", 3},
    {"naturality_square", 4},
    {"vertical_composition", 2},
    {"horizontal_composition", 2},
    {"natural_isomorphism", 2},
    /* Group VIII: Adjunctions (4) */
    {"adjunction", 2},
    {"unit_of_adjunction", 2},
    {"counit_of_adjunction", 2},
    {"triangle_identities", 4},
    /* Group IX: Special Categories (6) */
    {"opposite_category", 1},
    {"product_category", 2},
    {"slice_category", 2},
    {"coslice_category", 2},
    {"arrow_category", 1},
    {"monoidal_category", 1},
    /* Group X: Equivalence & Properties (4) */
    {"equivalence_of_categories", 2},
    {"skeleton", 1},
    {"full_subcategory", 2},
    {"commutative_diagram", 2},
    /* Group XI: Yoneda Lemma (3) */
    {"hom_functor", 1},
    {"representable_functor", 2},
    {"yoneda_embedding", 1},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcMinDepsExpectation k_unconstructibles[] = {
    {"word_problem_for_fp_categories", "undecidable", 4, true},
    {"equality_of_morphisms_fpc", "word_problem_for_fp_categories", 4, true},
    {"isomorphism_of_fp_categories", "undecidable", 5, true},
    {"existence_of_limit_in_fpc", "undecidable", 5, true},
    {"is_category_equivalent_to_poset", "undecidable", 4, true},
    {"finite_model_property_for_fpc", "undecidable", 4, true},
    {"functor_equivalence_in_fpc", "undecidable", 5, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "category_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params_min 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS_MIN,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 60 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（min_deps 形态） */
        .uc_style = AXIOM_TEST_UC_MIN_DEPS,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 7 unconstructible problems",
        .uc_min_deps = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（checked 形态） */
        .lf_style = AXIOM_TEST_LF_M,
        .lf_header = "Test 4: Verify logical framework settings...",
        .lf_bottom_geometry = "directed_multigraph_with_composition",
        .lf_negation_encoding = "categorical_subobject_complement",
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
    printf("Test 10: Key category theory templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core category definition: these 8 templates form the minimal category */
    const char *minimal_category[] = {"object",        "morphism",          "composability",
                                      "composition",   "identity_morphism", "associativity",
                                      "left_identity", "right_identity"};

    for (int i = 0; i < 8; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, minimal_category[i]);
        TEST_ASSERT(tmpl != NULL, "core category template should exist");
        /* Verify basic parameter count sanity */
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4, "parameter count should be reasonable");
    }

    /* Functor minimal definition: 4 templates */
    const char *minimal_functor[] = {"functor_object_map", "functor_morphism_map", "functor_preserves_composition",
                                     "functor_preserves_identity"};

    for (int i = 0; i < 4; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, minimal_functor[i]);
        TEST_ASSERT(tmpl != NULL, "minimal functor template should exist");
    }

    /* Natural transformation */
    ConstraintTemplate *nt = axiom_package_get_template(pkg, "natural_transformation");
    TEST_ASSERT(nt != NULL, "natural_transformation template should exist");
    ConstraintTemplate *ns = axiom_package_get_template(pkg, "naturality_square");
    TEST_ASSERT(ns != NULL, "naturality_square template should exist");

    /* Adjunction */
    ConstraintTemplate *adj = axiom_package_get_template(pkg, "adjunction");
    TEST_ASSERT(adj != NULL, "adjunction template should exist");
    ConstraintTemplate *tri = axiom_package_get_template(pkg, "triangle_identities");
    TEST_ASSERT(tri != NULL, "triangle_identities template should exist");

    /* Yoneda */
    ConstraintTemplate *yoneda = axiom_package_get_template(pkg, "yoneda_embedding");
    TEST_ASSERT(yoneda != NULL, "yoneda_embedding template should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Category Theory")
    LV_REGISTER_AXIOM_CASES("CategoryTheory", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_key_templates);
TEST_MAIN_END()

