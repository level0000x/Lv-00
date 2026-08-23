/**
 * @file test_geo_aabb_tree_ext.c
 * @brief AABB 树契约测试（批次 C-㊺续33：geo_aabb_tree.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（4 个）：
 *   lv_aabb2d_center / lv_aabb3d_ray_query
 *   lv_aabb_tree_build / lv_aabb_tree_query（旧版兼容，本批补齐实现）
 *
 * 契约要点（与 aabb_box.c / aabb_tree_impl.h 模板 / 本批补齐核对）：
 *   - aabb2d_center：((xmin+xmax)/2, (ymin+ymax)/2)。
 *   - aabb3d_ray_query：3D 射线（slab method），返回命中信息与 primitive_id。
 *   - 旧版 build：点数组（dim 交错）构建树；dim 2/3；无效参数 NULL。
 *   - 旧版 query：2D 包围盒查询命中点，输出坐标（x,y 交错），返回输出数量。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv/geo_aabb_tree.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#define TOL 1e-12

/* ============== 测试：2D 中心点 ============== */

static void test_aabb2d_center(void) {
    lvAABB2D bb = {0.0, 0.0, 2.0, 4.0};
    lvAABBPoint2D c = lv_aabb2d_center(bb);
    TEST_ASSERT_DOUBLE(c.x, 1.0, TOL);
    TEST_ASSERT_DOUBLE(c.y, 2.0, TOL);
}

/* ============== 测试：3D 射线查询 ============== */

static void test_aabb3d_ray_query(void) {
    lvAABB3D boxes[2];
    boxes[0] = (lvAABB3D){0.5, 0.0, 0.0, 1.5, 1.0, 1.0};
    boxes[1] = (lvAABB3D){5.0, 5.0, 5.0, 6.0, 6.0, 6.0};

    lvAABBTree3D *tree = lv_aabb3d_build(boxes, 2, NULL);
    TEST_ASSERT_NOT_NULL(tree);

    /* 射线从原点沿 +x：命中 box0（t=0.5） */
    lvAABBRay3D ray;
    ray.ox = 0.0;
    ray.oy = 0.0;
    ray.oz = 0.0;
    ray.dx = 1.0;
    ray.dy = 0.0;
    ray.dz = 0.0;
    lvAABBRayHit hit = lv_aabb3d_ray_query(tree, ray);
    TEST_ASSERT(hit.hit, "ray hits box0");
    TEST_ASSERT_EQ(hit.primitive_id, 0);
    TEST_ASSERT_DOUBLE(hit.t, 0.5, 1e-9);

    /* 射线沿 +y：不命中任何 box */
    ray.dx = 0.0;
    ray.dy = 1.0;
    ray.dz = 0.0;
    hit = lv_aabb3d_ray_query(tree, ray);
    TEST_ASSERT(!hit.hit, "ray +y misses");

    /* NULL 树：hit=false */
    lvAABBRayHit nh = lv_aabb3d_ray_query(NULL, ray);
    TEST_ASSERT(!nh.hit, "NULL tree no hit");

    lv_aabb3d_destroy(tree);
}

/* ============== 测试：旧版兼容 build/query ============== */

static void test_legacy_build_query(void) {
    /* 2D：点 (0,0)、(10,10) */
    double pts2[] = {0.0, 0.0, 10.0, 10.0};
    lvAABBTree *tree = lv_aabb_tree_build(pts2, 2, 2);
    TEST_ASSERT_NOT_NULL(tree);

    /* 查询 box [0,5]x[0,5]：命中点 0 → 输出 (0,0) */
    lvAABB box = {0.0, 0.0, 5.0, 5.0};
    double out[8];
    size_t n = lv_aabb_tree_query(tree, &box, out, 8);
    TEST_ASSERT(n >= 2, "hit point 0");
    TEST_ASSERT_DOUBLE(out[0], 0.0, TOL);
    TEST_ASSERT_DOUBLE(out[1], 0.0, TOL);

    /* 查询 box [20,20]-[30,30]：无命中 */
    lvAABB far_box = {20.0, 20.0, 30.0, 30.0};
    n = lv_aabb_tree_query(tree, &far_box, out, 8);
    TEST_ASSERT_EQ((int) n, 0);

    lv_aabb2d_destroy((lvAABBTree2D *) tree);

    /* 3D：点 (0,0,0)、(1,1,1) */
    double pts3[] = {0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    tree = lv_aabb_tree_build(pts3, 2, 3);
    TEST_ASSERT_NOT_NULL(tree);
    lv_aabb3d_destroy((lvAABBTree3D *) tree);

    /* 无效参数 */
    TEST_ASSERT_NULL(lv_aabb_tree_build(NULL, 2, 2));
    TEST_ASSERT_NULL(lv_aabb_tree_build(pts2, 0, 2));
    TEST_ASSERT_NULL(lv_aabb_tree_build(pts2, 2, 1));
    TEST_ASSERT_EQ((int) lv_aabb_tree_query(NULL, &box, out, 8), 0);
    TEST_ASSERT_EQ((int) lv_aabb_tree_query(tree, NULL, out, 8), 0);
    TEST_ASSERT_EQ((int) lv_aabb_tree_query(tree, &box, NULL, 8), 0);
    TEST_ASSERT_EQ((int) lv_aabb_tree_query(tree, &box, out, 0), 0);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("GeoAABBTreeExt")

    printf("\n--- geo_aabb_tree (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_aabb2d_center);
    TEST_MAIN_RUN(test_aabb3d_ray_query);
    TEST_MAIN_RUN(test_legacy_build_query);

TEST_MAIN_END()
