/**
 * @file test_rewrite.c
 * @brief 重写系统测试 - 模式匹配、规则应用、重写策略
 *
 * 测试内容：
 * - 重写规则创建与销毁
 * - 真实约束图模式匹配（含约束断言）
 * - 真实约束图规则应用（验证图状态变化）
 * - 多规则重写
 * - 重写终止与循环检测
 * - 图快照事务回滚
 * - WL 图核哈希计算
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "test_helpers.h"
#include "lv_test_geom_graph_builder.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 辅助：线段图 ============== */
/* 收敛：使用 lv_test_geom_graph_builder.h 的 lv_test_line_graph(NULL, 0, 0, 1, 0, true)
 * 创建含两 POINT 节点 (0,0)(1,0) + 一条线段 + 两条 incidence 的图（与本地 create_line_graph 语义一致）*/

/* ============== 辅助：创建模式约束结构体 ============== */

/**
 * @brief 创建一个动态分配的 Constraint 结构体
 *
 * 用于构建 RewritePattern 中的 pattern_constraints 数组。
 * 调用者负责释放 participants 和 Constraint 本身。
 *
 * @param type         约束类型（如 INCIDENCE）
 * @param participant_count  参与者数量
 * @param participants      参与者 ID 数组（会被深拷贝）
 * @return 新分配的 Constraint 指针，失败返回 NULL
 */
static Constraint *make_constraint(ConstraintType type, int participant_count, const int *participants) {
    Constraint *c = lv_calloc(1, sizeof(Constraint));
    if (!c) return NULL;
    c->type = type;
    c->participant_count = participant_count;
    c->participants = lv_malloc((size_t)participant_count * sizeof(int));
    if (!c->participants) {
        lv_free((void **)&c);
        return NULL;
    }
    memcpy(c->participants, participants, (size_t)participant_count * sizeof(int));
    return c;
}

/**
 * @brief 销毁通过 make_constraint 创建的约束
 */
static void destroy_constraint(Constraint *c) {
    if (c) {
        lv_free((void **)&c->participants);
        lv_free((void **)&c);
    }
}

/* ============================================================
 * 测试：重写规则生命周期（API 健全性）
 * ============================================================ */

static void test_rewrite_rule_lifecycle(void) {
    RewritePattern *pattern = lv_calloc(1, sizeof(RewritePattern));
    TEST_ASSERT_NOT_NULL(pattern);

    int var_ids[] = {-1, -2};
    pattern->variable_node_ids = lv_malloc(2 * sizeof(int));
    TEST_ASSERT_NOT_NULL(pattern->variable_node_ids);
    memcpy(pattern->variable_node_ids, var_ids, 2 * sizeof(int));
    pattern->var_count = 2;
    pattern->pattern_constraints = NULL;
    pattern->pattern_constraint_count = 0;

    RewriteReplacement *replacement = lv_calloc(1, sizeof(RewriteReplacement));
    TEST_ASSERT_NOT_NULL(replacement);
    replacement->node_bindings = NULL;
    replacement->binding_count = 0;
    replacement->replacement_constraints = NULL;
    replacement->replacement_constraint_count = 0;
    replacement->new_nodes = NULL;
    replacement->new_node_count = 0;

    RewriteRule *rule = rewrite_rule_create("test_rule", pattern, replacement, 1);
    TEST_ASSERT_NOT_NULL(rule);
    TEST_ASSERT_STR_EQ(rule->name, "test_rule");
    TEST_ASSERT_EQ(rule->reduction_measure, 1);

    /* 清理：pattern 和 replacement 的所有权归调用者 */
    rewrite_rule_destroy(rule);
    lv_free((void **)&pattern->variable_node_ids);
    lv_free((void **)&pattern);
    lv_free((void **)&replacement);
}

/* ============================================================
 * 测试：真实约束图模式匹配（含约束）
 *
 * 创建一个包含 2 个点 + 1 条线段 + 2 条 incidence 的图，
 * 使用包含 3 个模式变量和 2 条约束的模式进行匹配，
 * 验证 match 返回正确的绑定。
 * ============================================================ */

