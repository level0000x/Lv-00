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
 * @brief 创建成功结果对象（接管 lvDStr 内部缓冲区所有权）
 * @param d 已完成写入的字符串构建器
 * @return 成功结果对象指针，失败返回 NULL
 *
 * @note 所有权语义（与 lv_dstr_free 的参数注释"释放为调用者职责"
 *       区分开，避免误读为"返回后仍需对 d 调用 lv_dstr_free"）：
 *       - 返回后，d->data 的所有权已转移给 r->output，由调用方持有；
 *       - 调用方使用完毕后应通过 proof_export_result_destroy() 释放
 *         （内部对 r->output 执行 lv_free）；
 *       - 切勿再对原始 d 调用 lv_dstr_free()，否则会对同一指针
 *         双重释放（双 free）。
 */
static lvExportResult *make_success(lvDStr *d) {
    lvExportResult *r = (lvExportResult *) lv_calloc(1, sizeof(lvExportResult));
    if (!r) {
        lv_dstr_free(d);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "make_success: lv_calloc failed");
    }
    r->success = true;
    r->output = d->data;
    r->output_size = d->len;
    /* d->data ownership transferred — don't free it again */
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
        strncpy(dst, "theorem", dst_sz);
    }
}

/**
 * @brief JSON 字符串转义：在 str 前后加引号，经 lv_str_json_escape 两遍法完整转义
 *        （覆盖 \\、\"、\n、\r、\t、\b、\f 及其它控制字符）
 * @param d   字符串构建器指针
 * @param str 要转义的字符串（可为 NULL，输出 "null"）
 * @return 0 成功，-1 失败
 */
