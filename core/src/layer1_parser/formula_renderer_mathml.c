/**
 * @file formula_renderer_mathml.c
 * @brief MathML 渲染后端
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
 * MathML 渲染器
 * ============================================================ */

/**
 * @brief 将 AST 渲染为 MathML 格式
 *
 * 当前使用 LaTeX-in-annotation 方式（annotation encoding），这是合法的 MathML 表示。
 * 未来可扩展为原生 MathML 语义元素（<mfrac>, <msqrt>, <msup> 等）。
 */
int render_mathml_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    /* options 预留：未来可控制输出精度、样式等 */
    (void) options;
    if (!node || !buffer || size == 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "invalid params for mathml render");

    char *latex_buf = (char *) lv_malloc(lv_MAX_RENDER_BUFFER);
    if (!latex_buf)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate latex buffer");

    int latex_len = render_latex_internal(node, latex_buf, lv_MAX_RENDER_BUFFER, options);
    if (latex_len < 0) {
        lv_free((void **) &latex_buf);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "latex sub-render failed");
    }

    int written = snprintf(buffer, size,
                           "<math xmlns=\"http://www.w3.org/1998/Math/MathML\" display=\"block\">\n"
                           "  <semantics>\n"
                           "    <mrow>\n"
                           "      <mi>%s</mi>\n"
                           "    </mrow>\n"
                           "    <annotation encoding=\"application/x-tex\">%s</annotation>\n"
                           "  </semantics>\n"
                           "</math>",
                           latex_buf, latex_buf);

    lv_free((void **) &latex_buf);
    return written;
}

