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

#include "lv00/proof_export_enhanced.h"
#include "lv00/lv00_utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * 动态字符串构建器
 * ================================================================ */

#define DSTR_INIT_CAP 4096

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} DStr;

static int dstr_init(DStr *d, size_t cap) {
    d->data = (char *)lv00_malloc(cap);
    if (!d->data) return -1;
    d->data[0] = '\0';
    d->len = 0;
    d->cap = cap;
    return 0;
}

static int dstr_grow(DStr *d, size_t extra) {
    size_t needed = d->len + extra + 1;
    if (needed <= d->cap) return 0;
    size_t new_cap = d->cap * 2;
    while (new_cap < needed) new_cap *= 2;
    char *nd = (char *)lv00_realloc(d->data, new_cap);
    if (!nd) return -1;
    d->data = nd;
    d->cap  = new_cap;
    return 0;
}

static int dstr_append(DStr *d, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) return -1;
    if (dstr_grow(d, (size_t)needed) != 0) return -1;
    va_start(ap, fmt);
    vsnprintf(d->data + d->len, d->cap - d->len, fmt, ap);
    va_end(ap);
    d->len += (size_t)needed;
    return 0;
}

static int dstr_append_raw(DStr *d, const char *s, size_t n) {
    if (!s || n == 0) return 0;
    if (dstr_grow(d, n) != 0) return -1;
    memcpy(d->data + d->len, s, n);
    d->len += n;
    d->data[d->len] = '\0';
    return 0;
}

static int dstr_append_str(DStr *d, const char *s) {
    if (!s) return 0;
    return dstr_append_raw(d, s, strlen(s));
}

static void dstr_free(DStr *d) {
    if (d->data) {
        lv00_free((void **)&d->data);
        d->data = NULL;
    }
    d->len = 0;
    d->cap = 0;
}

/* ================================================================
 * 错误结果工厂
 * ================================================================ */

static Lv00ExportResult *make_error(const char *msg) {
    Lv00ExportResult *r = (Lv00ExportResult *)lv00_malloc(sizeof(Lv00ExportResult));
    if (!r) return NULL;
    const char *src = msg ? msg : "Unknown error";
    size_t len = strlen(src);
    r->success     = false;
    r->output      = (char *)lv00_malloc(len + 1);
    if (r->output) {
        memcpy(r->output, src, len + 1);
        r->output_size = len;
    } else {
        r->output_size = 0;
    }
    return r;
}

static Lv00ExportResult *make_success(DStr *d) {
    Lv00ExportResult *r = (Lv00ExportResult *)lv00_malloc(sizeof(Lv00ExportResult));
    if (!r) {
        dstr_free(d);
        return NULL;
    }
    r->success     = true;
    r->output      = d->data;
    r->output_size = d->len;
    /* d->data ownership transferred — don't free it again */
    return r;
}

/* ================================================================
 * 工具函数
 * ================================================================ */

/** 安全字符串（NULL → ""） */
static const char *safe_str(const char *s) {
    return s ? s : "";
}

/** 将定理名转换为安全的标识符（替换特殊字符） */
static void sanitize_id(char *dst, size_t dst_sz, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < dst_sz; i++) {
        char c = src[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_') {
            dst[j++] = c;
        } else if (c == ' ' || c == ':' || c == '-' || c == '>' || c == '(' ||
                   c == ')' || c == ',') {
            dst[j++] = '_';
        }
        /* skip other chars */
    }
    dst[j] = '\0';
    if (j == 0) {
        strncpy(dst, "theorem", dst_sz);
    }
}

/** JSON 字符串转义：在 str 前后加引号，转义 \ 和 " */
static int dstr_append_json_string(DStr *d, const char *str) {
    if (!str) {
        return dstr_append(d, "null");
    }
    if (dstr_append(d, "\"") != 0) return -1;
    for (const char *p = str; *p; p++) {
        if (*p == '\\') {
            if (dstr_append(d, "\\\\") != 0) return -1;
        } else if (*p == '"') {
            if (dstr_append(d, "\\\"") != 0) return -1;
        } else if (*p == '\n') {
            if (dstr_append(d, "\\n") != 0) return -1;
        } else if (*p == '\t') {
            if (dstr_append(d, "\\t") != 0) return -1;
        } else if (*p == '\r') {
            if (dstr_append(d, "\\r") != 0) return -1;
        } else {
            char tmp[2] = {*p, '\0'};
            if (dstr_append_str(d, tmp) != 0) return -1;
        }
    }
    return dstr_append(d, "\"");
}

