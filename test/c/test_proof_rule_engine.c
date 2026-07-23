/**
 * @file test_proof_rule_engine.c
 * @brief Proof rule engine and proof session tests
 *
 * Tests the proof rule search engine and proof session management:
 * - Rule engine creation and configuration
 * - Rule registration and lookup
 * - Best-first search with weighted rules
 * - Proof state management (goal stack, hypotheses)
 * - Proof session lifecycle (create, submit steps, get state, destroy)
 *
 * Follows the Lv-00 test framework pattern with TEST_ASSERT macros
 * and global pass/fail counters.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "proof_rule_engine.h"
#include "proof_session.h"
#include "test_helpers.h"

/* Global test counters */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============== Test: Rule Engine Creation ============== */

static void test_rule_engine_create(void) {
    printf("Testing rule engine creation...\n");

    /* Test default creation */
    lvRuleEngine *engine = rule_engine_create();
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQ(rule_engine_rule_count(engine), 0);

    /* Verify default configuration */
    TEST_ASSERT_EQ(engine->search_strategy, SEARCH_BEST_FIRST);
    TEST_ASSERT(engine->max_depth > 0, "max_depth should be positive");
    TEST_ASSERT_EQ(engine->timeout_ms, 0);

    rule_engine_destroy(engine);

    /* Test creation with custom parameters */
    engine = rule_engine_create_ex(SEARCH_DEPTH_FIRST, 16, 5000);
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQ(engine->search_strategy, SEARCH_DEPTH_FIRST);
    TEST_ASSERT_EQ(engine->max_depth, 16);
    TEST_ASSERT_EQ(engine->timeout_ms, 5000);

    rule_engine_destroy(engine);

    /* Test NULL safety on destroy */
    rule_engine_destroy(NULL);

    printf("  PASSED\n");
}

/* ============== Test: Rule Engine Add Rule ============== */

/* Sample applicability check: always applicable */
static bool sample_always_applicable(const void *rule, const void *state) {
    (void)rule;
    (void)state;
    return true;
}

/* Sample applicability check: never applicable */
static bool sample_never_applicable(const void *rule, const void *state) {
    (void)rule;
    (void)state;
    return false;
}

/* Sample apply function: pops the current goal (proves it) */
static bool sample_pop_goal_apply(void *rule, void *state) {
    (void)rule;
    return proof_state_pop_goal((lvProofState *)state);
}

/* Sample apply function: pushes a sub-goal */
static bool sample_push_subgoal_apply(void *rule, void *state) {
    (void)rule;
    return proof_state_push_goal((lvProofState *)state, "sub_goal_trivial");
}

static void test_rule_engine_add_rule(void) {
    printf("Testing rule engine add rule...\n");

    lvRuleEngine *engine = rule_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    /* Create and add a rule */
    lvProofRule *rule = (lvProofRule *)lv_malloc(sizeof(lvProofRule));
    TEST_ASSERT_NOT_NULL(rule);
    memset(rule, 0, sizeof(lvProofRule));
    strncpy(rule->name, "test_intro", lv_PROOF_RULE_NAME_MAX - 1);
    rule->type = RULE_INTRO;
    rule->priority = 10;
    rule->weight = 0.8;
    rule->applicability_check_fn = sample_always_applicable;
    rule->apply_fn = sample_pop_goal_apply;

    bool ok = rule_engine_add_rule(engine, rule);
    TEST_ASSERT(ok, "rule_engine_add_rule should succeed");
    TEST_ASSERT_EQ(rule_engine_rule_count(engine), 1);

    /* Verify the rule can be found by name */
    const lvProofRule *found = rule_engine_find_rule(engine, "test_intro");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQ(found->type, RULE_INTRO);
    TEST_ASSERT(found->weight > 0.7, "found rule weight should be greater than 0.7");

    /* Test finding non-existent rule */
    found = rule_engine_find_rule(engine, "nonexistent");
    TEST_ASSERT_NULL(found);

    /* Add a second rule */
    lvProofRule *rule2 = (lvProofRule *)lv_malloc(sizeof(lvProofRule));
    TEST_ASSERT_NOT_NULL(rule2);
    memset(rule2, 0, sizeof(lvProofRule));
    strncpy(rule2->name, "test_elim", lv_PROOF_RULE_NAME_MAX - 1);
    rule2->type = RULE_ELIM;
    rule2->priority = 5;
    rule2->weight = 0.5;
    rule2->applicability_check_fn = sample_always_applicable;
    rule2->apply_fn = sample_push_subgoal_apply;

    ok = rule_engine_add_rule(engine, rule2);
    TEST_ASSERT(ok, "rule_engine_add_rule second rule should succeed");
    TEST_ASSERT_EQ(rule_engine_rule_count(engine), 2);

    /* Test remove rule */
    ok = rule_engine_remove_rule(engine, "test_elim");
    TEST_ASSERT(ok, "rule_engine_remove_rule should succeed");
    TEST_ASSERT_EQ(rule_engine_rule_count(engine), 1);

    /* Verify removed rule is gone */
    found = rule_engine_find_rule(engine, "test_elim");
    TEST_ASSERT_NULL(found);

    /* Test removing non-existent rule */
    ok = rule_engine_remove_rule(engine, "nonexistent");
    TEST_ASSERT(!ok, "rule_engine_remove_rule nonexistent should fail");

    /* Test NULL arguments */
    ok = rule_engine_add_rule(NULL, rule);
    TEST_ASSERT(!ok, "add_rule with NULL engine should fail");
    ok = rule_engine_add_rule(engine, NULL);
    TEST_ASSERT(!ok, "add_rule with NULL rule should fail");

    rule_engine_destroy(engine);
    printf("  PASSED\n");
}

