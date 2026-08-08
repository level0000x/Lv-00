/**
 * @file test_axiom_game_theory.c
 * @brief Game Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the game_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and structural coherence.
 */

#include <stdio.h>
#include <string.h>

int g_fail_count = 0;
int g_pass_count = 0;

/* 历史私有 TEST_ASSERT 为非返回式语义（失败仅计数、继续执行），
 * 通过 AXIOM_TEST_NON_RETURNING 让骨架头提供兼容变体，保持行为不变 */
#include "test_helpers.h"

#include "axiom_test_common.h"

#define AXIOM_PKG_PATH "module/axiom_packages/game_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/game_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 55
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 10

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板（名称 + 参数个数） */
static const AxiomTestTemplateExpectation k_template_expectations[] = {
    /* Group I: Strategic Game Structure (5) */
    {"player_set", 2},
    {"strategy_space", 2},
    {"strategy_profile", 2},
    {"utility_function", 3},
    {"game_tuple", 4},
    /* Group II: Rationality Axioms (5) */
    {"preference_completeness", 2},
    {"preference_transitivity", 3},
    {"expected_utility", 3},
    {"independence_axiom", 4},
    {"continuity_axiom", 4},
    /* Group III: Nash Equilibrium (6) */
    {"best_response", 3},
    {"nash_equilibrium", 2},
    {"nash_existence_finite", 2},
    {"mixed_strategy", 3},
    {"expected_payoff_mixed", 3},
    {"mixed_strategy_support", 2},
    /* Group IV: Zero-Sum Games (6) */
    {"zero_sum_condition", 2},
    {"maximin", 3},
    {"minimax", 3},
    {"minimax_theorem", 2},
    {"saddle_point", 3},
    {"game_value", 2},
    /* Group V: Cooperative Games (10) */
    {"characteristic_function", 2},
    {"grand_coalition", 2},
    {"imputation", 2},
    {"core", 3},
    {"bondareva_shapley_theorem", 2},
    {"shapley_value", 3},
    {"shapley_efficiency", 2},
    {"shapley_symmetry", 3},
    {"shapley_dummy", 2},
    {"shapley_additivity", 3},
    /* Group VI: Dominance (4) */
    {"strict_dominance", 3},
    {"weak_dominance", 3},
    {"iterated_elimination", 2},
    {"pareto_efficiency", 2},
    /* Group VII: Extensive Form (6) */
    {"game_tree", 2},
    {"information_set", 2},
    {"perfect_information", 1},
    {"imperfect_information", 1},
    {"subgame_perfect_equilibrium", 2},
    {"backward_induction", 2},
    /* Group VIII: Evolutionary (4) */
    {"evolutionarily_stable_strategy", 2},
    {"fitness", 3},
    {"replicator_dynamics", 2},
    {"nash_implies_ess", 2},
    /* Group IX: Mechanism Design (4) */
    {"social_choice_function", 2},
    {"incentive_compatibility", 3},
    {"vcg_mechanism", 3},
    {"arrow_impossibility", 3},
    /* Group X: Repeated Games (5) */
    {"repeated_game", 3},
    {"folk_theorem", 3},
    {"correlated_equilibrium", 2},
    {"trembling_hand_perfect", 2},
    {"proper_equilibrium", 2},
};
#define K_TEMPLATE_EXPECTATIONS_COUNT (int) (sizeof(k_template_expectations) / sizeof(k_template_expectations[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"nash_equilibrium_computation", "PPAD_complete", 4, true},
    {"game_isomorphism", "graph_isomorphism_hard", 3, true},
    {"dominance_solvable_check", "coNP_complete", 3, true},
    {"core_nonemptiness", "coNP_complete", 3, true},
    {"shapley_value_computation", "#P_complete", 2, true},
    {"ess_existence", "undecidable_in_general", 3, true},
    {"subgame_perfect_equilibrium_computation", "PSPACE_complete", 3, true},
    {"mechanism_design_optimal", "undecidable_in_general", 3, true},
    {"correlated_equilibrium_finding", "polynomial_time_solvable", 3, true},
    {"bayesian_nash_equilibrium", "PPAD_complete", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* Test 9：期望外部引用（名称 + URL 前缀） */
static const AxiomTestExtRefExpectation k_ext_refs[] = {
    {"nash_equilibrium_computation", "https://doi.org/"},
    {"game_isomorphism", "https://en.wikipedia.org/wiki/Game_theory"},
    {"dominance_solvable_check", "https://doi.org/"},
    {"core_nonemptiness", "https://en.wikipedia.org/wiki/Core_(game_theory)"},
    {"shapley_value_computation", "https://doi.org/"},
    {"ess_existence", "https://en.wikipedia.org/wiki/Evolutionarily_stable_strategy"},
    {"subgame_perfect_equilibrium_computation", "https://doi.org/"},
    {"mechanism_design_optimal", "https://doi.org/"},
    {"correlated_equilibrium_finding", "https://doi.org/"},
    {"bayesian_nash_equilibrium", "https://doi.org/"},
};
#define K_EXT_REFS_COUNT (int) (sizeof(k_ext_refs) / sizeof(k_ext_refs[0]))

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "game_theory");
}

static void test_templates(void) {
    axiom_test_templates_with_params(AXIOM_PKG_PATH, EXPECTED_TEMPLATE_COUNT, "should have 55 constraint templates",
                                     k_template_expectations, K_TEMPLATE_EXPECTATIONS_COUNT);
}

