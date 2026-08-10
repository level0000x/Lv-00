/**
 * @file interop_export_coq.c
 * @brief 导出 —— Coq 定理证明导出
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

#include "debug.h"
#include "interop_export_internal.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"
#include "../../layer4_reasoning/proof/trust_color_x.h"


/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 步骤类型 → 英文标识符（用于生成 Coq 注释）
 * @param type 证明步骤类型
 * @return 类型标识符字符串
 */
/* 步骤类型 -> 英文标识符 映射表（本文件内局部 X 主源，按枚举值升序） */
#define LV_PROOF_STEP_TYPE_X(X) \
    X(PROOF_STEP_ADD_NODE,       "ADD_NODE") \
    X(PROOF_STEP_ADD_CONSTRAINT, "ADD_CONSTRAINT") \
    X(PROOF_STEP_REWRITE,        "REWRITE") \
    X(PROOF_STEP_FUNCTION_APP,   "FUNCTION_APP") \
    X(PROOF_STEP_PACK_FUNCTION,  "PACK_FUNCTION") \
    X(PROOF_STEP_NORMALIZATION,  "NORMALIZATION") \
    X(PROOF_STEP_UNIFY,          "UNIFY") \
    X(PROOF_STEP_EX_FALSO,       "EX_FALSO") \
    X(PROOF_STEP_ORACLE,         "ORACLE")

#define LV_PROOF_STEP_TO_COQNAME(sym, coq) [sym] = coq,
static const char *const s_proof_step_coq_names[] = {
    LV_PROOF_STEP_TYPE_X(LV_PROOF_STEP_TO_COQNAME)
};
#undef LV_PROOF_STEP_TO_COQNAME
#undef LV_PROOF_STEP_TYPE_X

static const char *lv_step_type_name(ProofStepType type) {
    if ((unsigned) type < lv_ARRAY_SIZE(s_proof_step_coq_names))
        return s_proof_step_coq_names[type];
    return "UNKNOWN";
}

/**
 * @brief 信任颜色 → 英文名（用于 admit 注释）
 * @param color 信任颜色
 * @return 颜色名
 */
/* 颜色 -> Coq 名称 映射表（按枚举值升序，自 LV_PROOF_COLOR_X 生成） */
#define LV_PROOF_COLOR_TO_COQNAME(sym, disp, hex, coq) [sym] = coq,
static const char *const s_proof_color_coq_names[] = {
    LV_PROOF_COLOR_X(LV_PROOF_COLOR_TO_COQNAME)
};
#undef LV_PROOF_COLOR_TO_COQNAME

static const char *lv_color_name(ProofColor color) {
    if ((unsigned) color < lv_ARRAY_SIZE(s_proof_color_coq_names))
        return s_proof_color_coq_names[color];
    return "UNKNOWN";
}

/**
 * @brief 判断信任颜色是否属于全构造（可信）族
 * @param color 信任颜色
 * @return true 表示可生成完整 tactic；false 表示需 admit
 */
static bool lv_color_is_trusted(ProofColor color) {
    return color == PROOF_COLOR_GREEN || color == PROOF_COLOR_GREEN_VERIFIED ||
           color == PROOF_COLOR_GREEN_COMPLETE;
}

/**
 * @brief 获取证明步骤的陈述文本（HOL Light 扩展结论优先，其次用户注释）
 * @param step 证明步骤
 * @return 陈述文本，无则返回 NULL
 */
static const char *lv_step_statement(const ProofStep *step) {
    if (step->ext && step->ext->conclusion)
        return step->ext->conclusion;
    return step->note;
}

/**
 * @brief 获取导出目标的命题文本（目标命题 label/name，回退 True）
 * @param proof 证明导航器
 * @return 命题文本
 */
static const char *lv_nav_goal(const ProofNavigator *proof) {
    if (proof->target_prop) {
        if (proof->target_prop->label && proof->target_prop->label[0])
            return proof->target_prop->label;
        if (proof->target_prop->name && proof->target_prop->name[0])
            return proof->target_prop->name;
    }
    return "True";
}

