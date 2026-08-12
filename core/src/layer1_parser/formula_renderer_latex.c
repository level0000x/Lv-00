/**
 * @file formula_renderer_latex.c
 * @brief LaTeX 渲染后端
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
 * LaTeX 渲染
 * ============================================================ */

/**
 * @brief LaTeX 格式内部渲染器（递归）
 *
 * 所有 >256 字节的子表达式缓冲区均从池或堆获取，
 * 避免递归调用时的栈溢出风险。
 */
int render_latex_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options);

/**
 * @brief 将名称字符串做 LaTeX 特殊字符转义（lv_str_latex_escape 两遍法，堆分配）
 *
 * 转义后长度可能变长（如 \ → \textbackslash{}），故使用两遍法精确分配，
 * 避免固定缓冲区截断破坏 LaTeX 结构。调用者需用 lv_free 释放。
 *
 * @param name 源字符串（可为 NULL）
 * @return 转义后的堆字符串；失败（含 NULL 入参）返回 NULL
 */
static char *latex_escape_alloc(const char *name) {
    return name ? lv_str_latex_escape_alloc(name) : NULL;
}

static int helper_latex_number(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    if (node->data.number.is_integer) {
        /* STACK_SAFE: snprintf 直接写入输出 buffer，无中间缓冲区 */
        written = snprintf(buffer, size, "%lld", (long long) node->data.number.numerator);
    } else {
        /* 分数渲染为 \frac{a}{b} */
        written = snprintf(buffer, size, "\\frac{%lld}{%llu}", (long long) node->data.number.numerator,
                           (unsigned long long) node->data.number.denominator);
    }
    return written;
}

static int helper_latex_variable(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    if (is_greek_letter(node->data.variable.name)) {
        written = snprintf(buffer, size, "%s", formula_latex_greek_name(node->data.variable.name));
    } else {
        written = snprintf(buffer, size, "%s", node->data.variable.name);
    }
    return written;
}

static int helper_latex_identifier(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    (void) options;
    int written = 0;
    /* NODE_IDENTIFIER 名称接入 LaTeX 转义，防止特殊字符破坏 LaTeX 结构 */
    char *esc = latex_escape_alloc(node->data.identifier.name);
    written = snprintf(buffer, size, "%s", esc ? esc : "");
    lv_free((void **) &esc);
    return written;
}

static int helper_latex_binary_add(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_binary_via(node, "%s + %s", RENDER_VIA_CHECK_RET | RENDER_VIA_ERROR_CTX, buffer, size, options,
                             render_latex_internal);
}

static int helper_latex_binary_sub(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    const char *fmt = needs_parentheses(node->data.binary_op.right, NODE_BINARY_OP_SUB, true)
                          ? "%s - \\left(%s\\right)"
                          : "%s - %s";
    return render_binary_via(node, fmt, RENDER_VIA_CHECK_RET, buffer, size, options, render_latex_internal);
}

static int helper_latex_binary_mul(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    const char *fmt = (options && options->implicit_multiplication) ? "%s %s" : "%s \\cdot %s";
    return render_binary_via(node, fmt, RENDER_VIA_CHECK_RET, buffer, size, options, render_latex_internal);
}

static int helper_latex_binary_div(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_binary_via(node, "\\frac{%s}{%s}", RENDER_VIA_CHECK_RET, buffer, size, options,
                             render_latex_internal);
}

static int helper_latex_binary_pow(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    const char *fmt = needs_parentheses(node->data.binary_op.left, NODE_BINARY_OP_POW, false)
                          ? "\\left(%s\\right)^{%s}"
                          : "%s^{%s}";
    return render_binary_via(node, fmt, RENDER_VIA_CHECK_RET, buffer, size, options, render_latex_internal);
}

static int helper_latex_unary_neg(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    bool need_paren = needs_parentheses(node->data.unary_op.operand, NODE_UNARY_OP_NEG, false);
    return render_unary_via(node, need_paren ? "-\\left(" : "-", need_paren ? "\\right)" : "",
                            RENDER_VIA_CHECK_RET | RENDER_VIA_ERROR_CTX, buffer, size, options,
                            render_latex_internal);
}

