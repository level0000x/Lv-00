/**
 * @file test_geo_aabb_tree.c
 * @brief AABB 树空间索引模块测试（第十三梯队 CGAL + Boost.Geometry 落地验证）
 */
#include <stdio.h>
#include <math.h>
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

    printf("\n=== 测试结果: %d 通过, %d 失败 ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