/* ================================================================
 * HTML 导出
 * ================================================================ */

static Lv00ExportResult *export_html(const Lv00Proof *proof,
                                     const Lv00ExportConfig *config) {
    DStr d;
    if (dstr_init(&d, DSTR_INIT_CAP) != 0) return NULL;

    const char *indent = config->pretty_print ? "\n" : "";
    const char *indent2 = config->pretty_print ? "  " : "";

    dstr_append(&d, "<!DOCTYPE html>%s", indent);
    dstr_append(&d, "<html lang=\"en\">%s", indent);
    dstr_append(&d, "<head>%s", indent);
    dstr_append(&d, "%s<meta charset=\"UTF-8\">%s", indent2, indent);
    dstr_append(&d, "%s<title>%s</title>%s", indent2, safe_str(proof->theorem), indent);
    dstr_append(&d, "%s<style>%s", indent2, indent);
    dstr_append(&d, "%s  body { font-family: 'Segoe UI', sans-serif; margin: 2em; background: #f9f9f9; }%s",
                indent2, indent);
    dstr_append(&d, "%s  h1 { color: #333; border-bottom: 2px solid #4a90d9; padding-bottom: 0.3em; }%s",
                indent2, indent);
    dstr_append(&d, "%s  table { border-collapse: collapse; width: 100%%; background: #fff; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }%s",
                indent2, indent);
    dstr_append(&d, "%s  th { background: #4a90d9; color: #fff; padding: 10px; text-align: left; }%s",
                indent2, indent);
    dstr_append(&d, "%s  td { padding: 8px 10px; border-bottom: 1px solid #e0e0e0; }%s",
                indent2, indent);
    dstr_append(&d, "%s  tr:nth-child(even) { background: #f2f7fd; }%s",
                indent2, indent);
    dstr_append(&d, "%s  .trace { margin-top: 2em; padding: 1em; background: #fff3cd; border: 1px solid #ffc107; }%s",
                indent2, indent);
    dstr_append(&d, "%s</style>%s", indent2, indent);
    dstr_append(&d, "</head>%s", indent);
    dstr_append(&d, "<body>%s", indent);
    dstr_append(&d, "%s<h1>%s</h1>%s", indent2, safe_str(proof->theorem), indent);
    dstr_append(&d, "%s<table>%s", indent2, indent);
    dstr_append(&d, "%s  <thead><tr><th>Step</th><th>Rule</th><th>Premise</th><th>Conclusion</th><th>Depth</th></tr></thead>%s",
                indent2, indent);
    dstr_append(&d, "%s  <tbody>%s", indent2, indent);
    for (int i = 0; i < proof->n_steps; i++) {
        const Lv00ProofStep *s = &proof->steps[i];
        dstr_append(&d, "%s    <tr>", indent2);
        dstr_append(&d, "<td>%d</td>", s->step_id);
        dstr_append(&d, "<td>%s</td>", safe_str(s->rule));
        dstr_append(&d, "<td>%s</td>", s->premise ? s->premise : "<em>none</em>");
        dstr_append(&d, "<td>%s</td>", safe_str(s->conclusion));
        dstr_append(&d, "<td>%d</td>", s->depth);
        dstr_append(&d, "</tr>%s", indent);
    }
    dstr_append(&d, "%s  </tbody>%s", indent2, indent);
    dstr_append(&d, "%s</table>%s", indent2, indent);

    if (config->include_proof_trace) {
        dstr_append(&d, "%s<div class=\"trace\">%s", indent2, indent);
        dstr_append(&d, "%s  <h3>Proof Trace</h3>%s", indent2, indent);
        dstr_append(&d, "%s  <p>Total steps: %d</p>%s", indent2, proof->n_steps, indent);
        for (int i = 0; i < proof->n_steps; i++) {
            const Lv00ProofStep *s = &proof->steps[i];
            dstr_append(&d, "%s  <p>[%d] %s", indent2, s->step_id, safe_str(s->rule));
            if (s->premise) dstr_append(&d, " (from %s)", s->premise);
            dstr_append(&d, " &rarr; %s</p>%s", safe_str(s->conclusion), indent);
        }
        dstr_append(&d, "%s</div>%s", indent2, indent);
    }

    dstr_append(&d, "</body>%s", indent);
    dstr_append(&d, "</html>%s", indent);

    return make_success(&d);
}

