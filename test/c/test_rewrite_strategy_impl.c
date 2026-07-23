/**
 * @file test_rewrite_strategy_impl.c
 * @brief 重写策略组合子实现测试
 *
 * 验证 rewrite_strategy_impl.c 中实现的策略树构造、执行和销毁。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv.h"
#include "lv/rewrite.h"
#include "lv/constraint_graph.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ================================================================
 *  Group 1: 策略树构造与销毁
 * ================================================================ */

static void test_strategy_construct(void) {
    printf("  Running: test_strategy_construct ...\n");

    RewriteStrategy *idle = rewrite_strategy_create_idle();
    TEST_ASSERT_NOT_NULL(idle);
    TEST_ASSERT(idle->kind == REWRITE_STRATEGY_KIND_IDLE, "idle kind");
    rewrite_strategy_destroy(idle);

    RewriteStrategy *fail = rewrite_strategy_create_fail();
    TEST_ASSERT_NOT_NULL(fail);
    TEST_ASSERT(fail->kind == REWRITE_STRATEGY_KIND_FAIL, "fail kind");
    rewrite_strategy_destroy(fail);

    RewriteStrategy *apply = rewrite_strategy_create_apply_rule(42);
    TEST_ASSERT_NOT_NULL(apply);
    TEST_ASSERT(apply->kind == REWRITE_STRATEGY_KIND_APPLY_RULE, "apply kind");
    TEST_ASSERT(apply->rule_id == 42, "apply rule_id");
    rewrite_strategy_destroy(apply);

    RewriteStrategy *match = rewrite_strategy_create_match("test_pattern");
    TEST_ASSERT_NOT_NULL(match);
    TEST_ASSERT(match->kind == REWRITE_STRATEGY_KIND_MATCH_PATTERN, "match kind");
    TEST_ASSERT_NOT_NULL(match->pattern_expr);
    TEST_ASSERT(strcmp(match->pattern_expr, "test_pattern") == 0, "match pattern");
    rewrite_strategy_destroy(match);

    /* match 接受 NULL */
    RewriteStrategy *null_match = rewrite_strategy_create_match(NULL);
    TEST_ASSERT(null_match == NULL, "match(NULL) returns NULL");

    /* 复合策略 */
    RewriteStrategy *s1 = rewrite_strategy_create_idle();
    RewriteStrategy *s2 = rewrite_strategy_create_fail();
    RewriteStrategy *seq = rewrite_strategy_sequence(s1, s2);
    TEST_ASSERT_NOT_NULL(seq);
    TEST_ASSERT(seq->kind == REWRITE_STRATEGY_KIND_SEQUENCE, "sequence kind");
    TEST_ASSERT_NOT_NULL(seq->left);
    TEST_ASSERT_NOT_NULL(seq->right);
    rewrite_strategy_destroy(seq); /* 递归释放 s1, s2 */

    /* orelse */
    RewriteStrategy *o1 = rewrite_strategy_create_idle();
    RewriteStrategy *o2 = rewrite_strategy_create_fail();
    RewriteStrategy *orel = rewrite_strategy_orelse(o1, o2);
    TEST_ASSERT_NOT_NULL(orel);
    rewrite_strategy_destroy(orel);

    /* repeat */
    RewriteStrategy *child = rewrite_strategy_create_idle();
    RewriteStrategy *rep = rewrite_strategy_repeat(child, 5);
    TEST_ASSERT_NOT_NULL(rep);
    TEST_ASSERT(rep->kind == REWRITE_STRATEGY_KIND_REPEAT, "repeat kind");
    TEST_ASSERT(rep->max_iterations == 5, "repeat max_iter");
    rewrite_strategy_destroy(rep);

    /* normalize */
    child = rewrite_strategy_create_idle();
    RewriteStrategy *norm = rewrite_strategy_normalize(child);
    TEST_ASSERT_NOT_NULL(norm);
    TEST_ASSERT(norm->kind == REWRITE_STRATEGY_KIND_NORMALIZE, "normalize kind");
    rewrite_strategy_destroy(norm);

    /* try */
    child = rewrite_strategy_create_idle();
    RewriteStrategy *try_s = rewrite_strategy_try(child);
    TEST_ASSERT_NOT_NULL(try_s);
    TEST_ASSERT(try_s->kind == REWRITE_STRATEGY_KIND_TRY, "try kind");
    rewrite_strategy_destroy(try_s);
}

