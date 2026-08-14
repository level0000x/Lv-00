/**
 * @file proof_export_enhanced.c
 * @brief 增强的证明导出功能实现
 *
 * 提供证明的多格式导出能力：
 * - HTML（Web 展示）
 * - LaTeX（学术文档嵌入）
 * - Coq（交互式定理证明器）
 * - Lean 4（现代定理证明器）
 * - JSON（结构化数据交换）
 * - DOT（Graphviz 图形化证明树）
 *
 * 【与其他导出模块的定位关系（勿误合并）】
 * 本模块（layer5_output）与 layer4_reasoning/engine/proof_export.c 是双轨设计：
 *   - 本模块：输入 lvProof*（轻量步骤数组，见 lv/proof_export_enhanced.h），
 *     输出 lvExportResult*（proof_export_result_destroy 释放），格式为
 *     HTML / LaTeX / Coq / Lean4 / JSON / DOT；格式分发已表驱动
 *     （kExportHandlers[]，按 lvExportFormat 索引）。
 *   - proof_export.c：输入 lvProofTraceTree*（引擎追踪树），输出 char*
 *     （lv_free 释放），格式为 自然语言 / LaTeX / Coq / Isar；其格式分发
 *     同样已表驱动（kCoqFormats / kIsarFormats 等，按 lvTraceNodeType 索引）。
 * 两模块格式枚举语义不同（导出格式 vs 节点类型），输入数据模型不同，
 * 无法统一为单一 dispatch；本模块的导出格式表是"格式枚举 → 处理函数"
 * 的唯一事实来源，新增格式只需在 lvExportFormat 与 kExportHandlers 各加一项。
 */

#include "lv/proof_export_enhanced.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/lv_dot_writer.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

/* ================================================================
 * 错误结果工厂
 * ================================================================ */

/**
 * @brief 创建错误结果对象
 * @param msg 错误消息（可为 NULL，使用默认消息）
 * @return 错误结果对象指针
 */
static lvExportResult *make_error(const char *msg) {
    lvExportResult *r = (lvExportResult *) lv_calloc(1, sizeof(lvExportResult));
    if (!r)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "make_error: lv_calloc failed");
    const char *src = msg ? msg : "Unknown error";
    size_t len = strlen(src);
    r->success = false;
    r->output = (char *) lv_malloc(len + 1);
    if (r->output) {
        memcpy(r->output, src, len + 1);
        r->output_size = len;
    } else {
        r->output_size = 0;
    }
    return r;
}

/**
 * @brief 创建成功结果对象（接管 lvStrBuf 内容所有权）
 * @param d 已完成写入的字符串构建器
 * @return 成功结果对象指针，失败返回 NULL
 *
 * @note 所有权语义：调用方使用完毕后应通过 proof_export_result_destroy()
 *       释放（内部对 r->output 执行 lv_free）。
 */
static lvExportResult *make_success(lvStrBuf *d) {
    lvExportResult *r = (lvExportResult *) lv_calloc(1, sizeof(lvExportResult));
    if (!r) {
        lv_strbuf_destroy(d);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "make_success: lv_calloc failed");
    }
    r->output = lv_strbuf_to_string(d); /* 堆拷贝（lv_malloc）并销毁构建器 */
    if (!r->output) {
        lv_free((void **) &r);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "make_success: to_string failed");
    }
    r->success = true;
    r->output_size = strlen(r->output);
    return r;
}

/* ================================================================
 * 工具函数
 * ================================================================ */

/**
 * @brief 安全字符串宏：NULL 转为空字符串
 * @param s 输入字符串
 * @return 非空字符串指针
 */
static const char *safe_str(const char *s) {
    return s ? s : "";
}

/**
 * @brief 将定理名转换为安全的标识符（替换特殊字符为下划线）
 * @param dst    输出缓冲区
 * @param dst_sz 输出缓冲区大小
 * @param src    源字符串
 */
static void sanitize_id(char *dst, size_t dst_sz, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < dst_sz; i++) {
        char c = src[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
            dst[j++] = c;
        } else if (c == ' ' || c == ':' || c == '-' || c == '>' || c == '(' || c == ')' || c == ',') {
            dst[j++] = '_';
        }
        /* skip other chars */
    }
    dst[j] = '\0';
    if (j == 0) {
        lv_strlcpy(dst, "theorem", dst_sz);
    }
}

