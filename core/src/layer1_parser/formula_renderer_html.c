/**
 * @file formula_renderer_html.c
 * @brief HTML 渲染后端
 *
 * @details 从 formula_renderer.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "formula_renderer.h"
#include "formula_renderer_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_thread.h"
#include "lv_utils.h"

/* ============================================================
 * HTML 渲染器
 * ============================================================ */

/**
 * @brief 将 AST 渲染为 HTML MathJax 兼容格式
 *
 * 将 LaTeX 渲染结果包装在 MathJax 兼容的 HTML 标签中。
 * options 控制精度和输出样式（inline/block）。
 */
int render_html_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    int precision = 6; /* 默认精度 */
    int use_block = 0; /* 0=inline <code>, 1=block <div> */
    if (options) {
        if (options->precision > 0)
            precision = options->precision;
        if (options->style)
            use_block = (options->style[0] == 'b' || options->style[0] == 'B');
    }
    if (!node)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "invalid params for html render");

    char *latex_buf = (char *) lv_malloc(lv_MAX_RENDER_BUFFER);
    if (!latex_buf)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate latex buffer");

    int latex_len = render_latex_internal(node, latex_buf, lv_MAX_RENDER_BUFFER, options);
    if (latex_len < 0) {
        lv_free((void **) &latex_buf);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "latex sub-render failed");
    }

    /* latex_buf 为外部可注入内容：data-formula 属性与正文均经 HTML 实体转义（两遍法） */
    size_t esc_len = strlen(latex_buf);
    size_t need = lv_str_html_escape(latex_buf, esc_len, NULL, 0);
    char *esc_buf = (char *) lv_malloc(need + 1);
    if (!esc_buf) {
        lv_free((void **) &latex_buf);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate html escape buffer");
    }
    lv_str_html_escape(latex_buf, esc_len, esc_buf, need + 1);

    int written = snprintf(buffer, size, "<span class=\"mathjax-container\" data-formula=\"%s\">\\(%s\\)</span>",
                           esc_buf, esc_buf);

    lv_free((void **) &esc_buf);
    lv_free((void **) &latex_buf);
    return written;
}

