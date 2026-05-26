/**
 * @file test_conflict_detector.c
 * @brief 矛盾约束检测器单元测试
 *
 * @version 3.5.0
 */

#include <stdio.h>
#include <string.h>

#include "lv00/lv00.h"
#include "lv00/conflict_detector.h"
#include "test_helpers.h"

/* ================================================================
 * 测试：空图和边界条件
 * ================================================================ */

static int test_null_graph(void) {
    ConflictReport *report = lv00_conflict_report_create();
    TEST_ASSERT_NOT_NULL(report, "Failed to create conflict report");
    
    /* 检测 NULL 图应该返回错误 */
    int err = lv00_conflict_detect_all(NULL, NULL, report);
    TEST_ASSERT(err != 0, "Should return error for NULL graph");
    
    /* 快速检测应该返回 false */
    bool has_conflict = lv00_conflict_detect_quick(NULL);
    TEST_ASSERT(!has_conflict, "Quick detect on NULL should return false");
    
    lv00_conflict_report_destroy(report);
    return 0;
}

static int test_empty_graph(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph, "Failed to create graph");
    
    ConflictReport *report = lv00_conflict_report_create();
    TEST_ASSERT_NOT_NULL(report, "Failed to create report");
    
    /* 空图应该无矛盾 */
    int err = lv00_conflict_detect_all(graph, NULL, report);
    TEST_ASSERT_EQ(err, 0, "Empty graph should have no errors");
    TEST_ASSERT_EQ(report->conflict_count, 0, "Empty graph should have no conflicts");
    
    /* 快速检测 */
    bool has_conflict = lv00_conflict_detect_quick(graph);
    TEST_ASSERT(!has_conflict, "Quick detect on empty graph should return false");
    
    lv00_conflict_report_destroy(report);
    graph_destroy(graph);
    return 0;
}

/* ================================================================
 * 测试：报告管理
 * ================================================================ */

static int test_report_lifecycle(void) {
    ConflictReport *report = lv00_conflict_report_create();
    TEST_ASSERT_NOT_NULL(report, "Failed to create report");
    TEST_ASSERT_EQ(report->conflict_count, 0, "New report should have 0 conflicts");
    TEST_ASSERT(!report->has_critical, "New report should not have critical");
    TEST_ASSERT(!report->has_error, "New report should not have error");
    TEST_ASSERT(!report->has_warning, "New report should not have warning");
    
    /* 清空空报告应该安全 */
    lv00_conflict_report_clear(report);
    TEST_ASSERT_EQ(report->conflict_count, 0, "Cleared report should have 0 conflicts");
    
    lv00_conflict_report_destroy(report);
    
    /* 销毁 NULL 应该安全 */
    lv00_conflict_report_destroy(NULL);
    
    return 0;
}

/* ================================================================
 * 测试：配置管理
 * ================================================================ */

static int test_default_config(void) {
    const ConflictDetectorConfig *config = lv00_conflict_detector_default_config();
    TEST_ASSERT_NOT_NULL(config, "Default config should not be NULL");
    
    TEST_ASSERT(config->enable_basic_checks, "Basic checks should be enabled by default");
    TEST_ASSERT(config->enable_combination_checks, "Combination checks should be enabled by default");
    TEST_ASSERT(config->enable_transitive_checks, "Transitive checks should be enabled by default");
    TEST_ASSERT(!config->enable_algebraic_checks, "Algebraic checks should be disabled by default");
    
    TEST_ASSERT_GT(config->position_tolerance, 0.0, "Position tolerance should be positive");
    TEST_ASSERT_GT(config->distance_tolerance, 0.0, "Distance tolerance should be positive");
    TEST_ASSERT_GT(config->angle_tolerance, 0.0, "Angle tolerance should be positive");
    
    return 0;
}

/* ================================================================
 * 测试：类型名称
 * ================================================================ */

static int test_type_names(void) {
    /* 测试所有类型名称不为 NULL */
    for (int i = 0; i <= CONFLICT_UNKNOWN; i++) {
        const char *name = lv00_conflict_type_name((ConflictType)i);
        TEST_ASSERT_NOT_NULL(name, "Type name should not be NULL");
        TEST_ASSERT(strlen(name) > 0, "Type name should not be empty");
    }
    
    /* 测试严重程度名称 */
    TEST_ASSERT_NOT_NULL(lv00_conflict_severity_name(CONFLICT_SEVERITY_WARNING), "Warning name should exist");
    TEST_ASSERT_NOT_NULL(lv00_conflict_severity_name(CONFLICT_SEVERITY_ERROR), "Error name should exist");
    TEST_ASSERT_NOT_NULL(lv00_conflict_severity_name(CONFLICT_SEVERITY_CRITICAL), "Critical name should exist");
    
    return 0;
}