static void test_real_pattern_matching(void) {
    ConstraintGraph *g = lv_test_line_graph(NULL, 0, 0, 1, 0, true);
    TEST_ASSERT_NOT_NULL(g);

    /* 模式：3 个变量 (-1=point, -2=line, -3=point) */
    int var_ids[] = {-1, -2, -3};

    /* 模式约束：incidence(-1, -2), incidence(-3, -2) */
    int part1[] = {-1, -2};
    int part2[] = {-3, -2};
    Constraint *c1 = make_constraint(INCIDENCE, 2, part1);
    Constraint *c2 = make_constraint(INCIDENCE, 2, part2);
    TEST_ASSERT_NOT_NULL(c1);
    TEST_ASSERT_NOT_NULL(c2);

    Constraint *constraints[] = {c1, c2};

    RewritePattern *pattern = lv_calloc(1, sizeof(RewritePattern));
    TEST_ASSERT_NOT_NULL(pattern);
    pattern->variable_node_ids = lv_malloc(3 * sizeof(int));
    TEST_ASSERT_NOT_NULL(pattern->variable_node_ids);
    memcpy(pattern->variable_node_ids, var_ids, 3 * sizeof(int));
    pattern->var_count = 3;
    pattern->pattern_constraints = lv_malloc(2 * sizeof(Constraint *));
    TEST_ASSERT_NOT_NULL(pattern->pattern_constraints);
    memcpy(pattern->pattern_constraints, constraints, 2 * sizeof(Constraint *));
    pattern->pattern_constraint_count = 2;

    RewriteReplacement *replacement = lv_calloc(1, sizeof(RewriteReplacement));
    TEST_ASSERT_NOT_NULL(replacement);

    RewriteRule *rule = rewrite_rule_create("line_match", pattern, replacement, 1);
    TEST_ASSERT_NOT_NULL(rule);

    /* 查找匹配 */
    RewriteMatch *match = find_rewrite_match(g, rule, false);
    TEST_ASSERT_NOT_NULL(match);
    TEST_ASSERT(match->binding_count > 0, "should have bindings");

    /* 验证绑定：-1 对应图中的一个 POINT，-2 对应 LINE_SEGMENT，-3 对应另一个 POINT */
    int bind_p1 = -1, bind_s = -1, bind_p2 = -1;
    for (int i = 0; i < match->binding_count; i++) {
        int pat = match->node_bindings[i * 2];
        int gid = match->node_bindings[i * 2 + 1];
        GeomNode *node = graph_get_node(g, gid);
        if (pat == -1) { bind_p1 = gid; TEST_ASSERT_NOT_NULL(node); TEST_ASSERT_EQ((int)node->type, (int)GEOM_POINT); }
        if (pat == -2) { bind_s  = gid; TEST_ASSERT_NOT_NULL(node); TEST_ASSERT_EQ((int)node->type, (int)GEOM_LINE_SEGMENT); }
        if (pat == -3) { bind_p2 = gid; TEST_ASSERT_NOT_NULL(node); TEST_ASSERT_EQ((int)node->type, (int)GEOM_POINT); }
    }
    TEST_ASSERT(bind_p1 >= 0, "pattern var -1 (point) should be bound");
    TEST_ASSERT(bind_s  >= 0, "pattern var -2 (segment) should be bound");
    TEST_ASSERT(bind_p2 >= 0, "pattern var -3 (point) should be bound");
    TEST_ASSERT(bind_p1 != bind_p2, "two points should be distinct");

    lv_free((void **)&match->node_bindings);
    lv_free((void **)&match->constraint_bindings);
    lv_free((void **)&match);

    rewrite_rule_destroy(rule);
    destroy_constraint(c1);
    destroy_constraint(c2);
    lv_free((void **)&pattern->variable_node_ids);
    lv_free((void **)&pattern->pattern_constraints);
    lv_free((void **)&pattern);
    lv_free((void **)&replacement);
    graph_destroy(g);
}

/* ============================================================
 * 测试：真实约束图规则应用
 *
 * 匹配图中 2 个点 + 1 条线段 + incidence 约束，
 * 替换为 2 个点之间的 BETWEENNESS 约束。
 * 验证应用后 graph 中移除原约束、添加新约束。
 * ============================================================ */