/**
 * @brief JSON 字符串转义：在 str 前后加引号，经 lv_str_json_escape 两遍法完整转义
 *        （覆盖 \\、\"、\n、\r、\t、\b、\f 及其它控制字符）
 * @param d   字符串构建器指针
 * @param str 要转义的字符串（可为 NULL，输出 "null"）
 * @return 0 成功，-1 失败
 */
static int lv_sb_append_json_string(lvStrBuf *d, const char *str) {
    if (!str) {
        lv_strbuf_printf(d, "null");
        return 0;
    }
    size_t len = strlen(str);
    size_t need;
    char *buf = lv_str_json_escape_alloc(str, len, &need);
    if (!buf)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_sb_append_json_string: escape buffer alloc failed");
    lv_strbuf_printf(d, "\"");
    lv_strbuf_append_raw(d, buf, need);
    lv_strbuf_printf(d, "\"");
    lv_free((void **) &buf);
    return 0;
}

/**
 * @brief 将字符串 HTML 实体转义后写入 lvStrBuf（lv_str_html_escape_alloc 两遍法封装）
 * @param d   字符串构建器指针
 * @param str 要转义的字符串（可为 NULL，空操作）
 * @return 0 成功，-1 失败
 */
static int lv_sb_append_html_escaped(lvStrBuf *d, const char *str) {
    if (!str)
        return 0;
    char *buf = lv_str_html_escape_alloc(str);
    if (!buf)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_sb_append_html_escaped: escape buffer alloc failed");
    lv_strbuf_append_raw(d, buf, strlen(buf));
    lv_free((void **) &buf);
    return 0;
}

/**
 * @brief 将字符串 LaTeX 特殊字符转义后写入 lvStrBuf（lv_str_latex_escape_alloc 两遍法封装）
 * @param d   字符串构建器指针
 * @param str 要转义的字符串（可为 NULL，空操作）
 * @return 0 成功，-1 失败
 */
static int lv_sb_append_latex_escaped(lvStrBuf *d, const char *str) {
    if (!str)
        return 0;
    char *buf = lv_str_latex_escape_alloc(str);
    if (!buf)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_sb_append_latex_escaped: escape buffer alloc failed");
    lv_strbuf_append_raw(d, buf, strlen(buf));
    lv_free((void **) &buf);
    return 0;
}

/* ================================================================
 * HTML 导出
 * ================================================================ */

