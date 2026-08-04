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

/* ============================================================
 * 宏定义区:L4 推理预设通用骨架
 *
 * 说明:
 *   - LV_GEOM_GET_NODE2/3/4:约 15 个函数开头的连续守卫
 *     (ctx → graph → 节点 → GEOM_POINT → symbolic_coords → coord_count)
 *     抽取为守卫宏,按节点数分 2/3/4 三个变体。
 *   - LV_GRAPH_ADD_POINT_RET / LV_GRAPH_ADD_SEG_RET:加节点/线段并
 *     检查 ADD_NODE_OK 的内联模板宏。
 *   - LV_TRI_SKELETON:五个三角形中心函数(circumcenter/centroid/
 *     orthocenter/incenter/excenter)尾部"3 条边 + 3 次 incidence"
 *     公共块。
 * ============================================================ */

/**
 * @brief 两点 GEOM_POINT 输入守卫(声明 graph 及 n1/n2 节点)
 *
 * 等价于 preset_midpoint / preset_perpendicular_bisector 等函数
 * 开头的 6 级连续守卫:
 *   ctx 非空 → main_graph 非空 → 节点非空 → 均为 GEOM_POINT →
 *   symbolic_coords 非空 → coord_count >= 2。
 * 注意:本宏为语句序列(非 do-while)——需在调用函数作用域内声明
 * graph 与节点变量供后续逻辑使用,调用处须以分号结尾。
 *
 * @param ctx  引擎上下文(宏内检查非空)
 * @param fn   错误消息前缀字符串,如 "preset_midpoint"
 * @param n1   节点 1 变量名(宏内声明,形如 GeomNode *p1)
 * @param id1  节点 1 的 id 表达式
 * @param n2   节点 2 变量名
 * @param id2  节点 2 的 id 表达式
 */
#define LV_GEOM_GET_NODE2(ctx, fn, n1, id1, n2, id2) \
    if (!(ctx)) \
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, fn ": NULL ctx"); \
    ConstraintGraph *graph = (ctx)->main_graph; \
    if (!graph) \
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, fn ": NULL graph"); \
    GeomNode *n1 = graph_get_node(graph, (int) (id1)); \
    GeomNode *n2 = graph_get_node(graph, (int) (id2)); \
    if (!n1 || !n2) \
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, fn ": NULL node input"); \
    if (n1->type != GEOM_POINT || n2->type != GEOM_POINT) \
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, fn ": nodes not GEOM_POINT"); \
    if (!n1->symbolic_coords || !n2->symbolic_coords) \
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, fn ": NULL symbolic_coords"); \
    if (n1->coord_count < 2 || n2->coord_count < 2) \
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, fn ": coord_count < 2")

/**
 * @brief 三点 GEOM_POINT 输入守卫(声明 graph 及 n1/n2/n3 节点)
 *
 * 同 LV_GEOM_GET_NODE2,但守卫三个节点。适用于
 * preset_circumcenter/centroid/orthocenter/incenter/excenter/
 * angle_bisector/euler_line/symmedian/nine_point_circle/incircle。
 *
 * @param ctx  引擎上下文
 * @param fn   错误消息前缀字符串
 * @param n1   节点 1 变量名(宏内声明)
 * @param id1  节点 1 的 id 表达式
 * @param n2   节点 2 变量名
 * @param id2  节点 2 的 id 表达式
 * @param n3   节点 3 变量名
 * @param id3  节点 3 的 id 表达式
 */
#define LV_GEOM_GET_NODE3(ctx, fn, n1, id1, n2, id2, n3, id3) \
    if (!(ctx)) \
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, fn ": NULL ctx"); \
    ConstraintGraph *graph = (ctx)->main_graph; \
    if (!graph) \
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, fn ": NULL graph"); \
    GeomNode *n1 = graph_get_node(graph, (int) (id1)); \
    GeomNode *n2 = graph_get_node(graph, (int) (id2)); \
    GeomNode *n3 = graph_get_node(graph, (int) (id3)); \
    if (!n1 || !n2 || !n3) \
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, fn ": NULL node input"); \
    if (n1->type != GEOM_POINT || n2->type != GEOM_POINT || n3->type != GEOM_POINT) \
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, fn ": nodes not GEOM_POINT"); \
    if (!n1->symbolic_coords || !n2->symbolic_coords || !n3->symbolic_coords) \
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, fn ": NULL symbolic_coords"); \
    if (n1->coord_count < 2 || n2->coord_count < 2 || n3->coord_count < 2) \
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, fn ": coord_count < 2")

