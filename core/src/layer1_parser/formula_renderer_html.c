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
    if (!node || !buffer || size == 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "invalid params for html render");

    char *latex_buf = (char *) lv_malloc(lv_MAX_RENDER_BUFFER);
    if (!latex_buf)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate latex buffer");

    int latex_len = render_latex_internal(node, latex_buf, lv_MAX_RENDER_BUFFER, options);
    if (latex_len < 0) {
        lv_free((void **) &latex_buf);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "latex sub-render failed");
    }

    int written = snprintf(buffer, size, "<span class=\"mathjax-container\" data-formula=\"%s\">\\(%s\\)</span>",
                           latex_buf, latex_buf);

    lv_free((void **) &latex_buf);
    return written;
}

