/**
 * @file test_constraint_graph_ext.c
 * @brief 约束图扩展契约测试（批次 C-㊵：constraint_graph.h 14 个零覆盖 API）
 *
 * 覆盖 14 个 ctest 零覆盖 API：
 *   - 约束族：graph_add_angle / graph_remove_constraint / graph_deactivate_
 *     constraint / graph_detect_redundancy
 *   - 索引族：graph_find_constraints_involving / graph_node_index_insert /
 *     graph_constraint_index_insert / graph_index_rebuild / graph_mark_dirty /
 *     graph_sync_nodes
 *   - 工具族：graph_find_app_sink_input / graph_set_error / graph_set_stream_
 *     context / graph_export_dot_to_svg（NULL 契约；正路径依赖外部 graphviz）
 *
 * 契约要点（与实现核对）：
 *   - graph_add_line_segment / graph_add_port 返回 AddNodeResult 成功码
 *     （ADD_NODE_OK=0）而非节点 id；节点 id 用 graph_get_last_added_node_id。
 *   - graph_remove_constraint 按约束数组下标移除（非 id）。
 *   - graph_detect_redundancy 返回 1 冗余 / 0 不冗余 / -1 参数无效。
 *   - graph_deactivate_constraint 按约束 id 惰性删除（is_active=false）。
 *   - graph_export_dot_to_svg 经 system() 调外部 layout 工具，仅测 NULL 契约。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* 构造图：3 点 + 2 条线段（共享端点）+ 一条 INCIDENCE 约束 */
static ConstraintGraph *make_graph(int with_constraint, int *out_p, int *out_seg1, int *out_seg2) {
    ConstraintGraph *g = graph_create();
    if (!g)
        return NULL;
    int p0 = add_point(g, 0, 1, 0, 1);
    int p1 = add_point(g, 1, 1, 0, 1);
    int p2 = add_point(g, 0, 1, 1, 1);
    if (graph_add_line_segment(g, p0, p1) != ADD_NODE_OK) {
        graph_destroy(g);
        return NULL;
    }
    int s1 = graph_get_last_added_node_id(g);
    if (graph_add_line_segment(g, p0, p2) != ADD_NODE_OK) {
        graph_destroy(g);
        return NULL;
    }
    int s2 = graph_get_last_added_node_id(g);
    if (out_p)
        *out_p = p0;
    if (out_seg1)
        *out_seg1 = s1;
    if (out_seg2)
        *out_seg2 = s2;
    if (with_constraint) {
        if (graph_add_incidence(g, p0, s1) < 0) {
            graph_destroy(g);
            return NULL;
        }
    }
    return g;
}

/* ============== 测试：角度约束 ============== */

