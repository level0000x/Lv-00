/**
 * @file test_geometry_ops.c
 * @brief 蓝图几何运算契约测试（TEN_LAYER_OPTIMIZED_PLAN §12.3/12.5/12.6/12.7，批次 G3）
 *
 * 覆盖：point（距离平方/中点/共线）、segment（长度/中点/方向/平行/垂直/
 * 相交/包含/点到线段距离）、triangle（面积/心点/半径）、intersect（线线/
 * 圆圆/线圆）。符号精度路径 + double 容差路径 + NULL/退化契约。
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/geometry_ops.h"

int g_pass_count = 0;
int g_fail_count = 0;

/** 创建有理坐标 num/den */
static SymbolicCoord *rc(int64_t num, uint64_t den) {
    return symbolic_coord_create_rational(num, den);
}

static SymbolicCoord *cy_holder = NULL;
static SymbolicCoord *cy_ptr(void) {
    if (cy_holder == NULL)
        cy_holder = rc(4, 1);
    return cy_holder;
}

/** 断言坐标 double 值在 [lo, hi] */
static void assert_in_range(const char *label, SymbolicCoord *c, double lo, double hi) {
    TEST_ASSERT_NOT_NULL(c);
    if (c == NULL)
        return;
    double v = symbolic_coord_to_double(c);
    TEST_ASSERT(v >= lo - 1e-6 && v <= hi + 1e-6, label);
}

/* ============== Point ============== */

static void test_point_ops(void) {
    /* A(1,2) B(4,6)：距离平方 = 3^2+4^2 = 25 */
    SymbolicCoord *ax = rc(1, 1), *ay = rc(2, 1), *bx = rc(4, 1), *by = rc(6, 1);
    SymbolicCoord *out = NULL;
    TEST_ASSERT(lv_point_distance_sq(ax, ay, bx, by, &out), "距离平方成功");
    assert_in_range("d2=25", out, 24.99, 25.01);
    symbolic_coord_destroy(out);

    /* 中点 (2.5, 4) */
    SymbolicCoord *mx = NULL, *my = NULL;
    TEST_ASSERT(lv_point_midpoint(ax, ay, bx, by, &mx, &my), "中点成功");
    assert_in_range("mx=2.5", mx, 2.49, 2.51);
    assert_in_range("my=4", my, 3.99, 4.01);
    symbolic_coord_destroy(mx);
    symbolic_coord_destroy(my);

    /* 共线：A(0,0) B(2,2) C(4,4) → true；D(4,5) → false */
    SymbolicCoord *z = rc(0, 1), *o2 = rc(2, 1), *o4x = rc(4, 1), *o4y = rc(4, 1), *o5y = rc(5, 1);
    bool coll = false;
    TEST_ASSERT(lv_point_is_collinear(z, z, o2, o2, o4x, o4y, &coll, 1e-10), "共线检测成功");
    TEST_ASSERT(coll, "共线返回 true");
    TEST_ASSERT(lv_point_is_collinear(z, z, o2, o2, o4x, o5y, &coll, 1e-10), "共线检测成功 2");
    TEST_ASSERT(!coll, "非共线返回 false");

    /* NULL 契约 */
    TEST_ASSERT(!lv_point_distance_sq(NULL, ay, bx, by, &out), "NULL 拒绝");
    TEST_ASSERT(!lv_point_distance_sq(ax, ay, bx, by, NULL), "NULL 输出拒绝");
    TEST_ASSERT(!lv_point_midpoint(ax, ay, bx, by, NULL, &my), "NULL 输出拒绝");
    TEST_ASSERT(!lv_point_is_collinear(ax, ay, bx, by, NULL, cy_ptr(), &coll, 1e-10), "NULL 拒绝");

    symbolic_coord_destroy(ax);
    symbolic_coord_destroy(ay);
    symbolic_coord_destroy(bx);
    symbolic_coord_destroy(by);
    symbolic_coord_destroy(z);
    symbolic_coord_destroy(o2);
    symbolic_coord_destroy(o4x);
    symbolic_coord_destroy(o4y);
    symbolic_coord_destroy(o5y);
    symbolic_coord_destroy(cy_holder);
}

/* ============== Segment ============== */