static lvExportResult *export_html(const lvProof *proof, const lvExportConfig *config) {
    lvStrBuf d;
    lv_strbuf_init(&d);

    const char *indent = config->pretty_print ? "\n" : "";
    const char *indent2 = config->pretty_print ? "  " : "";

    lv_strbuf_printf(&d, "<!DOCTYPE html>%s", indent);
    lv_strbuf_printf(&d, "<html lang=\"en\">%s", indent);
    lv_strbuf_printf(&d, "<head>%s", indent);
    lv_strbuf_printf(&d, "%s<meta charset=\"UTF-8\">%s", indent2, indent);
    lv_strbuf_printf(&d, "%s<title>", indent2);
    lv_sb_append_html_escaped(&d, proof->theorem);
    lv_strbuf_printf(&d, "</title>%s", indent);
    lv_strbuf_printf(&d, "%s<style>%s", indent2, indent);
    lv_strbuf_printf(&d, "%s  body { font-family: 'Segoe UI', sans-serif; margin: 2em; background: #f9f9f9; }%s", indent2,
                indent);
    lv_strbuf_printf(&d, "%s  h1 { color: #333; border-bottom: 2px solid #4a90d9; padding-bottom: 0.3em; }%s", indent2,
                indent);
    lv_strbuf_printf(&d,
                "%s  table { border-collapse: collapse; width: 100%%; background: #fff; box-shadow: 0 1px 3px "
                "rgba(0,0,0,0.1); }%s",
                indent2, indent);
    lv_strbuf_printf(&d, "%s  th { background: #4a90d9; color: #fff; padding: 10px; text-align: left; }%s", indent2, indent);
    lv_strbuf_printf(&d, "%s  td { padding: 8px 10px; border-bottom: 1px solid #e0e0e0; }%s", indent2, indent);
    lv_strbuf_printf(&d, "%s  tr:nth-child(even) { background: #f2f7fd; }%s", indent2, indent);
    lv_strbuf_printf(&d, "%s  .trace { margin-top: 2em; padding: 1em; background: #fff3cd; border: 1px solid #ffc107; }%s",
                indent2, indent);
    lv_strbuf_printf(&d, "%s</style>%s", indent2, indent);
    lv_strbuf_printf(&d, "</head>%s", indent);
    lv_strbuf_printf(&d, "<body>%s", indent);
    lv_strbuf_printf(&d, "%s<h1>", indent2);
    lv_sb_append_html_escaped(&d, proof->theorem);
    lv_strbuf_printf(&d, "</h1>%s", indent);
    lv_strbuf_printf(&d, "%s<table>%s", indent2, indent);
    lv_strbuf_printf(
        &d, "%s  <thead><tr><th>Step</th><th>Rule</th><th>Premise</th><th>Conclusion</th><th>Depth</th></tr></thead>%s",
        indent2, indent);
    lv_strbuf_printf(&d, "%s  <tbody>%s", indent2, indent);
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        lv_strbuf_printf(&d, "%s    <tr>", indent2);
        lv_strbuf_printf(&d, "<td>%d</td>", s->step_id);
        lv_strbuf_printf(&d, "<td>");
        lv_sb_append_html_escaped(&d, s->rule);
        lv_strbuf_printf(&d, "</td>");
        if (s->premise) {
            lv_strbuf_printf(&d, "<td>");
            lv_sb_append_html_escaped(&d, s->premise);
            lv_strbuf_printf(&d, "</td>");
        } else {
            lv_strbuf_printf(&d, "<td><em>none</em></td>");
        }
        lv_strbuf_printf(&d, "<td>");
        lv_sb_append_html_escaped(&d, s->conclusion);
        lv_strbuf_printf(&d, "</td>");
        lv_strbuf_printf(&d, "<td>%d</td>", s->depth);
        lv_strbuf_printf(&d, "</tr>%s", indent);
    }
    lv_strbuf_printf(&d, "%s  </tbody>%s", indent2, indent);
    lv_strbuf_printf(&d, "%s</table>%s", indent2, indent);

    if (config->include_proof_trace) {
        lv_strbuf_printf(&d, "%s<div class=\"trace\">%s", indent2, indent);
        lv_strbuf_printf(&d, "%s  <h3>Proof Trace</h3>%s", indent2, indent);
        lv_strbuf_printf(&d, "%s  <p>Total steps: %d</p>%s", indent2, proof->n_steps, indent);
        for (int i = 0; i < proof->n_steps; i++) {
            const lvProofStep *s = &proof->steps[i];
            lv_strbuf_printf(&d, "%s  <p>[%d] ", indent2, s->step_id);
            lv_sb_append_html_escaped(&d, s->rule);
            if (s->premise) {
                lv_strbuf_printf(&d, " (from ");
                lv_sb_append_html_escaped(&d, s->premise);
                lv_strbuf_printf(&d, ")");
            }
            lv_strbuf_printf(&d, " &rarr; ");
            lv_sb_append_html_escaped(&d, s->conclusion);
            lv_strbuf_printf(&d, "</p>%s", indent);
        }
        lv_strbuf_printf(&d, "%s</div>%s", indent2, indent);
    }

    lv_strbuf_printf(&d, "</body>%s", indent);
    lv_strbuf_printf(&d, "</html>%s", indent);

    return make_success(&d);
}

/* ================================================================
 * LaTeX 导出
 * ================================================================ */

