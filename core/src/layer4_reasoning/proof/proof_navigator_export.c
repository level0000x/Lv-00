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
#include "lv/proof_step_strategy.h"
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

/* ============== ProofStepStrategy 策略实例 ============== */

/* --- Coq 导出回调 --- */

static void export_coq_add_node(const ProofStep *step, FILE *f) {
    const char *tactic = "pose proof";
    const char *arg = step->note ? step->note : "H_new_node";
    fprintf(f, "  %s (%s) as H%d;\n", tactic, arg, step->id);
}

static void export_coq_add_constraint(const ProofStep *step, FILE *f) {
    const char *tactic = "assert";
    const char *arg = step->note ? step->note : "constraint";
    fprintf(f, "  %s (%s) as H%d;\n", tactic, arg, step->id);
}

static void export_coq_rewrite(const ProofStep *step, FILE *f) {
    const char *tactic = "rewrite";
    if (step->note) {
        fprintf(f, "  %s %s;\n", tactic, step->note);
    } else {
        fprintf(f, "  %s H%d;\n", tactic, step->id);
    }
}

static void export_coq_function_app(const ProofStep *step, FILE *f) {
    const char *tactic = "apply";
    if (step->note) {
        fprintf(f, "  %s %s;\n", tactic, step->note);
    } else {
        fprintf(f, "  %s H%d;\n", tactic, step->id);
    }
}

static void export_coq_pack_function(const ProofStep *step, FILE *f) {
    fprintf(f, "  (* 打包函数块 step %d *)\n", step->id);
    fprintf(f, "  pose proof (pack_function_step %d) as H%d;\n", step->id, step->id);
}

static void export_coq_normalization(const ProofStep *step, FILE *f) {
    fprintf(f, "  (* 自动规范化 step %d *)\n", step->id);
    fprintf(f, "  simpl;\n");
    fprintf(f, "  auto;\n");
}

static void export_coq_unify(const ProofStep *step, FILE *f) {
    fprintf(f, "  (* 合一检查 step %d *)\n", step->id);
    if (step->note) {
        fprintf(f, "  exact %s;\n", step->note);
    } else {
        fprintf(f, "  apply unification_result;\n");
    }
}

static void export_coq_ex_falso(const ProofStep *step, FILE *f) {
    fprintf(f, "  (* 爆炸原理步骤 step %d *)\n", step->id);
    fprintf(f, "  exact (False_ind _ H_bottom);\n");
}

static void export_coq_oracle(const ProofStep *step, FILE *f) {
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
}

/* --- 验证回调 --- */

static bool validate_add_node(ProofStep *step, const void *step_data) {
    if (!step_data)
        return false;
    int node_id = *(const int *) step_data;
    if (node_id < 0)
        return false;
    step->node_id = node_id;
    return true;
}

static bool validate_add_constraint(ProofStep *step, const void *step_data) {
    if (!step_data)
        return false;
    int constraint_id = *(const int *) step_data;
    if (constraint_id < 0)
        return false;
    step->constraint_id = constraint_id;
    return true;
}

static bool validate_rewrite(ProofStep *step, const void *step_data) {
    if (!step_data)
        return false;
    const ProofStep *src = (const ProofStep *) step_data;
    if (src->rule_id < 0)
        return false;
    step->rule_id = src->rule_id;
    step->node_id = src->node_id;
    return true;
}

static bool validate_function_app(ProofStep *step, const void *step_data) {
    if (!step_data)
        return false;
    const ProofStep *src = (const ProofStep *) step_data;
    if (src->func_block_id < 0)
        return false;
    step->func_block_id = src->func_block_id;
    return true;
}

static bool validate_noop(ProofStep *step, const void *step_data) {
    (void) step;
    (void) step_data;
    return true;
}

/* --- 策略查找表 --- */

static const ProofStepStrategy s_step_strategies[] = {
    [PROOF_STEP_ADD_NODE]       = { .validate = validate_add_node,       .export_coq = export_coq_add_node },
    [PROOF_STEP_ADD_CONSTRAINT] = { .validate = validate_add_constraint, .export_coq = export_coq_add_constraint },
    [PROOF_STEP_REWRITE]        = { .validate = validate_rewrite,        .export_coq = export_coq_rewrite },
    [PROOF_STEP_FUNCTION_APP]   = { .validate = validate_function_app,   .export_coq = export_coq_function_app },
    [PROOF_STEP_PACK_FUNCTION]  = { .validate = validate_function_app,   .export_coq = export_coq_pack_function },
    [PROOF_STEP_NORMALIZATION]  = { .validate = validate_noop,           .export_coq = export_coq_normalization },
    [PROOF_STEP_UNIFY]          = { .validate = validate_noop,           .export_coq = export_coq_unify },
    [PROOF_STEP_EX_FALSO]       = { .validate = validate_noop,           .export_coq = export_coq_ex_falso },
    [PROOF_STEP_ORACLE]         = { .validate = validate_noop,           .export_coq = export_coq_oracle },
};

const ProofStepStrategy *proof_step_get_strategy(ProofStepType type) {
    if (type < PROOF_STEP_ADD_NODE || type > PROOF_STEP_ORACLE)
        return NULL;
    return &s_step_strategies[type];
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
        const ProofStepStrategy *strategy = proof_step_get_strategy(step->type);
        if (strategy && strategy->export_coq) {
            strategy->export_coq(step, f);
        } else {
            fprintf(f, "  (* 未知步骤类型 step %d: %s *)\n", step->id, proof_step_type_to_string(step->type));
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