/* ================================================================
 * LaTeX 导出
 * ================================================================ */

static Lv00ExportResult *export_latex(const Lv00Proof *proof,
                                      const Lv00ExportConfig *config) {
    DStr d;
    if (dstr_init(&d, DSTR_INIT_CAP) != 0) return NULL;

    const char *nl = config->pretty_print ? "\n" : "\n";

    dstr_append(&d, "\\documentclass{article}%s", nl);
    dstr_append(&d, "\\usepackage{amsthm}%s", nl);
    dstr_append(&d, "\\usepackage{amsmath}%s", nl);
    dstr_append(&d, "\\usepackage{amssymb}%s", nl);
    dstr_append(&d, "%s\\begin{document}%s", nl, nl);

    /* 将定理名转义 LaTeX 特殊字符 */
    const char *th = safe_str(proof->theorem);
    dstr_append(&d, "\\begin{proof}[");
    /* 简单转义：\ → \textbackslash, { → \{, } → \}, _ → \_, & → \&, # → \#, % → \% */
    for (const char *p = th; *p; p++) {
        switch (*p) {
            case '\\': dstr_append(&d, "\\textbackslash{}"); break;
            case '{':  dstr_append(&d, "\\{"); break;
            case '}':  dstr_append(&d, "\\}"); break;
            case '_':  dstr_append(&d, "\\_"); break;
            case '&':  dstr_append(&d, "\\&"); break;
            case '#':  dstr_append(&d, "\\#"); break;
            case '%':  dstr_append(&d, "\\%%"); break;
            case '$':  dstr_append(&d, "\\$"); break;
            case '^':  dstr_append(&d, "\\^{}"); break;
            case '~':  dstr_append(&d, "\\~{}"); break;
            default: {
                char tmp[2] = {*p, '\0'};
                dstr_append_str(&d, tmp);
                break;
            }
        }
    }
    dstr_append(&d, "]%s", nl);

    dstr_append(&d, "\\begin{tabular}{|c|c|c|c|}%s", nl);
    dstr_append(&d, "\\hline%s", nl);
    dstr_append(&d, "Step & Rule & Premise & Conclusion \\\\\\hline%s", nl);
    for (int i = 0; i < proof->n_steps; i++) {
        const Lv00ProofStep *s = &proof->steps[i];
        dstr_append(&d, "%d & %s & %s & %s \\\\\\hline%s",
                    s->step_id,
                    safe_str(s->rule),
                    s->premise ? s->premise : "---",
                    safe_str(s->conclusion),
                    nl);
    }
    dstr_append(&d, "\\end{tabular}%s", nl);

    dstr_append(&d, "\\end{proof}%s", nl);
    dstr_append(&d, "\\end{document}%s", nl);

    return make_success(&d);
}

/* ================================================================
 * Coq 导出
 * ================================================================ */

