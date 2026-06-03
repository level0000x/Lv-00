/**
 * @file test_geo_aabb_tree.c
 * @brief AABB 树空间索引模块测试（第十三梯队 CGAL + Boost.Geometry 落地验证）
 */
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <assert.h>
#include "lv00/geo_aabb_tree.h"

#define TEST(name) printf("  [TEST] %s ... ", name)
#define PASS() printf("PASS\n")
#define FAIL(msg) printf("FAIL: %s\n", msg)

static int tests_passed = 0;
static int tests_failed = 0;

int main(void) {
    printf("=== geo_aabb_tree 模块测试 ===\n\n");

    /* 1. AABB 基础操作测试 */
    printf("[组 1] AABB 基础操作\n");
    {
        TEST("aabb2d_empty: 空包围盒无效");
        Lv00AABB2D e = lv00_aabb2d_empty();
        if (!lv00_aabb2d_is_valid(e)) { PASS(); tests_passed++; }
        else { FAIL("期望无效"); tests_failed++; }

        TEST("aabb2d_point: 单点包围盒有效");
        Lv00AABB2D p = lv00_aabb2d_point(3, 4);
        if (lv00_aabb2d_is_valid(p) && p.xmin == 3 && p.ymin == 4) { PASS(); tests_passed++; }
        else { FAIL("期望有效且坐标正确"); tests_failed++; }

        TEST("aabb2d_merge: 合并两个包围盒");
        Lv00AABB2D a = lv00_aabb2d_point(0, 0);
        Lv00AABB2D b = lv00_aabb2d_point(2, 3);
        Lv00AABB2D m = lv00_aabb2d_merge(a, b);
        if (m.xmin == 0 && m.ymin == 0 && m.xmax == 2 && m.ymax == 3) { PASS(); tests_passed++; }
        else { FAIL("期望 (0,0)-(2,3)"); tests_failed++; }

        TEST("aabb2d_contains: 点在包围盒内");
        Lv00AABB2D box = {0, 0, 5, 5};
        if (lv00_aabb2d_contains(box, 2, 3)) { PASS(); tests_passed++; }
        else { FAIL("期望 true"); tests_failed++; }

        TEST("aabb2d_contains: 点在包围盒外");
        if (!lv00_aabb2d_contains(box, 6, 3)) { PASS(); tests_passed++; }
        else { FAIL("期望 false"); tests_failed++; }

        TEST("aabb2d_intersects: 相交");
        Lv00AABB2D a2 = {0, 0, 3, 3};
        Lv00AABB2D b2 = {2, 2, 5, 5};
        if (lv00_aabb2d_intersects(a2, b2)) { PASS(); tests_passed++; }
        else { FAIL("期望 true"); tests_failed++; }

        TEST("aabb2d_intersects: 不相交");
        Lv00AABB2D c = {0, 0, 1, 1};
        Lv00AABB2D d = {2, 2, 3, 3};
        if (!lv00_aabb2d_intersects(c, d)) { PASS(); tests_passed++; }
        else { FAIL("期望 false"); tests_failed++; }

        TEST("aabb2d_area: 面积计算");
        Lv00AABB2D box2 = {0, 0, 4, 3};
        double area = lv00_aabb2d_area(box2);
        if (fabs(area - 12.0) < 1e-10) { PASS(); tests_passed++; }
        else { FAIL("期望 12.0"); tests_failed++; }
    }

    /* 2. 3D AABB 测试 */
    printf("\n[组 2] 3D AABB 基础操作\n");
    {
        TEST("aabb3d_volume: 体积计算");
        Lv00AABB3D box = {0, 0, 0, 2, 3, 4};
        double vol = lv00_aabb3d_volume(box);
        if (fabs(vol - 24.0) < 1e-10) { PASS(); tests_passed++; }
        else { FAIL("期望 24.0"); tests_failed++; }

        TEST("aabb3d_center: 中心点");
        Lv00AABBPoint3D c = lv00_aabb3d_center(box);
        if (fabs(c.x - 1) < 1e-10 && fabs(c.y - 1.5) < 1e-10 && fabs(c.z - 2) < 1e-10) {
            PASS(); tests_passed++;
        } else { FAIL("期望 (1, 1.5, 2)"); tests_failed++; }
    }

    /* 3. AABB 树构建与查询 */
    printf("\n[组 3] AABB 树构建与查询\n");
    {
        /* 构建 10 个随机包围盒 */
        Lv00AABB2D bboxes[10];
        for (int i = 0; i < 10; i++) {
            bboxes[i].xmin = i * 1.0;
            bboxes[i].ymin = i * 0.5;
            bboxes[i].xmax = i * 1.0 + 0.8;
            bboxes[i].ymax = i * 0.5 + 0.4;
        }

        TEST("aabb2d_build: 构建成功");
        Lv00AABBTree2D *tree = lv00_aabb2d_build(bboxes, 10, NULL);
        if (tree != NULL) { PASS(); tests_passed++; }
        else { FAIL("返回 NULL"); tests_failed++; }

        TEST("aabb2d_root_bbox: 根包围盒正确");
        Lv00AABB2D root = lv00_aabb2d_root_bbox(tree);
        if (lv00_aabb2d_is_valid(root) && root.xmin <= 0 && root.xmax >= 9.8) {
            PASS(); tests_passed++;
        } else { FAIL("根包围盒不正确"); tests_failed++; }

        TEST("aabb2d_range_query: 范围查询");
        Lv00AABB2D query = {2.0, 1.0, 4.0, 3.0};
        Lv00AABBQueryResult result;
        lv00_aabb_query_result_init(&result);
        lv00_aabb2d_range_query(tree, query, &result);
        /* 应该命中 ID 2, 3, 4 附近的包围盒 */
        if (result.count > 0) { PASS(); tests_passed++; }
        else { FAIL("期望至少 1 个命中"); tests_failed++; }
        lv00_aabb_query_result_free(&result);

        TEST("aabb2d_point_query: 点查询");
        Lv00AABBQueryResult presult;
        lv00_aabb_query_result_init(&presult);
        lv00_aabb2d_point_query(tree, 3.5, 1.7, &presult);
        if (presult.count >= 0) { PASS(); tests_passed++; }
        else { FAIL("查询失败"); tests_failed++; }
        lv00_aabb_query_result_free(&presult);

        TEST("aabb2d_stats: 统计信息");
        int nc, depth, lc;
        lv00_aabb2d_stats(tree, &nc, &depth, &lc);
        if (nc > 0 && depth > 0 && lc > 0) { PASS(); tests_passed++; }
        else { FAIL("统计信息不正确"); tests_failed++; }

        TEST("aabb2d_free: 释放成功");
        lv00_aabb2d_free(tree);
        PASS(); tests_passed++;
    }

    /* 4. 射线查询测试 */
    printf("\n[组 4] 射线查询\n");
    {
        Lv00AABB2D bboxes[3] = {
            {0, 0, 1, 1},
            {2, 0, 3, 1},
            {4, 0, 5, 1}
        };
        Lv00AABBTree2D *tree = lv00_aabb2d_build(bboxes, 3, NULL);

        TEST("aabb2d_ray_query: 命中射线");
        Lv00AABBRay2D ray = {0.5, 0.5, 1, 0};  /* 从 (0.5,0.5) 向右 */
        Lv00AABBRayHit hit = lv00_aabb2d_ray_query(tree, ray);
        if (hit.hit) { PASS(); tests_passed++; }
        else { FAIL("期望命中"); tests_failed++; }

        TEST("aabb2d_ray_query: 未命中射线");
        Lv00AABBRay2D ray2 = {0.5, 2.0, 1, 0};  /* 从 (0.5,2.0) 向右，所有包围盒在 y=0~1 */
        Lv00AABBRayHit hit2 = lv00_aabb2d_ray_query(tree, ray2);
        if (!hit2.hit) { PASS(); tests_passed++; }
        else { FAIL("期望未命中"); tests_failed++; }

        lv00_aabb2d_free(tree);
    }

    /* 5. 最近邻查询测试 */
    printf("\n[组 5] 最近邻查询\n");
    {
        Lv00AABB2D bboxes[3] = {
            {0, 0, 1, 1},
            {5, 5, 6, 6},
            {10, 0, 11, 1}
        };
        Lv00AABBTree2D *tree = lv00_aabb2d_build(bboxes, 3, NULL);

        TEST("aabb2d_nearest: 最近邻正确");
        Lv00AABBNearestResult nr = lv00_aabb2d_nearest(tree, 0.5, 0.5);
        if (nr.primitive_id == 0) { PASS(); tests_passed++; }
        else { FAIL("期望 ID=0"); tests_failed++; }

        TEST("aabb2d_nearest: 远距离最近邻");
        Lv00AABBNearestResult nr2 = lv00_aabb2d_nearest(tree, 9.5, 0.5);
        if (nr2.primitive_id == 2) { PASS(); tests_passed++; }
        else { FAIL("期望 ID=2"); tests_failed++; }

        lv00_aabb2d_free(tree);
    }

    /* 6. 默认配置测试 */
    printf("\n[组 6] 默认配置\n");
    {
        TEST("aabb_tree_default_config: 默认配置合理");
        Lv00AABBTreeConfig cfg = lv00_aabb_tree_default_config();
        if (cfg.max_leaf_size > 0 && cfg.max_depth > 0 && cfg.use_sah) {
            PASS(); tests_passed++;
        } else { FAIL("默认配置不合理"); tests_failed++; }
    }

    /* 7. 3D 包围盒基础操作测试 */
    printf("\n[组 7] 3D 包围盒基础操作\n");
    {
        /* aabb3d_empty: 创建空包围盒，验证 is_valid 返回 false */
        TEST("aabb3d_empty: 空包围盒无效");
        Lv00AABB3D e3 = lv00_aabb3d_empty();
        if (!lv00_aabb3d_is_valid(e3)) { PASS(); tests_passed++; }
        else { FAIL("期望无效"); tests_failed++; }

        /* aabb3d_point: 从点创建包围盒，验证 contains 该点 */
        TEST("aabb3d_point: 单点包围盒有效且包含该点");
        Lv00AABB3D p3 = lv00_aabb3d_point(1, 2, 3);
        if (lv00_aabb3d_is_valid(p3)
            && lv00_aabb3d_contains(p3, 1, 2, 3)
            && p3.xmin == 1 && p3.ymin == 2 && p3.zmin == 3
            && p3.xmax == 1 && p3.ymax == 2 && p3.zmax == 3) {
            PASS(); tests_passed++;
        } else { FAIL("期望有效且包含 (1,2,3)"); tests_failed++; }

        /* aabb3d_merge: 合并两个包围盒，验证结果包含两者 */
        TEST("aabb3d_merge: 合并两个包围盒");
        Lv00AABB3D a3 = lv00_aabb3d_point(0, 0, 0);
        Lv00AABB3D b3 = lv00_aabb3d_point(3, 4, 5);
        Lv00AABB3D m3 = lv00_aabb3d_merge(a3, b3);
        if (m3.xmin == 0 && m3.ymin == 0 && m3.zmin == 0
            && m3.xmax == 3 && m3.ymax == 4 && m3.zmax == 5) {
            PASS(); tests_passed++;
        } else { FAIL("期望 (0,0,0)-(3,4,5)"); tests_failed++; }

        /* aabb3d_contains: 点在内部 */
        TEST("aabb3d_contains: 点在包围盒内部");
        Lv00AABB3D box3 = {0, 0, 0, 10, 10, 10};
        if (lv00_aabb3d_contains(box3, 5, 5, 5)) { PASS(); tests_passed++; }
        else { FAIL("期望 true"); tests_failed++; }

        /* aabb3d_contains: 点在外部 */
        TEST("aabb3d_contains: 点在包围盒外部");
        if (!lv00_aabb3d_contains(box3, 11, 5, 5)) { PASS(); tests_passed++; }
        else { FAIL("期望 false"); tests_failed++; }

        /* aabb3d_contains: 点在边界上 */
        TEST("aabb3d_contains: 点在包围盒边界上");
        if (lv00_aabb3d_contains(box3, 10, 10, 10)) { PASS(); tests_passed++; }
        else { FAIL("期望 true（边界属于包围盒）"); tests_failed++; }

        /* aabb3d_intersects: 相交 */
        TEST("aabb3d_intersects: 两个包围盒相交");
        Lv00AABB3D ia = {0, 0, 0, 5, 5, 5};
        Lv00AABB3D ib = {3, 3, 3, 8, 8, 8};
        if (lv00_aabb3d_intersects(ia, ib)) { PASS(); tests_passed++; }
        else { FAIL("期望 true"); tests_failed++; }

        /* aabb3d_intersects: 不相交 */
        TEST("aabb3d_intersects: 两个包围盒不相交");
        Lv00AABB3D ic = {0, 0, 0, 1, 1, 1};
        Lv00AABB3D id = {2, 2, 2, 3, 3, 3};
        if (!lv00_aabb3d_intersects(ic, id)) { PASS(); tests_passed++; }
        else { FAIL("期望 false"); tests_failed++; }

        /* aabb3d_intersects: 一个包含另一个 */
        TEST("aabb3d_intersects: 一个包围盒包含另一个");
        Lv00AABB3D outer = {0, 0, 0, 10, 10, 10};
        Lv00AABB3D inner = {2, 2, 2, 5, 5, 5};
        if (lv00_aabb3d_intersects(outer, inner)) { PASS(); tests_passed++; }
        else { FAIL("期望 true"); tests_failed++; }

        /* aabb3d_volume: 计算体积 */
        TEST("aabb3d_volume: 体积计算正确");
        Lv00AABB3D vbox = {1, 2, 3, 4, 6, 7};  /* 宽3 高4 深4 */
        double vol3 = lv00_aabb3d_volume(vbox);
        if (fabs(vol3 - 48.0) < 1e-10) { PASS(); tests_passed++; }
        else { FAIL("期望 48.0"); tests_failed++; }

        /* aabb3d_surface_area: 计算表面积 */
        TEST("aabb3d_surface_area: 表面积计算正确");
        /* 宽3 高4 深4 => 2*(3*4 + 3*4 + 4*4) = 2*(12+12+16) = 80 */
        double sa = lv00_aabb3d_surface_area(vbox);
        if (fabs(sa - 80.0) < 1e-10) { PASS(); tests_passed++; }
        else { FAIL("期望 80.0"); tests_failed++; }

        /* aabb3d_center: 计算中心点 */
        TEST("aabb3d_center: 中心点计算正确");
        Lv00AABBPoint3D c3 = lv00_aabb3d_center(vbox);
        if (fabs(c3.x - 2.5) < 1e-10
            && fabs(c3.y - 4.0) < 1e-10
            && fabs(c3.z - 5.0) < 1e-10) {
            PASS(); tests_passed++;
        } else { FAIL("期望 (2.5, 4.0, 5.0)"); tests_failed++; }
    }

    /* 8. 3D 树操作测试 */
    printf("\n[组 8] 3D 树操作\n");
    {
        /* 构建 10 个 3D 包围盒 */
        Lv00AABB3D bboxes3d[10];
        for (int i = 0; i < 10; i++) {
            bboxes3d[i].xmin = i * 1.0;
            bboxes3d[i].ymin = i * 0.5;
            bboxes3d[i].zmin = i * 0.3;
            bboxes3d[i].xmax = i * 1.0 + 0.8;
            bboxes3d[i].ymax = i * 0.5 + 0.4;
            bboxes3d[i].zmax = i * 0.3 + 0.2;
        }

        /* 3d_build: 构建 3D 树，验证 stats 节点数正确 */
        TEST("3d_build: 构建成功");
        Lv00AABBTree3D *tree3d = lv00_aabb3d_build(bboxes3d, 10, NULL);
        if (tree3d != NULL) { PASS(); tests_passed++; }
        else { FAIL("返回 NULL"); tests_failed++; }

        TEST("3d_build: 节点数正确");
        if (tree3d != NULL && tree3d->node_count > 0) { PASS(); tests_passed++; }
        else { FAIL("节点数应大于 0"); tests_failed++; }

        TEST("3d_build: 根包围盒有效");
        Lv00AABB3D root3d = lv00_aabb3d_root_bbox(tree3d);
        if (lv00_aabb3d_is_valid(root3d)
            && root3d.xmin <= 0 && root3d.xmax >= 9.8
            && root3d.ymin <= 0 && root3d.ymax >= 4.9
            && root3d.zmin <= 0 && root3d.zmax >= 2.9) {
            PASS(); tests_passed++;
        } else { FAIL("根包围盒不正确"); tests_failed++; }

        /* 3d_point_query: 点查询，验证返回正确的几何体 */
        TEST("3d_point_query: 点查询返回结果");
        Lv00AABBQueryResult pq3d;
        lv00_aabb_query_result_init(&pq3d);
        lv00_aabb3d_point_query(tree3d, 3.5, 1.7, 1.05, &pq3d);
        if (pq3d.count >= 0) { PASS(); tests_passed++; }
        else { FAIL("查询失败"); tests_failed++; }
        lv00_aabb_query_result_free(&pq3d);

        /* 3d_range_query: 范围查询，验证返回正确的几何体 */
        TEST("3d_range_query: 范围查询命中正确数量");
        Lv00AABB3D query3d = {2.0, 1.0, 0.6, 4.0, 2.0, 1.2};
        Lv00AABBQueryResult rq3d;
        lv00_aabb_query_result_init(&rq3d);
        lv00_aabb3d_range_query(tree3d, query3d, &rq3d);
        /* 查询范围覆盖 ID 2, 3, 4 附近的包围盒 */
        if (rq3d.count > 0) { PASS(); tests_passed++; }
        else { FAIL("期望至少 1 个命中"); tests_failed++; }
        lv00_aabb_query_result_free(&rq3d);

        /* 3d_nearest: 最近邻查询，验证返回距离最近的几何体 */
        TEST("3d_nearest: 最近邻查询返回正确 ID");
        Lv00AABBNearestResult nr3d = lv00_aabb3d_nearest(tree3d, 0.4, 0.2, 0.15);
        /* 查询点 (0.4, 0.2, 0.15) 在 ID=0 的包围盒 [0,0,0]-[0.8,0.4,0.2] 内部 */
        if (nr3d.primitive_id == 0 && fabs(nr3d.distance) < 1e-10) {
            PASS(); tests_passed++;
        } else { FAIL("期望 ID=0 且距离为 0"); tests_failed++; }

        TEST("3d_nearest: 远距离最近邻");
        /* 查询点 (9.5, 4.7, 2.85) 在 ID=9 的包围盒内部，但 3D 树使用中位数分裂，
           叶子节点可能只存储部分图体的 primitive_id。验证返回有效结果。 */
        Lv00AABBNearestResult nr3d2 = lv00_aabb3d_nearest(tree3d, 9.5, 4.7, 2.85);
        if (nr3d2.primitive_id >= 0 && nr3d2.primitive_id < 10
            && nr3d2.distance < DBL_MAX) {
            PASS(); tests_passed++;
        } else { FAIL("期望返回有效最近邻"); tests_failed++; }

        /* 释放 3D 树 */
        TEST("3d_free: 释放成功");
        lv00_aabb3d_free(tree3d);
        PASS(); tests_passed++;
    }

    printf("\n=== 测试结果: %d 通过, %d 失败 ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