static lvExportResult *export_latex(const lvProof *proof, const lvExportConfig *config) {
    lvStrBuf d;
    lv_strbuf_init(&d);

    const char *nl = config->pretty_print ? "\n" : "\n";

    lv_strbuf_printf(&d, "\\documentclass{article}%s", nl);
    lv_strbuf_printf(&d, "\\usepackage{amsthm}%s", nl);
    lv_strbuf_printf(&d, "\\usepackage{amsmath}%s", nl);
    lv_strbuf_printf(&d, "\\usepackage{amssymb}%s", nl);
    lv_strbuf_printf(&d, "%s\\begin{document}%s", nl, nl);

    /* 将定理名转义 LaTeX 特殊字符（\ → \textbackslash, { → \{, } → \}, _ → \_, & → \&, # → \#, % → \%, $ → \$, ^ → \^{}, ~ → \~{}） */
    lv_strbuf_printf(&d, "\\begin{proof}[");
    lv_sb_append_latex_escaped(&d, proof->theorem);
    lv_strbuf_printf(&d, "]%s", nl);

    lv_strbuf_printf(&d, "\\begin{tabular}{|c|c|c|c|}%s", nl);
    lv_strbuf_printf(&d, "\\hline%s", nl);
    lv_strbuf_printf(&d, "Step & Rule & Premise & Conclusion \\\\\\hline%s", nl);
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        lv_strbuf_printf(&d, "%d & ", s->step_id);
        lv_sb_append_latex_escaped(&d, s->rule);
        lv_strbuf_printf(&d, " & ");
        if (s->premise) {
            lv_sb_append_latex_escaped(&d, s->premise);
        } else {
            lv_strbuf_printf(&d, "---");
        }
        lv_strbuf_printf(&d, " & ");
        lv_sb_append_latex_escaped(&d, s->conclusion);
        lv_strbuf_printf(&d, " \\\\\\hline%s", nl);
    }
    lv_strbuf_printf(&d, "\\end{tabular}%s", nl);

    lv_strbuf_printf(&d, "\\end{proof}%s", nl);
    lv_strbuf_printf(&d, "\\end{document}%s", nl);

    return make_success(&d);
}

/* ================================================================
 * Coq 导出
 * ================================================================ */

/**
 * @brief 判断规则字符串是否为无参 Coq tactic 关键字
 * @param rule 规则字符串
 * @return true 表示无参 tactic（直接输出 `rule.`）
 */
static bool is_zero_arg_coq_tactic(const char *rule) {
    static const char *const kZeroArg[] = {
        "reflexivity", "assumption", "trivial", "auto", "congruence", "tauto",
        "contradiction", "exfalso", "simpl", "compute", "symmetry", "split",
        "constructor", "now", "intuition", "omega", "lia"
    };
    for (size_t i = 0; i < sizeof(kZeroArg) / sizeof(kZeroArg[0]); i++) {
        if (lv_str_eq(rule, kZeroArg[i]))
            return true;
    }
    return false;
}

/**
 * @brief 判断规则字符串是否为带单参的 Coq tactic 关键字
 * @param rule 规则字符串
 * @return true 表示单参 tactic（输出 `rule conclusion.`）
 */
static bool is_one_arg_coq_tactic(const char *rule) {
    static const char *const kOneArg[] = {
        "exact", "apply", "rewrite", "destruct", "induction", "exists", "unfold",
        "specialize", "pose", "injection", "discriminate", "subst", "clear", "rename"
    };
    for (size_t i = 0; i < sizeof(kOneArg) / sizeof(kOneArg[0]); i++) {
        if (lv_str_eq(rule, kOneArg[i]))
            return true;
    }
    return false;
}

/**
 * @brief 判断规则字符串是否以已知 Coq tactic 关键字开头（形如 "apply thm"）
 * @param rule 规则字符串
 * @return true 表示整行可直接作为 tactic 字符串直译
 */
static bool is_coq_tactic_prefix(const char *rule) {
    static const char *const kPrefixes[] = { "exact ", "apply ", "rewrite ", "pose ", "assert " };
    for (size_t i = 0; i < sizeof(kPrefixes) / sizeof(kPrefixes[0]); i++) {
        if (strncmp(rule, kPrefixes[i], strlen(kPrefixes[i])) == 0)
            return true;
    }
    return false;
}

