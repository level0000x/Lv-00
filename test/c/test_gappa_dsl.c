/**
 * @file test_gappa_dsl.c
 * @brief Test suite for the Gappa DSL module
 *
 * Tests the Gappa-style floating-point proof DSL:
 * - Predefined formats (binary32, binary64)
 * - DSL parsing (hypotheses and goals)
 * - Proof engine (interval-based proving)
 * - Predicate propagation
 *
 * @version 3.4.0
 * @date 2026-05-25
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gappa_dsl.h"
#include "gappa_propagate.h"
#include "interval_arithmetic.h"
#include "test_helpers.h"

/* ============================================================
 * Global test counters
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Test: predefined formats
 * ============================================================ */

static void test_gappa_format_predefined(void) {
    Lv00GappaFormat fmt;

    /* binary32 */
    TEST_ASSERT_MSG(gappa_format_predefined("binary32", &fmt) == true,
                    "binary32 should be recognized");
    TEST_ASSERT_EQ(fmt.precision_bits, 24);
    TEST_ASSERT_EQ(fmt.exponent_bits, 8);
    TEST_ASSERT_EQ(fmt.rounding, LV00_ROUND_NE);
    TEST_ASSERT_MSG(strcmp(fmt.name, "binary32") == 0, "name should be binary32");

    /* binary64 */
    TEST_ASSERT_MSG(gappa_format_predefined("binary64", &fmt) == true,
                    "binary64 should be recognized");
    TEST_ASSERT_EQ(fmt.precision_bits, 53);
    TEST_ASSERT_EQ(fmt.exponent_bits, 11);

    /* binary16 */
    TEST_ASSERT_MSG(gappa_format_predefined("binary16", &fmt) == true,
                    "binary16 should be recognized");
    TEST_ASSERT_EQ(fmt.precision_bits, 11);

    /* binary128 */
    TEST_ASSERT_MSG(gappa_format_predefined("binary128", &fmt) == true,
                    "binary128 should be recognized");
    TEST_ASSERT_EQ(fmt.precision_bits, 113);

    /* Unknown format */
    TEST_ASSERT_MSG(gappa_format_predefined("binary256", &fmt) == false,
                    "binary256 should not be recognized");
}

/* ============================================================
 * Test: DSL parsing
 * ============================================================ */

static void test_gappa_parse_simple(void) {
    Lv00GappaPredicate *hypotheses = NULL;
    int hyp_count = 0;
    Lv00GappaProofGoal *goals = NULL;
    int goal_count = 0;

    /* Parse: "x in [0, 1] -> |x - 0.5| <= 0.5" */
    bool ok = gappa_parse(
        "x in [0, 1] -> |x - 0.5| <= 0.5",
        &hypotheses, &hyp_count,
        &goals, &goal_count);

    TEST_ASSERT_MSG(ok, "parsing should succeed");
    TEST_ASSERT_EQ(hyp_count, 1);
    TEST_ASSERT_EQ(goal_count, 1);

    if (ok && hyp_count > 0) {
        TEST_ASSERT_EQ(hypotheses[0].type, LV00_PRED_BND);
        TEST_ASSERT_MSG(hypotheses[0].is_hypothesis == 1, "should be hypothesis");
        TEST_ASSERT_MSG(fabs(hypotheses[0].bound_lo - 0.0) < 1e-15, "lo should be 0");
        TEST_ASSERT_MSG(fabs(hypotheses[0].bound_hi - 1.0) < 1e-15, "hi should be 1");
    }

    if (ok && goal_count > 0) {
        TEST_ASSERT_EQ(goals[0].predicate.type, LV00_PRED_ABS);
        TEST_ASSERT_MSG(goals[0].predicate.is_hypothesis == 0, "should be goal");
        TEST_ASSERT_MSG(fabs(goals[0].predicate.bound_abs - 0.5) < 1e-15,
                        "bound should be 0.5");
    }

    /* Cleanup */
    gappa_predicates_free(hypotheses, hyp_count);
    gappa_goals_free(goals, goal_count);
}

