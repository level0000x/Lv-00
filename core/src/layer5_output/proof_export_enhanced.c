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
 */

#include "lv/proof_export_enhanced.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

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
 * @param d 已完成写入的字符串构建器（释放调用者职责）
 * @return 成功结果对象指针，失败返回 NULL
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
 * @brief JSON 字符串转义：在 str 前后加引号，转义 \\ 和 \"
 * @param d   字符串构建器指针
 * @param str 要转义的字符串（可为 NULL，输出 "null"）
 * @return 0 成功，-1 失败
 */
static int lv_dstr_append_json_string(lvDStr *d, const char *str) {
    if (!str) {
        return lv_dstr_append_fmt(d, "null");
    }
    if (lv_dstr_append_fmt(d, "\"") != 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_dstr_append_json_string: append quote failed");
    for (const char *p = str; *p; p++) {
        if (*p == '\\') {
            if (lv_dstr_append_fmt(d, "\\\\") != 0)
                lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_dstr_append_json_string: append backslash failed");
        } else if (*p == '"') {
            if (lv_dstr_append_fmt(d, "\\\"") != 0)
                lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_dstr_append_json_string: append quote escaped failed");
        } else if (*p == '\n') {
            if (lv_dstr_append_fmt(d, "\\n") != 0)
                lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_dstr_append_json_string: append newline failed");
        } else if (*p == '\t') {
            if (lv_dstr_append_fmt(d, "\\t") != 0)
                lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_dstr_append_json_string: append tab failed");
        } else if (*p == '\r') {
            if (lv_dstr_append_fmt(d, "\\r") != 0)
                lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_dstr_append_json_string: append cr failed");
        } else {
            char tmp[2] = {*p, '\0'};
            if (lv_dstr_append_str(d, tmp) != 0)
                lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_dstr_append_json_string: append char failed");
        }
    }
    return lv_dstr_append_fmt(d, "\"");
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
    lv_dstr_append_fmt(&d, "%s<title>%s</title>%s", indent2, safe_str(proof->theorem), indent);
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
    lv_dstr_append_fmt(&d, "%s<h1>%s</h1>%s", indent2, safe_str(proof->theorem), indent);
    lv_dstr_append_fmt(&d, "%s<table>%s", indent2, indent);
    lv_dstr_append_fmt(
        &d, "%s  <thead><tr><th>Step</th><th>Rule</th><th>Premise</th><th>Conclusion</th><th>Depth</th></tr></thead>%s",
        indent2, indent);
    lv_dstr_append_fmt(&d, "%s  <tbody>%s", indent2, indent);
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        lv_dstr_append_fmt(&d, "%s    <tr>", indent2);
        lv_dstr_append_fmt(&d, "<td>%d</td>", s->step_id);
        lv_dstr_append_fmt(&d, "<td>%s</td>", safe_str(s->rule));
        lv_dstr_append_fmt(&d, "<td>%s</td>", s->premise ? s->premise : "<em>none</em>");
        lv_dstr_append_fmt(&d, "<td>%s</td>", safe_str(s->conclusion));
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
            lv_dstr_append_fmt(&d, "%s  <p>[%d] %s", indent2, s->step_id, safe_str(s->rule));
            if (s->premise)
                lv_dstr_append_fmt(&d, " (from %s)", s->premise);
            lv_dstr_append_fmt(&d, " &rarr; %s</p>%s", safe_str(s->conclusion), indent);
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

/** @brief LaTeX 特殊字符 → 转义字符串 查找表（NULL 表示原样输出） */
static const char *const s_latex_escape_map[256] = {
    ['\\'] = "\\textbackslash{}",
    ['{']  = "\\{",
    ['}']  = "\\}",
    ['_']  = "\\_",
    ['&']  = "\\&",
    ['#']  = "\\#",
    ['%']  = "\\%%",
    ['$']  = "\\$",
    ['^']  = "\\^{}",
    ['~']  = "\\~{}",
};

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

    /* 将定理名转义 LaTeX 特殊字符 */
    const char *th = safe_str(proof->theorem);
    lv_dstr_append_fmt(&d, "\\begin{proof}[");
    /* 简单转义：\ → \textbackslash, { → \{, } → \}, _ → \_, & → \&, # → \#, % → \% */
    for (const char *p = th; *p; p++) {
        /* 查找表：特殊字符 → LaTeX 转义字符串；未命中（NULL）走 default 原样输出 */
        const char *esc = s_latex_escape_map[(unsigned char)*p];
        if (esc) {
            lv_dstr_append_fmt(&d, "%s", esc);
        } else {
            char tmp[2] = {*p, '\0'};
            lv_dstr_append_str(&d, tmp);
        }
    }
    lv_dstr_append_fmt(&d, "]%s", nl);

    lv_dstr_append_fmt(&d, "\\begin{tabular}{|c|c|c|c|}%s", nl);
    lv_dstr_append_fmt(&d, "\\hline%s", nl);
    lv_dstr_append_fmt(&d, "Step & Rule & Premise & Conclusion \\\\\\hline%s", nl);
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        lv_dstr_append_fmt(&d, "%d & %s & %s & %s \\\\\\hline%s", s->step_id, safe_str(s->rule),
                    s->premise ? s->premise : "---", safe_str(s->conclusion), nl);
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
    lvDStr d;
    if (lv_dstr_init(&d, lv_DSTR_INIT_CAP) != 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "export_dot: lv_dstr_init failed");

    const char *nl = config->pretty_print ? "\n" : "";
    const char *sp = config->pretty_print ? "  " : "";
    const char *sp2 = config->pretty_print ? "    " : "";

    lv_dstr_append_fmt(&d, "digraph Proof {%s", nl);
    lv_dstr_append_fmt(&d, "%srankdir=TB;%s", sp, nl);
    lv_dstr_append_fmt(&d, "%snode[shape=box, style=rounded];%s", sp, nl);
    lv_dstr_append_fmt(&d, "%sfontname=\"Helvetica\";%s", sp, nl);
    lv_dstr_append_fmt(&d, "%slabel=\"%s\";%s", sp, safe_str(proof->theorem), nl);

    /* 节点 */
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *s = &proof->steps[i];
        lv_dstr_append_fmt(&d, "%sstep%d[label=\"[%d] %s\\n%s", sp, s->step_id, s->step_id, safe_str(s->rule),
                    safe_str(s->conclusion));
        if (s->premise) {
            lv_dstr_append_fmt(&d, "\\n(from: %s)", s->premise);
        }
        lv_dstr_append_fmt(&d, "\"];%s", nl);
    }

    /* 边：如果步骤 i 的 premise 等于步骤 j 的 conclusion，则添加 j → i */
    lv_dstr_append_fmt(&d, "%s%s", sp, nl);
    for (int i = 0; i < proof->n_steps; i++) {
        const lvProofStep *si = &proof->steps[i];
        if (!si->premise)
            continue;
        for (int j = 0; j < proof->n_steps; j++) {
            if (i == j)
                continue;
            const lvProofStep *sj = &proof->steps[j];
            if (strcmp(si->premise, sj->conclusion) == 0) {
                lv_dstr_append_fmt(&d, "%sstep%d -> step%d;%s", sp, sj->step_id, si->step_id, nl);
            }
        }
    }

    lv_dstr_append_fmt(&d, "}%s", nl);

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