static Lv00ExportResult *export_coq(const Lv00Proof *proof,
                                    const Lv00ExportConfig *config) {
    DStr d;
    if (dstr_init(&d, DSTR_INIT_CAP) != 0) return NULL;

    (void)config;

    char id[256];
    sanitize_id(id, sizeof(id), proof->theorem);

    dstr_append(&d, "(* Proof: %s *)%s", safe_str(proof->theorem), "\n");
    dstr_append(&d, "%s", "\n");
    dstr_append(&d, "Theorem %s : True.%s", id, "\n");
    dstr_append(&d, "Proof.%s", "\n");
    for (int i = 0; i < proof->n_steps; i++) {
        const Lv00ProofStep *s = &proof->steps[i];
        dstr_append(&d, "  (* Step %d: %s", s->step_id, safe_str(s->rule));
        if (s->premise) dstr_append(&d, " (premise: %s)", s->premise);
        dstr_append(&d, " → %s *)%s", safe_str(s->conclusion), "\n");
        dstr_append(&d, "  %s. (* %s *)%s", "exact I", safe_str(s->rule), "\n");
    }
    dstr_append(&d, "Qed.%s", "\n");

    return make_success(&d);
}

/* ================================================================
 * Lean 4 导出
 * ================================================================ */

static Lv00ExportResult *export_lean4(const Lv00Proof *proof,
                                      const Lv00ExportConfig *config) {
    DStr d;
    if (dstr_init(&d, DSTR_INIT_CAP) != 0) return NULL;

    (void)config;

    char id[256];
    sanitize_id(id, sizeof(id), proof->theorem);

    dstr_append(&d, "import Mathlib%s", "\n");
    dstr_append(&d, "%s", "\n");
    dstr_append(&d, "/- Proof: %s -/%s", safe_str(proof->theorem), "\n");
    dstr_append(&d, "theorem %s : True := by%s", id, "\n");
    for (int i = 0; i < proof->n_steps; i++) {
        const Lv00ProofStep *s = &proof->steps[i];
        dstr_append(&d, "  -- Step %d: %s", s->step_id, safe_str(s->rule));
        if (s->premise) dstr_append(&d, " (from %s)", s->premise);
        dstr_append(&d, " → %s%s", safe_str(s->conclusion), "\n");
        dstr_append(&d, "  have h%d : True := by trivial%s", s->step_id, "\n");
    }
    dstr_append(&d, "  exact h%d%s", proof->steps[proof->n_steps - 1].step_id, "\n");

    return make_success(&d);
}

/* ================================================================
 * JSON 导出
 * ================================================================ */

static Lv00ExportResult *export_json(const Lv00Proof *proof,
                                     const Lv00ExportConfig *config) {
    DStr d;
    if (dstr_init(&d, DSTR_INIT_CAP) != 0) return NULL;

    const char *nl  = config->pretty_print ? "\n" : "";
    const char *sp  = config->pretty_print ? "  " : "";
    const char *sp2 = config->pretty_print ? "    " : "";
    const char *sp3 = config->pretty_print ? "      " : "";

    dstr_append(&d, "{%s", nl);
    dstr_append(&d, "%s\"theorem\": ", sp);
    dstr_append_json_string(&d, proof->theorem);
    dstr_append(&d, ",%s", nl);
    dstr_append(&d, "%s\"n_steps\": %d,%s", sp, proof->n_steps, nl);
    dstr_append(&d, "%s\"steps\": [%s", sp, nl);

    for (int i = 0; i < proof->n_steps; i++) {
        const Lv00ProofStep *s = &proof->steps[i];
        dstr_append(&d, "%s{%s", sp2, nl);
        dstr_append(&d, "%s\"id\": %d,%s", sp3, s->step_id, nl);
        dstr_append(&d, "%s\"rule\": ", sp3);
        dstr_append_json_string(&d, s->rule);
        dstr_append(&d, ",%s", nl);
        dstr_append(&d, "%s\"premise\": ", sp3);
        if (s->premise) {
            dstr_append_json_string(&d, s->premise);
        } else {
            dstr_append(&d, "null");
        }
        dstr_append(&d, ",%s", nl);
        dstr_append(&d, "%s\"conclusion\": ", sp3);
        dstr_append_json_string(&d, s->conclusion);
        dstr_append(&d, ",%s", nl);
        dstr_append(&d, "%s\"depth\": %d%s", sp3, s->depth, nl);
        dstr_append(&d, "%s}%s%s", sp2, (i < proof->n_steps - 1) ? "," : "", nl);
    }

    dstr_append(&d, "%s]%s", sp, nl);
    dstr_append(&d, "}%s", nl);

    return make_success(&d);
}

