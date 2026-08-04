/*
 * @file lv_impl_upper_utils.c
 * @brief Lv-00 upper unified impl - comprehensive utilities
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
 * 第14部分:综合工具函数 -- 为上层提供便捷入口
 * ============================================================ */

/**
 * @brief 从引擎获取全局唯一ID
 *
 * 每次调用递增 s_upper_state.upper_id,返回新ID。
 * 供所有需要唯一标识的上层API使用。
 */
int64_t lv_upper_alloc_id(lvEngine *ctx) {
    (void) ctx;
    return s_upper_state.upper_id++;
}

/**
 * @brief 获取当前全局ID计数器的值(只读)
 */
int64_t lv_upper_get_id_counter(lvEngine *ctx) {
    (void) ctx;
    return s_upper_state.upper_id;
}

/**
 * @brief 执行完整验证流水线(元验证综合入口)
 *
 * 依次调用 consistency / completeness / soundness / differential /
 * 四个检查,返回 AND 结果。
 */
int64_t lv_upper_full_verify(lvEngine *ctx) {
    if (!ctx || !ctx->main_graph)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_upper_full_verify: NULL ctx or main_graph");
    ConstraintGraph *graph = ctx->main_graph;
    int64_t c = meta_verify_consistency(ctx);
    int64_t m = meta_verify_completeness(graph);
    int64_t s = meta_verify_soundness(graph);
    int64_t d = meta_verify_differential(graph, graph);
    return (c && m && s && (d == 0)) ? 1 : 0;
}

/** @brief 综合导出目标条目 -- 名称 + 函数指针 */
typedef struct {
    const char *name; /**< 导出格式名称 */
    int64_t (*fn)(lvEngine *ctx, int64_t id, char *buf, int64_t buf_size); /**< 导出函数 */
} UpperExportEntry;

/** @brief 综合导出的目标表（Coq / Lean4 / SVG,三个导出函数签名一致） */
static const UpperExportEntry kUpperExportTable[] = {
    {"coq", upper_interop_export_coq},
    {"lean4", interop_export_lean4},
    {"svg", upper_interop_export_svg},
};

/**
 * @brief 综合导出 -- 将证明结果同时导出为 Coq / Lean4 / SVG
 *
 * 表驱动遍历 kUpperExportTable,将结果写入对应缓冲区,
 * 返回成功导出的格式数量。
 */
int64_t lv_upper_export_all(lvEngine *ctx, int64_t proof_id, char *coq_buf, int64_t coq_sz, char *lean_buf,
                            int64_t lean_sz, char *svg_buf, int64_t svg_sz) {
    int64_t n = 0;
    char *bufs[] = {coq_buf, lean_buf, svg_buf};
    int64_t szs[] = {coq_sz, lean_sz, svg_sz};
    for (size_t i = 0; i < sizeof(kUpperExportTable) / sizeof(kUpperExportTable[0]); i++) {
        if (kUpperExportTable[i].fn(ctx, proof_id, bufs[i], szs[i]) > 0)
            n++;
    }
    return n;
}

/* ============================================================
 * 文件结束
 *
 * 总计覆盖:
 *   L3 几何扩展        7 函数
 *   L4 预设基础几何   21 函数
 *   预设变换          17 函数
 *   预设测量          17 函数
 *   预设多边形        15 函数
 *   预设代数          14 函数
 *   L6 可视化层       11 函数
 *   L7 编排层          6 函数 + struct
 *   L8 元验证层        5 函数
 *   L9 应用层          5 函数 + struct
 *   L10 互操作层       6 函数
 *   func_block_preset 40 函数
 *   综合工具           4 函数
 * ───────────────────────────
 * 总计              ~168 函数 + 头部, ~1000行
 * ============================================================ */