/* ============== Test: Rule Engine Search (Simple) ============== */

static void test_rule_engine_search_simple(void) {
    printf("Testing rule engine simple search...\n");

    lvRuleEngine *engine = rule_engine_create_ex(SEARCH_BEST_FIRST, 8, 0);
    TEST_ASSERT_NOT_NULL(engine);

    /* Add a rule that proves the current goal by popping it */
    lvProofRule *solve_rule = (lvProofRule *)lv_malloc(sizeof(lvProofRule));
    TEST_ASSERT_NOT_NULL(solve_rule);
    memset(solve_rule, 0, sizeof(lvProofRule));
    strncpy(solve_rule->name, "solve_trivial", lv_PROOF_RULE_NAME_MAX - 1);
    solve_rule->type = RULE_INTRO;
    solve_rule->priority = 100;
    solve_rule->weight = 1.0;
    solve_rule->applicability_check_fn = sample_always_applicable;
    solve_rule->apply_fn = sample_pop_goal_apply;

    rule_engine_add_rule(engine, solve_rule);

    /* Create a proof state with a single goal */
    lvProofState *state = proof_state_create("trivial_goal");
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT(!proof_state_is_complete(state), "state should not be complete initially");
    TEST_ASSERT_STR_EQ(proof_state_current_goal(state), "trivial_goal");

    /* Run search -- should find proof immediately */
    lvSearchResultStatus result = rule_engine_search(engine, state);
    TEST_ASSERT_EQ(result, SEARCH_RESULT_FOUND);
    TEST_ASSERT(proof_state_is_complete(state), "state should be complete");

    /* Verify rule was recorded */
    TEST_ASSERT_EQ(state->applied_rule_count, 1);

    proof_state_destroy(state);

    /* Test search with no applicable rules (exhausted) */
    lvRuleEngine *empty_engine = rule_engine_create();
    TEST_ASSERT_NOT_NULL(empty_engine);

    lvProofState *state2 = proof_state_create("unsolvable_goal");
    TEST_ASSERT_NOT_NULL(state2);

    result = rule_engine_search(empty_engine, state2);
    TEST_ASSERT_EQ(result, SEARCH_RESULT_EXHAUSTED);
    TEST_ASSERT(!proof_state_is_complete(state2), "state2 should remain incomplete");

    proof_state_destroy(state2);
    rule_engine_destroy(empty_engine);

    /* Test search with depth limit */
    lvRuleEngine *deep_engine = rule_engine_create_ex(SEARCH_BEST_FIRST, 1, 0);
    TEST_ASSERT_NOT_NULL(deep_engine);

    /* Add a rule that pushes a sub-goal instead of solving */
    lvProofRule *deep_rule = (lvProofRule *)lv_malloc(sizeof(lvProofRule));
    TEST_ASSERT_NOT_NULL(deep_rule);
    memset(deep_rule, 0, sizeof(lvProofRule));
    strncpy(deep_rule->name, "push_subgoal", lv_PROOF_RULE_NAME_MAX - 1);
    deep_rule->type = RULE_CASE_SPLIT;
    deep_rule->priority = 50;
    deep_rule->weight = 0.9;
    deep_rule->applicability_check_fn = sample_always_applicable;
    deep_rule->apply_fn = sample_push_subgoal_apply;

    rule_engine_add_rule(deep_engine, deep_rule);

    lvProofState *state3 = proof_state_create("deep_goal");
    TEST_ASSERT_NOT_NULL(state3);

    result = rule_engine_search(deep_engine, state3);
    TEST_ASSERT_EQ(result, SEARCH_RESULT_DEPTH_LIMIT);

    proof_state_destroy(state3);
    rule_engine_destroy(deep_engine);

    /* Test NULL arguments */
    result = rule_engine_search(NULL, state);
    TEST_ASSERT_EQ(result, SEARCH_RESULT_ERROR);

    rule_engine_destroy(engine);
    printf("  PASSED\n");
}

