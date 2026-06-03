/**
 * @file test_rewrite_strategy.c
 * @brief Tests for the extended rewrite strategy engine.
 *
 * @details Tests cover:
 *          - Engine lifecycle (create, destroy)
 *          - Rule management (add rules with priorities)
 *          - Innermost strategy
 *          - Outermost strategy
 *          - Parallel strategy
 *          - E-graph strategy
 *          - Conditional rules
 *          - Iteration limits
 *          - Priority ordering
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"
#include "rewrite_strategy.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Test: Engine lifecycle
 * ============================================================ */

static void test_rewrite_engine_create_destroy(void) {
    Lv00RewriteEngineEx *engine = rewrite_engine_ex_create(REWRITE_INNERMOST, 100);
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQ(engine->strategy, REWRITE_INNERMOST);
    TEST_ASSERT_EQ(engine->max_iterations, 100);
    TEST_ASSERT_EQ(engine->rule_count, 0);
    rewrite_engine_ex_destroy(engine);

    /* Test with default max_iterations (0) */
    engine = rewrite_engine_ex_create(REWRITE_OUTERMOST, 0);
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQ(engine->max_iterations, 1000);
    rewrite_engine_ex_destroy(engine);

    /* Test NULL-safe destroy */
    rewrite_engine_ex_destroy(NULL);
}

/* ============================================================
 * Test: Add rules
 * ============================================================ */

static void test_rewrite_engine_add_rule(void) {
    Lv00RewriteEngineEx *engine = rewrite_engine_ex_create(REWRITE_INNERMOST, 100);
    TEST_ASSERT_NOT_NULL(engine);

    bool ok = rewrite_engine_ex_add_rule(engine, "simplify_add_0",
        "x + 0", "x", 1, NULL);
    TEST_ASSERT(ok, "add_rule should succeed");

    ok = rewrite_engine_ex_add_rule(engine, "simplify_mul_1",
        "x * 1", "x", 2, NULL);
    TEST_ASSERT(ok, "add_rule should succeed");

    TEST_ASSERT_EQ(engine->rule_count, 2);

    /* Rules should be sorted by priority */
    TEST_ASSERT_EQ(strcmp(engine->rules[0].name, "simplify_add_0"), 0);
    TEST_ASSERT_EQ(strcmp(engine->rules[1].name, "simplify_mul_1"), 0);

    /* Test NULL parameters */
    ok = rewrite_engine_ex_add_rule(NULL, "name", "p", "r", 1, NULL);
    TEST_ASSERT(!ok, "add_rule with NULL engine should fail");

    rewrite_engine_ex_destroy(engine);
}

/* ============================================================
 * Test: Innermost strategy
 * ============================================================ */

static void test_innermost_strategy(void) {
    Lv00RewriteEngineEx *engine = rewrite_engine_ex_create(REWRITE_INNERMOST, 100);
    TEST_ASSERT_NOT_NULL(engine);

    /* Rule: replace "a" with "b" */
    rewrite_engine_ex_add_rule(engine, "a_to_b", "a", "b", 1, NULL);

    Lv00RewriteResultEx result;
    bool ok = rewrite_engine_ex_apply(engine, "aaa", &result);
    TEST_ASSERT(ok, "apply should succeed");
    TEST_ASSERT_NOT_NULL(result.output);
    TEST_ASSERT_STR_EQ(result.output, "bbb");
    TEST_ASSERT(result.converged, "should converge");
    rewrite_engine_result_ex_destroy(&result);

    rewrite_engine_ex_destroy(engine);
}

/* ============================================================
 * Test: Outermost strategy
 * ============================================================ */

static void test_outermost_strategy(void) {
    Lv00RewriteEngineEx *engine = rewrite_engine_ex_create(REWRITE_OUTERMOST, 100);
    TEST_ASSERT_NOT_NULL(engine);

    /* Rule: replace "ab" with "x" */
    rewrite_engine_ex_add_rule(engine, "ab_to_x", "ab", "x", 1, NULL);

    Lv00RewriteResultEx result;
    bool ok = rewrite_engine_ex_apply(engine, "abab", &result);
    TEST_ASSERT(ok, "apply should succeed");
    TEST_ASSERT_NOT_NULL(result.output);
    TEST_ASSERT_STR_EQ(result.output, "xx");
    TEST_ASSERT(result.converged, "should converge");
    rewrite_engine_result_ex_destroy(&result);

    rewrite_engine_ex_destroy(engine);
}

