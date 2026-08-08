/**
 * @file test_axiom_computability_theory.c
 * @brief Computability Theory (Recursion Theory) Axiom Package Test
 *
 * Tests the loading, template verification, unconstructible problem
 * lookup, logical framework, content hashing, round-trip save/load,
 * dependency validation, and negative lookups for the computability
 * theory axiom package.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_fail_count = 0;
int g_pass_count = 0;

/* 历史私有 TEST_ASSERT 为非返回式语义（失败仅计数、继续执行），
 * 通过 AXIOM_TEST_NON_RETURNING 让骨架头提供兼容变体，保持行为不变 */
#include "test_helpers.h"

#include "axiom_test_common.h"

#define AXIOM_PKG_PATH "module/axiom_packages/computability_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/computability_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 55
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 14

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板名 */
static const char *const k_template_names[] = {
    /* Group I: Initial Functions */
    "zero_function", "successor_function", "projection_function",
    /* Group II: Operators */
    "composition", "primitive_recursion", "minimization_operator",
    /* Group III: Fundamental Constructions */
    "universal_turing_machine", "kleene_T_predicate", "result_extraction",
    /* Group IV: Fundamental Theorems */
    "kleene_normal_form", "smn_theorem", "kleene_recursion_theorem", "rice_theorem",
    /* Group V: Computable and c.e. Sets */
    "computable_set", "computably_enumerable_set", "halting_set_K", "complement_halting_set",
    /* Group VI: Reducibilities and Degrees */
    "many_one_reducibility", "turing_reducibility", "turing_equivalence", "turing_degree", "turing_jump",
    /* Group VII: Arithmetical Hierarchy */
    "sigma_1_set", "pi_1_set", "sigma_n_set", "pi_n_set", "delta_n_set", "post_theorem",
    /* Group VIII: Core Constructors */
    "cantor_pairing", "cantor_unpairing", "godel_numbering", "program_enumeration", "diagonalization",
    "oracle_turing_machine", "relative_computability", "finite_injury_priority", "infinite_injury_priority",
    /* Group IX: Primitive Recursive Functions */
    "primitive_addition", "primitive_multiplication", "primitive_exponentiation", "primitive_factorial",
    "primitive_predecessor", "primitive_subtraction", "primitive_sign", "primitive_absolute_difference",
    "bounded_minimization", "bounded_existential", "bounded_universal",
    /* Group X: Advanced Constructions */
    "ackermann_function", "busy_beaver_function", "kolmogorov_complexity", "martin_lof_randomness_test",
    "friedberg_muchnik_theorem",
    /* Group XI: Computable Reals */
    "computable_real_number", "computable_function_on_reals",
};
#define K_TEMPLATE_NAMES_COUNT (int) (sizeof(k_template_names) / sizeof(k_template_names[0]))

/* ============================================================
 * 统一数据驱动用例表（纯共享 wrapper 收敛至此；test_templates /
 * test_unconstructible_problems / test_external_references 含文件特有
 * 手写逻辑，保留在下方原样运行）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "computability_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（test_templates 为混合 wrapper，下方保留） */
        .tmpl_style = AXIOM_TEST_TMPL_NONE,

        /* Test 3: 不可构造项（文件特有手写，下方保留） */
        .uc_style = AXIOM_TEST_UC_NONE,

        /* Test 4: 逻辑框架（S 形态） */
        .lf_style = AXIOM_TEST_LF_S,
        .lf_bottom_geometry = "turing_machine_configuration_space",
        .lf_negation_encoding = "complement_in_natural_numbers",
        .lf_contradiction_behavior = PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
        .lf_contradiction_name = "PROPOSITION_KIND_EXPLOSION_PRINCIPLE",

        /* Test 5: 内容哈希（单次形态） */
        .hash_style = AXIOM_TEST_HASH_SINGLE,
        .hash_free = AXIOM_TEST_FREE_LV_FREE_PTR,

        /* Test 6: 往返保存/加载（basic 形态） */
        .rt_style = AXIOM_TEST_RT_BASIC,

        /* Test 7: 依赖验证（V1 形态） */
        .dep_style = AXIOM_TEST_DEP_V1,
        .dep_fail_msg = "FAIL (acceptable)",
        .dep_suffix = " (expected: may fail for cross-reference reduces_to)",

        /* Test 8: 负向查找 */
        .neg_style = AXIOM_TEST_NEG_BASIC,

        /* Test 9: 外部引用（文件特有手写，下方保留） */
        .ext_style = AXIOM_TEST_EXT_NONE,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