static void test_angle_constraint_api(void) {
    ConstraintGraph *g = make_graph(0, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(g);

    /* NULL 契约：节点不存在 / 非线段参与者 */
    TEST_ASSERT_EQ(graph_add_angle(g, 999, 999, 45.0), ADD_CONSTRAINT_CONFLICT);
    TEST_ASSERT_EQ(graph_add_angle(g, 0, 999, 45.0), ADD_CONSTRAINT_CONFLICT);

    /* 正路径：两条线段 + 角度值 */
    int s1 = -1, s2 = -1;
    graph_destroy(g);
    g = make_graph(0, NULL, &s1, &s2);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT(s1 >= 0 && s2 >= 0 && s1 != s2, "两条线段节点");
    TEST_ASSERT_EQ(graph_add_angle(g, s1, s2, 30.0), ADD_CONSTRAINT_OK);
    TEST_ASSERT_EQ(g->constraint_count, 1);
    TEST_ASSERT(g->dirty, "添加约束标记脏");
    Constraint *con = g->constraints[0];
    TEST_ASSERT_NOT_NULL(con);
    TEST_ASSERT_EQ(con->type, ANGLE);
    TEST_ASSERT_EQ(con->participant_count, 2);
    TEST_ASSERT_EQ(con->numeric_value, 30.0);
    TEST_ASSERT_EQ(con->participants[0], s1);
    TEST_ASSERT_EQ(con->participants[1], s2);

    graph_destroy(g);
    printf("  test_angle_constraint_api: PASSED\n");
}

/* ============== 测试：移除 / 废弃约束 ============== */

static void test_remove_deactivate_api(void) {
    /* graph_remove_constraint：越界 → NOT_FOUND */
    ConstraintGraph *g = make_graph(0, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(graph_remove_constraint(g, 0), REMOVE_CONSTRAINT_NOT_FOUND);

    /* 正路径：添加约束后移除 */
    int p = -1, s1 = -1;
    graph_destroy(g);
    g = make_graph(1, &p, &s1, NULL);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(g->constraint_count, 1);
    int cid = g->constraints[0]->id;
    TEST_ASSERT_EQ(graph_remove_constraint(g, 0), REMOVE_CONSTRAINT_OK);
    TEST_ASSERT_EQ(g->constraint_count, 0);
    TEST_ASSERT(g->dirty, "移除标记脏");
    TEST_ASSERT_NULL(graph_get_constraint(g, cid));

    /* graph_deactivate_constraint：NULL / 未找到 / 已不活跃 / 正常 */
    TEST_ASSERT_EQ(graph_deactivate_constraint(NULL, 1), lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_EQ(graph_deactivate_constraint(g, 999), lv_ERROR_NOT_FOUND);

    graph_destroy(g);
    g = make_graph(1, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(g);
    cid = g->constraints[0]->id;
    TEST_ASSERT_EQ(graph_deactivate_constraint(g, cid), lv_OK);
    TEST_ASSERT(!g->constraints[0]->is_active, "约束已废弃");
    TEST_ASSERT(g->dirty, "废弃标记脏");
    TEST_ASSERT_EQ(graph_deactivate_constraint(g, cid), lv_ERROR_UNKNOWN);

    graph_destroy(g);
    printf("  test_remove_deactivate_api: PASSED\n");
}

/* ============== 测试：涉及约束查询 ============== */

static void test_involving_index_api(void) {
    /* NULL 契约 */
    ConstraintGraph *g = make_graph(0, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(g);
    int out[8];
    TEST_ASSERT_EQ(graph_find_constraints_involving(NULL, 0, out, 8), 0);
    TEST_ASSERT_EQ(graph_find_constraints_involving(g, 0, NULL, 8), 0);
    TEST_ASSERT_EQ(graph_find_constraints_involving(g, 0, out, 0), 0);

    /* 正路径：INCIDENCE 约束涉及 p0 和 s1 */
    int p = -1, s1 = -1;
    graph_destroy(g);
    g = make_graph(1, &p, &s1, NULL);
    TEST_ASSERT_NOT_NULL(g);
    memset(out, -1, sizeof(out));
    int n = graph_find_constraints_involving(g, p, out, 8);
    TEST_ASSERT_EQ(n, 1);
    TEST_ASSERT(out[0] >= 0 && out[0] < g->constraint_count, "索引有效");
    /* 不涉及节点 → 0 */
    TEST_ASSERT_EQ(graph_find_constraints_involving(g, 999, out, 8), 0);
    /* 容量限制 */
    TEST_ASSERT_EQ(graph_find_constraints_involving(g, p, out, 1), 1);

    /* 废弃后查询过滤（involving_index 保留废弃条目但查询按 is_active 过滤） */
    int cid = g->constraints[0]->id;
    TEST_ASSERT_EQ(graph_deactivate_constraint(g, cid), lv_OK);
    TEST_ASSERT_EQ(graph_find_constraints_involving(g, p, out, 8), 0);

    graph_destroy(g);
    printf("  test_involving_index_api: PASSED\n");
}

/* ============== 测试：冗余检测 ============== */

static void test_redundancy_api(void) {
    /* NULL 契约 → -1 */
    ConstraintGraph *g = make_graph(0, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(g);
    int parts[2] = {0, 1};
    TEST_ASSERT_EQ(graph_detect_redundancy(NULL, INCIDENCE, parts, 2), -1);
    TEST_ASSERT_EQ(graph_detect_redundancy(g, INCIDENCE, NULL, 2), -1);
    TEST_ASSERT_EQ(graph_detect_redundancy(g, INCIDENCE, parts, 0), -1);

    /* 无该约束 → 0 */
    TEST_ASSERT_EQ(graph_detect_redundancy(g, INCIDENCE, parts, 2), 0);

    /* 存在相同约束 → 1 */
    int p = -1, s1 = -1;
    graph_destroy(g);
    g = make_graph(1, &p, &s1, NULL);
    TEST_ASSERT_NOT_NULL(g);
    int same[2] = {p, s1};
    TEST_ASSERT_EQ(graph_detect_redundancy(g, INCIDENCE, same, 2), 1);
    /* 不同参与者 → 0 */
    int diff[2] = {p, p};
    TEST_ASSERT_EQ(graph_detect_redundancy(g, INCIDENCE, diff, 2), 0);

    graph_destroy(g);
    printf("  test_redundancy_api: PASSED\n");
}

/* ============== 测试：索引维护（insert / rebuild / dirty / sync） ============== */

static void test_index_maintenance_api(void) {
    ConstraintGraph *g = make_graph(1, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(g);

    /* mark_dirty：NULL 安全 / 置位 */
    graph_mark_dirty(NULL); /* 不崩溃即通过 */
    graph_mark_dirty(g);
    TEST_ASSERT(g->dirty, "脏标记已置位");

    /* sync_nodes：NULL 安全 / 同步后清脏 */
    graph_sync_nodes(NULL);
    graph_sync_nodes(g);
    TEST_ASSERT(!g->dirty, "同步后脏标记清除");

    /* sync_nodes 信任调整：INCIDENCE 参与者 trust>GREEN → GREEN */
    graph_mark_dirty(g);
    GeomNode *pn = graph_get_node(g, g->constraints[0]->participants[0]);
    TEST_ASSERT_NOT_NULL(pn);
    pn->trust = TRUST_YELLOW;
    graph_sync_nodes(g);
    TEST_ASSERT_EQ(pn->trust, TRUST_GREEN);

    /* node_index_insert / constraint_index_insert：重复插入索引一致（幂等） */
    GeomNode *node0 = graph_get_node(g, 0);
    TEST_ASSERT_NOT_NULL(node0);
    graph_node_index_insert(g, node0); /* 已索引节点重复插入：开放寻址跳过（有哨兵风险，仅验证后续查询仍一致） */
    TEST_ASSERT(graph_get_node(g, 0) != NULL, "节点仍可按 ID 查询");

    Constraint *con0 = g->constraints[0];
    TEST_ASSERT_NOT_NULL(con0);
    graph_constraint_index_insert(g, con0);
    TEST_ASSERT(graph_get_constraint(g, con0->id) != NULL, "约束仍可按 ID 查询");

    /* index_rebuild：NULL 安全 / 重建后查询一致 */
    graph_index_rebuild(NULL);
    graph_index_rebuild(g);
    TEST_ASSERT(graph_get_node(g, 0) != NULL, "重建后节点可查");
    TEST_ASSERT(graph_get_constraint(g, con0->id) != NULL, "重建后约束可查");
    TEST_ASSERT_EQ(graph_get_node_count(g), g->node_count);

    graph_destroy(g);
    printf("  test_index_maintenance_api: PASSED\n");
}

/* ============== 测试：错误存储 / 流上下文 / app_sink 查找 ============== */

static void test_error_stream_sink_api(void) {
    /* graph_set_error / graph_get_error */
    graph_set_error(NULL, "ignored"); /* NULL 安全：不存储 */
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    graph_set_error(g, "boom %d", 42);
    const char *err = graph_get_error(g);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT(strstr(err, "boom 42") != NULL, "错误信息写入 error_buffer");

    /* graph_set_stream_context：NULL 安全 */
    graph_set_stream_context(NULL);
    graph_set_stream_context(NULL);

    /* graph_find_app_sink_input：NULL / parent<0 / 无匹配 */
    TEST_ASSERT_EQ(graph_find_app_sink_input(NULL, 1), -1);
    TEST_ASSERT_EQ(graph_find_app_sink_input(g, -1), -1);
    TEST_ASSERT_EQ(graph_find_app_sink_input(g, 42), -1);

    /* 正路径：INPUT 端口 parent_block_id=42（is_formal_param 默认 false）命中 */
    TEST_ASSERT_EQ(graph_add_port(g, PORT_INPUT, 0, 42), ADD_NODE_OK);
    int in_port = graph_get_last_added_node_id(g);
    TEST_ASSERT(in_port >= 0, "输入端口节点");
    TEST_ASSERT_EQ(graph_find_app_sink_input(g, 42), in_port);

    /* OUTPUT 端口同 parent → 不命中（仅匹配 INPUT） */
    TEST_ASSERT_EQ(graph_add_port(g, PORT_OUTPUT, 0, 42), ADD_NODE_OK);
    TEST_ASSERT_EQ(graph_find_app_sink_input(g, 42), in_port);

    /* 形式参数端口（is_formal_param=true）→ 不命中 */
    TEST_ASSERT_EQ(graph_add_port(g, PORT_INPUT, 0, 43), ADD_NODE_OK);
    int fp = graph_get_last_added_node_id(g);
    GeomNode *fp_node = graph_get_node(g, fp);
    TEST_ASSERT_NOT_NULL(fp_node);
    TEST_ASSERT(fp_node->data.port != NULL, "端口数据");
    fp_node->data.port->is_formal_param = true;
    TEST_ASSERT_EQ(graph_find_app_sink_input(g, 43), -1);

    graph_destroy(g);
    printf("  test_error_stream_sink_api: PASSED\n");
}

/* ============== 测试：DOT → SVG（NULL 契约，正路径依赖外部 graphviz） ============== */

static void test_dot_svg_null_api(void) {
    ConstraintGraph *g = make_graph(1, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(g);
    DOTExportConfig cfg = dot_export_config_default();
    TEST_ASSERT_EQ(graph_export_dot_to_svg(NULL, &cfg, "out.svg"), lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_EQ(graph_export_dot_to_svg(g, &cfg, NULL), lv_ERROR_INVALID_PARAM);
    graph_destroy(g);
    printf("  test_dot_svg_null_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Constraint Graph Ext Test Suite")
    printf("=== Lv-00 Constraint Graph Ext Test Suite (batch C-㊵) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_angle_constraint_api);
    TEST_MAIN_RUN(test_remove_deactivate_api);
    TEST_MAIN_RUN(test_involving_index_api);
    TEST_MAIN_RUN(test_redundancy_api);
    TEST_MAIN_RUN(test_index_maintenance_api);
    TEST_MAIN_RUN(test_error_stream_sink_api);
    TEST_MAIN_RUN(test_dot_svg_null_api);

    lv_cleanup();
TEST_MAIN_END()
