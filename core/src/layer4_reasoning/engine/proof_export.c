/**
 * @file proof_export.c
 * @brief 证明导出（自然语言/LaTeX/Coq/Isar）
 *
 * @details 本文件从 proof_engine_enhanced.c 拆分子模块生成（Lv-00 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "proof_engine_enhanced.h"
#include "proof_engine_enhanced_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"

#include "axiom_rule_engine.h"
#include "error_codes.h"
#include "lv.h"
#include "three_valued_logic.h"

#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"

/* ============== 证明导出 ============== */
/* 自然语言、LaTeX、Coq、Isar 格式输出 */

/**
 * @brief 导出证明为自然语言文本
 *
 * 生成 AlphaGeometry 风格的人类可读证明文本。
 * 每一步都包含完整的自然语言描述，说明：
 *   - 应用了什么推理规则
 *   - 涉及哪些数学对象
 *   - 为什么可以进行这一步
 *
 * @param trace 溯源树
 * @param lang  输出语言（中文/英文）
 * @return 自然语言文本（调用者需用 lv_free 释放），失败返回 NULL
 */
char *lv_proof_to_natural_language(const lvProofTraceTree *trace, ProofNaturalLanguage lang) {
    if (!trace) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_to_natural_language: trace is NULL");
    }

    lvStrBuf buf = {0};

    const char *proof_str = (lang == PROOF_NL_LANG_ZH_CN) ? "证明" : "Proof";
    const char *begin_str =
        (lang == PROOF_NL_LANG_ZH_CN) ? "以下是该命题的证明过程：" : "Below is the proof of this proposition:";

    lv_strbuf_printf(&buf, "%s\n", proof_str);
    lv_strbuf_printf(&buf, "%s\n\n", begin_str);

    /* 遍历溯源树生成自然语言 */
    for (int i = 0; i < trace->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&trace->all_nodes, i);
        lvProofTraceNode *node = *node_p;

        /* 跳过根节点 */
        if (node == trace->root)
            continue;

        const char *status_str;
        switch (node->status) {
            case TRACE_STATUS_PROVED:
                status_str = (lang == PROOF_NL_LANG_ZH_CN) ? "[已证明]" : "[PROVED]";
                break;
            case TRACE_STATUS_DISPROVED:
                status_str = (lang == PROOF_NL_LANG_ZH_CN) ? "[已证伪]" : "[DISPROVED]";
                break;
            case TRACE_STATUS_BLOCKED:
                status_str = (lang == PROOF_NL_LANG_ZH_CN) ? "[阻塞]" : "[BLOCKED]";
                break;
            default:
                status_str = (lang == PROOF_NL_LANG_ZH_CN) ? "[探索中]" : "[EXPLORING]";
                break;
        }

        const char *type_str;
        switch (node->type) {
            case TRACE_NODE_AXIOM:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "公理" : "Axiom";
                break;
            case TRACE_NODE_DEFINITION:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "定义" : "Definition";
                break;
            case TRACE_NODE_THEOREM:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "定理" : "Theorem";
                break;
            case TRACE_NODE_LEMMA:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "引理" : "Lemma";
                break;
            case TRACE_NODE_HYPOTHESIS:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "假设" : "Hypothesis";
                break;
            case TRACE_NODE_DERIVATION:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "推导" : "Derivation";
                break;
            case TRACE_NODE_CONTRADICTION:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "矛盾" : "Contradiction";
                break;
            case TRACE_NODE_GOAL:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "目标" : "Goal";
                break;
            default:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "未知" : "Unknown";
                break;
        }

        if (lang == PROOF_NL_LANG_ZH_CN) {
            lv_strbuf_printf(&buf, "步骤 %u: [%s] %s %s", i + 1, type_str, node->label, status_str);

            if (node->description[0] != '\0') {
                lv_strbuf_printf(&buf, "\n  说明: %s", node->description);
            }

            if (node->rule && node->rule->name[0] != '\0') {
                lv_strbuf_printf(&buf, "\n  应用规则: %s", node->rule->name);
            }

            if (node->elapsed_ms > 0) {
                lv_strbuf_printf(&buf, "\n  耗时: %.2f ms", node->elapsed_ms);
            }
        } else {
            lv_strbuf_printf(&buf, "Step %u: [%s] %s %s", i + 1, type_str, node->label, status_str);

            if (node->description[0] != '\0') {
                lv_strbuf_printf(&buf, "\n  Description: %s", node->description);
            }

            if (node->rule && node->rule->name[0] != '\0') {
                lv_strbuf_printf(&buf, "\n  Applied rule: %s", node->rule->name);
            }

            if (node->elapsed_ms > 0) {
                lv_strbuf_printf(&buf, "\n  Time: %.2f ms", node->elapsed_ms);
            }
        }

        lv_strbuf_printf(&buf, "\n\n");
    }

    /* 结论 */
    if (trace->is_complete) {
        if (lang == PROOF_NL_LANG_ZH_CN) {
            lv_strbuf_printf(&buf, "证毕。\\qed\n");
        } else {
            lv_strbuf_printf(&buf, "Q.E.D.\\qed\n");
        }
    } else {
        if (lang == PROOF_NL_LANG_ZH_CN) {
            lv_strbuf_printf(&buf, "证明未完成。\n");
        } else {
            lv_strbuf_printf(&buf, "Proof incomplete.\n");
        }
    }

    return lv_strbuf_to_string(&buf);
}

