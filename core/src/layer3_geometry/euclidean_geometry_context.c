/**
 * @file euclidean_geometry_context.c
 * @brief 欧几里得几何公理体系实现 —— 上下文生命周期管理
 *
 * @details 本文件由 euclidean_geometry.c 拆分而来，是 上下文生命周期管理 模块。
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
 * 第一部分：上下文生命周期管理
 * ======================================================================== */

/**
 * @brief 创建欧几里得几何上下文
 *
 * 初始化一个全新的 EuclideanContext，默认使用 Hilbert 公理体系，
 * 启用全部五大公理组的所有公理。内部数组按初始容量分配。
 *
 * @param graph 关联的约束图（可为 NULL，后续通过 euclidean_bind_graph() 绑定）
 * @return 新分配的 EuclideanContext，失败返回 NULL
 */
EuclideanContext *euclidean_init(ConstraintGraph *graph) {
    EuclideanContext *ctx = lv_calloc(1, sizeof(EuclideanContext));
    if (!ctx) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "euclidean_init: lv_calloc failed");
    }

    /* 默认使用 Hilbert 公理体系 */
    ctx->active_axiom_system = EUCLID_HILBERT;

    /* 初始化动态数组 */
    lv_darray_init(&ctx->points_da, sizeof(int));
    lv_darray_init(&ctx->lines_da, sizeof(int));
    lv_darray_init(&ctx->circles_da, sizeof(int));

    /* 绑定约束图（可为 NULL） */
    ctx->constraint_graph = graph;

    /* 默认启用全部公理 */
    ctx->enabled_axioms_mask = EUCLID_DEFAULT_AXIOM_MASK;

    /* 初始一致性状态 */
    ctx->is_consistent = true;
    ctx->inconsistency_source = -1;
    ctx->inconsistency_message[0] = '\0';

    /* 等价性证明链初始为 NULL */
    ctx->equivalence_chain = NULL;

    return ctx;
}

/**
 * @brief 销毁欧几里得几何上下文
 *
 * 释放所有已注册的实体列表、等价性证明链及其内部约束图。
 * 注意：不释放关联的外部 ConstraintGraph（由调用者管理）。
 *
 * @param ctx 欧几里得上下文
 */
void euclidean_destroy(EuclideanContext *ctx) {
    if (!ctx) {
        return;
    }

    lv_darray_free(&ctx->points_da);
    lv_darray_free(&ctx->lines_da);
    lv_darray_free(&ctx->circles_da);
    if (ctx->equivalence_chain) {
        euclidean_destroy_equivalence_chain(ctx->equivalence_chain);
        ctx->equivalence_chain = NULL;
    }
    lv_free((void **) &ctx);
}