static int helper_latex_unary_sqrt(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_unary_via(node, "\\sqrt{", "}", RENDER_VIA_CHECK_RET | RENDER_VIA_ERROR_CTX, buffer, size, options,
                            render_latex_internal);
}

static int helper_latex_unary_sin_cos_tan(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    /* 前缀自共享一元函数名表构造："\sin\left(" 等（与历史输出逐字一致） */
    const char *name = formula_unary_fn_name(node);
    if (!name)
        return snprintf(buffer, size, "\\text{<unknown>}");
    char prefix[lv_FORMULA_BUF_SMALL];
    snprintf(prefix, sizeof(prefix), "\\%s\\left(", name);
    return render_unary_via(node, prefix, "\\right)", RENDER_VIA_CHECK_RET | RENDER_VIA_ERROR_CTX, buffer, size,
                            options, render_latex_internal);
}

static int helper_latex_unary_abs(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_unary_via(node, "\\left|", "\\right|", RENDER_VIA_CHECK_RET | RENDER_VIA_ERROR_CTX, buffer, size,
                            options, render_latex_internal);
}

static int helper_latex_unary_ln(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_unary_via(node, "\\ln\\left(", "\\right)", RENDER_VIA_CHECK_RET | RENDER_VIA_ERROR_CTX, buffer,
                            size, options, render_latex_internal);
}

static int helper_latex_unary_log(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_unary_via(node, "\\log\\left(", "\\right)", RENDER_VIA_CHECK_RET | RENDER_VIA_ERROR_CTX, buffer,
                            size, options, render_latex_internal);
}

static int helper_latex_equation(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
    char *lhs_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
    char *rhs_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
    if (!lhs_buf || !rhs_buf) {
        formula_pool_free(lhs_buf);
        formula_pool_free(rhs_buf);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate equation buffers");
    }

    int lhs_ret = render_latex_internal(node->data.equation.lhs, lhs_buf, lv_FORMULA_BUF_SIZE, options);
    int rhs_ret = render_latex_internal(node->data.equation.rhs, rhs_buf, lv_FORMULA_BUF_SIZE, options);
    if (lhs_ret < 0 || rhs_ret < 0) {
        formula_pool_free(lhs_buf);
        formula_pool_free(rhs_buf);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "equation sub-render failed");
    }

    written = snprintf(buffer, size, "%s = %s", lhs_buf, rhs_buf);

    formula_pool_free(lhs_buf);
    formula_pool_free(rhs_buf);
    return written;
}

static int helper_latex_geom_point(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* HEAP_ALLOCATED: 大型坐标缓冲区使用直接 malloc */
    char *coords_buf = (char *) lv_malloc(lv_FORMULA_BUF_LARGE);
    if (!coords_buf)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate coords buffer");
    memset(coords_buf, 0, lv_FORMULA_BUF_LARGE);

    if (node->data.geom_point.coords) {
        int coords_ret =
            render_latex_internal(node->data.geom_point.coords, coords_buf, lv_FORMULA_BUF_LARGE, options);
        if (coords_ret < 0) {
            lv_free((void **) &coords_buf);
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "coords sub-render failed");
        }
    }

    if (node->data.geom_point.name) {
        char *esc_name = latex_escape_alloc(node->data.geom_point.name);
        written = snprintf(buffer, size, "%s = \\left(%s\\right)", esc_name ? esc_name : "", coords_buf);
        lv_free((void **) &esc_name);
    } else {
        written = snprintf(buffer, size, "\\left(%s\\right)", coords_buf);
    }

    lv_free((void **) &coords_buf);
    return written;
}