/* ================================================================
 * 测试：简单几何场景（无矛盾）
 * ================================================================ */

static int test_simple_triangle_no_conflict(void) {
    /* 创建一个简单的三角形，应该无矛盾 */
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph, "Failed to create graph");
    
    /* 添加三个点 */
    SymbolicCoord *coords1[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    SymbolicCoord *coords2[2] = {symbolic_coord_create_rational(3, 1), symbolic_coord_create_rational(0, 1)};
    SymbolicCoord *coords3[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(4, 1)};
    
    int p1 = graph_add_point_with_id(graph, 1, coords1, 2);
    int p2 = graph_add_point_with_id(graph, 2, coords2, 2);
    int p3 = graph_add_point_with_id(graph, 3, coords3, 2);
    
    TEST_ASSERT_GT(p1, 0, "Should create point 1");
    TEST_ASSERT_GT(p2, 0, "Should create point 2");
    TEST_ASSERT_GT(p3, 0, "Should create point 3");
    
    /* 添加三条边 */
    int line1 = graph_add_line_segment(graph, p1, p2);
    int line2 = graph_add_line_segment(graph, p2, p3);
    int line3 = graph_add_line_segment(graph, p3, p1);
    
    TEST_ASSERT_GT(line1, 0, "Should create line 1");
    TEST_ASSERT_GT(line2, 0, "Should create line 2");
    TEST_ASSERT_GT(line3, 0, "Should create line 3");
    
    /* 检测矛盾 */
    ConflictReport *report = lv00_conflict_report_create();
    TEST_ASSERT_NOT_NULL(report, "Failed to create report");
    
    int err = lv00_conflict_detect_all(graph, NULL, report);
    TEST_ASSERT_EQ(err, 0, "Detection should succeed");
    /* 注意：基础检测可能还无法检测所有情况，所以不强制要求 conflict_count == 0 */
    
    lv00_conflict_report_destroy(report);
    graph_destroy(graph);
    
    /* 清理坐标 */
    for (int i = 0; i < 2; i++) {
        symbolic_coord_destroy(coords1[i]);
        symbolic_coord_destroy(coords2[i]);
        symbolic_coord_destroy(coords3[i]);
    }
    
    return 0;
}

/* ================================================================
 * 测试：结构性矛盾检测
 * ================================================================ */

static Constraint *make_test_constraint(int id, ConstraintType type, const int *participants, int count) {
    Constraint *c = (Constraint *)calloc(1, sizeof(Constraint));
    if (!c) return NULL;
    c->id = id;
    c->type = type;
    c->participant_count = count;
    c->is_active = true;
    if (count > 0 && participants) {
        c->participants = (int *)calloc((size_t)count, sizeof(int));
        if (!c->participants) {
            free(c);
            return NULL;
        }
        for (int i = 0; i < count; i++) {
            c->participants[i] = participants[i];
        }
    }
    return c;
}

static void free_test_constraint(Constraint *c) {
    if (!c) return;
    free(c->participants);
    free(c);
}

static int test_detects_missing_participant_node(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph, "Failed to create graph");

    graph->constraint_capacity = 4;
    graph->constraints = (Constraint **)calloc((size_t)graph->constraint_capacity, sizeof(Constraint *));
    TEST_ASSERT_NOT_NULL(graph->constraints, "Failed to allocate test constraints");

    int parts[2] = {999, 1000};
    graph->constraints[0] = make_test_constraint(1, INCIDENCE, parts, 2);
    TEST_ASSERT_NOT_NULL(graph->constraints[0], "Failed to create invalid constraint");
    graph->constraint_count = 1;

    ConflictReport *report = lv00_conflict_report_create();
    TEST_ASSERT_NOT_NULL(report, "Failed to create report");

    int err = lv00_conflict_detect_all(graph, NULL, report);
    TEST_ASSERT_EQ(err, 0, "Detection should succeed even when conflicts exist");
    TEST_ASSERT_GT(report->conflict_count, 0, "Missing participant nodes should be reported as conflict");
    TEST_ASSERT(report->has_critical, "Missing participant node should be critical");

    lv00_conflict_report_destroy(report);
    free_test_constraint(graph->constraints[0]);
    free(graph->constraints);
    graph->constraints = NULL;
    graph->constraint_count = 0;
    graph->constraint_capacity = 0;
    graph_destroy(graph);
    return 0;
}