/**
 * @brief 四点 GEOM_POINT 输入守卫(声明 graph 及 n1/n2/n3/n4 节点)
 *
 * 同 LV_GEOM_GET_NODE2,但守卫四个节点。
 * 目前仅 preset_pedal_triangle 使用。
 *
 * @param ctx  引擎上下文
 * @param fn   错误消息前缀字符串
 * @param n1   节点 1 变量名(宏内声明)
 * @param id1  节点 1 的 id 表达式
 * @param n2   节点 2 变量名
 * @param id2  节点 2 的 id 表达式
 * @param n3   节点 3 变量名
 * @param id3  节点 3 的 id 表达式
 * @param n4   节点 4 变量名
 * @param id4  节点 4 的 id 表达式
 */
#define LV_GEOM_GET_NODE4(ctx, fn, n1, id1, n2, id2, n3, id3, n4, id4) \
    if (!(ctx)) \
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, fn ": NULL ctx"); \
    ConstraintGraph *graph = (ctx)->main_graph; \
    if (!graph) \
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, fn ": NULL graph"); \
    GeomNode *n1 = graph_get_node(graph, (int) (id1)); \
    GeomNode *n2 = graph_get_node(graph, (int) (id2)); \
    GeomNode *n3 = graph_get_node(graph, (int) (id3)); \
    GeomNode *n4 = graph_get_node(graph, (int) (id4)); \
    if (!n1 || !n2 || !n3 || !n4) \
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, fn ": NULL node input"); \
    if (n1->type != GEOM_POINT || n2->type != GEOM_POINT || n3->type != GEOM_POINT || n4->type != GEOM_POINT) \
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, fn ": nodes not GEOM_POINT"); \
    if (!n1->symbolic_coords || !n2->symbolic_coords || !n3->symbolic_coords || !n4->symbolic_coords) \
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, fn ": NULL symbolic_coords"); \
    if (n1->coord_count < 2 || n2->coord_count < 2 || n3->coord_count < 2 || n4->coord_count < 2) \
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, fn ": coord_count < 2")

/**
 * @brief 添加点节点并检查 ADD_NODE_OK,失败则返回错误
 *
 * 等价于全文件反复出现的"graph_add_point + 失败检查"两行模板。
 *
 * @param graph  约束图指针
 * @param coords SymbolicCoord* 数组名(形如 SymbolicCoord *coords[2])
 * @param count  坐标数量
 * @param fn     错误消息前缀,如 "preset_midpoint" 或
 *               "preset_pedal_triangle: graph_add_point foot1",
 *               展开后生成 fn " failed"
 */
#define LV_GRAPH_ADD_POINT_RET(graph, coords, count, fn) \
    do { \
        if (graph_add_point((graph), (SymbolicCoord *const *) (coords), (count)) != ADD_NODE_OK) \
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, fn " failed"); \
    } while (0)

/**
 * @brief 添加线段并检查 ADD_NODE_OK,失败则返回错误
 *
 * @param graph 约束图指针
 * @param a     端点 1 id(内部强转 int)
 * @param b     端点 2 id
 * @param fn    错误消息前缀,如 "preset_midpoint: graph_add_line_segment"
 *              或 "preset_circumcenter: graph_add_line_segment AB",
 *              展开后生成 fn " failed"
 */
#define LV_GRAPH_ADD_SEG_RET(graph, a, b, fn) \
    do { \
        if (graph_add_line_segment((graph), (int) (a), (int) (b)) != ADD_NODE_OK) \
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, fn " failed"); \
    } while (0)

/**
 * @brief 三角形三边骨架:AB/BC/CA 三条线段 + 中心点与三边各一次 incidence
 *
 * 等价于 preset_circumcenter/centroid/orthocenter/incenter/excenter
 * 五个函数尾部 17 行公共块。ab/bc/ca 仅在宏块内使用(关联后即弃),
 * 故可安全包在 do-while 中。
 *
 * @param graph  约束图指针
 * @param center 中心点节点 id(形如 center_id/g_id/h_id/i_id/e_id)
 * @param p1     顶点 A id
 * @param p2     顶点 B id
 * @param p3     顶点 C id
 * @param fn     错误消息前缀,如 "preset_circumcenter"
 */