static void test_segment_ops(void) {
    /* 线段 A(0,0)-B(4,0)，线段 C(2,-2)-D(2,2) 垂直相交于 (2,0) */
    SymbolicCoord *z = rc(0, 1), *o4 = rc(4, 1), *o2 = rc(2, 1), *n2 = rc(-2, 1);
    SymbolicCoord *p2 = rc(2, 1);

    /* 长度平方 = 16 */
    SymbolicCoord *out = NULL;
    TEST_ASSERT(lv_segment_length_sq(z, z, o4, z, &out), "长度平方成功");
    assert_in_range("len2=16", out, 15.99, 16.01);
    symbolic_coord_destroy(out);

    /* 中点 (2,0) */
    SymbolicCoord *mx = NULL, *my = NULL;
    TEST_ASSERT(lv_segment_midpoint(z, z, o4, z, &mx, &my), "线段中点成功");
    assert_in_range("mx=2", mx, 1.99, 2.01);
    assert_in_range("my=0", my, -0.01, 0.01);
    symbolic_coord_destroy(mx);
    symbolic_coord_destroy(my);

    /* 方向 (4,0) */
    TEST_ASSERT(lv_segment_direction(z, z, o4, z, &mx, &my), "方向成功");
    assert_in_range("dx=4", mx, 3.99, 4.01);
    assert_in_range("dy=0", my, -0.01, 0.01);
    symbolic_coord_destroy(mx);
    symbolic_coord_destroy(my);

    /* 平行/垂直：水平 (0,0)-(4,0) vs 竖直 (2,-2)-(2,2) */
    bool outb = false;
    TEST_ASSERT(lv_segment_is_parallel(z, z, o4, z, p2, n2, p2, p2, &outb, 1e-10), "平行检测成功");
    TEST_ASSERT(!outb, "水平/竖直不平行为 false");
    TEST_ASSERT(lv_segment_is_perpendicular(z, z, o4, z, p2, n2, p2, p2, &outb, 1e-10), "垂直检测成功");
    TEST_ASSERT(outb, "水平/竖直垂直为 true");

    /* 平行：两条水平线 */
    SymbolicCoord *y1 = rc(1, 1);
    TEST_ASSERT(lv_segment_is_parallel(z, z, o4, z, z, y1, o4, y1, &outb, 1e-10), "平行检测成功 2");
    TEST_ASSERT(outb, "两水平线平行为 true");

    /* 相交：(2,0) */
    SymbolicCoord *ix = NULL, *iy = NULL;
    TEST_ASSERT(lv_segment_intersection(z, z, o4, z, p2, n2, p2, p2, &ix, &iy), "相交成功");
    assert_in_range("ix=2", ix, 1.99, 2.01);
    assert_in_range("iy=0", iy, -0.01, 0.01);
    symbolic_coord_destroy(ix);
    symbolic_coord_destroy(iy);

    /* 包含：点 (2,0) 在线段 (0,0)-(4,0) 上；点 (5,0) 不在 */
    TEST_ASSERT(lv_segment_contains_point(z, z, o4, z, p2, z, &outb, 1e-10), "包含检测成功");
    TEST_ASSERT(outb, "点在线段上");
    SymbolicCoord *o5 = rc(5, 1);
    TEST_ASSERT(lv_segment_contains_point(z, z, o4, z, o5, z, &outb, 1e-10), "包含检测成功 2");
    TEST_ASSERT(!outb, "点不在线段上");

    /* 点到线段距离：点 (2,3) 到 (0,0)-(4,0) = 3 */
    SymbolicCoord *y3 = rc(3, 1);
    SymbolicCoord *dist = NULL;
    TEST_ASSERT(lv_segment_distance_to_point(z, z, o4, z, p2, y3, &dist), "距离成功");
    assert_in_range("dist=3", dist, 2.99, 3.01);
    symbolic_coord_destroy(dist);

    /* NULL 契约 */
    TEST_ASSERT(!lv_segment_length_sq(z, z, NULL, z, &out), "NULL 拒绝");
    TEST_ASSERT(!lv_segment_intersection(z, z, o4, z, p2, n2, p2, p2, NULL, &iy), "NULL 输出拒绝");
    TEST_ASSERT(!lv_segment_is_parallel(z, z, o4, z, p2, n2, p2, p2, NULL, 1e-10), "NULL out 拒绝");

    symbolic_coord_destroy(z);
    symbolic_coord_destroy(o4);
    symbolic_coord_destroy(o2);
    symbolic_coord_destroy(n2);
    symbolic_coord_destroy(p2);
    symbolic_coord_destroy(y1);
    symbolic_coord_destroy(o5);
    symbolic_coord_destroy(y3);
}

/* ============== Triangle ============== */

