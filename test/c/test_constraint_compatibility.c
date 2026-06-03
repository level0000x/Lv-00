/**
 * @file test_constraint_compatibility.c
 * @brief 约束拓扑规约层：约束相容性四态检测测试
 *
 * 这些测试先刻画新接口的期望行为：相容、矛盾、欠约束、过约束。
 * 实现层必须保证退化几何和冗余约束不会被静默当作普通相容事实。
 */

#include <stdio.h>

#include "lv00.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_empty_graph_is_under_constrained(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    Lv00ConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(graph, &result);

    TEST_ASSERT(ok, "空图相容性检查应成功返回诊断结果");
    TEST_ASSERT_EQ(result.status, LV00_CONSTRAINT_STATUS_UNDER_CONSTRAINED);

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

    Lv00ConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(graph, &result);

    TEST_ASSERT(ok, "单线段图相容性检查应成功");
    TEST_ASSERT_EQ(result.status, LV00_CONSTRAINT_STATUS_CONSISTENT);

    graph_destroy(graph);
}

static void test_degenerate_line_from_same_point_is_not_consistent(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    int a = add_point(graph, 0, 1, 0, 1);
    TEST_ASSERT(a >= 0, "点 A 应成功创建");
    (void) graph_add_line_segment(graph, a, a);

    Lv00ConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(graph, &result);

    TEST_ASSERT(ok, "退化线段图相容性检查应成功返回诊断结果");
    TEST_ASSERT(result.status == LV00_CONSTRAINT_STATUS_INCONSISTENT ||
                    result.status == LV00_CONSTRAINT_STATUS_UNDER_CONSTRAINED,
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

    Lv00ConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(graph, &result);

    TEST_ASSERT(ok, "重复线段图相容性检查应成功");
    TEST_ASSERT(result.status == LV00_CONSTRAINT_STATUS_OVER_CONSTRAINED || result.redundant_constraint_count > 0,
                "重复线段应被识别为过约束或冗余约束");

    graph_destroy(graph);
}

int main(void) {
    TEST_SUITE_BEGIN("约束相容性四态检测");

    TEST_RUN(test_empty_graph_is_under_constrained);
    TEST_RUN(test_single_segment_is_consistent);
    TEST_RUN(test_degenerate_line_from_same_point_is_not_consistent);
    TEST_RUN(test_duplicate_segment_constraint_is_over_constrained_or_redundant);

    TEST_SUITE_END();
    return g_fail_count == 0 ? 0 : 1;
}
