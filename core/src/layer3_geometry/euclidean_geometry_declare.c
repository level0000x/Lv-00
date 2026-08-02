/**
 * @file euclidean_geometry_declare.c
 * @brief 欧几里得几何公理体系实现 —— 几何实体声明
 *
 * @details 本文件由 euclidean_geometry.c 拆分而来，是 几何实体声明 模块。
 *          原文件按功能域拆分为 8 个模块，通过容器文件 euclidean_geometry.c 聚合。
 *
 * @date 2026-08-02
 */

#include "euclidean_geometry.h"
#include "euclidean_geometry_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_check.h"

#include "debug.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "symbolic_coord.h"

/* ========================================================================
 * 第三部分：几何实体声明
 * ======================================================================== */

/**
 * @brief 声明一个点
 *
 * 在上下文中注册一个新点。该点会被同步到关联的约束图中
 * 创建一个 GEOM_POINT 类型的节点。
 *
 * @param ctx  欧几里得上下文
 * @param x    X 坐标（可为 NULL 表示未定坐标）
 * @param y    Y 坐标（可为 NULL 表示未定坐标）
 * @param name 可选的名称（可为 NULL，仅在日志中使用）
 * @return 新注册的点 ID（>= 0），失败返回 -1
 */
int euclidean_declare_point(EuclideanContext *ctx, SymbolicCoord *x, SymbolicCoord *y, const char *name) {
    if (!ctx) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "euclidean_declare_point: ctx is NULL");
    }

    lv_UNUSED(name);

    if (ctx->constraint_graph) {
        SymbolicCoord *coords[2] = {x, y};
        AddNodeResult result = graph_add_point(ctx->constraint_graph, coords, 2);
        if (result != ADD_NODE_OK) {
            lv_RETURN_ERROR(lv_ERROR_INVALID_GEOM_TYPE, "euclidean_declare_point: graph_add_point failed");
        }

        int point_id = graph_get_last_added_node_id(ctx->constraint_graph);
        if (point_id < 0) {
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "euclidean_declare_point: got invalid point_id from graph");
        }

        if (!euclidean_register_point_id(ctx, point_id)) {
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "euclidean_declare_point: register_point_id failed");
        }

        return point_id;
    }

    /* 无约束图时的备用处理 */
    int point_id = (int)ctx->points_da.count;
    if (!euclidean_register_point_id(ctx, point_id)) {
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "euclidean_declare_point: register_point_id (no graph) failed");
    }
    return point_id;
}

/**
 * @brief 声明一条直线
 *
 * 由两个不同的点确定一条直线。两点必须已在上下文中注册。
 * 在约束图中创建 GEOM_LINE_SEGMENT 节点。
 *
 * @param ctx   欧几里得上下文
 * @param p1_id 第一个点的 ID
 * @param p2_id 第二个点的 ID
 * @return 新注册的线 ID（>= 0），失败返回 -1（点不存在或两点相同）
 */
int euclidean_declare_line(EuclideanContext *ctx, int p1_id, int p2_id) {
    lv_CHECK_NOT_NULL(ctx);


    if (p1_id == p2_id) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "euclidean_declare_line: p1_id == p2_id");
    }

    if (!euclidean_point_is_registered(ctx, p1_id) || !euclidean_point_is_registered(ctx, p2_id)) {
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "euclidean_declare_line: point not registered");
    }

    if (ctx->constraint_graph) {
        AddNodeResult result = graph_add_line_segment(ctx->constraint_graph, p1_id, p2_id);
        if (result != ADD_NODE_OK) {
            lv_RETURN_ERROR(lv_ERROR_INVALID_GEOM_TYPE, "euclidean_declare_line: graph_add_line_segment failed");
        }

        int line_id = graph_get_last_added_node_id(ctx->constraint_graph);
        if (line_id < 0) {
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "euclidean_declare_line: got invalid line_id from graph");
        }

        if (!euclidean_register_line_id(ctx, line_id)) {
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "euclidean_declare_line: register_line_id failed");
        }

        return line_id;
    }

    int line_id = ctx->lines_da.count + 1000;
    if (!euclidean_register_line_id(ctx, line_id)) {
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "euclidean_declare_line: register_line_id (no graph) failed");
    }
    return line_id;
}

/**
 * @brief 声明一个圆
 *
 * 由圆心和半径确定一个圆。圆心必须在上下文中已注册。
 *
 * @param ctx      欧几里得上下文
 * @param center_id 圆心点 ID
 * @param radius    半径（符号坐标，不能为 NULL）
 * @return 新注册的圆 ID（>= 0），失败返回 -1
 */
int euclidean_declare_circle(EuclideanContext *ctx, int center_id, SymbolicCoord *radius) {
    if (!ctx) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "euclidean_declare_circle: ctx is NULL");
    }

    if (!radius) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "euclidean_declare_circle: radius is NULL");
    }

    if (!euclidean_point_is_registered(ctx, center_id)) {
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "euclidean_declare_circle: center point not registered");
    }

    if (ctx->constraint_graph) {
        SymbolicCoord *coords[3];
        coords[0] = NULL;
        coords[1] = radius;
        coords[2] = NULL;

        AddNodeResult result = graph_add_point(ctx->constraint_graph, coords, 3);
        if (result != ADD_NODE_OK) {
            lv_RETURN_ERROR(lv_ERROR_INVALID_GEOM_TYPE, "euclidean_declare_circle: graph_add_point failed");
        }

        int circle_id = graph_get_last_added_node_id(ctx->constraint_graph);
        if (circle_id < 0) {
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "euclidean_declare_circle: got invalid circle_id from graph");
        }

        if (!euclidean_register_circle_id(ctx, circle_id)) {
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "euclidean_declare_circle: register_circle_id failed");
        }

        return circle_id;
    }

    int circle_id = (int)ctx->circles_da.count + 2000;
    if (!euclidean_register_circle_id(ctx, circle_id)) {
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "euclidean_declare_circle: register_circle_id (no graph) failed");
    }
    return circle_id;
}
