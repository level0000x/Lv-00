/**
 * @file formula_renderer.c
 * @brief 公式渲染器实现
 *
 * @details 将 AST 渲染为 LaTeX、Python 或 DSL 格式的字符串。
 *          支持自定义精度和格式选项。
 *
 * @note 本文件对缓冲区溢出进行了严格加固：
 *       - 所有 >256 字节的临时缓冲区改为堆分配（lv_malloc/lv_free）
 *       - 所有 ≤256 字节的栈缓冲区使用 snprintf 边界检查
 *       - 1024 字节子表达式缓冲区通过可复用池管理，避免递归时的 malloc/free 开销
 *       - 所有字符串操作均已添加边界检查
 *       - 通过 #define lv_FORMULA_BUF_SIZE 统一控制缓冲大小
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - formula_renderer.h : 渲染器公共接口定义
 *   - lv_internal.h    : 内部数据结构和常量
 *   - lv_utils.h       : 统一内存分配器（lv_malloc/lv_free）
 *   - error_codes.h      : 错误码定义
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
#include "lv_utils.h" /* lv_malloc / lv_realloc / lv_free —— 统一内存分配器 */

/* 渲染函数指针表 — 文件作用域，供所有渲染函数使用 */
static const RenderNodeFunc s_render_funcs[] = {
    [OUTPUT_LATEX]  = render_latex_internal,
    [OUTPUT_PYTHON] = render_python_internal,
    [OUTPUT_DSL]    = render_dsl_internal,
    [OUTPUT_MATHML] = render_mathml_internal,
    [OUTPUT_ASCII]  = render_ascii_internal,
    [OUTPUT_HTML]   = render_html_internal,
};

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

/**
 * @brief 渲染公式 AST 为指定格式的字符串（简化版）
 *
 * 使用默认渲染选项将 AST 渲染为字符串。
 *
 * @param node   AST 根节点
 * @param format 输出格式（LaTeX/Python/DSL）
 * @return 新分配的字符串指针，失败返回 NULL
 */
char *formula_render(const FormulaNode *node, OutputFormat format) {
    RenderOptions options = RENDER_OPTIONS_DEFAULT;
    return formula_render_ex(node, format, &options);
}

/**
 * @brief 渲染公式 AST 为指定格式的字符串（扩展版）
 *
 * 使用自定义渲染选项将 AST 渲染为字符串。
 *
 * @param node    AST 根节点
 * @param format  输出格式（LaTeX/Python/DSL）
 * @param options 渲染选项指针
 * @return 新分配的字符串指针，失败返回 NULL
 */
char *formula_render_ex(const FormulaNode *node, OutputFormat format, const RenderOptions *options) {
    if (!node) {
        lv_set_error(lv_ERROR_INTERNAL, "NULL node");
        return NULL;
    }

    /* HEAP_ALLOCATED: 输出缓冲区使用 lv_malloc */
    char *buffer = (char *) lv_malloc(lv_MAX_RENDER_BUFFER);
    if (!buffer) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "Memory allocation failed");
        return NULL;
    }

    int written = 0;

    if ((unsigned)format < sizeof(s_render_funcs) / sizeof(s_render_funcs[0]) && s_render_funcs[format]) {
        written = s_render_funcs[format](node, buffer, lv_MAX_RENDER_BUFFER, options);
    } else {
        lv_set_error(lv_ERROR_UNSUPPORTED, "Unknown output format");
        lv_free((void **) &buffer);
        return NULL;
    }

    if (written < 0) {
        lv_set_error(lv_ERROR_INTERNAL, "Render failed");
        lv_free((void **) &buffer);
        return NULL;
    }

    /* 重新分配到实际大小 */
    char *result = (char *) lv_realloc(buffer, written + 1);
    return result ? result : buffer;
}

/**
 * @brief 渲染公式 AST 到缓冲区（简化版）
 *
 * @param node   AST 根节点
 * @param format 输出格式
 * @param buffer 输出缓冲区
 * @param size   缓冲区大小
 * @return 写入的字节数（不含终止符），失败返回负值
 */
int formula_render_to_buffer(const FormulaNode *node, OutputFormat format, char *buffer, size_t size) {
    RenderOptions options = RENDER_OPTIONS_DEFAULT;
    return formula_render_to_buffer_ex(node, format, &options, buffer, size);
}

/**
 * @brief 渲染公式 AST 到缓冲区（扩展版）
 *
 * @param node    AST 根节点
 * @param format  输出格式
 * @param options 渲染选项指针
 * @param buffer  输出缓冲区
 * @param size    缓冲区大小
 * @return 写入的字节数（不含终止符），失败返回负值
 */