static lvExportResult *export_coq(const lvProof *proof, const lvExportConfig *config) {
    lvStrBuf d;
    lv_strbuf_init(&d);

    /* tactic 行必须换行分隔；pretty_print 额外在步骤间插入空行 */
    const char *nl = "\n";
    const char *blank = config->pretty_print ? "\n" : "";

    char id[256];
    sanitize_id(id, sizeof(id), proof->theorem);

    /* 定理目标：取最后一个证明步骤的结论（无步骤时回退 True，避免恒真占位） */
    const char *goal = "True";
    if (proof->n_steps > 0 && proof->steps[proof->n_steps - 1].conclusion)
        goal = proof->steps[proof->n_steps - 1].conclusion;

    lv_strbuf_printf(&d, "(* Proof: %s *)%s", safe_str(proof->theorem), nl);
    if (config->include_geometry)
        lv_strbuf_printf(&d, "(* Geometry: %d steps, %d depth *)%s", proof->n_steps,
                         proof->n_steps > 0 ? proof->steps[proof->n_steps - 1].depth : 0, nl);
    lv_strbuf_printf(&d, "%s", nl);
    lv_strbuf_printf(&d, "Theorem %s : %s.%s", id, goal, nl);
    lv_strbuf_printf(&d, "Proof.%s", nl);
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        if (config->include_proof_trace) {
            lv_strbuf_printf(&d, "  (* Step %d: %s", s->step_id, safe_str(s->rule));
            if (s->premise)
                lv_strbuf_printf(&d, " (premise: %s)", s->premise);
            lv_strbuf_printf(&d, " → %s *)%s", safe_str(s->conclusion), nl);
        }
        /* 根据规则字符串翻译为真实 Coq tactic（exact/apply/assert/tactic 字符串） */
        const char *rule = safe_str(s->rule);
        if (lv_str_eq(rule, "assert") || lv_str_eq(rule, "have")) {
            lv_strbuf_printf(&d, "  assert (step_%d : %s).%s", s->step_id, safe_str(s->conclusion), nl);
            if (s->premise && s->premise[0])
                lv_strbuf_printf(&d, "  { exact %s. }%s", s->premise, nl);
            else
                lv_strbuf_printf(&d, "  { admit. }%s", nl);
        } else if (is_zero_arg_coq_tactic(rule)) {
            lv_strbuf_printf(&d, "  %s.%s", rule, nl);
        } else if (is_one_arg_coq_tactic(rule)) {
            lv_strbuf_printf(&d, "  %s %s.%s", rule, safe_str(s->conclusion), nl);
        } else if (is_coq_tactic_prefix(rule)) {
            /* 完整 tactic 字符串直译（如 "apply lv_func_3"） */
            lv_strbuf_printf(&d, "  %s.%s", rule, nl);
        } else {
            /* 默认：有前提则用前提 exact，否则断言待补 */
            if (s->premise && s->premise[0]) {
                lv_strbuf_printf(&d, "  exact %s.%s", s->premise, nl);
            } else {
                lv_strbuf_printf(&d, "  assert (step_%d : %s).%s", s->step_id, safe_str(s->conclusion), nl);
                lv_strbuf_printf(&d, "  { admit. }%s", nl);
            }
        }
        if (config->pretty_print)
            lv_strbuf_printf(&d, "%s", blank);
    }
    lv_strbuf_printf(&d, "Qed.%s", nl);

    return make_success(&d);
}

/* ================================================================
 * Lean 4 导出
 * ================================================================ */