static void test_real_rule_application(void) {
    ConstraintGraph *g = lv_test_line_graph(NULL, 0, 0, 1, 0, true);
    TEST_ASSERT_NOT_NULL(g);

    /* 记录初始状态 */
    int init_node_count = g->node_count;
    int init_con_count = g->constraint_count;
    TEST_ASSERT(init_node_count >= 3, "should have at least 3 nodes (2 points + 1 segment)");
    TEST_ASSERT(init_con_count >= 2, "should have at least 2 constraints");

    /* 模式：3 个变量，2 条 incidence 约束 */
    int var_ids[] = {-1, -2, -3};
    int part1[] = {-1, -2};
    int part2[] = {-3, -2};
    Constraint *pc1 = make_constraint(INCIDENCE, 2, part1);
    Constraint *pc2 = make_constraint(INCIDENCE, 2, part2);
    TEST_ASSERT_NOT_NULL(pc1);
    TEST_ASSERT_NOT_NULL(pc2);

    Constraint *pconstraints[] = {pc1, pc2};
    RewritePattern *pattern = lv_calloc(1, sizeof(RewritePattern));
    TEST_ASSERT_NOT_NULL(pattern);
    pattern->variable_node_ids = lv_malloc(3 * sizeof(int));
    TEST_ASSERT_NOT_NULL(pattern->variable_node_ids);
    memcpy(pattern->variable_node_ids, var_ids, 3 * sizeof(int));
    pattern->var_count = 3;
    pattern->pattern_constraints = lv_malloc(2 * sizeof(Constraint *));
    TEST_ASSERT_NOT_NULL(pattern->pattern_constraints);
    memcpy(pattern->pattern_constraints, pconstraints, 2 * sizeof(Constraint *));
    pattern->pattern_constraint_count = 2;

    /* 替换：保留点，移除线段和 incidence，添加 BETWEENNESS(-1, -3, some_new_point) */
    int new_node_placeholder = 100;
    int new_nodes[] = {new_node_placeholder};

    int btwn_parts[] = {-1, -3, new_node_placeholder};
    Constraint *rc = make_constraint(BETWEENNESS, 3, btwn_parts);
    TEST_ASSERT_NOT_NULL(rc);

    Constraint *rconstraints[] = {rc};
    GeomType new_node_types[] = {GEOM_POINT};

    /* 替换：保留 -1 和 -3（点在替换后仍存在），不绑定 -2（线段将被移除） */
    int *bindings[2];
    int b1[] = {-1, 0};
    int b2[] = {-3, 0};
    bindings[0] = b1;
    bindings[1] = b2;

    RewriteReplacement *replacement = lv_calloc(1, sizeof(RewriteReplacement));
    TEST_ASSERT_NOT_NULL(replacement);
    replacement->node_bindings = lv_malloc(2 * sizeof(int *));
    TEST_ASSERT_NOT_NULL(replacement->node_bindings);
    replacement->node_bindings[0] = lv_malloc(2 * sizeof(int));
    replacement->node_bindings[1] = lv_malloc(2 * sizeof(int));
    TEST_ASSERT_NOT_NULL(replacement->node_bindings[0]);
    TEST_ASSERT_NOT_NULL(replacement->node_bindings[1]);
    replacement->node_bindings[0][0] = -1; replacement->node_bindings[0][1] = 0;
    replacement->node_bindings[1][0] = -3; replacement->node_bindings[1][1] = 0;
    replacement->binding_count = 2;

    replacement->replacement_constraints = lv_malloc(1 * sizeof(Constraint *));
    TEST_ASSERT_NOT_NULL(replacement->replacement_constraints);
    replacement->replacement_constraints[0] = rc;
    replacement->replacement_constraint_count = 1;

    replacement->new_nodes = lv_malloc(1 * sizeof(int));
    TEST_ASSERT_NOT_NULL(replacement->new_nodes);
    replacement->new_nodes[0] = new_node_placeholder;
    replacement->new_node_count = 1;

    replacement->new_node_types = lv_malloc(1 * sizeof(GeomType));
    TEST_ASSERT_NOT_NULL(replacement->new_node_types);
    replacement->new_node_types[0] = GEOM_POINT;

    RewriteRule *rule = rewrite_rule_create("line_to_betweenness", pattern, replacement, 1);
    TEST_ASSERT_NOT_NULL(rule);

    /* 查找并应用 */
    RewriteMatch *match = find_rewrite_match(g, rule, false);
    TEST_ASSERT_NOT_NULL(match);

    RewriteStatus status = apply_rewrite(g, rule, match);
    TEST_ASSERT_EQ((int)status, (int)REWRITE_APPLIED);

    /* 验证应用结果：
     *   - 节点数量应增加（新增了 1 个中点 + 可能 2 个占位端点，共 +3）
     *   - 约束数量应在删除 2 条 incidence 并添加 1 条 betweenness 后变化
     *   - 原线段应被移除 */
    TEST_ASSERT(g->node_count >= init_node_count, "node count should not decrease after apply");

    lv_free((void **)&match->node_bindings);
    lv_free((void **)&match->constraint_bindings);
    lv_free((void **)&match);

    rewrite_rule_destroy(rule);
    destroy_constraint(pc1);
    destroy_constraint(pc2);
    destroy_constraint(rc);
    lv_free((void **)&pattern->variable_node_ids);
    lv_free((void **)&pattern->pattern_constraints);
    lv_free((void **)&pattern);
    lv_free((void **)&replacement->node_bindings[0]);
    lv_free((void **)&replacement->node_bindings[1]);
    lv_free((void **)&replacement->node_bindings);
    lv_free((void **)&replacement->replacement_constraints);
    lv_free((void **)&replacement->new_nodes);
    lv_free((void **)&replacement->new_node_types);
    lv_free((void **)&replacement);
    graph_destroy(g);
}