int formula_render_to_buffer_ex(const FormulaNode *node, OutputFormat format, const RenderOptions *options,
                                char *buffer, size_t size) {
    if (!node || !buffer || size == 0) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "invalid params for render to buffer");
    }

    int written = 0;

    if ((unsigned)format < sizeof(s_render_funcs) / sizeof(s_render_funcs[0]) && s_render_funcs[format]) {
        written = s_render_funcs[format](node, buffer, size, options);
    } else {
        lv_RETURN_ERROR(lv_ERROR_UNSUPPORTED, "unsupported output format");
    }

    return written;
}

/**
 * @brief 渲染公式 AST 为 LaTeX 字符串（便捷函数）
 *
 * @param node AST 根节点
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_latex(const FormulaNode *node) {
    return formula_render(node, OUTPUT_LATEX);
}

/**
 * @brief 渲染公式 AST 为 Python 字符串（便捷函数）
 *
 * @param node AST 根节点
 * @return 新分配的 Python 字符串指针，失败返回 NULL
 */
char *formula_render_python(const FormulaNode *node) {
    return formula_render(node, OUTPUT_PYTHON);
}

/**
 * @brief 渲染公式 AST 为 DSL 字符串（便捷函数）
 *
 * @param node AST 根节点
 * @return 新分配的 DSL 字符串指针，失败返回 NULL
 */
char *formula_render_dsl(const FormulaNode *node) {
    return formula_render(node, OUTPUT_DSL);
}

/**
 * @brief 渲染点坐标为 LaTeX 字符串
 *
 * @param name        点名称
 * @param coords      坐标 AST 节点数组
 * @param coord_count 坐标数量
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_point_latex(const char *name, const FormulaNode **coords, int coord_count) {
    if (!name || !coords || coord_count == 0) {
        return NULL;
    }

    /* HEAP_ALLOCATED: 坐标组合缓冲区使用池分配 */
    char *coords_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
    if (!coords_buf)
        return NULL;

    size_t pos = 0;

    for (int i = 0; i < coord_count; i++) {
        /* STACK_SAFE: 单个坐标缓冲区 ≤256 字节 */
        char coord_buf[lv_FORMULA_BUF_MEDIUM] = {0};
        formula_render_to_buffer(coords[i], OUTPUT_LATEX, coord_buf, sizeof(coord_buf));

        if (!lv_str_append_sep(coords_buf, lv_FORMULA_BUF_SIZE, &pos, ", ", coord_buf))
            break;
    }

    /* HEAP_ALLOCATED: 结果字符串 */
    char *result = (char *) lv_malloc(lv_POINT_LATEX_BUF_SIZE);
    if (result) {
        snprintf(result, lv_POINT_LATEX_BUF_SIZE, "%s = \\left(%s\\right)", name, coords_buf);
    }

    formula_pool_free(coords_buf);
    return result;
}

/**
 * @brief 渲染线段名称为 LaTeX 字符串
 *
 * @param name 线段名称
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_segment_latex(const char *name) {
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "name is NULL");

    /* HEAP_ALLOCATED: 结果字符串 */
    char *result = (char *) lv_malloc(lv_SEGMENT_LATEX_BUF_SIZE);
    if (result) {
        snprintf(result, lv_SEGMENT_LATEX_BUF_SIZE, "\\overline{%s}", name);
    }
    return result;
}

/**
 * @brief 渲染圆为 LaTeX 字符串
 *
 * @param name   圆名称
 * @param center 圆心名称
 * @param radius 半径 AST 节点
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_circle_latex(const char *name, const char *center, const FormulaNode *radius) {
    if (!name || !center || !radius)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "invalid params for circle latex");

    /* STACK_SAFE: 半径渲染缓冲区 ≤256 字节 */
    char radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};
    formula_render_to_buffer(radius, OUTPUT_LATEX, radius_buf, sizeof(radius_buf));

    /* HEAP_ALLOCATED: 结果字符串 */
    char *result = (char *) lv_malloc(lv_CIRCLE_LATEX_BUF_SIZE);
    if (result) {
        snprintf(result, lv_CIRCLE_LATEX_BUF_SIZE,
                 "\\text{circle } %s \\text{ with center } %s \\text{ and radius } %s", name, center, radius_buf);
    }
    return result;
}

/**
 * @brief 渲染分数为 LaTeX 字符串
 *
 * @param numerator   分子
 * @param denominator 分母
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_fraction_latex(int64_t numerator, uint64_t denominator) {
    /* HEAP_ALLOCATED: 结果字符串 */
    char *result = (char *) lv_malloc(lv_FRACTION_LATEX_BUF_SIZE);
    if (result) {
        snprintf(result, lv_FRACTION_LATEX_BUF_SIZE, "\\frac{%lld}{%llu}", (long long) numerator,
                 (unsigned long long) denominator);
    }
    return result;
}
