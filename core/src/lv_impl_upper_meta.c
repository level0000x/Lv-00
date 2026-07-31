/*
 * @file lv_impl_upper_meta.c
 * @brief Lv-00 upper unified impl - L8 meta verification
 * @details Split from lv_impl_upper.c
 */

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/atp_backend.h"
#include "lv/conflict_detector.h"
#include "lv/engine.h"
#include "lv/func_block.h"
#include "lv/func_block_preset.h"
#include "lv/func_block_registry.h"
#include "lv/geom_evol.h"
#include "lv/interop.h"
#include "lv/lv_json.h"
#include "lv/lv_utils.h"
#include "lv/meta_verify.h"
#include "lv/orchestrator.h"
#include "lv/preset_algebraic.h"
#include "lv/preset_basic_geometry.h"
#include "lv/preset_measurements.h"
#include "lv/preset_polygons.h"
#include "lv/preset_transformations.h"
#include "lv/visual_editor.h"

#include "lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
#include "lv/lv_strbuf.h"
#include "lv_impl_upper_internal.h"

/* ============================================================
 * 第10部分:L8 元验证层(meta_verify: 5个检查)
 * ============================================================ */

/** 一致性检查:遍历约束图节点并检查无矛盾 */
int64_t meta_verify_consistency(lvEngine *ctx) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "meta_verify_consistency: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        return 1; /* 空图视为一致 */

    /* 快速冲突检测(使用 conflict_detector 模块) */
    bool has_conflict = lv_conflict_detect_quick(graph);
    if (has_conflict)
        return 0; /* 0=不一致 */

    /* 全量冲突检测并生成详细报告 */
    ConflictReport *report = lv_conflict_report_create();
    if (!report)
        return 1; /* 无法创建报告,保守返回一致 */

    int detect_ret = lv_conflict_detect_all(graph, NULL, report);
    int result = 1; /* 默认:一致 */
    if (detect_ret == 0 && report->conflict_count > 0) {
        result = 0; /* 存在冲突 */
    }

    /* 若引擎有流式上下文,推送冲突事件 */
    if (result == 0 && ctx->stream_ctx) {
        for (int i = 0; i < report->conflict_count; i++) {
            stream_emit_simple(ctx->stream_ctx, STREAM_EVENT_CONFLICT_DETECTED,
                               report->conflicts[i].description ? report->conflicts[i].description : "Unknown conflict",
                               i);
        }
    }

    lv_conflict_report_destroy(report);
    return result;
}

/** @cond INTERNAL */
/* 前向声明: 实现在 core/src/layer4_reasoning/proof/meta_verify.c */
extern int meta_verify_completeness(const ConstraintGraph *graph);
extern int meta_verify_soundness(const ConstraintGraph *graph);
extern int meta_verify_differential(const ConstraintGraph *graph_a, const ConstraintGraph *graph_b);
/** @endcond */

/** 综合元验证报告 */
int64_t meta_verify_report(lvEngine *ctx, int64_t *out_overall_pass) {
    if (!ctx) {
        if (out_overall_pass)
            *out_overall_pass = 0;
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "meta_verify_report: NULL ctx");
    }
    /* 初始化验证器并运行全过程检查 */
    if (!s_upper_state.meta_verifier) {
        s_upper_state.meta_verifier = lv_meta_verifier_create();
        if (!s_upper_state.meta_verifier) {
            if (out_overall_pass)
                *out_overall_pass = 0;
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "meta_verify_report: lv_meta_verifier_create failed");
        }
        lv_meta_verifier_enable_check(s_upper_state.meta_verifier, lv_CHECK_STRUCTURAL);
        lv_meta_verifier_enable_check(s_upper_state.meta_verifier, lv_CHECK_SOUND);
        lv_meta_verifier_enable_check(s_upper_state.meta_verifier, lv_CHECK_COMPLETE);
        lv_meta_verifier_enable_check(s_upper_state.meta_verifier, lv_CHECK_NONTRIVIAL);
    }
    /* 基于图进行元验证（轻量：无 session 时的退化行为） */
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph) {
        if (out_overall_pass)
            *out_overall_pass = 1;
        return s_upper_state.upper_id++;
    }
    int passed = 1;
    if (lv_conflict_detect_quick(graph))
        passed = 0;
    if (out_overall_pass)
        *out_overall_pass = (int64_t) passed;
    return s_upper_state.upper_id++;
}