/* ================================================================
 * DOT（Graphviz）导出
 * ================================================================ */

static Lv00ExportResult *export_dot(const Lv00Proof *proof,
                                    const Lv00ExportConfig *config) {
    DStr d;
    if (dstr_init(&d, DSTR_INIT_CAP) != 0) return NULL;

    const char *nl  = config->pretty_print ? "\n" : "";
    const char *sp  = config->pretty_print ? "  " : "";
    const char *sp2 = config->pretty_print ? "    " : "";

    dstr_append(&d, "digraph Proof {%s", nl);
    dstr_append(&d, "%srankdir=TB;%s", sp, nl);
    dstr_append(&d, "%snode[shape=box, style=rounded];%s", sp, nl);
    dstr_append(&d, "%sfontname=\"Helvetica\";%s", sp, nl);
    dstr_append(&d, "%slabel=\"%s\";%s", sp, safe_str(proof->theorem), nl);

    /* 节点 */
    for (int i = 0; i < proof->n_steps; i++) {
        const Lv00ProofStep *s = &proof->steps[i];
        dstr_append(&d, "%sstep%d[label=\"[%d] %s\\n%s",
                    sp, s->step_id, s->step_id,
                    safe_str(s->rule),
                    safe_str(s->conclusion));
        if (s->premise) {
            dstr_append(&d, "\\n(from: %s)", s->premise);
        }
        dstr_append(&d, "\"];%s", nl);
    }

    /* 边：如果步骤 i 的 premise 等于步骤 j 的 conclusion，则添加 j → i */
    dstr_append(&d, "%s%s", sp, nl);
    for (int i = 0; i < proof->n_steps; i++) {
        const Lv00ProofStep *si = &proof->steps[i];
        if (!si->premise) continue;
        for (int j = 0; j < proof->n_steps; j++) {
            if (i == j) continue;
            const Lv00ProofStep *sj = &proof->steps[j];
            if (strcmp(si->premise, sj->conclusion) == 0) {
                dstr_append(&d, "%sstep%d -> step%d;%s", sp, sj->step_id, si->step_id, nl);
            }
        }
    }

    dstr_append(&d, "}%s", nl);

    return make_success(&d);
}

/* ================================================================
 * 公共 API
 * ================================================================ */

Lv00ExportResult *proof_export_enhanced(const Lv00Proof *proof,
                                        const Lv00ExportConfig *config) {
    if (!proof) {
        return make_error("proof_export_enhanced: proof is NULL");
    }
    if (!config) {
        return make_error("proof_export_enhanced: config is NULL");
    }

    switch (config->format) {
        case EXPORT_HTML:  return export_html(proof, config);
        case EXPORT_LATEX: return export_latex(proof, config);
        case EXPORT_COQ:   return export_coq(proof, config);
        case EXPORT_LEAN4: return export_lean4(proof, config);
        case EXPORT_JSON:  return export_json(proof, config);
        case EXPORT_DOT:   return export_dot(proof, config);
        default:
            return make_error("proof_export_enhanced: unknown format");
    }
}

Lv00ExportResult *proof_export_from_navigator(const char *theorem_name,
                                              Lv00ExportFormat format) {
    if (!theorem_name) {
        return make_error("proof_export_from_navigator: theorem_name is NULL");
    }

    Lv00ProofStep step;
    step.step_id    = 1;
    step.rule       = "given";
    step.premise    = NULL;
    step.conclusion = theorem_name;
    step.depth      = 0;

    Lv00Proof proof;
    proof.steps   = &step;
    proof.n_steps = 1;
    proof.theorem = theorem_name;

    Lv00ExportConfig config;
    config.format              = format;
    config.include_proof_trace = false;
    config.include_geometry    = false;
    config.pretty_print        = true;

    return proof_export_enhanced(&proof, &config);
}

void proof_export_result_destroy(Lv00ExportResult *result) {
    if (!result) return;
    if (result->output) {
        lv00_free((void **)&result->output);
        result->output = NULL;
    }
    lv00_free((void **)&result);
}
