/**
 * @file test_geo_predicate_ext.c
 * @brief 几何谓词契约测试（批次 C-㊺续31：geo_predicate.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（8 个：5 真实 + 3 兼容宏）：
 *   lv_orientation_3d / lv_segment_side / lv_same_side_of_circle
 *   lv_point_in_convex_polygon / lv_predicate_get_mode
 *   lv_orient2d / lv_orient3d / lv_incircle（兼容宏，展开到底层谓词）
 *
 * 契约要点（与 geo_predicate.c 核对）：
 *   - orientation_3d：四面体有符号体积的六倍；>0 LEFT、<0 RIGHT、=0 COPLANAR；
 *     p1 与任一其它点重合 -> DEGENERATE。
 *   - segment_side：与 line_side 同判定（方向谓词映射）；定义两点重合 -> DEGENERATE。
 *   - same_side_of_circle：两圆侧相同（或任一在圆上）true；异侧 false；退化 false。
 *   - point_in_convex_polygon：n<3 或数组 NULL 返回 false；退化多边形 false。
 *   - predicate_get_mode/set_mode：全局模式往返（测试保存并恢复初始值）。
 *
 * @author Lv-00 Project
 */

#include <stdbool.h>
#include <stdio.h>

#include "lv/geo_predicate.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：3D 方向谓词 ============== */

static void test_orientation_3d(void) {
    /* 正体积（右手系）：LEFT */
    lvOrientation o = lv_orientation_3d(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, lv_PREDICATE_EXACT);
    TEST_ASSERT_EQ((int) o, (int) lv_ORIENTATION_LEFT);

    /* 交换 p3/p4：负体积 -> RIGHT */
    o = lv_orientation_3d(0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, lv_PREDICATE_EXACT);
    TEST_ASSERT_EQ((int) o, (int) lv_ORIENTATION_RIGHT);

    /* 四点共面（z=0 平面）：COPLANAR */
    o = lv_orientation_3d(0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, lv_PREDICATE_EXACT);
    TEST_ASSERT_EQ((int) o, (int) lv_ORIENTATION_COPLANAR);

    /* p4 与 p1 重合：DEGENERATE */
    o = lv_orientation_3d(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, lv_PREDICATE_EXACT);
    TEST_ASSERT_EQ((int) o, (int) lv_ORIENTATION_DEGENERATE);
}

/* ============== 测试：线段侧判定 ============== */

static void test_segment_side(void) {
    /* 线段 (0,0)-(2,0)：上方点 LEFT */
    lvLineSide s = lv_segment_side(1.0, 1.0, 0.0, 0.0, 2.0, 0.0, lv_PREDICATE_EXACT);
    TEST_ASSERT_EQ((int) s, (int) lv_LINE_SIDE_LEFT);

    /* 下方点 RIGHT */
    s = lv_segment_side(1.0, -1.0, 0.0, 0.0, 2.0, 0.0, lv_PREDICATE_EXACT);
    TEST_ASSERT_EQ((int) s, (int) lv_LINE_SIDE_RIGHT);

    /* 线段上的点 ON */
    s = lv_segment_side(1.0, 0.0, 0.0, 0.0, 2.0, 0.0, lv_PREDICATE_EXACT);
    TEST_ASSERT_EQ((int) s, (int) lv_LINE_SIDE_ON);

    /* 退化线段（两端重合）：DEGENERATE */
    s = lv_segment_side(1.0, 1.0, 0.0, 0.0, 0.0, 0.0, lv_PREDICATE_EXACT);
    TEST_ASSERT_EQ((int) s, (int) lv_LINE_SIDE_DEGENERATE);
}

/* ============== 测试：圆同侧 ============== */

static void test_same_side_of_circle(void) {
    /* 圆心 (0,0) 半径 1 */
    /* 两点都在圆外：同侧 true */
    TEST_ASSERT(lv_same_side_of_circle(2.0, 0.0, 0.0, 2.0, 0.0, 0.0, 1.0, lv_PREDICATE_EXACT),
                "both outside same side");
    /* 一外一内：异侧 false */
    TEST_ASSERT(!lv_same_side_of_circle(2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, lv_PREDICATE_EXACT),
                "outside vs inside different side");
    /* 一在圆上：视为同侧 true */
    TEST_ASSERT(lv_same_side_of_circle(2.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, lv_PREDICATE_EXACT),
                "on-circle treated same side");
    /* 半径无效（负）：退化 false */
    TEST_ASSERT(!lv_same_side_of_circle(2.0, 0.0, 0.0, 2.0, 0.0, 0.0, -1.0, lv_PREDICATE_EXACT),
                "negative radius degenerate");
}

/* ============== 测试：凸多边形内部 ============== */

