/**
 * @file test_constraint_compatibility.c
 * @brief 约束拓扑规约层：约束相容性四态检测测试
 *
 * 这些测试先刻画新接口的期望行为：相容、矛盾、欠约束、过约束。
 * 实现层必须保证退化几何和冗余约束不会被静默当作普通相容事实。
 *
 * v3.6.0: 新增过约束检测、不活跃节点跳过、约束类型 DOF 差异化、边界情况测试。
 */

#include <stdio.h>

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_empty_graph_is_under_constrained(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    lvConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(graph, &result);

    TEST_ASSERT(ok, "空图相容性检查应成功返回诊断结果");
    TEST_ASSERT_EQ(result.status, lv_CONSTRAINT_STATUS_UNDER_CONSTRAINED);

    graph_destroy(graph);
}

static void test_single_segment_is_consistent(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    int a = add_point(graph, 0, 1, 0, 1);
    int b = add_point(graph, 1, 1, 0, 1);
    TEST_ASSERT(a >= 0, "点 A 应成功创建");
    TEST_ASSERT(b >= 0, "点 B 应成功创建");
    TEST_ASSERT_EQ(graph_add_line_segment(graph, a, b), ADD_NODE_OK);

    lvConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(graph, &result);

    TEST_ASSERT(ok, "单线段图相容性检查应成功");
    TEST_ASSERT_EQ(result.status, lv_CONSTRAINT_STATUS_CONSISTENT);

    graph_destroy(graph);
}

static void test_degenerate_line_from_same_point_is_not_consistent(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    int a = add_point(graph, 0, 1, 0, 1);
    TEST_ASSERT(a >= 0, "点 A 应成功创建");
    (void) graph_add_line_segment(graph, a, a);

    lvConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(graph, &result);

    TEST_ASSERT(ok, "退化线段图相容性检查应成功返回诊断结果");
    TEST_ASSERT(result.status == lv_CONSTRAINT_STATUS_INCONSISTENT ||
                    result.status == lv_CONSTRAINT_STATUS_UNDER_CONSTRAINED,
                "重合点构造线段不得被判为普通相容");

    graph_destroy(graph);
}

static void test_duplicate_segment_constraint_is_over_constrained_or_redundant(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    int a = add_point(graph, 0, 1, 0, 1);
    int b = add_point(graph, 1, 1, 0, 1);
    TEST_ASSERT(a >= 0, "点 A 应成功创建");
    TEST_ASSERT(b >= 0, "点 B 应成功创建");
    TEST_ASSERT_EQ(graph_add_line_segment(graph, a, b), ADD_NODE_OK);
    TEST_ASSERT_EQ(graph_add_line_segment(graph, a, b), ADD_NODE_OK);

    lvConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(graph, &result);

    TEST_ASSERT(ok, "重复线段图相容性检查应成功");
    TEST_ASSERT(result.status == lv_CONSTRAINT_STATUS_OVER_CONSTRAINED || result.redundant_constraint_count > 0,
                "重复线段应被识别为过约束或冗余约束");

    graph_destroy(graph);
}

/* ================================================================
 * v3.6.0: 新增测试用例
 * ================================================================ */

/**
 * @brief 过约束检测：当 free_dof < 0 时应报告 OVER_CONSTRAINED
 *
 * 构造：2 点 + 1 线段（free_dof = 0） + 1 关联约束（free_dof = -1）
 */
static void test_over_constrained_with_extra_incidence(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    int a = add_point(graph, 0, 1, 0, 1);
    int b = add_point(graph, 1, 1, 0, 1);
    TEST_ASSERT(a >= 0, "点 A 应成功创建");
    TEST_ASSERT(b >= 0, "点 B 应成功创建");
    TEST_ASSERT_EQ(graph_add_line_segment(graph, a, b), ADD_NODE_OK);
    int seg_id = graph_get_last_added_node_id(graph);

    /* 添加关联约束：点 A 在线段上（额外约束使系统过约束） */
    AddConstraintResult cr = graph_add_incidence(graph, a, seg_id);
    TEST_ASSERT_EQ(cr, ADD_CONSTRAINT_OK);

    lvConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(graph, &result);

    TEST_ASSERT(ok, "过约束图相容性检查应成功");
    TEST_ASSERT_EQ(result.status, lv_CONSTRAINT_STATUS_OVER_CONSTRAINED);
    TEST_ASSERT(result.free_degree_count < 0, "过约束时 free_dof 应为负数");

    graph_destroy(graph);
}

/**
 * @brief 不活跃节点应被跳过
 *
 * 构造：创建 2 点 + 1 线段（CONSISTENT），然后标记一个点为不活跃。
 * 此时只有 1 个活跃点 + 1 个不活跃点 + 1 线段，应判为欠约束。
 */
static void test_inactive_node_skipped(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    int a = add_point(graph, 0, 1, 0, 1);
    int b = add_point(graph, 1, 1, 0, 1);
    TEST_ASSERT(a >= 0, "点 A 应成功创建");
    TEST_ASSERT(b >= 0, "点 B 应成功创建");
    TEST_ASSERT_EQ(graph_add_line_segment(graph, a, b), ADD_NODE_OK);

    /* 标记点 B 为不活跃 */
    GeomNode *node_b = graph_get_node(graph, b);
    TEST_ASSERT_NOT_NULL(node_b);
    node_b->is_active = false;

    lvConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(graph, &result);

    TEST_ASSERT(ok, "不活跃节点跳过后的相容性检查应成功");
    /* 1 活跃点 * 2 DOF = 2，1 线段 * 4 = 4 约束，free_dof = 2 - 4 = -2 */
    TEST_ASSERT(result.status == lv_CONSTRAINT_STATUS_OVER_CONSTRAINED ||
                    result.status == lv_CONSTRAINT_STATUS_UNDER_CONSTRAINED,
                "不活跃节点应被跳过，改变 DOF 计算结果");

    graph_destroy(graph);
}

