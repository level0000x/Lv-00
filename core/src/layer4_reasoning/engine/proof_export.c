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

/* ============== 查找表 ============== */

/* 状态名称表（按 lvTraceNodeStatus 枚举值索引） */
static const char *kStatusNameZh[] = {
    "[探索中]",  /* TRACE_STATUS_UNEXPLORED */
    "[探索中]",  /* TRACE_STATUS_EXPLORING */
    "[已证明]",  /* TRACE_STATUS_PROVED */
    "[已证伪]",  /* TRACE_STATUS_DISPROVED */
    "[阻塞]"     /* TRACE_STATUS_BLOCKED */
};
static const char *kStatusNameEn[] = {
    "[EXPLORING]",
    "[EXPLORING]",
    "[PROVED]",
    "[DISPROVED]",
    "[BLOCKED]"
};

/* 类型名称表（按 lvTraceNodeType 枚举值索引） */
static const char *kTypeNameZh[] = {
    "公理",         /* TRACE_NODE_AXIOM */
    "定义",         /* TRACE_NODE_DEFINITION */
    "定理",         /* TRACE_NODE_THEOREM */
    "引理",         /* TRACE_NODE_LEMMA */
    "假设",         /* TRACE_NODE_HYPOTHESIS */
    "推导",         /* TRACE_NODE_DERIVATION */
    "矛盾",         /* TRACE_NODE_CONTRADICTION */
    "目标"          /* TRACE_NODE_GOAL */
};
static const char *kTypeNameEn[] = {
    "Axiom",
    "Definition",
    "Theorem",
    "Lemma",
    "Hypothesis",
    "Derivation",
    "Contradiction",
    "Goal"
};

/* LaTeX 类型标签表（按 lvTraceNodeType 枚举值索引） */
static const char *kTypeLatexLabel[] = {
    "\\textbf{Axiom}",      /* TRACE_NODE_AXIOM */
    "\\textbf{Def}",        /* TRACE_NODE_DEFINITION */
    "\\textbf{Thm}",        /* TRACE_NODE_THEOREM */
    "\\textbf{Lemma}",      /* TRACE_NODE_LEMMA */
    "\\textit{Hyp}",        /* TRACE_NODE_HYPOTHESIS */
    "\\textbf{Step}",       /* TRACE_NODE_DERIVATION */
    "\\textbf{Contr!}",     /* TRACE_NODE_CONTRADICTION */
    "\\textbf{Goal}"        /* TRACE_NODE_GOAL */
};

/* Coq 格式条目 */
typedef struct {
    const char *comment_fmt;   /* 注释行格式 (使用 node->label) */
    const char *action_fmt;    /* 动作行格式 (使用 node->label), NULL 表示无动作 */
} CoqFormatEntry;

/* Coq 格式查找表（按 lvTraceNodeType 枚举值索引） */
/* DERIVATION 有特殊条件逻辑，表中不包含 */
static const CoqFormatEntry kCoqFormats[] = {
    [TRACE_NODE_AXIOM]         = { "  (* Axiom: %s *)\n",           "  apply %s_axiom.\n" },
    [TRACE_NODE_DEFINITION]    = { "  (* Unfold definition: %s *)\n","  unfold %s.\n" },
    [TRACE_NODE_THEOREM]       = { "  (* Apply theorem: %s *)\n",   "  apply %s.\n" },
    [TRACE_NODE_LEMMA]         = { "  (* Apply lemma: %s *)\n",     "  apply %s_lemma.\n" },
    [TRACE_NODE_HYPOTHESIS]    = { "  (* Hypothesis: %s *)\n",      "  intro H%s.\n" },
    [TRACE_NODE_CONTRADICTION] = { "  (* Contradiction: %s *)\n",   "  contradiction.\n" },
    [TRACE_NODE_GOAL]          = { "  (* Sub-goal: %s *)\n",        NULL },
};

/* Isar 格式条目 */
typedef struct {
    const char *comment_fmt;   /* 注释行格式 (使用 node->label) */
    const char *action_fmt1;   /* 第一动作行格式 (使用 node->label), NULL 表示无 */
    const char *action_fmt2;   /* 第二动作行格式, NULL 表示无 */
    const char *action_fmt3;   /* 第三动作行格式, NULL 表示无 */
} IsarFormatEntry;

