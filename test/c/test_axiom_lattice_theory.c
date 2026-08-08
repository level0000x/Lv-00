/**
 * @file test_axiom_lattice_theory.c
 * @brief Lattice Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the lattice_theory.lvz
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

#define AXIOM_PKG_PATH "module/axiom_packages/lattice_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/lattice_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 53
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Meet Semilattice Axioms (3) */
    {"meet_idempotence", 1},
    {"meet_commutativity", 2},
    {"meet_associativity", 3},
    /* Group II: Join Semilattice Axioms (3) */
    {"join_idempotence", 1},
    {"join_commutativity", 2},
    {"join_associativity", 3},
    /* Group III: Absorption Laws (2) */
    {"absorption_join_over_meet", 2},
    {"absorption_meet_over_join", 2},
    /* Group IV: Bounded Lattice Axioms (2) */
    {"bottom_identity", 1},
    {"top_identity", 1},
    /* Group V: Distributive Lattice Axioms (2) */
    {"meet_distributes_over_join", 3},
    {"join_distributes_over_meet", 3},
    /* Group VI: Modular Lattice Axiom (1) */
    {"modular_law", 3},
    /* Group VII: Complemented Lattice Axiom (1) */
    {"complement_existence", 1},
    /* Group VIII: Core Constructors (7) */
    {"meet", 2},
    {"join", 2},
    {"leq_from_meet", 2},
    {"leq_from_join", 2},
    {"top_element", 0},
    {"bottom_element", 0},
    {"complement", 1},
    /* Group IX: Derived Constructors (8) */
    {"strict_less_than", 2},
    {"incomparable", 2},
    {"covering_relation", 2},
    {"meet_irreducible", 1},
    {"join_irreducible", 1},
    {"is_atom", 1},
    {"is_coatom", 1},
    /* Group X: Sublattice and Homomorphism Constructors (6) */
    {"sublattice_test", 2},
    {"lattice_homomorphism_test", 3},
    {"lattice_isomorphism_test", 3},
    {"direct_product", 2},
    {"dual_lattice", 1},
    {"interval_sublattice", 2},
    /* Group XI: Advanced Lattice Constructions (7) */
    {"ideal", 2},
    {"filter", 2},
    {"prime_ideal_test", 1},
    {"prime_filter_test", 1},
    {"maximal_ideal_test", 1},
    {"congruence_relation_test", 2},
    {"quotient_lattice", 2},
    /* Group XII: Special Lattice Type Verifiers (7) */
    {"is_distributive", 1},
    {"is_modular", 1},
    {"is_complemented", 1},
    {"is_boolean_algebra", 1},
    {"is_heyting_algebra", 1},
    {"is_complete", 1},
    {"is_chain", 1},
    /* Group XIII: Fundamental Lattice Theorems (5) */
    {"whitman_condition", 4},
    {"dedekind_macneille_completion", 1},
    {"birkhoff_representation", 1},
    {"stone_representation", 1},
    {"knaster_tarski_fixed_point", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"lattice_variety_membership", "equational_theory_undecidability", 4, true},
    {"finite_lattice_embeddability", "finite_representation_problem", 3, true},
    {"congruence_lattice_problem", "universal_algebra_undecidability", 3, true},
    {"free_lattice_word_problem", "exp_space_hardness", 3, true},
    {"lattice_isomorphism_problem", "graph_isomorphism_hardness", 2, true},
    {"equational_basis_for_lattice_variety", "finite_basis_problem", 3, true},
    {"lattice_identity_entailment", "equational_unification", 4, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* Test 9：期望外部引用 URL 前缀 */
static const AxiomTestExtRefExpectation k_external_refs[] = {
    {"lattice_variety_membership", "https://en.wikipedia.org/wiki/Lattice_(order)"},
    {"finite_lattice_embeddability", "https://en.wikipedia.org/wiki/Lattice_(order)"},
    {"congruence_lattice_problem", "https://en.wikipedia.org/wiki/Lattice_(order)"},
    {"free_lattice_word_problem", "https://en.wikipedia.org/wiki/Free_lattice"},
    {"lattice_isomorphism_problem", "https://en.wikipedia.org/wiki/Lattice_(order)"},
    {"equational_basis_for_lattice_variety", "https://en.wikipedia.org/wiki/Lattice_(order)"},
    {"lattice_identity_entailment", "https://en.wikipedia.org/wiki/Word_problem_(mathematics)"},
};
#define K_EXTERNAL_REFS_COUNT (int) (sizeof(k_external_refs) / sizeof(k_external_refs[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "lattice_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 53 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（A 形态） */
        .uc_style = AXIOM_TEST_UC_A,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 7 unconstructible problems",
        .uc_expectations = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（S 形态） */
        .lf_style = AXIOM_TEST_LF_S,
        .lf_bottom_geometry = "lattice_partial_order",
        .lf_negation_encoding = "complement_in_complemented_lattice",
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

static void test_lattice_axiom_coherence(void) {
    printf("Test 10: Verify lattice axiom coherence...\n");

    AxiomPackage *pkg = axiom_package_create("lattice_theory", "1.0.0");
    TEST_ASSERT(pkg != NULL, "create package");
    AxiomLoadStatus s = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(s == AXIOM_LOAD_OK, "load lattice_theory.lvz");

    /* Verify the 8 core lattice axioms (L1-L8) are present */
    const char *core_axioms[] = {"meet_idempotence",          "meet_commutativity",        "meet_associativity",
                                 "join_idempotence",          "join_commutativity",        "join_associativity",
                                 "absorption_join_over_meet", "absorption_meet_over_join", NULL};
    for (int i = 0; core_axioms[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_axioms[i]);
        TEST_ASSERT(tmpl != NULL, core_axioms[i]);
    }

    /* Verify core constructors are present */
    const char *core_constructors[] = {"meet",        "join",           "leq_from_meet", "leq_from_join",
                                       "top_element", "bottom_element", "complement",    NULL};
    for (int i = 0; core_constructors[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_constructors[i]);
        TEST_ASSERT(tmpl != NULL, core_constructors[i]);
    }

    /* Verify special lattice type verifiers are present */
    const char *type_verifiers[] = {"is_distributive",    "is_modular",  "is_complemented", "is_boolean_algebra",
                                    "is_heyting_algebra", "is_complete", "is_chain",        NULL};
    for (int i = 0; type_verifiers[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, type_verifiers[i]);
        TEST_ASSERT(tmpl != NULL, type_verifiers[i]);
    }

    /* Verify fundamental theorems are present */
    const char *theorems[] = {"whitman_condition",    "dedekind_macneille_completion", "birkhoff_representation",
                              "stone_representation", "knaster_tarski_fixed_point",    NULL};
    for (int i = 0; theorems[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, theorems[i]);
        TEST_ASSERT(tmpl != NULL, theorems[i]);
    }

    printf("Test 10 passed: all core lattice axioms, constructors, type verifiers, and theorems verified.\n");
    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Lattice Theory")
    LV_REGISTER_AXIOM_CASES("LatticeTheory", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_lattice_axiom_coherence);
TEST_MAIN_END()