/* ================================================================
 *  Group 2: 策略执行 — idle / fail / try
 * ================================================================ */

static int g_test_ctx_val = 0;
static int test_cond_cb(void *ctx) {
    return *(int *)ctx;
}

static void test_strategy_exec_basic(void) {
    printf("  Running: test_strategy_exec_basic ...\n");

    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    /* idle: 返回副本 */
    {
        RewriteStrategy *strat = rewrite_strategy_create_idle();
        ConstraintGraph *out = NULL;
        int steps = 0;
        bool ok = rewrite_strategy_apply(g, strat, NULL, 0, &out, &steps);
        TEST_ASSERT(ok, "idle returns true");
        TEST_ASSERT_NOT_NULL(out);
        TEST_ASSERT(steps == 0, "idle steps == 0");
        graph_destroy(out);
        rewrite_strategy_destroy(strat);
    }

    /* fail: 总是失败 */
    {
        RewriteStrategy *strat = rewrite_strategy_create_fail();
        ConstraintGraph *out = NULL;
        int steps = 0;
        bool ok = rewrite_strategy_apply(g, strat, NULL, 0, &out, &steps);
        TEST_ASSERT(!ok, "fail returns false");
        TEST_ASSERT(out == NULL, "fail out == NULL");
        rewrite_strategy_destroy(strat);
    }

    /* try: 内部失败时返回原图副本 */
    {
        RewriteStrategy *inner = rewrite_strategy_create_fail();
        RewriteStrategy *strat = rewrite_strategy_try(inner);
        ConstraintGraph *out = NULL;
        int steps = 0;
        bool ok = rewrite_strategy_apply(g, strat, NULL, 0, &out, &steps);
        TEST_ASSERT(ok, "try returns true even when inner fails");
        TEST_ASSERT_NOT_NULL(out);
        TEST_ASSERT(steps == 0, "try steps == 0");
        graph_destroy(out);
        rewrite_strategy_destroy(strat); /* 递归释放 inner */
    }

    /* test: 条件为真 */
    {
        g_test_ctx_val = 1;
        RewriteStrategy *strat = rewrite_strategy_create_test(test_cond_cb, &g_test_ctx_val);
        ConstraintGraph *out = NULL;
        int steps = 0;
        bool ok = rewrite_strategy_apply(g, strat, NULL, 0, &out, &steps);
        TEST_ASSERT(ok, "test(true) returns true");
        graph_destroy(out);
        rewrite_strategy_destroy(strat);
    }

    /* test: 条件为假 */
    {
        g_test_ctx_val = 0;
        RewriteStrategy *strat = rewrite_strategy_create_test(test_cond_cb, &g_test_ctx_val);
        ConstraintGraph *out = NULL;
        int steps = 0;
        bool ok = rewrite_strategy_apply(g, strat, NULL, 0, &out, &steps);
        TEST_ASSERT(!ok, "test(false) returns false");
        rewrite_strategy_destroy(strat);
    }

    graph_destroy(g);
}

/* ================================================================
 *  Group 3: 复合策略 — sequence / orelse / repeat
 * ================================================================ */

