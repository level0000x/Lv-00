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

#include "lv/lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
#include "lv/lv_strbuf.h"
#include "lv_impl_upper_internal.h"

/* ============================================================
 * 第10部分:L10 互操作层（导出包装）
 *
 * 导出包装统一语义：把约束图/会话导出为指定格式文本写入调用方 buf，
 * 返回写入字符数（不含 NUL），失败返回负错误码。
 * ============================================================ */

/**
 * @brief 经「临时文件导出 + 读回」模式调用文件式导出 API
 *
 * interop_export_* 家族均写 config.output_path 文件并返回错误码；
 * 上层接口契约是写 buf，故导出到临时文件后读回。失败时清理临时文件。
 *
 * @param graph       约束图（NULL 时返回 INVALID_PARAM）
 * @param tag         临时文件名标签（如 "svg"）
 * @param id          用于区分临时文件名的对象 ID
 * @param buf         输出缓冲区
 * @param buf_size    缓冲区大小
 * @param file_exporter 真实导出函数（写文件、返回错误码）
 * @return 写入 buf 的字符数；失败返回负错误码
 */
static int64_t upper_export_via_temp_file(const ConstraintGraph *graph, const char *tag, int64_t id, char *buf,
                                          int64_t buf_size,
                                          int (*file_exporter)(const ConstraintGraph *, const InteropExportConfig *)) {
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_export_via_temp_file: NULL buf or small buf_size");
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_export_via_temp_file: NULL graph");

    char path[512];
    snprintf(path, sizeof(path), "lv_%s_%lld.tmp", tag, (long long) id);

    InteropExportConfig config;
    memset(&config, 0, sizeof(config));
    config.format = (strcmp(tag, "geojson") == 0) ? INTEROP_EXPORT_GEOJSON : INTEROP_EXPORT_SVG;
    config.include_proofs = 0;
    config.pretty_print = 1;
    snprintf(config.output_path, sizeof(config.output_path), "%s", path);

    int rc = file_exporter(graph, &config);
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
    return upper_export_via_temp_file(graph, "geojson", graph_id, buf, buf_size, interop_export_geojson);
}

/** 导出为 SVG（委托真实导出引擎 interop_export_svg） */
int64_t upper_interop_export_svg(lvEngine *ctx, int64_t graph_id, char *buf, int64_t buf_size) {
    (void) graph_id;
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    return upper_export_via_temp_file(graph, "svg", graph_id, buf, buf_size, interop_export_svg);
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
 * @brief 导出为 Coq
 *
 * 真实导出 interop_export_coq 需要 ProofNavigator；上层接口当前无法从
 * lvEngine 解析证明对象（无 ProofNavigator 访问器），故显式报错而非生成
 * 假证明模板（原实现生成硬编码 True 定理，属静默降级，已移除）。
 */
int64_t upper_interop_export_coq(lvEngine *ctx, int64_t proof_id, char *buf, int64_t buf_size) {
    (void) ctx;
    (void) proof_id;
    (void) buf;
    (void) buf_size;
    lv_RETURN_ERROR(lv_ERROR_UNSUPPORTED,
                    "upper_interop_export_coq: no ProofNavigator accessor on lvEngine (wire when available)");
}

/**
 * @brief 导出为 Lean4
 *
 * 同 upper_interop_export_coq：真实导出 interop_export_lean 需要 ProofNavigator，
 * 上层无访问器，显式报错（原实现生成假 trivial 定理，已移除）。
 */
int64_t interop_export_lean4(lvEngine *ctx, int64_t proof_id, char *buf, int64_t buf_size) {
    (void) ctx;
    (void) proof_id;
    (void) buf;
    (void) buf_size;
    lv_RETURN_ERROR(lv_ERROR_UNSUPPORTED,
                    "interop_export_lean4: no ProofNavigator accessor on lvEngine (wire when available)");
}

/**
 * @brief 导出为 OPML 大纲
 *
 * 当前无 OPML 导出 API（opml_codec 仅注册插件，经 lvInteropManager 调用，
 * 上层接口未接线 manager），显式报错（原实现生成空骨架，已移除）。
 */
int64_t interop_export_opml(lvEngine *ctx, int64_t session_id, char *buf, int64_t buf_size) {
    (void) ctx;
    (void) session_id;
    (void) buf;
    (void) buf_size;
    lv_RETURN_ERROR(lv_ERROR_UNSUPPORTED, "interop_export_opml: no OPML export API wired on upper layer");
}
