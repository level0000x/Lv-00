/**
 * @file test_conflict_detector_ext.c
 * @brief 矛盾约束检测器契约测试（批次 C-㊺续31：conflict_detector.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（3 个）：
 *   lv_conflict_detect_for_node / lv_conflict_detect_for_constraint / lv_conflict_report_print
 *
 * 契约要点（与 conflict_detector.c 核对）：
 *   - detect_for_node：graph/report NULL 或节点不存在/非活跃 -> lv_ERROR_NULL_POINTER；
 *     正常返回 0。
 *   - detect_for_constraint：graph/report NULL 或约束不存在/非活跃 -> lv_ERROR_NULL_POINTER；
 *     正常返回 0。
 *   - report_print：NULL 安全；output 为 FILE*（NULL = stdout）；空报告输出
 *     "Conflict Detection Report" 头部。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/conflict_detector.h"
#include "lv/constraint_graph.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：NULL 与不存在对象 ============== */

static void test_null_and_missing(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);
    ConflictReport *report = lv_conflict_report_create();
    TEST_ASSERT_NOT_NULL(report);

    /* NULL 契约 */
    TEST_ASSERT(lv_conflict_detect_for_node(NULL, 0, report) != 0, "NULL graph");
    TEST_ASSERT(lv_conflict_detect_for_node(graph, 0, NULL) != 0, "NULL report");
    TEST_ASSERT(lv_conflict_detect_for_constraint(NULL, 0, report) != 0, "NULL graph");
    TEST_ASSERT(lv_conflict_detect_for_constraint(graph, 0, NULL) != 0, "NULL report");

    /* 空图：节点/约束不存在 -> NULL_POINTER */
    TEST_ASSERT(lv_conflict_detect_for_node(graph, 0, report) != 0, "node missing");
    TEST_ASSERT(lv_conflict_detect_for_constraint(graph, 0, report) != 0, "constraint missing");

    lv_conflict_report_destroy(report);
    graph_destroy(graph);
}

/* ============== 测试：正常路径 ============== */

static void test_normal_path(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);
    ConflictReport *report = lv_conflict_report_create();
    TEST_ASSERT_NOT_NULL(report);

    /* 添加活跃点节点 */
    GeomNode *n = graph_add_node_with_id(graph, 0, GEOM_POINT, NULL, 0);
    TEST_ASSERT_NOT_NULL(n);

    /* 存在的活跃节点：返回 0（无冲突报告） */
    TEST_ASSERT_EQ(lv_conflict_detect_for_node(graph, 0, report), 0);
    TEST_ASSERT_EQ(report->conflict_count, 0);

    lv_conflict_report_destroy(report);
    graph_destroy(graph);
}

/* ============== 测试：报告输出 ============== */

static void test_report_print(void) {
    /* NULL 安全 */
    lv_conflict_report_print(NULL, NULL, false);

    ConflictReport *report = lv_conflict_report_create();
    TEST_ASSERT_NOT_NULL(report);

    /* 输出到 tmpfile，验证报告头部 */
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);
    lv_conflict_report_print(report, f, false);
    fflush(f);
    rewind(f);
    char buf[512];
    TEST_ASSERT_NOT_NULL(fgets(buf, sizeof(buf), f));
    TEST_ASSERT(strstr(buf, "Conflict Detection Report") != NULL, "report header");
    fclose(f);

    lv_conflict_report_destroy(report);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("ConflictDetectorExt")

    printf("\n--- conflict_detector (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_null_and_missing);
    TEST_MAIN_RUN(test_normal_path);
    TEST_MAIN_RUN(test_report_print);

TEST_MAIN_END()