#define LV_TRI_SKELETON(graph, center, p1, p2, p3, fn) \
    do { \
        LV_GRAPH_ADD_SEG_RET((graph), p1, p2, fn ": graph_add_line_segment AB"); \
        int ab = graph_get_last_added_node_id(graph); \
        graph_add_incidence((graph), (center), ab); \
        LV_GRAPH_ADD_SEG_RET((graph), p2, p3, fn ": graph_add_line_segment BC"); \
        int bc = graph_get_last_added_node_id(graph); \
        graph_add_incidence((graph), (center), bc); \
        LV_GRAPH_ADD_SEG_RET((graph), p3, p1, fn ": graph_add_line_segment CA"); \
        int ca = graph_get_last_added_node_id(graph); \
        graph_add_incidence((graph), (center), ca); \
    } while (0)

/** 求线段中点 */
int64_t preset_midpoint(lvEngine *ctx, int64_t p1_id, int64_t p2_id) {
    LV_GEOM_GET_NODE2(ctx, "preset_midpoint", p1, p1_id, p2, p2_id);

    /* 创建中点节点 */
    SymbolicCoord *coords[2] = {p1->symbolic_coords[0], p2->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, coords, 2, "preset_midpoint");
    int mid_id = graph_get_last_added_node_id(graph);

    /* 线段 AB */
    LV_GRAPH_ADD_SEG_RET(graph, p1_id, p2_id, "preset_midpoint: graph_add_line_segment");
    int line_id = graph_get_last_added_node_id(graph);

    /* 中点在线段上 */
    graph_add_incidence(graph, mid_id, line_id);

    return (int64_t) mid_id;
}

/** 求三角形外心 */
int64_t preset_circumcenter(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    LV_GEOM_GET_NODE3(ctx, "preset_circumcenter", a, p1, b, p2, c, p3);

    /* 创建外心节点 */
    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, coords, 2, "preset_circumcenter");
    int center_id = graph_get_last_added_node_id(graph);

    /* 外心到三顶点等距:通过 incidence 关联三边 */
    LV_TRI_SKELETON(graph, center_id, p1, p2, p3, "preset_circumcenter");

    return (int64_t) center_id;
}

/** 求三角形重心 */
int64_t preset_centroid(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    LV_GEOM_GET_NODE3(ctx, "preset_centroid", a, p1, b, p2, c, p3);

    /* 创建重心节点 */
    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, coords, 2, "preset_centroid");
    int g_id = graph_get_last_added_node_id(graph);

    /* 重心关联三条中线辅助线 */
    LV_TRI_SKELETON(graph, g_id, p1, p2, p3, "preset_centroid");

    return (int64_t) g_id;
}

/** 求三角形垂心 */
int64_t preset_orthocenter(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    LV_GEOM_GET_NODE3(ctx, "preset_orthocenter", a, p1, b, p2, c, p3);

    /* 创建垂心节点 */
    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, coords, 2, "preset_orthocenter");
    int h_id = graph_get_last_added_node_id(graph);

    /* 垂心关联三条边(垂足约束由求解器处理) */
    LV_TRI_SKELETON(graph, h_id, p1, p2, p3, "preset_orthocenter");

    return (int64_t) h_id;
}

/** 求三角形内心 */
int64_t preset_incenter(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    LV_GEOM_GET_NODE3(ctx, "preset_incenter", a, p1, b, p2, c, p3);

    /* 创建内心节点 */
    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, coords, 2, "preset_incenter");
    int i_id = graph_get_last_added_node_id(graph);

    /* 内心关联三条边 */
    LV_TRI_SKELETON(graph, i_id, p1, p2, p3, "preset_incenter");

    return (int64_t) i_id;
}

/** 求三角形旁心(excenter) */
int64_t preset_excenter(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    LV_GEOM_GET_NODE3(ctx, "preset_excenter", a, p1, b, p2, c, p3);

    /* 创建旁心节点 */
    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, coords, 2, "preset_excenter");
    int e_id = graph_get_last_added_node_id(graph);

    /* 旁心关联三条边 */
    LV_TRI_SKELETON(graph, e_id, p1, p2, p3, "preset_excenter");

    return (int64_t) e_id;
}

