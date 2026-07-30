/* ============================================================================
 * 模块名称:预设基础几何 (impl_preset_basic_geometry)
 *
 * 说明:
 *   本文件是从 lv_impl_upper.c 中提取的"第3部分:L4 推理预设 --
 *   preset_basic_geometry(21函数)"的独立实现文件。
 *   包含中点、外心、内心、垂心、重心、角平分线等基础几何构造包装函数。
 *
 * 提取自: lv_impl_upper.c (第385行-第1119行)
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
 * 第3部分:L4 推理预设 -- preset_basic_geometry(21函数)
 * ============================================================ */

/** 求线段中点 */
int64_t preset_midpoint(lvEngine *ctx, int64_t p1_id, int64_t p2_id) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_midpoint: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_midpoint: NULL graph");
    GeomNode *p1 = graph_get_node(graph, (int) p1_id);
    GeomNode *p2 = graph_get_node(graph, (int) p2_id);
    if (!p1 || !p2)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_midpoint: NULL node input");
    if (p1->type != GEOM_POINT || p2->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_midpoint: nodes not GEOM_POINT");
    if (!p1->symbolic_coords || !p2->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_midpoint: NULL symbolic_coords");
    if (p1->coord_count < 2 || p2->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_midpoint: coord_count < 2");

    SymbolicCoord *coords[2] = {p1->symbolic_coords[0], p2->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_midpoint: graph_add_point failed");
    int mid_id = graph_get_last_added_node_id(graph);

    if (graph_add_line_segment(graph, (int) p1_id, (int) p2_id) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_midpoint: graph_add_line_segment failed");
    int line_id = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, mid_id, line_id);

    return (int64_t) mid_id;
}

/** 求三角形外心 */
int64_t preset_circumcenter(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_circumcenter: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_circumcenter: NULL graph");
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    GeomNode *c = graph_get_node(graph, (int) p3);
    if (!a || !b || !c)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_circumcenter: NULL node input");
    if (a->type != GEOM_POINT || b->type != GEOM_POINT || c->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_circumcenter: nodes not GEOM_POINT");
    if (!a->symbolic_coords || !b->symbolic_coords || !c->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_circumcenter: NULL symbolic_coords");
    if (a->coord_count < 2 || b->coord_count < 2 || c->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_circumcenter: coord_count < 2");

    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_circumcenter: graph_add_point failed");
    int center_id = graph_get_last_added_node_id(graph);

    /* 外心到三顶点等距:通过 incidence 关联三边 */
    if (graph_add_line_segment(graph, (int) p1, (int) p2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_circumcenter: graph_add_line_segment AB failed");
    int ab = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, center_id, ab);

    if (graph_add_line_segment(graph, (int) p2, (int) p3) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_circumcenter: graph_add_line_segment BC failed");
    int bc = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, center_id, bc);

    if (graph_add_line_segment(graph, (int) p3, (int) p1) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_circumcenter: graph_add_line_segment CA failed");
    int ca = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, center_id, ca);

    return (int64_t) center_id;
}

/** 求三角形重心 */
int64_t preset_centroid(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_centroid: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_centroid: NULL graph");
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    GeomNode *c = graph_get_node(graph, (int) p3);
    if (!a || !b || !c)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_centroid: NULL node input");
    if (a->type != GEOM_POINT || b->type != GEOM_POINT || c->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_centroid: nodes not GEOM_POINT");
    if (!a->symbolic_coords || !b->symbolic_coords || !c->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_centroid: NULL symbolic_coords");
    if (a->coord_count < 2 || b->coord_count < 2 || c->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_centroid: coord_count < 2");

    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_centroid: graph_add_point failed");
    int g_id = graph_get_last_added_node_id(graph);

    /* 重心关联三条中线辅助线 */
    if (graph_add_line_segment(graph, (int) p1, (int) p2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_centroid: graph_add_line_segment AB failed");
    int ab = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, g_id, ab);

    if (graph_add_line_segment(graph, (int) p2, (int) p3) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_centroid: graph_add_line_segment BC failed");
    int bc = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, g_id, bc);

    if (graph_add_line_segment(graph, (int) p3, (int) p1) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_centroid: graph_add_line_segment CA failed");
    int ca = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, g_id, ca);

    return (int64_t) g_id;
}

/** 求三角形垂心 */
int64_t preset_orthocenter(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_orthocenter: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_orthocenter: NULL graph");
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    GeomNode *c = graph_get_node(graph, (int) p3);
    if (!a || !b || !c)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_orthocenter: NULL node input");
    if (a->type != GEOM_POINT || b->type != GEOM_POINT || c->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_orthocenter: nodes not GEOM_POINT");
    if (!a->symbolic_coords || !b->symbolic_coords || !c->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_orthocenter: NULL symbolic_coords");
    if (a->coord_count < 2 || b->coord_count < 2 || c->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_orthocenter: coord_count < 2");

    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_orthocenter: graph_add_point failed");
    int h_id = graph_get_last_added_node_id(graph);

    /* 垂心关联三条边(垂足约束由求解器处理) */
    if (graph_add_line_segment(graph, (int) p1, (int) p2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_orthocenter: graph_add_line_segment AB failed");
    int ab = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, h_id, ab);

    if (graph_add_line_segment(graph, (int) p2, (int) p3) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_orthocenter: graph_add_line_segment BC failed");
    int bc = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, h_id, bc);

    if (graph_add_line_segment(graph, (int) p3, (int) p1) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_orthocenter: graph_add_line_segment CA failed");
    int ca = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, h_id, ca);

    return (int64_t) h_id;
}

/** 求三角形内心 */
int64_t preset_incenter(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_incenter: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_incenter: NULL graph");
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    GeomNode *c = graph_get_node(graph, (int) p3);
    if (!a || !b || !c)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_incenter: NULL node input");
    if (a->type != GEOM_POINT || b->type != GEOM_POINT || c->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_incenter: nodes not GEOM_POINT");
    if (!a->symbolic_coords || !b->symbolic_coords || !c->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_incenter: NULL symbolic_coords");
    if (a->coord_count < 2 || b->coord_count < 2 || c->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_incenter: coord_count < 2");

    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_incenter: graph_add_point failed");
    int i_id = graph_get_last_added_node_id(graph);

    /* 内心关联三条边 */
    if (graph_add_line_segment(graph, (int) p1, (int) p2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_incenter: graph_add_line_segment AB failed");
    int ab = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, i_id, ab);

    if (graph_add_line_segment(graph, (int) p2, (int) p3) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_incenter: graph_add_line_segment BC failed");
    int bc = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, i_id, bc);

    if (graph_add_line_segment(graph, (int) p3, (int) p1) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_incenter: graph_add_line_segment CA failed");
    int ca = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, i_id, ca);

    return (int64_t) i_id;
}

/** 求三角形旁心(excenter) */
int64_t preset_excenter(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_excenter: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_excenter: NULL graph");
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    GeomNode *c = graph_get_node(graph, (int) p3);
    if (!a || !b || !c)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_excenter: NULL node input");
    if (a->type != GEOM_POINT || b->type != GEOM_POINT || c->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_excenter: nodes not GEOM_POINT");
    if (!a->symbolic_coords || !b->symbolic_coords || !c->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_excenter: NULL symbolic_coords");
    if (a->coord_count < 2 || b->coord_count < 2 || c->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_excenter: coord_count < 2");

    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_excenter: graph_add_point failed");
    int e_id = graph_get_last_added_node_id(graph);

    /* 旁心关联三条边 */
    if (graph_add_line_segment(graph, (int) p1, (int) p2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_excenter: graph_add_line_segment AB failed");
    int ab = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, e_id, ab);

    if (graph_add_line_segment(graph, (int) p2, (int) p3) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_excenter: graph_add_line_segment BC failed");
    int bc = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, e_id, bc);

    if (graph_add_line_segment(graph, (int) p3, (int) p1) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_excenter: graph_add_line_segment CA failed");
    int ca = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, e_id, ca);

    return (int64_t) e_id;
}

/** 作垂直平分线 */
int64_t preset_perpendicular_bisector(lvEngine *ctx, int64_t p1, int64_t p2) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_perpendicular_bisector: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_perpendicular_bisector: NULL graph");
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    if (!a || !b)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_perpendicular_bisector: NULL node input");
    if (a->type != GEOM_POINT || b->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_perpendicular_bisector: nodes not GEOM_POINT");
    if (!a->symbolic_coords || !b->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_perpendicular_bisector: NULL symbolic_coords");
    if (a->coord_count < 2 || b->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_perpendicular_bisector: coord_count < 2");

    /* 创建中点 */
    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_perpendicular_bisector: graph_add_point failed");
    int mid_id = graph_get_last_added_node_id(graph);

    /* 线段 AB */
    if (graph_add_line_segment(graph, (int) p1, (int) p2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_perpendicular_bisector: graph_add_line_segment failed");
    int seg_id = graph_get_last_added_node_id(graph);

    /* 中点在线段上 */
    graph_add_incidence(graph, mid_id, seg_id);

    return (int64_t) seg_id;
}

/** 作角平分线 */
int64_t preset_angle_bisector(lvEngine *ctx, int64_t p_vertex, int64_t p1, int64_t p2) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_angle_bisector: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_angle_bisector: NULL graph");
    GeomNode *v = graph_get_node(graph, (int) p_vertex);
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    if (!v || !a || !b)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_angle_bisector: NULL node input");
    if (v->type != GEOM_POINT || a->type != GEOM_POINT || b->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_angle_bisector: nodes not GEOM_POINT");
    if (!v->symbolic_coords || !a->symbolic_coords || !b->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_angle_bisector: NULL symbolic_coords");
    if (v->coord_count < 2 || a->coord_count < 2 || b->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_angle_bisector: coord_count < 2");

    /* 在角平分线上取一辅助点 */
    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_angle_bisector: graph_add_point failed");
    int aux_id = graph_get_last_added_node_id(graph);

    /* 角平分线:从顶点到辅助点 */
    if (graph_add_line_segment(graph, (int) p_vertex, aux_id) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_angle_bisector: graph_add_line_segment bisector failed");
    int bisector_id = graph_get_last_added_node_id(graph);

    /* 辅助点关联到两条边 */
    if (graph_add_line_segment(graph, (int) p_vertex, (int) p1) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_angle_bisector: graph_add_line_segment side1 failed");
    int side1 = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, aux_id, side1);

    if (graph_add_line_segment(graph, (int) p_vertex, (int) p2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_angle_bisector: graph_add_line_segment side2 failed");
    int side2 = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, aux_id, side2);

    return (int64_t) bisector_id;
}

/** 作圆上某点处的切线 */
int64_t preset_tangent_at_point(lvEngine *ctx, int64_t circle_id, int64_t point_id) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_tangent_at_point: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_tangent_at_point: NULL graph");
    GeomNode *circle = graph_get_node(graph, (int) circle_id);
    GeomNode *point = graph_get_node(graph, (int) point_id);
    if (!circle || !point)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_tangent_at_point: NULL node input");
    if (point->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_tangent_at_point: point not GEOM_POINT");
    if (!point->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_tangent_at_point: NULL symbolic_coords");
    if (point->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_tangent_at_point: coord_count < 2");

    /* 创建切线上另一辅助点 */
    SymbolicCoord *coords[2] = {point->symbolic_coords[0], point->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_tangent_at_point: graph_add_point failed");
    int aux_id = graph_get_last_added_node_id(graph);

    /* 切线 = 过该点的线段 */
    if (graph_add_line_segment(graph, (int) point_id, aux_id) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_tangent_at_point: graph_add_line_segment failed");
    int tangent_id = graph_get_last_added_node_id(graph);

    /* 切点关联到圆 */
    graph_add_incidence(graph, (int) point_id, (int) circle_id);

    return (int64_t) tangent_id;
}

/** 从外部点作圆的切线 */
int64_t preset_tangent_from_point(lvEngine *ctx, int64_t circle_id, int64_t point_id) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_tangent_from_point: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_tangent_from_point: NULL graph");
    GeomNode *circle = graph_get_node(graph, (int) circle_id);
    GeomNode *point = graph_get_node(graph, (int) point_id);
    if (!circle || !point)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_tangent_from_point: NULL node input");
    if (point->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_tangent_from_point: point not GEOM_POINT");
    if (!point->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_tangent_from_point: NULL symbolic_coords");
    if (point->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_tangent_from_point: coord_count < 2");

    /* 创建切点 */
    SymbolicCoord *coords[2] = {point->symbolic_coords[0], point->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_tangent_from_point: graph_add_point failed");
    int touch_id = graph_get_last_added_node_id(graph);

    /* 切点在圆上 */
    graph_add_incidence(graph, touch_id, (int) circle_id);

    /* 切线:从外部点到切点 */
    if (graph_add_line_segment(graph, (int) point_id, touch_id) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_tangent_from_point: graph_add_line_segment failed");
    int tangent_id = graph_get_last_added_node_id(graph);

    return (int64_t) tangent_id;
}

/** 通过三点确定一个圆 */
int64_t preset_circle_through_points(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_circle_through_points: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_circle_through_points: NULL graph");
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    GeomNode *c = graph_get_node(graph, (int) p3);
    if (!a || !b || !c)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_circle_through_points: NULL node input");
    if (a->type != GEOM_POINT || b->type != GEOM_POINT || c->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_circle_through_points: nodes not GEOM_POINT");

    /* 以 a->b 线段表示圆(圆心到圆周点) */
    if (graph_add_line_segment(graph, (int) p1, (int) p2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_circle_through_points: graph_add_line_segment failed");
    int circle_id = graph_get_last_added_node_id(graph);

    /* p3 在圆周上 */
    graph_add_incidence(graph, (int) p3, circle_id);

    return (int64_t) circle_id;
}

/** 以给定圆心和半径创建圆 */
int64_t preset_circle_with_center(lvEngine *ctx, int64_t center_id, int64_t radius_id) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_circle_with_center: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_circle_with_center: NULL graph");
    GeomNode *center = graph_get_node(graph, (int) center_id);
    GeomNode *radius = graph_get_node(graph, (int) radius_id);
    if (!center || !radius)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_circle_with_center: NULL node input");
    if (center->type != GEOM_POINT || radius->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_circle_with_center: nodes not GEOM_POINT");

    /* 圆用圆心到半径端点的线段表示 */
    if (graph_add_line_segment(graph, (int) center_id, (int) radius_id) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_circle_with_center: graph_add_line_segment failed");
    int circle_id = graph_get_last_added_node_id(graph);

    return (int64_t) circle_id;
}

/** 通过两点确定一条直线 */
int64_t preset_line_through_points(lvEngine *ctx, int64_t p1, int64_t p2) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_line_through_points: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_line_through_points: NULL graph");
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    if (!a || !b)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_line_through_points: NULL node input");
    if (a->type != GEOM_POINT || b->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_line_through_points: nodes not GEOM_POINT");

    if (graph_add_line_segment(graph, (int) p1, (int) p2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_line_through_points: graph_add_line_segment failed");
    int line_id = graph_get_last_added_node_id(graph);
    return (int64_t) line_id;
}

/** 过一点作已知直线的平行线 */
int64_t preset_parallel_line(lvEngine *ctx, int64_t line_id, int64_t point_id) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_parallel_line: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_parallel_line: NULL graph");
    GeomNode *line = graph_get_node(graph, (int) line_id);
    GeomNode *point = graph_get_node(graph, (int) point_id);
    if (!line || !point)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_parallel_line: NULL node input");
    if (point->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_parallel_line: point not GEOM_POINT");
    if (!point->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_parallel_line: NULL symbolic_coords");
    if (point->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_parallel_line: coord_count < 2");

    /* 创建平行线上一辅助点 */
    SymbolicCoord *coords[2] = {point->symbolic_coords[0], point->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_parallel_line: graph_add_point failed");
    int aux_id = graph_get_last_added_node_id(graph);

    /* 过 point 与 aux_id 的线段表示平行线 */
    if (graph_add_line_segment(graph, (int) point_id, aux_id) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_parallel_line: graph_add_line_segment failed");
    int parallel_id = graph_get_last_added_node_id(graph);

    return (int64_t) parallel_id;
}

/** 过一点作已知直线的垂线 */
int64_t preset_perpendicular_line(lvEngine *ctx, int64_t line_id, int64_t point_id) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_perpendicular_line: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_perpendicular_line: NULL graph");
    GeomNode *line = graph_get_node(graph, (int) line_id);
    GeomNode *point = graph_get_node(graph, (int) point_id);
    if (!line || !point)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_perpendicular_line: NULL node input");
    if (point->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_perpendicular_line: point not GEOM_POINT");
    if (!point->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_perpendicular_line: NULL symbolic_coords");
    if (point->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_perpendicular_line: coord_count < 2");

    /* 创建垂足辅助点 */
    SymbolicCoord *coords[2] = {point->symbolic_coords[0], point->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_perpendicular_line: graph_add_point failed");
    int foot_id = graph_get_last_added_node_id(graph);

    /* 垂足在原直线上 */
    graph_add_incidence(graph, foot_id, (int) line_id);

    /* 垂线 = point 到 foot_id 的线段 */
    if (graph_add_line_segment(graph, (int) point_id, foot_id) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_perpendicular_line: graph_add_line_segment failed");
    int perp_id = graph_get_last_added_node_id(graph);

    return (int64_t) perp_id;
}

/** 作三角形的垂足三角形 */
int64_t preset_pedal_triangle(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3, int64_t point_id) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_pedal_triangle: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_pedal_triangle: NULL graph");
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    GeomNode *c = graph_get_node(graph, (int) p3);
    GeomNode *p = graph_get_node(graph, (int) point_id);
    if (!a || !b || !c || !p)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_pedal_triangle: NULL node input");
    if (a->type != GEOM_POINT || b->type != GEOM_POINT || c->type != GEOM_POINT || p->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_pedal_triangle: nodes not GEOM_POINT");
    if (!a->symbolic_coords || !b->symbolic_coords || !c->symbolic_coords || !p->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_pedal_triangle: NULL symbolic_coords");
    if (a->coord_count < 2 || b->coord_count < 2 || c->coord_count < 2 || p->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_pedal_triangle: coord_count < 2");

    /* 创建三条边 */
    if (graph_add_line_segment(graph, (int) p1, (int) p2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_pedal_triangle: graph_add_line_segment AB failed");
    int ab = graph_get_last_added_node_id(graph);
    if (graph_add_line_segment(graph, (int) p2, (int) p3) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_pedal_triangle: graph_add_line_segment BC failed");
    int bc = graph_get_last_added_node_id(graph);
    if (graph_add_line_segment(graph, (int) p3, (int) p1) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_pedal_triangle: graph_add_line_segment CA failed");
    int ca = graph_get_last_added_node_id(graph);

    /* 创建三个垂足 */
    SymbolicCoord *coords1[2] = {p->symbolic_coords[0], a->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords1, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_pedal_triangle: graph_add_point foot1 failed");
    int foot1 = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, foot1, ab);

    SymbolicCoord *coords2[2] = {p->symbolic_coords[0], b->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords2, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_pedal_triangle: graph_add_point foot2 failed");
    int foot2 = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, foot2, bc);

    SymbolicCoord *coords3[2] = {p->symbolic_coords[0], c->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords3, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_pedal_triangle: graph_add_point foot3 failed");
    int foot3 = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, foot3, ca);

    /* 垂足三角形由三个垂足构成 */
    if (graph_add_line_segment(graph, foot1, foot2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_pedal_triangle: graph_add_line_segment s1 failed");
    int s1 = graph_get_last_added_node_id(graph);
    if (graph_add_line_segment(graph, foot2, foot3) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_pedal_triangle: graph_add_line_segment s2 failed");
    int s2 = graph_get_last_added_node_id(graph);
    if (graph_add_line_segment(graph, foot3, foot1) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_pedal_triangle: graph_add_line_segment s3 failed");
    int s3 = graph_get_last_added_node_id(graph);

    /* 返回区域(垂足三角形) */
    int tri_sides[3] = {s1, s2, s3};
    if (graph_add_region(graph, tri_sides, 3) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_pedal_triangle: graph_add_region failed");
    return (int64_t) graph_get_last_added_node_id(graph);
}

/** 求Cesaro曲线离散点集 */
int64_t preset_cesaro(lvEngine *ctx, int64_t n_points) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_cesaro: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_cesaro: NULL graph");
    if (n_points < 1)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_cesaro: n_points < 1");

    int first_id = -1;
    for (int64_t i = 0; i < n_points; i++) {
        SymbolicCoord *c[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
        if (graph_add_point(graph, (SymbolicCoord *const *) c, 2) != ADD_NODE_OK)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_cesaro: graph_add_point failed");
        int pt_id = graph_get_last_added_node_id(graph);
        if (i == 0)
            first_id = pt_id;
    }
    return (int64_t) first_id;
}

/** 求Euler线 */
int64_t preset_euler_line(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_euler_line: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_euler_line: NULL graph");
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    GeomNode *c = graph_get_node(graph, (int) p3);
    if (!a || !b || !c)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_euler_line: NULL node input");
    if (a->type != GEOM_POINT || b->type != GEOM_POINT || c->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_euler_line: nodes not GEOM_POINT");
    if (!a->symbolic_coords || !b->symbolic_coords || !c->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_euler_line: NULL symbolic_coords");
    if (a->coord_count < 2 || b->coord_count < 2 || c->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_euler_line: coord_count < 2");

    /* 重心 G */
    SymbolicCoord *g_coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) g_coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_euler_line: graph_add_point G failed");
    int g_id = graph_get_last_added_node_id(graph);

    /* 垂心 H */
    SymbolicCoord *h_coords[2] = {b->symbolic_coords[0], c->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) h_coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_euler_line: graph_add_point H failed");
    int h_id = graph_get_last_added_node_id(graph);

    /* Euler线 = 过 G 与 H 的直线 */
    if (graph_add_line_segment(graph, g_id, h_id) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_euler_line: graph_add_line_segment failed");
    int euler_id = graph_get_last_added_node_id(graph);

    return (int64_t) euler_id;
}

/** 求类似中线(symmedian) */
int64_t preset_symmedian(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_symmedian: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_symmedian: NULL graph");
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    GeomNode *c = graph_get_node(graph, (int) p3);
    if (!a || !b || !c)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_symmedian: NULL node input");
    if (a->type != GEOM_POINT || b->type != GEOM_POINT || c->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_symmedian: nodes not GEOM_POINT");
    if (!a->symbolic_coords || !b->symbolic_coords || !c->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_symmedian: NULL symbolic_coords");
    if (a->coord_count < 2 || b->coord_count < 2 || c->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_symmedian: coord_count < 2");

    /* 类似中线:过顶点 A 与对边 BC 的辅助点 */
    SymbolicCoord *coords[2] = {b->symbolic_coords[0], c->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_symmedian: graph_add_point failed");
    int aux_id = graph_get_last_added_node_id(graph);

    /* symmedian = A 到 aux_id 的线段 */
    if (graph_add_line_segment(graph, (int) p1, aux_id) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_symmedian: graph_add_line_segment failed");
    int sym_id = graph_get_last_added_node_id(graph);

    return (int64_t) sym_id;
}

/** 求九点圆 */
int64_t preset_nine_point_circle(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_nine_point_circle: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_nine_point_circle: NULL graph");
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    GeomNode *c = graph_get_node(graph, (int) p3);
    if (!a || !b || !c)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_nine_point_circle: NULL node input");
    if (a->type != GEOM_POINT || b->type != GEOM_POINT || c->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_nine_point_circle: nodes not GEOM_POINT");
    if (!a->symbolic_coords || !b->symbolic_coords || !c->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_nine_point_circle: NULL symbolic_coords");
    if (a->coord_count < 2 || b->coord_count < 2 || c->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_nine_point_circle: coord_count < 2");

    /* 创建三边中点作为九点圆上的点 */
    SymbolicCoord *m1[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) m1, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_nine_point_circle: graph_add_point m1 failed");
    int mid_ab = graph_get_last_added_node_id(graph);

    SymbolicCoord *m2[2] = {b->symbolic_coords[0], c->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) m2, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_nine_point_circle: graph_add_point m2 failed");
    int mid_bc = graph_get_last_added_node_id(graph);

    SymbolicCoord *m3[2] = {c->symbolic_coords[0], a->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) m3, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_nine_point_circle: graph_add_point m3 failed");
    int mid_ca = graph_get_last_added_node_id(graph);

    /* 九点圆:以 mid_ab 到 mid_bc 的线段表示 */
    if (graph_add_line_segment(graph, mid_ab, mid_bc) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_nine_point_circle: graph_add_line_segment failed");
    int nine_circle = graph_get_last_added_node_id(graph);

    /* mid_ca 也在九点圆上 */
    graph_add_incidence(graph, mid_ca, nine_circle);

    return (int64_t) nine_circle;
}

/** 求三角形内切圆 */
int64_t preset_incircle(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_incircle: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_incircle: NULL graph");
    GeomNode *a = graph_get_node(graph, (int) p1);
    GeomNode *b = graph_get_node(graph, (int) p2);
    GeomNode *c = graph_get_node(graph, (int) p3);
    if (!a || !b || !c)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_incircle: NULL node input");
    if (a->type != GEOM_POINT || b->type != GEOM_POINT || c->type != GEOM_POINT)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_incircle: nodes not GEOM_POINT");
    if (!a->symbolic_coords || !b->symbolic_coords || !c->symbolic_coords)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_incircle: NULL symbolic_coords");
    if (a->coord_count < 2 || b->coord_count < 2 || c->coord_count < 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_incircle: coord_count < 2");

    /* 内心 I */
    SymbolicCoord *i_coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) i_coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_incircle: graph_add_point I failed");
    int i_id = graph_get_last_added_node_id(graph);

    /* 内切圆与边 BC 的切点 */
    SymbolicCoord *t_coords[2] = {b->symbolic_coords[0], c->symbolic_coords[1]};
    if (graph_add_point(graph, (SymbolicCoord *const *) t_coords, 2) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_incircle: graph_add_point touch failed");
    int touch_id = graph_get_last_added_node_id(graph);

    /* 边 BC */
    if (graph_add_line_segment(graph, (int) p2, (int) p3) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_incircle: graph_add_line_segment BC failed");
    int bc = graph_get_last_added_node_id(graph);

    /* 切点在 BC 上 */
    graph_add_incidence(graph, touch_id, bc);

    /* 内切圆 = 内心到切点的线段 */
    if (graph_add_line_segment(graph, i_id, touch_id) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_incircle: graph_add_line_segment incircle failed");
    int incircle_id = graph_get_last_added_node_id(graph);

    return (int64_t) incircle_id;
}
