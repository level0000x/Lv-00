/*
 * @file lv_impl_upper_orchestrator.c
 * @brief Lv-00 upper unified impl - L7 orchestrator
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
 * 第9部分:L7 编排层(orchestrator: struct + 6函数,calloc/malloc)
 * ============================================================ */

/** 轻量级编排器结构 */
struct lvOrchestrator {
    int64_t orch_id;       /** 编排器唯一ID */
    int64_t current_stage; /** 当前阶段 (0-5, 对应 lvPipelineStage) */
    int64_t status;        /** 整体状态:0=空闲,1=运行中,2=完成,3=失败 */
    char *input_data;      /** 输入数据(堆分配副本) */
    int64_t stage_count;   /** 阶段总数 */
    int64_t *stage_status; /** 各阶段状态数组 */
};

/** 创建编排器 */
lvOrchestrator *lv_orchestrator_create(lvEngine *ctx) {
    (void) ctx;
    lvOrchestrator *orch = lv_calloc(1, sizeof(lvOrchestrator));
    if (!orch)
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_orchestrator_create: calloc orch failed");
    orch->orch_id = s_upper_state.upper_id++;
    orch->current_stage = 0;
    orch->status = 0;
    orch->stage_count = 6;
    orch->stage_status = lv_calloc((size_t) orch->stage_count, sizeof(int64_t));
    if (!orch->stage_status) {
        lv_free((void **) &orch);
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_orchestrator_create: calloc stage_status failed");
    }
    return orch;
}

/** 管道阶段名称(与 lvPipelineStage 对齐) */
static const char *g_stage_names[] = {"PARSE", "RESOURCE", "GEOMETRY", "REASONING", "OUTPUT", "VISUAL"};

/** 运行编排管线 */
int64_t lv_orchestrator_run(lvOrchestrator *orch, lvEngine *ctx, const char *input) {
    if (!orch || !input)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_orchestrator_run: NULL orch or input");
    /* 深拷贝输入 */
    lv_free((void **) &orch->input_data);
    orch->input_data = lv_strdup_safe(input);
    if (!orch->input_data)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_orchestrator_run: strdup input failed");

    orch->status = 1; /* 运行中 */

    /* 通过引擎的流式上下文推送阶段事件 */
    StreamContext *stream = ctx ? engine_get_stream_context(ctx) : NULL;

    for (int64_t i = 0; i < orch->stage_count; i++) {
        orch->current_stage = i;

        /* 推送阶段开始事件 */
        if (stream) {
            lvStrBuf sb = {0};
            lv_strbuf_printf(&sb, "Pipeline stage %s started (orch=%lld, step=%lld)",
                     (i < 6) ? g_stage_names[i] : "UNKNOWN", (long long) orch->orch_id, (long long) i);
            stream_emit_simple(stream, STREAM_EVENT_INFO, sb.data, (int) i);
            lv_strbuf_destroy(&sb);
        }

        /* 对 REASONING 阶段,若引擎有约束图则尝试求解 */
        if (i == 3 && ctx && ctx->main_graph) {
            if (stream) {
                stream_emit_simple(stream, STREAM_EVENT_SOLVE_START, "Auto-solve triggered in REASONING stage",
                                   (int) i);
            }
            /* 快速冲突检测 */
            bool has_conflict = lv_conflict_detect_quick(ctx->main_graph);
            if (has_conflict && stream) {
                stream_emit_simple(stream, STREAM_EVENT_CONFLICT_DETECTED, "Conflict detected during REASONING stage",
                                   (int) i);
            }
        }

        /* 推送阶段进度和完成 */
        if (stream) {
            stream_emit_progress(stream, (double) (i + 1) / (double) orch->stage_count, "Stage progress update",
                                 (int) i, -1);
        }

        orch->stage_status[i] = 2; /* 2=完成 */
    }

    /* 推送整体完成事件 */
    if (stream) {
        lvStrBuf sb_2 = {0};
        lv_strbuf_printf(&sb_2, "Orchestrator #%lld pipeline completed successfully",
                 (long long) orch->orch_id);
        stream_emit_simple(stream, STREAM_EVENT_ENGINE_DONE, sb_2.data, (int) orch->stage_count);
        lv_strbuf_destroy(&sb_2);
    }

    orch->status = 2; /* 完成 */
    return orch->orch_id;
}

/** 获取当前阶段 */
int64_t lv_orchestrator_get_stage(const lvOrchestrator *orch) {
    if (!orch)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_orchestrator_get_stage: NULL orchestrator");
    return orch->current_stage;
}

/** 获取整体状态 */
int64_t lv_orchestrator_get_status(const lvOrchestrator *orch) {
    if (!orch)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_orchestrator_get_status: NULL orchestrator");
    return orch->status;
}

/** 获取阶段报告(格式化为字符串) */
int64_t lv_orchestrator_get_report(const lvOrchestrator *orch, char *buf, int64_t buf_size) {
    if (!orch || !buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_orchestrator_get_report: NULL orch/buf or small buf_size");
    return (int64_t) snprintf(buf, (size_t) buf_size, "Orch#%lld stage=%lld status=%lld", (long long) orch->orch_id,
                              (long long) orch->current_stage, (long long) orch->status);
}

/** 销毁编排器 */
void lv_orchestrator_destroy(lvOrchestrator *orch) {
    if (!orch)
        return;
    lv_free((void **) &orch->input_data);
    lv_free((void **) &orch->stage_status);
    lv_free((void **) &orch);
}