/** 作垂直平分线 */
int64_t preset_perpendicular_bisector(lvEngine *ctx, int64_t p1, int64_t p2) {
    LV_GEOM_GET_NODE2(ctx, "preset_perpendicular_bisector", a, p1, b, p2);

    /* 创建中点 */
    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, coords, 2, "preset_perpendicular_bisector");
    int mid_id = graph_get_last_added_node_id(graph);

    /* 线段 AB */
    LV_GRAPH_ADD_SEG_RET(graph, p1, p2, "preset_perpendicular_bisector: graph_add_line_segment");
    int seg_id = graph_get_last_added_node_id(graph);

    /* 中点在线段上 */
    graph_add_incidence(graph, mid_id, seg_id);

    return (int64_t) seg_id;
}

/** 作角平分线 */
int64_t preset_angle_bisector(lvEngine *ctx, int64_t p_vertex, int64_t p1, int64_t p2) {
    LV_GEOM_GET_NODE3(ctx, "preset_angle_bisector", v, p_vertex, a, p1, b, p2);

    /* 在角平分线上取一辅助点 */
    SymbolicCoord *coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, coords, 2, "preset_angle_bisector");
    int aux_id = graph_get_last_added_node_id(graph);

    /* 角平分线:从顶点到辅助点 */
    LV_GRAPH_ADD_SEG_RET(graph, p_vertex, aux_id, "preset_angle_bisector: graph_add_line_segment bisector");
    int bisector_id = graph_get_last_added_node_id(graph);

    /* 辅助点关联到两条边 */
    LV_GRAPH_ADD_SEG_RET(graph, p_vertex, p1, "preset_angle_bisector: graph_add_line_segment side1");
    int side1 = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, aux_id, side1);

    LV_GRAPH_ADD_SEG_RET(graph, p_vertex, p2, "preset_angle_bisector: graph_add_line_segment side2");
    int side2 = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, aux_id, side2);

    return (int64_t) bisector_id;
}

/** 作圆上某点处的切线 */
/* 守卫不完全统一(circle 无 GEOM_POINT/坐标检查),保留原守卫;仅中间块宏化 */
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
    LV_GRAPH_ADD_POINT_RET(graph, coords, 2, "preset_tangent_at_point");
    int aux_id = graph_get_last_added_node_id(graph);

    /* 切线 = 过该点的线段 */
    LV_GRAPH_ADD_SEG_RET(graph, point_id, aux_id, "preset_tangent_at_point: graph_add_line_segment");
    int tangent_id = graph_get_last_added_node_id(graph);

    /* 切点关联到圆 */
    graph_add_incidence(graph, (int) point_id, (int) circle_id);

    return (int64_t) tangent_id;
}

/** 从外部点作圆的切线 */
/* 守卫不完全统一(circle 无 GEOM_POINT/坐标检查),保留原守卫;仅中间块宏化 */
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
    LV_GRAPH_ADD_POINT_RET(graph, coords, 2, "preset_tangent_from_point");
    int touch_id = graph_get_last_added_node_id(graph);

    /* 切点在圆上 */
    graph_add_incidence(graph, touch_id, (int) circle_id);

    /* 切线:从外部点到切点 */
    LV_GRAPH_ADD_SEG_RET(graph, point_id, touch_id, "preset_tangent_from_point: graph_add_line_segment");
    int tangent_id = graph_get_last_added_node_id(graph);

    return (int64_t) tangent_id;
}

/** 通过三点确定一个圆 */
/* 守卫仅 4 级(无 symbolic_coords/coord_count 检查),保留原守卫;仅中间块宏化 */
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
    LV_GRAPH_ADD_SEG_RET(graph, p1, p2, "preset_circle_through_points: graph_add_line_segment");
    int circle_id = graph_get_last_added_node_id(graph);

    /* p3 在圆周上 */
    graph_add_incidence(graph, (int) p3, circle_id);

    return (int64_t) circle_id;
}

/** 以给定圆心和半径创建圆 */
/* 守卫仅 4 级(无 symbolic_coords/coord_count 检查),保留原守卫;仅中间块宏化 */
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
    LV_GRAPH_ADD_SEG_RET(graph, center_id, radius_id, "preset_circle_with_center: graph_add_line_segment");
    int circle_id = graph_get_last_added_node_id(graph);

    return (int64_t) circle_id;
}

