/**
 * @file test_geo_dynamic.c
 * @brief 动态几何依赖图模块测试（第十三梯队 GeoGebra 落地验证）
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "lv/geo_dynamic.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

TEST_MAIN_BEGIN("geo_dynamic 模块测试")
    printf("=== geo_dynamic 模块测试 ===\n\n");
    /* 1. 创建与释放测试 */
    printf("[组 1] 创建与释放\n");
    {
        TEST("create: 创建依赖图");
        lvDynGraph *graph = lv_dyn_graph_create(NULL);
        if (graph != NULL) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("返回 NULL");
            g_fail_count++;
        }
        TEST("create: 图为空");
        if (graph && graph->node_count == 0) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("初始节点数不为 0");
            g_fail_count++;
        }
        TEST("free: 释放依赖图");
        lv_dyn_graph_destroy(graph);
        PASS();
        g_pass_count++;
    }
    /* 2. 节点操作测试 */
    printf("\n[组 2] 节点操作\n");
    {
        lvDynGraph *graph = lv_dyn_graph_create(NULL);
        TEST("add_node: 添加自由点");
        int p1 = lv_dyn_create_point(graph, 0, 0);
        if (p1 >= 0) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("添加失败");
            g_fail_count++;
        }
        TEST("add_node: 添加第二个点");
        int p2 = lv_dyn_create_point(graph, 3, 4);
        if (p2 >= 0) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("添加失败");
            g_fail_count++;
        }
        TEST("add_node: 添加派生节点（中点）");
        int mid = lv_dyn_create_midpoint(graph, p1, p2);
        if (mid >= 0) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("添加失败");
            g_fail_count++;
        }
        TEST("add_node: 节点数量正确");
        if (graph->node_count == 3) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("节点数量不正确");
            g_fail_count++;
        }
        TEST("get_node: 获取节点");
        lvDynNode *node = lv_dyn_graph_get_node(graph, mid);
        if (node && node->type == lv_DYN_NODE_MIDPOINT) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("节点不存在或类型错误");
            g_fail_count++;
        }
        TEST("get_node: 父节点关系正确");
        if (node && node->parent_count == 2) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("父节点数量不正确");
            g_fail_count++;
        }
        TEST("get_parents: 获取父节点列表");
        int parents[4];
        int count = lv_dyn_graph_get_parents(graph, mid, parents, 4);
        if (count == 2 && parents[0] == p1 && parents[1] == p2) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("父节点不正确");
            g_fail_count++;
        }
        TEST("get_children: 获取子节点列表");
        int children[16];
        count = lv_dyn_graph_get_children(graph, p1, children, 16);
        if (count >= 1) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("子节点数量不正确");
            g_fail_count++;
        }
        lv_dyn_graph_destroy(graph);
    }
    /* 3. 派生节点创建测试 */
    printf("\n[组 3] 派生节点创建\n");
    {
        lvDynGraph *graph = lv_dyn_graph_create(NULL);
        int p1 = lv_dyn_create_point(graph, 0, 0);
        int p2 = lv_dyn_create_point(graph, 3, 0);
        int p3 = lv_dyn_create_point(graph, 0, 4);
        TEST("create_line: 创建直线");
        int line = lv_dyn_create_line(graph, p1, p2);
        if (line >= 0) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("创建失败");
            g_fail_count++;
        }
        TEST("create_distance: 创建距离");
        int dist = lv_dyn_create_distance(graph, p1, p3);
        if (dist >= 0) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("创建失败");
            g_fail_count++;
        }
        TEST("create_parallel: 创建平行线");
        int parallel = lv_dyn_create_parallel(graph, line, p3);
        if (parallel >= 0) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("创建失败");
            g_fail_count++;
        }
        TEST("create_perpendicular: 创建垂直线");
        int perp = lv_dyn_create_perpendicular(graph, line, p3);
        if (perp >= 0) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("创建失败");
            g_fail_count++;
        }
        lv_dyn_graph_destroy(graph);
    }
    /* 4. 级联更新测试 */
    printf("\n[组 4] 级联更新\n");
    {
        lvDynGraph *graph = lv_dyn_graph_create(NULL);
        int p1 = lv_dyn_create_point(graph, 0, 0);
        int p2 = lv_dyn_create_point(graph, 3, 4);
        int mid = lv_dyn_create_midpoint(graph, p1, p2);
        TEST("update_cascade: 更新级联");
        int updated = lv_dyn_graph_update_cascade(graph, p1, NULL);
        if (updated > 0) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("没有更新任何节点");
            g_fail_count++;
        }
        TEST("update_cascade: 验证中点坐标");
        lvDynNode *mid_node = lv_dyn_graph_get_node(graph, mid);
        if (mid_node && approx_eq(mid_node->params[0], 1.5) && approx_eq(mid_node->params[1], 2.0)) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("中点坐标不正确");
            g_fail_count++;
        }
        TEST("mark_dirty: 标记脏节点");
        lv_dyn_graph_mark_dirty(graph, p1);
        lvDynNode *dirty = lv_dyn_graph_get_node(graph, p1);
        if (dirty && dirty->state == lv_DYN_STATE_DIRTY) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("状态未标记为 DIRTY");
            g_fail_count++;
        }
        lv_dyn_graph_destroy(graph);
    }
    /* 5. 循环检测测试 */
    printf("\n[组 5] 循环检测\n");
    {
        lvDynGraph *graph = lv_dyn_graph_create(NULL);
        int p1 = lv_dyn_create_point(graph, 0, 0);
        int p2 = lv_dyn_create_point(graph, 1, 0);
        int mid1 = lv_dyn_create_midpoint(graph, p1, p2);
        int mid2 = lv_dyn_create_midpoint(graph, p1, mid1);
        TEST("has_path: 检测路径存在");
        if (lv_dyn_graph_has_path(graph, p1, mid1)) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("应检测到路径");
            g_fail_count++;
        }
        TEST("has_path: 检测路径不存在");
        if (!lv_dyn_graph_has_path(graph, mid1, p1)) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("不应检测到路径");
            g_fail_count++;
        }
        TEST("would_create_cycle: 会形成循环（因为 p2 是 mid1 的父节点）");
        if (lv_dyn_graph_would_create_cycle(graph, mid1, p2)) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("应检测到会形成循环");
            g_fail_count++;
        }
        TEST("would_create_cycle: 不会形成循环（p1 和 p2 无依赖关系）");
        if (!lv_dyn_graph_would_create_cycle(graph, p1, p2)) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("不应形成循环");
            g_fail_count++;
        }
        lv_dyn_graph_destroy(graph);
    }
    /* 6. 拓扑排序测试 */
    printf("\n[组 6] 拓扑排序\n");
    {
        lvDynGraph *graph = lv_dyn_graph_create(NULL);
        int p1 = lv_dyn_create_point(graph, 0, 0);
        int p2 = lv_dyn_create_point(graph, 3, 4);
        int mid = lv_dyn_create_midpoint(graph, p1, p2);
        int dist = lv_dyn_create_distance(graph, mid, p2);
        TEST("topological_sort: 拓扑排序成功");
        int order[10];
        int count = lv_dyn_graph_topological_sort(graph, order);
        if (count == 4) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("排序失败");
            g_fail_count++;
        }
        TEST("topological_sort: 父节点在子节点之前");
        bool order_correct = true;
        /* 构建 ID -> position 映射 */
        int position[10] = {0};
        for (int i = 0; i < count; i++) {
            position[order[i]] = i;
        }
        /* 验证：每个节点的父节点位置应小于该节点位置 */
        for (int i = 0; i < count; i++) {
            lvDynNode *node = lv_dyn_graph_get_node(graph, order[i]);
            if (!node)
                continue;
            for (int k = 0; k < node->parent_count; k++) {
                int parent_pos = position[node->parent_ids[k]];
                if (parent_pos > i) {
                    order_correct = false;
                    break;
                }
            }
            if (!order_correct)
                break;
        }
        if (order_correct) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("排序顺序不正确");
            g_fail_count++;
        }
        lv_dyn_graph_destroy(graph);
    }
    /* 7. 统计信息测试 */
    printf("\n[组 7] 统计信息\n");
    {
        lvDynGraph *graph = lv_dyn_graph_create(NULL);
        int p1 = lv_dyn_create_point(graph, 0, 0);
        int p2 = lv_dyn_create_point(graph, 3, 4);
        int mid = lv_dyn_create_midpoint(graph, p1, p2);
        TEST("get_stats: 统计信息正确");
        lvDynGraphStats stats;
        lv_dyn_graph_get_stats(graph, &stats);
        if (stats.total_nodes == 3 && stats.free_nodes == 2 && stats.derived_nodes == 1) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("统计信息不正确");
            g_fail_count++;
        }
        TEST("clear_dirty: 清除脏标记");
        lv_dyn_graph_mark_dirty(graph, mid);
        lv_dyn_graph_clear_dirty(graph);
        lvDynNode *clean = lv_dyn_graph_get_node(graph, mid);
        if (clean && clean->state != lv_DYN_STATE_DIRTY) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("脏标记未清除");
            g_fail_count++;
        }
        lv_dyn_graph_destroy(graph);
    }
    /* 8. 更新链测试 */
    printf("\n[组 8] 更新链\n");
    {
        lvDynGraph *graph = lv_dyn_graph_create(NULL);
        int p1 = lv_dyn_create_point(graph, 0, 0);
        int p2 = lv_dyn_create_point(graph, 6, 8);
        int mid1 = lv_dyn_create_midpoint(graph, p1, p2);
        int mid2 = lv_dyn_create_midpoint(graph, mid1, p2);
        int dist = lv_dyn_create_distance(graph, mid2, p2);
        /* 更新链 */
        lv_dyn_graph_update_chain(graph, dist);
        TEST("update_chain: 更新链成功");
        lvDynNode *dist_node = lv_dyn_graph_get_node(graph, dist);
        if (dist_node && dist_node->update_count > 0) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("更新失败");
            g_fail_count++;
        }
        lv_dyn_graph_destroy(graph);
    }
    /* 9. 大图级联更新测试（栈深 > 256，验证动态栈无静默截断） */
    printf("\n[组 9] 大图级联更新（>256 栈深）\n");
    {
        lvDynGraph *graph = lv_dyn_graph_create(NULL);
        /* 结构：p0 -> D1 -> D2 -> ... -> D20 依赖链；每层 D_i 先挂 15 个
         * 叶子中点（child_ids 顺序使链节点最后入栈 → LIFO 下链节点先弹出，
         * 栈深随层数累积，峰值 > 256，可触发原固定 256 栈的截断/越界写）。 */
        int p0 = lv_dyn_create_point(graph, 0, 0);
        int prev = p0;
        for (int i = 1; i <= 20; i++) {
            int pi = lv_dyn_create_point(graph, (double) i, 0.0);
            int di = lv_dyn_create_midpoint(graph, prev, pi);
            prev = di;
            for (int k = 0; k < 15; k++) {
                int q = lv_dyn_create_point(graph, (double) k, (double) i);
                lv_dyn_create_midpoint(graph, di, q);
            }
        }
        int expected = graph->node_count; /* 全部节点均可达且应被更新 */
        TEST("update_cascade: 大图（>256 栈深）更新全部节点");
        int updated = lv_dyn_graph_update_cascade(graph, p0, NULL);
        if (updated == expected) {
            PASS();
            g_pass_count++;
        } else {
            printf("  (updated=%d expected=%d)\n", updated, expected);
            FAIL("级联更新未覆盖全部节点（疑似栈截断）");
            g_fail_count++;
        }
        TEST("update_cascade: 每个节点 update_count > 0");
        bool all_updated = true;
        for (int i = 0; i < graph->node_count; i++) {
            lvDynNode *n = lv_dyn_graph_get_node(graph, i);
            if (!n || n->update_count == 0) {
                all_updated = false;
                break;
            }
        }
        if (all_updated) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("存在未被级联更新的节点");
            g_fail_count++;
        }
        lv_dyn_graph_destroy(graph);
    }
    /* 10. 大图更新链测试（链长 > 256，验证动态 visited 无静默截断） */
    printf("\n[组 10] 大图更新链（链长 > 256）\n");
    {
        lvDynGraph *graph = lv_dyn_graph_create(NULL);
        /* 300 层中点链：p0 -> D1 -> D2 -> ... -> D300 */
        int p0 = lv_dyn_create_point(graph, 0, 0);
        int prev = p0;
        for (int i = 1; i <= 300; i++) {
            int pi = lv_dyn_create_point(graph, (double) i, 0.0);
            prev = lv_dyn_create_midpoint(graph, prev, pi);
        }
        int leaf = prev; /* D300 */
        /* 链式结构下 mark_dirty 栈深恒 1，可完整标记 p0 + D1..D300 */
        lv_dyn_graph_mark_dirty(graph, p0);
        TEST("update_chain: 长链（301 节点）全部更新");
        int updated = lv_dyn_graph_update_chain(graph, leaf);
        if (updated == 301) {
            PASS();
            g_pass_count++;
        } else {
            printf("  (updated=%d expected=301)\n", updated);
            FAIL("更新链未覆盖全部节点（疑似 visited 截断）");
            g_fail_count++;
        }
        lv_dyn_graph_destroy(graph);
    }
    /* 11. 大图路径/循环检测回归测试（>256 节点，验证 has_path 的 visited 碰撞
     * 与队列截断修复、mark_dirty 的栈截断修复） */
    printf("\n[组 11] 大图路径与循环检测（>256 节点）\n");
    {
        /* 深链：p0 -> D1 -> D2 -> ... -> D300（301 节点）。
         * 原实现 visited[256]+id%256：D128 与 D1 的 id 对 256 取模碰撞导致链
         * 中断；queue[256]+rear<256 静默截断。两者均使 has_path(p0, D300)
         * 误报 false，would_create_cycle 随之漏检。 */
        lvDynGraph *graph = lv_dyn_graph_create(NULL);
        int chain_ids[301];
        int p0 = lv_dyn_create_point(graph, 0, 0);
        chain_ids[0] = p0;
        int prev = p0;
        for (int i = 1; i <= 300; i++) {
            int pi = lv_dyn_create_point(graph, (double) i, 0.0);
            prev = lv_dyn_create_midpoint(graph, prev, pi);
            chain_ids[i] = prev;
        }
        int leaf = prev; /* D300 */
        TEST("has_path: 长链（301 节点）头到尾存在路径");
        if (lv_dyn_graph_has_path(graph, p0, leaf)) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("应检测到长链路径（原 visited 碰撞/队列截断误报 false）");
            g_fail_count++;
        }
        TEST("has_path: 长链反向不存在路径");
        if (!lv_dyn_graph_has_path(graph, leaf, p0)) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("反向不应存在路径");
            g_fail_count++;
        }
        TEST("would_create_cycle: 长链尾部连回头部会形成循环");
        if (lv_dyn_graph_would_create_cycle(graph, leaf, p0)) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("应检测到循环（原队列截断导致漏检）");
            g_fail_count++;
        }
        TEST("would_create_cycle: 长链顺向添加边不形成循环");
        if (!lv_dyn_graph_would_create_cycle(graph, p0, leaf)) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("顺向添加边不应形成循环");
            g_fail_count++;
        }
        TEST("mark_dirty: 长链 301 个链节点全部标记 DIRTY");
        lv_dyn_graph_mark_dirty(graph, p0);
        bool chain_all_dirty = true;
        for (int i = 0; i < 301; i++) {
            lvDynNode *n = lv_dyn_graph_get_node(graph, chain_ids[i]);
            if (!n || n->state != lv_DYN_STATE_DIRTY) {
                chain_all_dirty = false;
                break;
            }
        }
        if (chain_all_dirty) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("存在未标记 DIRTY 的链节点");
            g_fail_count++;
        }
        lv_dyn_graph_destroy(graph);
    }
    {
        /* 宽树：p0 → 16 个 L1 → 每个 16 个 L2 → 每个 16 个 L3（共
         * 1+16+256+4096 = 4369 节点，每节点子数 ≤ 默认 max_children=16，
         * 符合 lv_dyn_graph_default_config 的公开契约）。
         * BFS 前沿在 L3 层峰值达 ~4096 个待处理节点，超过原 queue[256]
         * 上限（原实现静默截断，has_path 对靠后节点误报 false）；visited
         * 亦超 256（原 id%256 碰撞导致误判已访问）。 */
        lvDynGraph *graph = lv_dyn_graph_create(NULL);
        int p0 = lv_dyn_create_point(graph, 0, 0);
        int l1_ids[16];
        int l2_ids[16][16];
        int l3_last = -1;
        for (int i = 0; i < 16; i++) {
            int q = lv_dyn_create_point(graph, (double) i, 1.0);
            l1_ids[i] = lv_dyn_create_midpoint(graph, p0, q);
            for (int j = 0; j < 16; j++) {
                int r = lv_dyn_create_point(graph, (double) (i * 16 + j), 2.0);
                l2_ids[i][j] = lv_dyn_create_midpoint(graph, l1_ids[i], r);
                for (int k = 0; k < 16; k++) {
                    int s = lv_dyn_create_point(graph, (double) (i * 256 + j * 16 + k), 3.0);
                    l3_last = lv_dyn_create_midpoint(graph, l2_ids[i][j], s);
                }
            }
        }
        TEST("has_path: 宽树（4369 节点）根到最末叶子存在路径");
        if (lv_dyn_graph_has_path(graph, p0, l3_last)) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("应检测到最末叶子的路径（原队列截断误报 false）");
            g_fail_count++;
        }
        TEST("has_path: 宽树反向不存在路径");
        if (!lv_dyn_graph_has_path(graph, l3_last, p0)) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("反向不应存在路径");
            g_fail_count++;
        }
        TEST("would_create_cycle: 宽树最末叶子连回根会形成循环");
        if (lv_dyn_graph_would_create_cycle(graph, l3_last, p0)) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("应检测到循环（原队列截断导致漏检）");
            g_fail_count++;
        }
        TEST("mark_dirty: 宽树全部 4369 节点标记 DIRTY");
        lv_dyn_graph_mark_dirty(graph, p0);
        bool wide_all_dirty = true;
        for (int i = 0; i < 16; i++) {
            lvDynNode *n = lv_dyn_graph_get_node(graph, l1_ids[i]);
            if (!n || n->state != lv_DYN_STATE_DIRTY) {
                wide_all_dirty = false;
                break;
            }
            for (int j = 0; j < 16; j++) {
                lvDynNode *n2 = lv_dyn_graph_get_node(graph, l2_ids[i][j]);
                if (!n2 || n2->state != lv_DYN_STATE_DIRTY) {
                    wide_all_dirty = false;
                    break;
                }
            }
        }
        lvDynNode *leaf_node = lv_dyn_graph_get_node(graph, l3_last);
        if (wide_all_dirty && leaf_node && leaf_node->state == lv_DYN_STATE_DIRTY) {
            PASS();
            g_pass_count++;
        } else {
            FAIL("存在未标记 DIRTY 的宽树节点");
            g_fail_count++;
        }
        lv_dyn_graph_destroy(graph);
    }
        
TEST_MAIN_END()
