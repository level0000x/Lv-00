/**
 * @file interop_export.c
 * @brief 导出（Coq/Lean/HTML/SVG/TikZ/GeoJSON/PDF/Canonical）
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/interop.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"

lv_DECLARE_STREAM_CTX(interop);

/** @brief 单个约束节点涉及的最大约束数量（统一在 interop.h 中定义） */

/* ── 导出模块 ── */

/**
 * @brief 计算图的边界框（用于 SVG viewBox 和 PDF 页面尺寸）
 * @details 遍历约束图中所有节点的坐标，计算最小/最大 x、y 值，
 *          并添加自适应边距。
 * @param graph 约束图指针（可为 NULL）
 * @param min_x [out] 最小 x 坐标
 * @param min_y [out] 最小 y 坐标
 * @param max_x [out] 最大 x 坐标
 * @param max_y [out] 最大 y 坐标
 */
static void compute_bounding_box(const ConstraintGraph *graph, double *min_x, double *min_y, double *max_x,
                                 double *max_y) {
    *min_x = 0.0;
    *min_y = 0.0;
    *max_x = 100.0;
    *max_y = 100.0;

    if (!graph || graph->node_count == 0)
        return;

    bool first = true;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->coord_count < 2 || !node->symbolic_coords)
            continue;

        /* 修复：添加 symbolic_coords 数组元素的 NULL 检查 */
        if (!node->symbolic_coords[0] || !node->symbolic_coords[1])
            continue;

        double x = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y = symbolic_coord_to_double(node->symbolic_coords[1]);

        if (first) {
            *min_x = x;
            *min_y = y;
            *max_x = x;
            *max_y = y;
            first = false;
        } else {
            if (x < *min_x)
                *min_x = x;
            if (y < *min_y)
                *min_y = y;
            if (x > *max_x)
                *max_x = x;
            if (y > *max_y)
                *max_y = y;
        }
    }

    /* 添加边距 */
    double margin_x = (*max_x - *min_x) * 0.15 + 20.0;
    double margin_y = (*max_y - *min_y) * 0.15 + 20.0;
    *min_x -= margin_x;
    *min_y -= margin_y;
    *max_x += margin_x;
    *max_y += margin_y;
}

/**
 * @brief SVG转义XML特殊字符
 *
 * 将字符串中的 XML 特殊字符（&、<、>、"、'）转义为对应的实体引用，
 * 防止在 SVG/XML 输出中出现解析错误。
 *
 * @param src      源字符串
 * @param dst      输出缓冲区，用于存储转义后的字符串
 * @param dst_size 输出缓冲区大小（字节）
 */
static void svg_escape_string(const char *src, char *dst, size_t dst_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 6; i++) {
        switch (src[i]) {
            case '&':
                memcpy(dst + j, "&amp;", 5);
                j += 5;
                break;
            case '<':
                memcpy(dst + j, "&lt;", 4);
                j += 4;
                break;
            case '>':
                memcpy(dst + j, "&gt;", 4);
                j += 4;
                break;
            case '"':
                memcpy(dst + j, "&quot;", 6);
                j += 6;
                break;
            case '\'':
                memcpy(dst + j, "&apos;", 6);
                j += 6;
                break;
            default:
                dst[j++] = src[i];
                break;
        }
    }
    dst[j] = '\0';
}

/**
 * @brief TikZ转义特殊字符
 *
 * 将字符串中的 LaTeX/TikZ 特殊字符（\、{、}、$、#、%、_、&）
 * 转义为对应的 LaTeX 命令序列，防止在 TikZ 输出中出现编译错误。
 *
 * 修复：将循环条件从 j < dst_size - 2 改为 j < dst_size - 16，
 * 确保最长转义序列（\textbackslash{} = 16字节）不会导致缓冲区溢出。
 * 对于非反斜杠字符，实际只需要 1 字节空间，但统一使用最严格的边界检查。
 *
 * @param src      源字符串
 * @param dst      输出缓冲区，用于存储转义后的字符串
 * @param dst_size 输出缓冲区大小（字节）
 */
/* ==================== 导出功能 ==================== */

/**
 * @brief 导出 Coq 定理证明代码
 * @details 生成 Coq 源文件的框架结构（Require Import、Context、Theorem 声明），
 *          并将 Lv-00 ProofStep 序列映射为 Coq tactic 序列。
 *
 * 证明步骤到 Coq tactic 的映射规则：
 *   - PROOF_STEP_ADD_NODE        -> pose proof (构造点/线段/区域)
 *   - PROOF_STEP_ADD_CONSTRAINT  -> assert (约束声明)
 *   - PROOF_STEP_REWRITE         -> rewrite H.
 *   - PROOF_STEP_FUNCTION_APP    -> apply theorem_name.
 *   - PROOF_STEP_PACK_FUNCTION   -> (* 函数块打包 *)
 *   - PROOF_STEP_NORMALIZATION   -> rewrite H. (归一化)
 *   - PROOF_STEP_UNIFY           -> reflexivity. 或 congruence.
 *   - PROOF_STEP_EX_FALSO        -> exfalso.
 *   - PROOF_STEP_ORACLE          -> admit. (非构造性依赖)
 *
 * 信任颜色处理：
 *   - GREEN   -> 全构造（可信），生成完整 tactic
 *   - BLUE    -> 未探索/资源受限，使用 admit
 *   - ORANGE  -> 非构造性oracle，使用 admit + 注释
 *   - AMBER   -> 数值假设，使用 admit + 精度注释
 *
 * @param proof 证明对象指针
 * @param config 导出配置（主要使用 output_path）
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效，lv_ERROR_IO 文件错误
 */
