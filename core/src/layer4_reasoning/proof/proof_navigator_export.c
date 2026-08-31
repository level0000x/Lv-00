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

#include "lv/lv_str_utils.h" /* lv_str_latex_escape_alloc（K35 LaTeX 转义补齐） */

#include "lv/lv_platform.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"
#include "lv/axiom_pkg.h"
#include "lv/constraint_graph.h"
#include "lv/proof.h"
#include "lv/proof_step_strategy.h"
#include "lv/proof_trace.h"
#include "lv/smt_backend.h"
#include "lv/trust_color.h"

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream.h"
#include "proof_navigator_internal.h"

/* ============== 导出功能 ============== */

bool proof_export_html(ProofNavigator *nav, const char *filepath) {
    (void) nav;
    (void) filepath;
    /* HTML 渲染已迁移至 UI 层（ui/L3-modules/P4-Proof/）。
       内核通过 lv_protocol.h 的 lvProofNavigator 结构体提供数据。 */
    return false;
}

/* 注：ProofColor → HTML 十六进制颜色映射（proof_color_to_html_hex）
 * 已收敛至 trust_color.c 公共层，见 lv/trust_color.h。 */

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
            /* K35：note 用户可控，LaTeX 转义防 _ % \ 破坏编译 */
            char *esc_note = lv_str_latex_escape_alloc(step->note);
            fprintf(f, " (%s)", esc_note ? esc_note : step->note);
            lv_free((void **) &esc_note);
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
