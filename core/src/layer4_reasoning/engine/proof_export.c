/**
 * @file proof_export.c
 * @brief 证明导出（自然语言/LaTeX/Coq/Isar）
 *
 * @details 本文件从 proof_engine_enhanced.c 拆分子模块生成（Lv-00 v3.3.0+）。
 *
 * 【与其他导出模块的定位关系（勿误合并）】
 * 本模块（layer4_reasoning）与 layer5_output/proof_export_enhanced.c
 * 是双轨设计，输入类型与输出契约均不同，无法统一为单一 dispatch：
 *   - 本模块：输入 lvProofTraceTree*（引擎追踪树），输出 char*（lv_free 释放），
 *     格式为 自然语言 / LaTeX / Coq / Isar；格式分发已表驱动
 *     （kCoqFormats / kIsarFormats / kTypeLatexLabel 等，按 lvTraceNodeType 索引）。
 *   - proof_export_enhanced.c：输入 lvProof*（轻量步骤数组），输出 lvExportResult*
 *     （proof_export_result_destroy 释放），格式为 HTML / LaTeX / Coq / Lean4 /
 *     JSON / DOT；格式分发已表驱动（kExportHandlers[]，按 lvExportFormat 索引）。
 * 两模块格式枚举语义不同（节点类型 vs 导出格式），层方向为 layer5 依赖 layer4。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/proof_engine_enhanced.h"
#include "proof_engine_enhanced_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"
#include "lv/lv_str_utils.h" /* lv_str_latex_escape_alloc（K35 LaTeX 转义补齐） */

#include "lv/axiom_rule_engine.h"
#include "lv/error_codes.h"
#include "lv/three_valued_logic.h"

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/trust_color_x.h"

/* ============== 查找表 ============== */

/* ================================================================
 * lvTraceNodeType / lvTraceNodeStatus 呈现属性（单一事实来源）
 *
 * 由下列条目宏生成 kTraceNodeProps[] / kTraceStatusProps[] 结构体数组；
 * proof_export.c 与 proof_trace_tree.c 一律索引该数组取呈现属性，
 * 禁止再手写任何按 lvTraceNodeType / lvTraceNodeStatus 下标的平行表。
 * ================================================================ */

/* 信任色哨兵值（与 proof_trace_tree.c 定义一致）：
 * DERIVATION / GOAL 需递归取子节点最小颜色；UNKNOWN 表示未知类型 */
#define TRACE_NODE_COLOR_AUTO (-1)
#define TRACE_NODE_COLOR_UNKNOWN (-2)

/**
 * @brief lvTraceNodeType 全字段条目宏（单一事实来源）
 *
 * 每行携带 7 列：ENUM（枚举值）、ZH（中文名）、EN（英文名）、
 * LATEX（LaTeX 标签）、TRUST（初始信任色，可为 TRACE_NODE_COLOR_* 哨兵）、
 * DOT_FILL（DOT 填充色）、DOT_SHAPE（DOT 形状）。
 */
#define LV_TRACE_NODE_TYPE_ENTRY(x) \
    x(TRACE_NODE_AXIOM, "公理", "Axiom", "\\textbf{Axiom}", TRUST_GREEN, "lightgreen", "ellipse") \
    x(TRACE_NODE_DEFINITION, "定义", "Definition", "\\textbf{Def}", TRUST_GREEN, "palegreen", "box") \
    x(TRACE_NODE_THEOREM, "定理", "Theorem", "\\textbf{Thm}", TRUST_GREEN, "green", "box") \
    x(TRACE_NODE_LEMMA, "引理", "Lemma", "\\textbf{Lemma}", TRUST_GREEN, "limegreen", "box") \
    x(TRACE_NODE_HYPOTHESIS, "假设", "Hypothesis", "\\textit{Hyp}", TRUST_BLUE_UNEXPLORED, "lightblue", "diamond") \
    x(TRACE_NODE_DERIVATION, "推导", "Derivation", "\\textbf{Step}", TRACE_NODE_COLOR_AUTO, "lightgray", "rounded") \
    x(TRACE_NODE_CONTRADICTION, "矛盾", "Contradiction", "\\textbf{Contr!}", TRUST_GREEN, "salmon", "octagon") \
    x(TRACE_NODE_GOAL, "目标", "Goal", "\\textbf{Goal}", TRACE_NODE_COLOR_AUTO, "gold", "doublecircle")

/** @brief lvTraceNodeType 呈现属性条目（索引 kTraceNodeProps[]） */
typedef struct TraceNodeProps {
    const char *name_zh;     /* 中文名 */
    const char *name_en;     /* 英文名 */
    const char *latex_label; /* LaTeX 标签 */
    int         trust_color; /* 初始信任色（TRACE_NODE_COLOR_* 哨兵除外） */
    const char *dot_fill;    /* DOT 填充色 */
    const char *dot_shape;   /* DOT 形状 */
} TraceNodeProps;

#define LV_TRACE_NODE_PROP_ROW(ENUM, ZH, EN, LATEX, TRUST, DOT_FILL, DOT_SHAPE) \
    { ZH, EN, LATEX, TRUST, DOT_FILL, DOT_SHAPE },

/* extern 前向声明：确保 const 数组具有 external linkage，
 * proof_trace_tree.c 可 extern 引用并按下标索引（单一事实来源） */
extern const TraceNodeProps kTraceNodeProps[];
const TraceNodeProps kTraceNodeProps[] = {
    LV_TRACE_NODE_TYPE_ENTRY(LV_TRACE_NODE_PROP_ROW)
};
#undef LV_TRACE_NODE_PROP_ROW

/**
 * @brief lvTraceNodeStatus 全字段条目宏（单一事实来源，中文/英文显示名）
 */