static void test_gappa_parse_multiple(void) {
    Lv00GappaPredicate *hypotheses = NULL;
    int hyp_count = 0;
    Lv00GappaProofGoal *goals = NULL;
    int goal_count = 0;

    /* Parse multiple statements */
    bool ok = gappa_parse(
        "x in [0, 1]; y in [-1, 1]",
        &hypotheses, &hyp_count,
        &goals, &goal_count);

    TEST_ASSERT_MSG(ok, "parsing multiple hypotheses should succeed");
    TEST_ASSERT_EQ(hyp_count, 2);
    TEST_ASSERT_EQ(goal_count, 0);

    gappa_predicates_free(hypotheses, hyp_count);
    gappa_goals_free(goals, goal_count);
}

/* ============================================================
 * Test: proof engine
 * ============================================================ */

static void test_gappa_prove_simple(void) {
    /* Prove: x in [0,1] => |x - 0.5| <= 0.5 */
    Lv00GappaPredicate hyp;
    memset(&hyp, 0, sizeof(hyp));
    hyp.type = LV00_PRED_BND;
    strncpy(hyp.expr_lhs, "x", sizeof(hyp.expr_lhs) - 1);
    hyp.bound_lo = 0.0;
    hyp.bound_hi = 1.0;
    hyp.is_hypothesis = 1;

    Lv00GappaProofGoal goal;
    memset(&goal, 0, sizeof(goal));
    goal.predicate.type = LV00_PRED_ABS;
    strncpy(goal.predicate.expr_lhs, "x", sizeof(goal.predicate.expr_lhs) - 1);
    strncpy(goal.predicate.expr_rhs, "0.5", sizeof(goal.predicate.expr_rhs) - 1);
    goal.predicate.bound_abs = 0.5;
    goal.predicate.is_hypothesis = 0;

    Lv00GappaProofResult result = gappa_prove(&hyp, 1, &goal, 1, NULL);

    TEST_ASSERT_MSG(result.success == 1, "simple proof should succeed");
    TEST_ASSERT_EQ(result.goals_total, 1);
    TEST_ASSERT_EQ(result.goals_proven, 1);
    TEST_ASSERT_EQ(result.goals_failed, 0);

    gappa_result_free(&result);
}

static void test_gappa_prove_bnd(void) {
    /* Prove: x in [1, 2] => x in [0, 3] */
    Lv00GappaPredicate hyp;
    memset(&hyp, 0, sizeof(hyp));
    hyp.type = LV00_PRED_BND;
    strncpy(hyp.expr_lhs, "x", sizeof(hyp.expr_lhs) - 1);
    hyp.bound_lo = 1.0;
    hyp.bound_hi = 2.0;
    hyp.is_hypothesis = 1;

    Lv00GappaProofGoal goal;
    memset(&goal, 0, sizeof(goal));
    goal.predicate.type = LV00_PRED_BND;
    strncpy(goal.predicate.expr_lhs, "x", sizeof(goal.predicate.expr_lhs) - 1);
    goal.predicate.bound_lo = 0.0;
    goal.predicate.bound_hi = 3.0;
    goal.predicate.is_hypothesis = 0;

    Lv00GappaProofResult result = gappa_prove(&hyp, 1, &goal, 1, NULL);

    TEST_ASSERT_MSG(result.success == 1, "BND proof should succeed");
    TEST_ASSERT_EQ(result.goals_proven, 1);

    gappa_result_free(&result);
}

