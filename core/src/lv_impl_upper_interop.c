/*
 * @file lv_impl_upper_interop.c
 * @brief Lv-00 upper unified impl - L10 interop export layer
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
#include "lv/tikz_export.h"
#include "lv/lv_file.h"
#include "lv/proof.h"

#include "lv/lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
#include "lv/lv_strbuf.h"
#include "lv_impl_upper_internal.h"

/* ============================================================
 * 第10部分:L10 互操作层（导出包装）
 *
 * 导出包装统一语义：把约束图/证明导出为指定格式文本写入调用方 buf，
 * 返回写入字符数（不含 NUL），失败返回负错误码。
 * ============================================================ */

/**
 * @brief 经「临时文件导出 + 读回」模式调用文件式导出 API
 *
 * interop_export_* 家族均写 config.output_path 文件并返回错误码；
 * 上层接口契约是写 buf，故导出到临时文件后读回。失败时清理临时文件。
 *
 * @param input       导出输入对象（ConstraintGraph* 或 ProofNavigator*）
 * @param format      导出格式（INTEROP_EXPORT_SVG / GEOJSON / COQ / LEAN）
 * @param tag         临时文件名标签（如 "svg"）
 * @param id          用于区分临时文件名的对象 ID
 * @param buf         输出缓冲区
 * @param buf_size    缓冲区大小
 * @param file_exporter 真实导出函数（写文件、返回错误码）
 * @return 写入 buf 的字符数；失败返回负错误码
 */
static int64_t upper_export_via_temp_file(const void *input, InteropExportFormat format, const char *tag, int64_t id,
                                          char *buf, int64_t buf_size,
                                          int (*file_exporter)(const void *, const InteropExportConfig *)) {
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_export_via_temp_file: NULL buf or small buf_size");
    if (!input)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_export_via_temp_file: NULL input");

    char path[512];
    snprintf(path, sizeof(path), "lv_%s_%lld.tmp", tag, (long long) id);

    InteropExportConfig config;
    memset(&config, 0, sizeof(config));
    config.format = format;
    config.include_proofs = 0;
    config.pretty_print = 1;
    snprintf(config.output_path, sizeof(config.output_path), "%s", path);

    int rc = file_exporter(input, &config);
    if (rc != lv_OK) {
        remove(path);
        lv_RETURN_ERROR(rc, "upper_export_via_temp_file: export failed");
    }

    if (!lv_file_read_text(path, buf, (size_t) buf_size)) {
        remove(path);
        lv_RETURN_ERROR(lv_ERROR_IO, "upper_export_via_temp_file: read back failed");
    }
    remove(path);
    return (int64_t) strlen(buf);
}

/** 导出为 GeoJSON（委托真实导出引擎 interop_export_geojson） */
int64_t upper_interop_export_geojson(lvEngine *ctx, int64_t graph_id, char *buf, int64_t buf_size) {
    (void) graph_id;
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    return upper_export_via_temp_file(graph, INTEROP_EXPORT_GEOJSON, "geojson", graph_id, buf, buf_size,
                                      (int (*)(const void *, const InteropExportConfig *)) interop_export_geojson);
}

/** 导出为 SVG（委托真实导出引擎 interop_export_svg） */
int64_t upper_interop_export_svg(lvEngine *ctx, int64_t graph_id, char *buf, int64_t buf_size) {
    (void) graph_id;
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    return upper_export_via_temp_file(graph, INTEROP_EXPORT_SVG, "svg", graph_id, buf, buf_size,
                                      (int (*)(const void *, const InteropExportConfig *)) interop_export_svg);
}

/** 导出为 TikZ（内存导出 lv_tikz_export，无临时文件） */
int64_t upper_interop_export_tikz(lvEngine *ctx, int64_t graph_id, char *buf, int64_t buf_size) {
    (void) graph_id;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_interop_export_tikz: NULL buf or small buf_size");
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_interop_export_tikz: NULL graph");
    int n = lv_tikz_export(graph, buf, (size_t) buf_size);
    if (n < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "upper_interop_export_tikz: lv_tikz_export failed");
    return (int64_t) n;
}

/**
 * @brief 导出为 Coq（委托 interop_export_coq，经 proof_navigator_create 接线）
 *
 * 与 interop_command_export.c 的 export_graph_coq 同构：从引擎创建 ProofNavigator，
 * 临时文件导出后读回。无可用引擎时显式报错。
 */
int64_t upper_interop_export_coq(lvEngine *ctx, int64_t proof_id, char *buf, int64_t buf_size) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_interop_export_coq: NULL ctx");
    ProofNavigator *nav = proof_navigator_create(NULL, ctx);
    if (!nav)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "upper_interop_export_coq: proof_navigator_create failed");
    int64_t n = upper_export_via_temp_file(nav, INTEROP_EXPORT_COQ, "coq", proof_id, buf, buf_size,
                                           (int (*)(const void *, const InteropExportConfig *)) interop_export_coq);
    proof_navigator_destroy(nav);
    return n;
}

/**
 * @brief 导出为 Lean4（委托 interop_export_lean，经 proof_navigator_create 接线）
 */
int64_t interop_export_lean4(lvEngine *ctx, int64_t proof_id, char *buf, int64_t buf_size) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "interop_export_lean4: NULL ctx");
    ProofNavigator *nav = proof_navigator_create(NULL, ctx);
    if (!nav)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "interop_export_lean4: proof_navigator_create failed");
    int64_t n = upper_export_via_temp_file(nav, INTEROP_EXPORT_LEAN, "lean4", proof_id, buf, buf_size,
                                           (int (*)(const void *, const InteropExportConfig *)) interop_export_lean);
    proof_navigator_destroy(nav);
    return n;
}

/**
 * @brief 导出为 OPML 大纲（委托 lv_opml_export_navigator，经 proof_navigator_create 接线）
 *
 * opml_codec.c 的 lv_opml_export_navigator 直接写调用方 buf（内存导出，无临时文件）。
 */
int64_t interop_export_opml(lvEngine *ctx, int64_t session_id, char *buf, int64_t buf_size) {
    (void) session_id;
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "interop_export_opml: NULL ctx");
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "interop_export_opml: NULL buf or small buf_size");
    ProofNavigator *nav = proof_navigator_create(NULL, ctx);
    if (!nav)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "interop_export_opml: proof_navigator_create failed");
    int rc = lv_opml_export_navigator(nav, buf, (int) buf_size);
    proof_navigator_destroy(nav);
    if (rc != 0)
        lv_RETURN_ERROR(rc, "interop_export_opml: lv_opml_export_navigator failed");
    return (int64_t) strlen(buf);
}
