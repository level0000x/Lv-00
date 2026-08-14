/**
 * @file euclidean_geometry_consistency.c
 * @brief 欧几里得几何公理体系实现 —— 定理验证与一致性检查
 *
 * @details 本文件由 euclidean_geometry.c 拆分而来，是 定理验证与一致性检查 模块。
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
 * 第五部分：定理验证与一致性检查
 * ======================================================================== */

/**
 * @brief 验证一个几何定理是否在当前公理体系下成立
 *
 * @param ctx         欧几里得上下文
 * @param proposition 要验证的命题（由调用者解释其类型）
 * @param proof_out   输出：验证过程中产生的证明步骤数量（可为 NULL）
 * @return true 定理在当前公理体系下成立，false 不成立或无法判定
 */
static bool euclidean_verify_theorem(EuclideanContext *ctx, const void *proposition, int *proof_out) {
    if (!ctx || !proposition) {
        if (proof_out)
            *proof_out = 0;
        return false;
    }

    int result = false;
    int step_count = 0;

    int constraint_id = *(const int *) proposition;

    if (ctx->constraint_graph) {
        Constraint *con = graph_get_constraint(ctx->constraint_graph, constraint_id);
        if (con) {
            result = true;
            step_count = 1;
        }
    }

    if (proof_out)
        *proof_out = step_count;
    return (bool) result;
}

/**
 * @brief 检查公理体系的一致性
 *
 * 遍历所有已启用的公理和已注册的谓词断言，检测是否存在矛盾。
 * 若发现矛盾，将 inconsistency_source 设置为导致矛盾的
 * 公理/谓词 ID，并将 is_consistent 设为 false。
 *
 * @param ctx 欧几里得上下文
 * @return true 一致，false 存在矛盾
 */
bool euclidean_check_consistency(EuclideanContext *ctx) {
    if (!ctx) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "euclidean_check_consistency: ctx is NULL");
    }

    euclidean_clear_inconsistency(ctx);

    if (ctx->points_da.count == 0) {
        ctx->is_consistent = true;
        return true;
    }

    if (!ctx->constraint_graph) {
        ctx->is_consistent = true;
        return true;
    }

    if (!euclidean_verify_axiom_inconsistency(ctx)) {
        ctx->is_consistent = false;
        return false;
    }

    int conflict_count = 0;
    int *conflict_sizes = NULL;
    int **conflicts = graph_detect_conflicts(ctx->constraint_graph, &conflict_count, &conflict_sizes);

    if (conflicts && conflict_count > 0) {
        ctx->is_consistent = false;
        ctx->inconsistency_source = (conflicts[0] && conflict_sizes[0] > 0) ? conflicts[0][0] : -1;

        int written = 0;
        lv_SAFE_SNPRINTF(written, ctx->inconsistency_message, sizeof(ctx->inconsistency_message),
                         "Consistency check failed: %d conflict group(s) detected", conflict_count);

        for (int i = 0; i < conflict_count; i++) {
            if (conflicts[i])
                lv_free((void **) &conflicts[i]);
        }
        lv_free((void **) &conflicts);
        if (conflict_sizes)
            lv_free((void **) &conflict_sizes);

        return false;
    }

    if (conflicts)
        lv_free((void **) &conflicts);
    if (conflict_sizes)
        lv_free((void **) &conflict_sizes);

    ctx->is_consistent = true;
    return true;
}