static lvExportResult *export_lean4(const lvProof *proof, const lvExportConfig *config) {
    lvStrBuf d;
    lv_strbuf_init(&d);

    /* tactic 行必须换行分隔；pretty_print 额外在步骤间插入空行 */
    const char *nl = "\n";
    const char *blank = config->pretty_print ? "\n" : "";

    char id[256];
    sanitize_id(id, sizeof(id), proof->theorem);

    /* 定理目标：取最后一个证明步骤的结论（无步骤时回退 True，避免恒真占位） */
    const char *goal = "True";
    if (proof->n_steps > 0 && proof->steps[proof->n_steps - 1].conclusion)
        goal = proof->steps[proof->n_steps - 1].conclusion;

    lv_strbuf_printf(&d, "import Mathlib%s", nl);
    lv_strbuf_printf(&d, "%s", nl);
    lv_strbuf_printf(&d, "/- Proof: %s -/%s", safe_str(proof->theorem), nl);
    if (config->include_geometry)
        lv_strbuf_printf(&d, "/- Geometry: %d steps -/%s", proof->n_steps, nl);
    lv_strbuf_printf(&d, "theorem %s : %s := by%s", id, goal, nl);
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        if (config->include_proof_trace) {
            lv_strbuf_printf(&d, "  -- Step %d: %s", s->step_id, safe_str(s->rule));
            if (s->premise)
                lv_strbuf_printf(&d, " (from %s)", s->premise);
            lv_strbuf_printf(&d, " → %s%s", safe_str(s->conclusion), nl);
        }
        /* 每步翻译为 have 中间假设，证明体用 by/exact/apply/calc/tactic 字符串 */
        const char *rule = safe_str(s->rule);
        lv_strbuf_printf(&d, "  have h%d : %s := by%s", s->step_id, safe_str(s->conclusion), nl);
        if (lv_str_eq(rule, "exact")) {
            lv_strbuf_printf(&d, "    exact %s%s", safe_str(s->premise ? s->premise : s->conclusion), nl);
        } else if (lv_str_eq(rule, "apply")) {
            lv_strbuf_printf(&d, "    apply %s%s", safe_str(s->premise ? s->premise : s->conclusion), nl);
        } else if (lv_str_eq(rule, "rewrite")) {
            lv_strbuf_printf(&d, "    rw [%s]%s", safe_str(s->conclusion), nl);
        } else if (lv_str_eq(rule, "calc")) {
            lv_strbuf_printf(&d, "    calc%s", nl);
            lv_strbuf_printf(&d, "      %s := by trivial%s", safe_str(s->conclusion), nl);
        } else if (lv_str_eq(rule, "assumption") || lv_str_eq(rule, "trivial") ||
                   lv_str_eq(rule, "rfl")) {
            lv_strbuf_printf(&d, "    %s%s", rule, nl);
        } else if (s->premise && s->premise[0]) {
            /* 默认：前提即结论的证明 */
            lv_strbuf_printf(&d, "    exact %s%s", s->premise, nl);
        } else {
            lv_strbuf_printf(&d, "    trivial%s", nl);
        }
        if (config->pretty_print)
            lv_strbuf_printf(&d, "%s", blank);
    }
    if (proof->n_steps > 0) {
        const lvProofStep *last = &proof->steps[proof->n_steps - 1];
        lv_strbuf_printf(&d, "  exact h%d%s", last->step_id, nl);
    } else {
        lv_strbuf_printf(&d, "  trivial%s", nl);
    }

    return make_success(&d);
}

/* ================================================================
 * JSON 导出
 * ================================================================ */

static lvExportResult *export_json(const lvProof *proof, const lvExportConfig *config) {
    lvStrBuf d;
    lv_strbuf_init(&d);

    const char *nl = config->pretty_print ? "\n" : "";
    const char *sp = config->pretty_print ? "  " : "";
    const char *sp2 = config->pretty_print ? "    " : "";
    const char *sp3 = config->pretty_print ? "      " : "";

    lv_strbuf_printf(&d, "{%s", nl);
    lv_strbuf_printf(&d, "%s\"theorem\": ", sp);
    lv_sb_append_json_string(&d, proof->theorem);
    lv_strbuf_printf(&d, ",%s", nl);
    lv_strbuf_printf(&d, "%s\"n_steps\": %d,%s", sp, proof->n_steps, nl);
    lv_strbuf_printf(&d, "%s\"steps\": [%s", sp, nl);

    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        lv_strbuf_printf(&d, "%s{%s", sp2, nl);
        lv_strbuf_printf(&d, "%s\"id\": %d,%s", sp3, s->step_id, nl);
        lv_strbuf_printf(&d, "%s\"rule\": ", sp3);
        lv_sb_append_json_string(&d, s->rule);
        lv_strbuf_printf(&d, ",%s", nl);
        lv_strbuf_printf(&d, "%s\"premise\": ", sp3);
        if (s->premise) {
            lv_sb_append_json_string(&d, s->premise);
        } else {
            lv_strbuf_printf(&d, "null");
        }
        lv_strbuf_printf(&d, ",%s", nl);
        lv_strbuf_printf(&d, "%s\"conclusion\": ", sp3);
        lv_sb_append_json_string(&d, s->conclusion);
        lv_strbuf_printf(&d, ",%s", nl);
        lv_strbuf_printf(&d, "%s\"depth\": %d%s", sp3, s->depth, nl);
        lv_strbuf_printf(&d, "%s}%s%s", sp2, (i < proof->n_steps - 1) ? "," : "", nl);
    }

    lv_strbuf_printf(&d, "%s]%s", sp, nl);
    lv_strbuf_printf(&d, "}%s", nl);

    return make_success(&d);
}

