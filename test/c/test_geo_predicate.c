/**
 * @file test_geo_predicate.c
 * @brief 精确几何谓词模块测试（第十三梯队 CGAL 落地验证）
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "lv/geo_predicate.h"
#include "lv/lv_numeric.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

TEST_MAIN_BEGIN("geo_predicate 模块测试")
    printf("=== geo_predicate 模块测试 ===\n\n");
    /* 1. orientation_2d 基础测试 */
    printf("[组 1] orientation_2d 基础测试\n");
    {
        /* 逆时针三角形 */
        TEST("orientation_2d: 逆时针三角形");
        lvOrientation o = lv_orientation_2d(0, 0, 1, 0, 0, 1, lv_PREDICATE_APPROX);
        if (o == lv_ORIENTATION_LEFT) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 LEFT");
            g_fail_count++;
        }
        /* 顺时针三角形 */
        TEST("orientation_2d: 顺时针三角形");
        o = lv_orientation_2d(0, 0, 0, 1, 1, 0, lv_PREDICATE_APPROX);
        if (o == lv_ORIENTATION_RIGHT) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 RIGHT");
            g_fail_count++;
        }
        /* 共线 */
        TEST("orientation_2d: 三点共线");
        o = lv_orientation_2d(0, 0, 1, 1, 2, 2, lv_PREDICATE_APPROX);
        if (o == lv_ORIENTATION_COLLINEAR) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 COLLINEAR");
            g_fail_count++;
        }
    }
    /* 2. orientation_2d 精度模式测试 */
    printf("\n[组 2] orientation_2d 精度模式测试\n");
    {
        /* 近似退化情况：接近共线但不完全共线 */
        double px = 1e-14;
        TEST("orientation_2d: 近似模式处理退化情况");
        lvOrientation o_approx = lv_orientation_2d(0, 0, 1, 0, 1, px, lv_PREDICATE_APPROX);
        /* 精确模式应该能正确判定 */
        lvOrientation o_exact = lv_orientation_2d(0, 0, 1, 0, 1, px, lv_PREDICATE_EXACT);
        /* 至少两种模式都应该返回有效结果 */
        if (o_approx != lv_ORIENTATION_DEGENERATE && o_exact != lv_ORIENTATION_DEGENERATE) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("返回了 DEGENERATE");
            g_fail_count++;
        }
        /* 自适应模式 */
        TEST("orientation_2d: 自适应模式");
        lvOrientation o_adapt = lv_orientation_2d(0, 0, 1, 0, 0, 1, lv_PREDICATE_ADAPTIVE);
        if (o_adapt == lv_ORIENTATION_LEFT) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 LEFT");
            g_fail_count++;
        }
    }
    /* 3. line_side 测试 */
    printf("\n[组 3] line_side 测试\n");
    {
        TEST("line_side: 点在直线左侧");
        lvLineSide s = lv_line_side(0, 1, 0, 0, 1, 0, lv_PREDICATE_APPROX);
        if (s == lv_LINE_SIDE_LEFT) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 LEFT");
            g_fail_count++;
        }
        TEST("line_side: 点在直线右侧");
        s = lv_line_side(0, -1, 0, 0, 1, 0, lv_PREDICATE_APPROX);
        if (s == lv_LINE_SIDE_RIGHT) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 RIGHT");
            g_fail_count++;
        }
        TEST("line_side: 点在直线上");
        s = lv_line_side(0.5, 0, 0, 0, 1, 0, lv_PREDICATE_APPROX);
        if (s == lv_LINE_SIDE_ON) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 ON");
            g_fail_count++;
        }
    }
    /* 4. side_of_circle 测试 */
    printf("\n[组 4] side_of_circle 测试\n");
    {
        TEST("side_of_circle: 点在圆内");
        lvSideOfCircle sc = lv_side_of_circle(0, 0, 1, 0, 2.0, lv_PREDICATE_APPROX);
        if (sc == lv_SIDE_INSIDE) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 INSIDE");
            g_fail_count++;
        }
        TEST("side_of_circle: 点在圆上");
        sc = lv_side_of_circle(3, 0, 1, 0, 2.0, lv_PREDICATE_APPROX);
        if (sc == lv_SIDE_ON) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 ON");
            g_fail_count++;
        }
        TEST("side_of_circle: 点在圆外");
        sc = lv_side_of_circle(5, 0, 1, 0, 2.0, lv_PREDICATE_APPROX);
        if (sc == lv_SIDE_OUTSIDE) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 OUTSIDE");
            g_fail_count++;
        }
    }
    /* 5. same_side_of_line 测试 */
    printf("\n[组 5] same_side_of_line 测试\n");
    {
        TEST("same_side_of_line: 两点在同侧");
        bool r = lv_same_side_of_line(0, 1, 1, 2, 0, 0, 1, 0, lv_PREDICATE_APPROX);
        if (r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 true");
            g_fail_count++;
        }
        TEST("same_side_of_line: 两点在异侧");
        r = lv_same_side_of_line(0, 1, 0, -1, 0, 0, 1, 0, lv_PREDICATE_APPROX);
        if (!r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 false");
            g_fail_count++;
        }
    }
    /* 6. segments_intersect 测试 */
    printf("\n[组 6] segments_intersect 测试\n");
    {
        TEST("segments_intersect: 相交线段");
        bool r = lv_segments_intersect(0, 0, 2, 2, 0, 2, 2, 0, lv_PREDICATE_APPROX);
        if (r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 true");
            g_fail_count++;
        }
        TEST("segments_intersect: 不相交线段");
        r = lv_segments_intersect(0, 0, 1, 1, 2, 2, 3, 3, lv_PREDICATE_APPROX);
        if (!r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 false");
            g_fail_count++;
        }
        TEST("segments_intersect: 共享端点");
        r = lv_segments_intersect(0, 0, 2, 2, 0, 0, 0, 2, lv_PREDICATE_APPROX);
        if (!r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 false（共享端点不算内部相交）");
            g_fail_count++;
        }
    }
    /* 7. point_in_triangle 测试 */
    printf("\n[组 7] point_in_triangle 测试\n");
    {
        TEST("point_in_triangle: 内部点");
        bool r = lv_point_in_triangle(0.5, 0.5, 0, 0, 1, 0, 0, 1, lv_PREDICATE_APPROX);
        if (r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 true");
            g_fail_count++;
        }
        TEST("point_in_triangle: 外部点");
        r = lv_point_in_triangle(2, 2, 0, 0, 1, 0, 0, 1, lv_PREDICATE_APPROX);
        if (!r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 false");
            g_fail_count++;
        }
        TEST("point_in_triangle: 顶点");
        r = lv_point_in_triangle(0, 0, 0, 0, 1, 0, 0, 1, lv_PREDICATE_APPROX);
        if (r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 true（顶点算内部）");
            g_fail_count++;
        }
    }
    /* 8. polygon_is_convex 测试 */
    printf("\n[组 8] polygon_is_convex 测试\n");
    {
        double sq_x[] = {0, 1, 1, 0};
        double sq_y[] = {0, 0, 1, 1};
        TEST("polygon_is_convex: 正方形");
        bool r = lv_polygon_is_convex(sq_x, sq_y, 4, lv_PREDICATE_APPROX);
        if (r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 true");
            g_fail_count++;
        }
        double l_x[] = {0, 2, 1, 1};
        double l_y[] = {0, 0, 1, 2};
        TEST("polygon_is_convex: L 形（凹）");
        r = lv_polygon_is_convex(l_x, l_y, 4, lv_PREDICATE_APPROX);
        if (!r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 false");
            g_fail_count++;
        }
    }
    /* 9. point_in_polygon 测试 */
    printf("\n[组 9] point_in_polygon 测试\n");
    {
        double sq_x[] = {0, 1, 1, 0};
        double sq_y[] = {0, 0, 1, 1};
        TEST("point_in_polygon: 内部点");
        bool r = lv_point_in_polygon(0.5, 0.5, sq_x, sq_y, 4, lv_PREDICATE_APPROX);
        if (r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 true");
            g_fail_count++;
        }
        TEST("point_in_polygon: 外部点");
        r = lv_point_in_polygon(2, 2, sq_x, sq_y, 4, lv_PREDICATE_APPROX);
        if (!r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 false");
            g_fail_count++;
        }
    }
    /* 10. four_points_concyclic 测试 */
    printf("\n[组 10] four_points_concyclic 测试\n");
    {
        TEST("four_points_concyclic: 单位圆上四点");
        bool r = lv_four_points_concyclic(1, 0, 0, 1, -1, 0, 0, -1, lv_PREDICATE_APPROX);
        if (r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 true");
            g_fail_count++;
        }
        TEST("four_points_concyclic: 不共圆四点");
        r = lv_four_points_concyclic(0, 0, 1, 0, 0, 1, 1, 1, lv_PREDICATE_APPROX);
        if (!r) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 false");
            g_fail_count++;
        }
    }
    /* 11. 谓词统计测试 */
    printf("\n[组 11] 谓词统计测试\n");
    {
        lv_predicate_reset_stats();
        lv_predicate_set_mode(lv_PREDICATE_EXACT);
        lv_orientation_2d(0, 0, 1, 0, 0, 1, lv_PREDICATE_EXACT);
        lv_orientation_2d(0, 0, 1, 0, 0, 1, lv_PREDICATE_EXACT);
        lvPredicateStats stats;
        lv_predicate_get_stats(&stats);
        TEST("predicate stats: 精确计数正确");
        if (stats.exact_count == 2) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 2");
            g_fail_count++;
        }
        lv_predicate_set_mode(lv_PREDICATE_ADAPTIVE);
    }
    /* 12. K5-3B 相对容差缩放 helper 测试 */
    printf("\n[组 12] K5-3B lv_rel_tol_scale 相对容差缩放测试\n");
    {
        /* 逐位等价：helper 必须与手写 eps * fmax(1.0, fabs(mag)) 完全一致 */
        TEST("lv_rel_tol_scale: 与手写形态逐位等价");
        static const double eps_cases[4] = {1e-9, 1e-6, 1e-12, 1e-3};
        static const double mag_cases[9] = {0.0, 0.5, 1.0, 2.0, 1e6, 1e-6, -3.5, -1e9, 1.0 + 1e-15};
        int equiv_ok = 1;
        for (int e = 0; e < 4 && equiv_ok; e++) {
            for (int m = 0; m < 9; m++) {
                double got = lv_rel_tol_scale(eps_cases[e], mag_cases[m]);
                double exp = eps_cases[e] * fmax(1.0, fabs(mag_cases[m]));
                if (got != exp) {
                    equiv_ok = 0;
                    break;
                }
            }
        }
        if (equiv_ok) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("helper 与手写形态存在逐位差异");
            g_fail_count++;
        }

        /* 边界行为：|mag|<=1 时保持 eps；0/1 恰在边界 */
        TEST("lv_rel_tol_scale: 边界行为");
        if (lv_rel_tol_scale(1e-9, 0.0) == 1e-9 && lv_rel_tol_scale(1e-9, 1.0) == 1e-9 &&
            lv_rel_tol_scale(1e-9, 0.5) == 1e-9 && lv_rel_tol_scale(1e-9, -0.5) == 1e-9 &&
            lv_rel_tol_scale(0.0, 1e6) == 0.0) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("边界行为不符合预期");
            g_fail_count++;
        }

        /* 量纲行为：k=1/2/3 的 scale^n 折算由调用点完成，helper 恒为 eps*max(1,|mag|) */
        TEST("lv_rel_tol_scale: 量纲指数 k=1/2/3 折算行为");
        double m1 = 1e6;             /* k=1: 坐标量级 */
        double m2 = 1e6 * 1e6;       /* k=2: 坐标²（行列式） */
        double m3 = m2 * 1e6;        /* k=3: 坐标³（叉积） */
        double r1 = lv_rel_tol_scale(1e-9, m1);
        double r2 = lv_rel_tol_scale(1e-9, m2);
        double r3 = lv_rel_tol_scale(1e-9, m3);
        /* 1e9 量级结果用相对误差比较（绝对容差 1e-10 低于该量级 double ulp） */
        if (fabs(r1 / 1e-3 - 1.0) < 1e-10 && fabs(r2 / 1e3 - 1.0) < 1e-10 && fabs(r3 / 1e9 - 1.0) < 1e-10) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("量纲折算行为不符合预期");
            g_fail_count++;
        }

        /* 钉住 k=3 容差缩放语义：大坐标近似共线判定。
         * p=(0,0), q=(1e6,0), r=(1e6,1e-3)：cross=1e6*1e-3=1e3；
         * adapted_eps = collinear_epsilon * max(1, 1e6^3) = 1e-9*1e18 = 1e9 > cross → COLLINEAR */
        TEST("orientation_2d: 大坐标近似共线（k=3 容差缩放钉住）");
        lvOrientation o_large = lv_orientation_2d(0, 0, 1e6, 0, 1e6, 1e-3, lv_PREDICATE_APPROX);
        if (o_large == lv_ORIENTATION_COLLINEAR) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 COLLINEAR（大坐标容差缩放语义改变）");
            g_fail_count++;
        }

        /* 对照：偏差量超过容差缩放后仍可判定方向（k=3 缩放不掩盖真实方向） */
        TEST("orientation_2d: 大坐标显著非共线判定 LEFT");
        lvOrientation o_large2 = lv_orientation_2d(0, 0, 1e6, 0, 1e6, 2e3, lv_PREDICATE_APPROX);
        if (o_large2 == lv_ORIENTATION_LEFT) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("期望 LEFT（cross=2e9 > adapted_eps=1e9）");
            g_fail_count++;
        }
    }
    /* 结果汇总 */
        
TEST_MAIN_END()