static int lv_dstr_append_json_string(lvDStr *d, const char *str) {
    if (!str) {
        return lv_dstr_append_fmt(d, "null");
    }
    size_t len = strlen(str);
    size_t need;
    char *buf = lv_str_json_escape_alloc(str, len, &need);
    if (!buf)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_dstr_append_json_string: escape buffer alloc failed");
    int rc = lv_dstr_append_fmt(d, "\"");
    if (rc == 0)
        rc = lv_dstr_append_raw(d, buf, need);
    if (rc == 0)
        rc = lv_dstr_append_fmt(d, "\"");
    lv_free((void **) &buf);
    if (rc != 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_dstr_append_json_string: append failed");
    return 0;
}

/**
 * @brief 将字符串 HTML 实体转义后写入 lvDStr（lv_str_html_escape 两遍法）
 * @param d   字符串构建器指针
 * @param str 要转义的字符串（可为 NULL，空操作）
 * @return 0 成功，-1 失败
 */
static int lv_dstr_append_html_escaped(lvDStr *d, const char *str) {
    if (!str)
        return 0;
    size_t len = strlen(str);
    size_t need = lv_str_html_escape(str, len, NULL, 0);
    char *buf = (char *) lv_malloc(need + 1);
    if (!buf)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_dstr_append_html_escaped: escape buffer alloc failed");
    lv_str_html_escape(str, len, buf, need + 1);
    int rc = lv_dstr_append_raw(d, buf, need);
    lv_free((void **) &buf);
    if (rc != 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_dstr_append_html_escaped: append failed");
    return 0;
}

/**
 * @brief 将字符串 LaTeX 特殊字符转义后写入 lvDStr（lv_str_latex_escape 两遍法）
 * @param d   字符串构建器指针
 * @param str 要转义的字符串（可为 NULL，空操作）
 * @return 0 成功，-1 失败
 */
static int lv_dstr_append_latex_escaped(lvDStr *d, const char *str) {
    if (!str)
        return 0;
    size_t len = strlen(str);
    size_t need = lv_str_latex_escape(str, len, NULL, 0);
    char *buf = (char *) lv_malloc(need + 1);
    if (!buf)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_dstr_append_latex_escaped: escape buffer alloc failed");
    lv_str_latex_escape(str, len, buf, need + 1);
    int rc = lv_dstr_append_raw(d, buf, need);
    lv_free((void **) &buf);
    if (rc != 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_dstr_append_latex_escaped: append failed");
    return 0;
}

/* ================================================================
 * HTML 导出
 * ================================================================ */

static lvExportResult *export_html(const lvProof *proof, const lvExportConfig *config) {
    lvDStr d;
    if (lv_dstr_init(&d, lv_DSTR_INIT_CAP) != 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "export_html: lv_dstr_init failed");

    const char *indent = config->pretty_print ? "\n" : "";
    const char *indent2 = config->pretty_print ? "  " : "";

    lv_dstr_append_fmt(&d, "<!DOCTYPE html>%s", indent);
    lv_dstr_append_fmt(&d, "<html lang=\"en\">%s", indent);
    lv_dstr_append_fmt(&d, "<head>%s", indent);
    lv_dstr_append_fmt(&d, "%s<meta charset=\"UTF-8\">%s", indent2, indent);
    lv_dstr_append_fmt(&d, "%s<title>", indent2);
    lv_dstr_append_html_escaped(&d, proof->theorem);
    lv_dstr_append_fmt(&d, "</title>%s", indent);
    lv_dstr_append_fmt(&d, "%s<style>%s", indent2, indent);
    lv_dstr_append_fmt(&d, "%s  body { font-family: 'Segoe UI', sans-serif; margin: 2em; background: #f9f9f9; }%s", indent2,
                indent);
    lv_dstr_append_fmt(&d, "%s  h1 { color: #333; border-bottom: 2px solid #4a90d9; padding-bottom: 0.3em; }%s", indent2,
                indent);
    lv_dstr_append_fmt(&d,
                "%s  table { border-collapse: collapse; width: 100%%; background: #fff; box-shadow: 0 1px 3px "
                "rgba(0,0,0,0.1); }%s",
                indent2, indent);
    lv_dstr_append_fmt(&d, "%s  th { background: #4a90d9; color: #fff; padding: 10px; text-align: left; }%s", indent2, indent);
    lv_dstr_append_fmt(&d, "%s  td { padding: 8px 10px; border-bottom: 1px solid #e0e0e0; }%s", indent2, indent);
    lv_dstr_append_fmt(&d, "%s  tr:nth-child(even) { background: #f2f7fd; }%s", indent2, indent);
    lv_dstr_append_fmt(&d, "%s  .trace { margin-top: 2em; padding: 1em; background: #fff3cd; border: 1px solid #ffc107; }%s",
                indent2, indent);
    lv_dstr_append_fmt(&d, "%s</style>%s", indent2, indent);
    lv_dstr_append_fmt(&d, "</head>%s", indent);
    lv_dstr_append_fmt(&d, "<body>%s", indent);
    lv_dstr_append_fmt(&d, "%s<h1>", indent2);
    lv_dstr_append_html_escaped(&d, proof->theorem);
    lv_dstr_append_fmt(&d, "</h1>%s", indent);
    lv_dstr_append_fmt(&d, "%s<table>%s", indent2, indent);
    lv_dstr_append_fmt(
        &d, "%s  <thead><tr><th>Step</th><th>Rule</th><th>Premise</th><th>Conclusion</th><th>Depth</th></tr></thead>%s",
        indent2, indent);
    lv_dstr_append_fmt(&d, "%s  <tbody>%s", indent2, indent);
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        lv_dstr_append_fmt(&d, "%s    <tr>", indent2);
        lv_dstr_append_fmt(&d, "<td>%d</td>", s->step_id);
        lv_dstr_append_fmt(&d, "<td>");
        lv_dstr_append_html_escaped(&d, s->rule);
        lv_dstr_append_fmt(&d, "</td>");
        if (s->premise) {
            lv_dstr_append_fmt(&d, "<td>");
            lv_dstr_append_html_escaped(&d, s->premise);
            lv_dstr_append_fmt(&d, "</td>");
        } else {
            lv_dstr_append_fmt(&d, "<td><em>none</em></td>");
        }
        lv_dstr_append_fmt(&d, "<td>");
        lv_dstr_append_html_escaped(&d, s->conclusion);
        lv_dstr_append_fmt(&d, "</td>");
        lv_dstr_append_fmt(&d, "<td>%d</td>", s->depth);
        lv_dstr_append_fmt(&d, "</tr>%s", indent);
    }
    lv_dstr_append_fmt(&d, "%s  </tbody>%s", indent2, indent);
    lv_dstr_append_fmt(&d, "%s</table>%s", indent2, indent);

    if (config->include_proof_trace) {
        lv_dstr_append_fmt(&d, "%s<div class=\"trace\">%s", indent2, indent);
        lv_dstr_append_fmt(&d, "%s  <h3>Proof Trace</h3>%s", indent2, indent);
        lv_dstr_append_fmt(&d, "%s  <p>Total steps: %d</p>%s", indent2, proof->n_steps, indent);
        for (int i = 0; i < proof->n_steps; i++) {
            const lvProofStep *s = &proof->steps[i];
            lv_dstr_append_fmt(&d, "%s  <p>[%d] ", indent2, s->step_id);
            lv_dstr_append_html_escaped(&d, s->rule);
            if (s->premise) {
                lv_dstr_append_fmt(&d, " (from ");
                lv_dstr_append_html_escaped(&d, s->premise);
                lv_dstr_append_fmt(&d, ")");
            }
            lv_dstr_append_fmt(&d, " &rarr; ");
            lv_dstr_append_html_escaped(&d, s->conclusion);
            lv_dstr_append_fmt(&d, "</p>%s", indent);
        }
        lv_dstr_append_fmt(&d, "%s</div>%s", indent2, indent);
    }

    lv_dstr_append_fmt(&d, "</body>%s", indent);
    lv_dstr_append_fmt(&d, "</html>%s", indent);

    return make_success(&d);
}

/* ================================================================
 * LaTeX 导出
 * ================================================================ */

static lvExportResult *export_latex(const lvProof *proof, const lvExportConfig *config) {
    lvDStr d;
    if (lv_dstr_init(&d, lv_DSTR_INIT_CAP) != 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "export_latex: lv_dstr_init failed");

    const char *nl = config->pretty_print ? "\n" : "\n";

    lv_dstr_append_fmt(&d, "\\documentclass{article}%s", nl);
    lv_dstr_append_fmt(&d, "\\usepackage{amsthm}%s", nl);
    lv_dstr_append_fmt(&d, "\\usepackage{amsmath}%s", nl);
    lv_dstr_append_fmt(&d, "\\usepackage{amssymb}%s", nl);
    lv_dstr_append_fmt(&d, "%s\\begin{document}%s", nl, nl);

    /* 将定理名转义 LaTeX 特殊字符（\ → \textbackslash, { → \{, } → \}, _ → \_, & → \&, # → \#, % → \%, $ → \$, ^ → \^{}, ~ → \~{}） */
    lv_dstr_append_fmt(&d, "\\begin{proof}[");
    lv_dstr_append_latex_escaped(&d, proof->theorem);
    lv_dstr_append_fmt(&d, "]%s", nl);

    lv_dstr_append_fmt(&d, "\\begin{tabular}{|c|c|c|c|}%s", nl);
    lv_dstr_append_fmt(&d, "\\hline%s", nl);
    lv_dstr_append_fmt(&d, "Step & Rule & Premise & Conclusion \\\\\\hline%s", nl);
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        lv_dstr_append_fmt(&d, "%d & ", s->step_id);
        lv_dstr_append_latex_escaped(&d, s->rule);
        lv_dstr_append_fmt(&d, " & ");
        if (s->premise) {
            lv_dstr_append_latex_escaped(&d, s->premise);
        } else {
            lv_dstr_append_fmt(&d, "---");
        }
        lv_dstr_append_fmt(&d, " & ");
        lv_dstr_append_latex_escaped(&d, s->conclusion);
        lv_dstr_append_fmt(&d, " \\\\\\hline%s", nl);
    }
    lv_dstr_append_fmt(&d, "\\end{tabular}%s", nl);

    lv_dstr_append_fmt(&d, "\\end{proof}%s", nl);
    lv_dstr_append_fmt(&d, "\\end{document}%s", nl);

    return make_success(&d);
}

/* ================================================================
 * Coq 导出
 * ================================================================ */

static lvExportResult *export_coq(const lvProof *proof, const lvExportConfig *config) {
    lvDStr d;
    if (lv_dstr_init(&d, lv_DSTR_INIT_CAP) != 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "export_coq: lv_dstr_init failed");

    (void) config;

    char id[256];
    sanitize_id(id, sizeof(id), proof->theorem);

    lv_dstr_append_fmt(&d, "(* Proof: %s *)%s", safe_str(proof->theorem), "\n");
    lv_dstr_append_fmt(&d, "%s", "\n");
    lv_dstr_append_fmt(&d, "Theorem %s : True.%s", id, "\n");
    lv_dstr_append_fmt(&d, "Proof.%s", "\n");
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        lv_dstr_append_fmt(&d, "  (* Step %d: %s", s->step_id, safe_str(s->rule));
        if (s->premise)
            lv_dstr_append_fmt(&d, " (premise: %s)", s->premise);
        lv_dstr_append_fmt(&d, " → %s *)%s", safe_str(s->conclusion), "\n");
        lv_dstr_append_fmt(&d, "  %s. (* %s *)%s", "exact I", safe_str(s->rule), "\n");
    }
    lv_dstr_append_fmt(&d, "Qed.%s", "\n");

    return make_success(&d);
}