static int helper_latex_geom_segment(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    if (node->data.geom_segment.name) {
        char *esc_name = latex_escape_alloc(node->data.geom_segment.name);
        written = snprintf(buffer, size, "\\overline{%s}", esc_name ? esc_name : "");
        lv_free((void **) &esc_name);
    } else {
        /* STACK_SAFE: 端点名缓冲区 ≤64 字节，使用 snprintf 边界检查 */
        char ep1_buf[lv_FORMULA_BUF_SMALL] = {0};
        char ep2_buf[lv_FORMULA_BUF_SMALL] = {0};
        if (node->data.geom_segment.endpoint1) {
            int ep_ret =
                render_latex_internal(node->data.geom_segment.endpoint1, ep1_buf, sizeof(ep1_buf), options);
            if (ep_ret < 0)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "endpoint1 sub-render failed");
        }
        if (node->data.geom_segment.endpoint2) {
            int ep_ret =
                render_latex_internal(node->data.geom_segment.endpoint2, ep2_buf, sizeof(ep2_buf), options);
            if (ep_ret < 0)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "endpoint2 sub-render failed");
        }
        written = snprintf(buffer, size, "\\overline{%s%s}", ep1_buf, ep2_buf);
    }
    return written;
}

static int helper_latex_geom_circle(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 小型缓冲区 ≤256 字节，使用 snprintf 边界检查 */
    char center_buf[lv_FORMULA_BUF_SMALL] = {0};
    char radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};

    if (node->data.geom_circle.center) {
        if (node->data.geom_circle.center->type == NODE_IDENTIFIER) {
            char *esc_name = latex_escape_alloc(node->data.geom_circle.center->data.identifier.name);
            snprintf(center_buf, sizeof(center_buf), "%s", esc_name ? esc_name : "");
            lv_free((void **) &esc_name);
        } else {
            int center_ret =
                render_latex_internal(node->data.geom_circle.center, center_buf, sizeof(center_buf), options);
            if (center_ret < 0)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "center sub-render failed");
        }
    }

    if (node->data.geom_circle.radius) {
        int radius_ret =
            render_latex_internal(node->data.geom_circle.radius, radius_buf, sizeof(radius_buf), options);
        if (radius_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "radius sub-render failed");
    }

    {
        const char *circle_name = node->data.geom_circle.name ? node->data.geom_circle.name : "O";
        char *esc_name = latex_escape_alloc(circle_name);
        written = snprintf(buffer, size, "\\text{circle } %s \\text{ with center } %s \\text{ and radius } %s",
                           esc_name ? esc_name : circle_name, center_buf, radius_buf);
        lv_free((void **) &esc_name);
    }
    return written;
}

static int helper_latex_geom_triangle(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    if (node->data.geom_triangle.name) {
        char *esc_name = latex_escape_alloc(node->data.geom_triangle.name);
        written = snprintf(buffer, size, "\\triangle %s", esc_name ? esc_name : "");
        lv_free((void **) &esc_name);
    } else {
        written = snprintf(buffer, size, "\\triangle");
    }
    return written;
}

static int helper_latex_coord_list(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    size_t pos = 0;

    for (int i = 0; i < node->data.coord_list.coord_count; i++) {
        /* STACK_SAFE: 坐标缓冲区 ≤256 字节 */
        char coord_buf[lv_FORMULA_BUF_MEDIUM] = {0};
        int coord_ret =
            render_latex_internal(node->data.coord_list.coords[i], coord_buf, sizeof(coord_buf), options);
        if (coord_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "coord sub-render failed");

        if (!lv_str_append_sep(buffer, size, &pos, ", ", coord_buf))
            break;
    }
    return (int) pos;
}

static int helper_latex_constraint_perpendicular(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 约束参与者名 ≤64 字节 */
    char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
         p3_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 3) {
        int p1_ret =
            render_latex_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
        int p2_ret =
            render_latex_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
        int p3_ret =
            render_latex_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
        if (p1_ret < 0 || p2_ret < 0 || p3_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "participant render failed");
        written = snprintf(buffer, size, "%s \\perp %s%s", p1_buf, p2_buf, p3_buf);
    }
    return written;
}

static int helper_latex_constraint_parallel(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 线名缓冲区 ≤64 字节 */
    char l1_buf[lv_FORMULA_BUF_SMALL] = {0}, l2_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 2) {
        int l1_ret =
            render_latex_internal(node->data.constraint.participants[0], l1_buf, sizeof(l1_buf), options);
        int l2_ret =
            render_latex_internal(node->data.constraint.participants[1], l2_buf, sizeof(l2_buf), options);
        if (l1_ret < 0 || l2_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "line render failed");
        written = snprintf(buffer, size, "%s \\parallel %s", l1_buf, l2_buf);
    }
    return written;
}