/* ============== Test: Proof Session Creation ============== */

static void test_proof_session_create(void) {
    printf("Testing proof session creation...\n");

    /* Create session without rule engine */
    lvProofSession *session = proof_session_create("forall x, P(x) -> P(x)", NULL);
    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_NOT_NULL(proof_session_get_id(session));
    TEST_ASSERT_STR_EQ(proof_session_get_target(session), "forall x, P(x) -> P(x)");
    TEST_ASSERT_EQ(proof_session_get_step_count(session), 0);
    TEST_ASSERT(!proof_session_is_complete(session), "session should not be complete");
    TEST_ASSERT_EQ(proof_session_get_status(session), SESSION_STATUS_ACTIVE);

    proof_session_destroy(session);

    /* Create session with custom ID */
    session = proof_session_create_with_id("my_session_001",
                                            "A /\\ B -> B /\\ A",
                                            NULL);
    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_STR_EQ(proof_session_get_id(session), "my_session_001");
    TEST_ASSERT_STR_EQ(proof_session_get_target(session), "A /\\ B -> B /\\ A");

    proof_session_destroy(session);

    /* Test NULL safety */
    session = proof_session_create(NULL, NULL);
    TEST_ASSERT_NULL(session);

    proof_session_destroy(NULL);

    printf("  PASSED\n");
}

/* ============== Test: Proof Session Submit Step ============== */

static void test_proof_session_submit_step(void) {
    printf("Testing proof session submit step...\n");

    lvProofSession *session = proof_session_create("P -> P", NULL);
    TEST_ASSERT_NOT_NULL(session);

    lvStepResult result;

    /* Submit an "exact" tactic to close the goal */
    bool ok = proof_session_submit_step(session, "exact P", &result);
    TEST_ASSERT(ok, "submit_step should succeed");
    TEST_ASSERT_EQ(result, STEP_RESULT_PROVED);
    TEST_ASSERT(proof_session_is_complete(session), "session should be complete");
    TEST_ASSERT_EQ(proof_session_get_status(session), SESSION_STATUS_COMPLETE);
    TEST_ASSERT_EQ(proof_session_get_step_count(session), 1);

    proof_session_destroy(session);

    /* Test step rejection (unknown tactic) */
    session = proof_session_create("Q", NULL);
    TEST_ASSERT_NOT_NULL(session);

    ok = proof_session_submit_step(session, "unknown_tactic", &result);
    TEST_ASSERT(ok, "submit_step should process even unknown tactics");
    TEST_ASSERT_EQ(result, STEP_RESULT_REJECTED);
    TEST_ASSERT_EQ(proof_session_get_step_count(session), 0);

    proof_session_destroy(session);

    /* Test session reset */
    session = proof_session_create("R -> R", NULL);
    TEST_ASSERT_NOT_NULL(session);

    ok = proof_session_submit_step(session, "exact R", &result);
    TEST_ASSERT_EQ(result, STEP_RESULT_PROVED);
    TEST_ASSERT(proof_session_is_complete(session), "session should be complete");

    ok = proof_session_reset(session);
    TEST_ASSERT(ok, "reset should succeed");
    TEST_ASSERT(!proof_session_is_complete(session), "session should not be complete");
    TEST_ASSERT_EQ(proof_session_get_step_count(session), 0);
    TEST_ASSERT_EQ(proof_session_get_status(session), SESSION_STATUS_ACTIVE);

    proof_session_destroy(session);

    /* Test session abandonment */
    session = proof_session_create("S", NULL);
    TEST_ASSERT_NOT_NULL(session);

    ok = proof_session_abandon(session);
    TEST_ASSERT(ok, "abandon should succeed");
    TEST_ASSERT_EQ(proof_session_get_status(session), SESSION_STATUS_ABANDONED);

    /* Cannot submit steps after abandonment */
    ok = proof_session_submit_step(session, "exact S", &result);
    TEST_ASSERT(!ok, "submit_step should return false for abandoned session");
    TEST_ASSERT_EQ(result, STEP_RESULT_ERROR);

    proof_session_destroy(session);

    printf("  PASSED\n");
}

