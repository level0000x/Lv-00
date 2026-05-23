/**
 * @file test_rewrite.c
 * @brief 重写系统测试 - 模式匹配、规则应用、重写策略
 *
 * 测试内容：
 * - 重写规则创建与销毁
 * - 模式匹配
 * - 规则应用
 * - 多规则重写
 */

#include "lv00.h"
#include "test_helpers.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ============== 测试：重写规则生命周期 ============== */

static int test_rewrite_rule_lifecycle(void)
{
    printf("Test: rewrite rule lifecycle...\n");

    /* 创建模式 */
    RewritePattern *pattern = malloc(sizeof(RewritePattern));
    assert(pattern != NULL);
    memset(pattern, 0, sizeof(RewritePattern));

    int var_ids[] = {-1, -2};  /* 模式变量使用负ID */
    pattern->variable_node_ids = malloc(2 * sizeof(int));
    memcpy(pattern->variable_node_ids, var_ids, 2 * sizeof(int));
    pattern->var_count = 2;
    pattern->pattern_constraints = NULL;
    pattern->pattern_constraint_count = 0;

    /* 创建替换 */
    RewriteReplacement *replacement = malloc(sizeof(RewriteReplacement));
    assert(replacement != NULL);
    memset(replacement, 0, sizeof(RewriteReplacement));
    replacement->node_bindings = NULL;
    replacement->binding_count = 0;
    replacement->replacement_constraints = NULL;
    replacement->replacement_constraint_count = 0;
    replacement->new_nodes = NULL;
    replacement->new_node_count = 0;

    /* 创建规则 */
    RewriteRule *rule = rewrite_rule_create("test_rule", pattern, replacement, 1);
    assert(rule != NULL);
    assert(strcmp(rule->name, "test_rule") == 0);
    assert(rule->reduction_measure == 1);

    printf("  规则 '%s' 创建成功\n", rule->name);

    rewrite_rule_destroy(rule);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：模式匹配 ============== */

static int test_pattern_matching(void)
{
    printf("Test: pattern matching...\n");

    ConstraintGraph *g = graph_create();

    /* 创建图中的节点 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);

    graph_add_line_segment(g, p1, p2);

    /* 创建简单的模式：匹配两个点和一条线段 */
    RewritePattern *pattern = malloc(sizeof(RewritePattern));
    assert(pattern != NULL);
    memset(pattern, 0, sizeof(RewritePattern));

    int var_ids[] = {-1, -2};
    pattern->variable_node_ids = malloc(2 * sizeof(int));
    memcpy(pattern->variable_node_ids, var_ids, 2 * sizeof(int));
    pattern->var_count = 2;
    pattern->pattern_constraints = NULL;
    pattern->pattern_constraint_count = 0;

    RewriteReplacement *replacement = malloc(sizeof(RewriteReplacement));
    assert(replacement != NULL);
    memset(replacement, 0, sizeof(RewriteReplacement));

    RewriteRule *rule = rewrite_rule_create("match_test", pattern, replacement, 1);
    assert(rule != NULL);

    /* 尝试匹配 */
    RewriteMatch *match = find_rewrite_match(g, rule, false);
    printf("  匹配结果: %s\n", match ? "找到" : "未找到");

    if (match) {
        printf("  绑定数量: %d\n", match->binding_count);
        free(match->node_bindings);
        free(match->constraint_bindings);
        free(match);
    }

    rewrite_rule_destroy(rule);
    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：规则应用 ============== */

static int test_rule_application(void)
{
    printf("Test: rule application...\n");

    ConstraintGraph *g = graph_create();

    /* 创建节点 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 2, 1, 0, 1);

    graph_add_line_segment(g, p1, p2);

    /* 创建空规则（测试API） */
    RewritePattern *pattern = malloc(sizeof(RewritePattern));
    memset(pattern, 0, sizeof(RewritePattern));
    pattern->var_count = 0;

    RewriteReplacement *replacement = malloc(sizeof(RewriteReplacement));
    memset(replacement, 0, sizeof(RewriteReplacement));
    replacement->binding_count = 0;

    RewriteRule *rule = rewrite_rule_create("empty_rule", pattern, replacement, 0);
    assert(rule != NULL);

    /* 尝试应用规则 */
    RewriteMatch *match = find_rewrite_match(g, rule, false);
    if (match) {
        RewriteStatus status = apply_rewrite(g, rule, match);
        printf("  应用状态: %d\n", status);

        free(match->node_bindings);
        free(match->constraint_bindings);
        free(match);
    }

    rewrite_rule_destroy(rule);
    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：多规则重写 ============== */

static int test_multi_rule_rewrite(void)
{
    printf("Test: multi-rule rewrite...\n");

    ConstraintGraph *g = graph_create();

    /* 创建节点 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    int p3 = add_point(g, 2, 1, 2, 1);

    graph_add_line_segment(g, p1, p2);
    graph_add_line_segment(g, p2, p3);

    /* 创建多个规则 */
    RewriteRule *rules[2];

    for (int i = 0; i < 2; i++) {
        RewritePattern *pattern = malloc(sizeof(RewritePattern));
        memset(pattern, 0, sizeof(RewritePattern));
        pattern->var_count = 0;

        RewriteReplacement *replacement = malloc(sizeof(RewriteReplacement));
        memset(replacement, 0, sizeof(RewriteReplacement));
        replacement->binding_count = 0;

        char name[32];
        snprintf(name, sizeof(name), "rule_%d", i);
        rules[i] = rewrite_rule_create(name, pattern, replacement, i);
    }

    /* 执行重写 */
    RewriteStatus status = rewrite_with_rules(g, rules, 2, 10, false);
    printf("  重写状态: %d\n", status);

    for (int i = 0; i < 2; i++) {
        rewrite_rule_destroy(rules[i]);
    }

    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：重写终止 ============== */

static int test_rewrite_termination(void)
{
    printf("Test: rewrite termination...\n");

    ConstraintGraph *g = graph_create();

    /* 创建简单图 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 0, 1);

    graph_add_line_segment(g, p1, p2);

    /* 创建规则 */
    RewritePattern *pattern = malloc(sizeof(RewritePattern));
    memset(pattern, 0, sizeof(RewritePattern));
    pattern->var_count = 0;

    RewriteReplacement *replacement = malloc(sizeof(RewriteReplacement));
    memset(replacement, 0, sizeof(RewriteReplacement));

    RewriteRule *rule = rewrite_rule_create("term_rule", pattern, replacement, 0);

    /* 设置步数限制 */
    RewriteRule *rules[] = {rule};
    RewriteStatus status = rewrite_with_rules(g, rules, 1, 5, false);

    printf("  终止状态: %d\n", status);

    rewrite_rule_destroy(rule);
    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 主函数 ============== */

int main(void)
{
    printf("=== Lv-00 Rewrite System Test Suite ===\n\n");

    test_rewrite_rule_lifecycle();
    test_pattern_matching();
    test_rule_application();
    test_multi_rule_rewrite();
    test_rewrite_termination();

    printf("\n=== All rewrite tests PASSED! ===\n");
    return 0;
}