static void test_triangle_ops(void) {
    /* 直角三角形 A(0,0) B(4,0) C(0,4) */
    SymbolicCoord *z = rc(0, 1), *o4 = rc(4, 1), *y4 = rc(4, 1);

    /* 面积 = 8 */
    SymbolicCoord *out = NULL;
    TEST_ASSERT(lv_triangle_area(z, z, o4, z, z, y4, &out), "面积成功");
    assert_in_range("area=8", out, 7.99, 8.01);
    symbolic_coord_destroy(out);

    /* 重心 (4/3, 4/3) */
    SymbolicCoord *gx = NULL, *gy = NULL;
    TEST_ASSERT(lv_triangle_centroid(z, z, o4, z, z, y4, &gx, &gy), "重心成功");
    assert_in_range("gx", gx, 1.32, 1.35);
    assert_in_range("gy", gy, 1.32, 1.35);
    symbolic_coord_destroy(gx);
    symbolic_coord_destroy(gy);

    /* 外心 (2,2) */
    TEST_ASSERT(lv_triangle_circumcenter(z, z, o4, z, z, y4, &gx, &gy), "外心成功");
    assert_in_range("ccx", gx, 1.99, 2.01);
    assert_in_range("ccy", gy, 1.99, 2.01);
    symbolic_coord_destroy(gx);
    symbolic_coord_destroy(gy);

    /* 垂心 (0,0)（直角顶点） */
    TEST_ASSERT(lv_triangle_orthocenter(z, z, o4, z, z, y4, &gx, &gy), "垂心成功");
    assert_in_range("hx", gx, -0.01, 0.01);
    assert_in_range("hy", gy, -0.01, 0.01);
    symbolic_coord_destroy(gx);
    symbolic_coord_destroy(gy);

    /* 内心 ≈ (1.172, 1.172) */
    TEST_ASSERT(lv_triangle_incenter(z, z, o4, z, z, y4, &gx, &gy), "内心成功");
    assert_in_range("ix", gx, 1.15, 1.20);
    assert_in_range("iy", gy, 1.15, 1.20);
    symbolic_coord_destroy(gx);
    symbolic_coord_destroy(gy);

    /* 九点圆心 (1,1)（外心与垂心中点） */
    TEST_ASSERT(lv_triangle_nine_point_center(z, z, o4, z, z, y4, &gx, &gy), "九点圆心成功");
    assert_in_range("npx", gx, 0.99, 1.01);
    assert_in_range("npy", gy, 0.99, 1.01);
    symbolic_coord_destroy(gx);
    symbolic_coord_destroy(gy);

    /* 旁心（A 角）≈ (-1.172, 1.172) 或 A 外旁心 ( -r, r ) 附近 */
    TEST_ASSERT(lv_triangle_excenter(z, z, o4, z, z, y4, &gx, &gy), "旁心成功");
    symbolic_coord_destroy(gx);
    symbolic_coord_destroy(gy);

    /* 内切圆半径 ≈ 1.172 */
    SymbolicCoord *r = NULL;
    TEST_ASSERT(lv_triangle_inradius(z, z, o4, z, z, y4, &r), "内半径成功");
    assert_in_range("inr", r, 1.15, 1.20);
    symbolic_coord_destroy(r);

    /* 外接圆半径 = 2√2 ≈ 2.828 */
    TEST_ASSERT(lv_triangle_circumradius(z, z, o4, z, z, y4, &r), "外半径成功");
    assert_in_range("cir", r, 2.82, 2.84);
    symbolic_coord_destroy(r);

    /* NULL 契约 */
    TEST_ASSERT(!lv_triangle_area(z, z, o4, z, z, NULL, &out), "NULL 拒绝");
    TEST_ASSERT(!lv_triangle_centroid(z, z, o4, z, z, y4, NULL, &gy), "NULL 输出拒绝");

    symbolic_coord_destroy(z);
    symbolic_coord_destroy(o4);
    symbolic_coord_destroy(y4);
}

/* ============== Intersect ============== */

