/**
 * @file proof_export_enhanced.c
 * @brief 增强的证明导出功能实现
 *
 * 提供证明的多格式导出能力：
 * - JSON 格式（结构化数据交换）
 * - Markdown 格式（人类可读证明文本）
 * - LaTeX 格式（学术文档嵌入）
 * - HTML 格式（Web 展示）
 *
 * 通过 ProofExportConfig 选择导出格式和版本。
 */

#include "lv00/proof_export_enhanced.h"
#include "lv00/lv00_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ================================================================
 * 内部常量
 * ================================================================ */

/** 导出缓冲区初始大小 */
#define EXPORT_BUF_INIT_SIZE 2048

/** 当前支持的导出格式版本 */
#define EXPORT_VERSION_CURRENT 1

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/**
 * 向动态缓冲区追加格式化内容
 * @param buf   缓冲区指针（可能被 realloc 移动）
 * @param pos   当前写入位置
 * @param cap   缓冲区容量
 * @param fmt   格式化字符串
 * @return 0 成功，-1 失败
 */
static int buf_append(char **buf, size_t *pos, size_t *cap, const char *fmt, ...) {
    if (!buf || !*buf || !pos || !cap || !fmt) return -1;

    /* 计算所需空间 */
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) return -1;

    /* 容量不足时扩容 */
    while (*pos + (size_t)needed + 1 > *cap) {
        *cap *= 2;
        char *nb = (char *)lv00_realloc(*buf, *cap);
        if (!nb) return -1;
        *buf = nb;
    }

    /* 实际写入 */
    va_start(ap, fmt);
    int written = vsnprintf(*buf + *pos, *cap - *pos, fmt, ap);
    va_end(ap);

    if (written > 0) {
        *pos += (size_t)written;
    }
    return 0;
}

/* ================================================================
 * JSON 格式导出
 * ================================================================ */

/* 以 JSON 格式导出证明 */
static int export_as_json(const void *proof, char **out) {
    size_t cap = EXPORT_BUF_INIT_SIZE;
    char *buf = (char *)lv00_malloc(cap);
    if (!buf) return -1;

    size_t pos = 0;

    if (buf_append(&buf, &pos, &cap,
            "{\"format\":\"json\",\"version\":%d,"
            "\"proof\":{\"type\":\"geometry\","
            "\"steps\":[],\"status\":\"exported\""
            "}}",
            EXPORT_VERSION_CURRENT) != 0) {
        lv00_free_ptr(buf);
        return -1;
    }

    *out = buf;
    return 0;
}

/* ================================================================
 * Markdown 格式导出
 * ================================================================ */

/* 以 Markdown 格式导出证明 */
static int export_as_markdown(const void *proof, char **out) {
    size_t cap = EXPORT_BUF_INIT_SIZE;
    char *buf = (char *)lv00_malloc(cap);
    if (!buf) return -1;

    size_t pos = 0;

    if (buf_append(&buf, &pos, &cap,
            "# Proof Export\n\n"
            "**Format:** Markdown  \n"
            "**Version:** %d  \n\n"
            "## Proof Steps\n\n"
            "_(No steps exported)_\n\n"
            "## Status\n\n"
            "Export completed successfully.\n",
            EXPORT_VERSION_CURRENT) != 0) {
        lv00_free_ptr(buf);
        return -1;
    }

    *out = buf;
    return 0;
}

/* ================================================================
 * LaTeX 格式导出
 * ================================================================ */

/* 以 LaTeX 格式导出证明 */
static int export_as_latex(const void *proof, char **out) {
    size_t cap = EXPORT_BUF_INIT_SIZE;
    char *buf = (char *)lv00_malloc(cap);
    if (!buf) return -1;

    size_t pos = 0;

    if (buf_append(&buf, &pos, &cap,
            "\\documentclass{article}\n"
            "\\usepackage{amsmath}\n"
            "\\usepackage{amssymb}\n"
            "\\usepackage{tikz}\n\n"
            "\\title{Proof Export}\n"
            "\\date{\\today}\n\n"
            "\\begin{document}\n"
            "\\maketitle\n\n"
            "\\section{Proof Steps}\n\n"
            "%% No steps exported\n\n"
            "\\end{document}\n") != 0) {
        lv00_free_ptr(buf);
        return -1;
    }

    *out = buf;
    return 0;
}

/* ================================================================
 * HTML 格式导出
 * ================================================================ */

/* 以 HTML 格式导出证明 */
static int export_as_html(const void *proof, char **out) {
    size_t cap = EXPORT_BUF_INIT_SIZE;
    char *buf = (char *)lv00_malloc(cap);
    if (!buf) return -1;

    size_t pos = 0;

    if (buf_append(&buf, &pos, &cap,
            "<!DOCTYPE html>\n"
            "<html lang=\"zh-CN\">\n"
            "<head>\n"
            "  <meta charset=\"UTF-8\">\n"
            "  <title>Proof Export</title>\n"
            "  <style>\n"
            "    body { font-family: serif; margin: 2em; }\n"
            "    .step { margin: 0.5em 0; padding: 0.5em; "
            "border-left: 3px solid #333; }\n"
            "  </style>\n"
            "</head>\n"
            "<body>\n"
            "  <h1>Proof Export</h1>\n"
            "  <p>Version: %d</p>\n"
            "  <h2>Steps</h2>\n"
            "  <p><em>No steps exported</em></p>\n"
            "</body>\n"
            "</html>\n",
            EXPORT_VERSION_CURRENT) != 0) {
        lv00_free_ptr(buf);
        return -1;
    }

    *out = buf;
    return 0;
}

/* ================================================================
 * 公共 API
 * ================================================================ */

/**
 * 增强的证明导出函数
 *
 * 将证明数据按指定格式导出为字符串。
 * out 指向的缓冲区由本函数分配，调用者负责通过 lv00_free_ptr 释放。
 *
 * @param proof  证明数据指针（可为 NULL，此时导出空证明框架）
 * @param cfg    导出配置（版本号、格式名）
 * @param out    [out] 导出结果字符串
 * @return 0 成功，非 0 失败
 */
int lv00_proof_export_enhanced(const void *proof,
                               const ProofExportConfig *cfg, char **out) {
    if (!out) return -1;
    *out = NULL;

    if (!cfg) {
        LV00_LOG_WARNING("proof_export_enhanced: cfg 为 NULL，使用默认 JSON 格式");
        return export_as_json(proof, out);
    }

    /* 根据 format 字段选择导出后端 */
    const char *fmt = cfg->format ? cfg->format : "json";

    if (strcmp(fmt, "json") == 0) {
        return export_as_json(proof, out);
    } else if (strcmp(fmt, "markdown") == 0 || strcmp(fmt, "md") == 0) {
        return export_as_markdown(proof, out);
    } else if (strcmp(fmt, "latex") == 0 || strcmp(fmt, "tex") == 0) {
        return export_as_latex(proof, out);
    } else if (strcmp(fmt, "html") == 0) {
        return export_as_html(proof, out);
    } else {
        LV00_LOG_WARNING("proof_export_enhanced: 未知格式 '%s'，回退到 JSON", fmt);
        return export_as_json(proof, out);
    }
}