/**
 * @brief 导出证明为 LaTeX 格式
 *
 * 生成完整的 LaTeX 证明文档，包含：
 *   - proof 环境
 *   - 每个步骤的描述
 *   - 规则引用
 *   - 信任颜色标注
 *
 * @param trace 溯源树
 * @return LaTeX 文本（调用者需用 lv_free 释放），失败返回 NULL
 */
char *lv_proof_to_latex(const lvProofTraceTree *trace) {
    if (!trace) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_to_latex: trace is NULL");
    }

    lvStrBuf buf = {0};

    /* LaTeX 文档头 */
    lv_strbuf_printf(&buf, "\\begin{proof}\n");

    /* 根节点描述 */
    if (trace->root && trace->root->description[0] != '\0') {
        lv_strbuf_printf(&buf, "  \\textit{%s}\n\n", trace->root->description);
    }

    /* 信任颜色映射到 LaTeX 颜色 */
    static const char *color_map[] = {
        "\\textcolor{green}{}",     /* TRUST_GREEN */
        "\\textcolor{blue}{}",      /* TRUST_BLUE_UNEXPLORED */
        "\\textcolor{blue}{}",      /* TRUST_BLUE_EXCEEDED */
        "\\textcolor{blue}{}",      /* TRUST_BLUE_OUT_OF_SCOPE */
        "\\textcolor{yellow}{}",    /* TRUST_YELLOW */
        "\\textcolor{orange!70}{}", /* TRUST_LIGHT_ORANGE_ORACLE */
        "\\textcolor{orange!70}{}", /* TRUST_LIGHT_ORANGE_EXPLOSION */
        "\\textcolor{orange!50}{}", /* TRUST_AMBER */
        "\\textcolor{orange}{}",    /* TRUST_DEEP_ORANGE */
        "\\textcolor{red}{}"        /* TRUST_RED */
    };
    static const int color_map_count = sizeof(color_map) / sizeof(color_map[0]);

    /* 遍历节点生成 LaTeX */
    for (int i = 0; i < trace->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&trace->all_nodes, i);
        lvProofTraceNode *node = *node_p;
        if (node == trace->root)
            continue;

        /* 节点类型标签 */
        const char *type_label;
        switch (node->type) {
            case TRACE_NODE_AXIOM:
                type_label = "\\textbf{Axiom}";
                break;
            case TRACE_NODE_DEFINITION:
                type_label = "\\textbf{Def}";
                break;
            case TRACE_NODE_THEOREM:
                type_label = "\\textbf{Thm}";
                break;
            case TRACE_NODE_LEMMA:
                type_label = "\\textbf{Lemma}";
                break;
            case TRACE_NODE_HYPOTHESIS:
                type_label = "\\textit{Hyp}";
                break;
            case TRACE_NODE_DERIVATION:
                type_label = "\\textbf{Step}";
                break;
            case TRACE_NODE_CONTRADICTION:
                type_label = "\\textbf{Contr!}";
                break;
            case TRACE_NODE_GOAL:
                type_label = "\\textbf{Goal}";
                break;
            default:
                type_label = "Step";
                break;
        }

        int color_idx = (int) node->trust_color;
        if (color_idx < 0 || color_idx >= color_map_count)
            color_idx = 0;

        lv_strbuf_printf(&buf, "  \\noindent %s[%s] %s%s}\n", type_label, node->label, color_map[color_idx],
                             node->label);

        if (node->description[0] != '\0') {
            lv_strbuf_printf(&buf, "  \\\\ \\quad %s\n", node->description);
        }

        if (node->rule && node->rule->name[0] != '\0') {
            lv_strbuf_printf(&buf, "  \\\\ \\quad \\textit{by} \\texttt{%s}\n", node->rule->name);
        }

        lv_strbuf_printf(&buf, "\n");
    }

    /* 结论 */
    if (trace->is_complete) {
        lv_strbuf_printf(&buf, "  \\hfill $\\qed$\n");
    } else {
        lv_strbuf_printf(&buf, "  \\textcolor{red}{\\textit{Proof incomplete.}}\n");
    }

    lv_strbuf_printf(&buf, "\\end{proof}\n");

    return lv_strbuf_to_string(&buf);
}

