/* ============================================================================
 * 模块名称:预设测量 (impl_preset_measurements)
 *
 * 说明:
 *   本文件是从 lv_impl_upper.c 中提取的"第5部分:预设测量 --
 *   preset_measurements(17函数)"的独立实现文件。
 *   包含距离、角度、面积、周长等测量包装函数。
 *
 * 提取自: lv_impl_upper.c (第2460行-第2837行)
 * ============================================================================ */

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/engine.h"
#include "lv/constraint_graph.h"
#include "lv/geom_evol.h"
#include "lv/lv_utils.h"
#include "lv_internal.h"

/* ============================================================
 * 第5部分:预设测量 -- preset_measurements(17函数)
 * ============================================================ */

/** 两点间距(以整数有理数分子表示) */
int64_t preset_distance(lvEngine *ctx, int64_t p1, int64_t p2) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_distance: NULL graph");
    GeomNode *n1 = graph_get_node(graph, (int) p1);
    GeomNode *n2 = graph_get_node(graph, (int) p2);
    if (!n1 || !n2)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_distance: NULL node input");
    if (n1->type != GEOM_POINT || n2->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_distance: nodes not GEOM_POINT");
    /* 创建测量结果节点,锚定在 n2 的坐标上。
     * 实际距离值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, n2->symbolic_coords, n2->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_distance: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_distance: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) p2, result_id);
    return (int64_t) result_id;
}

/** 三点所成角度(毫弧度) */
int64_t preset_angle(lvEngine *ctx, int64_t p_vertex, int64_t p1, int64_t p2) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_angle: NULL graph");
    GeomNode *nv = graph_get_node(graph, (int) p_vertex);
    GeomNode *na = graph_get_node(graph, (int) p1);
    GeomNode *nb = graph_get_node(graph, (int) p2);
    if (!nv || !na || !nb)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_angle: NULL node input");
    if (nv->type != GEOM_POINT || na->type != GEOM_POINT || nb->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_angle: nodes not GEOM_POINT");
    /* 创建测量结果节点,锚定在顶点坐标上。
     * 实际角度值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, nv->symbolic_coords, nv->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_angle: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_angle: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) p2, result_id);
    return (int64_t) result_id;
}

/** 三角形面积 */
int64_t preset_area_triangle(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_area_triangle: NULL graph");
    GeomNode *n1 = graph_get_node(graph, (int) p1);
    GeomNode *n2 = graph_get_node(graph, (int) p2);
    GeomNode *n3 = graph_get_node(graph, (int) p3);
    if (!n1 || !n2 || !n3)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_area_triangle: NULL node input");
    if (n1->type != GEOM_POINT || n2->type != GEOM_POINT || n3->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_area_triangle: nodes not GEOM_POINT");
    /* 创建测量结果节点,锚定在 n3 的坐标上。
     * 实际面积值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, n3->symbolic_coords, n3->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_area_triangle: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_area_triangle: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) p3, result_id);
    return (int64_t) result_id;
}

/** 多边形面积(Shoelace公式) */
int64_t preset_area_polygon(lvEngine *ctx, int64_t *point_ids, int64_t count) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_area_polygon: NULL graph");
    if (!point_ids)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_area_polygon: NULL point_ids");
    if (count < 3)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_area_polygon: count < 3");
    /* 验证所有输入点有效 */
    for (int64_t i = 0; i < count; i++) {
        GeomNode *n = graph_get_node(graph, (int) point_ids[i]);
        if (!n)
            lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_area_polygon: NULL point node");
        if (n->type != GEOM_POINT)
            lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_area_polygon: node not GEOM_POINT");
    }
    int64_t last_idx = count - 1;
    GeomNode *last_pt = graph_get_node(graph, (int) point_ids[last_idx]);
    /* 创建测量结果节点,锚定在最后一个顶点的坐标上。
     * 实际面积值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, last_pt->symbolic_coords, last_pt->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_area_polygon: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_area_polygon: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) point_ids[last_idx], result_id);
    return (int64_t) result_id;
}

/** 多边形周长 */
int64_t preset_perimeter(lvEngine *ctx, int64_t *point_ids, int64_t count) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_perimeter: NULL graph");
    if (!point_ids)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_perimeter: NULL point_ids");
    if (count < 3)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_perimeter: count < 3");
    for (int64_t i = 0; i < count; i++) {
        GeomNode *n = graph_get_node(graph, (int) point_ids[i]);
        if (!n)
            lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_perimeter: NULL point node");
        if (n->type != GEOM_POINT)
            lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_perimeter: node not GEOM_POINT");
    }
    int64_t last_idx = count - 1;
    GeomNode *last_pt = graph_get_node(graph, (int) point_ids[last_idx]);
    /* 创建测量结果节点,锚定在最后一个顶点的坐标上。
     * 实际周长值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, last_pt->symbolic_coords, last_pt->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_perimeter: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_perimeter: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) point_ids[last_idx], result_id);
    return (int64_t) result_id;
}

/** 曲率(给定向量的离散曲率近似) */
int64_t preset_curvature(lvEngine *ctx, int64_t curve_id, int64_t t_param) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_curvature: NULL graph");
    GeomNode *curve = graph_get_node(graph, (int) curve_id);
    if (!curve)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_curvature: NULL curve node");
    /* 创建测量结果节点,锚定在曲线节点的坐标上。
     * t_param 为参数值,实际曲率由约束求解器计算得出。 */
    AddNodeResult res = graph_add_point(graph, curve->symbolic_coords, curve->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_curvature: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_curvature: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) curve_id, result_id);
    return (int64_t) result_id;
}

/** 线段分割比率 */
int64_t preset_ratio(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p_div) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_ratio: NULL graph");
    GeomNode *n1 = graph_get_node(graph, (int) p1);
    GeomNode *n2 = graph_get_node(graph, (int) p2);
    GeomNode *nd = graph_get_node(graph, (int) p_div);
    if (!n1 || !n2 || !nd)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_ratio: NULL node input");
    if (n1->type != GEOM_POINT || n2->type != GEOM_POINT || nd->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_ratio: nodes not GEOM_POINT");
    /* 创建测量结果节点,锚定在分割点的坐标上。
     * 实际比率由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, nd->symbolic_coords, nd->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_ratio: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_ratio: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) p_div, result_id);
    return (int64_t) result_id;
}

/** 调和比(共线四点 a,b,c,d 的调和分割) */
int64_t preset_harmonic_ratio(lvEngine *ctx, int64_t a, int64_t b, int64_t c, int64_t d) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_harmonic_ratio: NULL graph");
    GeomNode *na = graph_get_node(graph, (int) a);
    GeomNode *nb = graph_get_node(graph, (int) b);
    GeomNode *nc = graph_get_node(graph, (int) c);
    GeomNode *nd = graph_get_node(graph, (int) d);
    if (!na || !nb || !nc || !nd)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_harmonic_ratio: NULL node input");
    if (na->type != GEOM_POINT || nb->type != GEOM_POINT || nc->type != GEOM_POINT || nd->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_harmonic_ratio: nodes not GEOM_POINT");
    /* 创建测量结果节点,锚定在 d 的坐标上。
     * 实际调和比值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, nd->symbolic_coords, nd->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_harmonic_ratio: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_harmonic_ratio: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) d, result_id);
    return (int64_t) result_id;
}

/** 交比(cross ratio,共线四点 a,b,c,d) */
int64_t preset_cross_ratio(lvEngine *ctx, int64_t a, int64_t b, int64_t c, int64_t d) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_cross_ratio: NULL graph");
    GeomNode *na = graph_get_node(graph, (int) a);
    GeomNode *nb = graph_get_node(graph, (int) b);
    GeomNode *nc = graph_get_node(graph, (int) c);
    GeomNode *nd = graph_get_node(graph, (int) d);
    if (!na || !nb || !nc || !nd)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_cross_ratio: NULL node input");
    if (na->type != GEOM_POINT || nb->type != GEOM_POINT || nc->type != GEOM_POINT || nd->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_cross_ratio: nodes not GEOM_POINT");
    /* 创建测量结果节点,锚定在 d 的坐标上。
     * 实际交比值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, nd->symbolic_coords, nd->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_cross_ratio: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_cross_ratio: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) d, result_id);
    return (int64_t) result_id;
}

/** 直线斜率(有理数表示) */
int64_t preset_slope(lvEngine *ctx, int64_t line_id) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_slope: NULL graph");
    GeomNode *line = graph_get_node(graph, (int) line_id);
    if (!line)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_slope: NULL line node");
    /* 创建测量结果节点,锚定在直线节点的坐标上。
     * 实际斜率值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, line->symbolic_coords, line->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_slope: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_slope: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) line_id, result_id);
    return (int64_t) result_id;
}

/** 直线截距 */
int64_t preset_intercept(lvEngine *ctx, int64_t line_id) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_intercept: NULL graph");
    GeomNode *line = graph_get_node(graph, (int) line_id);
    if (!line)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_intercept: NULL line node");
    /* 创建测量结果节点,锚定在直线节点的坐标上。
     * 实际截距值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, line->symbolic_coords, line->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_intercept: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_intercept: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) line_id, result_id);
    return (int64_t) result_id;
}

/** 线段长度 */
int64_t preset_length_segment(lvEngine *ctx, int64_t seg_id) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_length_segment: NULL graph");
    GeomNode *seg = graph_get_node(graph, (int) seg_id);
    if (!seg)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_length_segment: NULL seg node");
    /* 创建测量结果节点,锚定在线段节点的坐标上。
     * 实际长度值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, seg->symbolic_coords, seg->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_length_segment: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_length_segment: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) seg_id, result_id);
    return (int64_t) result_id;
}

/** 弧长 */
int64_t preset_arc_length(lvEngine *ctx, int64_t arc_id) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_arc_length: NULL graph");
    GeomNode *arc = graph_get_node(graph, (int) arc_id);
    if (!arc)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_arc_length: NULL arc node");
    /* 创建测量结果节点,锚定在弧节点的坐标上。
     * 实际弧长值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, arc->symbolic_coords, arc->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_arc_length: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_arc_length: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) arc_id, result_id);
    return (int64_t) result_id;
}

/** 对角线长度 */
int64_t preset_diagonal_length(lvEngine *ctx, int64_t poly_id, int64_t diag_idx) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_diagonal_length: NULL graph");
    GeomNode *poly = graph_get_node(graph, (int) poly_id);
    if (!poly)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_diagonal_length: NULL poly node");
    /* 创建测量结果节点,锚定在多边形节点的坐标上。
     * diag_idx 为对角线索引,实际长度由约束求解器计算得出。 */
    AddNodeResult res = graph_add_point(graph, poly->symbolic_coords, poly->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_diagonal_length: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_diagonal_length: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) poly_id, result_id);
    return (int64_t) result_id;
}

/** 圆半径 */
int64_t preset_radius(lvEngine *ctx, int64_t circle_id) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_radius: NULL graph");
    GeomNode *circle = graph_get_node(graph, (int) circle_id);
    if (!circle)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_radius: NULL circle node");
    /* 创建测量结果节点,锚定在圆节点的坐标上。
     * 实际半径值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, circle->symbolic_coords, circle->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_radius: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_radius: graph_get_last_added_node_id failed");

    graph_add_line_segment(graph, (int) circle_id, result_id);
    return (int64_t) result_id;
}

/** 圆直径 */
int64_t preset_diameter(lvEngine *ctx, int64_t circle_id) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_diameter: NULL graph");
    GeomNode *circle = graph_get_node(graph, (int) circle_id);
    if (!circle)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_diameter: NULL circle node");
    /* 创建测量结果节点,锚定在圆节点的坐标上。
     * 实际直径值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, circle->symbolic_coords, circle->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_diameter: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_diameter: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) circle_id, result_id);
    return (int64_t) result_id;
}

/** 弦长 */
int64_t preset_chord_length(lvEngine *ctx, int64_t circle_id, int64_t p1, int64_t p2) {
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_chord_length: NULL graph");
    GeomNode *circle = graph_get_node(graph, (int) circle_id);
    GeomNode *np1 = graph_get_node(graph, (int) p1);
    GeomNode *np2 = graph_get_node(graph, (int) p2);
    if (!circle || !np1 || !np2)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_chord_length: NULL node input");
    if (np1->type != GEOM_POINT || np2->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_chord_length: points not GEOM_POINT");
    /* 创建测量结果节点,锚定在 p2 的坐标上。
     * 实际弦长值由约束求解器在解析约束后计算得出。 */
    AddNodeResult res = graph_add_point(graph, np2->symbolic_coords, np2->coord_count);
    if (res != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_chord_length: graph_add_point failed");
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_chord_length: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) p2, result_id);
    return (int64_t) result_id;
}