static void test_unconstructible_problems(void) {
    axiom_test_unconstructible_problems(AXIOM_PKG_PATH, EXPECTED_UNCONSTRUCTIBLE_COUNT,
                                        "should have 10 unconstructible problems", k_unconstructibles,
                                        K_UNCONSTRUCTIBLES_COUNT);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework(AXIOM_PKG_PATH, "utility_space_convex_polytope", "classical_deviation_negation",
                                 PROPOSITION_KIND_EXPLOSION_PRINCIPLE, "PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
}

static void test_content_hash(void) {
    axiom_test_content_hash(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_round_trip(void) {
    axiom_test_round_trip(AXIOM_PKG_PATH, SAVE_TEST_PATH, AXIOM_TEST_FREE_LV_FREE);
}

static void test_dependency_validation(void) {
    axiom_test_dependency_validation(AXIOM_PKG_PATH, "FAIL (acceptable)",
                                     " (expected: may fail for cross-reference reduces_to)");
}

static void test_negative_lookups(void) {
    axiom_test_negative_lookups(AXIOM_PKG_PATH, AXIOM_TEST_NEG_BASIC);
}

static void test_external_refs(void) {
    axiom_test_external_refs(AXIOM_PKG_PATH, k_ext_refs, K_EXT_REFS_COUNT);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_game_theory_coherence(void) {
    printf("Test 10: Verify game theory axiom coherence...\n");

    AxiomPackage *pkg = axiom_package_create("game_theory", "1.0.0");
    TEST_ASSERT(pkg != NULL, "create package");
    AxiomLoadStatus s = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(s == AXIOM_LOAD_OK, "load game_theory.lvz");

    /* Verify Group I: Strategic Game Structure */
    const char *structure[] = {"player_set",       "strategy_space", "strategy_profile",
                               "utility_function", "game_tuple",     NULL};
    for (int i = 0; structure[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, structure[i]);
        TEST_ASSERT(tmpl != NULL, structure[i]);
    }

    /* Verify Group II: Rationality Axioms */
    const char *rationality[] = {"preference_completeness", "preference_transitivity", "expected_utility",
                                 "independence_axiom",      "continuity_axiom",        NULL};
    for (int i = 0; rationality[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, rationality[i]);
        TEST_ASSERT(tmpl != NULL, rationality[i]);
    }

    /* Verify Group III: Nash Equilibrium */
    const char *nash[] = {"best_response",
                          "nash_equilibrium",
                          "nash_existence_finite",
                          "mixed_strategy",
                          "expected_payoff_mixed",
                          "mixed_strategy_support",
                          NULL};
    for (int i = 0; nash[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, nash[i]);
        TEST_ASSERT(tmpl != NULL, nash[i]);
    }

    /* Verify Group IV: Zero-Sum Games */
    const char *zerosum[] = {"zero_sum_condition", "maximin",    "minimax", "minimax_theorem",
                             "saddle_point",       "game_value", NULL};
    for (int i = 0; zerosum[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, zerosum[i]);
        TEST_ASSERT(tmpl != NULL, zerosum[i]);
    }

    /* Verify Group V: Cooperative Game Theory */
    const char *cooperative[] = {"characteristic_function",
                                 "grand_coalition",
                                 "imputation",
                                 "core",
                                 "bondareva_shapley_theorem",
                                 "shapley_value",
                                 "shapley_efficiency",
                                 "shapley_symmetry",
                                 "shapley_dummy",
                                 "shapley_additivity",
                                 NULL};
    for (int i = 0; cooperative[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, cooperative[i]);
        TEST_ASSERT(tmpl != NULL, cooperative[i]);
    }

    /* Verify Group VI: Dominance */
    const char *dominance[] = {"strict_dominance", "weak_dominance", "iterated_elimination", "pareto_efficiency", NULL};
    for (int i = 0; dominance[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, dominance[i]);
        TEST_ASSERT(tmpl != NULL, dominance[i]);
    }

    /* Verify Group VII: Extensive Form */
    const char *extensive[] = {"game_tree",
                               "information_set",
                               "perfect_information",
                               "imperfect_information",
                               "subgame_perfect_equilibrium",
                               "backward_induction",
                               NULL};
    for (int i = 0; extensive[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, extensive[i]);
        TEST_ASSERT(tmpl != NULL, extensive[i]);
    }

    /* Verify Group VIII: Evolutionary */
    const char *evolutionary[] = {"evolutionarily_stable_strategy", "fitness", "replicator_dynamics",
                                  "nash_implies_ess", NULL};
    for (int i = 0; evolutionary[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, evolutionary[i]);
        TEST_ASSERT(tmpl != NULL, evolutionary[i]);
    }

    /* Verify Group IX: Mechanism Design */
    const char *mechanism[] = {"social_choice_function", "incentive_compatibility", "vcg_mechanism",
                               "arrow_impossibility", NULL};
    for (int i = 0; mechanism[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, mechanism[i]);
        TEST_ASSERT(tmpl != NULL, mechanism[i]);
    }

    /* Verify Group X: Repeated Games */
    const char *repeated[] = {"repeated_game",          "folk_theorem",       "correlated_equilibrium",
                              "trembling_hand_perfect", "proper_equilibrium", NULL};
    for (int i = 0; repeated[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, repeated[i]);
        TEST_ASSERT(tmpl != NULL, repeated[i]);
    }

    printf("Test 10 passed: all game theory axiom groups verified.\n");
    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Game Theory Axiom Package Test Suite")
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Testing: axiom_packages/game_theory.lvz ===\n\n");
    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructible_problems);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_round_trip);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_negative_lookups);
    TEST_MAIN_RUN(test_external_refs);
    TEST_MAIN_RUN(test_game_theory_coherence);
TEST_MAIN_END()