static void test_intersect_ops(void) {
    /* 线线：y=x 与 y=-x+4 → (2,2)；平行线无交点 */
    SymbolicCoord *z = rc(0, 1), *o1 = rc(1, 1), *o2 = rc(2, 1), *o4 = rc(4, 1);
    SymbolicCoord *n1 = rc(-1, 1);
    SymbolicCoord *ix = NULL, *iy = NULL;
    bool parallel = false;
    /* L1: 过(0,0) 方向(1,1)；L2: 过(0,4) 方向(1,-1) */
    TEST_ASSERT(lv_intersect_lines(z, z, o1, o1, z, o4, o1, n1, &ix, &iy, &parallel), "线线相交成功");
    TEST_ASSERT(!parallel, "不平行");
    assert_in_range("ix=2", ix, 1.99, 2.01);
    assert_in_range("iy=2", iy, 1.99, 2.01);
    symbolic_coord_destroy(ix);
    symbolic_coord_destroy(iy);
    /* L1 与自身平行：方向 (1,1) vs (1,1) */
    TEST_ASSERT(!lv_intersect_lines(z, z, o1, o1, o2, o2, o1, o1, &ix, &iy, &parallel), "平行返回 false");
    TEST_ASSERT(parallel, "parallel 置 true");
    TEST_ASSERT(ix == NULL && iy == NULL, "无交点输出");
    symbolic_coord_destroy(ix);
    symbolic_coord_destroy(iy);

    /* 圆圆：C1(0,0,r=2) C2(4,0,r=2) → 交点 (2, ±√(4-1))=... 圆心距 4=2r 外切 1 点 (2,0) */
    SymbolicCoord *r2s = rc(4, 1);
    SymbolicCoord *p1x = NULL, *p1y = NULL, *p2x = NULL, *p2y = NULL;
    int count = 0;
    TEST_ASSERT(lv_intersect_circles(z, z, r2s, o4, z, r2s, &p1x, &p1y, &p2x, &p2y, &count), "圆圆相交成功");
    TEST_ASSERT(count == 1, "外切 1 交点");
    if (count >= 1) {
        assert_in_range("p1x=2", p1x, 1.99, 2.01);
        assert_in_range("p1y=0", p1y, -0.01, 0.01);
    }
    symbolic_coord_destroy(p1x);
    symbolic_coord_destroy(p1y);
    symbolic_coord_destroy(p2x);
    symbolic_coord_destroy(p2y);

    /* 圆圆：C1(0,0,2) C2(2,0,2) → 2 交点 */
    SymbolicCoord *o2x = rc(2, 1);
    TEST_ASSERT(lv_intersect_circles(z, z, r2s, o2x, z, r2s, &p1x, &p1y, &p2x, &p2y, &count), "圆圆相交成功 2");
    TEST_ASSERT(count == 2, "2 交点");
    if (count >= 1) {
        assert_in_range("q1x=1", p1x, 0.99, 1.01);
        assert_in_range("q1y=sqrt3", p1y, 1.72, 1.74);
    }
    symbolic_coord_destroy(p1x);
    symbolic_coord_destroy(p1y);
    symbolic_coord_destroy(p2x);
    symbolic_coord_destroy(p2y);

    /* 线圆：直线 y=0 与 C(0,2,2) → 2 交点 (±√(4-4)=0？) 用 C(0,0,2) 直线 y=0 → (-2,0),(2,0) */
    TEST_ASSERT(lv_intersect_line_circle(z, z, o1, z, z, z, r2s, &p1x, &p1y, &p2x, &p2y, &count), "线圆相交成功");
    TEST_ASSERT(count == 2, "2 交点");
    if (count >= 1) {
        double v1 = symbolic_coord_to_double(p1x), v2 = symbolic_coord_to_double(p2x);
        TEST_ASSERT((fabs(fabs(v1) - 2.0) < 1e-6) || (fabs(fabs(v2) - 2.0) < 1e-6), "交点 x=±2");
    }
    symbolic_coord_destroy(p1x);
    symbolic_coord_destroy(p1y);
    symbolic_coord_destroy(p2x);
    symbolic_coord_destroy(p2y);

    /* NULL 契约 */
    TEST_ASSERT(!lv_intersect_lines(z, z, o1, o1, z, o4, o1, n1, NULL, &iy, &parallel), "NULL 输出拒绝");
    TEST_ASSERT(!lv_intersect_circles(z, z, r2s, o4, z, r2s, &p1x, &p1y, &p2x, &p2y, NULL), "NULL count 拒绝");
    TEST_ASSERT(!lv_intersect_line_circle(z, z, o1, z, z, z, NULL, &p1x, &p1y, &p2x, &p2y, &count), "NULL r 拒绝");

    symbolic_coord_destroy(z);
    symbolic_coord_destroy(o1);
    symbolic_coord_destroy(o2);
    symbolic_coord_destroy(o2x);
    symbolic_coord_destroy(o4);
    symbolic_coord_destroy(n1);
    symbolic_coord_destroy(r2s);
}

/* ============== 入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Geometry Ops Test Suite")
    printf("=== Lv-00 Geometry Ops Test Suite (batch G3) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_point_ops);
    TEST_MAIN_RUN(test_segment_ops);
    TEST_MAIN_RUN(test_triangle_ops);
    TEST_MAIN_RUN(test_intersect_ops);
    lv_cleanup();
TEST_MAIN_END()