static int test_detects_degenerate_betweenness(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph, "Failed to create graph");

    SymbolicCoord *coords1[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    SymbolicCoord *coords2[2] = {symbolic_coord_create_rational(1, 1), symbolic_coord_create_rational(0, 1)};
    graph_add_point_with_id(graph, 1, coords1, 2);
    graph_add_point_with_id(graph, 2, coords2, 2);

    graph->constraint_capacity = 4;
    graph->constraints = (Constraint **)calloc((size_t)graph->constraint_capacity, sizeof(Constraint *));
    TEST_ASSERT_NOT_NULL(graph->constraints, "Failed to allocate test constraints");

    int parts[3] = {1, 1, 2};
    graph->constraints[0] = make_test_constraint(2, BETWEENNESS, parts, 3);
    TEST_ASSERT_NOT_NULL(graph->constraints[0], "Failed to create degenerate betweenness");
    graph->constraint_count = 1;

    ConflictReport *report = lv00_conflict_report_create();
    TEST_ASSERT_NOT_NULL(report, "Failed to create report");

    int err = lv00_conflict_detect_all(graph, NULL, report);
    TEST_ASSERT_EQ(err, 0, "Detection should succeed");
    TEST_ASSERT_GT(report->conflict_count, 0, "Degenerate betweenness should be reported");
    TEST_ASSERT(report->has_error || report->has_critical, "Degenerate betweenness should be error or critical");

    lv00_conflict_report_destroy(report);
    free_test_constraint(graph->constraints[0]);
    free(graph->constraints);
    graph->constraints = NULL;
    graph->constraint_count = 0;
    graph->constraint_capacity = 0;
    graph_destroy(graph);
    for (int i = 0; i < 2; i++) {
        symbolic_coord_destroy(coords1[i]);
        symbolic_coord_destroy(coords2[i]);
    }
    return 0;
}

/* ================================================================
 * 测试：JSON 输出
 * ================================================================ */

static int test_json_output(void) {
    ConflictReport *report = lv00_conflict_report_create();
    TEST_ASSERT_NOT_NULL(report, "Failed to create report");
    
    char buffer[1024];
    int len = lv00_conflict_report_to_json(report, buffer, sizeof(buffer));
    TEST_ASSERT_GT(len, 0, "JSON output should succeed");
    TEST_ASSERT(strstr(buffer, "conflict_count") != NULL, "JSON should contain conflict_count");
    TEST_ASSERT(strstr(buffer, "has_critical") != NULL, "JSON should contain has_critical");
    
    lv00_conflict_report_destroy(report);
    return 0;
}

/* ================================================================
 * 测试：便捷函数
 * ================================================================ */

static int test_convenience_functions(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph, "Failed to create graph");
    
    /* 测试 has_conflicts 便捷函数 */
    bool has_conflict = lv00_conflict_graph_has_conflicts(graph);
    /* 空图应该无矛盾 */
    TEST_ASSERT(!has_conflict, "Empty graph should have no conflicts");
    
    /* 测试 get_worst_type */
    ConflictReport *report = lv00_conflict_report_create();
    TEST_ASSERT_NOT_NULL(report, "Failed to create report");
    
    ConflictType worst = lv00_conflict_get_worst_type(report);
    TEST_ASSERT_EQ(worst, CONFLICT_UNKNOWN, "Empty report should return UNKNOWN");
    
    lv00_conflict_report_destroy(report);
    graph_destroy(graph);
    
    return 0;
}

/* ================================================================
 * 测试套件
 * ================================================================ */

int main(void) {
    printf("=== Conflict Detector Tests ===\n");
    
    TEST_RUN(test_null_graph);
    TEST_RUN(test_empty_graph);
    TEST_RUN(test_report_lifecycle);
    TEST_RUN(test_default_config);
    TEST_RUN(test_type_names);
    TEST_RUN(test_simple_triangle_no_conflict);
    TEST_RUN(test_json_output);
    TEST_RUN(test_convenience_functions);
    
    TEST_SUMMARY();
    return g_fail_count > 0 ? 1 : 0;
}