/** 通过两点确定一条直线 */
/* 守卫仅 4 级(无 symbolic_coords/coord_count 检查),保留原守卫;仅中间块宏化 */
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

    LV_GRAPH_ADD_SEG_RET(graph, p1, p2, "preset_line_through_points: graph_add_line_segment");
    int line_id = graph_get_last_added_node_id(graph);
    return (int64_t) line_id;
}

/** 过一点作已知直线的平行线 */
/* 守卫不完全统一(line 无 GEOM_POINT/坐标检查),保留原守卫;仅中间块宏化 */
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
    LV_GRAPH_ADD_POINT_RET(graph, coords, 2, "preset_parallel_line");
    int aux_id = graph_get_last_added_node_id(graph);

    /* 过 point 与 aux_id 的线段表示平行线 */
    LV_GRAPH_ADD_SEG_RET(graph, point_id, aux_id, "preset_parallel_line: graph_add_line_segment");
    int parallel_id = graph_get_last_added_node_id(graph);

    return (int64_t) parallel_id;
}

/** 过一点作已知直线的垂线 */
/* 守卫不完全统一(line 无 GEOM_POINT/坐标检查),保留原守卫;仅中间块宏化 */
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
    LV_GRAPH_ADD_POINT_RET(graph, coords, 2, "preset_perpendicular_line");
    int foot_id = graph_get_last_added_node_id(graph);

    /* 垂足在原直线上 */
    graph_add_incidence(graph, foot_id, (int) line_id);

    /* 垂线 = point 到 foot_id 的线段 */
    LV_GRAPH_ADD_SEG_RET(graph, point_id, foot_id, "preset_perpendicular_line: graph_add_line_segment");
    int perp_id = graph_get_last_added_node_id(graph);

    return (int64_t) perp_id;
}

/** 作三角形的垂足三角形 */
int64_t preset_pedal_triangle(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3, int64_t point_id) {
    LV_GEOM_GET_NODE4(ctx, "preset_pedal_triangle", a, p1, b, p2, c, p3, p, point_id);

    /* 创建三条边 */
    LV_GRAPH_ADD_SEG_RET(graph, p1, p2, "preset_pedal_triangle: graph_add_line_segment AB");
    int ab = graph_get_last_added_node_id(graph);
    LV_GRAPH_ADD_SEG_RET(graph, p2, p3, "preset_pedal_triangle: graph_add_line_segment BC");
    int bc = graph_get_last_added_node_id(graph);
    LV_GRAPH_ADD_SEG_RET(graph, p3, p1, "preset_pedal_triangle: graph_add_line_segment CA");
    int ca = graph_get_last_added_node_id(graph);

    /* 创建三个垂足 */
    SymbolicCoord *coords1[2] = {p->symbolic_coords[0], a->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, coords1, 2, "preset_pedal_triangle: graph_add_point foot1");
    int foot1 = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, foot1, ab);

    SymbolicCoord *coords2[2] = {p->symbolic_coords[0], b->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, coords2, 2, "preset_pedal_triangle: graph_add_point foot2");
    int foot2 = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, foot2, bc);

    SymbolicCoord *coords3[2] = {p->symbolic_coords[0], c->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, coords3, 2, "preset_pedal_triangle: graph_add_point foot3");
    int foot3 = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, foot3, ca);

    /* 垂足三角形由三个垂足构成 */
    LV_GRAPH_ADD_SEG_RET(graph, foot1, foot2, "preset_pedal_triangle: graph_add_line_segment s1");
    int s1 = graph_get_last_added_node_id(graph);
    LV_GRAPH_ADD_SEG_RET(graph, foot2, foot3, "preset_pedal_triangle: graph_add_line_segment s2");
    int s2 = graph_get_last_added_node_id(graph);
    LV_GRAPH_ADD_SEG_RET(graph, foot3, foot1, "preset_pedal_triangle: graph_add_line_segment s3");
    int s3 = graph_get_last_added_node_id(graph);

    /* 返回区域(垂足三角形) */
    int tri_sides[3] = {s1, s2, s3};
    if (graph_add_region(graph, tri_sides, 3) != ADD_NODE_OK)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_pedal_triangle: graph_add_region failed");
    return (int64_t) graph_get_last_added_node_id(graph);
}

