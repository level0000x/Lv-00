/**
 * @file formula_renderer_python.c
 * @brief Python 渲染后端
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
 * Python 渲染
 * ============================================================ */

/**
 * @brief Python 格式内部渲染器（递归）
 *
 * 所有 >256 字节的子表达式缓冲区均从池或堆获取。
 */

static int helper_python_number(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    if (node->data.number.is_integer) {
        written = snprintf(buffer, size, "%lld", (long long) node->data.number.numerator);
    } else {
        if (options && options->fraction_mode) {
            written = snprintf(buffer, size, "Fraction(%lld, %llu)", (long long) node->data.number.numerator,
                               (unsigned long long) node->data.number.denominator);
        } else {
            if (node->data.number.denominator == 0) {
                written = snprintf(buffer, size, "NaN");
            } else {
                double val = (double) node->data.number.numerator / (double) node->data.number.denominator;
                written = snprintf(buffer, size, "%.*f", options ? options->precision : 6, val);
            }
        }
    }
    return written;
}

static int helper_python_variable(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    written = (int) lv_strlcpy(buffer, node->data.variable.name, size);
    return written;
}

static int helper_python_identifier(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    written = (int) lv_strlcpy(buffer, node->data.identifier.name, size);
    return written;
}

/* Python 二元/一元算子模板表（格式串为外部契约，内容与历史输出逐字一致） */
static const RenderBinarySpec s_python_binary_specs[] = {
    [NODE_BINARY_OP_ADD] = {"(%s + %s)", 0},
    [NODE_BINARY_OP_SUB] = {"(%s - %s)", 0},
    [NODE_BINARY_OP_MUL] = {"(%s * %s)", 0},
    [NODE_BINARY_OP_DIV] = {"(%s / %s)", 0},
    [NODE_BINARY_OP_POW] = {"(%s ** %s)", 0},
};

static const RenderUnarySpec s_python_unary_specs[] = {
    [NODE_UNARY_OP_NEG] = {"(-", ")", 0},
    [NODE_UNARY_OP_SQRT] = {"sqrt(", ")", 0},
    [NODE_UNARY_OP_ABS] = {"abs(", ")", 0},
    [NODE_UNARY_OP_LN] = {"log(", ")", 0},
    [NODE_UNARY_OP_LOG] = {"log10(", ")", 0},
};

/* 前缀自共享一元函数名表构造："sin(" 等（与历史输出逐字一致） */
static const RenderFnNameSpec s_python_fn_name_spec = {"%s(", ")", 0, "# <unknown>"};

static int helper_python_binary(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_binary_spec(node, s_python_binary_specs, lv_ARRAY_SIZE(s_python_binary_specs), buffer, size, options,
                              render_python_internal);
}

static int helper_python_unary(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_unary_spec(node, s_python_unary_specs, lv_ARRAY_SIZE(s_python_unary_specs), buffer, size, options,
                             render_python_internal);
}

static int helper_python_fn_name(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_fn_name_spec(node, &s_python_fn_name_spec, buffer, size, options, render_python_internal);
}

static int helper_python_equation(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
    char *lhs_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
    char *rhs_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
    if (!lhs_buf || !rhs_buf) {
        formula_pool_free(lhs_buf);
        formula_pool_free(rhs_buf);
        return -1;
    }

    render_python_internal(node->data.equation.lhs, lhs_buf, lv_FORMULA_BUF_SIZE, options);
    render_python_internal(node->data.equation.rhs, rhs_buf, lv_FORMULA_BUF_SIZE, options);

    /* 方程转换为比较表达式 */
    written = snprintf(buffer, size, "(%s == %s)", lhs_buf, rhs_buf);

    formula_pool_free(lhs_buf);
    formula_pool_free(rhs_buf);
    return written;
}

static int helper_python_geom_point(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* HEAP_ALLOCATED: 大型坐标缓冲区使用直接 malloc */
    char *coords_buf = (char *) lv_malloc(lv_FORMULA_BUF_LARGE);
    if (!coords_buf)
        return -1;
    memset(coords_buf, 0, lv_FORMULA_BUF_LARGE);

    if (node->data.geom_point.coords) {
        render_python_internal(node->data.geom_point.coords, coords_buf, lv_FORMULA_BUF_LARGE, options);
    }

    if (node->data.geom_point.name) {
        written = snprintf(buffer, size, "%s = Point(%s)", node->data.geom_point.name, coords_buf);
    } else {
        written = snprintf(buffer, size, "Point(%s)", coords_buf);
    }

    lv_free((void **) &coords_buf);
    return written;
}

