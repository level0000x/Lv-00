/**
 * @file lv_test_geom_graph_builder.h
 * @brief 测试用几何约束图构造器
 *
 * 提供常用几何图形的约束图快捷构造函数（线段、三角形、四边形、圆），
 * 内部复用 test_helpers.h 的 add_point() 与 ConstraintGraph 的
 * graph_add_point / graph_add_line_segment / graph_add_incidence /
 * graph_add_containment 等 API，统一采用整数坐标（内部以分母为 1 的
 * 有理数精确表示），避免各测试文件重复手写图构造代码。
 *
 * 所有函数均可传入 NULL 以自动创建新图，也可传入已有图在其上追加
 * 节点/约束；成功返回图指针，失败返回 NULL。
 *
 * 圆图的建模方式与 test/examples/circle_intersection.c 一致：
 * 圆心 O(cx, cy) + 半径点 R(cx + r, cy)，通过 graph_add_containment
 * 表示"半径点在以圆心为心、|OR| 为半径的圆上"。
 */
#ifndef LV_TEST_GEOM_GRAPH_BUILDER_H
#define LV_TEST_GEOM_GRAPH_BUILDER_H

#include <stdbool.h>

#include "lv.h"
#include "test_helpers.h"

/**
 * @brief 构造线段图：两个端点 + 一条线段（可选 incidence 关联）
 *
 * @param g              约束图（NULL 则自动创建）
 * @param x0, y0         端点 1 坐标（整数）
 * @param x1, y1         端点 2 坐标（整数）
 * @param with_incidence 是否添加两条 incidence 约束（端点在线段上）
 * @return 图指针；失败返回 NULL
 */
static inline ConstraintGraph *lv_test_line_graph(ConstraintGraph *g, int64_t x0, int64_t y0, int64_t x1, int64_t y1,
                                                  bool with_incidence) {
    if (g == NULL)
        g = graph_create();
    if (g == NULL)
        return NULL;

    int p0 = add_point(g, x0, 1, y0, 1);
    int p1 = add_point(g, x1, 1, y1, 1);
    if (p0 < 0 || p1 < 0)
        return NULL;

    if (graph_add_line_segment(g, p0, p1) != ADD_NODE_OK)
        return NULL;

    if (with_incidence) {
        int s = graph_get_last_added_node_id(g);
        if (s < 0)
            return NULL;
        graph_add_incidence(g, p0, s);
        graph_add_incidence(g, p1, s);
    }
    return g;
}

/**
 * @brief 构造三角形图：三个端点 + 三条线段（可选 incidence 关联）
 *
 * 三条边按 (p0,p1)、(p1,p2)、(p2,p0) 顺序添加。
 *
 * @param g              约束图（NULL 则自动创建）
 * @param x0..y2         三个顶点坐标（整数）
 * @param with_incidence 是否添加 incidence 约束（每条边的端点在边上）
 * @return 图指针；失败返回 NULL
 */
static inline ConstraintGraph *lv_test_triangle_graph(ConstraintGraph *g, int64_t x0, int64_t y0, int64_t x1,
                                                      int64_t y1, int64_t x2, int64_t y2, bool with_incidence) {
    if (g == NULL)
        g = graph_create();
    if (g == NULL)
        return NULL;

    int p0 = add_point(g, x0, 1, y0, 1);
    int p1 = add_point(g, x1, 1, y1, 1);
    int p2 = add_point(g, x2, 1, y2, 1);
    if (p0 < 0 || p1 < 0 || p2 < 0)
        return NULL;

    int segs[3];
    if (graph_add_line_segment(g, p0, p1) != ADD_NODE_OK)
        return NULL;
    segs[0] = graph_get_last_added_node_id(g);
    if (graph_add_line_segment(g, p1, p2) != ADD_NODE_OK)
        return NULL;
    segs[1] = graph_get_last_added_node_id(g);
    if (graph_add_line_segment(g, p2, p0) != ADD_NODE_OK)
        return NULL;
    segs[2] = graph_get_last_added_node_id(g);
    if (segs[0] < 0 || segs[1] < 0 || segs[2] < 0)
        return NULL;

    if (with_incidence) {
        int pts[3] = {p0, p1, p2};
        int edges[3][2] = {{0, 1}, {1, 2}, {2, 0}};
        for (int i = 0; i < 3; i++) {
            graph_add_incidence(g, pts[edges[i][0]], segs[i]);
            graph_add_incidence(g, pts[edges[i][1]], segs[i]);
        }
    }
    return g;
}