#define LV_TRACE_NODE_STATUS_ENTRY(x) \
    x(TRACE_STATUS_UNEXPLORED, "[探索中]", "[EXPLORING]") \
    x(TRACE_STATUS_EXPLORING, "[探索中]", "[EXPLORING]") \
    x(TRACE_STATUS_PROVED, "[已证明]", "[PROVED]") \
    x(TRACE_STATUS_DISPROVED, "[已证伪]", "[DISPROVED]") \
    x(TRACE_STATUS_BLOCKED, "[阻塞]", "[BLOCKED]")

/** @brief lvTraceNodeStatus 呈现属性条目（索引 kTraceStatusProps[]） */
typedef struct TraceStatusProps {
    const char *name_zh; /* 中文名 */
    const char *name_en; /* 英文名 */
} TraceStatusProps;

#define LV_TRACE_NODE_STATUS_ROW(ENUM, ZH, EN) { ZH, EN },
static const TraceStatusProps kTraceStatusProps[] = {
    LV_TRACE_NODE_STATUS_ENTRY(LV_TRACE_NODE_STATUS_ROW)
};
#undef LV_TRACE_NODE_STATUS_ROW

/**
 * @brief 获取溯源节点类型的呈现属性条目（kTraceNodeProps 越界安全访问）
 * @param type 溯源节点类型
 * @return 指向 kTraceNodeProps[type] 的指针；越界返回 NULL
 */
const TraceNodeProps *lv_trace_node_type_props(lvTraceNodeType type) {
    if ((unsigned) type >= lv_ARRAY_SIZE(kTraceNodeProps))
        return NULL;
    return &kTraceNodeProps[type];
}

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

/* 数组大小常量（kStatusNameZh/kTypeNameZh/kTypeLatexLabel 已并入
 * kTraceStatusProps/kTraceNodeProps 结构体数组） */
#define kStatusNameCount (sizeof(kTraceStatusProps) / sizeof(kTraceStatusProps[0]))
#define kTypeNameCount   (sizeof(kTraceNodeProps)   / sizeof(kTraceNodeProps[0]))
#define kTypeLatexCount  (sizeof(kTraceNodeProps)   / sizeof(kTraceNodeProps[0]))
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
                status_str = (lang == PROOF_NL_LANG_ZH_CN) ? kTraceStatusProps[idx].name_zh : kTraceStatusProps[idx].name_en;
            } else {
                status_str = (lang == PROOF_NL_LANG_ZH_CN) ? "[探索中]" : "[EXPLORING]";
            }
        }

        const char *type_str;
        {
            int idx = (int)node->type;
            if (idx >= 0 && idx < (int)kTypeNameCount) {
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? kTraceNodeProps[idx].name_zh : kTraceNodeProps[idx].name_en;
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

    /* K35：LaTeX 转义辅助——对用户可控字符串（description/label/rule name）
     * 转义 _ % \ 等特殊字符，防止破坏 LaTeX 编译（原实现原样写入） */
    #define PROOF_LATEX_APPEND(sb, str)                                     \
        do {                                                               \
            if ((str) && (str)[0] != '\0') {                               \
                char *_esc = lv_str_latex_escape_alloc(str);               \
                if (_esc) {                                                \
                    lv_strbuf_append_str((sb), _esc);                      \
                    lv_free((void **) &_esc);                              \
                } else {                                                   \
                    lv_strbuf_append_str((sb), (str));                     \
                }                                                          \
            }                                                              \
        } while (0)

    /* LaTeX 文档头 */
    lv_strbuf_printf(&buf, "\\begin{proof}\n");

    /* 根节点描述 */
    if (trace->root && trace->root->description[0] != '\0') {
        lv_strbuf_printf(&buf, "  \\textit{");
        PROOF_LATEX_APPEND(&buf, trace->root->description);
        lv_strbuf_append_str(&buf, "}\n\n");
    }

    /* 信任颜色映射到 LaTeX 颜色（LaTeX 格式派生态；TrustColor 权威名称见 trust_color.c trust_color_name()） */
    #define LV_TRUST_COLOR_TO_LATEX(sym, disp, ser, dot, tex) tex,
    static const char *color_map[] = {
        LV_TRUST_COLOR_X(LV_TRUST_COLOR_TO_LATEX)
    };
    #undef LV_TRUST_COLOR_TO_LATEX
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
                type_label = kTraceNodeProps[idx].latex_label;
            } else {
                type_label = "Step";
            }
        }

        int color_idx = (int) node->trust_color;
        if (color_idx < 0 || color_idx >= color_map_count)
            color_idx = 0;

        lv_strbuf_append_str(&buf, "  \\noindent ");
        PROOF_LATEX_APPEND(&buf, type_label);
        lv_strbuf_append_str(&buf, "[");
        PROOF_LATEX_APPEND(&buf, node->label);
        lv_strbuf_append_str(&buf, "] ");
        PROOF_LATEX_APPEND(&buf, color_map[color_idx]);
        lv_strbuf_append_str(&buf, "{");
        PROOF_LATEX_APPEND(&buf, node->label);
        lv_strbuf_append_str(&buf, "}\n");

        if (node->description[0] != '\0') {
            lv_strbuf_append_str(&buf, "  \\\\ \\quad ");
            PROOF_LATEX_APPEND(&buf, node->description);
            lv_strbuf_append_str(&buf, "\n");
        }

        if (node->rule && node->rule->name[0] != '\0') {
            lv_strbuf_append_str(&buf, "  \\\\ \\quad \\textit{by} \\texttt{");
            PROOF_LATEX_APPEND(&buf, node->rule->name);
            lv_strbuf_append_str(&buf, "}\n");
        }

        lv_strbuf_append_str(&buf, "\n");
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