/* Isar 格式查找表（按 lvTraceNodeType 枚举值索引） */
/* DERIVATION 有特殊条件逻辑，表中不包含 */
static const IsarFormatEntry kIsarFormats[] = {
    [TRACE_NODE_AXIOM]         = { "  -- Axiom: %s\n",              "  have \"%s\" by auto\n",          NULL, NULL },
    [TRACE_NODE_DEFINITION]    = { "  -- Unfold: %s\n",             "  unfolding %s_def\n",             NULL, NULL },
    [TRACE_NODE_THEOREM]       = { "  -- Theorem: %s\n",            "  from `%s` have \"%s\" .\n",      NULL, NULL },
    [TRACE_NODE_LEMMA]         = { "  -- Lemma: %s\n",              "  using `%s_lemma`\n",             NULL, NULL },
    [TRACE_NODE_HYPOTHESIS]    = { "  -- Hypothesis: %s\n",         "  assume \"%s\"\n",                NULL, NULL },
    [TRACE_NODE_CONTRADICTION] = { "  -- Contradiction: %s\n",      "  then show False\n",              "    contradiction\n", NULL },
    [TRACE_NODE_GOAL]          = { "  -- Sub-goal: %s\n",           "  moreover have \"%s\"\n",         NULL, NULL },
};

/* 数组大小常量 */
#define kStatusNameCount (sizeof(kStatusNameZh) / sizeof(kStatusNameZh[0]))
#define kTypeNameCount   (sizeof(kTypeNameZh)   / sizeof(kTypeNameZh[0]))
#define kTypeLatexCount  (sizeof(kTypeLatexLabel)/ sizeof(kTypeLatexLabel[0]))
#define kCoqFormatCount  (sizeof(kCoqFormats)    / sizeof(kCoqFormats[0]))
#define kIsarFormatCount (sizeof(kIsarFormats)   / sizeof(kIsarFormats[0]))

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
        {
            int idx = (int)node->status;
            if (idx >= 0 && idx < (int)kStatusNameCount) {
                status_str = (lang == PROOF_NL_LANG_ZH_CN) ? kStatusNameZh[idx] : kStatusNameEn[idx];
            } else {
                status_str = (lang == PROOF_NL_LANG_ZH_CN) ? "[探索中]" : "[EXPLORING]";
            }
        }

        const char *type_str;
        {
            int idx = (int)node->type;
            if (idx >= 0 && idx < (int)kTypeNameCount) {
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? kTypeNameZh[idx] : kTypeNameEn[idx];
            } else {
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "未知" : "Unknown";
            }
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
        {
            int idx = (int)node->type;
            if (idx >= 0 && idx < (int)kTypeLatexCount) {
                type_label = kTypeLatexLabel[idx];
            } else {
                type_label = "Step";
            }
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

        if (node->type == TRACE_NODE_DERIVATION) {
            /* DERIVATION 有特殊条件逻辑 */
            if (node->rule && node->rule->name[0] != '\0') {
                lv_strbuf_printf(&buf, "  (* Apply rule: %s *)\n", node->rule->name);
                lv_strbuf_printf(&buf, "  apply %s.\n", node->rule->name);
            } else {
                lv_strbuf_printf(&buf, "  (* Derivation: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  assert (H%d : Prop).\n", node->id);
                lv_strbuf_printf(&buf, "  { %s. }\n", node->description);
            }
        } else {
            int idx = (int)node->type;
            if (idx >= 0 && idx < (int)kCoqFormatCount && kCoqFormats[idx].comment_fmt != NULL) {
                const CoqFormatEntry *entry = &kCoqFormats[idx];
                const char *comment_arg = (node->type == TRACE_NODE_CONTRADICTION) ? node->description : node->label;
                lv_strbuf_printf(&buf, entry->comment_fmt, comment_arg);
                if (entry->action_fmt) {
                    lv_strbuf_printf(&buf, entry->action_fmt, node->label);
                }
            } else {
                /* 默认情况 */
                lv_strbuf_printf(&buf, "  (* Step: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  admit.\n");
            }
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

        if (node->type == TRACE_NODE_DERIVATION) {
            /* DERIVATION 有特殊条件逻辑 */
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
        } else {
            int idx = (int)node->type;
            if (idx >= 0 && idx < (int)kIsarFormatCount && kIsarFormats[idx].comment_fmt != NULL) {
                const IsarFormatEntry *entry = &kIsarFormats[idx];
                const char *comment_arg = (node->type == TRACE_NODE_CONTRADICTION) ? node->description : node->label;
                lv_strbuf_printf(&buf, entry->comment_fmt, comment_arg);
                if (entry->action_fmt1) {
                    if (node->type == TRACE_NODE_THEOREM) {
                        /* THEOREM 使用两次 node->label */
                        lv_strbuf_printf(&buf, entry->action_fmt1, node->label, node->label);
                    } else {
                        lv_strbuf_printf(&buf, entry->action_fmt1, node->label);
                    }
                }
                if (entry->action_fmt2) {
                    lv_strbuf_printf(&buf, entry->action_fmt2);
                }
                if (entry->action_fmt3) {
                    lv_strbuf_printf(&buf, entry->action_fmt3);
                }
            } else {
                /* 默认情况 */
                lv_strbuf_printf(&buf, "  -- Step: %s\n", node->label);
                lv_strbuf_printf(&buf, "  sorry\n");
            }
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