static int helper_latex_constraint_midpoint(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
    char m_buf[lv_FORMULA_BUF_SMALL] = {0}, a_buf[lv_FORMULA_BUF_SMALL] = {0},
         b_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 3) {
        int m_ret = render_latex_internal(node->data.constraint.participants[0], m_buf, sizeof(m_buf), options);
        int a_ret = render_latex_internal(node->data.constraint.participants[1], a_buf, sizeof(a_buf), options);
        int b_ret = render_latex_internal(node->data.constraint.participants[2], b_buf, sizeof(b_buf), options);
        if (m_ret < 0 || a_ret < 0 || b_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "midpoint participant render failed");
        written = snprintf(buffer, size, "%s = \\text{midpoint}(%s, %s)", m_buf, a_buf, b_buf);
    }
    return written;
}

/* NODE_GEOM_REGION 区域渲染 */
static int helper_latex_geom_region(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    const char *name = node->data.geom_region.name ? node->data.geom_region.name : "R";
    written = snprintf(buffer, size, "\\text{region } %s", name);
    return written;
}

/* NODE_GEOM_ARC 弧渲染 */
static int helper_latex_geom_arc(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 名称/角度缓冲区 ≤64，半径缓冲区 ≤256 */
    char center_buf[lv_FORMULA_BUF_SMALL] = {0}, radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};
    char start_buf[lv_FORMULA_BUF_SMALL] = {0}, end_buf[lv_FORMULA_BUF_SMALL] = {0};

    if (node->data.geom_arc.center) {
        int center_ret =
            render_latex_internal(node->data.geom_arc.center, center_buf, sizeof(center_buf), options);
        if (center_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "center render failed");
    }
    if (node->data.geom_arc.radius) {
        int radius_ret =
            render_latex_internal(node->data.geom_arc.radius, radius_buf, sizeof(radius_buf), options);
        if (radius_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "arc radius sub-render failed");
    }
    if (node->data.geom_arc.start_angle) {
        int start_ret =
            render_latex_internal(node->data.geom_arc.start_angle, start_buf, sizeof(start_buf), options);
        if (start_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "arc start_angle sub-render failed");
    }
    if (node->data.geom_arc.end_angle) {
        int end_ret = render_latex_internal(node->data.geom_arc.end_angle, end_buf, sizeof(end_buf), options);
        if (end_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "arc end_angle sub-render failed");
    }

    written = snprintf(buffer, size,
                       "\\overset{\\frown}{%s} \\text{ with center } %s, \\text{ radius } %s, \\text{ from } "
                       "%s \\text{ to } %s",
                       node->data.geom_arc.name ? node->data.geom_arc.name : "AB", center_buf, radius_buf,
                       start_buf, end_buf);
    return written;
}

/* NODE_CONSTRAINT_ANGLE 角度约束渲染 */
static int helper_latex_constraint_angle(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
    char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
         p3_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 3) {
        int p1_ret =
            render_latex_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
        int p2_ret =
            render_latex_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
        int p3_ret =
            render_latex_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
        if (p1_ret < 0 || p2_ret < 0 || p3_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "angle participant sub-render failed");
        written = snprintf(buffer, size, "\\angle %s %s %s", p1_buf, p2_buf, p3_buf);
    }
    return written;
}

/* NODE_GEOM_LINE 直线渲染 */
static int helper_latex_geom_line(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 端点名缓冲区 ≤64 字节 */
    char p1_buf[lv_FORMULA_BUF_SMALL] = {0};
    char p2_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.geom_line.point1) {
        int p1_ret = render_latex_internal(node->data.geom_line.point1, p1_buf, sizeof(p1_buf), options);
        if (p1_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "line point1 sub-render failed");
    }
    if (node->data.geom_line.point2) {
        int p2_ret = render_latex_internal(node->data.geom_line.point2, p2_buf, sizeof(p2_buf), options);
        if (p2_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "line point2 sub-render failed");
    }
    if (node->data.geom_line.name) {
        char *esc_name = latex_escape_alloc(node->data.geom_line.name);
        written = snprintf(buffer, size, "\\text{line } %s", esc_name ? esc_name : "");
        lv_free((void **) &esc_name);
    } else {
        written = snprintf(buffer, size, "\\overleftrightarrow{%s%s}", p1_buf, p2_buf);
    }
    return written;
}