/* ================================================================
 * Lean 4 导出
 * ================================================================ */

static lvExportResult *export_lean4(const lvProof *proof, const lvExportConfig *config) {
    lvDStr d;
    if (lv_dstr_init(&d, lv_DSTR_INIT_CAP) != 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "export_lean4: lv_dstr_init failed");

    (void) config;

    char id[256];
    sanitize_id(id, sizeof(id), proof->theorem);

    lv_dstr_append_fmt(&d, "import Mathlib%s", "\n");
    lv_dstr_append_fmt(&d, "%s", "\n");
    lv_dstr_append_fmt(&d, "/- Proof: %s -/%s", safe_str(proof->theorem), "\n");
    lv_dstr_append_fmt(&d, "theorem %s : True := by%s", id, "\n");
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        lv_dstr_append_fmt(&d, "  -- Step %d: %s", s->step_id, safe_str(s->rule));
        if (s->premise)
            lv_dstr_append_fmt(&d, " (from %s)", s->premise);
        lv_dstr_append_fmt(&d, " → %s%s", safe_str(s->conclusion), "\n");
        lv_dstr_append_fmt(&d, "  have h%d : True := by trivial%s", s->step_id, "\n");
    }
    lv_dstr_append_fmt(&d, "  exact h%d%s", proof->steps[proof->n_steps - 1].step_id, "\n");

    return make_success(&d);
}