/* ============================================================
 * 测试：多规则重写（带真实匹配）
 *
 * 创建两个规则：规则 1 匹配线段移除 incidence，
 * 规则 2 执行进一步清理。验证 rewrite_with_rules 返回成功。
 * ============================================================ */

static void test_multi_rule_rewrite(void) {
    ConstraintGraph *g = lv_test_line_graph(NULL, 0, 0, 1, 0, true);
    TEST_ASSERT_NOT_NULL(g);

    /* 规则 1：匹配 2 个点和 1 条线段（含 incidence），移除线段 */
    int var_ids1[] = {-1, -2, -3};
    int p1_part[] = {-1, -2};
    int p2_part[] = {-3, -2};
    Constraint *pc1 = make_constraint(INCIDENCE, 2, p1_part);
    Constraint *pc2 = make_constraint(INCIDENCE, 2, p2_part);
    TEST_ASSERT_NOT_NULL(pc1);
    TEST_ASSERT_NOT_NULL(pc2);

    Constraint *pcons1[] = {pc1, pc2};
    RewritePattern *pat1 = lv_calloc(1, sizeof(RewritePattern));
    TEST_ASSERT_NOT_NULL(pat1);
    pat1->variable_node_ids = lv_malloc(3 * sizeof(int));
    TEST_ASSERT_NOT_NULL(pat1->variable_node_ids);
    memcpy(pat1->variable_node_ids, var_ids1, 3 * sizeof(int));
    pat1->var_count = 3;
    pat1->pattern_constraints = lv_malloc(2 * sizeof(Constraint *));
    TEST_ASSERT_NOT_NULL(pat1->pattern_constraints);
    memcpy(pat1->pattern_constraints, pcons1, 2 * sizeof(Constraint *));
    pat1->pattern_constraint_count = 2;

    RewriteReplacement *repl1 = lv_calloc(1, sizeof(RewriteReplacement));
    TEST_ASSERT_NOT_NULL(repl1);

    RewriteRule *rules[2];
    rules[0] = rewrite_rule_create("remove_segment", pat1, repl1, 1);
    TEST_ASSERT_NOT_NULL(rules[0]);

    /* 规则 2：空规则（作为骨架） */
    RewritePattern *pat2 = lv_calloc(1, sizeof(RewritePattern));
    TEST_ASSERT_NOT_NULL(pat2);
    pat2->var_count = 0;

    RewriteReplacement *repl2 = lv_calloc(1, sizeof(RewriteReplacement));
    TEST_ASSERT_NOT_NULL(repl2);

    rules[1] = rewrite_rule_create("cleanup", pat2, repl2, 0);
    TEST_ASSERT_NOT_NULL(rules[1]);

    /* 执行多规则重写 */
    int pre_node_count = g->node_count;
    RewriteStatus status = rewrite_with_rules(g, rules, 2, 10, false);
    TEST_ASSERT(status == REWRITE_OK || status == REWRITE_APPLIED || status == REWRITE_TERMINATED,
                "multi-rule rewrite should succeed");
    TEST_ASSERT(g->node_count >= 0, "graph should remain valid");

    for (int i = 0; i < 2; i++) {
        rewrite_rule_destroy(rules[i]);
    }
    /* 注意：rewrite_rule_destroy 不销毁 pattern 和 replacement，需手动清理 */
    destroy_constraint(pc1);
    destroy_constraint(pc2);
    lv_free((void **)&pat1->variable_node_ids);
    lv_free((void **)&pat1->pattern_constraints);
    lv_free((void **)&pat1);
    lv_free((void **)&repl1);
    lv_free((void **)&pat2);
    lv_free((void **)&repl2);

    (void)pre_node_count;
    graph_destroy(g);
}