static int helper_python_geom_segment(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 端点名缓冲区 ≤64 字节 */
    char ep1_buf[lv_FORMULA_BUF_SMALL] = {0};
    char ep2_buf[lv_FORMULA_BUF_SMALL] = {0};

    if (node->data.geom_segment.endpoint1) {
        render_python_internal(node->data.geom_segment.endpoint1, ep1_buf, sizeof(ep1_buf), options);
    }
    if (node->data.geom_segment.endpoint2) {
        render_python_internal(node->data.geom_segment.endpoint2, ep2_buf, sizeof(ep2_buf), options);
    }

    written = snprintf(buffer, size, "Segment(%s, %s)", ep1_buf, ep2_buf);
    return written;
}

static int helper_python_geom_circle(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 小型缓冲区 ≤256 字节 */
    char center_buf[lv_FORMULA_BUF_SMALL] = {0};
    char radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};

    if (node->data.geom_circle.center) {
        render_python_internal(node->data.geom_circle.center, center_buf, sizeof(center_buf), options);
    }
    if (node->data.geom_circle.radius) {
        render_python_internal(node->data.geom_circle.radius, radius_buf, sizeof(radius_buf), options);
    }

    written = snprintf(buffer, size, "Circle(%s, %s)", center_buf, radius_buf);
    return written;
}

static int helper_python_geom_triangle(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 顶点名缓冲区 ≤64 字节 */
    char v1_buf[lv_FORMULA_BUF_SMALL] = {0}, v2_buf[lv_FORMULA_BUF_SMALL] = {0},
         v3_buf[lv_FORMULA_BUF_SMALL] = {0};

    if (node->data.geom_triangle.vertex1) {
        render_python_internal(node->data.geom_triangle.vertex1, v1_buf, sizeof(v1_buf), options);
    }
    if (node->data.geom_triangle.vertex2) {
        render_python_internal(node->data.geom_triangle.vertex2, v2_buf, sizeof(v2_buf), options);
    }
    if (node->data.geom_triangle.vertex3) {
        render_python_internal(node->data.geom_triangle.vertex3, v3_buf, sizeof(v3_buf), options);
    }

    written = snprintf(buffer, size, "Triangle(%s, %s, %s)", v1_buf, v2_buf, v3_buf);
    return written;
}

static int helper_python_coord_list(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    size_t pos = 0;

    for (int i = 0; i < node->data.coord_list.coord_count; i++) {
        /* STACK_SAFE: 坐标缓冲区 ≤256 字节 */
        char coord_buf[lv_FORMULA_BUF_MEDIUM] = {0};
        render_python_internal(node->data.coord_list.coords[i], coord_buf, sizeof(coord_buf), options);

        if (!lv_str_append_sep(buffer, size, &pos, ", ", coord_buf))
            break;
    }
    return (int) pos;
}

static int helper_python_constraint_perpendicular(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
    char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
         p3_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 3) {
        render_python_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
        render_python_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
        render_python_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
        written = snprintf(buffer, size, "perpendicular(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
    }
    return written;
}

static int helper_python_constraint_parallel(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 线名缓冲区 ≤64 字节 */
    char l1_buf[lv_FORMULA_BUF_SMALL] = {0}, l2_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 2) {
        render_python_internal(node->data.constraint.participants[0], l1_buf, sizeof(l1_buf), options);
        render_python_internal(node->data.constraint.participants[1], l2_buf, sizeof(l2_buf), options);
        written = snprintf(buffer, size, "parallel(%s, %s)", l1_buf, l2_buf);
    }
    return written;
}

static int helper_python_constraint_midpoint(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
    char m_buf[lv_FORMULA_BUF_SMALL] = {0}, a_buf[lv_FORMULA_BUF_SMALL] = {0},
         b_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 3) {
        render_python_internal(node->data.constraint.participants[0], m_buf, sizeof(m_buf), options);
        render_python_internal(node->data.constraint.participants[1], a_buf, sizeof(a_buf), options);
        render_python_internal(node->data.constraint.participants[2], b_buf, sizeof(b_buf), options);
        written = snprintf(buffer, size, "%s = midpoint(%s, %s)", m_buf, a_buf, b_buf);
    }
    return written;
}

