/**
 * @file euclidean_geometry_axiom.c
 * @brief 欧几里得几何公理体系实现 —— 公理体系配置
 *
 * @details 本文件由 euclidean_geometry.c 拆分而来，是 公理体系配置 模块。
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
 * 第二部分：公理体系配置
 * ======================================================================== */

/**
 * @brief 设置当前活跃的公理体系
 *
 * 切换到指定的公理体系。切换时会对已注册的实体和已启用的公理
 * 执行一致性检查。如果新体系与当前构造不一致，返回 false 并
 * 设置 inconsistency_message 以描述冲突。
 *
 * @param ctx    欧几里得上下文
 * @param system 目标公理体系
 * @return true 切换成功，false 存在不一致
 */
bool euclidean_set_axiom_system(EuclideanContext *ctx, EuclideanAxiomSystem system) {
    if (!ctx) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "euclidean_set_axiom_system: ctx is NULL");
    }

    if (ctx->active_axiom_system == system) {
        return true;
    }

    EuclideanAxiomSystem old_system = ctx->active_axiom_system;
    ctx->active_axiom_system = system;

    if (!euclidean_check_consistency(ctx)) {
        ctx->active_axiom_system = old_system;
        return false;
    }

    /* 根据新体系调整公理默认启用状态（查找表替代 switch；CUSTOM 与未知体系不修改掩码） */
    static const uint32_t kAxiomSystemMasks[] = {
        [EUCLID_BIRKHOFF] = EUCLID_DEFAULT_AXIOM_MASK,
        [EUCLID_TARSKI] = EUCLID_DEFAULT_AXIOM_MASK,
        [EUCLID_HILBERT] = EUCLID_DEFAULT_AXIOM_MASK,
    };
    if ((unsigned) system < lv_ARRAY_SIZE(kAxiomSystemMasks) && kAxiomSystemMasks[system]) {
        ctx->enabled_axioms_mask = kAxiomSystemMasks[system];
    }

    return true;
}

/**
 * @brief 获取当前活跃的公理体系
 *
 * @param ctx 欧几里得上下文
 * @return 当前活跃的公理体系枚举值（ctx 为 NULL 时返回 EUCLID_HILBERT）
 */
EuclideanAxiomSystem euclidean_get_axiom_system(const EuclideanContext *ctx) {
    if (!ctx) {
        return EUCLID_HILBERT;
    }
    return ctx->active_axiom_system;
}

/**
 * @brief 将上下文绑定到新的约束图
 *
 * 所有后续的几何声明和谓词断言都会作用到此约束图上。
 *
 * @param ctx   欧几里得上下文
 * @param graph 约束图（可为 NULL 以解除绑定）
 */
void euclidean_bind_graph(EuclideanContext *ctx, ConstraintGraph *graph) {
    if (!ctx) {
        return;
    }
    ctx->constraint_graph = graph;
}

/**
 * @brief 启用或禁用特定公理
 *
 * 通过公理 ID 和组别启用或禁用一个公理。
 * 操作后会自动执行一致性检查。
 *
 * @param ctx      欧几里得上下文
 * @param group    公理组别（0=Incidence, 1=Order, 2=Congruence, 3=Parallel, 4=Continuity）
 * @param axiom_id 公理在组内的索引
 * @param enabled  true 启用，false 禁用
 * @return true 操作成功，false 参数无效
 */
static bool euclidean_toggle_axiom(EuclideanContext *ctx, int group, int axiom_id, bool enabled) {
    if (!ctx) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "euclidean_toggle_axiom: ctx is NULL");
    }

    int offset = euclidean_axiom_mask_offset(group, axiom_id);
    if (offset < 0) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "euclidean_toggle_axiom: invalid group/axiom_id");
    }

    if (enabled) {
        ctx->enabled_axioms_mask |= ((uint32_t) 1u << offset);
    } else {
        ctx->enabled_axioms_mask &= ~((uint32_t) 1u << offset);
    }

    euclidean_check_consistency(ctx);
    return true;
}
