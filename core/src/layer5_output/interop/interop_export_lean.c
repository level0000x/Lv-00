/**
 * @file interop_export_lean.c
 * @brief 导出 —— Lean 定理证明导出
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
#include "lv/lv_json.h"

#include "lv/debug.h"
#include "interop_export_internal.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_file.h"

/* ---- 证明步骤类型 → Lean 输出处理器查找表 ---- */
typedef void (*LeanStepHandler)(FILE *fp, const ProofStep *step,
                                 bool is_green, bool is_blue,
                                 bool is_orange, bool is_amber);

static void lean_handler_add_node(FILE *fp, const ProofStep *step,
                                   bool is_green, bool is_blue,
                                   bool is_orange, bool is_amber) {
    if (is_green) {
        fprintf(fp, "    have h_node_%d : True := by intro node_%d ; constructor\n", step->node_id, step->node_id);
    } else if (is_blue) {
        fprintf(fp, "    -- [BLUE] 构造节点 node_%d, 信任色: %s (未探索/资源受限)\n", step->node_id,
                proof_color_to_string(step->color));
        fprintf(fp, "    have h_node_%d : True := by admit\n", step->node_id);
    } else if (is_orange) {
        fprintf(fp, "    -- [ORANGE] 构造节点 node_%d, 信任色: %s (非构造性oracle依赖)\n",
                step->node_id, proof_color_to_string(step->color));
        fprintf(fp, "    have h_node_%d : True := by exact oracle_result.node_%d\n", step->node_id, step->node_id);
    } else if (is_amber) {
        fprintf(fp, "    -- [AMBER] 构造节点 node_%d, 信任色: %s (数值假设)\n", step->node_id,
                proof_color_to_string(step->color));
        fprintf(fp, "    have h_node_%d : True := by sorry -- [NUMERIC] 数值假设步骤\n", step->node_id);
    } else {
        fprintf(fp, "    -- 构造节点 node_%d, 信任色: %s\n", step->node_id, proof_color_to_string(step->color));
        fprintf(fp, "    have h_node_%d : True := by trivial\n", step->node_id);
    }
}

static void lean_handler_add_constraint(FILE *fp, const ProofStep *step,
                                         bool is_green, bool is_blue,
                                         bool is_orange, bool is_amber) {
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
        fprintf(fp, "    have h_cstr_%d : True := by sorry -- [NUMERIC] 数值假设步骤\n", step->constraint_id);
    } else {
        fprintf(fp, "    -- 添加约束 cstr_%d, 信任色: %s\n", step->constraint_id,
                proof_color_to_string(step->color));
        fprintf(fp, "    have h_cstr_%d : True := by trivial\n", step->constraint_id);
    }
}

static void lean_handler_rewrite(FILE *fp, const ProofStep *step,
                                  bool is_green, bool is_blue,
                                  bool is_orange, bool is_amber) {
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
        fprintf(fp, "    -- 重写步骤 step_%d, 信任色: %s\n", step->id, proof_color_to_string(step->color));
        fprintf(fp, "    by assumption\n");
    }
}

static void lean_handler_func_app(FILE *fp, const ProofStep *step,
                                   bool is_green, bool is_blue,
                                   bool is_orange, bool is_amber) {
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
        fprintf(fp, "    -- 函数应用 step_%d, 信任色: %s\n", step->id, proof_color_to_string(step->color));
        fprintf(fp, "    by trivial\n");
    }
}

static void lean_handler_pack_func(FILE *fp, const ProofStep *step,
                                    bool is_green, bool is_blue,
                                    bool is_orange, bool is_amber) {
    (void)is_green; (void)is_blue; (void)is_orange; (void)is_amber;
    fprintf(fp, "    -- 函数块打包: step_%d, func_block_%d\n", step->id, step->func_block_id);
}

static void lean_handler_normalization(FILE *fp, const ProofStep *step,
                                        bool is_green, bool is_blue,
                                        bool is_orange, bool is_amber) {
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
        fprintf(fp, "    -- 归一化 step_%d, 信任色: %s\n", step->id, proof_color_to_string(step->color));
        fprintf(fp, "    by assumption\n");
    }
}

static void lean_handler_unify(FILE *fp, const ProofStep *step,
                                bool is_green, bool is_blue,
                                bool is_orange, bool is_amber) {
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
        fprintf(fp, "    -- 合一检查 step_%d, 信任色: %s\n", step->id, proof_color_to_string(step->color));
        fprintf(fp, "    by trivial\n");
    }
}

static void lean_handler_ex_falso(FILE *fp, const ProofStep *step,
                                   bool is_green, bool is_blue,
                                   bool is_orange, bool is_amber) {
    (void)is_blue; (void)is_orange; (void)is_amber;
    if (is_green) {
        fprintf(fp, "    contradiction ; assumption\n");
    } else {
        fprintf(fp, "    -- [非绿色] 爆炸原理 step_%d, 信任色: %s\n", step->id,
                proof_color_to_string(step->color));
        fprintf(fp, "    exfalso ; by sorry -- 非构造性爆炸原理，需外部验证\n");
    }
}

static void lean_handler_oracle(FILE *fp, const ProofStep *step,
                                 bool is_green, bool is_blue,
                                 bool is_orange, bool is_amber) {
    (void)is_green; (void)is_blue; (void)is_orange; (void)is_amber;
    fprintf(fp, "    -- [ORACLE] Oracle依赖: step_%d, 信任色: %s\n", step->id,
            proof_color_to_string(step->color));
    fprintf(fp, "    by exact (oracle.verify step_%d) -- 非构造性依赖，需外部oracle验证\n", step->id);
}

static void lean_handler_default(FILE *fp, const ProofStep *step,
                                  bool is_green, bool is_blue,
                                  bool is_orange, bool is_amber) {
    (void)is_green; (void)is_blue; (void)is_orange; (void)is_amber;
    fprintf(fp, "    -- 未知步骤类型: %d, 信任色: %s\n", (int) step->type,
            proof_color_to_string(step->color));
    fprintf(fp, "    by trivial\n");
}

static const LeanStepHandler lean_step_handlers[] = {
    [PROOF_STEP_ADD_NODE]       = lean_handler_add_node,
    [PROOF_STEP_ADD_CONSTRAINT] = lean_handler_add_constraint,
    [PROOF_STEP_REWRITE]        = lean_handler_rewrite,
    [PROOF_STEP_FUNCTION_APP]   = lean_handler_func_app,
    [PROOF_STEP_PACK_FUNCTION]  = lean_handler_pack_func,
    [PROOF_STEP_NORMALIZATION]  = lean_handler_normalization,
    [PROOF_STEP_UNIFY]          = lean_handler_unify,
    [PROOF_STEP_EX_FALSO]       = lean_handler_ex_falso,
    [PROOF_STEP_ORACLE]         = lean_handler_oracle,
};

int interop_export_lean(const ProofNavigator *proof, const InteropExportConfig *config) {
    if (!proof || !config)
        return lv_ERROR_INVALID_PARAM;

    /* ---- 流式事件：开始 Lean 4 导出 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "开始 Lean 4 导出", 0);
    }

    /* 生成Lean代码 */
    FILE *fp = lv_file_open(config->output_path, "w");
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

            /* 查表选择类型专用 handler：未命中（越界/NULL 槽）回退默认 handler */
            {
                LeanStepHandler handler =
                    ((unsigned) step->type < lv_ARRAY_SIZE(lean_step_handlers) && lean_step_handlers[step->type])
                        ? lean_step_handlers[step->type]
                        : lean_handler_default;
                handler(fp, step, is_green, is_blue, is_orange, is_amber);
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
