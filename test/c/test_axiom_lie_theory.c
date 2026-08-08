/**
 * @file test_axiom_lie_theory.c
 * @brief Lie Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the lie_theory.lvz
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

#define AXIOM_PKG_PATH "module/axiom_packages/lie_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/lie_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 70
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Lie Algebra Core Axioms (5) */
    {"lie_algebra_bilinearity_left", 3},
    {"lie_algebra_bilinearity_right", 3},
    {"lie_algebra_alternating", 1},
    {"lie_algebra_anticommutativity", 2},
    {"jacobi_identity", 3},
    /* Group II: Lie Group Core Axioms (4) */
    {"lie_group_structure", 2},
    {"lie_group_smooth_manifold", 1},
    {"lie_group_smooth_multiplication", 2},
    {"lie_group_smooth_inversion", 1},
    /* Group III: Lie Group-Lie Algebra Correspondence (4) */
    {"tangent_space_at_identity", 1},
    {"exponential_map", 1},
    {"exponential_local_diffeomorphism", 1},
    {"lie_bracket_from_commutator", 2},
    {"homomorphism_induces_algebra_hom", 2},
    /* Group IV: Elementary Consequences (8) */
    {"skew_symmetry_derived", 2},
    {"jacobi_as_derivation", 3},
    {"baker_campbell_hausdorff", 2},
    {"exp_of_zero_is_identity", 0},
    {"exp_of_negative_is_inverse", 1},
    {"exponential_differential_equation", 1},
    {"adjoint_representation", 2},
    {"adjoint_is_homomorphism", 1},
    /* Group V: Lie Algebra Structure Theory (8) */
    {"lie_subalgebra", 1},
    {"lie_ideal", 1},
    {"quotient_lie_algebra", 1},
    {"solvable_derived_series", 1},
    {"nilpotent_lower_central_series", 1},
    {"simple_lie_algebra", 1},
    {"semisimple_lie_algebra", 1},
    {"abelian_lie_algebra", 0},
    /* Group VI: Classical Lie Algebras (8) */
    {"general_linear_algebra_gln", 1},
    {"special_linear_algebra_sln", 1},
    {"orthogonal_algebra_son", 1},
    {"special_orthogonal_algebra", 1},
    {"symplectic_algebra_sp2n", 1},
    {"unitary_algebra_un", 1},
    {"special_unitary_algebra_sun", 1},
    {"euclidean_algebra_se3", 1},
    /* Group VII: Representation Theory (6) */
    {"lie_algebra_representation", 2},
    {"adjoint_representation_algebra", 1},
    {"irreducible_representation", 1},
    {"universal_enveloping_algebra", 1},
    {"poincare_birkhoff_witt", 1},
    {"casimir_element", 1},
    /* Group VIII: Root Systems & Classification (6) */
    {"cartan_subalgebra", 1},
    {"root_of_lie_algebra", 1},
    {"root_system", 1},
    {"killing_form", 2},
    {"cartan_criterion_semisimple", 1},
    {"weyl_group", 1},
    /* Group IX: Core Constructors (6) */
    {"lie_bracket", 2},
    {"exponential", 1},
    {"adjoint_action", 2},
    {"direct_sum_lie_algebra", 2},
    {"derived_algebra", 1},
    {"center_lie_algebra", 1},
    /* Group X: Derived Constructors (8) */
    {"matrix_commutator", 2},
    {"cross_product_bracket", 2},
    {"lie_algebra_homomorphism", 2},
    {"lie_algebra_isomorphism", 2},
    {"semidirect_product_lie_algebra", 2},
    {"universal_covering_group", 1},
    {"lie_subgroup", 1},
    {"identity_component", 1},
    /* Group XI: Advanced Topics (6) */
    {"levi_decomposition", 1},
    {"ados_theorem", 1},
    {"lies_third_theorem", 1},
    {"hilberts_fifth_problem", 0},
    {"highest_weight_theory", 1},
    {"dynkin_diagram_classification", 1},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"lie_algebra_isomorphism_problem", "group_isomorphism_problem", 2, false},
    {"lie_group_isomorphism_problem", "lie_algebra_isomorphism_problem", 2, false},
    {"nilpotency_testing_infinite_dimensional", "word_problem_for_groups", 2, false},
    {"solvable_quotient_computation", "group_isomorphism_problem", 2, false},
    {"lie_algebra_word_problem", "word_problem_for_groups", 2, false},
    {"representation_equivalence_problem", "lie_algebra_isomorphism_problem", 2, false},
    {"invariant_subspace_lattice", "lie_algebra_isomorphism_problem", 2, false},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* Test 9：期望外部引用 URL 前缀 */
static const AxiomTestExtRefExpectation k_external_refs[] = {
    {"lie_algebra_isomorphism_problem", "https://en.wikipedia.org/wiki/Group_isomorphism_problem"},
    {"lie_group_isomorphism_problem", "https://en.wikipedia.org/wiki/Lie_group"},
    {"nilpotency_testing_infinite_dimensional", "https://en.wikipedia.org/wiki/Word_problem_for_groups"},
    {"solvable_quotient_computation", "https://en.wikipedia.org/wiki/Solvable_group"},
    {"lie_algebra_word_problem", "https://en.wikipedia.org/wiki/Word_problem_for_groups"},
    {"representation_equivalence_problem", "https://en.wikipedia.org/wiki/Representation_theory"},
    {"invariant_subspace_lattice", "https://en.wikipedia.org/wiki/Invariant_subspace_problem"},
};
#define K_EXTERNAL_REFS_COUNT (int) (sizeof(k_external_refs) / sizeof(k_external_refs[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "lie_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 70 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（A 形态） */
        .uc_style = AXIOM_TEST_UC_A,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 7 unconstructible problems",
        .uc_expectations = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（S 形态） */
        .lf_style = AXIOM_TEST_LF_S,
        .lf_bottom_geometry = "lie_group_smooth_manifold_with_lie_algebra_tangent_space",
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

static void test_key_axioms_present(void) {
    printf("Test 10: Verify key Lie theory axioms are present...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core Lie algebra axioms */
    TEST_ASSERT(axiom_package_get_template(pkg, "lie_algebra_bilinearity_left") != NULL,
                "bilinearity left should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "lie_algebra_bilinearity_right") != NULL,
                "bilinearity right should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "lie_algebra_alternating") != NULL,
                "alternating property should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "jacobi_identity") != NULL, "Jacobi identity should exist");

    /* Core Lie group axioms */
    TEST_ASSERT(axiom_package_get_template(pkg, "lie_group_structure") != NULL, "Lie group structure should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "lie_group_smooth_manifold") != NULL, "smooth manifold should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "exponential_map") != NULL, "exponential map should exist");

    /* Correspondence */
    TEST_ASSERT(axiom_package_get_template(pkg, "tangent_space_at_identity") != NULL,
                "tangent space at identity should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "lie_bracket_from_commutator") != NULL,
                "Lie bracket from commutator should exist");

    /* Structure theory */
    TEST_ASSERT(axiom_package_get_template(pkg, "killing_form") != NULL, "Killing form should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "root_system") != NULL, "root system should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "levi_decomposition") != NULL, "Levi decomposition should exist");

    printf("  Key axioms: all present\n");

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Lie Theory")
    LV_REGISTER_AXIOM_CASES("LieTheory", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_key_axioms_present);
TEST_MAIN_END()

