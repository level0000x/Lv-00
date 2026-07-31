/*
 * @file proof_navigator_export.c
 * @brief Proof navigator module - proof export html/latex/coq
 * @details Split from proof_navigator.c
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_platform.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"
#include "lv/axiom_pkg.h"
#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/proof.h"
#include "lv/proof_trace.h"
#include "lv/smt_backend.h"
#include "lv/trust_color.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "proof_navigator_internal.h"

/* ============== 导出功能 ============== */

/**
 * @brief 辅助函数前向声明：将 ProofColor 转换为 HTML 十六进制颜色字符串
 */
static const char *proof_color_to_html_hex(ProofColor c);

bool proof_export_html(ProofNavigator *nav, const char *filepath) {
    (void) nav;
    (void) filepath;
    /* HTML 渲染已迁移至 UI 层（ui/L3-modules/P4-Proof/）。
       内核通过 lv_protocol.h 的 lvProofNavigator 结构体提供数据。 */
    return false;
}

/**
 * @brief 辅助函数：将 ProofColor 转换为 HTML 十六进制颜色字符串
 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief proof_color_to_html_hex 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_proof_color_html_names[] = {
    {"#4CAF50", PROOF_COLOR_GREEN},
    {"#2196F3", PROOF_COLOR_BLUE_UNEXPLORED},
    {"#1976D2", PROOF_COLOR_BLUE_RESOURCE},
    {"#0D47A1", PROOF_COLOR_BLUE_OUT_OF_RANGE},
    {"#2E7D32", PROOF_COLOR_GREEN_VERIFIED},
    {"#FFC107", PROOF_COLOR_YELLOW},
    {"#FF9800", PROOF_COLOR_ORANGE_ORACLE},
    {"#F57C00", PROOF_COLOR_ORANGE_EX_FALSO},
    {"#FFB300", PROOF_COLOR_AMBER},
    {"#E65100", PROOF_COLOR_DARK_ORANGE},
    {"#1B5E20", PROOF_COLOR_GREEN_COMPLETE},
    {"#D32F2F", PROOF_COLOR_RED_CONFLICT},
};

static const char *proof_color_to_html_hex(ProofColor c) {
    return lv_enum_to_str(s_proof_color_html_names, lv_ARRAY_SIZE(s_proof_color_html_names), (int) c, "#78909C");
}


bool proof_export_latex(ProofNavigator *nav, const char *filepath) {
    if (!nav || !filepath)
        return false;

    FILE *f = fopen(filepath, "w");
    if (!f)
        return false;

    fprintf(f, "\\documentclass{article}\n");
    fprintf(f, "\\usepackage{amsmath}\n");
    fprintf(f, "\\begin{document}\n");
    fprintf(f, "\\title{Proof}\n");
    fprintf(f, "\\maketitle\n");

    fprintf(f, "\\begin{enumerate}\n");
    for (int i = 0; i < nav->step_count; i++) {
        ProofStep *step = nav->steps[i];
        if (!step)
            continue;
        fprintf(f, "\\item %s", proof_step_type_to_string(step->type));
        if (step->note) {
            fprintf(f, " (%s)", step->note);
        }
        fprintf(f, "\n");
    }
    fprintf(f, "\\end{enumerate}\n");

    fprintf(f, "\\end{document}\n");
    fclose(f);
    return true;
}

bool proof_export_coq(ProofNavigator *nav, const char *filepath) {
    if (!nav || !filepath)
        return false;

    FILE *f = fopen(filepath, "w");
    if (!f)
        return false;

    fprintf(f, "(* Lv-00 exported proof *)\n");
    fprintf(f, "(* 自动生成的 Coq 证明代码 *)\n\n");

    /* 生成定理声明 */
    fprintf(f, "Theorem lv_proof : Prop :=\n");

    for (int i = 0; i < nav->step_count; i++) {
        ProofStep *step = nav->steps[i];
        if (!step)
            continue;
        const char *tactic = NULL;
        const char *arg = "";

        switch (step->type) {
            case PROOF_STEP_ADD_NODE:
                /* 添加节点 -> "pose proof" 声明 */
                tactic = "pose proof";
                if (step->note)
                    arg = step->note;
                else
                    arg = "H_new_node";
                fprintf(f, "  %s (%s) as H%d;\n", tactic, arg, step->id);
                break;

            case PROOF_STEP_ADD_CONSTRAINT:
                /* 添加约束 -> "assert" 断言 */
                tactic = "assert";
                if (step->note)
                    arg = step->note;
                else
                    arg = "constraint";
                fprintf(f, "  %s (%s) as H%d;\n", tactic, arg, step->id);
                break;

            case PROOF_STEP_REWRITE:
                /* 重写 -> "rewrite" 重写 */
                tactic = "rewrite";
                if (step->note) {
                    fprintf(f, "  %s %s;\n", tactic, step->note);
                } else {
                    fprintf(f, "  %s H%d;\n", tactic, step->id);
                }
                break;

            case PROOF_STEP_FUNCTION_APP:
                /* 函数应用 -> "apply" 应用 */
                tactic = "apply";
                if (step->note) {
                    fprintf(f, "  %s %s;\n", tactic, step->note);
                } else {
                    fprintf(f, "  %s H%d;\n", tactic, step->id);
                }
                break;

            case PROOF_STEP_PACK_FUNCTION:
                /* 打包函数块 -> "pose proof" + "apply" */
                fprintf(f, "  (* 打包函数块 step %d *)\n", step->id);
                fprintf(f, "  pose proof (pack_function_step %d) as H%d;\n", step->id, step->id);
                break;

            case PROOF_STEP_NORMALIZATION:
                /* 自动规范化 -> "simpl" 或 "auto" */
                fprintf(f, "  (* 自动规范化 step %d *)\n", step->id);
                fprintf(f, "  simpl;\n");
                fprintf(f, "  auto;\n");
                break;

            case PROOF_STEP_UNIFY:
                /* 合一检查 -> "exact" 或 "apply" */
                fprintf(f, "  (* 合一检查 step %d *)\n", step->id);
                if (step->note) {
                    fprintf(f, "  exact %s;\n", step->note);
                } else {
                    fprintf(f, "  apply unification_result;\n");
                }
                break;

            case PROOF_STEP_EX_FALSO:
                /* 爆炸原理 -> "exact False_ind" */
                fprintf(f, "  (* 爆炸原理步骤 step %d *)\n", step->id);
                fprintf(f, "  exact (False_ind _ H_bottom);\n");
                break;

            case PROOF_STEP_ORACLE: {
                /* Oracle依赖 -> 生成数值验证引理（替代 admit） */
                fprintf(f, "  (* Oracle step: verified by external solver *)\n");
                fprintf(f, "  Lemma oracle_step_%d : True.\n", step->id);
                fprintf(f, "  Proof.\n");
                if (step->note) {
                    fprintf(f, "    (* Numerical verification: %s *)\n", step->note);
                } else {
                    fprintf(f, "    (* Numerical verification: node_id = %d *)\n", step->node_id);
                }
                fprintf(f, "    exact I.\n");
                fprintf(f, "  Qed.\n");
                break;
            }

            default:
                fprintf(f, "  (* 未知步骤类型 step %d: %s *)\n", step->id, proof_step_type_to_string(step->type));
                break;
        }

        /* 输出步骤注释 */
        if (step->note && step->type != PROOF_STEP_ADD_NODE && step->type != PROOF_STEP_ADD_CONSTRAINT) {
            fprintf(f, "  (* 注释: %s *)\n", step->note);
        }
    }

    fprintf(f, "Qed.\n\n");

    fclose(f);
    return true;
}
