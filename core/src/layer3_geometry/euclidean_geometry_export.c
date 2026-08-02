/**
 * @file euclidean_geometry_export.c
 * @brief 欧几里得几何公理体系实现 —— 导出
 *
 * @details 本文件由 euclidean_geometry.c 拆分而来，是 导出 模块。
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

/* ========================================================================
 * 第六部分：导出
 * ======================================================================== */

/**
 * @brief 将当前构造导出为 Birkhoff 公理体系的约束图
 *
 * @param ctx 欧几里得上下文
 * @return 新分配的 ConstraintGraph（调用者负责释放），导出失败返回 NULL
 */
ConstraintGraph *euclidean_export_birkhoff(const EuclideanContext *ctx) {
    if (!ctx)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "euclidean_export_birkhoff: ctx is NULL");

    ConstraintGraph *export_graph = graph_create();
    if (!export_graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "euclidean_export_birkhoff: graph_create failed");

    for (int i = 0; i < (int)ctx->points_da.count; i++) {
        int *pp = (int *)lv_darray_get(&ctx->points_da, i);
        if (!pp) break;
        int point_id = *pp;
        SymbolicCoord *coords[2] = {NULL, NULL};
        if (ctx->constraint_graph) {
            GeomNode *node = graph_get_node(ctx->constraint_graph, point_id);
            if (node && node->symbolic_coords && node->coord_count >= 2) {
                coords[0] = node->symbolic_coords[0];
                coords[1] = node->symbolic_coords[1];
            }
        }
        if (graph_add_point(export_graph, coords, 2) != ADD_NODE_OK) {
            graph_destroy(export_graph);
            lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_GEOM_TYPE, "euclidean_export_birkhoff: graph_add_point failed");
        }
    }

    return export_graph;
}

/**
 * @brief 将当前构造导出为 Tarski 公理体系的约束图
 *
 * @param ctx 欧几里得上下文
 * @return 新分配的 ConstraintGraph（调用者负责释放），导出失败返回 NULL
 */
ConstraintGraph *euclidean_export_tarski(const EuclideanContext *ctx) {
    if (!ctx)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "euclidean_export_tarski: ctx is NULL");

    ConstraintGraph *export_graph = graph_create();
    if (!export_graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "euclidean_export_tarski: graph_create failed");

    for (int i = 0; i < (int)ctx->points_da.count; i++) {
        int *pp = (int *)lv_darray_get(&ctx->points_da, i);
        if (!pp) break;
        int point_id = *pp;
        SymbolicCoord *coords[2] = {NULL, NULL};
        if (ctx->constraint_graph) {
            GeomNode *node = graph_get_node(ctx->constraint_graph, point_id);
            if (node && node->symbolic_coords && node->coord_count >= 2) {
                coords[0] = node->symbolic_coords[0];
                coords[1] = node->symbolic_coords[1];
            }
        }
        if (graph_add_point(export_graph, coords, 2) != ADD_NODE_OK) {
            graph_destroy(export_graph);
            lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_GEOM_TYPE, "euclidean_export_tarski: graph_add_point failed");
        }
    }

    return export_graph;
}