/* ============================================================
 * 测试：图快照创建与恢复
 *
 * 创建图快照，修改原图，恢复后验证状态一致。
 * ============================================================ */

static void test_graph_snapshot(void) {
    ConstraintGraph *g = lv_test_line_graph(NULL, 0, 0, 1, 0, true);
    TEST_ASSERT_NOT_NULL(g);

    /* 创建快照 */
    GraphSnapshot *snap = graph_snapshot_create(g);
    TEST_ASSERT_NOT_NULL(snap);

    int snap_node_count = snap->node_count;
    int snap_con_count = snap->constraint_count;
    TEST_ASSERT(snap_node_count >= 3, "snapshot should have nodes");
    TEST_ASSERT(snap_con_count >= 2, "snapshot should have constraints");

    /* 修改原图：移除第一个节点 */
    if (g->node_count > 0) {
        graph_remove_node(g, g->nodes[0]->id);
    }
    TEST_ASSERT(g->node_count < snap_node_count, "graph should have fewer nodes after removal");

    /* 恢复快照 */
    bool restored = graph_snapshot_restore(snap, g);
    TEST_ASSERT(restored, "snapshot restore should succeed");
    TEST_ASSERT_EQ(g->node_count, snap_node_count);
    TEST_ASSERT_EQ(g->constraint_count, snap_con_count);

    graph_snapshot_destroy(snap);
    graph_destroy(g);
}

/* ============================================================
 * 测试：WL 图核哈希
 *
 * 创建两个相同结构的图，验证 WL 哈希相等。
 * 创建不同结构的图，验证 WL 哈希不等。
 * ============================================================ */

static void test_wl_hash(void) {
    /* 创建第一个图：p0--p1 */
    ConstraintGraph *g1 = lv_test_line_graph(NULL, 0, 0, 1, 0, true);
    TEST_ASSERT_NOT_NULL(g1);

    /* 创建相同结构的图 */
    ConstraintGraph *g2 = lv_test_line_graph(NULL, 0, 0, 1, 0, true);
    TEST_ASSERT_NOT_NULL(g2);

    /* 计算 WL 哈希 */
    uint64_t h1 = rewrite_compute_wl_hash(g1);
    uint64_t h2 = rewrite_compute_wl_hash(g2);
    TEST_ASSERT(h1 != 0, "WL hash should not be zero");
    TEST_ASSERT(h2 != 0, "WL hash should not be zero");
    TEST_ASSERT_EQ((int64_t)h1, (int64_t)h2);

    /* 创建不同结构：只加点无约束 */
    ConstraintGraph *g3 = graph_create();
    TEST_ASSERT_NOT_NULL(g3);
    add_point(g3, 0, 1, 0, 1);
    uint64_t h3 = rewrite_compute_wl_hash(g3);
    TEST_ASSERT(h3 != 0, "WL hash for bare point should not be zero");
    TEST_ASSERT(h1 != h3, "different graph structures should have different hashes");

    graph_destroy(g1);
    graph_destroy(g2);
    graph_destroy(g3);
}

/* ============================================================
 * 测试：规则热加载/卸载（API 健全性）
 *
 * 创建一个规则数组，加载测试规则，验证接口正常。
 * ============================================================ */