/* ================================================================
 * JSON 导出
 * ================================================================ */

static lvExportResult *export_json(const lvProof *proof, const lvExportConfig *config) {
    lvDStr d;
    if (lv_dstr_init(&d, lv_DSTR_INIT_CAP) != 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "export_json: lv_dstr_init failed");

    const char *nl = config->pretty_print ? "\n" : "";
    const char *sp = config->pretty_print ? "  " : "";
    const char *sp2 = config->pretty_print ? "    " : "";
    const char *sp3 = config->pretty_print ? "      " : "";

    lv_dstr_append_fmt(&d, "{%s", nl);
    lv_dstr_append_fmt(&d, "%s\"theorem\": ", sp);
    lv_dstr_append_json_string(&d, proof->theorem);
    lv_dstr_append_fmt(&d, ",%s", nl);
    lv_dstr_append_fmt(&d, "%s\"n_steps\": %d,%s", sp, proof->n_steps, nl);
    lv_dstr_append_fmt(&d, "%s\"steps\": [%s", sp, nl);

    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        lv_dstr_append_fmt(&d, "%s{%s", sp2, nl);
        lv_dstr_append_fmt(&d, "%s\"id\": %d,%s", sp3, s->step_id, nl);
        lv_dstr_append_fmt(&d, "%s\"rule\": ", sp3);
        lv_dstr_append_json_string(&d, s->rule);
        lv_dstr_append_fmt(&d, ",%s", nl);
        lv_dstr_append_fmt(&d, "%s\"premise\": ", sp3);
        if (s->premise) {
            lv_dstr_append_json_string(&d, s->premise);
        } else {
            lv_dstr_append_fmt(&d, "null");
        }
        lv_dstr_append_fmt(&d, ",%s", nl);
        lv_dstr_append_fmt(&d, "%s\"conclusion\": ", sp3);
        lv_dstr_append_json_string(&d, s->conclusion);
        lv_dstr_append_fmt(&d, ",%s", nl);
        lv_dstr_append_fmt(&d, "%s\"depth\": %d%s", sp3, s->depth, nl);
        lv_dstr_append_fmt(&d, "%s}%s%s", sp2, (i < proof->n_steps - 1) ? "," : "", nl);
    }

    lv_dstr_append_fmt(&d, "%s]%s", sp, nl);
    lv_dstr_append_fmt(&d, "}%s", nl);

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
        char idbuf[32];
        snprintf(idbuf, sizeof(idbuf), "step%d", s->step_id);
        lv_dot_node(&sb, idbuf, lv_strbuf_cstr(&lbl), NULL);
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
            if (strcmp(si->premise, sj->conclusion) == 0) {
                char frombuf[32], tobuf[32];
                snprintf(frombuf, sizeof(frombuf), "step%d", sj->step_id);
                snprintf(tobuf, sizeof(tobuf), "step%d", si->step_id);
                lv_dot_edge(&sb, frombuf, tobuf, NULL, NULL);
            }
        }
    }

    lv_dot_end(&sb);

    lvDStr d;
    if (lv_dstr_init(&d, lv_DSTR_INIT_CAP) != 0) {
        lv_strbuf_destroy(&sb);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "export_dot: lv_dstr_init failed");
    }
    lv_dstr_append_raw(&d, sb.data, sb.len);
    lv_strbuf_destroy(&sb);

    return make_success(&d);
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

    if ((unsigned)config->format < sizeof(kExportHandlers)/sizeof(kExportHandlers[0]) && kExportHandlers[config->format]) {
        return kExportHandlers[config->format](proof, config);
    }
    return make_error("proof_export_enhanced: unknown format");
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