/* NODE_GEOM_VECTOR 向量渲染 */
static int helper_latex_geom_vector(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 端点名缓冲区 ≤64 字节 */
    char s_buf[lv_FORMULA_BUF_SMALL] = {0};
    char e_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.geom_vector.start) {
        int s_ret = render_latex_internal(node->data.geom_vector.start, s_buf, sizeof(s_buf), options);
        if (s_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "vector start sub-render failed");
    }
    if (node->data.geom_vector.end) {
        int e_ret = render_latex_internal(node->data.geom_vector.end, e_buf, sizeof(e_buf), options);
        if (e_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "vector end sub-render failed");
    }
    if (node->data.geom_vector.name) {
        char *esc_name = latex_escape_alloc(node->data.geom_vector.name);
        written = snprintf(buffer, size, "\\vec{%s}", esc_name ? esc_name : "");
        lv_free((void **) &esc_name);
    } else {
        written = snprintf(buffer, size, "\\overrightarrow{%s%s}", s_buf, e_buf);
    }
    return written;
}

/* NODE_CONSTRAINT_BISECTOR 角平分线约束渲染 */
static int helper_latex_constraint_bisector(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 参与者名缓冲区 ≤64 字节 */
    char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
         p3_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 3) {
        int p1_ret =
            render_latex_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
        int p2_ret =
            render_latex_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
        int p3_ret =
            render_latex_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
        if (p1_ret < 0 || p2_ret < 0 || p3_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "bisector participant render failed");
        written = snprintf(buffer, size, "\\text{bisector}(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
    }
    return written;
}

/* NODE_CONSTRAINT_COLLINEAR 共线约束渲染 */
static int helper_latex_constraint_collinear(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 参与者名缓冲区 ≤64 字节 */
    char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
         p3_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 3) {
        int p1_ret =
            render_latex_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
        int p2_ret =
            render_latex_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
        int p3_ret =
            render_latex_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
        if (p1_ret < 0 || p2_ret < 0 || p3_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "collinear participant render failed");
        written = snprintf(buffer, size, "\\text{collinear}(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
    }
    return written;
}

/* NODE_CONSTRAINT_TANGENT 相切约束渲染 */
static int helper_latex_constraint_tangent(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 参与者名缓冲区 ≤64 字节 */
    char l_buf[lv_FORMULA_BUF_SMALL] = {0}, c_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 2) {
        int l_ret = render_latex_internal(node->data.constraint.participants[0], l_buf, sizeof(l_buf), options);
        int c_ret = render_latex_internal(node->data.constraint.participants[1], c_buf, sizeof(c_buf), options);
        if (l_ret < 0 || c_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "tangent participant render failed");
        written = snprintf(buffer, size, "%s \\text{ tangent to } %s", l_buf, c_buf);
    }
    return written;
}

/* NODE_CONSTRAINT_CONGRUENT 全等约束渲染 */
static int helper_latex_constraint_congruent(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 参与者名缓冲区 ≤64 字节 */
    char s1_buf[lv_FORMULA_BUF_SMALL] = {0}, s2_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 2) {
        int s1_ret =
            render_latex_internal(node->data.constraint.participants[0], s1_buf, sizeof(s1_buf), options);
        int s2_ret =
            render_latex_internal(node->data.constraint.participants[1], s2_buf, sizeof(s2_buf), options);
        if (s1_ret < 0 || s2_ret < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "congruent participant render failed");
        written = snprintf(buffer, size, "%s \\cong %s", s1_buf, s2_buf);
    }
    return written;
}