/* NODE_GEOM_REGION 区域渲染 */
static int helper_python_geom_region(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    const char *name = node->data.geom_region.name ? node->data.geom_region.name : "R";
    written = snprintf(buffer, size, "Region('%s')", name);
    return written;
}

/* NODE_GEOM_ARC 弧渲染 */
static int helper_python_geom_arc(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 名称/角度缓冲区 ≤64，半径缓冲区 ≤256 */
    char center_buf[lv_FORMULA_BUF_SMALL] = {0}, radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};
    char start_buf[lv_FORMULA_BUF_SMALL] = {0}, end_buf[lv_FORMULA_BUF_SMALL] = {0};

    if (node->data.geom_arc.center) {
        render_python_internal(node->data.geom_arc.center, center_buf, sizeof(center_buf), options);
    }
    if (node->data.geom_arc.radius) {
        render_python_internal(node->data.geom_arc.radius, radius_buf, sizeof(radius_buf), options);
    }
    if (node->data.geom_arc.start_angle) {
        render_python_internal(node->data.geom_arc.start_angle, start_buf, sizeof(start_buf), options);
    }
    if (node->data.geom_arc.end_angle) {
        render_python_internal(node->data.geom_arc.end_angle, end_buf, sizeof(end_buf), options);
    }

    written = snprintf(buffer, size, "Arc('%s', center=%s, radius=%s, start_angle=%s, end_angle=%s)",
                       node->data.geom_arc.name ? node->data.geom_arc.name : "AB", center_buf, radius_buf,
                       start_buf, end_buf);
    return written;
}

/* NODE_CONSTRAINT_ANGLE 角度约束渲染 */
static int helper_python_constraint_angle(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
    char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
         p3_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 3) {
        render_python_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
        render_python_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
        render_python_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
        written = snprintf(buffer, size, "angle(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
    }
    return written;
}

/* NODE_GEOM_LINE 直线渲染 */
static int helper_python_geom_line(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 端点名缓冲区 ≤64 字节 */
    char p1_buf[lv_FORMULA_BUF_SMALL] = {0};
    char p2_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.geom_line.point1) {
        render_python_internal(node->data.geom_line.point1, p1_buf, sizeof(p1_buf), options);
    }
    if (node->data.geom_line.point2) {
        render_python_internal(node->data.geom_line.point2, p2_buf, sizeof(p2_buf), options);
    }
    written = snprintf(buffer, size, "Line(%s, %s)", p1_buf, p2_buf);
    return written;
}

/* NODE_GEOM_VECTOR 向量渲染 */
static int helper_python_geom_vector(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 端点名缓冲区 ≤64 字节 */
    char s_buf[lv_FORMULA_BUF_SMALL] = {0};
    char e_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.geom_vector.start) {
        render_python_internal(node->data.geom_vector.start, s_buf, sizeof(s_buf), options);
    }
    if (node->data.geom_vector.end) {
        render_python_internal(node->data.geom_vector.end, e_buf, sizeof(e_buf), options);
    }
    written = snprintf(buffer, size, "Vector(%s, %s)", s_buf, e_buf);
    return written;
}

/* NODE_CONSTRAINT_BISECTOR 角平分线约束渲染 */
static int helper_python_constraint_bisector(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 参与者名缓冲区 ≤64 字节 */
    char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
         p3_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 3) {
        render_python_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
        render_python_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
        render_python_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
        written = snprintf(buffer, size, "bisector(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
    }
    return written;
}

/* NODE_CONSTRAINT_COLLINEAR 共线约束渲染 */
static int helper_python_constraint_collinear(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 参与者名缓冲区 ≤64 字节 */
    char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
         p3_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 3) {
        render_python_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
        render_python_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
        render_python_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
        written = snprintf(buffer, size, "collinear(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
    }
    return written;
}

/* NODE_CONSTRAINT_TANGENT 相切约束渲染 */
static int helper_python_constraint_tangent(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 参与者名缓冲区 ≤64 字节 */
    char l_buf[lv_FORMULA_BUF_SMALL] = {0}, c_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 2) {
        render_python_internal(node->data.constraint.participants[0], l_buf, sizeof(l_buf), options);
        render_python_internal(node->data.constraint.participants[1], c_buf, sizeof(c_buf), options);
        written = snprintf(buffer, size, "tangent(%s, %s)", l_buf, c_buf);
    }
    return written;
}

