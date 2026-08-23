/**
 * @file test_geo_dynamic_ext.c
 * @brief 动态几何图契约测试（批次 C-㊺续34：geo_dynamic.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（6 个真实函数；lv_DYN_INVALID 为宏，经返回值覆盖）：
 *   lv_dyn_graph_add_node / remove_node / update_all / reset_states
 *   lv_dyn_create_circle / lv_geo_dynamic_step
 *
 * 契约要点（与 geo_dynamic.c 核对）：
 *   - add_node：graph NULL 或满 → lv_DYN_INVALID(-1)；成功返回新节点 id。
 *   - create_circle：以 center/point 为父节点创建圆节点。
 *   - remove_node：不存在节点/NULL → false；成功 true。
 *   - update_all：NULL → 0；更新 DIRTY 根节点的级联；初始无 DIRTY → 0。
 *   - reset_states：清空节点状态（NULL 安全）。
 *   - geo_dynamic_step：x += vx*dt, y += vy*dt（Euler）；NULL/空/dt<=0 无操作。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv/geo_dynamic.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#define TOL 1e-12

/* ============== 测试：节点操作 ============== */

static void test_node_ops(void) {
    lvDynGraph *graph = lv_dyn_graph_create(NULL);
    TEST_ASSERT_NOT_NULL(graph);

    /* 添加两个点节点 */
    double params[2] = {1.0, 2.0};
    int p0 = lv_dyn_graph_add_node(graph, lv_DYN_NODE_POINT, NULL, 0, params, 2);
    int p1 = lv_dyn_graph_add_node(graph, lv_DYN_NODE_POINT, NULL, 0, params, 2);
    TEST_ASSERT_EQ(p0, 0);
    TEST_ASSERT_EQ(p1, 1);

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_dyn_graph_add_node(NULL, lv_DYN_NODE_POINT, NULL, 0, params, 2), lv_DYN_INVALID);

    /* 圆节点（以两个点为父） */
    int c0 = lv_dyn_create_circle(graph, p0, p1);
    TEST_ASSERT_EQ(c0, 2);

    /* 节点类型/父关系 */
    lvDynNode *cn = lv_dyn_graph_get_node(graph, c0);
    TEST_ASSERT_NOT_NULL(cn);
    TEST_ASSERT_EQ((int) cn->type, (int) lv_DYN_NODE_CIRCLE);
    TEST_ASSERT_EQ(cn->parent_count, 2);

    /* 删除 */
    TEST_ASSERT(lv_dyn_graph_remove_node(graph, p0), "remove p0");
    TEST_ASSERT(!lv_dyn_graph_remove_node(graph, 999), "remove missing");
    TEST_ASSERT(!lv_dyn_graph_remove_node(NULL, 0), "remove NULL graph");

    lv_dyn_graph_destroy(graph);
}

/* ============== 测试：更新与状态 ============== */

static void test_update_reset(void) {
    lvDynGraph *graph = lv_dyn_graph_create(NULL);
    TEST_ASSERT_NOT_NULL(graph);

    double params[2] = {0.0, 0.0};
    int p0 = lv_dyn_graph_add_node(graph, lv_DYN_NODE_POINT, NULL, 0, params, 2);
    TEST_ASSERT(p0 >= 0, "point added");

    /* 初始无 DIRTY：update_all 返回 0 */
    TEST_ASSERT_EQ(lv_dyn_graph_update_all(graph), 0);
    TEST_ASSERT_EQ(lv_dyn_graph_update_all(NULL), 0);

    /* 标记 DIRTY 后 reset_states 恢复 */
    lv_dyn_graph_mark_dirty(graph, p0);
    lvDynNode *n = lv_dyn_graph_get_node(graph, p0);
    TEST_ASSERT_EQ((int) n->state, (int) lv_DYN_STATE_DIRTY);
    lv_dyn_graph_reset_states(graph);
    TEST_ASSERT_EQ((int) n->state, (int) lv_DYN_STATE_VALID);
    lv_dyn_graph_reset_states(NULL);

    lv_dyn_graph_destroy(graph);
}

/* ============== 测试：物理步进 ============== */

static void test_dynamic_step(void) {
    lvDynamicPoint pts[2];
    pts[0].x = 0.0;
    pts[0].y = 0.0;
    pts[0].vx = 2.0;
    pts[0].vy = 3.0;
    pts[1].x = 10.0;
    pts[1].y = 20.0;
    pts[1].vx = 0.0;
    pts[1].vy = -1.0;

    lv_geo_dynamic_step(pts, 2, 0.5);
    TEST_ASSERT_DOUBLE(pts[0].x, 1.0, TOL);
    TEST_ASSERT_DOUBLE(pts[0].y, 1.5, TOL);
    TEST_ASSERT_DOUBLE(pts[1].x, 10.0, TOL);
    TEST_ASSERT_DOUBLE(pts[1].y, 19.5, TOL);

    /* NULL/空/dt<=0 无操作 */
    lv_geo_dynamic_step(NULL, 2, 0.5);
    lv_geo_dynamic_step(pts, 0, 0.5);
    lv_geo_dynamic_step(pts, 2, 0.0);
    TEST_ASSERT_DOUBLE(pts[0].x, 1.0, TOL);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("GeoDynamicExt")

    printf("\n--- geo_dynamic (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_node_ops);
    TEST_MAIN_RUN(test_update_reset);
    TEST_MAIN_RUN(test_dynamic_step);

TEST_MAIN_END()