static void test_strategy_compound(void) {
    printf("  Running: test_strategy_compound ...\n");

    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    /* sequence(idle, idle): 两次无操作 */
    {
        RewriteStrategy *s1 = rewrite_strategy_create_idle();
        RewriteStrategy *s2 = rewrite_strategy_create_idle();
        RewriteStrategy *seq = rewrite_strategy_sequence(s1, s2);

        ConstraintGraph *out = NULL;
        int steps = 0;
        bool ok = rewrite_strategy_apply(g, seq, NULL, 0, &out, &steps);
        TEST_ASSERT(ok, "sequence(idle, idle) returns true");
        TEST_ASSERT_NOT_NULL(out);
        graph_destroy(out);
        rewrite_strategy_destroy(seq);
    }

    /* sequence(idle, fail): 左成功，右失败 → 整体失败 */
    {
        RewriteStrategy *s1 = rewrite_strategy_create_idle();
        RewriteStrategy *s2 = rewrite_strategy_create_fail();
        RewriteStrategy *seq = rewrite_strategy_sequence(s1, s2);

        ConstraintGraph *out = NULL;
        int steps = 0;
        bool ok = rewrite_strategy_apply(g, seq, NULL, 0, &out, &steps);
        TEST_ASSERT(!ok, "sequence(idle, fail) returns false");
        rewrite_strategy_destroy(seq);
    }

    /* orelse(fail, idle): 左失败，右成功 */
    {
        RewriteStrategy *s1 = rewrite_strategy_create_fail();
        RewriteStrategy *s2 = rewrite_strategy_create_idle();
        RewriteStrategy *orel = rewrite_strategy_orelse(s1, s2);

        ConstraintGraph *out = NULL;
        int steps = 0;
        bool ok = rewrite_strategy_apply(g, orel, NULL, 0, &out, &steps);
        TEST_ASSERT(ok, "orelse(fail, idle) returns true");
        TEST_ASSERT_NOT_NULL(out);
        graph_destroy(out);
        rewrite_strategy_destroy(orel);
    }

    /* repeat(idle): 一次之后不动点 */
    {
        RewriteStrategy *inner = rewrite_strategy_create_idle();
        RewriteStrategy *rep = rewrite_strategy_repeat(inner, 10);

        ConstraintGraph *out = NULL;
        int steps = 0;
        bool ok = rewrite_strategy_apply(g, rep, NULL, 0, &out, &steps);
        TEST_ASSERT(ok, "repeat(idle) returns true");
        TEST_ASSERT_NOT_NULL(out);
        graph_destroy(out);
        rewrite_strategy_destroy(rep);
    }

    graph_destroy(g);
}

/* ================================================================
 *  Group 4: 逆向证明搜索（空规则集）
 * ================================================================ */

static void test_search_backward_empty(void) {
    printf("  Running: test_search_backward_empty ...\n");

    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    int *path = NULL;
    int path_len = 0;
    bool found = rewrite_search_backward(g, NULL, 0, 5, true, &path, &path_len);
    TEST_ASSERT(!found, "search with empty rules returns false");
    TEST_ASSERT(path == NULL, "path is NULL");
    TEST_ASSERT(path_len == 0, "path_len is 0");

    graph_destroy(g);
}

/* ================================================================
 *  Group 5: 数值优化规则
 * ================================================================ */

static void test_num_rules(void) {
    printf("  Running: test_num_rules ...\n");

    RewriteNumRule *rule = rewrite_num_rule_create(
        "test-rule", "a*b", "b*a", REWRITE_NUM_HIGH, 2.0);
    TEST_ASSERT_NOT_NULL(rule);
    printf("    rule created\n");

    TEST_ASSERT(strcmp(rule->name, "test-rule") == 0, "rule name");
    TEST_ASSERT(rule->priority == REWRITE_NUM_HIGH, "rule priority");
    printf("    rule fields ok\n");

    /* optimize: no match */
    RewriteNumRule *rules_arr[] = {rule};
    double impr = 0.0;
    char *result = rewrite_num_optimize("x+y", rules_arr, 1, &impr);
    TEST_ASSERT_NOT_NULL(result);
    printf("    optimize result: %s\n", result);
    TEST_ASSERT(strcmp(result, "x+y") == 0, "no match returns input");
    lv_free((void **)&result);

    rewrite_num_rule_destroy(rule);
    printf("    rule destroyed\n");

    /* register builtins */
    int count = rewrite_num_register_builtins();
    TEST_ASSERT(count == 6, "6 builtin rules registered");
    printf("    builtins: %d\n", count);
    TEST_ASSERT(rewrite_num_rule_count() == 6, "rule_count == 6");
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    lv_init();

    printf("=== Rewrite Strategy Implementation Test ===\n\n");

    TEST_RUN(test_strategy_construct);
    TEST_RUN(test_strategy_exec_basic);
    TEST_RUN(test_strategy_compound);
    TEST_RUN(test_search_backward_empty);
    TEST_RUN(test_num_rules);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n",
           g_pass_count, g_fail_count, g_pass_count + g_fail_count);
    return g_fail_count > 0 ? 1 : 0;
}