/* NODE_CONSTRAINT_CONGRUENT 全等约束渲染 */
static int helper_python_constraint_congruent(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 参与者名缓冲区 ≤64 字节 */
    char s1_buf[lv_FORMULA_BUF_SMALL] = {0}, s2_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 2) {
        render_python_internal(node->data.constraint.participants[0], s1_buf, sizeof(s1_buf), options);
        render_python_internal(node->data.constraint.participants[1], s2_buf, sizeof(s2_buf), options);
        written = snprintf(buffer, size, "congruent(%s, %s)", s1_buf, s2_buf);
    }
    return written;
}

static int helper_python_compound(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    char *ptr = buffer;
    size_t remaining = size;
    int total = 0;

    for (int i = 0; i < node->data.compound.statement_count; i++) {
        /* HEAP_ALLOCATED: 池分配语句缓冲区 */
        char *stmt_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
        if (!stmt_buf)
            break;

        render_python_internal(node->data.compound.statements[i], stmt_buf, lv_FORMULA_BUF_SIZE, options);

        int w = snprintf(ptr, remaining, "%s\n", stmt_buf);
        formula_pool_free(stmt_buf);

        /* exempt: w 为 snprintf 返回码（n<0||n>=remaining 是返回码检测，非索引越界），
         * 保持原样，不得替换为 lv_index_in_range */
        if (w < 0 || (size_t) w >= remaining)
            break;
        ptr += w;
        remaining -= w;
        total += w;
    }
    written = total;
    return written;
}

static const RenderNodeFunc s_render_python_funcs[] = {
    [NODE_NUMBER] = helper_python_number,
    [NODE_VARIABLE] = helper_python_variable,
    [NODE_IDENTIFIER] = helper_python_identifier,
    [NODE_BINARY_OP_ADD] = helper_python_binary,
    [NODE_BINARY_OP_SUB] = helper_python_binary,
    [NODE_BINARY_OP_MUL] = helper_python_binary,
    [NODE_BINARY_OP_DIV] = helper_python_binary,
    [NODE_BINARY_OP_POW] = helper_python_binary,
    [NODE_UNARY_OP_NEG] = helper_python_unary,
    [NODE_UNARY_OP_SQRT] = helper_python_unary,
    [NODE_UNARY_OP_SIN] = helper_python_fn_name,
    [NODE_UNARY_OP_COS] = helper_python_fn_name,
    [NODE_UNARY_OP_TAN] = helper_python_fn_name,
    [NODE_UNARY_OP_ABS] = helper_python_unary,
    [NODE_UNARY_OP_LN] = helper_python_unary,
    [NODE_UNARY_OP_LOG] = helper_python_unary,
    [NODE_EQUATION] = helper_python_equation,
    [NODE_GEOM_POINT] = helper_python_geom_point,
    [NODE_GEOM_SEGMENT] = helper_python_geom_segment,
    [NODE_GEOM_LINE] = helper_python_geom_line,
    [NODE_GEOM_CIRCLE] = helper_python_geom_circle,
    [NODE_GEOM_TRIANGLE] = helper_python_geom_triangle,
    [NODE_COORDINATE_LIST] = helper_python_coord_list,
    [NODE_CONSTRAINT_PERPENDICULAR] = helper_python_constraint_perpendicular,
    [NODE_CONSTRAINT_PARALLEL] = helper_python_constraint_parallel,
    [NODE_CONSTRAINT_MIDPOINT] = helper_python_constraint_midpoint,
    [NODE_CONSTRAINT_BISECTOR] = helper_python_constraint_bisector,
    [NODE_CONSTRAINT_COLLINEAR] = helper_python_constraint_collinear,
    [NODE_CONSTRAINT_TANGENT] = helper_python_constraint_tangent,
    [NODE_CONSTRAINT_CONGRUENT] = helper_python_constraint_congruent,
    [NODE_GEOM_REGION] = helper_python_geom_region,
    [NODE_GEOM_ARC] = helper_python_geom_arc,
    [NODE_GEOM_VECTOR] = helper_python_geom_vector,
    [NODE_CONSTRAINT_ANGLE] = helper_python_constraint_angle,
    [NODE_COMPOUND] = helper_python_compound,
};

static int helper_python_unknown(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return snprintf(buffer, size, "# <unknown>");
}

int render_python_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    return dispatch_via(node, buffer, size, options, s_render_python_funcs, lv_ARRAY_SIZE(s_render_python_funcs),
                        helper_python_unknown);
}