/**
 * @brief 构造四边形图：四个端点 + 四条线段（可选 incidence 关联）
 *
 * 四条边按 (p0,p1)、(p1,p2)、(p2,p3)、(p3,p0) 顺序添加。
 *
 * @param g              约束图（NULL 则自动创建）
 * @param x0..y3         四个顶点坐标（整数）
 * @param with_incidence 是否添加 incidence 约束（每条边的端点在边上）
 * @return 图指针；失败返回 NULL
 */
static inline ConstraintGraph *lv_test_quad_graph(ConstraintGraph *g, int64_t x0, int64_t y0, int64_t x1, int64_t y1,
                                                  int64_t x2, int64_t y2, int64_t x3, int64_t y3,
                                                  bool with_incidence) {
    if (g == NULL)
        g = graph_create();
    if (g == NULL)
        return NULL;

    int p0 = add_point(g, x0, 1, y0, 1);
    int p1 = add_point(g, x1, 1, y1, 1);
    int p2 = add_point(g, x2, 1, y2, 1);
    int p3 = add_point(g, x3, 1, y3, 1);
    if (p0 < 0 || p1 < 0 || p2 < 0 || p3 < 0)
        return NULL;

    int segs[4];
    if (graph_add_line_segment(g, p0, p1) != ADD_NODE_OK)
        return NULL;
    segs[0] = graph_get_last_added_node_id(g);
    if (graph_add_line_segment(g, p1, p2) != ADD_NODE_OK)
        return NULL;
    segs[1] = graph_get_last_added_node_id(g);
    if (graph_add_line_segment(g, p2, p3) != ADD_NODE_OK)
        return NULL;
    segs[2] = graph_get_last_added_node_id(g);
    if (graph_add_line_segment(g, p3, p0) != ADD_NODE_OK)
        return NULL;
    segs[3] = graph_get_last_added_node_id(g);
    if (segs[0] < 0 || segs[1] < 0 || segs[2] < 0 || segs[3] < 0)
        return NULL;

    if (with_incidence) {
        int pts[4] = {p0, p1, p2, p3};
        int edges[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
        for (int i = 0; i < 4; i++) {
            graph_add_incidence(g, pts[edges[i][0]], segs[i]);
            graph_add_incidence(g, pts[edges[i][1]], segs[i]);
        }
    }
    return g;
}

/**
 * @brief 构造圆图：圆心 + 半径点 + 包含约束
 *
 * 以圆心 O(cx, cy) 与半径点 R(cx + r, cy) 定义圆，通过
 * graph_add_containment(center, radius_point) 表示"R 在以 O 为心、
 * |OR| 为半径的圆上"。
 *
 * @param g       约束图（NULL 则自动创建）
 * @param cx, cy  圆心坐标（整数）
 * @param r       半径（整数，不允许为 0）
 * @return 图指针；失败返回 NULL
 */
static inline ConstraintGraph *lv_test_circle_graph(ConstraintGraph *g, int64_t cx, int64_t cy, int64_t r) {
    if (g == NULL)
        g = graph_create();
    if (g == NULL)
        return NULL;

    int center = add_point(g, cx, 1, cy, 1);
    int radius_point = add_point(g, cx + r, 1, cy, 1);
    if (center < 0 || radius_point < 0)
        return NULL;

    if (graph_add_containment(g, center, radius_point) != ADD_CONSTRAINT_OK)
        return NULL;
    return g;
}

#endif /* LV_TEST_GEOM_GRAPH_BUILDER_H */