/** 求Cesaro曲线离散点集 */
/* 守卫特殊(仅 ctx/graph/n_points 检查),不适合守卫宏,保留原代码 */
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
    LV_GEOM_GET_NODE3(ctx, "preset_euler_line", a, p1, b, p2, c, p3);

    /* 重心 G */
    SymbolicCoord *g_coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, g_coords, 2, "preset_euler_line: graph_add_point G");
    int g_id = graph_get_last_added_node_id(graph);

    /* 垂心 H */
    SymbolicCoord *h_coords[2] = {b->symbolic_coords[0], c->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, h_coords, 2, "preset_euler_line: graph_add_point H");
    int h_id = graph_get_last_added_node_id(graph);

    /* Euler线 = 过 G 与 H 的直线 */
    LV_GRAPH_ADD_SEG_RET(graph, g_id, h_id, "preset_euler_line: graph_add_line_segment");
    int euler_id = graph_get_last_added_node_id(graph);

    return (int64_t) euler_id;
}

/** 求类似中线(symmedian) */
int64_t preset_symmedian(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    LV_GEOM_GET_NODE3(ctx, "preset_symmedian", a, p1, b, p2, c, p3);

    /* 类似中线:过顶点 A 与对边 BC 的辅助点 */
    SymbolicCoord *coords[2] = {b->symbolic_coords[0], c->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, coords, 2, "preset_symmedian");
    int aux_id = graph_get_last_added_node_id(graph);

    /* symmedian = A 到 aux_id 的线段 */
    LV_GRAPH_ADD_SEG_RET(graph, p1, aux_id, "preset_symmedian: graph_add_line_segment");
    int sym_id = graph_get_last_added_node_id(graph);

    return (int64_t) sym_id;
}

/** 求九点圆 */
int64_t preset_nine_point_circle(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    LV_GEOM_GET_NODE3(ctx, "preset_nine_point_circle", a, p1, b, p2, c, p3);

    /* 创建三边中点作为九点圆上的点 */
    SymbolicCoord *m1[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, m1, 2, "preset_nine_point_circle: graph_add_point m1");
    int mid_ab = graph_get_last_added_node_id(graph);

    SymbolicCoord *m2[2] = {b->symbolic_coords[0], c->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, m2, 2, "preset_nine_point_circle: graph_add_point m2");
    int mid_bc = graph_get_last_added_node_id(graph);

    SymbolicCoord *m3[2] = {c->symbolic_coords[0], a->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, m3, 2, "preset_nine_point_circle: graph_add_point m3");
    int mid_ca = graph_get_last_added_node_id(graph);

    /* 九点圆:以 mid_ab 到 mid_bc 的线段表示 */
    LV_GRAPH_ADD_SEG_RET(graph, mid_ab, mid_bc, "preset_nine_point_circle: graph_add_line_segment");
    int nine_circle = graph_get_last_added_node_id(graph);

    /* mid_ca 也在九点圆上 */
    graph_add_incidence(graph, mid_ca, nine_circle);

    return (int64_t) nine_circle;
}

/** 求三角形内切圆 */
int64_t preset_incircle(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3) {
    LV_GEOM_GET_NODE3(ctx, "preset_incircle", a, p1, b, p2, c, p3);

    /* 内心 I */
    SymbolicCoord *i_coords[2] = {a->symbolic_coords[0], b->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, i_coords, 2, "preset_incircle: graph_add_point I");
    int i_id = graph_get_last_added_node_id(graph);

    /* 内切圆与边 BC 的切点 */
    SymbolicCoord *t_coords[2] = {b->symbolic_coords[0], c->symbolic_coords[1]};
    LV_GRAPH_ADD_POINT_RET(graph, t_coords, 2, "preset_incircle: graph_add_point touch");
    int touch_id = graph_get_last_added_node_id(graph);

    /* 边 BC */
    LV_GRAPH_ADD_SEG_RET(graph, p2, p3, "preset_incircle: graph_add_line_segment BC");
    int bc = graph_get_last_added_node_id(graph);

    /* 切点在 BC 上 */
    graph_add_incidence(graph, touch_id, bc);

    /* 内切圆 = 内心到切点的线段 */
    LV_GRAPH_ADD_SEG_RET(graph, i_id, touch_id, "preset_incircle: graph_add_line_segment incircle");
    int incircle_id = graph_get_last_added_node_id(graph);

    return (int64_t) incircle_id;
}