static void test_templates(void) {
    axiom_test_templates_names_only(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 55 constraint templates",
                                    k_template_names, K_TEMPLATE_NAMES_COUNT);

    /* 文件特有：具体参数个数校验（差异部分，原样保留） */
    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *t;

    t = axiom_package_get_template(pkg, "zero_function");
    TEST_ASSERT(t && t->param_count == 1, "zero_function should have 1 param");

    t = axiom_package_get_template(pkg, "successor_function");
    TEST_ASSERT(t && t->param_count == 1, "successor_function should have 1 param");

    t = axiom_package_get_template(pkg, "projection_function");
    TEST_ASSERT(t && t->param_count == 2, "projection_function should have 2 params");

    t = axiom_package_get_template(pkg, "composition");
    TEST_ASSERT(t && t->param_count == 3, "composition should have 3 params");

    t = axiom_package_get_template(pkg, "primitive_recursion");
    TEST_ASSERT(t && t->param_count == 2, "primitive_recursion should have 2 params");

    t = axiom_package_get_template(pkg, "minimization_operator");
    TEST_ASSERT(t && t->param_count == 2, "minimization_operator should have 2 params");

    t = axiom_package_get_template(pkg, "universal_turing_machine");
    TEST_ASSERT(t && t->param_count == 2, "universal_turing_machine should have 2 params");

    t = axiom_package_get_template(pkg, "kleene_T_predicate");
    TEST_ASSERT(t && t->param_count == 3, "kleene_T_predicate should have 3 params");

    t = axiom_package_get_template(pkg, "kleene_normal_form");
    TEST_ASSERT(t && t->param_count == 2, "kleene_normal_form should have 2 params");

    t = axiom_package_get_template(pkg, "smn_theorem");
    TEST_ASSERT(t && t->param_count == 2, "smn_theorem should have 2 params");

    t = axiom_package_get_template(pkg, "kleene_recursion_theorem");
    TEST_ASSERT(t && t->param_count == 1, "kleene_recursion_theorem should have 1 param");

    t = axiom_package_get_template(pkg, "rice_theorem");
    TEST_ASSERT(t && t->param_count == 1, "rice_theorem should have 1 param");

    t = axiom_package_get_template(pkg, "halting_set_K");
    TEST_ASSERT(t && t->param_count == 1, "halting_set_K should have 1 param");

    t = axiom_package_get_template(pkg, "turing_jump");
    TEST_ASSERT(t && t->param_count == 1, "turing_jump should have 1 param");

    t = axiom_package_get_template(pkg, "sigma_n_set");
    TEST_ASSERT(t && t->param_count == 2, "sigma_n_set should have 2 params");

    t = axiom_package_get_template(pkg, "ackermann_function");
    TEST_ASSERT(t && t->param_count == 2, "ackermann_function should have 2 params");

    t = axiom_package_get_template(pkg, "busy_beaver_function");
    TEST_ASSERT(t && t->param_count == 1, "busy_beaver_function should have 1 param");

    t = axiom_package_get_template(pkg, "friedberg_muchnik_theorem");
    TEST_ASSERT(t && t->param_count == 0, "friedberg_muchnik_theorem should have 0 params (existence theorem)");

    t = axiom_package_get_template(pkg, "primitive_addition");
    TEST_ASSERT(t && t->param_count == 2, "primitive_addition should have 2 params");

    t = axiom_package_get_template(pkg, "kolmogorov_complexity");
    TEST_ASSERT(t && t->param_count == 1, "kolmogorov_complexity should have 1 param");

    axiom_package_destroy(pkg);
}

/* Test 3：不可构造项（文件特有：printf 为 [%2d] 对齐格式，保留原体） */
static void test_unconstructible_problems(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "should have 14 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        {"halting_problem", "non_computable_set", 3, true},
        {"rice_theorem_undecidability", "halting_problem", 3, true},
        {"totality_problem", "non_computable_set", 2, true},
        {"program_equivalence_problem", "non_computable_set", 2, true},
        {"post_correspondence_problem", "halting_problem", 2, true},
        {"hilberts_tenth_problem", "halting_problem", 3, true},
        {"word_problem_for_groups", "halting_problem", 2, true},
        {"kolmogorov_complexity_exact", "non_computable_function", 3, true},
        {"busy_beaver_values", "non_computable_function", 2, true},
        {"entscheidungsproblem", "halting_problem", 2, true},
        {"tiling_problem", "halting_problem", 2, true},
        {"mortal_matrix_problem", "halting_problem", 1, true},
        {"posts_problem_uniform_solution", "non_uniform_construction", 3, true},
        {"zero_of_computable_function", "halting_problem", 2, true},
    };

    for (int i = 0; i < (int) (sizeof(expected) / sizeof(expected[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        TEST_ASSERT(uc != NULL, expected[i].name);

        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL && strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                        expected[i].name);
            TEST_ASSERT(uc->dependency_chain.count == expected[i].dep_count, expected[i].name);
            TEST_ASSERT(uc->green_verified == expected[i].green_verified, expected[i].name);
            TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "should have external_ref URL");
            printf("  [%2d] %-40s -> %-30s (deps=%d, verified=%s)\n", i, uc->name, uc->reduces_to, uc->dependency_chain.count,
                   uc->green_verified ? "true" : "false");
        }
    }

    axiom_package_destroy(pkg);
}