/**
 * @brief 将文本中的换行/回车/Tab 替换为空格（保持 Coq 脚本单行合法）
 * @param d  输出缓冲
 * @param s  源文本（可为 NULL，空操作）
 */
static void lv_coq_sanitize_line(lvStrBuf *d, const char *s) {
    if (!s)
        return;
    for (const char *p = s; *p; p++) {
        char c = *p;
        if (c == '\n' || c == '\r' || c == '\t')
            c = ' ';
        lv_strbuf_append_n(d, c, 1);
    }
}

/**
 * @brief 将单个 ProofStep 翻译为 Coq tactic 序列并写入缓冲
 *
 * 按 9 种 ProofStepType → Coq tactic 映射表生成：
 *   - ADD_NODE        -> pose proof (构造点/线段/区域)
 *   - ADD_CONSTRAINT  -> assert (约束声明)
 *   - REWRITE         -> rewrite <依赖|规则>.
 *   - FUNCTION_APP    -> apply <函数块|依赖|定理名>.
 *   - PACK_FUNCTION   -> (* 函数块打包 *)
 *   - NORMALIZATION   -> rewrite <归一化规则>.
 *   - UNIFY           -> reflexivity. 或 congruence.
 *   - EX_FALSO        -> exfalso.
 *   - ORACLE          -> admit. (非构造性依赖)
 *
 * 信任颜色处理：非 GREEN 系颜色步骤输出 admit + 颜色注释；
 * 其余步骤生成完整 tactic，tactic 参数随步骤的 id/node/rule 等数据变化。
 *
 * @param d     输出缓冲
 * @param step  证明步骤
 * @param trace 是否输出详细步骤注释
 */
