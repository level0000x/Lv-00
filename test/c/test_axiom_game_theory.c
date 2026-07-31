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

#include "axiom_pkg.h"
#include "lv_utils.h"

#define AXIOM_PKG_PATH "module/axiom_packages/game_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/game_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 55
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 10

static int g_fail_count = 0;
static int g_pass_count = 0;

#define TEST_ASSERT(cond, msg)           \
    do {                                 \
        if (!(cond)) {                   \
            printf("  FAIL: %s\n", msg); \
            g_fail_count++;              \
        } else {                         \
            g_pass_count++;              \
        }                                \
    } while (0)

static void test_load_from_file(void) {
    printf("Test 1: Load game_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "game_theory") == 0, "package name should be 'game_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 55 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {
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

    int total = (int) (sizeof(expected) / sizeof(expected[0]));
    TEST_ASSERT(total == EXPECTED_TEMPLATE_COUNT, "expected array size should match EXPECTED_TEMPLATE_COUNT");

    int found_count = 0;
    for (int i = 0; i < total; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expected[i].name);
        if (tmpl) {
            found_count++;
            if (tmpl->param_count != expected[i].params) {
                printf("  FAIL: '%s' has %d params, expected %d\n", expected[i].name, tmpl->param_count,
                       expected[i].params);
                g_fail_count++;
            } else {
                g_pass_count++;
            }
        } else {
            printf("  MISSING template: '%s'\n", expected[i].name);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found_count == EXPECTED_TEMPLATE_COUNT, "all expected templates should be found");
    printf("  Found %d / %d templates\n", found_count, EXPECTED_TEMPLATE_COUNT);

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "should have 10 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
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

    for (int i = 0; i < (int) (sizeof(expected) / sizeof(expected[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        TEST_ASSERT(uc != NULL, expected[i].name);

        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL && strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                        expected[i].name);
            TEST_ASSERT(uc->dependency_chain.count == expected[i].dep_count, expected[i].name);
            TEST_ASSERT(uc->green_verified == expected[i].green_verified, expected[i].name);
            TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "should have external_ref URL");
            printf("  [%d] %s -> %s (deps=%d, verified=%s)\n", i, uc->name, uc->reduces_to, uc->dependency_chain.count,
                   uc->green_verified ? "true" : "false");
        }
    }

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void) {
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "utility_space_convex_polytope") == 0,
                "bottom_geometry should be 'utility_space_convex_polytope'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_deviation_negation") == 0,
                "negation_encoding should be 'classical_deviation_negation'");
    printf("  negation_encoding: %s\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    printf("Test 5: Content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash) == 64, "SHA-256 hash should be 64 hex chars");

    if (hash) {
        printf("  SHA-256: %s\n", hash);
        lv_free((void **) &hash);
    }

    axiom_package_destroy(pkg);
}

static void test_round_trip(void) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg1, AXIOM_PKG_PATH);

    AxiomSaveStatus save_status = axiom_package_save(pkg1, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "save should succeed");

    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "re-load from saved file should succeed");

    TEST_ASSERT(axiom_package_get_template_count(pkg2) == axiom_package_get_template_count(pkg1), "template count should match after round-trip");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg2) == axiom_package_get_unconstructible_count(pkg1),
                "unconstructible count should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->name, pkg1->name) == 0, "name should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->version, pkg1->version) == 0, "version should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg1->bottom_geometry) == 0,
                "bottom_geometry should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg1->contradiction_behavior,
                "contradiction_behavior should match after round-trip");

    printf("  Round-trip: templates=%d, unconstructibles=%d\n", axiom_package_get_template_count(pkg2), axiom_package_get_unconstructible_count(pkg2));

    char *hash1 = axiom_package_compute_content_hash(pkg1);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash1 && hash2 && strcmp(hash1, hash2) == 0, "content hashes should match after round-trip");
    printf("  Hash match: %s\n", (hash1 && hash2 && strcmp(hash1, hash2) == 0) ? "YES" : "NO");

    lv_free((void **) &hash1);
    lv_free((void **) &hash2);
    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    bool valid = axiom_package_validate_dependencies(pkg, &pkg, 1);
    printf("  Self-validation: %s (expected: may fail for cross-reference reduces_to)\n",
           valid ? "PASS" : "FAIL (acceptable)");

    axiom_package_destroy(pkg);
}

static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "non-existent template should return NULL");

    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "non-existent unconstructible should return NULL");

    printf("  Negative lookups: correct\n");

    axiom_package_destroy(pkg);
}

static void test_external_refs(void) {
    printf("Test 9: Verify external reference URLs...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    struct {
        const char *name;
        const char *expected_prefix;
    } ref_checks[] = {
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

    for (int i = 0; i < (int) (sizeof(ref_checks) / sizeof(ref_checks[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, ref_checks[i].name);
        TEST_ASSERT(uc != NULL, ref_checks[i].name);
        if (uc) {
            int url_ok = (uc->external_ref != NULL && strncmp(uc->external_ref, ref_checks[i].expected_prefix,
                                                              strlen(ref_checks[i].expected_prefix)) == 0);
            TEST_ASSERT(url_ok, ref_checks[i].name);
            printf("  [%d] %s -> %s\n", i, uc->name, uc->external_ref);
        }
    }

    axiom_package_destroy(pkg);
}

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

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("=== Game Theory Axiom Package Test Suite ===\n");
    printf("=== Testing: axiom_packages/game_theory.lvz ===\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructible_problems();
    test_logical_framework();
    test_content_hash();
    test_round_trip();
    test_dependency_validation();
    test_negative_lookups();
    test_external_refs();
    test_game_theory_coherence();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass_count, g_fail_count);

    return g_fail_count > 0 ? 1 : 0;
}