/* ============== Test: Proof Session JSON State ============== */

static void test_proof_session_get_state_json(void) {
    printf("Testing proof session get state JSON...\n");

    lvProofSession *session = proof_session_create("A -> A", NULL);
    TEST_ASSERT_NOT_NULL(session);

    char *json = proof_session_get_state_json(session);
    TEST_ASSERT_NOT_NULL(json);

    /* Verify JSON contains expected fields */
    TEST_ASSERT(strstr(json, "\"session_id\"") != NULL,
                "JSON should contain session_id field");
    TEST_ASSERT(strstr(json, "\"status\": \"ACTIVE\"") != NULL,
                "JSON should contain ACTIVE status");
    TEST_ASSERT(strstr(json, "\"target_proposition\"") != NULL,
                "JSON should contain target_proposition field");
    TEST_ASSERT(strstr(json, "\"step_count\": 0") != NULL,
                "JSON should show step_count 0");
    TEST_ASSERT(strstr(json, "\"is_complete\": false") != NULL,
                "JSON should show is_complete false");

    if (json) {
        lv_free((void **)&json);
    }

    /* Test NULL session */
    json = proof_session_get_state_json(NULL);
    TEST_ASSERT_NULL(json);

    proof_session_destroy(session);
    printf("  PASSED\n");
}

/* ============== Test: Proof State Management ============== */

static void test_proof_state_management(void) {
    printf("Testing proof state management...\n");

    lvProofState *state = proof_state_create("main_goal");
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT(!proof_state_is_complete(state), "state should not be complete");
    TEST_ASSERT_STR_EQ(proof_state_current_goal(state), "main_goal");

    /* Push sub-goals */
    bool ok = proof_state_push_goal(state, "sub_goal_1");
    TEST_ASSERT(ok, "push_goal should succeed");
    TEST_ASSERT_STR_EQ(proof_state_current_goal(state), "sub_goal_1");

    ok = proof_state_push_goal(state, "sub_goal_2");
    TEST_ASSERT(ok, "push_goal second should succeed");
    TEST_ASSERT_STR_EQ(proof_state_current_goal(state), "sub_goal_2");

    /* Add hypotheses */
    ok = proof_state_add_hypothesis(state, "H1: P");
    TEST_ASSERT(ok, "add_hypothesis should succeed");
    ok = proof_state_add_hypothesis(state, "H2: Q");
    TEST_ASSERT(ok, "add_hypothesis second should succeed");
    TEST_ASSERT_EQ(state->hypothesis_count, 2);

    /* Record applied rules */
    ok = proof_state_record_rule(state, "intro");
    TEST_ASSERT(ok, "record_rule should succeed");
    ok = proof_state_record_rule(state, "apply H1");
    TEST_ASSERT(ok, "record_rule second should succeed");
    TEST_ASSERT_EQ(state->applied_rule_count, 2);

    /* Pop goals in LIFO order */
    ok = proof_state_pop_goal(state);
    TEST_ASSERT(ok, "pop_goal should succeed");
    TEST_ASSERT_STR_EQ(proof_state_current_goal(state), "sub_goal_1");

    ok = proof_state_pop_goal(state);
    TEST_ASSERT(ok, "pop_goal second should succeed");
    TEST_ASSERT_STR_EQ(proof_state_current_goal(state), "main_goal");

    ok = proof_state_pop_goal(state);
    TEST_ASSERT(ok, "pop_goal last should succeed");
    TEST_ASSERT(proof_state_is_complete(state), "state should be complete");
    TEST_ASSERT_NULL(proof_state_current_goal(state));

    /* Pop on empty stack should fail */
    ok = proof_state_pop_goal(state);
    TEST_ASSERT(!ok, "pop_goal on empty stack should fail");

    /* NULL safety */
    ok = proof_state_push_goal(NULL, "x");
    TEST_ASSERT(!ok, "push_goal NULL state should fail");
    ok = proof_state_add_hypothesis(NULL, "x");
    TEST_ASSERT(!ok, "add_hypothesis NULL state should fail");
    ok = proof_state_record_rule(NULL, "x");
    TEST_ASSERT(!ok, "record_rule NULL state should fail");

    proof_state_destroy(state);
    proof_state_destroy(NULL);

    printf("  PASSED\n");
}

/* ============== Test: Utility String Functions ============== */

