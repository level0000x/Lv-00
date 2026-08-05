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
 *
 * 说明:lv_orchestrator_run/get_stage/get_status/get_report/destroy
 * 仅被同为零调用的 lv_application_batch/destroy 引用，整链已按
 * 死代码删除。lv_orchestrator_create 被保留的 lv_application_run
 * 调用，予以保留。
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