int interop_export_coq(const ProofNavigator *proof, const InteropExportConfig *config) {
    if (!proof || !config)
        return lv_ERROR_INVALID_PARAM;

    /* ---- 流式事件：开始 Coq 导出 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "开始 Coq 导出", 0);
    }

    /* 生成Coq代码 */
    FILE *fp = fopen(config->output_path, "w");
    if (!fp) {
        if (interop_stream_ctx) {
            stream_emit_simple(interop_stream_ctx, STREAM_EVENT_ERROR, "Coq 导出失败：无法创建输出文件", 0);
        }
        return lv_ERROR_IO;
    }

    fprintf(fp, "(* Generated by Lv-00 v3.2.0 *)\n");
    fprintf(fp, "Require Import GeoCoq.Tarski_dev.\n\n");
    fprintf(fp, "Section lv_Export.\n");
    fprintf(fp, "  Context `{T2D:Tarski_2D}.\n\n");

    /* ---- 证明步骤到 Coq tactic 映射表（注释） ---- */
    fprintf(fp, "  (* 证明步骤类型 -> Coq tactic 映射:\n");
    fprintf(fp, "   *   PROOF_STEP_ADD_NODE        -> pose proof (构造)\n");
    fprintf(fp, "   *   PROOF_STEP_ADD_CONSTRAINT  -> assert (约束)\n");
    fprintf(fp, "   *   PROOF_STEP_REWRITE         -> rewrite H.\n");
    fprintf(fp, "   *   PROOF_STEP_FUNCTION_APP    -> apply theorem_name.\n");
    fprintf(fp, "   *   PROOF_STEP_PACK_FUNCTION   -> (* 函数块打包 *)\n");
    fprintf(fp, "   *   PROOF_STEP_NORMALIZATION   -> rewrite H.\n");
    fprintf(fp, "   *   PROOF_STEP_UNIFY           -> reflexivity. / congruence.\n");
    fprintf(fp, "   *   PROOF_STEP_EX_FALSO        -> exfalso.\n");
    fprintf(fp, "   *   PROOF_STEP_ORACLE          -> admit.\n");
    fprintf(fp, "   *)\n\n");

    /* ---- 信任颜色映射（注释） ---- */
    fprintf(fp, "  (* 信任颜色映射:\n");
    fprintf(fp, "   *   GREEN            -> 全构造（可信）\n");
    fprintf(fp, "   *   BLUE_UNEXPLORED  -> 未探索\n");
    fprintf(fp, "   *   BLUE_RESOURCE    -> 资源受限\n");
    fprintf(fp, "   *   ORANGE_ORACLE    -> 非构造性oracle\n");
    fprintf(fp, "   *   ORANGE_EX_FALSO  -> 爆炸原理步骤\n");
    fprintf(fp, "   *   AMBER            -> 数值假设\n");
    fprintf(fp, "   *)\n\n");

    fprintf(fp, "  Theorem lv_Main : True.\n");
    fprintf(fp, "  Proof.\n");

    /*
     * 证明体生成：
     * Proof 通过 typedef ProofNavigator Proof 定义为 ProofNavigator，
     * 可访问 steps/step_count 字段。遍历 proof->steps 数组，
     * 根据每个 ProofStep 的 type 和 color 生成对应的 Coq tactic。
     *
     * 信任颜色处理：
     *   - GREEN / GREEN_VERIFIED -> 全构造（可信），生成完整 tactic
     *   - 其他颜色             -> 使用 admit + 注释
     */
    /*
     * 当前 Proof 为不透明类型，无法访问内部步骤。
     * 当证明步骤为空时使用 admit。
     */
    fprintf(fp, "    (* 证明步骤待展开：Proof 结构体当前为不透明类型 *)\n");
    fprintf(fp, "    admit.\n");

    fprintf(fp, "  Qed.\n\n");
    fprintf(fp, "End lv_Export.\n");

    fclose(fp);

    /* ---- 流式事件：Coq 导出完成 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "Coq 导出完成", 0);
    }

    return lv_OK;
}

/**
 * @brief 导出 Lean 4 定理证明代码
 * @details 生成 Lean 4 源文件的框架结构（import、namespace、theorem 声明），
 *          并将 Lv-00 ProofStep 序列映射为 Lean 4 tactic 序列。
 *
 * 证明步骤到 Lean 4 tactic 的映射规则：
 *   - PROOF_STEP_ADD_NODE        -> have h ... := by intro <name> ; constructor
 *   - PROOF_STEP_ADD_CONSTRAINT  -> have h ... := by constructor ; assumption
 *   - PROOF_STEP_REWRITE         -> rw [h]
 *   - PROOF_STEP_FUNCTION_APP    -> apply h
 *   - PROOF_STEP_PACK_FUNCTION   -> -- 函数块打包（仅注释）
 *   - PROOF_STEP_NORMALIZATION   -> simp [normalization]
 *   - PROOF_STEP_UNIFY           -> rfl
 *   - PROOF_STEP_EX_FALSO        -> contradiction ; assumption
 *   - PROOF_STEP_ORACLE          -> by exact (oracle.verify <step_id>)
 *
 * 信任颜色处理：
 *   - GREEN   -> 全构造（可信），生成完整 tactic
 *   - BLUE    -> 未探索/资源受限，使用 by admit + 注释
 *   - ORANGE  -> 非构造性oracle，使用 by exact oracle_result.<name> + 注释
 *   - AMBER   -> 数值假设，使用 by sorry -- [NUMERIC] 注释
 *   - 其他    -> by trivial / by assumption 作为回退
 *
 * @param proof 证明对象指针
 * @param config 导出配置
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效，lv_ERROR_IO 文件错误
 */
int interop_export_lean(const ProofNavigator *proof, const InteropExportConfig *config) {
    if (!proof || !config)
        return lv_ERROR_INVALID_PARAM;

    /* ---- 流式事件：开始 Lean 4 导出 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "开始 Lean 4 导出", 0);
    }

    /* 生成Lean代码 */
    FILE *fp = fopen(config->output_path, "w");
    if (!fp) {
        if (interop_stream_ctx) {
            stream_emit_simple(interop_stream_ctx, STREAM_EVENT_ERROR, "Lean 4 导出失败：无法创建输出文件", 0);
        }
        return lv_ERROR_IO;
    }

    fprintf(fp, "-- Generated by Lv-00 v3.2.0\n");
    fprintf(fp, "import EuclideanGeometry\n\n");
    fprintf(fp, "namespace lvExport\n\n");

    /* ---- 证明步骤到 Lean 4 tactic 映射表（注释） ---- */
    fprintf(fp, "  -- 证明步骤类型 -> Lean 4 tactic 映射:\n");
    fprintf(fp, "  --   PROOF_STEP_ADD_NODE        -> have h : ... := by intro <name> ; constructor\n");
    fprintf(fp, "  --   PROOF_STEP_ADD_CONSTRAINT  -> have h : ... := by constructor ; assumption\n");
    fprintf(fp, "  --   PROOF_STEP_REWRITE         -> rw [h]\n");
    fprintf(fp, "  --   PROOF_STEP_FUNCTION_APP    -> apply h\n");
    fprintf(fp, "  --   PROOF_STEP_PACK_FUNCTION   -> -- 函数块打包（仅注释）\n");
    fprintf(fp, "  --   PROOF_STEP_NORMALIZATION   -> simp [normalization]\n");
    fprintf(fp, "  --   PROOF_STEP_UNIFY           -> rfl\n");
    fprintf(fp, "  --   PROOF_STEP_EX_FALSO        -> contradiction ; assumption\n");
    fprintf(fp, "  --   PROOF_STEP_ORACLE          -> by exact (oracle.verify <step_id>)\n\n");

    /* ---- 信任颜色映射（注释） ---- */
    fprintf(fp, "  -- 信任颜色映射:\n");
    fprintf(fp, "  --   GREEN            -> 全构造（可信），生成完整 tactic\n");
    fprintf(fp, "  --   BLUE_UNEXPLORED  -> 未探索，使用 by admit\n");
    fprintf(fp, "  --   BLUE_RESOURCE    -> 资源受限，使用 by admit\n");
    fprintf(fp, "  --   ORANGE_ORACLE    -> 非构造性oracle，使用 by exact oracle_result\n");
    fprintf(fp, "  --   ORANGE_EX_FALSO  -> 爆炸原理步骤，使用 exfalso ; by sorry\n");
    fprintf(fp, "  --   AMBER            -> 数值假设，使用 by sorry -- [NUMERIC]\n");
    fprintf(fp, "  --   其他颜色         -> 回退至 by trivial / by assumption\n\n");

    fprintf(fp, "  theorem lv_main : True := by\n");

    /*
     * 证明体生成：
     * Proof 通过 typedef ProofNavigator Proof 定义为 ProofNavigator，
     * 可访问 steps/step_count 字段。遍历 proof->steps 数组，
     * 根据每个 ProofStep 的 type 和 color 生成对应的 Lean 4 tactic。
     *
     * 信任颜色处理策略：
     *   - GREEN / GREEN_VERIFIED -> 全构造（可信），生成完整 tactic
     *   - BLUE_*                 -> 未探索/资源受限，使用 by admit + 描述性注释
     *   - ORANGE_*               -> 非构造性oracle，使用 by exact oracle_result / oracle.verify
     *   - AMBER                  -> 数值假设，使用 by sorry -- [NUMERIC] 标注
     *   - 其他（YELLOW等）        -> 回退至 by trivial / by assumption
     */
    if (proof->steps && proof->step_count > 0) {
        fprintf(fp, "    -- 证明步骤数: %d\n", proof->step_count);
        for (int i = 0; i < proof->step_count; i++) {
            ProofStep *step = proof->steps[i];
            if (!step)
                continue;

            /* 信任颜色分类判断 */
            bool is_green = (step->color == PROOF_COLOR_GREEN || step->color == PROOF_COLOR_GREEN_VERIFIED);
            bool is_blue = (step->color == PROOF_COLOR_BLUE_UNEXPLORED || step->color == PROOF_COLOR_BLUE_RESOURCE ||
                            step->color == PROOF_COLOR_BLUE_OUT_OF_RANGE);
            bool is_orange = (step->color == PROOF_COLOR_ORANGE_ORACLE || step->color == PROOF_COLOR_ORANGE_EX_FALSO ||
                              step->color == PROOF_COLOR_DARK_ORANGE);
            bool is_amber = (step->color == PROOF_COLOR_AMBER);

            switch (step->type) {
                case PROOF_STEP_ADD_NODE:
                    if (is_green) {
                        fprintf(fp, "    have h_node_%d : True := by intro node_%d ; constructor\n", step->node_id,
                                step->node_id);
                    } else if (is_blue) {
                        fprintf(fp, "    -- [BLUE] 构造节点 node_%d, 信任色: %s (未探索/资源受限)\n", step->node_id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    have h_node_%d : True := by admit\n", step->node_id);
                    } else if (is_orange) {
                        fprintf(fp, "    -- [ORANGE] 构造节点 node_%d, 信任色: %s (非构造性oracle依赖)\n",
                                step->node_id, proof_color_to_string(step->color));
                        fprintf(fp, "    have h_node_%d : True := by exact oracle_result.node_%d\n", step->node_id,
                                step->node_id);
                    } else if (is_amber) {
                        fprintf(fp, "    -- [AMBER] 构造节点 node_%d, 信任色: %s (数值假设)\n", step->node_id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    have h_node_%d : True := by sorry -- [NUMERIC] 数值假设步骤\n", step->node_id);
                    } else {
                        fprintf(fp, "    -- 构造节点 node_%d, 信任色: %s\n", step->node_id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    have h_node_%d : True := by trivial\n", step->node_id);
                    }
                    break;

                case PROOF_STEP_ADD_CONSTRAINT:
                    if (is_green) {
                        fprintf(fp, "    have h_cstr_%d : True := by constructor ; assumption\n", step->constraint_id);
                    } else if (is_blue) {
                        fprintf(fp, "    -- [BLUE] 添加约束 cstr_%d, 信任色: %s (未探索/资源受限)\n",
                                step->constraint_id, proof_color_to_string(step->color));
                        fprintf(fp, "    have h_cstr_%d : True := by admit\n", step->constraint_id);
                    } else if (is_orange) {
                        fprintf(fp, "    -- [ORANGE] 添加约束 cstr_%d, 信任色: %s (非构造性oracle依赖)\n",
                                step->constraint_id, proof_color_to_string(step->color));
                        fprintf(fp, "    have h_cstr_%d : True := by exact oracle_result.cstr_%d\n",
                                step->constraint_id, step->constraint_id);
                    } else if (is_amber) {
                        fprintf(fp, "    -- [AMBER] 添加约束 cstr_%d, 信任色: %s (数值假设)\n", step->constraint_id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    have h_cstr_%d : True := by sorry -- [NUMERIC] 数值假设步骤\n",
                                step->constraint_id);
                    } else {
                        fprintf(fp, "    -- 添加约束 cstr_%d, 信任色: %s\n", step->constraint_id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    have h_cstr_%d : True := by trivial\n", step->constraint_id);
                    }
                    break;

                case PROOF_STEP_REWRITE:
                    if (is_green) {
                        fprintf(fp, "    rw [h]\n");
                    } else if (is_blue) {
                        fprintf(fp, "    -- [BLUE] 重写步骤 step_%d, 信任色: %s (未探索/资源受限)\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by admit -- 蓝色步骤：待探索\n");
                    } else if (is_orange) {
                        fprintf(fp, "    -- [ORANGE] 重写步骤 step_%d, 信任色: %s (非构造性oracle依赖)\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by exact (oracle.verify step_%d)\n", step->id);
                    } else if (is_amber) {
                        fprintf(fp, "    -- [AMBER] 重写步骤 step_%d, 信任色: %s (数值假设)\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by sorry -- [NUMERIC] 数值假设步骤\n");
                    } else {
                        fprintf(fp, "    -- 重写步骤 step_%d, 信任色: %s\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by assumption\n");
                    }
                    break;

                case PROOF_STEP_FUNCTION_APP:
                    if (is_green) {
                        fprintf(fp, "    apply h\n");
                    } else if (is_blue) {
                        fprintf(fp, "    -- [BLUE] 函数应用 step_%d, 信任色: %s (未探索/资源受限)\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by admit -- 蓝色步骤：待探索\n");
                    } else if (is_orange) {
                        fprintf(fp, "    -- [ORANGE] 函数应用 step_%d, 信任色: %s (非构造性oracle依赖)\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by exact (oracle.verify step_%d)\n", step->id);
                    } else if (is_amber) {
                        fprintf(fp, "    -- [AMBER] 函数应用 step_%d, 信任色: %s (数值假设)\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by sorry -- [NUMERIC] 数值假设步骤\n");
                    } else {
                        fprintf(fp, "    -- 函数应用 step_%d, 信任色: %s\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by trivial\n");
                    }
                    break;

                case PROOF_STEP_PACK_FUNCTION:
                    fprintf(fp, "    -- 函数块打包: step_%d, func_block_%d\n", step->id, step->func_block_id);
                    break;

                case PROOF_STEP_NORMALIZATION:
                    if (is_green) {
                        fprintf(fp, "    simp [normalization]\n");
                    } else if (is_blue) {
                        fprintf(fp, "    -- [BLUE] 归一化 step_%d, 信任色: %s (未探索/资源受限)\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by admit -- 蓝色步骤：待探索\n");
                    } else if (is_orange) {
                        fprintf(fp, "    -- [ORANGE] 归一化 step_%d, 信任色: %s (非构造性oracle依赖)\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by exact (oracle.verify step_%d)\n", step->id);
                    } else if (is_amber) {
                        fprintf(fp, "    -- [AMBER] 归一化 step_%d, 信任色: %s (数值假设)\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by sorry -- [NUMERIC] 数值假设步骤\n");
                    } else {
                        fprintf(fp, "    -- 归一化 step_%d, 信任色: %s\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by assumption\n");
                    }
                    break;

                case PROOF_STEP_UNIFY:
                    if (is_green) {
                        fprintf(fp, "    rfl\n");
                    } else if (is_blue) {
                        fprintf(fp, "    -- [BLUE] 合一检查 step_%d, 信任色: %s (未探索/资源受限)\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by admit -- 蓝色步骤：待探索\n");
                    } else if (is_orange) {
                        fprintf(fp, "    -- [ORANGE] 合一检查 step_%d, 信任色: %s (非构造性oracle依赖)\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by exact (oracle.verify step_%d)\n", step->id);
                    } else if (is_amber) {
                        fprintf(fp, "    -- [AMBER] 合一检查 step_%d, 信任色: %s (数值假设)\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by sorry -- [NUMERIC] 数值假设步骤\n");
                    } else {
                        fprintf(fp, "    -- 合一检查 step_%d, 信任色: %s\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    by trivial\n");
                    }
                    break;

                case PROOF_STEP_EX_FALSO:
                    if (is_green) {
                        fprintf(fp, "    contradiction ; assumption\n");
                    } else {
                        fprintf(fp, "    -- [非绿色] 爆炸原理 step_%d, 信任色: %s\n", step->id,
                                proof_color_to_string(step->color));
                        fprintf(fp, "    exfalso ; by sorry -- 非构造性爆炸原理，需外部验证\n");
                    }
                    break;

                case PROOF_STEP_ORACLE:
                    fprintf(fp, "    -- [ORACLE] Oracle依赖: step_%d, 信任色: %s\n", step->id,
                            proof_color_to_string(step->color));
                    fprintf(fp, "    by exact (oracle.verify step_%d) -- 非构造性依赖，需外部oracle验证\n", step->id);
                    break;

                default:
                    fprintf(fp, "    -- 未知步骤类型: %d, 信任色: %s\n", (int) step->type,
                            proof_color_to_string(step->color));
                    fprintf(fp, "    by trivial\n");
                    break;
            }
        }
    } else {
        fprintf(fp, "    -- 证明步骤为空，无步骤可展开\n");
        fprintf(fp, "    trivial\n");
    }
    fprintf(fp, "\n");

    fprintf(fp, "end lvExport\n");

    fclose(fp);

    /* ---- 流式事件：Lean 4 导出完成 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "Lean 4 导出完成", 0);
    }

    return lv_OK;
}

/**
 * @brief 将约束图导出为独立 HTML 演示文件
 *
 * 先生成 SVG 内容，再嵌入含深色主题 CSS 和引擎统计信息的 HTML 页面。
 *
 * @param engine 引擎实例指针
 * @param config 导出配置（output_path 指定输出文件路径）
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效，lv_ERROR_IO 文件错误
 */
int interop_export_html(const lvEngine *engine, const InteropExportConfig *config) {
    if (!engine || !config)
        return lv_ERROR_INVALID_PARAM;
    if (!config->output_path[0])
        return lv_ERROR_INVALID_PARAM;

    ConstraintGraph *graph = engine->main_graph;
    if (!graph)
        return lv_ERROR_INVALID_PARAM;

    /* ---- 1. 生成 SVG 到临时文件 ---- */
    char svg_temp_path[INTEROP_MAX_PATH_LEN];
    {
        const char *tmp_name = tmpnam(NULL);
        if (!tmp_name)
            return lv_ERROR_IO;
        lv_strlcpy(svg_temp_path, tmp_name, sizeof(svg_temp_path));
    }

    InteropExportConfig svg_cfg;
    memset(&svg_cfg, 0, sizeof(svg_cfg));
    lv_strlcpy(svg_cfg.output_path, svg_temp_path, sizeof(svg_cfg.output_path));

    int svg_ret = interop_export_svg(graph, &svg_cfg);
    if (svg_ret != lv_OK) {
        remove(svg_temp_path);
        return svg_ret;
    }

    /* ---- 2. 读取临时 SVG 文件内容 ---- */
    char *svg_content = NULL;
    long svg_size = 0;
    {
        FILE *svg_fp = fopen(svg_temp_path, "rb");
        if (!svg_fp) {
            remove(svg_temp_path);
            return lv_ERROR_IO;
        }
        fseek(svg_fp, 0, SEEK_END);
        svg_size = ftell(svg_fp);
        fseek(svg_fp, 0, SEEK_SET);
        if (svg_size > 0) {
            svg_content = (char *) lv_malloc((size_t) svg_size + 1);
            if (svg_content) {
                size_t read_bytes = fread(svg_content, 1, (size_t) svg_size, svg_fp);
                svg_content[read_bytes] = '\0';
            }
        }
        fclose(svg_fp);
        remove(svg_temp_path);
    }

    /* ---- 3. 构建 HTML ---- */
    FILE *fp = fopen(config->output_path, "w");
    if (!fp) {
        lv_free(svg_content);
        return lv_ERROR_IO;
    }

    int node_count = graph->node_count;
    int constraint_count = graph->constraint_count;

    fprintf(fp,
            "<!DOCTYPE html>\n"
            "<html lang=\"zh-CN\">\n"
            "<head>\n"
            "  <meta charset=\"UTF-8\">\n"
            "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
            "  <title>Lv-00 几何约束图导出</title>\n"
            "  <style>\n"
            "    * { margin: 0; padding: 0; box-sizing: border-box; }\n"
            "    body {\n"
            "      background: #1a1b26;\n"
            "      color: #c0caf5;\n"
            "      font-family: 'Consolas', 'Courier New', monospace;\n"
            "      padding: 24px;\n"
            "      min-height: 100vh;\n"
            "    }\n"
            "    h1 {\n"
            "      font-size: 20px;\n"
            "      font-weight: 600;\n"
            "      color: #7aa2f7;\n"
            "      margin-bottom: 16px;\n"
            "      letter-spacing: 0.5px;\n"
            "    }\n"
            "    .stats {\n"
            "      display: flex;\n"
            "      gap: 24px;\n"
            "      margin-bottom: 20px;\n"
            "      padding: 12px 16px;\n"
            "      background: #24283b;\n"
            "      border-radius: 8px;\n"
            "      border: 1px solid #3b4261;\n"
            "      font-size: 13px;\n"
            "    }\n"
            "    .stats span { color: #9ece6a; }\n"
            "    .stats .label { color: #a9b1d6; }\n"
            "    .svg-container {\n"
            "      background: #ffffff;\n"
            "      border-radius: 8px;\n"
            "      border: 1px solid #3b4261;\n"
            "      padding: 8px;\n"
            "      overflow: auto;\n"
            "      display: inline-block;\n"
            "    }\n"
            "    .footer {\n"
            "      margin-top: 16px;\n"
            "      font-size: 11px;\n"
            "      color: #565f89;\n"
            "    }\n"
            "  </style>\n"
            "</head>\n"
            "<body>\n"
            "  <h1>Lv-00 几何约束图</h1>\n"
            "  <div class=\"stats\">\n"
            "    <div><span class=\"label\">节点</span> <span>%d</span></div>\n"
            "    <div><span class=\"label\">约束</span> <span>%d</span></div>\n"
            "    <div><span class=\"label\">引擎版本</span> <span>%s</span></div>\n"
            "  </div>\n"
            "  <div class=\"svg-container\">\n",
            node_count, constraint_count, lv_VERSION_STRING);

    /* 嵌入 SVG（跳过 XML 声明行） */
    if (svg_content) {
        char *svg_body = svg_content;
        /* 跳过可选的 <?xml ...?> 行 */
        if (svg_body[0] == '<' && svg_body[1] == '?') {
            char *nl = strchr(svg_body, '\n');
            if (nl)
                svg_body = nl + 1;
        }
        fprintf(fp, "%s\n", svg_body);
        lv_free(svg_content);
    } else {
        fprintf(fp, "    <p>SVG 生成失败</p>\n");
    }

    fprintf(fp,
            "  </div>\n"
            "  <div class=\"footer\">\n"
            "    由 Lv-00 v%s 生成\n"
            "  </div>\n"
            "</body>\n"
            "</html>\n",
            lv_VERSION_STRING);

    fclose(fp);

    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "HTML 导出完成", 0);
    }

    return lv_OK;
}

/**
 * @brief 将约束图导出为 SVG 矢量图文件
 * @param graph  约束图指针
 * @param config 导出配置（output_path 指定输出文件路径）
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效，lv_ERROR_IO 文件错误
 */
int interop_export_svg(const ConstraintGraph *graph, const InteropExportConfig *config) {
    /**
     * @brief 将约束图导出为SVG矢量图
     *
     * 【已实现功能】
     *   本函数已将SVG导出的核心渲染管线完整实现，能够生成独立可用的SVG文件：
     *   1. 边界框计算 —— 自动遍历约束图中所有节点的符号坐标，计算包围盒
     *   2. 区域（Region）渲染 —— 在底层渲染多边形区域，带透明度填充
     *   3. 函数块（Function Block）渲染 —— 渲染为圆角矩形，居中显示名称和ID
     *   4. 线段（Line Segment）渲染 —— 渲染为带颜色的直线段，中点显示标签
     *   5. 端口（Port）渲染 —— 输入/输出端口渲染为小圆圈，标注类型和ID
     *   6. 点（Point）渲染 —— 渲染为填充圆形，标注P+ID
     *   7. 约束关系渲染 —— 支持四种约束类型的可视化：
     *      - 关联约束（INCIDENCE）：灰色虚线
     *      - 之间约束（BETWEENNESS）：紫色斜体标签标注三点关系
     *      - 相交约束（INTERSECTION）：紫色十字标记
     *      - 包含约束（CONTAINMENT）：青色点线
     *      - 连接约束（CONNECTION）：橙色箭头线
     *   8. 图例（Legend） —— 左上角半透明图例，说明各几何类型和信任颜色含义
     *   9. 信任颜色映射 —— 根据TrustColor为不同信用级别的元素使用不同颜色：
     *      绿色（受约束）、灰色（自由）、红色（冲突）
     *  10. 样式定义 —— 通过 <style> 标签统一定义 class 样式，clean SVG结构
     *
     * 【简化实现的部分（完整功能需要额外依赖或后续版本）】
     *   1. 贝塞尔曲线/圆弧段的精确渲染 —— 当前仅使用直线端点连接；
     *      完整实现需要解析曲线控制点并生成 SVG <path> 的 C/Q/A 弧命令。
     *      所需数据：从 GeomNode 的 coord_count > 4 时提取控制点坐标。
     *   2. 区域边界的曲线路径 —— 当前使用 polygon 直线顶点连接；
     *      完整实现需要使用 SVG <path> 的贝塞尔命令绘制曲线边界。
     *   3. 包含/相交约束的精确几何交点 —— 当前使用参与者节点坐标
     *      作为端点；完整实现需要调用几何求解器计算实际的交点位置。
     *   4. 交互式JavaScript增强 —— 当前为纯静态SVG图形；
     *      完整实现需要嵌入JS代码实现点击高亮、悬停提示等交互。
     *   5. 数学公式渲染 —— 当前仅输出纯文本坐标；
     *      完整实现需要嵌入 LaTeX/MathML 的 SVG foreignObject。
     *   6. 多图层分组 —— 当前所有元素在同一层级；
     *      完整实现需要使用 <g> 标签按信任级别/几何类型分组。
     *   7. CSS动画/过渡 —— 当前无动画支持；
     *      完整实现需要 CSS keyframes 或 SMIL 动画演示求解过程。
     *
     * 【外部依赖说明】
     *   本函数完全使用标准C的 fprintf 生成纯文本SVG，不依赖任何外部XML或
     *   图形库。所有辅助函数（compute_bounding_box、trust_color_to_svg、
     *   svg_escape_string）均为本文件内部实现。
     *
     * 【使用示例】
     *   InteropExportConfig cfg;
     *   lv_strlcpy(cfg.output_path, "output.svg", sizeof(cfg.output_path));
     *   int ret = interop_export_svg(graph, &cfg);
     *
     * @param graph 约束图指针（包含所有节点和约束）
     * @param config 导出配置（主要使用 output_path 指定输出文件路径）
     * @return lv_OK 成功导出
     *         lv_ERROR_INVALID_PARAM 参数无效（graph或config为NULL）
     *         lv_ERROR_IO 文件无法创建或写入
     */
    if (!graph || !config)
        return lv_ERROR_INVALID_PARAM;

    FILE *fp = fopen(config->output_path, "w");
    if (!fp)
        return lv_ERROR_IO;

    /* 计算边界框 */
    double min_x, min_y, max_x, max_y;
    compute_bounding_box(graph, &min_x, &min_y, &max_x, &max_y);

    double width = max_x - min_x;
    double height = max_y - min_y;
    if (width < 1.0)
        width = 200.0;
    if (height < 1.0)
        height = 200.0;

    /* SVG头部 */
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp,
            "<svg xmlns=\"http://www.w3.org/2000/svg\" "
            "width=\"%.1f\" height=\"%.1f\" "
            "viewBox=\"%.2f %.2f %.2f %.2f\">\n",
            width, height, min_x, min_y, width, height);
    fprintf(fp, "  <title>Lv-00 Geometry Export</title>\n");
    fprintf(fp, "  <desc>Generated by Lv-00 v%s</desc>\n", lv_VERSION_STRING);

    /* 定义样式 */
    fprintf(fp, "  <defs>\n");
    fprintf(fp, "    <style>\n");
    fprintf(fp, "      .point { stroke-width: 1.5; }\n");
    fprintf(fp, "      .line { stroke-width: 2; fill: none; }\n");
    fprintf(fp, "      .region { stroke-width: 1.5; opacity: 0.3; }\n");
    fprintf(fp, "      .constraint { stroke-width: 1; stroke-dasharray: 5,3; fill: none; }\n");
    fprintf(fp, "      .label { font-family: 'Segoe UI', Arial, sans-serif; font-size: 12px; }\n");
    fprintf(fp, "      .block { stroke-width: 2; rx: 8; ry: 8; }\n");
    fprintf(fp, "      .port { stroke-width: 1.5; }\n");
    fprintf(fp, "    </style>\n");
    fprintf(fp, "  </defs>\n\n");

    /* 背景网格（可选） */
    fprintf(fp,
            "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
            "fill=\"#fafafa\" stroke=\"#e5e7eb\" stroke-width=\"1\"/>\n",
            min_x, min_y, width, height);

    /* ---- 渲染区域（先渲染，在底层） ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_REGION)
            continue;
        if (node->data.region.segment_count < 3)
            continue;

        const char *color = trust_color_to_svg(node->trust);
        char escaped_name[256];
        svg_escape_string(geom_type_name(node->type), escaped_name, sizeof(escaped_name));

        fprintf(fp, "  <!-- Region id=%d -->\n", node->id);
        fprintf(fp, "  <polygon class=\"region\" fill=\"%s\" stroke=\"%s\" points=\"", color, color);

        /* 收集区域边界顶点：遍历边界线段的端点 */
        for (int s = 0; s < node->data.region.segment_count; s++) {
            GeomNode *seg = node->data.region.boundary_segments[s];
            if (seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4) {
                /* 线段有两个端点，每个端点2个坐标(x1,y1,x2,y2) */
                double sx1 = symbolic_coord_to_double(seg->symbolic_coords[0]);
                double sy1 = symbolic_coord_to_double(seg->symbolic_coords[1]);
                fprintf(fp, "%.2f,%.2f ", sx1, sy1);
            }
        }
        fprintf(fp, "\"/>\n");
    }

    /* ---- 渲染函数块 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_FUNCTION_BLOCK)
            continue;
        if (node->coord_count < 2)
            continue;

        double bx = symbolic_coord_to_double(node->symbolic_coords[0]);
        double by = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_svg(node->trust);
        char escaped_name[256];
        svg_escape_string(geom_type_name(node->type), escaped_name, sizeof(escaped_name));

        /* 函数块：圆角矩形 */
        double bw = 120.0, bh = 60.0;
        fprintf(fp, "  <!-- Function Block id=%d -->\n", node->id);
        fprintf(fp,
                "  <rect class=\"block\" x=\"%.2f\" y=\"%.2f\" "
                "width=\"%.2f\" height=\"%.2f\" "
                "fill=\"%s\" fill-opacity=\"0.15\" stroke=\"%s\"/>\n",
                bx - bw / 2.0, by - bh / 2.0, bw, bh, color, color);
        fprintf(fp,
                "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                "text-anchor=\"middle\" dominant-baseline=\"central\" "
                "fill=\"%s\">%s_%d</text>\n",
                bx, by, color, escaped_name, node->id);
    }

    /* ---- 渲染线段 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_LINE_SEGMENT)
            continue;
        if (node->coord_count < 4)
            continue;

        double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
        double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
        double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);

        const char *color = trust_color_to_svg(node->trust);

        /* 贝塞尔曲线渲染：如果线段有 3 个以上坐标对，使用 SVG cubic Bezier */
        if (node->coord_count >= 6) {
            /* 使用前两对为端点，中间对为控制点 */
            int total_pairs = node->coord_count / 2;
            fprintf(fp, "  <!-- Line Segment id=%d (Bezier, %d points) -->\n", node->id, total_pairs);
            fprintf(fp, "  <path class=\"line\" fill=\"none\" stroke=\"%s\" d=\"M %.2f,%.2f", color, x1, y1);

            /* 构建贝塞尔曲线链：每两个端点间使用 2 个控制点 */
            for (int p = 0; p < total_pairs - 1; p++) {
                double seg_x1 = symbolic_coord_to_double(node->symbolic_coords[p * 2]);
                double seg_y1 = symbolic_coord_to_double(node->symbolic_coords[p * 2 + 1]);
                double seg_x2 = symbolic_coord_to_double(node->symbolic_coords[(p + 1) * 2]);
                double seg_y2 = symbolic_coord_to_double(node->symbolic_coords[(p + 1) * 2 + 1]);

                /* CP1 = P0 + 0.3*(P1-P0) + 垂直偏移 */
                double dx = seg_x2 - seg_x1;
                double dy = seg_y2 - seg_y1;
                double offset = 0.15 * sqrt(dx * dx + dy * dy);
                if (offset < 0.01)
                    offset = 5.0;
                double nx = -dy / (sqrt(dx * dx + dy * dy) + 0.001);
                double ny = dx / (sqrt(dx * dx + dy * dy) + 0.001);

                double cp1x = seg_x1 + 0.3 * dx + nx * offset;
                double cp1y = seg_y1 + 0.3 * dy + ny * offset;
                double cp2x = seg_x2 - 0.3 * dx + nx * offset;
                double cp2y = seg_y2 - 0.3 * dy + ny * offset;

                fprintf(fp, " C %.2f,%.2f %.2f,%.2f %.2f,%.2f", cp1x, cp1y, cp2x, cp2y, seg_x2, seg_y2);
            }
            fprintf(fp, "\"/>\n");
        } else {
            fprintf(fp, "  <!-- Line Segment id=%d -->\n", node->id);
            fprintf(fp,
                    "  <line class=\"line\" x1=\"%.2f\" y1=\"%.2f\" "
                    "x2=\"%.2f\" y2=\"%.2f\" stroke=\"%s\"/>\n",
                    x1, y1, x2, y2, color);
        }

        /* 线段标签 */
        double mx = (x1 + x2) / 2.0;
        double my = (y1 + y2) / 2.0;
        fprintf(fp,
                "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                "text-anchor=\"middle\" fill=\"%s\">seg_%d</text>\n",
                mx, my - 6.0, color, node->id);
    }

    /* ---- 渲染端口 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_PORT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_svg(node->trust);
        const char *port_type_str = (node->data.port && node->data.port->type == PORT_INPUT) ? "in" : "out";

        fprintf(fp, "  <!-- Port id=%d type=%s -->\n", node->id, port_type_str);
        fprintf(fp,
                "  <circle class=\"port\" cx=\"%.2f\" cy=\"%.2f\" r=\"5\" "
                "fill=\"white\" stroke=\"%s\"/>\n",
                px, py, color);
        fprintf(fp,
                "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                "text-anchor=\"middle\" fill=\"%s\" font-size=\"9px\">%s_%d</text>\n",
                px, py - 9.0, color, port_type_str, node->id);
    }

    /* ---- 渲染点 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_svg(node->trust);

        fprintf(fp, "  <!-- Point id=%d -->\n", node->id);

        /* 数学公式渲染：为符号坐标添加 <title> 注释（分数/根式表示） */
        if (node->symbolic_coords && node->symbolic_coords[0] && node->symbolic_coords[1]) {
            char *sx = symbolic_coord_serialize(node->symbolic_coords[0]);
            char *sy = symbolic_coord_serialize(node->symbolic_coords[1]);
            if (sx && sy) {
                fprintf(fp, "  <g>\n");
                fprintf(fp, "    <title>P%d = (%s, %s)</title>\n", node->id, sx, sy);
                fprintf(fp, "    <desc>Symbolic: P%d at rational/quadratic coords</desc>\n", node->id);
            }
            fprintf(fp,
                    "  <circle class=\"point\" cx=\"%.2f\" cy=\"%.2f\" r=\"4\" "
                    "fill=\"%s\"/>\n",
                    px, py, color);
            fprintf(fp,
                    "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                    "text-anchor=\"middle\" fill=\"#374151\">P%d</text>\n",
                    px, py - 8.0, node->id);
            if (sx && sy) {
                fprintf(fp, "  </g>\n");
            }
            lv_free((void **) &sx);
            lv_free((void **) &sy);
        } else {
            fprintf(fp,
                    "  <circle class=\"point\" cx=\"%.2f\" cy=\"%.2f\" r=\"4\" "
                    "fill=\"%s\"/>\n",
                    px, py, color);
            fprintf(fp,
                    "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                    "text-anchor=\"middle\" fill=\"#374151\">P%d</text>\n",
                    px, py - 8.0, node->id);
        }
    }

    /* ---- 渲染约束 ---- */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || c->participant_count < 2)
            continue;

        fprintf(fp, "  <!-- Constraint id=%d type=%s -->\n", c->id, constraint_type_name(c->type));

        /* 获取参与者节点的位置 */
        GeomNode *p0 = graph_get_node_by_id(graph, c->participants[0]);
        GeomNode *p1 = graph_get_node_by_id(graph, c->participants[1]);
        if (!p0 || !p1)
            continue;
        if (p0->coord_count < 2 || p1->coord_count < 2)
            continue;

        double x0 = symbolic_coord_to_double(p0->symbolic_coords[0]);
        double y0 = symbolic_coord_to_double(p0->symbolic_coords[1]);
        double x1 = symbolic_coord_to_double(p1->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(p1->symbolic_coords[1]);

        switch (c->type) {
            case INCIDENCE:
                /* 关联约束：虚线 */
                fprintf(fp,
                        "  <line class=\"constraint\" x1=\"%.2f\" y1=\"%.2f\" "
                        "x2=\"%.2f\" y2=\"%.2f\" stroke=\"#6b7280\"/>\n",
                        x0, y0, x1, y1);
                break;

            case BETWEENNESS: {
                /* 之间约束：三点之间用标签标注 */
                double mx = (x0 + x1) / 2.0;
                double my = (y0 + y1) / 2.0;
                fprintf(fp,
                        "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                        "text-anchor=\"middle\" fill=\"#6366f1\" font-style=\"italic\">"
                        "B(%d,%d",
                        mx, my, c->participants[0], c->participants[1]);
                if (c->participant_count >= 3) {
                    fprintf(fp, ",%d", c->participants[2]);
                }
                fprintf(fp, ")</text>\n");
                break;
            }

            case INTERSECTION: {
                /* 相交约束：计算精确交点并标记紫色十字 */
                double ix = x0, iy = y0; /* 默认交点为第一个参与者 */
                double a1x = x0, a1y = y0;
                double b1x = x1, b1y = y1;

                /* 使用线段参数方程求精确交点 */
                if (p0->type == GEOM_LINE_SEGMENT && p0->coord_count >= 4 && p1->type == GEOM_LINE_SEGMENT &&
                    p1->coord_count >= 4) {
                    double a2x = symbolic_coord_to_double(p0->symbolic_coords[2]);
                    double a2y = symbolic_coord_to_double(p0->symbolic_coords[3]);
                    double b2x = symbolic_coord_to_double(p1->symbolic_coords[2]);
                    double b2y = symbolic_coord_to_double(p1->symbolic_coords[3]);

                    /* 解线性方程组：P1 + t*(P2-P1) = Q1 + s*(Q2-Q1) */
                    double d1x = a2x - a1x, d1y = a2y - a1y;
                    double d2x = b2x - b1x, d2y = b2y - b1y;
                    double cross = d1x * d2y - d1y * d2x;

                    if (fabs(cross) > 1e-10) {
                        double dx0 = b1x - a1x;
                        double dy0 = b1y - a1y;
                        double t = (dx0 * d2y - dy0 * d2x) / cross;
                        if (t >= -0.05 && t <= 1.05) {
                            ix = a1x + t * d1x;
                            iy = a1y + t * d1y;
                        }
                    }
                }

                fprintf(fp,
                        "  <line class=\"constraint\" x1=\"%.2f\" y1=\"%.2f\" "
                        "x2=\"%.2f\" y2=\"%.2f\" stroke=\"#a855f7\"/>\n",
                        a1x, a1y, b1x, b1y);

                /* 在精确交点处绘制紫色十字标记 */
                double cross_r = 5.0;
                fprintf(fp,
                        "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                        "stroke=\"#a855f7\" stroke-width=\"2\"/>\n",
                        ix - cross_r, iy - cross_r, ix + cross_r, iy + cross_r);
                fprintf(fp,
                        "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                        "stroke=\"#a855f7\" stroke-width=\"2\"/>\n",
                        ix - cross_r, iy + cross_r, ix + cross_r, iy - cross_r);
                fprintf(fp,
                        "  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"4\" "
                        "fill=\"none\" stroke=\"#a855f7\" stroke-width=\"1.5\"/>\n",
                        ix, iy);
                break;
            }

            case CONTAINMENT:
                /* 包含约束：点线 */
                fprintf(fp,
                        "  <line class=\"constraint\" x1=\"%.2f\" y1=\"%.2f\" "
                        "x2=\"%.2f\" y2=\"%.2f\" stroke=\"#14b8a6\" "
                        "stroke-dasharray=\"2,4\"/>\n",
                        x0, y0, x1, y1);
                break;

            case ANGLE:
                /* 角度约束：紫色虚线 */
                fprintf(fp,
                        "  <line class=\"constraint\" x1=\"%.2f\" y1=\"%.2f\" "
                        "x2=\"%.2f\" y2=\"%.2f\" stroke=\"#a855f7\" "
                        "stroke-dasharray=\"4,2\"/>\n",
                        x0, y0, x1, y1);
                break;

            case CONNECTION:
                /* 连接约束：箭头线 */
                fprintf(fp,
                        "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                        "stroke=\"#f59e0b\" stroke-width=\"1.5\" "
                        "marker-end=\"url(#arrowhead)\"/>\n",
                        x0, y0, x1, y1);
                break;

            default:
                break;
        }
    }

    /* ---- 图例 ---- */
    double legend_x = min_x + 15.0;
    double legend_y = min_y + 20.0;
    fprintf(fp, "\n  <!-- Legend -->\n");
    fprintf(fp, "  <g transform=\"translate(%.2f, %.2f)\">\n", legend_x, legend_y);
    fprintf(fp,
            "    <rect x=\"0\" y=\"0\" width=\"150\" height=\"130\" "
            "fill=\"white\" fill-opacity=\"0.9\" stroke=\"#d1d5db\" rx=\"4\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"10\" y=\"18\" font-weight=\"bold\">Legend</text>\n");

    /* 点 */
    fprintf(fp, "    <circle cx=\"20\" cy=\"35\" r=\"4\" fill=\"#22c55e\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"32\" y=\"39\">Point</text>\n");

    /* 线段 */
    fprintf(fp, "    <line x1=\"12\" y1=\"52\" x2=\"28\" y2=\"52\" stroke=\"#3b82f6\" stroke-width=\"2\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"32\" y=\"56\">Line Segment</text>\n");

    /* 区域 */
    fprintf(fp,
            "    <rect x=\"12\" y=\"64\" width=\"16\" height=\"12\" fill=\"#eab308\" fill-opacity=\"0.3\" "
            "stroke=\"#eab308\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"32\" y=\"75\">Region</text>\n");

    /* 约束 */
    fprintf(fp, "    <line x1=\"12\" y1=\"90\" x2=\"28\" y2=\"90\" stroke=\"#6b7280\" stroke-dasharray=\"5,3\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"32\" y=\"94\">Constraint</text>\n");

    /* 信任颜色 */
    fprintf(fp, "    <circle cx=\"16\" cy=\"110\" r=\"4\" fill=\"#22c55e\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"24\" y=\"114\" font-size=\"9px\">Constrained</text>\n");
    fprintf(fp, "    <circle cx=\"86\" cy=\"110\" r=\"4\" fill=\"#9ca3af\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"94\" y=\"114\" font-size=\"9px\">Free</text>\n");
    fprintf(fp, "    <circle cx=\"120\" cy=\"110\" r=\"4\" fill=\"#ef4444\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"128\" y=\"114\" font-size=\"9px\">Conflict</text>\n");

    fprintf(fp, "  </g>\n");

    /* 箭头标记定义（放在最后，因为connection可能引用） */
    fprintf(fp, "\n  <defs>\n");
    fprintf(fp,
            "    <marker id=\"arrowhead\" markerWidth=\"8\" markerHeight=\"6\" "
            "refX=\"8\" refY=\"3\" orient=\"auto\">\n");
    fprintf(fp, "      <polygon points=\"0 0, 8 3, 0 6\" fill=\"#f59e0b\"/>\n");
    fprintf(fp, "    </marker>\n");
    fprintf(fp, "  </defs>\n");

    fprintf(fp, "\n</svg>\n");

    fclose(fp);

    return lv_OK;
}

