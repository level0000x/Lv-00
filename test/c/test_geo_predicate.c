/**
 * @file test_geo_predicate.c
 * @brief 精确几何谓词模块测试（第十三梯队 CGAL 落地验证）
 */
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "lv00/geo_predicate.h"

#define TEST(name) printf("  [TEST] %s ... ", name)
#define PASS() printf("PASS\n")
#define FAIL(msg) printf("FAIL: %s\n", msg)

static int tests_passed = 0;
static int tests_failed = 0;

int main(void) {
    printf("=== geo_predicate 模块测试 ===\n\n");

    /* 1. orientation_2d 基础测试 */
    printf("[组 1] orientation_2d 基础测试\n");
    {
        /* 逆时针三角形 */
        TEST("orientation_2d: 逆时针三角形");
        Lv00Orientation o = lv00_orientation_2d(0,0, 1,0, 0,1, LV00_PREDICATE_APPROX);
        if (o == LV00_ORIENTATION_LEFT) { PASS(); tests_passed++; }
        else { FAIL("期望 LEFT"); tests_failed++; }

        /* 顺时针三角形 */
        TEST("orientation_2d: 顺时针三角形");
        o = lv00_orientation_2d(0,0, 0,1, 1,0, LV00_PREDICATE_APPROX);
        if (o == LV00_ORIENTATION_RIGHT) { PASS(); tests_passed++; }
        else { FAIL("期望 RIGHT"); tests_failed++; }

        /* 共线 */
        TEST("orientation_2d: 三点共线");
        o = lv00_orientation_2d(0,0, 1,1, 2,2, LV00_PREDICATE_APPROX);
        if (o == LV00_ORIENTATION_COLLINEAR) { PASS(); tests_passed++; }
        else { FAIL("期望 COLLINEAR"); tests_failed++; }
    }

    /* 2. orientation_2d 精度模式测试 */
    printf("\n[组 2] orientation_2d 精度模式测试\n");
    {
        /* 近似退化情况：接近共线但不完全共线 */
        double px = 1e-14;
        TEST("orientation_2d: 近似模式处理退化情况");
        Lv00Orientation o_approx = lv00_orientation_2d(0,0, 1,0, 1,px, LV00_PREDICATE_APPROX);
        /* 精确模式应该能正确判定 */
        Lv00Orientation o_exact = lv00_orientation_2d(0,0, 1,0, 1,px, LV00_PREDICATE_EXACT);
        /* 至少两种模式都应该返回有效结果 */
        if (o_approx != LV00_ORIENTATION_DEGENERATE && o_exact != LV00_ORIENTATION_DEGENERATE) {
            PASS(); tests_passed++;
        } else { FAIL("返回了 DEGENERATE"); tests_failed++; }

        /* 自适应模式 */
        TEST("orientation_2d: 自适应模式");
        Lv00Orientation o_adapt = lv00_orientation_2d(0,0, 1,0, 0,1, LV00_PREDICATE_ADAPTIVE);
        if (o_adapt == LV00_ORIENTATION_LEFT) { PASS(); tests_passed++; }
        else { FAIL("期望 LEFT"); tests_failed++; }
    }

    /* 3. line_side 测试 */
    printf("\n[组 3] line_side 测试\n");
    {
        TEST("line_side: 点在直线左侧");
        Lv00LineSide s = lv00_line_side(0,1, 0,0, 1,0, LV00_PREDICATE_APPROX);
        if (s == LV00_LINE_SIDE_LEFT) { PASS(); tests_passed++; }
        else { FAIL("期望 LEFT"); tests_failed++; }

        TEST("line_side: 点在直线右侧");
        s = lv00_line_side(0,-1, 0,0, 1,0, LV00_PREDICATE_APPROX);
        if (s == LV00_LINE_SIDE_RIGHT) { PASS(); tests_passed++; }
        else { FAIL("期望 RIGHT"); tests_failed++; }

        TEST("line_side: 点在直线上");
        s = lv00_line_side(0.5,0, 0,0, 1,0, LV00_PREDICATE_APPROX);
        if (s == LV00_LINE_SIDE_ON) { PASS(); tests_passed++; }
        else { FAIL("期望 ON"); tests_failed++; }
    }

    /* 4. side_of_circle 测试 */
    printf("\n[组 4] side_of_circle 测试\n");
    {
        TEST("side_of_circle: 点在圆内");
        Lv00SideOfCircle sc = lv00_side_of_circle(0,0, 1,0, 2.0, LV00_PREDICATE_APPROX);
        if (sc == LV00_SIDE_INSIDE) { PASS(); tests_passed++; }
        else { FAIL("期望 INSIDE"); tests_failed++; }

        TEST("side_of_circle: 点在圆上");
        sc = lv00_side_of_circle(3,0, 1,0, 2.0, LV00_PREDICATE_APPROX);
        if (sc == LV00_SIDE_ON) { PASS(); tests_passed++; }
        else { FAIL("期望 ON"); tests_failed++; }

        TEST("side_of_circle: 点在圆外");
        sc = lv00_side_of_circle(5,0, 1,0, 2.0, LV00_PREDICATE_APPROX);
        if (sc == LV00_SIDE_OUTSIDE) { PASS(); tests_passed++; }
        else { FAIL("期望 OUTSIDE"); tests_failed++; }
    }

    /* 5. same_side_of_line 测试 */
    printf("\n[组 5] same_side_of_line 测试\n");
    {
        TEST("same_side_of_line: 两点在同侧");
        bool r = lv00_same_side_of_line(0,1, 1,2, 0,0, 1,0, LV00_PREDICATE_APPROX);
        if (r) { PASS(); tests_passed++; }
        else { FAIL("期望 true"); tests_failed++; }

        TEST("same_side_of_line: 两点在异侧");
        r = lv00_same_side_of_line(0,1, 0,-1, 0,0, 1,0, LV00_PREDICATE_APPROX);
        if (!r) { PASS(); tests_passed++; }
        else { FAIL("期望 false"); tests_failed++; }
    }

    /* 6. segments_intersect 测试 */
    printf("\n[组 6] segments_intersect 测试\n");
    {
        TEST("segments_intersect: 相交线段");
        bool r = lv00_segments_intersect(0,0, 2,2, 0,2, 2,0, LV00_PREDICATE_APPROX);
        if (r) { PASS(); tests_passed++; }
        else { FAIL("期望 true"); tests_failed++; }

        TEST("segments_intersect: 不相交线段");
        r = lv00_segments_intersect(0,0, 1,1, 2,2, 3,3, LV00_PREDICATE_APPROX);
        if (!r) { PASS(); tests_passed++; }
        else { FAIL("期望 false"); tests_failed++; }

        TEST("segments_intersect: 共享端点");
        r = lv00_segments_intersect(0,0, 2,2, 0,0, 0,2, LV00_PREDICATE_APPROX);
        if (!r) { PASS(); tests_passed++; }
        else { FAIL("期望 false（共享端点不算内部相交）"); tests_failed++; }
    }

    /* 7. point_in_triangle 测试 */
    printf("\n[组 7] point_in_triangle 测试\n");
    {
        TEST("point_in_triangle: 内部点");
        bool r = lv00_point_in_triangle(0.5,0.5, 0,0, 1,0, 0,1, LV00_PREDICATE_APPROX);
        if (r) { PASS(); tests_passed++; }
        else { FAIL("期望 true"); tests_failed++; }

        TEST("point_in_triangle: 外部点");
        r = lv00_point_in_triangle(2,2, 0,0, 1,0, 0,1, LV00_PREDICATE_APPROX);
        if (!r) { PASS(); tests_passed++; }
        else { FAIL("期望 false"); tests_failed++; }

        TEST("point_in_triangle: 顶点");
        r = lv00_point_in_triangle(0,0, 0,0, 1,0, 0,1, LV00_PREDICATE_APPROX);
        if (r) { PASS(); tests_passed++; }
        else { FAIL("期望 true（顶点算内部）"); tests_failed++; }
    }

    /* 8. polygon_is_convex 测试 */
    printf("\n[组 8] polygon_is_convex 测试\n");
    {
        double sq_x[] = {0,1,1,0};
        double sq_y[] = {0,0,1,1};
        TEST("polygon_is_convex: 正方形");
        bool r = lv00_polygon_is_convex(sq_x, sq_y, 4, LV00_PREDICATE_APPROX);
        if (r) { PASS(); tests_passed++; }
        else { FAIL("期望 true"); tests_failed++; }

        double l_x[] = {0,2,1,1};
        double l_y[] = {0,0,1,2};
        TEST("polygon_is_convex: L 形（凹）");
        r = lv00_polygon_is_convex(l_x, l_y, 4, LV00_PREDICATE_APPROX);
        if (!r) { PASS(); tests_passed++; }
        else { FAIL("期望 false"); tests_failed++; }
    }

    /* 9. point_in_polygon 测试 */
    printf("\n[组 9] point_in_polygon 测试\n");
    {
        double sq_x[] = {0,1,1,0};
        double sq_y[] = {0,0,1,1};
        TEST("point_in_polygon: 内部点");
        bool r = lv00_point_in_polygon(0.5,0.5, sq_x, sq_y, 4, LV00_PREDICATE_APPROX);
        if (r) { PASS(); tests_passed++; }
        else { FAIL("期望 true"); tests_failed++; }

        TEST("point_in_polygon: 外部点");
        r = lv00_point_in_polygon(2,2, sq_x, sq_y, 4, LV00_PREDICATE_APPROX);
        if (!r) { PASS(); tests_passed++; }
        else { FAIL("期望 false"); tests_failed++; }
    }

    /* 10. four_points_concyclic 测试 */
    printf("\n[组 10] four_points_concyclic 测试\n");
    {
        TEST("four_points_concyclic: 单位圆上四点");
        bool r = lv00_four_points_concyclic(
            1,0, 0,1, -1,0, 0,-1, LV00_PREDICATE_APPROX);
        if (r) { PASS(); tests_passed++; }
        else { FAIL("期望 true"); tests_failed++; }

        TEST("four_points_concyclic: 不共圆四点");
        r = lv00_four_points_concyclic(
            0,0, 1,0, 0,1, 1,1, LV00_PREDICATE_APPROX);
        if (!r) { PASS(); tests_passed++; }
        else { FAIL("期望 false"); tests_failed++; }
    }

    /* 11. 谓词统计测试 */
    printf("\n[组 11] 谓词统计测试\n");
    {
        lv00_predicate_reset_stats();
        lv00_predicate_set_mode(LV00_PREDICATE_EXACT);
        lv00_orientation_2d(0,0, 1,0, 0,1, LV00_PREDICATE_EXACT);
        lv00_orientation_2d(0,0, 1,0, 0,1, LV00_PREDICATE_EXACT);
        Lv00PredicateStats stats;
        lv00_predicate_get_stats(&stats);
        TEST("predicate stats: 精确计数正确");
        if (stats.exact_count == 2) { PASS(); tests_passed++; }
        else { FAIL("期望 2"); tests_failed++; }
        lv00_predicate_set_mode(LV00_PREDICATE_ADAPTIVE);
    }

    /* 结果汇总 */
    printf("\n=== 测试结果: %d 通过, %d 失败 ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