static void test_point_in_convex_polygon(void) {
    /* 正方形 (0,0),(2,0),(2,2),(0,2)（逆时针） */
    double xs[] = {0.0, 2.0, 2.0, 0.0};
    double ys[] = {0.0, 0.0, 2.0, 2.0};

    /* 内部点 */
    TEST_ASSERT(lv_point_in_convex_polygon(1.0, 1.0, xs, ys, 4, lv_PREDICATE_EXACT), "inside");
    /* 边界点（顶点） */
    TEST_ASSERT(lv_point_in_convex_polygon(0.0, 0.0, xs, ys, 4, lv_PREDICATE_EXACT), "on vertex");
    /* 外部点 */
    TEST_ASSERT(!lv_point_in_convex_polygon(3.0, 3.0, xs, ys, 4, lv_PREDICATE_EXACT), "outside");

    /* n < 3 / NULL 数组 */
    TEST_ASSERT(!lv_point_in_convex_polygon(1.0, 1.0, xs, ys, 2, lv_PREDICATE_EXACT), "n < 3");
    TEST_ASSERT(!lv_point_in_convex_polygon(1.0, 1.0, NULL, ys, 4, lv_PREDICATE_EXACT), "xs NULL");
    TEST_ASSERT(!lv_point_in_convex_polygon(1.0, 1.0, xs, NULL, 4, lv_PREDICATE_EXACT), "ys NULL");

    /* 退化多边形（三点共线）：false */
    double dxs[] = {0.0, 1.0, 2.0};
    double dys[] = {0.0, 0.0, 0.0};
    TEST_ASSERT(!lv_point_in_convex_polygon(1.0, 0.0, dxs, dys, 3, lv_PREDICATE_EXACT), "degenerate polygon");
}

/* ============== 测试：全局模式 get/set ============== */

static void test_predicate_get_mode(void) {
    lvPredicateMode orig = lv_predicate_get_mode();
    /* 初始默认自适应（与其他实现文件保持一致） */
    TEST_ASSERT_EQ((int) orig, (int) lv_PREDICATE_ADAPTIVE);

    lv_predicate_set_mode(lv_PREDICATE_EXACT);
    TEST_ASSERT_EQ((int) lv_predicate_get_mode(), (int) lv_PREDICATE_EXACT);

    lv_predicate_set_mode(lv_PREDICATE_APPROX);
    TEST_ASSERT_EQ((int) lv_predicate_get_mode(), (int) lv_PREDICATE_APPROX);

    /* 恢复初始值，避免污染同进程其他测试 */
    lv_predicate_set_mode(orig);
    TEST_ASSERT_EQ((int) lv_predicate_get_mode(), (int) orig);
}

/* ============== 测试：兼容宏别名 ============== */

static void test_compat_macros(void) {
    /* lv_orient2d -> lv_orientation_2d(ADAPTIVE)：(0,0),(1,0),(0,1) 逆时针 */
    TEST_ASSERT_EQ((int) lv_orient2d(0.0, 0.0, 1.0, 0.0, 0.0, 1.0), (int) lv_ORIENT_LEFT);

    /* lv_orient3d -> lv_orientation_3d(ADAPTIVE)：正体积 */
    TEST_ASSERT_EQ((int) lv_orient3d(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1), (int) lv_ORIENTATION_LEFT);

    /* lv_incircle -> lv_four_points_concyclic(ADAPTIVE)：共圆返回 0.0，否则 1.0 */
    TEST_ASSERT_DOUBLE(lv_incircle(1.0, 0.0, 0.0, 1.0, -1.0, 0.0, 0.0, -1.0), 0.0, 1e-15);
    TEST_ASSERT_DOUBLE(lv_incircle(0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0), 1.0, 1e-15);
}

/* ============== K41/F67：平行/垂直谓词 ============== */

static void test_lines_parallel_perpendicular(void) {
    /* 水平线段 A：(0,0)-(2,0)；水平线段 B：(1,1)-(3,1) → 平行 */
    TEST_ASSERT(lv_lines_parallel(0, 0, 2, 0, 1, 1, 3, 1, lv_PREDICATE_APPROX), "horizontal parallel");
    /* 竖直线段 C：(0,0)-(0,2)；水平线段 D：(1,1)-(3,1) → 垂直 */
    TEST_ASSERT(lv_lines_perpendicular(0, 0, 0, 2, 1, 1, 3, 1, lv_PREDICATE_APPROX), "vertical vs horizontal perpendicular");
    /* 非平行非垂直：45° 与水平 */
    TEST_ASSERT(!lv_lines_parallel(0, 0, 1, 1, 0, 0, 1, 0, lv_PREDICATE_APPROX), "45deg not parallel to horizontal");
    TEST_ASSERT(!lv_lines_perpendicular(0, 0, 1, 1, 0, 0, 1, 0, lv_PREDICATE_APPROX), "45deg not perpendicular to horizontal");
    /* 反平行（同方向反向）：仍平行 */
    TEST_ASSERT(lv_lines_parallel(0, 0, 2, 0, 3, 1, 1, 1, lv_PREDICATE_APPROX), "antiparallel still parallel");
    /* 退化线段（零长度）：false */
    TEST_ASSERT(!lv_lines_parallel(0, 0, 0, 0, 0, 0, 1, 0, lv_PREDICATE_APPROX), "zero-length segment not parallel");
    TEST_ASSERT(!lv_lines_perpendicular(0, 0, 0, 0, 0, 0, 1, 0, lv_PREDICATE_APPROX), "zero-length segment not perpendicular");
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("GeoPredicateExt")

    printf("\n--- geo_predicate (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_orientation_3d);
    TEST_MAIN_RUN(test_segment_side);
    TEST_MAIN_RUN(test_same_side_of_circle);
    TEST_MAIN_RUN(test_point_in_convex_polygon);
    TEST_MAIN_RUN(test_predicate_get_mode);
    TEST_MAIN_RUN(test_lines_parallel_perpendicular); /* K41/F67 */
    TEST_MAIN_RUN(test_compat_macros);

TEST_MAIN_END()