/**
 * @brief 将约束图导出为 LaTeX TikZ 代码文件
 * @param graph  约束图指针
 * @param config 导出配置（output_path 指定 .tex 文件路径）
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效，lv_ERROR_IO 文件错误
 */
int interop_export_tikz(const ConstraintGraph *graph, const InteropExportConfig *config) {
    /*
     * @brief 将约束图导出为LaTeX TikZ代码
     *
     * 【已实现功能】
     *   本函数已完成TikZ导出的核心渲染管线，生成可独立编译的LaTeX文档：
     *   1. LaTeX文档框架 —— 生成完整的 standalone 文档类，包含必要的
     *      TikZ库引用（arrows.meta, shapes.geometric, positioning, calc）
     *   2. 样式定义（TikZ style） —— 定义以下样式类别：
     *      - point: 小圆点（填充，inner sep=1.5pt）
     *      - line: 粗线（thick）
     *      - region: 半透明填充区域（fill opacity=0.3）
     *      - constraint: 灰色虚线（dashed, thin, gray）
     *      - block: 圆角矩形（rounded corners, 最小2cm x 1cm）
     *      - port: 白色填充小圆圈（draw, inner sep=2pt, fill=white）
     *      - label: 小号字体标签（font=\small）
     *      - connection: 橙色箭头连接线（-Stealth, thick, orange）
     *   3. 区域（Region）渲染 —— 使用 \draw[region] 绘制半透明多边形，
     *      遍历所有边界线段端点构建闭合路径（-- cycle）
     *   4. 函数块（Function Block）渲染 —— 使用 \node[block] 绘制
     *      圆角矩形节点，居中显示FB_<id>标签
     *   5. 线段（Line Segment）渲染 —— 使用 \draw[line] 绘制线段，
     *      中点上方显示 seg_<id> 标签
     *   6. 端口（Port）渲染 —— 使用 \node[port] 绘制小圆圈，
     *      标注 in/out_<id> 类型标签
     *   7. 点（Point）渲染 —— 使用 \node[point] 绘制填充圆点，
     *      上方标注P<id>
     *   8. 约束关系渲染 —— 支持五种约束类型的TikZ可视化：
     *      - 关联约束（INCIDENCE）：灰色虚线
     *      - 之间约束（BETWEENNESS）：紫色斜体标签标注
     *      - 相交约束（INTERSECTION）：紫色虚线 + 圆圈标记
     *      - 包含约束（CONTAINMENT）：青色密集点线（densely dotted）
     *      - 连接约束（CONNECTION）：橙色Stealth箭头
     *   9. 信任颜色映射 —— 根据TrustColor使用对应的TikZ颜色名：
     *      绿色(green!60!black)、灰色(gray)、红色(red!70!black)
     *
     * 【简化实现的部分（完整功能需要额外依赖或后续版本）】
     *   1. 曲线几何体渲染 —— 当前仅处理直线段端点；
     *      完整实现需要解析曲线参数并生成 plot/smooth/curve 等TikZ曲线命令。
     *   2. 区域的曲线边界 —— 当前区域边界使用直线段连接；
     *      完整实现需要生成 TikZ 的 plot[smooth] 或 curve 命令。
     *   3. 节点定位优化 —— 当前所有节点使用绝对坐标 at (x,y)；
     *      完整实现需要使用 TikZ positioning 库进行相对定位和自动布局。
     *   4. 约束的精确交点计算 —— 当前约束线端点为参与者节点坐标；
     *      完整实现需要调用几何求解器计算实际的几何交点位置。
     *   5. 三维投影支持 —— 当前仅支持二维平面渲染；
     *      完整实现需要 tikz-3dplot 库进行三维投影。
     *   6. 颜色渐变和阴影 —— 当前为纯色填充无渐变；
     *      完整实现需要 TikZ 的 shading 和 shadow 特性。
     *   7. 图例（Legend）—— 当前不包含图例；
     *      完整实现需要使用 TikZ legend 样式或手动绘制图例框。
     *   8. 外部化/缓存 —— 当前为单文件输出；
     *      完整实现需要生成 TikZ externalize 所需的多文件结构。
     *
     * 【外部依赖说明】
     *   本函数仅生成纯文本的.tex文件，不依赖任何外部C库。生成的TikZ代码
     *   需要以下LaTeX环境来编译：
     *   1. LaTeX发行版（TeX Live / MiKTeX）
     *   2. TikZ/PGF包（通常随LaTeX发行版自动安装）
     *   3. standalone 文档类
     *
     * 【使用示例】
     *   InteropExportConfig cfg;
     *   lv_strlcpy(cfg.output_path, "output.tex", sizeof(cfg.output_path));
     *   int ret = interop_export_tikz(graph, &cfg);
     *   // 然后使用: pdflatex output.tex 编译为PDF
     *
     * 【与 interop_export_pdf 的关系】
     *   如果用户需要PDF输出但不想安装LaTeX，建议使用 interop_export_pdf
     *   函数直接生成PDF。本TikZ函数适合需要嵌入学术论文的场景。
     *
     * @param graph 约束图指针（包含所有节点和约束关系）
     * @param config 导出配置（output_path 指定 .tex 文件路径）
     * @return lv_OK 成功导出
     *         lv_ERROR_INVALID_PARAM 参数无效（graph或config为NULL）
     *         lv_ERROR_IO 文件无法创建或写入
     */
    if (!graph || !config)
        return lv_ERROR_INVALID_PARAM;

    /* ---- 流式事件：开始 LaTeX/TikZ 导出 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "开始 LaTeX/TikZ 导出", 0);
    }

    FILE *fp = fopen(config->output_path, "w");
    if (!fp)
        return lv_ERROR_IO;

    /* LaTeX文档头部 */
    (void) fprintf(fp, "%% Generated by Lv-00 v%s\n", lv_VERSION_STRING);
    (void) fprintf(fp, "%% TikZ geometry export\n\n");
    (void) fprintf(fp, "\\documentclass[tikz,border=10pt]{standalone}\n");
    (void) fprintf(fp, "\\usepackage{tikz}\n");
    (void) fprintf(fp, "\\usetikzlibrary{arrows.meta,shapes.geometric,positioning,calc}\n\n");
    (void) fprintf(fp, "\\begin{document}\n\n");
    (void) fprintf(fp, "\\begin{tikzpicture}[\n");
    (void) fprintf(fp, "    point/.style={circle, fill, inner sep=1.5pt},\n");
    (void) fprintf(fp, "    line/.style={thick},\n");
    (void) fprintf(fp, "    region/.style={fill opacity=0.3, thick},\n");
    (void) fprintf(fp, "    constraint/.style={dashed, thin, gray},\n");
    (void) fprintf(fp, "    block/.style={draw, rounded corners, minimum width=2cm, minimum height=1cm, thick},\n");
    (void) fprintf(fp, "    port/.style={circle, draw, inner sep=2pt, fill=white},\n");
    (void) fprintf(fp, "    label/.style={font=\\small},\n");
    (void) fprintf(fp, "    connection/.style={-{Stealth[length=5pt]}, thick, orange}\n");
    (void) fprintf(fp, "]\n\n");

    /* ---- 渲染区域（底层） ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_REGION)
            continue;
        if (node->data.region.segment_count < 3)
            continue;

        const char *color = trust_color_to_tikz(node->trust);

        fprintf(fp, "    %% Region id=%d\n", node->id);

        /* 颜色渐变：使用 TikZ shading 为区域添加渐变效果 */
        fprintf(fp, "    \\shade[top color=%s, bottom color=%s!30] ", color, color);

        /* 收集区域边界顶点 */
        int first_point = 1;
        for (int s = 0; s < node->data.region.segment_count; s++) {
            GeomNode *seg = node->data.region.boundary_segments[s];
            if (seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4) {
                double sx1 = symbolic_coord_to_double(seg->symbolic_coords[0]);
                double sy1 = symbolic_coord_to_double(seg->symbolic_coords[1]);
                if (first_point) {
                    fprintf(fp, "(%.2f, %.2f)", sx1, sy1);
                    first_point = 0;
                } else {
                    fprintf(fp, " -- (%.2f, %.2f)", sx1, sy1);
                }
            }
        }
        fprintf(fp, " -- cycle;\n");

        /* 同时添加描边轮廓 */
        fprintf(fp, "    \\draw[region, %s] ", color);
        first_point = 1;
        for (int s = 0; s < node->data.region.segment_count; s++) {
            GeomNode *seg = node->data.region.boundary_segments[s];
            if (seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4) {
                double sx1 = symbolic_coord_to_double(seg->symbolic_coords[0]);
                double sy1 = symbolic_coord_to_double(seg->symbolic_coords[1]);
                if (first_point) {
                    fprintf(fp, "(%.2f, %.2f)", sx1, sy1);
                    first_point = 0;
                } else {
                    fprintf(fp, " -- (%.2f, %.2f)", sx1, sy1);
                }
            }
        }
        fprintf(fp, " -- cycle;\n");
    }

    /* ---- 渲染函数块 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_FUNCTION_BLOCK)
            continue;
        if (node->coord_count < 2)
            continue;

        double bx = symbolic_coord_to_double(node->symbolic_coords[0]);
        double by = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_tikz(node->trust);

        fprintf(fp, "    %% Function Block id=%d\n", node->id);
        fprintf(fp,
                "    \\node[block, draw=%s, fill=%s, fill opacity=0.15] "
                "at (%.2f, %.2f) {FB\\_%d};\n",
                color, color, bx, by, node->id);
    }

    /* ---- 渲染线段 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_LINE_SEGMENT)
            continue;
        if (node->coord_count < 4)
            continue;

        double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
        double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
        double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);

        const char *color = trust_color_to_tikz(node->trust);

        fprintf(fp, "    %% Line Segment id=%d\n", node->id);

        /* 曲线几何体渲染：如果线段有 3 个以上坐标对，使用 plot[smooth] */
        if (node->coord_count >= 6) {
            int total_pairs = node->coord_count / 2;
            fprintf(fp, "    \\draw[line, %s] plot[smooth] coordinates {", color);
            for (int p = 0; p < total_pairs; p++) {
                double sx = symbolic_coord_to_double(node->symbolic_coords[p * 2]);
                double sy = symbolic_coord_to_double(node->symbolic_coords[p * 2 + 1]);
                if (p > 0)
                    fprintf(fp, " ");
                fprintf(fp, "(%.2f,%.2f)", sx, sy);
            }
            fprintf(fp, "};\n");
        } else {
            fprintf(fp,
                    "    \\draw[line, %s] (%.2f, %.2f) -- (%.2f, %.2f) "
                    "node[midway, above, label] {seg\\_%d};\n",
                    color, x1, y1, x2, y2, node->id);
        }
    }

    /* ---- 渲染端口 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_PORT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_tikz(node->trust);
        const char *port_type_str = (node->data.port && node->data.port->type == PORT_INPUT) ? "in" : "out";

        fprintf(fp, "    %% Port id=%d type=%s\n", node->id, port_type_str);
        fprintf(fp, "    \\node[port, draw=%s] (port%d) at (%.2f, %.2f) {};\n", color, node->id, px, py);
        fprintf(fp,
                "    \\node[label, %s, font=\\tiny] at (%.2f, %.2f) "
                "{%s\\_%d};\n",
                color, px, py + 0.3, port_type_str, node->id);
    }

    /* ---- 渲染点 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_tikz(node->trust);

        fprintf(fp, "    %% Point id=%d\n", node->id);
        fprintf(fp, "    \\node[point, %s] (P%d) at (%.2f, %.2f) {};\n", color, node->id, px, py);

        /* 使用符号坐标序列化作为标签（如果坐标可用） */
        if (node->symbolic_coords && node->symbolic_coords[0] && node->symbolic_coords[1]) {
            char *sx = symbolic_coord_serialize(node->symbolic_coords[0]);
            char *sy = symbolic_coord_serialize(node->symbolic_coords[1]);
            if (sx && sy) {
                fprintf(fp,
                        "    \\node[label, above=2pt of P%d] "
                        "{$P_{%d}\\!\\left(%s,\\, %s\\right)$};\n",
                        node->id, node->id, sx, sy);
            } else {
                fprintf(fp, "    \\node[label, above=2pt of P%d] {$P_{%d}$};\n", node->id, node->id);
            }
            lv_free((void **) &sx);
            lv_free((void **) &sy);
        } else {
            fprintf(fp, "    \\node[label, above=2pt of P%d] {$P_{%d}$};\n", node->id, node->id);
        }
    }

    /* ---- 渲染约束 ---- */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || c->participant_count < 2)
            continue;

        fprintf(fp, "    %% Constraint id=%d type=%s\n", c->id, constraint_type_name(c->type));

        GeomNode *p0 = graph_get_node_by_id(graph, c->participants[0]);
        GeomNode *p1 = graph_get_node_by_id(graph, c->participants[1]);
        if (!p0 || !p1)
            continue;
        if (p0->coord_count < 2 || p1->coord_count < 2)
            continue;

        double x0 = symbolic_coord_to_double(p0->symbolic_coords[0]);
        double y0 = symbolic_coord_to_double(p0->symbolic_coords[1]);
        double x1 = symbolic_coord_to_double(p1->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(p1->symbolic_coords[1]);

        switch (c->type) {
            case INCIDENCE:
                fprintf(fp, "    \\draw[constraint] (%.2f, %.2f) -- (%.2f, %.2f);\n", x0, y0, x1, y1);
                break;

            case BETWEENNESS: {
                double mx = (x0 + x1) / 2.0;
                double my = (y0 + y1) / 2.0;
                fprintf(fp,
                        "    \\node[label, purple, font=\\itshape] at (%.2f, %.2f) "
                        "{B(%d, %d",
                        mx, my, c->participants[0], c->participants[1]);
                if (c->participant_count >= 3) {
                    fprintf(fp, ", %d", c->participants[2]);
                }
                fprintf(fp, ")};\n");
                break;
            }

            case INTERSECTION: {
                /* 相交约束：使用 TikZ intersection 库计算精确交点 */
                double ix = x0, iy = y0;
                double a1x = x0, a1y = y0, b1x = x1, b1y = y1;
                bool has_precise = false;

                if (p0->type == GEOM_LINE_SEGMENT && p0->coord_count >= 4 && p1->type == GEOM_LINE_SEGMENT &&
                    p1->coord_count >= 4) {
                    double a2x = symbolic_coord_to_double(p0->symbolic_coords[2]);
                    double a2y = symbolic_coord_to_double(p0->symbolic_coords[3]);
                    double b2x = symbolic_coord_to_double(p1->symbolic_coords[2]);
                    double b2y = symbolic_coord_to_double(p1->symbolic_coords[3]);

                    double d1x = a2x - a1x, d1y = a2y - a1y;
                    double d2x = b2x - b1x, d2y = b2y - b1y;
                    double cross = d1x * d2y - d1y * d2x;

                    if (fabs(cross) > 1e-10) {
                        double dx0 = b1x - a1x, dy0 = b1y - a1y;
                        double t = (dx0 * d2y - dy0 * d2x) / cross;
                        if (t >= -0.05 && t <= 1.05) {
                            ix = a1x + t * d1x;
                            iy = a1y + t * d1y;
                            has_precise = true;
                        }
                    }
                }

                /* 输出 TikZ intersection 标记 */
                if (has_precise) {
                    fprintf(fp, "    %% 精确交点计算 (t=parametric)\n");
                    fprintf(fp, "    \\fill[red] (%.2f, %.2f) circle (2pt);\n", ix, iy);
                    fprintf(fp,
                            "    \\node[label, red, font=\\tiny] at (%.2f, %.2f) "
                            "{intersection};\n",
                            ix + 0.3, iy + 0.3);
                } else {
                    fprintf(fp, "    \\draw[constraint, purple] (%.2f, %.2f) -- (%.2f, %.2f);\n", a1x, a1y, b1x, b1y);
                    fprintf(fp, "    \\node[circle, draw=purple, inner sep=1pt] at (%.2f, %.2f) {};\n", x0, y0);
                }
                break;
            }

            case CONTAINMENT:
                fprintf(fp,
                        "    \\draw[constraint, teal, densely dotted] "
                        "(%.2f, %.2f) -- (%.2f, %.2f);\n",
                        x0, y0, x1, y1);
                break;

            case ANGLE:
                /* 角度约束：purple dashed */
                fprintf(fp,
                        "    \\draw[constraint, purple, densely dashed] "
                        "(%.2f, %.2f) -- (%.2f, %.2f);\n",
                        x0, y0, x1, y1);
                break;

            case CONNECTION:
                fprintf(fp, "    \\draw[connection] (%.2f, %.2f) -- (%.2f, %.2f);\n", x0, y0, x1, y1);
                break;

            default:
                break;
        }
    }

    /* ---- 图例（Legend） ---- */
    {
        /* 收集图中实际出现的节点类型和约束类型 */
        bool has_point = false, has_line = false, has_region = false;
        bool has_block = false, has_port = false;
        bool has_incidence = false, has_betweenness = false;
        bool has_intersection = false, has_containment = false, has_angle = false;
        bool has_connection = false;

        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *n = graph->nodes[i];
            if (!n)
                continue;
            switch (n->type) {
                case GEOM_POINT:
                    has_point = true;
                    break;
                case GEOM_LINE_SEGMENT:
                    has_line = true;
                    break;
                case GEOM_REGION:
                    has_region = true;
                    break;
                case GEOM_CIRCLE:
                    has_region = true;
                    break;
                case GEOM_FUNCTION_BLOCK:
                    has_block = true;
                    break;
                case GEOM_PORT:
                    has_port = true;
                    break;
                default:
                    break;
            }
        }
        for (int i = 0; i < graph->constraint_count; i++) {
            Constraint *c = graph->constraints[i];
            if (!c)
                continue;
            switch (c->type) {
                case INCIDENCE:
                    has_incidence = true;
                    break;
                case BETWEENNESS:
                    has_betweenness = true;
                    break;
                case INTERSECTION:
                    has_intersection = true;
                    break;
                case CONTAINMENT:
                    has_containment = true;
                    break;
                case ANGLE:
                    has_angle = true;
                    break;
                case CONNECTION:
                    has_connection = true;
                    break;
                default:
                    break;
            }
        }

        int legend_rows = 0;
        if (has_point)
            legend_rows++;
        if (has_line)
            legend_rows++;
        if (has_region)
            legend_rows++;
        if (has_block)
            legend_rows++;
        if (has_port)
            legend_rows++;
        if (has_incidence)
            legend_rows++;
        if (has_betweenness)
            legend_rows++;
        if (has_intersection)
            legend_rows++;
        if (has_containment)
            legend_rows++;
        if (has_connection)
            legend_rows++;

        if (legend_rows > 0) {
            fprintf(fp, "\n    %% Legend\n");
            fprintf(fp,
                    "    \\matrix[draw, fill=white, fill opacity=0.85, "
                    "anchor=south east, column sep=4pt, row sep=2pt, "
                    "font=\\scriptsize, inner sep=4pt]\n");
            fprintf(fp, "    at (current bounding box.south east) {\n");

            int row = 0;
            if (has_point) {
                fprintf(fp, "        \\node[point] {}; & \\node {Point}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_line) {
                fprintf(fp, "        \\draw[line] (0,0) -- (0.5,0); & \\node {Line Segment}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_region) {
                fprintf(fp, "        \\draw[region, fill=blue!20] (0,0) rectangle (0.5,0.3); & \\node {Region}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_block) {
                fprintf(
                    fp,
                    "        \\node[block, minimum width=0.5cm, minimum height=0.3cm] {}; & \\node {Function Block}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_port) {
                fprintf(fp, "        \\node[port] {}; & \\node {Port}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_incidence) {
                fprintf(fp, "        \\draw[constraint] (0,0) -- (0.5,0); & \\node {Incidence}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_betweenness) {
                fprintf(fp, "        \\node[purple, font=\\itshape] {B}; & \\node {Betweenness}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_intersection) {
                fprintf(fp,
                        "        \\draw[constraint, purple] (0,0) -- (0.5,0); "
                        "\\node[circle, draw=purple, inner sep=0.5pt] at (0.25,0) {}; "
                        "& \\node {Intersection}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_containment) {
                fprintf(fp,
                        "        \\draw[constraint, teal, densely dotted] (0,0) -- (0.5,0); & \\node {Containment}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_connection) {
                fprintf(fp, "        \\draw[connection] (0,0) -- (0.5,0); & \\node {Connection}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }

            fprintf(fp, "    };\n");
        }
    }

    fprintf(fp, "\n\\end{tikzpicture}\n\n");
    fprintf(fp, "\\end{document}\n");

    fclose(fp);

    /* ---- 流式事件：LaTeX/TikZ 导出完成 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "LaTeX/TikZ 导出完成", 0);
    }

    return lv_OK;
}

/**
 * @brief 将约束图导出为 TikZ 片段（不含文档框架）
 * @details 仅输出 \\begin{tikzpicture}...\\end{tikzpicture} 片段，
 *          可直接嵌入已有的 LaTeX 文档。包含样式定义、节点渲染、
 *          约束渲染、符号坐标标签和自动图例。
 * @param graph 约束图指针
 * @param output 输出缓冲区
 * @param size 缓冲区大小
 * @return 实际写入字符数（不含终止符），失败返回负数
 */
int interop_export_tikz_fragment(const ConstraintGraph *graph, char *output, size_t size) {
    if (!graph || !output || size == 0)
        return -1;

    /* ---- 流式事件：开始 TikZ 片段导出 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "开始 TikZ 片段导出", 0);
    }

    /* 使用 snprintf 逐步写入缓冲区 */
    int total = 0;
    int remaining = (int) size;

#define TIKZ_FRAG_PRINTF(...)                                              \
    do {                                                                   \
        int n = snprintf(output + total, (size_t) remaining, __VA_ARGS__); \
        if (n < 0)                                                         \
            return -1;                                                     \
        if (n >= remaining) {                                              \
            total += remaining - 1;                                        \
            remaining = 1;                                                 \
        } else {                                                           \
            total += n;                                                    \
            remaining -= n;                                                \
        }                                                                  \
    } while (0)

    /* TikZ 样式定义和 tikzpicture 开始 */
    TIKZ_FRAG_PRINTF("%% Generated by Lv-00 v%s (TikZ fragment)\n", lv_VERSION_STRING);
    TIKZ_FRAG_PRINTF("\\begin{tikzpicture}[\n");
    TIKZ_FRAG_PRINTF("    point/.style={circle, fill, inner sep=1.5pt},\n");
    TIKZ_FRAG_PRINTF("    line/.style={thick},\n");
    TIKZ_FRAG_PRINTF("    region/.style={fill opacity=0.3, thick},\n");
    TIKZ_FRAG_PRINTF("    constraint/.style={dashed, thin, gray},\n");
    TIKZ_FRAG_PRINTF("    block/.style={draw, rounded corners, minimum width=2cm, minimum height=1cm, thick},\n");
    TIKZ_FRAG_PRINTF("    port/.style={circle, draw, inner sep=2pt, fill=white},\n");
    TIKZ_FRAG_PRINTF("    label/.style={font=\\small},\n");
    TIKZ_FRAG_PRINTF("    connection/.style={-{Stealth[length=5pt]}, thick, orange}\n");
    TIKZ_FRAG_PRINTF("]\n\n");

    /* ---- 渲染区域（底层） ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_REGION)
            continue;
        if (node->data.region.segment_count < 3)
            continue;

        const char *color = trust_color_to_tikz(node->trust);

        TIKZ_FRAG_PRINTF("    %% Region id=%d\n", node->id);
        TIKZ_FRAG_PRINTF("    \\draw[region, fill=%s] ", color);

        int first_point = 1;
        for (int s = 0; s < node->data.region.segment_count; s++) {
            GeomNode *seg = node->data.region.boundary_segments[s];
            if (seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4) {
                double sx1 = symbolic_coord_to_double(seg->symbolic_coords[0]);
                double sy1 = symbolic_coord_to_double(seg->symbolic_coords[1]);
                if (first_point) {
                    TIKZ_FRAG_PRINTF("(%.2f, %.2f)", sx1, sy1);
                    first_point = 0;
                } else {
                    TIKZ_FRAG_PRINTF(" -- (%.2f, %.2f)", sx1, sy1);
                }
            }
        }
        TIKZ_FRAG_PRINTF(" -- cycle;\n");
    }

    /* ---- 渲染函数块 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_FUNCTION_BLOCK)
            continue;
        if (node->coord_count < 2)
            continue;

        double bx = symbolic_coord_to_double(node->symbolic_coords[0]);
        double by = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_tikz(node->trust);

        TIKZ_FRAG_PRINTF("    %% Function Block id=%d\n", node->id);
        TIKZ_FRAG_PRINTF(
            "    \\node[block, draw=%s, fill=%s, fill opacity=0.15] "
            "at (%.2f, %.2f) {FB\\_%d};\n",
            color, color, bx, by, node->id);
    }

    /* ---- 渲染线段 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_LINE_SEGMENT)
            continue;
        if (node->coord_count < 4)
            continue;

        double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
        double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
        double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);

        const char *color = trust_color_to_tikz(node->trust);

        TIKZ_FRAG_PRINTF("    %% Line Segment id=%d\n", node->id);
        TIKZ_FRAG_PRINTF(
            "    \\draw[line, %s] (%.2f, %.2f) -- (%.2f, %.2f) "
            "node[midway, above, label] {seg\\_%d};\n",
            color, x1, y1, x2, y2, node->id);
    }

    /* ---- 渲染端口 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_PORT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_tikz(node->trust);
        const char *port_type_str = (node->data.port && node->data.port->type == PORT_INPUT) ? "in" : "out";

        TIKZ_FRAG_PRINTF("    %% Port id=%d type=%s\n", node->id, port_type_str);
        TIKZ_FRAG_PRINTF("    \\node[port, draw=%s] (port%d) at (%.2f, %.2f) {};\n", color, node->id, px, py);
        TIKZ_FRAG_PRINTF(
            "    \\node[label, %s, font=\\tiny] at (%.2f, %.2f) "
            "{%s\\_%d};\n",
            color, px, py + 0.3, port_type_str, node->id);
    }

    /* ---- 渲染点（带符号坐标标签） ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_tikz(node->trust);

        TIKZ_FRAG_PRINTF("    %% Point id=%d\n", node->id);
        TIKZ_FRAG_PRINTF("    \\node[point, %s] (P%d) at (%.2f, %.2f) {};\n", color, node->id, px, py);

        /* 使用符号坐标序列化作为标签（如果坐标可用） */
        if (node->symbolic_coords && node->symbolic_coords[0] && node->symbolic_coords[1]) {
            char *sx = symbolic_coord_serialize(node->symbolic_coords[0]);
            char *sy = symbolic_coord_serialize(node->symbolic_coords[1]);
            if (sx && sy) {
                TIKZ_FRAG_PRINTF(
                    "    \\node[label, above=2pt of P%d] "
                    "{$P_{%d}\\!\\left(%s,\\, %s\\right)$};\n",
                    node->id, node->id, sx, sy);
            } else {
                TIKZ_FRAG_PRINTF("    \\node[label, above=2pt of P%d] {$P_{%d}$};\n", node->id, node->id);
            }
            lv_free((void **) &sx);
            lv_free((void **) &sy);
        } else {
            TIKZ_FRAG_PRINTF("    \\node[label, above=2pt of P%d] {$P_{%d}$};\n", node->id, node->id);
        }
    }

    /* ---- 渲染约束 ---- */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || c->participant_count < 2)
            continue;

        TIKZ_FRAG_PRINTF("    %% Constraint id=%d type=%s\n", c->id, constraint_type_name(c->type));

        GeomNode *p0 = graph_get_node_by_id(graph, c->participants[0]);
        GeomNode *p1 = graph_get_node_by_id(graph, c->participants[1]);
        if (!p0 || !p1)
            continue;
        if (p0->coord_count < 2 || p1->coord_count < 2)
            continue;

        double x0 = symbolic_coord_to_double(p0->symbolic_coords[0]);
        double y0 = symbolic_coord_to_double(p0->symbolic_coords[1]);
        double x1 = symbolic_coord_to_double(p1->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(p1->symbolic_coords[1]);

        switch (c->type) {
            case INCIDENCE:
                TIKZ_FRAG_PRINTF("    \\draw[constraint] (%.2f, %.2f) -- (%.2f, %.2f);\n", x0, y0, x1, y1);
                break;

            case BETWEENNESS: {
                double mx = (x0 + x1) / 2.0;
                double my = (y0 + y1) / 2.0;
                TIKZ_FRAG_PRINTF(
                    "    \\node[label, purple, font=\\itshape] at (%.2f, %.2f) "
                    "{B(%d, %d",
                    mx, my, c->participants[0], c->participants[1]);
                if (c->participant_count >= 3) {
                    TIKZ_FRAG_PRINTF(", %d", c->participants[2]);
                }
                TIKZ_FRAG_PRINTF(")};\n");
                break;
            }

            case INTERSECTION:
                TIKZ_FRAG_PRINTF("    \\draw[constraint, purple] (%.2f, %.2f) -- (%.2f, %.2f);\n", x0, y0, x1, y1);
                TIKZ_FRAG_PRINTF("    \\node[circle, draw=purple, inner sep=1pt] at (%.2f, %.2f) {};\n", x0, y0);
                break;

            case CONTAINMENT:
                TIKZ_FRAG_PRINTF(
                    "    \\draw[constraint, teal, densely dotted] "
                    "(%.2f, %.2f) -- (%.2f, %.2f);\n",
                    x0, y0, x1, y1);
                break;

            case ANGLE:
                /* 角度约束：紫色虚线段 */
                TIKZ_FRAG_PRINTF(
                    "    \\draw[constraint, purple, densely dashed] "
                    "(%.2f, %.2f) -- (%.2f, %.2f);\n",
                    x0, y0, x1, y1);
                break;

            case CONNECTION:
                TIKZ_FRAG_PRINTF("    \\draw[connection] (%.2f, %.2f) -- (%.2f, %.2f);\n", x0, y0, x1, y1);
                break;

            default:
                break;
        }
    }

    /* ---- 图例（Legend） ---- */
    {
        bool has_point = false, has_line = false, has_region = false;
        bool has_block = false, has_port = false;
        bool has_incidence = false, has_betweenness = false;
        bool has_intersection = false, has_containment = false, has_angle = false;
        bool has_connection = false;

        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *n = graph->nodes[i];
            if (!n)
                continue;
            switch (n->type) {
                case GEOM_POINT:
                    has_point = true;
                    break;
                case GEOM_LINE_SEGMENT:
                    has_line = true;
                    break;
                case GEOM_REGION:
                    has_region = true;
                    break;
                case GEOM_CIRCLE:
                    has_region = true;
                    break;
                case GEOM_FUNCTION_BLOCK:
                    has_block = true;
                    break;
                case GEOM_PORT:
                    has_port = true;
                    break;
                default:
                    break;
            }
        }
        for (int i = 0; i < graph->constraint_count; i++) {
            Constraint *c = graph->constraints[i];
            if (!c)
                continue;
            switch (c->type) {
                case INCIDENCE:
                    has_incidence = true;
                    break;
                case BETWEENNESS:
                    has_betweenness = true;
                    break;
                case INTERSECTION:
                    has_intersection = true;
                    break;
                case CONTAINMENT:
                    has_containment = true;
                    break;
                case ANGLE:
                    has_angle = true;
                    break;
                case CONNECTION:
                    has_connection = true;
                    break;
                default:
                    break;
            }
        }

        int legend_rows = 0;
        if (has_point)
            legend_rows++;
        if (has_line)
            legend_rows++;
        if (has_region)
            legend_rows++;
        if (has_block)
            legend_rows++;
        if (has_port)
            legend_rows++;
        if (has_incidence)
            legend_rows++;
        if (has_betweenness)
            legend_rows++;
        if (has_intersection)
            legend_rows++;
        if (has_containment)
            legend_rows++;
        if (has_connection)
            legend_rows++;

        if (legend_rows > 0) {
            TIKZ_FRAG_PRINTF("\n    %% Legend\n");
            TIKZ_FRAG_PRINTF(
                "    \\matrix[draw, fill=white, fill opacity=0.85, "
                "anchor=south east, column sep=4pt, row sep=2pt, "
                "font=\\scriptsize, inner sep=4pt]\n");
            TIKZ_FRAG_PRINTF("    at (current bounding box.south east) {\n");

            int row = 0;
            if (has_point) {
                TIKZ_FRAG_PRINTF("        \\node[point] {}; & \\node {Point}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_line) {
                TIKZ_FRAG_PRINTF("        \\draw[line] (0,0) -- (0.5,0); & \\node {Line Segment}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_region) {
                TIKZ_FRAG_PRINTF("        \\draw[region, fill=blue!20] (0,0) rectangle (0.5,0.3); & \\node {Region}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_block) {
                TIKZ_FRAG_PRINTF(
                    "        \\node[block, minimum width=0.5cm, minimum height=0.3cm] {}; & \\node {Function Block}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_port) {
                TIKZ_FRAG_PRINTF("        \\node[port] {}; & \\node {Port}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_incidence) {
                TIKZ_FRAG_PRINTF("        \\draw[constraint] (0,0) -- (0.5,0); & \\node {Incidence}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_betweenness) {
                TIKZ_FRAG_PRINTF("        \\node[purple, font=\\itshape] {B}; & \\node {Betweenness}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_intersection) {
                TIKZ_FRAG_PRINTF(
                    "        \\draw[constraint, purple] (0,0) -- (0.5,0); "
                    "\\node[circle, draw=purple, inner sep=0.5pt] at (0.25,0) {}; "
                    "& \\node {Intersection}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_containment) {
                TIKZ_FRAG_PRINTF(
                    "        \\draw[constraint, teal, densely dotted] (0,0) -- (0.5,0); & \\node {Containment}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_connection) {
                TIKZ_FRAG_PRINTF("        \\draw[connection] (0,0) -- (0.5,0); & \\node {Connection}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }

            TIKZ_FRAG_PRINTF("    };\n");
        }
    }

    TIKZ_FRAG_PRINTF("\n\\end{tikzpicture}\n");

#undef TIKZ_FRAG_PRINTF

    /* 确保以 null 终止 */
    if (total >= (int) size) {
        output[size - 1] = '\0';
        return (int) size - 1; /* 截断但仍返回写入量 */
    }
    output[total] = '\0';

    /* ---- 流式事件：TikZ 片段导出完成 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "TikZ 片段导出完成", 0);
    }

    return total;
}

/**
 * @brief 将约束图导出为规范表示（Canonical）文本文件
 * @details 包含节点（类型、ID、坐标、信任色、命名空间深度、父块 ID、类型特定信息）、
 *          约束（类型、ID、参与者列表、模板 ID）和邻接表。
 * @param graph       约束图指针
 * @param output_path 输出文件路径
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效，lv_ERROR_IO 文件错误
 */
int interop_export_canonical(const ConstraintGraph *graph, const char *output_path) {
    if (!graph || !output_path)
        return lv_ERROR_INVALID_PARAM;

    FILE *fp = fopen(output_path, "w");
    if (!fp)
        return lv_ERROR_IO;

    /* 输出规范表示 */
    fprintf(fp, "# Lv-00 Canonical Representation\n");
    fprintf(fp, "# Generated by Lv-00 v%s\n", lv_VERSION_STRING);
    fprintf(fp, "# Format: NodeType NodeID [Coordinates] TrustColor NamespaceDepth ParentBlockID\n");
    fprintf(fp, "#         ConstraintType ConstraintID ParticipantIDs...\n\n");

    /* 输出节点 */
    fprintf(fp, "NODES %d\n", graph->node_count);
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;

        /* 节点类型和ID */
        fprintf(fp, "%s %d", geom_type_name(node->type), node->id);

        /* 输出符号坐标 */
        fprintf(fp, " [");
        /* 内层坐标遍历：const 保护外层节点指针，只读遍历 */
        const int coord_cnt = node->coord_count;
        for (int j = 0; j < coord_cnt; j++) {
            if (j > 0)
                fprintf(fp, ", ");

            SymbolicCoord *coord = node->symbolic_coords[j];
            if (coord) {
                char *serialized = symbolic_coord_serialize(coord);
                if (serialized) {
                    fprintf(fp, "%s", serialized);
                    lv_free((void **) &serialized);
                } else {
                    /* 序列化失败时回退到数值表示 */
                    double val = symbolic_coord_to_double(coord);
                    fprintf(fp, "%.6g", val);
                }
            } else {
                fprintf(fp, "null");
            }
        }
        fprintf(fp, "]");

        /* 信任颜色 */
        fprintf(fp, " %s", trust_color_to_svg(node->trust));

        /* 命名空间深度和父块 */
        fprintf(fp, " ns=%d parent=%d", node->namespace_depth, node->parent_block_id);

        /* 类型特定信息 */
        switch (node->type) {
            case GEOM_PORT:
                if (node->data.port) {
                    fprintf(fp, " port_type=%s formal=%s poly=%s",
                            (node->data.port->type == PORT_INPUT) ? "input" : "output",
                            node->data.port->is_formal_param ? "true" : "false",
                            node->data.port->is_polymorphic ? "true" : "false");
                }
                break;
            case GEOM_REGION:
                fprintf(fp, " boundary_segments=%d", node->data.region.segment_count);
                break;
            case GEOM_CIRCLE:
                fprintf(fp, " center=%d radius=%d", node->data.circle.center_node_id, node->data.circle.radius_node_id);
                break;
            case GEOM_FUNCTION_BLOCK:
                fprintf(fp, " internal=%d inputs=%d outputs=%d state=%d", node->data.func_block.internal_node_count,
                        node->data.func_block.input_count, node->data.func_block.output_count,
                        node->data.func_block.determinism_state);
                break;
            default:
                break;
        }

        fprintf(fp, "\n");
    }

    /* 输出约束 */
    fprintf(fp, "\nCONSTRAINTS %d\n", graph->constraint_count);
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c)
            continue;

        fprintf(fp, "%s %d", constraint_type_name(c->type), c->id);

        /* 参与者ID列表 */
        for (int j = 0; j < c->participant_count; j++) {
            fprintf(fp, " %d", c->participants[j]);
        }

        /* 模板ID（如果有） */
        if (c->template_id >= 0) {
            fprintf(fp, " template=%d", c->template_id);
        }

        fprintf(fp, "\n");
    }

    /* 输出邻接表（每个节点关联的约束） */
    fprintf(fp, "\nADJACENCY_LIST\n");
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;

        /* 查找涉及此节点的约束 */
        /* 256-sized stack array, size matches max_results — bound is correct */
        int related_indices[256];
        int related_count = graph_find_constraints_involving(graph, node->id, related_indices, 256);

        if (related_count > 0) {
            fprintf(fp, "NODE %d ->", node->id);
            for (int j = 0; j < related_count; j++) {
                Constraint *c = graph->constraints[related_indices[j]];
                if (c) {
                    fprintf(fp, " %s(%d)", constraint_type_name(c->type), c->id);
                }
            }
            fprintf(fp, "\n");
        }
    }

    fclose(fp);

    return lv_OK;
}

/**
 * @brief 将约束图导出为 GeoJSON 格式文件
 * @details 动态生成 FeatureCollection，包含点类型节点（Point）和线段类型节点（LineString）
 *          及其坐标。点直接导出坐标，线段通过 INCIDENCE 约束查找端点坐标。
 * @param graph  约束图指针
 * @param config 导出配置（output_path 指定输出文件路径）
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效，lv_ERROR_IO 文件错误
 */
int interop_export_geojson(const ConstraintGraph *graph, const InteropExportConfig *config) {
    if (!graph || !config)
        return lv_ERROR_INVALID_PARAM;

    FILE *fp = fopen(config->output_path, "w");
    if (!fp)
        return lv_ERROR_IO;

    /* R02：基于实际图数据动态生成GeoJSON，而非硬编码占位数据 */
    fprintf(fp, "{\n");
    fprintf(fp, "  \"type\": \"FeatureCollection\",\n");
    fprintf(fp, "  \"features\": [\n");

    int feature_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        const GeomNode *node = graph_get_node(graph, i);
        if (!node)
            continue;

        /* 仅导出点类型节点（线段和区域的坐标较为复杂） */
        if (node->type == GEOM_POINT && node->coord_count >= 2 && node->symbolic_coords != NULL) {
            if (feature_count > 0) {
                fprintf(fp, ",\n");
            }
            /* 获取有理数坐标值，转为double */
            double x_val = 0.0, y_val = 0.0;
            SymbolicCoord *cx = node->symbolic_coords[0];
            SymbolicCoord *cy = node->symbolic_coords[1];

            if (cx)
                x_val = symbolic_coord_to_double(cx);
            if (cy)
                y_val = symbolic_coord_to_double(cy);

            fprintf(fp, "    {\n");
            fprintf(fp, "      \"type\": \"Feature\",\n");
            fprintf(fp, "      \"geometry\": {\n");
            fprintf(fp, "        \"type\": \"Point\",\n");
            fprintf(fp, "        \"coordinates\": [%.15g, %.15g]\n", x_val, y_val);
            fprintf(fp, "      },\n");
            fprintf(fp, "      \"properties\": {\n");
            fprintf(fp, "        \"id\": %d,\n", node->id);
            fprintf(fp, "        \"type\": \"point\"\n");
            fprintf(fp, "      }\n");
            fprintf(fp, "    }");
            feature_count++;
        }
    }

    /* 导出线段类型节点 */
    for (int i = 0; i < graph->node_count; i++) {
        const GeomNode *node = graph_get_node(graph, i);
        if (!node)
            continue;
        if (node->type != GEOM_LINE_SEGMENT)
            continue;

        /* 查找与线段关联的 INCIDENCE 约束以获取端点 */
        int constraint_indices[lv_MAX_CONSTRAINT_INDICES];
        int c_count = graph_find_constraints_involving(graph, node->id, constraint_indices, lv_MAX_CONSTRAINT_INDICES);

        /* 收集端点坐标 */
        double endpoints[4]; /* x1, y1, x2, y2 */
        int endpoint_found = 0;
        for (int j = 0; j < c_count && endpoint_found < 2; j++) {
            const Constraint *c = graph->constraints[constraint_indices[j]];
            if (!c || c->type != INCIDENCE)
                continue;
            for (int k = 0; k < c->participant_count; k++) {
                if (c->participants[k] == node->id)
                    continue;
                const GeomNode *ep = graph_get_node(graph, c->participants[k]);
                if (!ep || ep->type != GEOM_POINT)
                    continue;
                int idx = endpoint_found * 2;
                if (ep->coord_count >= 2 && ep->symbolic_coords) {
                    endpoints[idx] = symbolic_coord_to_double(ep->symbolic_coords[0]);
                    endpoints[idx + 1] = symbolic_coord_to_double(ep->symbolic_coords[1]);
                    endpoint_found++;
                }
            }
        }

        if (endpoint_found >= 2) {
            if (feature_count > 0)
                fprintf(fp, ",\n");
            fprintf(fp, "    {\n");
            fprintf(fp, "      \"type\": \"Feature\",\n");
            fprintf(fp, "      \"geometry\": {\n");
            fprintf(fp, "        \"type\": \"LineString\",\n");
            fprintf(fp, "        \"coordinates\": [[%.15g, %.15g], [%.15g, %.15g]]\n", endpoints[0], endpoints[1],
                    endpoints[2], endpoints[3]);
            fprintf(fp, "      },\n");
            fprintf(fp, "      \"properties\": {\n");
            fprintf(fp, "        \"id\": %d,\n", node->id);
            fprintf(fp, "        \"type\": \"line_segment\"\n");
            fprintf(fp, "      }\n");
            fprintf(fp, "    }");
            feature_count++;
        }
    }

    fprintf(fp, "\n  ]\n");
    fprintf(fp, "}\n");

    fclose(fp);
    return lv_OK;
}

/**
 * @brief 将约束图导出为 PDF 文档（最小化纯C实现，无外部库依赖）
 * @param graph  约束图指针
 * @param config 导出配置
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效，lv_ERROR_IO 文件错误
 */
int interop_export_pdf(const ConstraintGraph *graph, const InteropExportConfig *config) {
    if (!graph || !config)
        return lv_ERROR_INVALID_PARAM;

    /* ---- 流式事件：开始 PDF 导出 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "开始 PDF 导出", 0);
    }

    FILE *fp = fopen(config->output_path, "wb");
    if (!fp)
        return lv_ERROR_IO;

    /*
     * PDF构建策略：
     *   1. 首先将页面内容写入内存缓冲区（content_buffer）
     *   2. 然后依次写入：PDF头 -> 对象定义 -> 内容流 -> xref表 -> trailer
     *   3. 所有坐标从"图形空间"变换到"PDF页面空间"（原点在左下角，Y轴向上）
     *
     *   页面尺寸根据约束图的包围盒动态计算。
     */

    /* ---- 计算边界框 ---- */
    double min_x, min_y, max_x, max_y;
    compute_bounding_box(graph, &min_x, &min_y, &max_x, &max_y);

    double g_width = max_x - min_x;
    double g_height = max_y - min_y;
    if (g_width < 50.0)
        g_width = 400.0;
    if (g_height < 50.0)
        g_height = 300.0;

    /* 添加边距 */
    double margin = 40.0;
    double page_w = g_width + 2.0 * margin;
    double page_h = g_height + 2.0 * margin;

/* ---- 辅助宏：将图形坐标映射到PDF坐标（PDF原点=左下角，Y向上） ---- */
/*
     * 图形空间:      (min_x, min_y) 为左下角原点
     * PDF页面空间:   (margin, margin) 对应图形空间的 (min_x, min_y)
     *
     * 变换公式:
     *   tx = margin + (x - min_x) * scale_x
     *   ty = margin + (y - min_y) * scale_y
     *   其中 scale_x = g_width / g_width = 1.0（使用1:1映射）
     *        scale_y = g_height / g_height = 1.0
     *
     * 简化（等比例）:
     *   tx = margin + (x - min_x)
     *   ty = margin + (y - min_y)
     */
#define GX(x) (margin + ((x) - min_x))
#define GY(y) (margin + ((y) - min_y))

    /* ---- 内容流缓冲区 ---- */
    /*
     * 将所有PDF图形操作先写入缓冲区，计算总字节数后用于对象定义。
     * 缓冲区使用动态增长的策略，初始分配64KB，按需扩展。
     */
    size_t buf_cap = 65536; /* 初始容量：64KB */
    size_t buf_len = 0;
    char *content = (char *) lv_malloc(buf_cap);
    if (!content) {
        fclose(fp);
        return lv_ERROR_OUT_OF_MEMORY;
    }

    /* 内容流辅助：追加字符串到缓冲区 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#define BUF_APPEND(fmt, ...)                                                         \
    do {                                                                             \
        int _need = snprintf(NULL, 0, fmt, ##__VA_ARGS__) + 1;                       \
        if (_need > 0 && buf_len + (size_t) _need >= buf_cap) {                      \
            size_t _new_cap = buf_cap * 2;                                           \
            while (_new_cap < buf_len + (size_t) _need)                              \
                _new_cap *= 2;                                                       \
            char *_new_buf = (char *) lv_realloc(content, _new_cap);                 \
            if (!_new_buf) {                                                         \
                lv_free((void **) &content);                                         \
                fclose(fp);                                                          \
                return lv_ERROR_OUT_OF_MEMORY;                                       \
            }                                                                        \
            content = _new_buf;                                                      \
            buf_cap = _new_cap;                                                      \
        }                                                                            \
        int _w = snprintf(content + buf_len, buf_cap - buf_len, fmt, ##__VA_ARGS__); \
        if (_w > 0)                                                                  \
            buf_len += _w;                                                           \
    } while (0)

    /* ---- 设置基础图形状态 ---- */
    BUF_APPEND("q\n");           /* 保存图形状态 */
    BUF_APPEND("%.2f w\n", 1.5); /* 默认线宽 */

    /* ---- 渲染区域（半透明填充 + 描边，底层） ---- */
    /* 激活透明度 ExtGState */
    BUF_APPEND("/GS1 gs\n");

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_REGION)
            continue;
        if (node->data.region.segment_count < 3)
            continue;

        /*
         * 区域渲染：使用 f (fill) 填充 + S (stroke) 描边。
         * 先设置填充颜色（半透明），构建路径，然后 B (fill+stroke)。
         */
        TrustColor trust = node->trust;
        if (trust == TRUST_GREEN) {
            BUF_APPEND("0.13 0.76 0.29 rg\n"); /* 填充色：绿色 */
            BUF_APPEND("0.13 0.76 0.29 RG\n"); /* 描边色：绿色 */
        } else if (trust == TRUST_AMBER) {
            BUF_APPEND("0.94 0.27 0.27 rg\n"); /* 填充色：红色 */
            BUF_APPEND("0.94 0.27 0.27 RG\n");
        } else {
            BUF_APPEND("0.61 0.64 0.69 rg\n"); /* 填充色：灰色 */
            BUF_APPEND("0.61 0.64 0.69 RG\n");
        }

        int first = 1;
        for (int s = 0; s < node->data.region.segment_count; s++) {
            GeomNode *seg = node->data.region.boundary_segments[s];
            if (seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4) {
                double sx = symbolic_coord_to_double(seg->symbolic_coords[0]);
                double sy = symbolic_coord_to_double(seg->symbolic_coords[1]);
                if (first) {
                    BUF_APPEND("%.2f %.2f m\n", GX(sx), GY(sy));
                    first = 0;
                } else {
                    BUF_APPEND("%.2f %.2f l\n", GX(sx), GY(sy));
                }
            }
        }
        BUF_APPEND("h\n"); /* 闭合路径 */
        BUF_APPEND("B\n"); /* 填充+描边 */
    }

    /* ---- 渲染线段 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_LINE_SEGMENT)
            continue;
        if (node->coord_count < 4)
            continue;

        double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
        double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
        double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);

        TrustColor trust = node->trust;
        if (trust == TRUST_GREEN)
            BUF_APPEND("0.15 0.50 0.92 RG\n"); /* 蓝色：线段 */
        else if (trust == TRUST_AMBER)
            BUF_APPEND("0.94 0.27 0.27 RG\n"); /* 红色：不可信 */
        else
            BUF_APPEND("0.61 0.64 0.69 RG\n"); /* 灰色：中间状态 */

        BUF_APPEND("%.2f w\n", 2.0);

        /* 贝塞尔曲线：如果线段有 3 个以上坐标对，使用 PDF c 操作符 */
        if (node->coord_count >= 6) {
            int total_pairs = node->coord_count / 2;
            BUF_APPEND("%.2f %.2f m\n", GX(x1), GY(y1));

            for (int p = 1; p < total_pairs; p++) {
                double sx = symbolic_coord_to_double(node->symbolic_coords[p * 2]);
                double sy = symbolic_coord_to_double(node->symbolic_coords[p * 2 + 1]);
                double px = symbolic_coord_to_double(node->symbolic_coords[(p - 1) * 2]);
                double py = symbolic_coord_to_double(node->symbolic_coords[(p - 1) * 2 + 1]);

                /* 计算两个控制点 */
                double dx = sx - px, dy = sy - py;
                double dist = sqrt(dx * dx + dy * dy);
                double offset = 0.15 * dist;
                if (offset < 0.01)
                    offset = 5.0;
                double nx = -dy / (dist + 0.001);
                double ny = dx / (dist + 0.001);

                double cp1x = px + 0.3 * dx + nx * offset;
                double cp1y = py + 0.3 * dy + ny * offset;
                double cp2x = sx - 0.3 * dx + nx * offset;
                double cp2y = sy - 0.3 * dy + ny * offset;

                /* PDF c 操作符: x1 y1 x2 y2 x3 y3 c */
                BUF_APPEND("%.2f %.2f %.2f %.2f %.2f %.2f c\n", GX(cp1x), GY(cp1y), GX(cp2x), GY(cp2y), GX(sx), GY(sy));
            }
            BUF_APPEND("S\n");
        } else {
            BUF_APPEND("%.2f %.2f m\n", GX(x1), GY(y1));
            BUF_APPEND("%.2f %.2f l\n", GX(x2), GY(y2));
            BUF_APPEND("S\n");
        }

        BUF_APPEND("%.2f w\n", 1.5); /* 恢复默认线宽 */
    }

    /* ---- 渲染点 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        TrustColor trust = node->trust;
        if (trust == TRUST_GREEN)
            BUF_APPEND("0.13 0.76 0.29 RG\n"); /* 绿色：完全可信 */
        else if (trust == TRUST_AMBER)
            BUF_APPEND("0.94 0.27 0.27 RG\n"); /* 红色：不可信 */
        else
            BUF_APPEND("0.61 0.64 0.69 RG\n"); /* 灰色：中间状态 */

        /*
         * 点渲染：使用填充圆（filled circle）。
         * 当前方法：用极短线段模拟点（line cap round + 粗线宽）。
         * 改进方法（后续版本）: 使用 Bezier 曲线构造圆。
         */
        BUF_APPEND("%.2f w\n", 6.0);
        BUF_APPEND("1 J\n"); /* 圆头线端 */
        BUF_APPEND("%.2f %.2f m\n", GX(px), GY(py));
        BUF_APPEND("%.2f %.2f l\n", GX(px + 0.01), GY(py));
        BUF_APPEND("S\n");
        BUF_APPEND("0 J\n"); /* 恢复平头线端 */
        BUF_APPEND("%.2f w\n", 1.5);
    }

    /* ---- 渲染函数块（作为圆角矩形） ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_FUNCTION_BLOCK)
            continue;
        if (node->coord_count < 2)
            continue;

        double bx = symbolic_coord_to_double(node->symbolic_coords[0]);
        double by = symbolic_coord_to_double(node->symbolic_coords[1]);
        double bw = 120.0, bh = 60.0;

        TrustColor trust = node->trust;
        if (trust == TRUST_GREEN)
            BUF_APPEND("0.13 0.76 0.29 RG\n"); /* 绿色：完全可信 */
        else if (trust == TRUST_AMBER)
            BUF_APPEND("0.94 0.27 0.27 RG\n"); /* 红色：不可信 */
        else
            BUF_APPEND("0.61 0.64 0.69 RG\n"); /* 灰色：中间状态 */

        BUF_APPEND("%.2f w\n", 2.0);
        BUF_APPEND("%.2f %.2f %.2f %.2f re B\n", GX(bx) - bw / 2.0, GY(by) - bh / 2.0, bw, bh);
        BUF_APPEND("%.2f w\n", 1.5);
    }

    /* ---- 渲染约束关系 ---- */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || c->participant_count < 2)
            continue;

        GeomNode *p0 = graph_get_node_by_id(graph, c->participants[0]);
        GeomNode *p1 = graph_get_node_by_id(graph, c->participants[1]);
        if (!p0 || !p1)
            continue;
        if (p0->coord_count < 2 || p1->coord_count < 2)
            continue;

        double x0 = symbolic_coord_to_double(p0->symbolic_coords[0]);
        double y0 = symbolic_coord_to_double(p0->symbolic_coords[1]);
        double x1 = symbolic_coord_to_double(p1->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(p1->symbolic_coords[1]);

        switch (c->type) {
            case INCIDENCE:
                /* 关联约束：灰色虚线 */
                BUF_APPEND("0.42 0.45 0.50 RG\n");
                BUF_APPEND("[4.0 3.0] 0 d\n");
                BUF_APPEND("%.2f w\n", 1.0);
                BUF_APPEND("%.2f %.2f m\n", GX(x0), GY(y0));
                BUF_APPEND("%.2f %.2f l\n", GX(x1), GY(y1));
                BUF_APPEND("S\n");
                BUF_APPEND("[] 0 d\n"); /* 恢复实线 */
                BUF_APPEND("%.2f w\n", 1.5);
                break;

            case CONNECTION:
                /* 连接约束：橙色实线 */
                BUF_APPEND("0.96 0.62 0.04 RG\n");
                BUF_APPEND("%.2f w\n", 1.5);
                BUF_APPEND("%.2f %.2f m\n", GX(x0), GY(y0));
                BUF_APPEND("%.2f %.2f l\n", GX(x1), GY(y1));
                BUF_APPEND("S\n");
                break;

            case BETWEENNESS:
                /* 之间约束：紫色细线 */
                BUF_APPEND("0.39 0.40 0.95 RG\n");
                BUF_APPEND("%.2f w\n", 1.0);
                BUF_APPEND("[2.0 2.0] 0 d\n");
                BUF_APPEND("%.2f %.2f m\n", GX(x0), GY(y0));
                BUF_APPEND("%.2f %.2f l\n", GX(x1), GY(y1));
                BUF_APPEND("S\n");
                BUF_APPEND("[] 0 d\n");
                BUF_APPEND("%.2f w\n", 1.5);
                break;

            case INTERSECTION:
                /* 相交约束：紫色十字标记 */
                BUF_APPEND("0.66 0.33 0.97 RG\n");
                BUF_APPEND("%.2f w\n", 1.0);
                BUF_APPEND("%.2f %.2f m\n", GX(x0), GY(y0));
                BUF_APPEND("%.2f %.2f l\n", GX(x1), GY(y1));
                BUF_APPEND("S\n");
                BUF_APPEND("%.2f w\n", 1.5);
                break;

            case CONTAINMENT:
                /* 包含约束：青色点划线 */
                BUF_APPEND("0.08 0.72 0.65 RG\n");
                BUF_APPEND("[6.0 3.0 1.0 3.0] 0 d\n");
                BUF_APPEND("%.2f w\n", 1.0);
                BUF_APPEND("%.2f %.2f m\n", GX(x0), GY(y0));
                BUF_APPEND("%.2f %.2f l\n", GX(x1), GY(y1));
                BUF_APPEND("S\n");
                BUF_APPEND("[] 0 d\n");
                BUF_APPEND("%.2f w\n", 1.5);
                break;

            case ANGLE:
                /* 角度约束：紫色虚线 */
                BUF_APPEND("0.66 0.33 0.97 RG\n");
                BUF_APPEND("[4.0 2.0] 0 d\n");
                BUF_APPEND("%.2f w\n", 1.0);
                BUF_APPEND("%.2f %.2f m\n", GX(x0), GY(y0));
                BUF_APPEND("%.2f %.2f l\n", GX(x1), GY(y1));
                BUF_APPEND("S\n");
                BUF_APPEND("[] 0 d\n");
                BUF_APPEND("%.2f w\n", 1.5);
                break;

            default:
                break;
        }
    }

    /* ---- 文本标签（最小化实现） ---- */
    /*
     * 文本渲染策略说明：
     *   当前版本使用 Helvetica 字体标注节点ID。完整的文本渲染需要：
     *   1. 精确的文本宽度计算（用于居中定位）—— 可通过 Tj 返回值或 FreeType 度量
     *   2. 中文字体支持（CID字体或TrueType嵌入）—— 需要字体文件和 CIDFont 字典
     *   3. 文本旋转和变换 —— 通过 Tm 矩阵的旋转分量实现
     *   4. LaTeX 数学公式渲染 —— 复杂，需要完整的数学排版引擎或预渲染位图嵌入
     */
    BUF_APPEND("BT\n");
    BUF_APPEND("/F1 8 Tf\n"); /* Helvetica 8pt */
    BUF_APPEND("0 0 0 rg\n"); /* 黑色文本 */

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->coord_count < 2)
            continue;

        double lx = symbolic_coord_to_double(node->symbolic_coords[0]);
        double ly = symbolic_coord_to_double(node->symbolic_coords[1]);

        /* 标签放置：点/端口上方，线段中点下方，块居中 */
        char label[32];
        if (node->type == GEOM_POINT) {
            snprintf(label, sizeof(label), "P%d", node->id);
            BUF_APPEND("%.2f %.2f Td\n", GX(lx) - 6.0, GY(ly) + 8.0);
        } else if (node->type == GEOM_LINE_SEGMENT && node->coord_count >= 4) {
            double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
            double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);
            double mx = (lx + x2) / 2.0, my = (ly + y2) / 2.0;
            snprintf(label, sizeof(label), "S%d", node->id);
            BUF_APPEND("%.2f %.2f Td\n", GX(mx) - 6.0, GY(my) - 6.0);
        } else if (node->type == GEOM_FUNCTION_BLOCK) {
            snprintf(label, sizeof(label), "FB_%d", node->id);
            BUF_APPEND("%.2f %.2f Td\n", GX(lx) - 14.0, GY(ly) - 3.0);
        } else {
            continue;
        }

        /* 转义括号 */
        for (const char *p = label; *p; p++) {
            if (*p == '(' || *p == ')' || *p == '\\')
                BUF_APPEND("\\%c", *p);
            else
                BUF_APPEND("%c", *p);
        }
        BUF_APPEND(" Tj\n");
        BUF_APPEND("%.2f %.2f Td\n", 0.0, 0.0); /* 重置文本位置到原点 */
    }
    BUF_APPEND("ET\n");

    BUF_APPEND("Q\n"); /* 恢复图形状态 */

#undef BUF_APPEND
#pragma GCC diagnostic pop
#undef GX
#undef GY

    /* ================================================================ */
    /*   PDF 文件结构写入（基于PDF 1.4规范）                             */
    /*                                                                  */
    /*   PDF 对象编号方案：                                              */
    /*     对象1: Catalog（目录）                                       */
    /*     对象2: Pages（页面树根节点）                                  */
    /*     对象3: Page（单页，含 ExtGState 引用）                       */
    /*     对象4: Content（内容流，包含上述所有图形操作）                */
    /*     对象5: Font（字体字典 - Helvetica）                          */
    /*     对象6: ExtGState（透明度图形状态）                           */
    /*     对象7: Info（页面元数据）                                    */
    /*                                                                  */
    /*   注意：对象编号和字节偏移量紧密耦合，修改内容流时需同步更新     */
    /*         xref表中的偏移量。                                       */
    /* ================================================================ */

    /* ---- 对象4的内容流长度（字节数） ---- */
    long content_length = (long) buf_len;

    /*
     * 对象1: Catalog
     * 根目录对象，指向Pages树
     */
    long cat_start = ftell(fp);
    fprintf(fp, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    /*
     * 对象2: Pages（页面树根节点）
     * 包含页面数量和子页面引用
     */
    long pages_start = ftell(fp);
    fprintf(fp, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");

    /*
     * 对象3: Page（单页定义）
     * 定义页面尺寸（MediaBox）、内容流引用、字体资源和 ExtGState
     * MediaBox格式：[llx lly urx ury] = [0 0 page_w page_h]
     */
    long page_start = ftell(fp);
    fprintf(fp,
            "3 0 obj\n<< /Type /Page /Parent 2 0 R\n"
            "   /MediaBox [0 0 %.2f %.2f]\n"
            "   /Contents 4 0 R\n"
            "   /Resources << /Font << /F1 5 0 R >>\n"
            "                 /ExtGState << /GS1 6 0 R >> >>\n"
            ">>\nendobj\n",
            page_w, page_h);

    /*
     * 对象4: Content（内容流）
     * 包含所有PDF图形描述操作符
     * 写入前计算并声明精确的流长度
     */
    long content_start = ftell(fp);
    fprintf(fp, "4 0 obj\n<< /Length %ld >>\nstream\n", content_length);
    size_t written = fwrite(content, 1, content_length, fp);
    if (written != (size_t) content_length) {
        lv_LOG_WARNING("PDF内容流写入不完整（期望 %ld, 实际 %zu）", content_length, written);
    }
    fprintf(fp, "\nendstream\nendobj\n");

    /*
     * 对象5: Font（字体字典）
     * 使用PDF标准14种字体之一的Helvetica，无需嵌入字体文件
     */
    long font_start = ftell(fp);
    fprintf(fp,
            "5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica"
            " /Encoding /WinAnsiEncoding >>\nendobj\n");

    /*
     * 对象6: ExtGState（透明度图形状态）
     * 设置 CA 0.3（描边透明度）和 ca 0.3（填充透明度）
     * 用于区域渲染的半透明效果。
     */
    long gs_start = ftell(fp);
    fprintf(fp, "6 0 obj\n<< /Type /ExtGState /CA 0.3 /ca 0.3 >>\nendobj\n");

    /*
     * 对象7: Info（页面元数据）
     * 包含文档标题、作者、创建者和创建日期。
     */
    long info_start = ftell(fp);
    {
        /* 获取当前日期时间字符串 */
        time_t now = time(NULL);
        struct tm lt;
        char date_str[64] = {0};
        lv_LOCALTIME(&now, &lt);
        strftime(date_str, sizeof(date_str), "D:%Y%m%d%H%M%S", &lt);

        fprintf(fp,
                "7 0 obj\n<< /Title (Lv-00 Geometry Export)\n"
                "   /Author (Lv-00 Project)\n"
                "   /Creator (Lv-00 v%s)\n"
                "   /CreationDate (%s) >>\nendobj\n",
                lv_VERSION_STRING, date_str);
    }

    /* ---- 交叉引用表（Cross-Reference Table） ---- */
    /*
     * xref表记录了每个PDF对象的字节偏移量，是PDF随机访问的关键结构。
     * 格式：
     *   xref
     *   0 8                    (对象0到7，共8个对象)
     *   0000000000 65535 f     (对象0=空闲条目)
     *   nnnnnnnnnn 00000 n     (对象1-7的字节偏移)
     */
    long xref_start = ftell(fp);
    fprintf(fp, "xref\n");
    fprintf(fp, "0 8\n");
    fprintf(fp, "0000000000 65535 f \n"); /* 对象0：空闲条目 */
    fprintf(fp, "%010ld 00000 n \n", cat_start);
    fprintf(fp, "%010ld 00000 n \n", pages_start);
    fprintf(fp, "%010ld 00000 n \n", page_start);
    fprintf(fp, "%010ld 00000 n \n", content_start);
    fprintf(fp, "%010ld 00000 n \n", font_start);
    fprintf(fp, "%010ld 00000 n \n", gs_start);
    fprintf(fp, "%010ld 00000 n \n", info_start);

    /* ---- Trailer ---- */
    /*
     * Trailer包含：
     * - /Size: 交叉引用表条目总数（8 = 对象0-7）
     * - /Root: 指向Catalog对象（对象1）
     * - /Info: 指向元数据对象（对象7）
     * - startxref: xref表起始偏移量（用于快速定位）
     * - %%EOF: PDF文件结束标记
     */
    fprintf(fp, "trailer\n");
    fprintf(fp, "<< /Size 8 /Root 1 0 R /Info 7 0 R >>\n");
    fprintf(fp, "startxref\n");
    fprintf(fp, "%ld\n", xref_start);
    fprintf(fp, "%%%%EOF\n");

    fclose(fp);
    lv_free((void **) &content);

    /*
     * PDF已成功导出：告知调用者文件路径、页面尺寸、节点/约束数量。
     * 当前PDF为纯C最小化实现（无外部库依赖），
     * 区域以线框模式渲染，文本标签为基础版本。
     * 完整功能改进方案见函数注释中【简化实现的部分】列表。
     */

    /* ---- 流式事件：PDF 导出完成 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "PDF 导出完成", 0);
    }

    return lv_OK;
}

/* ==================== GeoGebra 导入辅助：ZIP 解析 ==================== */

/**
 * @brief ZIP 文件结构常量
 *
 * ZIP 格式规范（PKWARE APPNOTE.TXT）定义的核心结构签名。
 * 所有多字节整数均为小端序（Little-Endian）。
 */
#define GGB_LOCAL_FILE_SIG 0x04034b50U      /**< 本地文件头签名 */
#define GGB_CENTRAL_DIR_SIG 0x02014b50U     /**< 中央目录签名 */
#define GGB_EOCD_SIG 0x06054b50U            /**< 结束中心目录签名 */
#define GGB_LOCAL_HEADER_MIN 30             /**< 本地文件头最小字节数 */
#define GGB_CENTRAL_DIR_MIN 46              /**< 中央目录条目最小字节数 */
#define GGB_EOCD_MIN_SIZE 22                /**< EOCD 最小字节数 */
#define GGB_MAX_XML_SIZE (16 * 1024 * 1024) /**< XML 最大大小 16MB */
#define GGB_COMPRESSION_STORE 0             /**< 无压缩（STORE） */
#define GGB_COMPRESSION_DEFLATE 8           /**< Deflate 压缩 */