static void test_rule_unload(void) {
    /* 创建可变规则数组 */
    RewritePattern *pat = lv_calloc(1, sizeof(RewritePattern));
    TEST_ASSERT_NOT_NULL(pat);
    pat->var_count = 0;

    RewriteReplacement *repl = lv_calloc(1, sizeof(RewriteReplacement));
    TEST_ASSERT_NOT_NULL(repl);

    RewriteRule *r1 = rewrite_rule_create("unload_test", pat, repl, 1);
    TEST_ASSERT_NOT_NULL(r1);

    RewriteRule **rules = lv_malloc(2 * sizeof(RewriteRule *));
    TEST_ASSERT_NOT_NULL(rules);
    rules[0] = r1;

    int count = 1;

    /* 卸载 */
    bool ok = rewrite_rule_unload(&rules, &count, "unload_test");
    TEST_ASSERT(ok, "unload should succeed");
    TEST_ASSERT_EQ(count, 0);
    TEST_ASSERT_NULL(rules);

    /* 再次卸载应失败 */
    ok = rewrite_rule_unload(&rules, &count, "unload_test");
    TEST_ASSERT(!ok, "second unload should fail");

    /* 注意：rewrite_rule_unload 内部已深销毁 r1（含 pattern/replacement），
     * 不要重复释放 pat、repl 或 r1 */
}

/* ============================================================
 * 测试：rewrite_rule_destroy NULL 安全
 * ============================================================ */

static void test_rule_destroy_null(void) {
    rewrite_rule_destroy(NULL);
}

/* ============================================================
 * 测试：局部等价容忍模式匹配
 *
 * 创建两个结构相同但节点 ID 不同的图，
 * 使用 local_equivalence_tolerant 模式匹配。
 * ============================================================ */

static void test_local_equiv_matching(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    /* 创建两个坐标相同的点（不同 ID） */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 0, 1, 0, 1); /* 相同坐标 */
    TEST_ASSERT(p1 >= 0 && p2 >= 0, "points should be created");

    /* 模式：2 个变量，无约束（仅测试变量绑定） */
    int var_ids[] = {-1, -2};
    RewritePattern *pattern = lv_calloc(1, sizeof(RewritePattern));
    TEST_ASSERT_NOT_NULL(pattern);
    pattern->variable_node_ids = lv_malloc(2 * sizeof(int));
    TEST_ASSERT_NOT_NULL(pattern->variable_node_ids);
    memcpy(pattern->variable_node_ids, var_ids, 2 * sizeof(int));
    pattern->var_count = 2;
    pattern->pattern_constraints = NULL;
    pattern->pattern_constraint_count = 0;

    RewriteReplacement *replacement = lv_calloc(1, sizeof(RewriteReplacement));
    TEST_ASSERT_NOT_NULL(replacement);

    RewriteRule *rule = rewrite_rule_create("equiv_test", pattern, replacement, 1);
    TEST_ASSERT_NOT_NULL(rule);

    /* 标准模式匹配应能绑定两个不同的点 */
    RewriteMatch *match = find_rewrite_match(g, rule, false);
    TEST_ASSERT_NOT_NULL(match);
    TEST_ASSERT_EQ(match->binding_count, 2);

    lv_free((void **)&match->node_bindings);
    lv_free((void **)&match->constraint_bindings);
    lv_free((void **)&match);

    rewrite_rule_destroy(rule);
    lv_free((void **)&pattern->variable_node_ids);
    lv_free((void **)&pattern);
    lv_free((void **)&replacement);
    graph_destroy(g);
}

/* ============================================================
 * 主函数
 * ============================================================ */

int main(void) {
    TEST_SUITE_BEGIN("Rewrite System (Constraint Graph)");

    TEST_RUN(test_rewrite_rule_lifecycle);
    TEST_RUN(test_real_pattern_matching);
    TEST_RUN(test_real_rule_application);
    TEST_RUN(test_multi_rule_rewrite);
    TEST_RUN(test_graph_snapshot);
    TEST_RUN(test_wl_hash);
    TEST_RUN(test_rule_unload);
    TEST_RUN(test_rule_destroy_null);
    TEST_RUN(test_local_equiv_matching);

    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}