/* Test 4/5/6/7/8 已收敛至 kCases 数据驱动用例（见上） */

/* Test 9：外部引用（文件特有：https 计数格式，保留原体） */
static void test_external_references(void) {
    printf("Test 9: Verify external references...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    int valid_ref_count = 0;
    for (int i = 0; i < axiom_package_get_unconstructible_count(pkg); i++) {
        KnownUnconstructible *uc = axiom_package_get_unconstructible(pkg, i);
        if (uc->external_ref) {
            /* Check that all external refs start with https:// */
            bool is_https = (strncmp(uc->external_ref, "https://", 8) == 0);
            TEST_ASSERT(is_https, uc->external_ref);
            if (is_https)
                valid_ref_count++;
        }
    }
    printf("  Valid external refs: %d / %d\n", valid_ref_count, axiom_package_get_unconstructible_count(pkg));

    axiom_package_destroy(pkg);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

/* ---- Test 10: Template group coverage ---- */

static void test_template_group_coverage(void) {
    printf("Test 10: Template group coverage...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify that key templates from each group exist */
    struct {
        const char *name;
        const char *group;
    } key_templates[] = {
        {"zero_function", "I: Initial Functions"},
        {"successor_function", "I: Initial Functions"},
        {"projection_function", "I: Initial Functions"},
        {"composition", "II: Operators"},
        {"primitive_recursion", "II: Operators"},
        {"minimization_operator", "II: Operators"},
        {"universal_turing_machine", "III: Constructions"},
        {"kleene_T_predicate", "III: Constructions"},
        {"kleene_normal_form", "IV: Theorems"},
        {"smn_theorem", "IV: Theorems"},
        {"kleene_recursion_theorem", "IV: Theorems"},
        {"rice_theorem", "IV: Theorems"},
        {"computable_set", "V: Sets"},
        {"computably_enumerable_set", "V: Sets"},
        {"halting_set_K", "V: Sets"},
        {"turing_reducibility", "VI: Degrees"},
        {"turing_jump", "VI: Degrees"},
        {"sigma_n_set", "VII: Arithmetical"},
        {"pi_n_set", "VII: Arithmetical"},
        {"delta_n_set", "VII: Arithmetical"},
        {"post_theorem", "VII: Arithmetical"},
        {"cantor_pairing", "VIII: Constructors"},
        {"godel_numbering", "VIII: Constructors"},
        {"diagonalization", "VIII: Constructors"},
        {"oracle_turing_machine", "VIII: Constructors"},
        {"primitive_addition", "IX: Primitive Recursive"},
        {"primitive_multiplication", "IX: Primitive Recursive"},
        {"bounded_minimization", "IX: Primitive Recursive"},
        {"ackermann_function", "X: Advanced"},
        {"busy_beaver_function", "X: Advanced"},
        {"kolmogorov_complexity", "X: Advanced"},
        {"friedberg_muchnik_theorem", "X: Advanced"},
        {"computable_real_number", "XI: Computable Reals"},
    };

    int found = 0;
    int total = (int) (sizeof(key_templates) / sizeof(key_templates[0]));
    for (int i = 0; i < total; i++) {
        ConstraintTemplate *t = axiom_package_get_template(pkg, key_templates[i].name);
        if (t) {
            found++;
        } else {
            printf("  MISSING: %s [%s]\n", key_templates[i].name, key_templates[i].group);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found == total, "all key templates from all groups should exist");
    printf("  Key template coverage: %d / %d groups represented\n", found, total);

    axiom_package_destroy(pkg);
}

/* ---- Main ---- */

TEST_MAIN_BEGIN("Computability Theory Axiom Package Test Suite")
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Testing: axiom_packages/computability_theory.lvz ===\n\n");
    LV_REGISTER_AXIOM_CASES("ComputabilityTheory", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructible_problems);
    TEST_MAIN_RUN(test_external_references);
    TEST_MAIN_RUN(test_template_group_coverage);
TEST_MAIN_END()