/**
 * @brief 所有节点不活跃时应判为欠约束
 */
static void test_all_inactive_nodes_is_under_constrained(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    int a = add_point(graph, 0, 1, 0, 1);
    int b = add_point(graph, 1, 1, 0, 1);
    TEST_ASSERT(a >= 0, "点 A 应成功创建");
    TEST_ASSERT(b >= 0, "点 B 应成功创建");

    /* 标记所有点为不活跃 */
    GeomNode *node_a = graph_get_node(graph, a);
    GeomNode *node_b = graph_get_node(graph, b);
    TEST_ASSERT_NOT_NULL(node_a);
    TEST_ASSERT_NOT_NULL(node_b);
    node_a->is_active = false;
    node_b->is_active = false;

    lvConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(graph, &result);

    TEST_ASSERT(ok, "所有节点不活跃时相容性检查应成功");
    TEST_ASSERT_EQ(result.status, lv_CONSTRAINT_STATUS_UNDER_CONSTRAINED);

    graph_destroy(graph);
}

/**
 * @brief 连接约束 (CONNECTION) 不消耗 DOF
 *
 * 构造：2 点 + 1 线段 + 1 连接约束，期望仍为 CONSISTENT
 * （因为 CONNECTION 约束消耗 0 DOF）
 */
static void test_connection_constraint_zero_dof(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    int a = add_point(graph, 0, 1, 0, 1);
    int b = add_point(graph, 1, 1, 0, 1);
    TEST_ASSERT(a >= 0, "点 A 应成功创建");
    TEST_ASSERT(b >= 0, "点 B 应成功创建");
    TEST_ASSERT_EQ(graph_add_line_segment(graph, a, b), ADD_NODE_OK);

    /* 创建端口节点（graph_add_connection 要求两端均为 GEOM_PORT） */
    TEST_ASSERT_EQ(graph_add_port(graph, PORT_OUTPUT, 0, -1), ADD_NODE_OK);
    int src_port = graph_get_last_added_node_id(graph);
    TEST_ASSERT_EQ(graph_add_port(graph, PORT_INPUT, 0, -1), ADD_NODE_OK);
    int dst_port = graph_get_last_added_node_id(graph);

    /* 添加连接约束（数据流，非几何约束） */
    AddConstraintResult cr = graph_add_connection(graph, src_port, dst_port);
    TEST_ASSERT_EQ(cr, ADD_CONSTRAINT_OK);

    lvConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(graph, &result);

    TEST_ASSERT(ok, "含连接约束的图相容性检查应成功");
    TEST_ASSERT_EQ(result.status, lv_CONSTRAINT_STATUS_CONSISTENT);
    TEST_ASSERT_EQ(result.free_degree_count, 0);

    graph_destroy(graph);
}

/**
 * @brief 恰好约束（free_dof == 0）应报告 CONSISTENT
 *
 * 构造：2 点 + 1 线段，free_dof = 4 - 4 = 0
 */
static void test_exactly_constrained_is_consistent(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    int a = add_point(graph, 0, 1, 0, 1);
    int b = add_point(graph, 2, 1, 0, 1);
    TEST_ASSERT(a >= 0, "点 A 应成功创建");
    TEST_ASSERT(b >= 0, "点 B 应成功创建");
    TEST_ASSERT_EQ(graph_add_line_segment(graph, a, b), ADD_NODE_OK);

    lvConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(graph, &result);

    TEST_ASSERT(ok, "恰好约束图相容性检查应成功");
    TEST_ASSERT_EQ(result.status, lv_CONSTRAINT_STATUS_CONSISTENT);
    TEST_ASSERT_EQ(result.free_degree_count, 0);

    graph_destroy(graph);
}

/**
 * @brief 空指针输入应返回 INVALID 并返回 false
 */
static void test_null_inputs_return_invalid(void) {
    lvConstraintCompatibilityResult result;

    bool ok1 = graph_check_compatibility(NULL, &result);
    TEST_ASSERT(!ok1, "NULL 图输入应返回 false");
    TEST_ASSERT_EQ(result.status, lv_CONSTRAINT_STATUS_INVALID);

    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);
    bool ok2 = graph_check_compatibility(graph, NULL);
    TEST_ASSERT(!ok2, "NULL 输出参数应返回 false");

    graph_destroy(graph);
}

int main(void) {
    TEST_SUITE_BEGIN("约束相容性四态检测");

    /* 原有测试 */
    TEST_RUN(test_empty_graph_is_under_constrained);
    TEST_RUN(test_single_segment_is_consistent);
    TEST_RUN(test_degenerate_line_from_same_point_is_not_consistent);
    TEST_RUN(test_duplicate_segment_constraint_is_over_constrained_or_redundant);

    /* v3.6.0: 新增测试 */
    TEST_RUN(test_over_constrained_with_extra_incidence);
    TEST_RUN(test_inactive_node_skipped);
    TEST_RUN(test_all_inactive_nodes_is_under_constrained);
    TEST_RUN(test_connection_constraint_zero_dof);
    TEST_RUN(test_exactly_constrained_is_consistent);
    TEST_RUN(test_null_inputs_return_invalid);

    TEST_SUITE_END();
    return g_fail_count == 0 ? 0 : 1;
}