/* ============================================================
 * Test: Parallel strategy
 * ============================================================ */

static void test_parallel_strategy(void) {
    Lv00RewriteEngineEx *engine = rewrite_engine_ex_create(REWRITE_PARALLEL, 100);
    TEST_ASSERT_NOT_NULL(engine);

    /* Two independent rules */
    rewrite_engine_ex_add_rule(engine, "a_to_A", "a", "A", 1, NULL);
    rewrite_engine_ex_add_rule(engine, "b_to_B", "b", "B", 2, NULL);

    Lv00RewriteResultEx result;
    bool ok = rewrite_engine_ex_apply(engine, "abba", &result);
    TEST_ASSERT(ok, "apply should succeed");
    TEST_ASSERT_NOT_NULL(result.output);
    TEST_ASSERT_STR_EQ(result.output, "ABBA");
    TEST_ASSERT(result.converged, "should converge");
    rewrite_engine_result_ex_destroy(&result);

    rewrite_engine_ex_destroy(engine);
}

/* ============================================================
 * Test: E-graph strategy
 * ============================================================ */

static void test_egraph_strategy(void) {
    Lv00RewriteEngineEx *engine = rewrite_engine_ex_create(REWRITE_EGRAPH, 100);
    TEST_ASSERT_NOT_NULL(engine);

    /* Commutativity-like rule: "ab" <-> "ba" */
    rewrite_engine_ex_add_rule(engine, "commute", "ab", "ba", 1, NULL);

    Lv00RewriteResultEx result;
    bool ok = rewrite_engine_ex_apply(engine, "ab", &result);
    TEST_ASSERT(ok, "apply should succeed");
    TEST_ASSERT_NOT_NULL(result.output);
    /* E-graph picks the canonical (lexicographically smallest) form */
    TEST_ASSERT_STR_EQ(result.output, "ab");
    rewrite_engine_result_ex_destroy(&result);

    rewrite_engine_ex_destroy(engine);
}

/* ============================================================
 * Test: Conditional rules
 * ============================================================ */

/* Condition: only apply if term contains "safe" */
static bool condition_contains_safe(const char *term) {
    return term != NULL && strstr(term, "safe") != NULL;
}

static void test_conditional_rules(void) {
    Lv00RewriteEngineEx *engine = rewrite_engine_ex_create(REWRITE_INNERMOST, 100);
    TEST_ASSERT_NOT_NULL(engine);

    /* Rule with condition */
    rewrite_engine_ex_add_rule(engine, "conditional",
        "x", "y", 1, condition_contains_safe);

    Lv00RewriteResultEx result;

    /* Condition satisfied: term contains "safe" */
    bool ok = rewrite_engine_ex_apply(engine, "safe_x", &result);
    TEST_ASSERT(ok, "apply should succeed");
    TEST_ASSERT_NOT_NULL(result.output);
    TEST_ASSERT_STR_EQ(result.output, "safe_y");
    rewrite_engine_result_ex_destroy(&result);

    /* Condition not satisfied: term does not contain "safe" */
    ok = rewrite_engine_ex_apply(engine, "untrusted_x", &result);
    TEST_ASSERT(ok, "apply should succeed");
    TEST_ASSERT_NOT_NULL(result.output);
    TEST_ASSERT_STR_EQ(result.output, "untrusted_x");
    rewrite_engine_result_ex_destroy(&result);

    rewrite_engine_ex_destroy(engine);
}

/* ============================================================
 * Test: Iteration limits
 * ============================================================ */