/**
 * @brief 导出证明为 Coq 脚本
 *
 * 生成 Coq 形式化证明脚本，包含：
 *   - Theorem/Lemma 声明
 *   - Proof 开始
 *   - 策略（tactic）序列
 *   - Qed 结束
 *
 * @param trace 溯源树
 * @return Coq 脚本（调用者需用 lv_free 释放），失败返回 NULL
 */
char *lv_proof_to_coq(const lvProofTraceTree *trace) {
    if (!trace) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_to_coq: trace is NULL");
    }

    lvStrBuf buf = {0};

    /* 定理声明 */
    const char *theorem_name = "theorem_result";
    if (trace->root && trace->root->label[0] != '\0') {
        theorem_name = trace->root->label;
    }

    lv_strbuf_printf(&buf, "Theorem %s : Prop.\n", theorem_name);
    lv_strbuf_printf(&buf, "Proof.\n");

    /* 遍历节点生成 Coq tactic */
    for (int i = 0; i < trace->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&trace->all_nodes, i);
        lvProofTraceNode *node = *node_p;
        if (node == trace->root)
            continue;

        switch (node->type) {
            case TRACE_NODE_AXIOM:
                lv_strbuf_printf(&buf, "  (* Axiom: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  apply %s_axiom.\n", node->label);
                break;

            case TRACE_NODE_HYPOTHESIS:
                lv_strbuf_printf(&buf, "  (* Hypothesis: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  intro H%s.\n", node->label);
                break;

            case TRACE_NODE_DERIVATION:
                if (node->rule && node->rule->name[0] != '\0') {
                    lv_strbuf_printf(&buf, "  (* Apply rule: %s *)\n", node->rule->name);
                    lv_strbuf_printf(&buf, "  apply %s.\n", node->rule->name);
                } else {
                    lv_strbuf_printf(&buf, "  (* Derivation: %s *)\n", node->label);
                    lv_strbuf_printf(&buf, "  assert (H%d : Prop).\n", node->id);
                    lv_strbuf_printf(&buf, "  { %s. }\n", node->description);
                }
                break;

            case TRACE_NODE_CONTRADICTION:
                lv_strbuf_printf(&buf, "  (* Contradiction: %s *)\n", node->description);
                lv_strbuf_printf(&buf, "  contradiction.\n");
                break;

            case TRACE_NODE_GOAL:
                lv_strbuf_printf(&buf, "  (* Sub-goal: %s *)\n", node->label);
                break;

            case TRACE_NODE_THEOREM:
                lv_strbuf_printf(&buf, "  (* Apply theorem: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  apply %s.\n", node->label);
                break;

            case TRACE_NODE_LEMMA:
                lv_strbuf_printf(&buf, "  (* Apply lemma: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  apply %s_lemma.\n", node->label);
                break;

            case TRACE_NODE_DEFINITION:
                lv_strbuf_printf(&buf, "  (* Unfold definition: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  unfold %s.\n", node->label);
                break;

            default:
                lv_strbuf_printf(&buf, "  (* Step: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  admit.\n");
                break;
        }
    }

    /* 结束证明 */
    if (trace->is_complete) {
        lv_strbuf_printf(&buf, "Qed.\n");
    } else {
        lv_strbuf_printf(&buf, "  (* Proof incomplete *)\n");
        lv_strbuf_printf(&buf, "Admitted.\n");
    }

    return lv_strbuf_to_string(&buf);
}