static void test_utility_string_functions(void) {
    printf("Testing utility string functions...\n");

    /* Proof rule type strings */
    TEST_ASSERT_STR_EQ(proof_rule_type_to_string(RULE_INTRO), "INTRO");
    TEST_ASSERT_STR_EQ(proof_rule_type_to_string(RULE_ELIM), "ELIM");
    TEST_ASSERT_STR_EQ(proof_rule_type_to_string(RULE_REWRITE), "REWRITE");
    TEST_ASSERT_STR_EQ(proof_rule_type_to_string(RULE_INDUCTION), "INDUCTION");
    TEST_ASSERT_STR_EQ(proof_rule_type_to_string(RULE_CONTRADICTION), "CONTRADICTION");
    TEST_ASSERT_STR_EQ(proof_rule_type_to_string(RULE_CASE_SPLIT), "CASE_SPLIT");
    TEST_ASSERT_STR_EQ(proof_rule_type_to_string(RULE_GENERALIZE), "GENERALIZE");
    TEST_ASSERT_STR_EQ(proof_rule_type_to_string(RULE_SPECIALIZE), "SPECIALIZE");
    TEST_ASSERT_STR_EQ(proof_rule_type_to_string(RULE_NEURAL_SUGGEST), "NEURAL_SUGGEST");
    TEST_ASSERT_STR_EQ(proof_rule_type_to_string(RULE_AUX_CONSTRUCT), "AUX_CONSTRUCT");

    /* Search strategy strings */
    TEST_ASSERT_STR_EQ(search_strategy_to_string(SEARCH_BEST_FIRST), "BEST_FIRST");
    TEST_ASSERT_STR_EQ(search_strategy_to_string(SEARCH_DEPTH_FIRST), "DEPTH_FIRST");
    TEST_ASSERT_STR_EQ(search_strategy_to_string(SEARCH_BREADTH_FIRST), "BREADTH_FIRST");
    TEST_ASSERT_STR_EQ(search_strategy_to_string(SEARCH_ITERATIVE_DEEPENING),
                        "ITERATIVE_DEEPENING");

    /* Search result status strings */
    TEST_ASSERT_STR_EQ(search_result_status_to_string(SEARCH_RESULT_FOUND), "FOUND");
    TEST_ASSERT_STR_EQ(search_result_status_to_string(SEARCH_RESULT_TIMEOUT), "TIMEOUT");
    TEST_ASSERT_STR_EQ(search_result_status_to_string(SEARCH_RESULT_DEPTH_LIMIT),
                        "DEPTH_LIMIT");
    TEST_ASSERT_STR_EQ(search_result_status_to_string(SEARCH_RESULT_EXHAUSTED), "EXHAUSTED");
    TEST_ASSERT_STR_EQ(search_result_status_to_string(SEARCH_RESULT_ERROR), "ERROR");

    /* Session status strings */
    TEST_ASSERT_STR_EQ(session_status_to_string(SESSION_STATUS_ACTIVE), "ACTIVE");
    TEST_ASSERT_STR_EQ(session_status_to_string(SESSION_STATUS_COMPLETE), "COMPLETE");
    TEST_ASSERT_STR_EQ(session_status_to_string(SESSION_STATUS_ABANDONED), "ABANDONED");
    TEST_ASSERT_STR_EQ(session_status_to_string(SESSION_STATUS_ERROR), "ERROR");

    /* Step result strings */
    TEST_ASSERT_STR_EQ(step_result_to_string(STEP_RESULT_ACCEPTED), "ACCEPTED");
    TEST_ASSERT_STR_EQ(step_result_to_string(STEP_RESULT_REJECTED), "REJECTED");
    TEST_ASSERT_STR_EQ(step_result_to_string(STEP_RESULT_GOAL_CHANGED), "GOAL_CHANGED");
    TEST_ASSERT_STR_EQ(step_result_to_string(STEP_RESULT_PROVED), "PROVED");
    TEST_ASSERT_STR_EQ(step_result_to_string(STEP_RESULT_ERROR), "ERROR");

    printf("  PASSED\n");
}

/* ============== Main ============== */

int main(void) {
    TEST_SUITE_BEGIN("Proof Rule Engine & Session Tests");

    TEST_RUN(test_rule_engine_create);
    TEST_RUN(test_rule_engine_add_rule);
    TEST_RUN(test_rule_engine_search_simple);
    TEST_RUN(test_proof_session_create);
    TEST_RUN(test_proof_session_submit_step);
    TEST_RUN(test_proof_session_get_state_json);
    TEST_RUN(test_proof_state_management);
    TEST_RUN(test_utility_string_functions);

    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}