static void test_iteration_limit(void) {
    Lv00RewriteEngineEx *engine = rewrite_engine_ex_create(REWRITE_INNERMOST, 3);
    TEST_ASSERT_NOT_NULL(engine);

    /* Rule that expands: "a" -> "aa" (non-terminating) */
    rewrite_engine_ex_add_rule(engine, "expand", "a", "aa", 1, NULL);

    Lv00RewriteResultEx result;
    bool ok = rewrite_engine_ex_apply(engine, "a", &result);
    TEST_ASSERT(ok, "apply should succeed");
    TEST_ASSERT(result.hit_limit, "should hit iteration limit");
    TEST_ASSERT(!result.converged, "should not converge");
    TEST_ASSERT_EQ(result.iterations, 3);
    rewrite_engine_result_ex_destroy(&result);

    rewrite_engine_ex_destroy(engine);
}

/* ============================================================
 * Test: Empty engine (no rules)
 * ============================================================ */

static void test_empty_engine(void) {
    Lv00RewriteEngineEx *engine = rewrite_engine_ex_create(REWRITE_INNERMOST, 100);
    TEST_ASSERT_NOT_NULL(engine);

    Lv00RewriteResultEx result;
    bool ok = rewrite_engine_ex_apply(engine, "input_term", &result);
    TEST_ASSERT(ok, "apply should succeed");
    TEST_ASSERT_NOT_NULL(result.output);
    TEST_ASSERT_STR_EQ(result.output, "input_term");
    TEST_ASSERT(result.converged, "should converge immediately");
    rewrite_engine_result_ex_destroy(&result);

    rewrite_engine_ex_destroy(engine);
}

/* ============================================================
 * Test: Multi-step simplification
 * ============================================================ */

static void test_multi_step_simplification(void) {
    Lv00RewriteEngineEx *engine = rewrite_engine_ex_create(REWRITE_INNERMOST, 100);
    TEST_ASSERT_NOT_NULL(engine);

    /* Simplification chain: "BIG" -> "big" -> "small" -> "tiny" */
    rewrite_engine_ex_add_rule(engine, "step1", "BIG", "big", 1, NULL);
    rewrite_engine_ex_add_rule(engine, "step2", "big", "small", 2, NULL);
    rewrite_engine_ex_add_rule(engine, "step3", "small", "tiny", 3, NULL);

    Lv00RewriteResultEx result;
    bool ok = rewrite_engine_ex_apply(engine, "BIG", &result);
    TEST_ASSERT(ok, "apply should succeed");
    TEST_ASSERT_NOT_NULL(result.output);
    TEST_ASSERT_STR_EQ(result.output, "tiny");
    TEST_ASSERT(result.converged, "should converge");
    rewrite_engine_result_ex_destroy(&result);

    rewrite_engine_ex_destroy(engine);
}

/* ============================================================
 * Test: Result destroy NULL safety
 * ============================================================ */

static void test_result_destroy_null(void) {
    rewrite_engine_result_ex_destroy(NULL);
    g_pass_count++; /* Should not crash */
}

/* ============================================================
 * Test: Apply with NULL parameters
 * ============================================================ */

static void test_apply_null_params(void) {
    Lv00RewriteEngineEx *engine = rewrite_engine_ex_create(REWRITE_INNERMOST, 100);
    TEST_ASSERT_NOT_NULL(engine);

    Lv00RewriteResultEx result;
    bool ok = rewrite_engine_ex_apply(NULL, "input", &result);
    TEST_ASSERT(!ok, "apply with NULL engine should fail");

    ok = rewrite_engine_ex_apply(engine, NULL, &result);
    TEST_ASSERT(!ok, "apply with NULL input should fail");

    ok = rewrite_engine_ex_apply(engine, "input", NULL);
    TEST_ASSERT(!ok, "apply with NULL result should fail");

    rewrite_engine_ex_destroy(engine);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void) {
    TEST_SUITE_BEGIN("Rewrite Strategy Engine");

    TEST_RUN(test_rewrite_engine_create_destroy);
    TEST_RUN(test_rewrite_engine_add_rule);
    TEST_RUN(test_innermost_strategy);
    TEST_RUN(test_outermost_strategy);
    TEST_RUN(test_parallel_strategy);
    TEST_RUN(test_egraph_strategy);
    TEST_RUN(test_conditional_rules);
    TEST_RUN(test_iteration_limit);
    TEST_RUN(test_empty_engine);
    TEST_RUN(test_multi_step_simplification);
    TEST_RUN(test_result_destroy_null);
    TEST_RUN(test_apply_null_params);

    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}