static void test_gappa_prove_fail(void) {
    /* Try to prove: x in [0,1] => |x - 0.5| <= 0.01 (should fail) */
    Lv00GappaPredicate hyp;
    memset(&hyp, 0, sizeof(hyp));
    hyp.type = LV00_PRED_BND;
    strncpy(hyp.expr_lhs, "x", sizeof(hyp.expr_lhs) - 1);
    hyp.bound_lo = 0.0;
    hyp.bound_hi = 1.0;
    hyp.is_hypothesis = 1;

    Lv00GappaProofGoal goal;
    memset(&goal, 0, sizeof(goal));
    goal.predicate.type = LV00_PRED_ABS;
    strncpy(goal.predicate.expr_lhs, "x", sizeof(goal.predicate.expr_lhs) - 1);
    strncpy(goal.predicate.expr_rhs, "0.5", sizeof(goal.predicate.expr_rhs) - 1);
    goal.predicate.bound_abs = 0.01;
    goal.predicate.is_hypothesis = 0;

    Lv00GappaProofResult result = gappa_prove(&hyp, 1, &goal, 1, NULL);

    TEST_ASSERT_MSG(result.success == 0, "tight bound proof should fail");
    TEST_ASSERT_EQ(result.goals_failed, 1);

    gappa_result_free(&result);
}

/* ============================================================
 * Test: predicate propagation
 * ============================================================ */

static void test_gappa_pred_set(void) {
    Lv00GappaPredSet set;
    gappa_pred_set_init(&set);

    TEST_ASSERT_EQ(set.count, 0);

    Lv00GappaPredicate pred;
    memset(&pred, 0, sizeof(pred));
    pred.type = LV00_PRED_BND;
    strncpy(pred.expr_lhs, "x", sizeof(pred.expr_lhs) - 1);
    pred.bound_lo = 0.0;
    pred.bound_hi = 1.0;

    TEST_ASSERT_MSG(gappa_pred_set_add(&set, &pred) == true, "add should succeed");
    TEST_ASSERT_EQ(set.count, 1);

    /* Find existing */
    Lv00GappaPredicate found;
    int idx = gappa_pred_set_find(&set, "x", &found);
    TEST_ASSERT_MSG(idx >= 0, "should find x");
    TEST_ASSERT_EQ(found.type, LV00_PRED_BND);

    /* Find non-existing */
    idx = gappa_pred_set_find(&set, "y", &found);
    TEST_ASSERT_MSG(idx < 0, "should not find y");

    /* Duplicate add should not increase count */
    TEST_ASSERT_MSG(gappa_pred_set_add(&set, &pred) == false, "duplicate add should fail");
    TEST_ASSERT_EQ(set.count, 1);

    gappa_pred_set_clear(&set);
    TEST_ASSERT_EQ(set.count, 0);
}

static void test_gappa_propagate_forward(void) {
    Lv00GappaPredSet input;
    gappa_pred_set_init(&input);

    /* Add hypotheses: x in [1, 2], y in [3, 4] */
    Lv00GappaPredicate px;
    memset(&px, 0, sizeof(px));
    px.type = LV00_PRED_BND;
    strncpy(px.expr_lhs, "x", sizeof(px.expr_lhs) - 1);
    px.bound_lo = 1.0;
    px.bound_hi = 2.0;
    gappa_pred_set_add(&input, &px);

    Lv00GappaPredicate py;
    memset(&py, 0, sizeof(py));
    py.type = LV00_PRED_BND;
    strncpy(py.expr_lhs, "y", sizeof(py.expr_lhs) - 1);
    py.bound_lo = 3.0;
    py.bound_hi = 4.0;
    gappa_pred_set_add(&input, &py);

    Lv00GappaPredSet output;
    Lv00GappaPropagateConfig cfg = gappa_propagate_config_default();

    int derived = gappa_propagate(&input, &output, &cfg);

    TEST_ASSERT_MSG(derived > 0, "forward propagation should derive new predicates");
    TEST_ASSERT_MSG(output.count > input.count, "output should have more predicates than input");

    /* Check that sum predicate was derived */
    int found_sum = 0;
    for (int i = 0; i < output.count; i++) {
        if (strstr(output.preds[i].expr_lhs, "+") != NULL) {
            found_sum = 1;
            break;
        }
    }
    TEST_ASSERT_MSG(found_sum == 1, "should derive sum predicate");
}