/**
 * @brief 导出证明为 Isar 脚本
 *
 * 生成 Isabelle/HOL Isar 结构化证明文本，包含：
 *   - theorem/lemma 声明
 *   - proof 开始
 *   - have/show/then 结构化步骤
 *   - qed 结束
 *
 * @param trace 溯源树
 * @return Isar 脚本（调用者需用 lv_free 释放），失败返回 NULL
 */
char *lv_proof_to_isar(const lvProofTraceTree *trace) {
    if (!trace) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_to_isar: trace is NULL");
    }

    lvStrBuf buf = {0};

    /* 定理声明 */
    const char *theorem_name = "theorem_result";
    if (trace->root && trace->root->label[0] != '\0') {
        theorem_name = trace->root->label;
    }

    lv_strbuf_printf(&buf, "theorem %s\n", theorem_name);
    if (trace->root && trace->root->description[0] != '\0') {
        lv_strbuf_printf(&buf, "  -- \"%s\"\n", trace->root->description);
    }
    lv_strbuf_printf(&buf, "where\n");
    lv_strbuf_printf(&buf, "proof -\n");

    /* 遍历节点生成 Isar */
    for (int i = 0; i < trace->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&trace->all_nodes, i);
        lvProofTraceNode *node = *node_p;
        if (node == trace->root)
            continue;

        switch (node->type) {
            case TRACE_NODE_AXIOM:
                lv_strbuf_printf(&buf, "  -- Axiom: %s\n", node->label);
                lv_strbuf_printf(&buf, "  have \"%s\" by auto\n", node->label);
                break;

            case TRACE_NODE_HYPOTHESIS:
                lv_strbuf_printf(&buf, "  -- Hypothesis: %s\n", node->label);
                lv_strbuf_printf(&buf, "  assume \"%s\"\n", node->label);
                break;

            case TRACE_NODE_DERIVATION:
                if (node->rule && node->rule->name[0] != '\0') {
                    lv_strbuf_printf(&buf, "  -- Apply rule: %s\n", node->rule->name);
                    lv_strbuf_printf(&buf, "  then have \"%s\" using %s\n", node->label, node->rule->name);
                } else {
                    lv_strbuf_printf(&buf, "  -- Derivation: %s\n", node->label);
                    lv_strbuf_printf(&buf, "  have \"%s\"\n", node->label);
                    if (node->description[0] != '\0') {
                        lv_strbuf_printf(&buf, "    -- \"%s\"\n", node->description);
                    }
                    lv_strbuf_printf(&buf, "    sorry\n");
                }
                break;

            case TRACE_NODE_CONTRADICTION:
                lv_strbuf_printf(&buf, "  -- Contradiction: %s\n", node->description);
                lv_strbuf_printf(&buf, "  then show False\n");
                lv_strbuf_printf(&buf, "    contradiction\n");
                break;

            case TRACE_NODE_GOAL:
                lv_strbuf_printf(&buf, "  -- Sub-goal: %s\n", node->label);
                lv_strbuf_printf(&buf, "  moreover have \"%s\"\n", node->label);
                break;

            case TRACE_NODE_THEOREM:
                lv_strbuf_printf(&buf, "  -- Theorem: %s\n", node->label);
                lv_strbuf_printf(&buf, "  from `%s` have \"%s\" .\n", node->label, node->label);
                break;

            case TRACE_NODE_LEMMA:
                lv_strbuf_printf(&buf, "  -- Lemma: %s\n", node->label);
                lv_strbuf_printf(&buf, "  using `%s_lemma`\n", node->label);
                break;

            case TRACE_NODE_DEFINITION:
                lv_strbuf_printf(&buf, "  -- Unfold: %s\n", node->label);
                lv_strbuf_printf(&buf, "  unfolding %s_def\n", node->label);
                break;

            default:
                lv_strbuf_printf(&buf, "  -- Step: %s\n", node->label);
                lv_strbuf_printf(&buf, "  sorry\n");
                break;
        }
    }

    /* 结束证明 */
    if (trace->is_complete) {
        lv_strbuf_printf(&buf, "qed\n");
    } else {
        lv_strbuf_printf(&buf, "  -- Proof incomplete\n");
        lv_strbuf_printf(&buf, "sorry\n");
    }

    return lv_strbuf_to_string(&buf);
}
