/**
 * @file euclidean_geometry_predicate.c
 * @brief 欧几里得几何公理体系实现 —— 几何谓词断言
 *
 * @details 本文件由 euclidean_geometry.c 拆分而来，是 几何谓词断言 模块。
 *          原文件按功能域拆分为 8 个模块，通过容器文件 euclidean_geometry.c 聚合。
 *
 * @date 2026-08-02
 */

#include "lv/euclidean_geometry.h"
#include "euclidean_geometry_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_check.h"

#include "lv/debug.h"
#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/symbolic_coord.h"

/* ========================================================================
 * 第四部分：几何谓词断言
 * ======================================================================== */

/**
 * @brief 断言一组点共线
 *
 * 在约束图中添加 INCIDENCE 约束并验证。Hilbert 体系下
 * 关联公理 I.1（任意两点确定唯一一条直线）保证此断言的合理性。
 *
 * @param ctx       欧几里得上下文
 * @param point_ids 点 ID 数组
 * @param count     点数量（必须 >= 3）
 * @return true 断言成功且一致，false 冲突
 */
bool euclidean_assert_collinear(EuclideanContext *ctx, const int *point_ids, int count) {
    if (!ctx) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "euclidean_assert_collinear: ctx is NULL");
    }
    if (!point_ids) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "euclidean_assert_collinear: point_ids is NULL");
    }
    if (count < 3) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "euclidean_assert_collinear: count < 3");
    }

    for (int i = 0; i < count; i++) {
        if (!euclidean_point_is_registered(ctx, point_ids[i])) {
            euclidean_set_inconsistency(ctx, point_ids[i], "Collinearity assertion failed: unregistered point");
            return false;
        }
    }

    if (ctx->constraint_graph) {
        AddNodeResult line_result = graph_add_line_segment(ctx->constraint_graph, point_ids[0], point_ids[1]);
        if (line_result != ADD_NODE_OK) {
            euclidean_set_inconsistency(ctx, -1, "Collinearity assertion failed: cannot create reference line");
            return false;
        }
        int line_id = graph_get_last_added_node_id(ctx->constraint_graph);

        for (int i = 2; i < count; i++) {
            AddConstraintResult con_result = graph_add_incidence(ctx->constraint_graph, point_ids[i], line_id);
            if (con_result == ADD_CONSTRAINT_CONFLICT) {
                euclidean_set_inconsistency(ctx, point_ids[i], "Collinearity assertion failed: constraint conflict");
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief 断言点 B 在点 A 和点 C 之间
 *
 * 添加 Betweenness 约束到约束图中。此断言对应
 * Hilbert 的顺序公理 II.3 和 Tarski 的介于性基础谓词。
 *
 * @param ctx 欧几里得上下文
 * @param a_id 点 A 的 ID
 * @param b_id 点 B 的 ID
 * @param c_id 点 C 的 ID
 * @return true 断言成功且一致，false 冲突
 */
bool euclidean_assert_between(EuclideanContext *ctx, int a_id, int b_id, int c_id) {
    if (!ctx) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "euclidean_assert_between: ctx is NULL");
    }

    if (a_id == b_id || b_id == c_id || a_id == c_id) {
        euclidean_set_inconsistency(ctx, b_id, "Betweenness assertion failed: points must be distinct");
        return false;
    }

    if (!euclidean_point_is_registered(ctx, a_id) || !euclidean_point_is_registered(ctx, b_id) ||
        !euclidean_point_is_registered(ctx, c_id)) {
        euclidean_set_inconsistency(ctx, b_id, "Betweenness assertion failed: unregistered point");
        return false;
    }

    if (ctx->constraint_graph) {
        AddConstraintResult result = graph_add_betweenness(ctx->constraint_graph, a_id, b_id, c_id);
        if (result == ADD_CONSTRAINT_CONFLICT) {
            euclidean_set_inconsistency(ctx, b_id, "Betweenness assertion failed: constraint conflict");
            return false;
        }
    }

    return true;
}

/**
 * @brief 断言两条线段全等
 *
 * 在约束图中添加全等约束表达两段等长。
 *
 * @param ctx   欧几里得上下文
 * @param a1_id 第一条线段的第一个端点 ID
 * @param a2_id 第一条线段的第二个端点 ID
 * @param b1_id 第二条线段的第一个端点 ID
 * @param b2_id 第二条线段的第二个端点 ID
 * @return true 断言成功且一致，false 冲突
 */
bool euclidean_assert_congruent(EuclideanContext *ctx, int a1_id, int a2_id, int b1_id, int b2_id) {
    if (!ctx) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "euclidean_assert_congruent: ctx is NULL");
    }

    if (a1_id == a2_id || b1_id == b2_id) {
        euclidean_set_inconsistency(ctx, a1_id, "Congruence assertion failed: segment endpoints must be distinct");
        return false;
    }

    if (!euclidean_point_is_registered(ctx, a1_id) || !euclidean_point_is_registered(ctx, a2_id) ||
        !euclidean_point_is_registered(ctx, b1_id) || !euclidean_point_is_registered(ctx, b2_id)) {
        euclidean_set_inconsistency(ctx, a1_id, "Congruence assertion failed: unregistered point");
        return false;
    }

    if (ctx->constraint_graph) {
        AddNodeResult seg_a_result = graph_add_line_segment(ctx->constraint_graph, a1_id, a2_id);
        if (seg_a_result != ADD_NODE_OK) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_GEOM_TYPE, "euclidean_assert_congruent: add line segment A failed");
        }
        int seg_a_id = graph_get_last_added_node_id(ctx->constraint_graph);

        AddNodeResult seg_b_result = graph_add_line_segment(ctx->constraint_graph, b1_id, b2_id);
        if (seg_b_result != ADD_NODE_OK) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_GEOM_TYPE, "euclidean_assert_congruent: add line segment B failed");
        }
        /* 注：当前 ConstraintType 枚举尚无 CONGRUENCE 类型，
         * 全等关系暂通过线段节点创建隐式表达。
         * 待新增 CONGRUENCE 约束类型后再添加显式约束边。 */
        (void) seg_a_id;
    }

    return true;
}
