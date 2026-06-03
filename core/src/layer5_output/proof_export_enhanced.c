/**
 * @file proof_export_enhanced.c
 * @brief Implementation of the enhanced proof export module.
 *
 * @details Implements serialization of proof data into multiple formats:
 *          HTML, LaTeX, Coq, Lean 4, JSON, and DOT.
 *
 *          Each format has its own internal serializer function that
 *          builds the output string using a dynamic buffer.
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include "lv00/proof_export_enhanced.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Dynamic string buffer for building output
 * ============================================================ */

typedef struct {
    char  *data;
    size_t size;
    size_t capacity;
} StringBuffer;

static void buf_init(StringBuffer *buf) {
    buf->data     = NULL;
    buf->size     = 0;
    buf->capacity = 0;
}

static bool buf_append(StringBuffer *buf, const char *str) {
    if (!str) return true;
    size_t len = strlen(str);
    if (len == 0) return true;

    if (buf->size + len + 1 > buf->capacity) {
        size_t new_cap = buf->capacity * 2;
        if (new_cap < buf->size + len + 1) {
            new_cap = buf->size + len + 256;
        }
        char *new_data = (char *)realloc(buf->data, new_cap);
        if (!new_data) return false;
        buf->data     = new_data;
        buf->capacity = new_cap;
    }

    memcpy(buf->data + buf->size, str, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
    return true;
}

static void buf_free(StringBuffer *buf) {
    free(buf->data);
    buf->data     = NULL;
    buf->size     = 0;
    buf->capacity = 0;
}

/* ============================================================
 * HTML serializer
 * ============================================================ */

static bool export_html(const Lv00Proof *proof, const Lv00ExportConfig *config,
                        StringBuffer *buf) {
    const char *indent = config->pretty_print ? "  " : "";

    buf_append(buf, "<!DOCTYPE html>\n<html>\n<head>\n");
    buf_append(buf, indent);
    buf_append(buf, "<meta charset=\"UTF-8\">\n");
    buf_append(buf, indent);
    buf_append(buf, "<title>Proof Export</title>\n");
    buf_append(buf, indent);
    buf_append(buf, "<style>\n");
    buf_append(buf, indent);
    buf_append(buf, "  .proof-step { margin: 4px 0; padding-left: ");
    if (config->pretty_print) {
        char depth_str[32];
        snprintf(depth_str, sizeof(depth_str), "%d", 20);
        buf_append(buf, depth_str);
    } else {
        buf_append(buf, "0");
    }
    buf_append(buf, "px; }\n");
    buf_append(buf, indent);
    buf_append(buf, "  .rule { font-weight: bold; color: #2a6; }\n");
    buf_append(buf, indent);
    buf_append(buf, "</style>\n");
    buf_append(buf, "</head>\n<body>\n");

    if (proof->theorem) {
        buf_append(buf, indent);
        buf_append(buf, "<h1>Theorem: ");
        buf_append(buf, proof->theorem);
        buf_append(buf, "</h1>\n");
    }

    buf_append(buf, indent);
    buf_append(buf, "<div class=\"proof\">\n");

    for (size_t i = 0; i < proof->n_steps; i++) {
        const Lv00ProofStep *step = &proof->steps[i];
        char line[1024];

        snprintf(line, sizeof(line), "%s  <div class=\"proof-step\" data-id=\"%d\">\n",
                 indent, step->step_id);
        buf_append(buf, line);

        if (step->rule) {
            snprintf(line, sizeof(line), "%s    <span class=\"rule\">%s</span>", indent, step->rule);
            buf_append(buf, line);
        }

        if (config->include_proof_trace && step->premise) {
            snprintf(line, sizeof(line), " [%s]", step->premise);
            buf_append(buf, line);
        }

        buf_append(buf, "\n");

        if (step->conclusion) {
            snprintf(line, sizeof(line), "%s    <div>%s</div>\n", indent, step->conclusion);
            buf_append(buf, line);
        }

        buf_append(buf, indent);
        buf_append(buf, "  </div>\n");
    }

    buf_append(buf, indent);
    buf_append(buf, "</div>\n");
    buf_append(buf, "</body>\n</html>\n");

    return true;
}

/* ============================================================
 * LaTeX serializer
 * ============================================================ */

static bool export_latex(const Lv00Proof *proof, const Lv00ExportConfig *config,
                         StringBuffer *buf) {
    (void)config;

    buf_append(buf, "\\documentclass{article}\n");
    buf_append(buf, "\\usepackage{amsmath}\n");
    buf_append(buf, "\\usepackage{amsthm}\n\n");
    buf_append(buf, "\\begin{document}\n\n");

    if (proof->theorem) {
        buf_append(buf, "\\begin{theorem}\n");
        buf_append(buf, proof->theorem);
        buf_append(buf, "\n\\end{theorem}\n\n");
    }

    buf_append(buf, "\\begin{proof}\n");

    for (size_t i = 0; i < proof->n_steps; i++) {
        const Lv00ProofStep *step = &proof->steps[i];
        char line[1024];

        if (step->rule) {
            buf_append(buf, "\\textbf{");
            buf_append(buf, step->rule);
            buf_append(buf, "}");
        }

        if (step->premise) {
            buf_append(buf, " (from $");
            buf_append(buf, step->premise);
            buf_append(buf, "$)");
        }

        if (step->conclusion) {
            buf_append(buf, ": $");
            buf_append(buf, step->conclusion);
            buf_append(buf, "$");
        }

        buf_append(buf, ".\n\n");
    }

    buf_append(buf, "\\end{proof}\n\n");
    buf_append(buf, "\\end{document}\n");

    return true;
}

/* ============================================================
 * Coq serializer
 * ============================================================ */

static bool export_coq(const Lv00Proof *proof, const Lv00ExportConfig *config,
                       StringBuffer *buf) {
    (void)config;

    if (proof->theorem) {
        buf_append(buf, "Theorem ");
        /* Use theorem name or default */
        buf_append(buf, proof->theorem);
        buf_append(buf, " : Prop.\nProof.\n");
    } else {
        buf_append(buf, "Lemma unnamed : Prop.\nProof.\n");
    }

    for (size_t i = 0; i < proof->n_steps; i++) {
        const Lv00ProofStep *step = &proof->steps[i];
        char line[1024];

        /* Indent based on depth */
        int indent_level = step->depth + 1;
        for (int j = 0; j < indent_level; j++) {
            buf_append(buf, "  ");
        }

        if (step->rule) {
            buf_append(buf, step->rule);
        } else {
            buf_append(buf, "apply ");
            buf_append(buf, step->conclusion ? step->conclusion : "H");
        }

        buf_append(buf, ".\n");
    }

    buf_append(buf, "Qed.\n");

    return true;
}

/* ============================================================
 * Lean 4 serializer
 * ============================================================ */

static bool export_lean4(const Lv00Proof *proof, const Lv00ExportConfig *config,
                         StringBuffer *buf) {
    (void)config;

    buf_append(buf, "import Mathlib\n\n");

    if (proof->theorem) {
        buf_append(buf, "theorem ");
        buf_append(buf, proof->theorem);
        buf_append(buf, " : Prop := by\n");
    } else {
        buf_append(buf, "theorem unnamed : Prop := by\n");
    }

    for (size_t i = 0; i < proof->n_steps; i++) {
        const Lv00ProofStep *step = &proof->steps[i];
        char line[1024];

        int indent_level = step->depth + 1;
        for (int j = 0; j < indent_level; j++) {
            buf_append(buf, "  ");
        }

        if (step->rule) {
            buf_append(buf, step->rule);
        } else {
            buf_append(buf, "apply ");
            buf_append(buf, step->conclusion ? step->conclusion : "h");
        }

        buf_append(buf, "\n");
    }

    return true;
}

/* ============================================================
 * JSON serializer
 * ============================================================ */

static void json_escape(const char *str, StringBuffer *buf) {
    if (!str) return;
    for (size_t i = 0; str[i] != '\0'; i++) {
        switch (str[i]) {
            case '"':  buf_append(buf, "\\\""); break;
            case '\\': buf_append(buf, "\\\\"); break;
            case '\n': buf_append(buf, "\\n");  break;
            case '\t': buf_append(buf, "\\t");  break;
            default: {
                char ch[2] = {str[i], '\0'};
                buf_append(buf, ch);
                break;
            }
        }
    }
}

static bool export_json(const Lv00Proof *proof, const Lv00ExportConfig *config,
                        StringBuffer *buf) {
    (void)config;

    buf_append(buf, "{\n");

    if (proof->theorem) {
        buf_append(buf, "  \"theorem\": \"");
        json_escape(proof->theorem, buf);
        buf_append(buf, "\",\n");
    }

    buf_append(buf, "  \"steps\": [\n");

    for (size_t i = 0; i < proof->n_steps; i++) {
        const Lv00ProofStep *step = &proof->steps[i];
        char line[256];

        buf_append(buf, "    {\n");

        snprintf(line, sizeof(line), "      \"id\": %d,\n", step->step_id);
        buf_append(buf, line);

        buf_append(buf, "      \"rule\": ");
        if (step->rule) {
            buf_append(buf, "\"");
            json_escape(step->rule, buf);
            buf_append(buf, "\"");
        } else {
            buf_append(buf, "null");
        }
        buf_append(buf, ",\n");

        buf_append(buf, "      \"premise\": ");
        if (step->premise) {
            buf_append(buf, "\"");
            json_escape(step->premise, buf);
            buf_append(buf, "\"");
        } else {
            buf_append(buf, "null");
        }
        buf_append(buf, ",\n");

        buf_append(buf, "      \"conclusion\": ");
        if (step->conclusion) {
            buf_append(buf, "\"");
            json_escape(step->conclusion, buf);
            buf_append(buf, "\"");
        } else {
            buf_append(buf, "null");
        }
        buf_append(buf, ",\n");

        snprintf(line, sizeof(line), "      \"depth\": %d\n", step->depth);
        buf_append(buf, line);

        buf_append(buf, "    }");
        if (i + 1 < proof->n_steps) {
            buf_append(buf, ",");
        }
        buf_append(buf, "\n");
    }

    buf_append(buf, "  ]\n");
    buf_append(buf, "}\n");

    return true;
}

/* ============================================================
 * DOT serializer
 * ============================================================ */

static bool export_dot(const Lv00Proof *proof, const Lv00ExportConfig *config,
                       StringBuffer *buf) {
    (void)config;

    buf_append(buf, "digraph Proof {\n");
    buf_append(buf, "  rankdir=TB;\n");
    buf_append(buf, "  node [shape=box, fontname=\"Helvetica\"];\n\n");

    if (proof->theorem) {
        buf_append(buf, "  label=\"");
        buf_append(buf, proof->theorem);
        buf_append(buf, "\";\n\n");
    }

    for (size_t i = 0; i < proof->n_steps; i++) {
        const Lv00ProofStep *step = &proof->steps[i];
        char line[512];

        /* Node */
        snprintf(line, sizeof(line), "  step%d [label=\"%d: %s\"];\n",
                 step->step_id, step->step_id,
                 step->rule ? step->rule : "step");
        buf_append(buf, line);

        /* Edge from previous step */
        if (i > 0) {
            const Lv00ProofStep *prev = &proof->steps[i - 1];
            snprintf(line, sizeof(line), "  step%d -> step%d;\n",
                     prev->step_id, step->step_id);
            buf_append(buf, line);
        }
    }

    buf_append(buf, "}\n");

    return true;
}

/* ============================================================
 * API: Export
 * ============================================================ */

Lv00ExportResult *proof_export_enhanced(const Lv00Proof       *proof,
                                        const Lv00ExportConfig *config) {
    if (!proof || !config) {
        Lv00ExportResult *result = (Lv00ExportResult *)calloc(1, sizeof(Lv00ExportResult));
        if (result) {
            result->success   = false;
            result->error_msg = strdup("NULL proof or config");
        }
        return result;
    }

    StringBuffer buf;
    buf_init(&buf);

    bool ok = false;
    switch (config->format) {
        case EXPORT_HTML:  ok = export_html(proof, config, &buf);  break;
        case EXPORT_LATEX: ok = export_latex(proof, config, &buf); break;
        case EXPORT_COQ:   ok = export_coq(proof, config, &buf);   break;
        case EXPORT_LEAN4: ok = export_lean4(proof, config, &buf); break;
        case EXPORT_JSON:  ok = export_json(proof, config, &buf);  break;
        case EXPORT_DOT:   ok = export_dot(proof, config, &buf);   break;
        default:
            buf_free(&buf);
            Lv00ExportResult *result = (Lv00ExportResult *)calloc(1, sizeof(Lv00ExportResult));
            if (result) {
                result->success   = false;
                result->error_msg = strdup("Unsupported export format");
            }
            return result;
    }

    Lv00ExportResult *result = (Lv00ExportResult *)calloc(1, sizeof(Lv00ExportResult));
    if (!result) {
        buf_free(&buf);
        return NULL;
    }

    if (ok && buf.data) {
        result->output      = buf.data;
        result->output_size = buf.size;
        result->success     = true;
        result->error_msg   = NULL;
    } else {
        buf_free(&buf);
        result->success   = false;
        result->error_msg = strdup("Export serialization failed");
    }

    return result;
}

/* ============================================================
 * API: Export from navigator
 * ============================================================ */

Lv00ExportResult *proof_export_from_navigator(const char      *theorem_name,
                                              Lv00ExportFormat format) {
    if (!theorem_name) {
        Lv00ExportResult *result = (Lv00ExportResult *)calloc(1, sizeof(Lv00ExportResult));
        if (result) {
            result->success   = false;
            result->error_msg = strdup("NULL theorem name");
        }
        return result;
    }

    /* Build a minimal proof with a single step */
    Lv00ProofStep step;
    step.step_id    = 1;
    step.rule       = "assume";
    step.premise    = NULL;
    step.conclusion = theorem_name;
    step.depth      = 0;

    Lv00Proof proof;
    proof.steps   = &step;
    proof.n_steps = 1;
    proof.theorem = theorem_name;

    Lv00ExportConfig config;
    config.format              = format;
    config.include_proof_trace = true;
    config.include_geometry    = false;
    config.pretty_print        = true;

    return proof_export_enhanced(&proof, &config);
}

/* ============================================================
 * API: Destroy
 * ============================================================ */

void proof_export_result_destroy(Lv00ExportResult *result) {
    if (!result) return;
    free(result->output);
    free(result->error_msg);
    free(result);
}