/* ================================================================
 * DOT（Graphviz）导出
 * ================================================================ */

static lvExportResult *export_dot(const lvProof *proof, const lvExportConfig *config) {
    (void) config; /* DOT 输出统一使用 lv_dot_writer 固定多行格式（不再区分 pretty_print 紧凑模式） */
    lvStrBuf sb;
    lv_strbuf_init(&sb);

    lv_dot_begin(&sb, "Proof", "TB", "shape=box, style=rounded", NULL);

    /* 图级 label：定理名（经 JSON/DOT 转义，原实现未转义） */
    const char *theorem = safe_str(proof->theorem);
    size_t t_len = strlen(theorem);
    char *t_esc = lv_str_json_escape_alloc(theorem, t_len, NULL);
    if (t_esc) {
        lv_strbuf_printf(&sb, "    label=\"%s\";\n", t_esc);
        lv_free((void **) &t_esc);
    }

    /* 节点 */
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        lvStrBuf lbl;
        lv_strbuf_init(&lbl);
        lv_strbuf_printf(&lbl, "[%d] %s\n%s", s->step_id, safe_str(s->rule), safe_str(s->conclusion));
        if (s->premise) {
            lv_strbuf_printf(&lbl, "\n(from: %s)", s->premise);
        }
        lv_dot_node_id(&sb, "step", s->step_id, lv_strbuf_cstr(&lbl), NULL);
        lv_strbuf_destroy(&lbl);
    }

    /* 边：如果步骤 i 的 premise 等于步骤 j 的 conclusion，则添加 j → i */
    lv_strbuf_printf(&sb, "\n");
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *si = &proof->steps[i];
        if (!si->premise)
            continue;
        for (int j = 0; j < proof->n_steps; j++) {
            if (i == j)
                continue;
            const lvProofStep *sj = &proof->steps[j];
            if (lv_str_eq(si->premise, sj->conclusion)) {
                lv_dot_edge_id(&sb, "step", sj->step_id, si->step_id, NULL, NULL);
            }
        }
    }

    lv_dot_end(&sb);

    /* lvStrBuf 直接移交（消除中间 lvDStr 包装） */
    return make_success(&sb);
}

/* ================================================================
 * 公共 API
 * ================================================================ */

/** 导出处理函数指针类型 */
typedef lvExportResult *(*ExportHandlerFn)(const lvProof *proof, const lvExportConfig *config);

/** 导出格式到处理函数的查找表 */
static const ExportHandlerFn kExportHandlers[] = {
    [EXPORT_HTML] = export_html,
    [EXPORT_LATEX] = export_latex,
    [EXPORT_COQ] = export_coq,
    [EXPORT_LEAN4] = export_lean4,
    [EXPORT_JSON] = export_json,
    [EXPORT_DOT] = export_dot,
};

lvExportResult *proof_export_enhanced(const lvProof *proof, const lvExportConfig *config) {
    if (!proof) {
        return make_error("proof_export_enhanced: proof is NULL");
    }
    if (!config) {
        return make_error("proof_export_enhanced: config is NULL");
    }

    return LV_DISPATCH(kExportHandlers, config->format, make_error("proof_export_enhanced: unknown format"), proof, config);
}

lvExportResult *proof_export_from_navigator(const char *theorem_name, lvExportFormat format) {
    if (!theorem_name) {
        return make_error("proof_export_from_navigator: theorem_name is NULL");
    }

    lvProofStep step;
    step.step_id = 1;
    step.rule = "given";
    step.premise = NULL;
    step.conclusion = theorem_name;
    step.depth = 0;

    lvProof proof;
    proof.steps = &step;
    proof.n_steps = 1;
    proof.theorem = theorem_name;

    lvExportConfig config;
    config.format = format;
    config.include_proof_trace = false;
    config.include_geometry = false;
    config.pretty_print = true;

    return proof_export_enhanced(&proof, &config);
}

void proof_export_result_destroy(lvExportResult *result) {
    if (!result)
        return;
    if (result->output) {
        lv_free((void **) &(result->output));
        result->output = NULL;
    }
    lv_free((void **) &(result));
}