static void test_gappa_propagate_backward(void) {
    Lv00GappaPredSet known;
    gappa_pred_set_init(&known);

    /* Goal: |x - 0.5| <= 0.3 */
    Lv00GappaPredicate goal;
    memset(&goal, 0, sizeof(goal));
    goal.type = LV00_PRED_ABS;
    strncpy(goal.expr_lhs, "x", sizeof(goal.expr_lhs) - 1);
    strncpy(goal.expr_rhs, "0.5", sizeof(goal.expr_rhs) - 1);
    goal.bound_abs = 0.3;

    Lv00GappaPredSet output;
    Lv00GappaPropagateConfig cfg = gappa_propagate_config_default();

    int needed = gappa_propagate_backward(&goal, &known, &output, &cfg);

    TEST_ASSERT_MSG(needed > 0, "backward propagation should generate hypotheses");

    /* Should derive x in [0.2, 0.8] */
    if (needed > 0) {
        Lv00GappaPredicate found;
        int idx = gappa_pred_set_find(&output, "x", &found);
        TEST_ASSERT_MSG(idx >= 0, "should find x hypothesis");
        if (idx >= 0) {
            TEST_ASSERT_MSG(fabs(found.bound_lo - 0.2) < 1e-15,
                            "backward: x lo should be 0.2");
            TEST_ASSERT_MSG(fabs(found.bound_hi - 0.8) < 1e-15,
                            "backward: x hi should be 0.8");
        }
    }
}

/* ============================================================
 * Test: rewrite rules
 * ============================================================ */

static void test_gappa_rewrite_rules(void) {
    Lv00GappaRewriteRule rules[2];

    strncpy(rules[0].match_pattern, "x * 1", sizeof(rules[0].match_pattern) - 1);
    strncpy(rules[0].replace_pattern, "x", sizeof(rules[0].replace_pattern) - 1);
    strncpy(rules[0].description, "identity multiplication", sizeof(rules[0].description) - 1);

    strncpy(rules[1].match_pattern, "x + 0", sizeof(rules[1].match_pattern) - 1);
    strncpy(rules[1].replace_pattern, "x", sizeof(rules[1].replace_pattern) - 1);
    strncpy(rules[1].description, "identity addition", sizeof(rules[1].description) - 1);

    TEST_ASSERT_MSG(gappa_register_rewrite_rules(rules, 2) == true,
                    "registering rewrite rules should succeed");
}

/* ============================================================
 * Test: result cleanup
 * ============================================================ */

static void test_gappa_result_free(void) {
    Lv00GappaProofResult result;
    memset(&result, 0, sizeof(result));
    result.goals = (Lv00GappaProofGoal *)malloc(sizeof(Lv00GappaProofGoal));
    result.goals_total = 1;

    gappa_result_free(&result);

    TEST_ASSERT_MSG(result.goals == NULL, "goals should be NULL after free");
    TEST_ASSERT_EQ(result.goals_total, 0);
}

/* ============================================================
 * Main
 * ============================================================ */
int main(void) {
    TEST_SUITE_BEGIN("GappaDSL");

    /* Predefined formats */
    TEST_RUN(test_gappa_format_predefined);

    /* Parsing */
    TEST_RUN(test_gappa_parse_simple);
    TEST_RUN(test_gappa_parse_multiple);

    /* Proof engine */
    TEST_RUN(test_gappa_prove_simple);
    TEST_RUN(test_gappa_prove_bnd);
    TEST_RUN(test_gappa_prove_fail);

    /* Predicate propagation */
    TEST_RUN(test_gappa_pred_set);
    TEST_RUN(test_gappa_propagate_forward);
    TEST_RUN(test_gappa_propagate_backward);

    /* Rewrite rules */
    TEST_RUN(test_gappa_rewrite_rules);

    /* Cleanup */
    TEST_RUN(test_gappa_result_free);

    TEST_SUITE_END();
    return (g_fail_count > 0) ? 1 : 0;
}
