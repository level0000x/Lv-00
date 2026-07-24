/**
 * @file test_critical_pair.c
 * @brief 关键对计算引擎测试
 *
 * 测试内容（简化版——跳过需完整 ConstraintGraph 构造的场景）：
 *   - 空规则集 / 单规则集 / 双规则集 compute_all
 *   - compare NULL 防护
 *   - compare_all 和统计
 *   - 导出 NULL 防护
 *   - destroy NULL 防护
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ---- 辅助函数：创建简单重写规则（空模式） ---- */

static RewriteRule *make_empty_rule(const char *name, int measure) {
    RewritePattern *pat = malloc(sizeof(RewritePattern));
    memset(pat, 0, sizeof(RewritePattern));
    pat->var_count = 0;
    pat->variable_node_ids = NULL;
    pat->pattern_constraints = NULL;
    pat->pattern_constraint_count = 0;

    RewriteReplacement *rep = malloc(sizeof(RewriteReplacement));
    memset(rep, 0, sizeof(RewriteReplacement));
    rep->binding_count = 0;
    rep->node_bindings = NULL;
    rep->replacement_constraints = NULL;
    rep->replacement_constraint_count = 0;
    rep->new_node_count = 0;
    rep->new_nodes = NULL;
    rep->new_node_types = NULL;

    return rewrite_rule_create(name, pat, rep, measure);
}

/* ============== 测试：空规则集 ============== */

static void test_empty_ruleset(void) {
    RewriteRule **rules = NULL;
    CriticalPairSet *cps = critical_pair_compute_all(rules, 0, NULL);
    TEST_ASSERT_NULL(cps);
    critical_pair_set_destroy(cps);
}

/* ============== 测试：单规则集 ============== */

static void test_single_rule(void) {
    RewriteRule *r1 = make_empty_rule("translate_x", 1);
    RewriteRule *rules[] = {r1};

    CriticalPairSet *cps = critical_pair_compute_all(rules, 1, NULL);
    TEST_ASSERT_NOT_NULL(cps);
    /* 空模式规则跳过，pair_count = 0 */
    TEST_ASSERT_EQ(cps->pair_count, 0);
    critical_pair_set_destroy(cps);
    rewrite_rule_destroy(r1);
}

/* ============== 测试：双规则集 ============== */

static void test_two_rules(void) {
    RewriteRule *r1 = make_empty_rule("r1", 1);
    RewriteRule *r2 = make_empty_rule("r2", 1);
    RewriteRule *rules[] = {r1, r2};

    CriticalPairSet *cps = critical_pair_compute_all(rules, 2, NULL);
    TEST_ASSERT_NOT_NULL(cps);
    TEST_ASSERT_EQ(cps->pair_count, 0);
    critical_pair_set_destroy(cps);
    rewrite_rule_destroy(r1);
    rewrite_rule_destroy(r2);
}

/* ============== 测试：compare NULL 防护 ============== */

static void test_compare_null_edge(void) {
    TEST_ASSERT_EQ(critical_pair_compare(NULL), false);
    TEST_ASSERT_EQ(critical_pair_compare_all(NULL), 0);
}

/* ============== 测试：统计 NULL 防护 ============== */

static void test_statistics(void) {
    int total = -1, confluent = -1, pending = -1;
    critical_pair_get_statistics(NULL, &total, &confluent, &pending);
    TEST_ASSERT_EQ(total, 0);
    TEST_ASSERT_EQ(confluent, 0);
    TEST_ASSERT_EQ(pending, 0);
}

/* ============== 测试：导出 NULL 防护 ============== */

static void test_export_null_edge(void) {
    TEST_ASSERT_EQ(critical_pair_export_text(NULL, "test.txt"), false);
    TEST_ASSERT_EQ(critical_pair_export_text(NULL, NULL), false);
}

/* ============== 测试：destroy NULL 防护 ============== */

static void test_destroy_null_safe(void) {
    critical_pair_set_destroy(NULL);
}

/* ============== 测试：三规则集（仍为空模式，验证遍历正确） ============== */

static void test_three_rules(void) {
    RewriteRule *r1 = make_empty_rule("a", 1);
    RewriteRule *r2 = make_empty_rule("b", 2);
    RewriteRule *r3 = make_empty_rule("c", 3);
    RewriteRule *rules[] = {r1, r2, r3};

    CriticalPairSet *cps = critical_pair_compute_all(rules, 3, NULL);
    TEST_ASSERT_NOT_NULL(cps);
    TEST_ASSERT_EQ(cps->pair_count, 0);
    critical_pair_set_destroy(cps);
    rewrite_rule_destroy(r1);
    rewrite_rule_destroy(r2);
    rewrite_rule_destroy(r3);
}

/* ============== 测试入口 ============== */

int main(void) {
    lv_init();
    TEST_SUITE_BEGIN("Critical Pair Engine");

    TEST_RUN(test_empty_ruleset);
    TEST_RUN(test_single_rule);
    TEST_RUN(test_two_rules);
    TEST_RUN(test_three_rules);
    TEST_RUN(test_compare_null_edge);
    TEST_RUN(test_statistics);
    TEST_RUN(test_export_null_edge);
    TEST_RUN(test_destroy_null_safe);

    TEST_SUITE_END();
    lv_cleanup();
    return g_fail_count > 0 ? 1 : 0;
}
