/**
 * @file test_geo_dynamic.c
 * @brief 动态几何依赖图模块测试（第十三梯队 GeoGebra 落地验证）
 */
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "lv00/geo_dynamic.h"

#define TEST(name) printf("  [TEST] %s ... ", name)
#define PASS() printf("PASS\n")
#define FAIL(msg) printf("FAIL: %s\n", msg)

static int tests_passed = 0;
static int tests_failed = 0;

int main(void) {
    printf("=== geo_dynamic 模块测试 ===\n\n");

    /* 1. 创建与释放测试 */
    printf("[组 1] 创建与释放\n");
    {
        TEST("create: 创建依赖图");
        Lv00DynGraph *graph = lv00_dyn_graph_create(NULL);
        if (graph != NULL) { PASS(); tests_passed++; }
        else { FAIL("返回 NULL"); tests_failed++; }

        TEST("create: 图为空");
        if (graph && graph->node_count == 0) { PASS(); tests_passed++; }
        else { FAIL("初始节点数不为 0"); tests_failed++; }

        TEST("free: 释放依赖图");
        lv00_dyn_graph_free(graph);
        PASS(); tests_passed++;
    }

    /* 2. 节点操作测试 */
    printf("\n[组 2] 节点操作\n");
    {
        Lv00DynGraph *graph = lv00_dyn_graph_create(NULL);

        TEST("add_node: 添加自由点");
        int p1 = lv00_dyn_create_point(graph, 0, 0);
        if (p1 >= 0) { PASS(); tests_passed++; }
        else { FAIL("添加失败"); tests_failed++; }

        TEST("add_node: 添加第二个点");
        int p2 = lv00_dyn_create_point(graph, 3, 4);
        if (p2 >= 0) { PASS(); tests_passed++; }
        else { FAIL("添加失败"); tests_failed++; }

        TEST("add_node: 添加派生节点（中点）");
        int mid = lv00_dyn_create_midpoint(graph, p1, p2);
        if (mid >= 0) { PASS(); tests_passed++; }
        else { FAIL("添加失败"); tests_failed++; }

        TEST("add_node: 节点数量正确");
        if (graph->node_count == 3) { PASS(); tests_passed++; }
        else { FAIL("节点数量不正确"); tests_failed++; }

        TEST("get_node: 获取节点");
        Lv00DynNode *node = lv00_dyn_graph_get_node(graph, mid);
        if (node && node->type == LV00_DYN_NODE_MIDPOINT) { PASS(); tests_passed++; }
        else { FAIL("节点不存在或类型错误"); tests_failed++; }

        TEST("get_node: 父节点关系正确");
        if (node && node->parent_count == 2) { PASS(); tests_passed++; }
        else { FAIL("父节点数量不正确"); tests_failed++; }

        TEST("get_parents: 获取父节点列表");
        int parents[4];
        int count = lv00_dyn_graph_get_parents(graph, mid, parents, 4);
        if (count == 2 && parents[0] == p1 && parents[1] == p2) {
            PASS(); tests_passed++;
        } else { FAIL("父节点不正确"); tests_failed++; }

        TEST("get_children: 获取子节点列表");
        int children[16];
        count = lv00_dyn_graph_get_children(graph, p1, children, 16);
        if (count >= 1) { PASS(); tests_passed++; }
        else { FAIL("子节点数量不正确"); tests_failed++; }

        lv00_dyn_graph_free(graph);
    }

    /* 3. 派生节点创建测试 */
    printf("\n[组 3] 派生节点创建\n");
    {
        Lv00DynGraph *graph = lv00_dyn_graph_create(NULL);

        int p1 = lv00_dyn_create_point(graph, 0, 0);
        int p2 = lv00_dyn_create_point(graph, 3, 0);
        int p3 = lv00_dyn_create_point(graph, 0, 4);

        TEST("create_line: 创建直线");
        int line = lv00_dyn_create_line(graph, p1, p2);
        if (line >= 0) { PASS(); tests_passed++; }
        else { FAIL("创建失败"); tests_failed++; }

        TEST("create_distance: 创建距离");
        int dist = lv00_dyn_create_distance(graph, p1, p3);
        if (dist >= 0) { PASS(); tests_passed++; }
        else { FAIL("创建失败"); tests_failed++; }

        TEST("create_parallel: 创建平行线");
        int parallel = lv00_dyn_create_parallel(graph, line, p3);
        if (parallel >= 0) { PASS(); tests_passed++; }
        else { FAIL("创建失败"); tests_failed++; }

        TEST("create_perpendicular: 创建垂直线");
        int perp = lv00_dyn_create_perpendicular(graph, line, p3);
        if (perp >= 0) { PASS(); tests_passed++; }
        else { FAIL("创建失败"); tests_failed++; }

        lv00_dyn_graph_free(graph);
    }

    /* 4. 级联更新测试 */
    printf("\n[组 4] 级联更新\n");
    {
        Lv00DynGraph *graph = lv00_dyn_graph_create(NULL);

        int p1 = lv00_dyn_create_point(graph, 0, 0);
        int p2 = lv00_dyn_create_point(graph, 3, 4);
        int mid = lv00_dyn_create_midpoint(graph, p1, p2);

        TEST("update_cascade: 更新级联");
        int updated = lv00_dyn_graph_update_cascade(graph, p1, NULL);
        if (updated > 0) { PASS(); tests_passed++; }
        else { FAIL("没有更新任何节点"); tests_failed++; }

        TEST("update_cascade: 验证中点坐标");
        Lv00DynNode *mid_node = lv00_dyn_graph_get_node(graph, mid);
        if (mid_node && fabs(mid_node->params[0] - 1.5) < 1e-10 &&
            fabs(mid_node->params[1] - 2.0) < 1e-10) {
            PASS(); tests_passed++;
        } else { FAIL("中点坐标不正确"); tests_failed++; }

        TEST("mark_dirty: 标记脏节点");
        lv00_dyn_graph_mark_dirty(graph, p1);
        Lv00DynNode *dirty = lv00_dyn_graph_get_node(graph, p1);
        if (dirty && dirty->state == LV00_DYN_STATE_DIRTY) { PASS(); tests_passed++; }
        else { FAIL("状态未标记为 DIRTY"); tests_failed++; }

        lv00_dyn_graph_free(graph);
    }

    /* 5. 循环检测测试 */
    printf("\n[组 5] 循环检测\n");
    {
        Lv00DynGraph *graph = lv00_dyn_graph_create(NULL);

        int p1 = lv00_dyn_create_point(graph, 0, 0);
        int p2 = lv00_dyn_create_point(graph, 1, 0);
        int mid1 = lv00_dyn_create_midpoint(graph, p1, p2);
        int mid2 = lv00_dyn_create_midpoint(graph, p1, mid1);

        TEST("has_path: 检测路径存在");
        if (lv00_dyn_graph_has_path(graph, p1, mid1)) { PASS(); tests_passed++; }
        else { FAIL("应检测到路径"); tests_failed++; }

        TEST("has_path: 检测路径不存在");
        if (!lv00_dyn_graph_has_path(graph, mid1, p1)) { PASS(); tests_passed++; }
        else { FAIL("不应检测到路径"); tests_failed++; }

        TEST("would_create_cycle: 会形成循环（因为 p2 是 mid1 的父节点）");
        if (lv00_dyn_graph_would_create_cycle(graph, mid1, p2)) {
            PASS(); tests_passed++;
        } else { FAIL("应检测到会形成循环"); tests_failed++; }

        TEST("would_create_cycle: 不会形成循环（p1 和 p2 无依赖关系）");
        if (!lv00_dyn_graph_would_create_cycle(graph, p1, p2)) {
            PASS(); tests_passed++;
        } else { FAIL("不应形成循环"); tests_failed++; }

        lv00_dyn_graph_free(graph);
    }

    /* 6. 拓扑排序测试 */
    printf("\n[组 6] 拓扑排序\n");
    {
        Lv00DynGraph *graph = lv00_dyn_graph_create(NULL);

        int p1 = lv00_dyn_create_point(graph, 0, 0);
        int p2 = lv00_dyn_create_point(graph, 3, 4);
        int mid = lv00_dyn_create_midpoint(graph, p1, p2);
        int dist = lv00_dyn_create_distance(graph, mid, p2);

        TEST("topological_sort: 拓扑排序成功");
        int order[10];
        int count = lv00_dyn_graph_topological_sort(graph, order);
        if (count == 4) { PASS(); tests_passed++; }
        else { FAIL("排序失败"); tests_failed++; }

        TEST("topological_sort: 父节点在子节点之前");
        bool order_correct = true;
        /* 构建 ID -> position 映射 */
        int position[10] = {0};
        for (int i = 0; i < count; i++) {
            position[order[i]] = i;
        }
        /* 验证：每个节点的父节点位置应小于该节点位置 */
        for (int i = 0; i < count; i++) {
            Lv00DynNode *node = lv00_dyn_graph_get_node(graph, order[i]);
            if (!node) continue;
            for (int k = 0; k < node->parent_count; k++) {
                int parent_pos = position[node->parent_ids[k]];
                if (parent_pos > i) {
                    order_correct = false;
                    break;
                }
            }
            if (!order_correct) break;
        }
        if (order_correct) { PASS(); tests_passed++; }
        else { FAIL("排序顺序不正确"); tests_failed++; }

        lv00_dyn_graph_free(graph);
    }

    /* 7. 统计信息测试 */
    printf("\n[组 7] 统计信息\n");
    {
        Lv00DynGraph *graph = lv00_dyn_graph_create(NULL);

        int p1 = lv00_dyn_create_point(graph, 0, 0);
        int p2 = lv00_dyn_create_point(graph, 3, 4);
        int mid = lv00_dyn_create_midpoint(graph, p1, p2);

        TEST("get_stats: 统计信息正确");
        Lv00DynGraphStats stats;
        lv00_dyn_graph_get_stats(graph, &stats);
        if (stats.total_nodes == 3 && stats.free_nodes == 2 && stats.derived_nodes == 1) {
            PASS(); tests_passed++;
        } else { FAIL("统计信息不正确"); tests_failed++; }

        TEST("clear_dirty: 清除脏标记");
        lv00_dyn_graph_mark_dirty(graph, mid);
        lv00_dyn_graph_clear_dirty(graph);
        Lv00DynNode *clean = lv00_dyn_graph_get_node(graph, mid);
        if (clean && clean->state != LV00_DYN_STATE_DIRTY) { PASS(); tests_passed++; }
        else { FAIL("脏标记未清除"); tests_failed++; }

        lv00_dyn_graph_free(graph);
    }

    /* 8. 更新链测试 */
    printf("\n[组 8] 更新链\n");
    {
        Lv00DynGraph *graph = lv00_dyn_graph_create(NULL);

        int p1 = lv00_dyn_create_point(graph, 0, 0);
        int p2 = lv00_dyn_create_point(graph, 6, 8);
        int mid1 = lv00_dyn_create_midpoint(graph, p1, p2);
        int mid2 = lv00_dyn_create_midpoint(graph, mid1, p2);
        int dist = lv00_dyn_create_distance(graph, mid2, p2);

        /* 更新链 */
        lv00_dyn_graph_update_chain(graph, dist);

        TEST("update_chain: 更新链成功");
        Lv00DynNode *dist_node = lv00_dyn_graph_get_node(graph, dist);
        if (dist_node && dist_node->update_count > 0) { PASS(); tests_passed++; }
        else { FAIL("更新失败"); tests_failed++; }

        lv00_dyn_graph_free(graph);
    }

    printf("\n=== 测试结果: %d 通过, %d 失败 ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