static void lv_coq_emit_step(lvStrBuf *d, const ProofStep *step, bool trace) {
    const char *stmt = lv_step_statement(step);

    if (trace) {
        lv_strbuf_printf(d, "    (* Step %d [%s] node=%d constraint=%d rule=%d func=%d", step->id,
                         lv_step_type_name(step->type), step->node_id, step->constraint_id, step->rule_id,
                         step->func_block_id);
        if (stmt) {
            lv_strbuf_printf(d, " | ");
            lv_coq_sanitize_line(d, stmt);
        }
        lv_strbuf_printf(d, " *)\n");
    }

    /* 非全构造颜色 -> admit（见信任颜色处理注释） */
    if (!lv_color_is_trusted(step->color)) {
        lv_strbuf_printf(d, "    admit. (* 颜色 %s: 非全构造，未生成构造性证明 *)\n", lv_color_name(step->color));
        return;
    }

    switch (step->type) {
        case PROOF_STEP_ADD_NODE:
            /* 构造点/线段/区域：引用 Require 引入的现有构造定义 */
            if (step->node_id >= 0)
                lv_strbuf_printf(d, "    pose proof (lv_point_%d).\n", step->node_id);
            else
                lv_strbuf_printf(d, "    pose proof (lv_construction_%d).\n", step->id);
            break;

        case PROOF_STEP_ADD_CONSTRAINT:
            /* 约束声明 -> assert，用户需在花括号内补充证明 */
            if (stmt && stmt[0]) {
                lv_strbuf_printf(d, "    assert (lv_constraint_%d : ",
                                 step->constraint_id >= 0 ? step->constraint_id : step->id);
                lv_coq_sanitize_line(d, stmt);
                lv_strbuf_printf(d, ").\n");
                lv_strbuf_printf(d, "    { admit. }\n");
            } else {
                lv_strbuf_printf(d, "    admit. (* 约束声明缺失: constraint=%d *)\n",
                                 step->constraint_id >= 0 ? step->constraint_id : step->id);
            }
            break;

        case PROOF_STEP_REWRITE:
            /* 重写步骤 -> rewrite <依赖前驱步骤|规则> */
            if (step->dependency_count > 0)
                lv_strbuf_printf(d, "    rewrite lv_step_%d.\n", step->dependency_step_ids[0]);
            else if (step->rule_id >= 0)
                lv_strbuf_printf(d, "    rewrite lv_rule_%d.\n", step->rule_id);
            else
                lv_strbuf_printf(d, "    rewrite lv_rewrite_%d.\n", step->id);
            break;

        case PROOF_STEP_FUNCTION_APP:
            /* 函数应用 -> apply <函数块|依赖步骤|定理名> */
            if (step->func_block_id >= 0)
                lv_strbuf_printf(d, "    apply lv_func_%d.\n", step->func_block_id);
            else if (step->dependency_count > 0)
                lv_strbuf_printf(d, "    apply lv_step_%d.\n", step->dependency_step_ids[0]);
            else if (stmt && stmt[0]) {
                lv_strbuf_printf(d, "    apply ");
                lv_coq_sanitize_line(d, stmt);
                lv_strbuf_printf(d, ".\n");
            } else {
                lv_strbuf_printf(d, "    apply lv_lemma_%d.\n", step->rule_id);
            }
            break;

        case PROOF_STEP_PACK_FUNCTION:
            /* 函数块打包：仅生成注释骨架 */
            lv_strbuf_printf(d, "    (* 函数块打包: func_block=%d *)\n", step->func_block_id);
            break;

        case PROOF_STEP_NORMALIZATION:
            /* 自动规范化 -> rewrite 归一化规则 */
            lv_strbuf_printf(d, "    rewrite lv_norm_%d.\n", step->rule_id >= 0 ? step->rule_id : step->id);
            break;

        case PROOF_STEP_UNIFY:
            /* 合一检查 -> 有依赖时 congruence，否则 reflexivity */
            if (step->dependency_count > 0 || step->node_id >= 0)
                lv_strbuf_printf(d, "    congruence.\n");
            else
                lv_strbuf_printf(d, "    reflexivity.\n");
            break;

        case PROOF_STEP_EX_FALSO:
            /* 爆炸原理 -> exfalso */
            lv_strbuf_printf(d, "    exfalso.\n");
            if (stmt && stmt[0]) {
                lv_strbuf_printf(d, "    (* 爆炸原理目标: ");
                lv_coq_sanitize_line(d, stmt);
                lv_strbuf_printf(d, " *)\n");
            }
            break;

        case PROOF_STEP_ORACLE:
            /* 非构造性 oracle 依赖 -> admit */
            lv_strbuf_printf(d, "    admit. (* Oracle 非构造性依赖: step=%d *)\n", step->id);
            break;

        default:
            lv_strbuf_printf(d, "    admit. (* 未知步骤类型 %d *)\n", (int) step->type);
            break;
    }
}

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

    lvStrBuf d;
    lv_strbuf_init(&d);

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
     * 按映射表真实翻译：
     * 定理目标取自目标命题 label/name（非恒真），
     * 每个步骤依据其 id/type/color/依赖等数据生成对应 tactic。
     */
    const char *goal = lv_nav_goal(proof);
    lv_strbuf_printf(&d, "  Theorem lv_Main : ");
    lv_coq_sanitize_line(&d, goal);
    lv_strbuf_printf(&d, ".\n");

    if (config->include_proofs && proof->step_count > 0) {
        lv_strbuf_printf(&d, "  Proof.\n");
        bool has_admit = false;
        for (int i = 0; i < proof->step_count; i++) {
            const ProofStep *step = proof->steps[i];
            if (!step)
                continue;
            /* 非全构造颜色或 Oracle 步骤将产生 admit，需以 Admitted 收尾 */
            if (step->type == PROOF_STEP_ORACLE || !lv_color_is_trusted(step->color))
                has_admit = true;
            lv_coq_emit_step(&d, step, config->include_metadata);
            if (config->pretty_print)
                lv_strbuf_printf(&d, "\n");
        }
        if (has_admit)
            lv_strbuf_printf(&d, "  Admitted.\n\n");
        else
            lv_strbuf_printf(&d, "  Qed.\n\n");
    } else {
        /* 不输出证明体（include_proofs=false 或步骤为空） */
        lv_strbuf_printf(&d, "  Admitted. (* 证明体未导出: include_proofs=false 或步骤为空 *)\n\n");
    }

    lv_strbuf_printf(&d, "End lv_Export.\n");

    fwrite(d.data, 1, d.len, fp);
    fclose(fp);
    lv_strbuf_destroy(&d);

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