static int helper_latex_compound(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* lvStrBuf 动态构建（自动扩容），消除游标式 snprintf 的静默截断；
     * 首遍探测（buffer==NULL）与第二遍写入（buffer 非 NULL）行为一致 */
    lvStrBuf sb;
    lv_strbuf_init(&sb);

    for (int i = 0; i < node->data.compound.statement_count; i++) {
        /* HEAP_ALLOCATED: 池分配语句缓冲区 */
        char *stmt_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
        if (!stmt_buf) {
            lv_strbuf_destroy(&sb);
            return 0;
        }

        int stmt_ret =
            render_latex_internal(node->data.compound.statements[i], stmt_buf, lv_FORMULA_BUF_SIZE, options);
        if (stmt_ret < 0) {
            formula_pool_free(stmt_buf);
            lv_strbuf_destroy(&sb);
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "compound sub-render failed");
        }

        lv_strbuf_printf(&sb, "%s\\\\\n", stmt_buf);
        formula_pool_free(stmt_buf);
    }

    /* 按 snprintf 语义写入调用方缓冲区：空间不足时截断安全，返回完整长度
     * （formula_render_ex 两遍法中首遍返回完整长度触发扩容，第二遍容量充足） */
    written = (int) sb.len;
    if (buffer && size > 0) {
        lv_strlcpy_n(buffer, size, sb.data, (size_t) sb.len);
    }
    lv_strbuf_destroy(&sb);
    return written;
}

static const RenderNodeFunc s_render_latex_funcs[] = {
    [NODE_NUMBER] = helper_latex_number,
    [NODE_VARIABLE] = helper_latex_variable,
    [NODE_IDENTIFIER] = helper_latex_identifier,
    [NODE_BINARY_OP_ADD] = helper_latex_binary_add,
    [NODE_BINARY_OP_SUB] = helper_latex_binary_sub,
    [NODE_BINARY_OP_MUL] = helper_latex_binary_mul,
    [NODE_BINARY_OP_DIV] = helper_latex_binary_div,
    [NODE_BINARY_OP_POW] = helper_latex_binary_pow,
    [NODE_UNARY_OP_NEG] = helper_latex_unary_neg,
    [NODE_UNARY_OP_SQRT] = helper_latex_unary_sqrt,
    [NODE_UNARY_OP_SIN] = helper_latex_unary_sin_cos_tan,
    [NODE_UNARY_OP_COS] = helper_latex_unary_sin_cos_tan,
    [NODE_UNARY_OP_TAN] = helper_latex_unary_sin_cos_tan,
    [NODE_UNARY_OP_ABS] = helper_latex_unary_abs,
    [NODE_UNARY_OP_LN] = helper_latex_unary_ln,
    [NODE_UNARY_OP_LOG] = helper_latex_unary_log,
    [NODE_EQUATION] = helper_latex_equation,
    [NODE_GEOM_POINT] = helper_latex_geom_point,
    [NODE_GEOM_SEGMENT] = helper_latex_geom_segment,
    [NODE_GEOM_LINE] = helper_latex_geom_line,
    [NODE_GEOM_CIRCLE] = helper_latex_geom_circle,
    [NODE_GEOM_TRIANGLE] = helper_latex_geom_triangle,
    [NODE_COORDINATE_LIST] = helper_latex_coord_list,
    [NODE_CONSTRAINT_PERPENDICULAR] = helper_latex_constraint_perpendicular,
    [NODE_CONSTRAINT_PARALLEL] = helper_latex_constraint_parallel,
    [NODE_CONSTRAINT_MIDPOINT] = helper_latex_constraint_midpoint,
    [NODE_CONSTRAINT_BISECTOR] = helper_latex_constraint_bisector,
    [NODE_CONSTRAINT_COLLINEAR] = helper_latex_constraint_collinear,
    [NODE_CONSTRAINT_TANGENT] = helper_latex_constraint_tangent,
    [NODE_CONSTRAINT_CONGRUENT] = helper_latex_constraint_congruent,
    [NODE_GEOM_REGION] = helper_latex_geom_region,
    [NODE_GEOM_ARC] = helper_latex_geom_arc,
    [NODE_GEOM_VECTOR] = helper_latex_geom_vector,
    [NODE_CONSTRAINT_ANGLE] = helper_latex_constraint_angle,
    [NODE_COMPOUND] = helper_latex_compound,
};

static int helper_latex_unknown(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return snprintf(buffer, size, "\\text{<unknown>}");
}

int render_latex_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    return dispatch_via(node, buffer, size, options, s_render_latex_funcs, lv_ARRAY_SIZE(s_render_latex_funcs),
                        helper_latex_unknown);
}

