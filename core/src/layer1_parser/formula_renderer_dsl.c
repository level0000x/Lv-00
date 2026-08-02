/**
 * @file formula_renderer_dsl.c
 * @brief DSL 渲染后端
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
 * DSL 渲染
 * ============================================================ */

/**
 * @brief DSL 格式内部渲染器（递归）
 *
 * 所有 >256 字节的子表达式缓冲区均从池或堆获取。
 */

static int helper_dsl_number(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    if (node->data.number.is_integer) {
        written = snprintf(buffer, size, "%lld", (long long) node->data.number.numerator);
    } else {
        written = snprintf(buffer, size, "%lld/%llu", (long long) node->data.number.numerator,
                           (unsigned long long) node->data.number.denominator);
    }
    return written;
}

static int helper_dsl_variable(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    written = snprintf(buffer, size, "%s", node->data.variable.name);
    return written;
}

static int helper_dsl_identifier(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    written = snprintf(buffer, size, "%s", node->data.identifier.name);
    return written;
}

static int helper_dsl_binary_add(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_binary_via(node, "%s + %s", 0, buffer, size, options, render_dsl_internal);
}

static int helper_dsl_binary_sub(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_binary_via(node, "%s - %s", 0, buffer, size, options, render_dsl_internal);
}

static int helper_dsl_binary_mul(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_binary_via(node, "%s * %s", 0, buffer, size, options, render_dsl_internal);
}

static int helper_dsl_binary_div(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_binary_via(node, "%s / %s", 0, buffer, size, options, render_dsl_internal);
}

static int helper_dsl_binary_pow(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_binary_via(node, "%s ^ %s", 0, buffer, size, options, render_dsl_internal);
}

static int helper_dsl_unary_neg(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_unary_via(node, "-", "", 0, buffer, size, options, render_dsl_internal);
}

static int helper_dsl_unary_sqrt(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_unary_via(node, "sqrt(", ")", 0, buffer, size, options, render_dsl_internal);
}

static int helper_dsl_unary_sin_cos_tan(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    static const char *const prefixes[] = {
        [NODE_UNARY_OP_SIN - NODE_UNARY_OP_NEG] = "sin(",
        [NODE_UNARY_OP_COS - NODE_UNARY_OP_NEG] = "cos(",
        [NODE_UNARY_OP_TAN - NODE_UNARY_OP_NEG] = "tan(",
    };
    return render_unary_via(node, formula_render_trig_name(node, prefixes, lv_ARRAY_SIZE(prefixes)), ")", 0, buffer,
                            size, options, render_dsl_internal);
}

static int helper_dsl_unary_abs(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_unary_via(node, "abs(", ")", 0, buffer, size, options, render_dsl_internal);
}

static int helper_dsl_equation(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
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

    render_dsl_internal(node->data.equation.lhs, lhs_buf, lv_FORMULA_BUF_SIZE, options);
    render_dsl_internal(node->data.equation.rhs, rhs_buf, lv_FORMULA_BUF_SIZE, options);

    written = snprintf(buffer, size, "%s = %s", lhs_buf, rhs_buf);

    formula_pool_free(lhs_buf);
    formula_pool_free(rhs_buf);
    return written;
}

static int helper_dsl_geom_point(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* HEAP_ALLOCATED: 大型坐标缓冲区使用直接 malloc */
    char *coords_buf = (char *) lv_malloc(lv_FORMULA_BUF_LARGE);
    if (!coords_buf)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate coords buffer");
    memset(coords_buf, 0, lv_FORMULA_BUF_LARGE);

    if (node->data.geom_point.coords) {
        render_dsl_internal(node->data.geom_point.coords, coords_buf, lv_FORMULA_BUF_LARGE, options);
    }

    written = snprintf(buffer, size, "point %s(%s)",
                       node->data.geom_point.name ? node->data.geom_point.name : "P", coords_buf);

    lv_free((void **) &coords_buf);
    return written;
}

static int helper_dsl_geom_segment(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 端点名缓冲区 ≤64 字节 */
    char ep1_buf[lv_FORMULA_BUF_SMALL] = {0};
    char ep2_buf[lv_FORMULA_BUF_SMALL] = {0};

    if (node->data.geom_segment.endpoint1) {
        render_dsl_internal(node->data.geom_segment.endpoint1, ep1_buf, sizeof(ep1_buf), options);
    }
    if (node->data.geom_segment.endpoint2) {
        render_dsl_internal(node->data.geom_segment.endpoint2, ep2_buf, sizeof(ep2_buf), options);
    }

    written = snprintf(buffer, size, "segment %s(%s, %s)",
                       node->data.geom_segment.name ? node->data.geom_segment.name : "AB", ep1_buf, ep2_buf);
    return written;
}

static int helper_dsl_geom_circle(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 小型缓冲区 ≤256 字节 */
    char center_buf[lv_FORMULA_BUF_SMALL] = {0};
    char radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};

    if (node->data.geom_circle.center) {
        render_dsl_internal(node->data.geom_circle.center, center_buf, sizeof(center_buf), options);
    }
    if (node->data.geom_circle.radius) {
        render_dsl_internal(node->data.geom_circle.radius, radius_buf, sizeof(radius_buf), options);
    }

    written = snprintf(buffer, size, "circle %s(%s, %s)",
                       node->data.geom_circle.name ? node->data.geom_circle.name : "O", center_buf, radius_buf);
    return written;
}

static int helper_dsl_geom_triangle(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 顶点名缓冲区 ≤64 字节 */
    char v1_buf[lv_FORMULA_BUF_SMALL] = {0}, v2_buf[lv_FORMULA_BUF_SMALL] = {0},
         v3_buf[lv_FORMULA_BUF_SMALL] = {0};

    if (node->data.geom_triangle.vertex1) {
        render_dsl_internal(node->data.geom_triangle.vertex1, v1_buf, sizeof(v1_buf), options);
    }
    if (node->data.geom_triangle.vertex2) {
        render_dsl_internal(node->data.geom_triangle.vertex2, v2_buf, sizeof(v2_buf), options);
    }
    if (node->data.geom_triangle.vertex3) {
        render_dsl_internal(node->data.geom_triangle.vertex3, v3_buf, sizeof(v3_buf), options);
    }

    written =
        snprintf(buffer, size, "triangle %s(%s, %s, %s)",
                 node->data.geom_triangle.name ? node->data.geom_triangle.name : "ABC", v1_buf, v2_buf, v3_buf);
    return written;
}

static int helper_dsl_coord_list(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    size_t pos = 0;

    for (int i = 0; i < node->data.coord_list.coord_count; i++) {
        /* STACK_SAFE: 坐标缓冲区 ≤256 字节 */
        char coord_buf[lv_FORMULA_BUF_MEDIUM] = {0};
        render_dsl_internal(node->data.coord_list.coords[i], coord_buf, sizeof(coord_buf), options);

        if (!lv_str_append_sep(buffer, size, &pos, ", ", coord_buf))
            break;
    }
    return (int) pos;
}

static int helper_dsl_constraint_perpendicular(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
    char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
         p3_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 3) {
        render_dsl_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
        render_dsl_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
        render_dsl_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
        written = snprintf(buffer, size, "perpendicular(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
    }
    return written;
}

static int helper_dsl_constraint_parallel(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 线名缓冲区 ≤64 字节 */
    char l1_buf[lv_FORMULA_BUF_SMALL] = {0}, l2_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 2) {
        render_dsl_internal(node->data.constraint.participants[0], l1_buf, sizeof(l1_buf), options);
        render_dsl_internal(node->data.constraint.participants[1], l2_buf, sizeof(l2_buf), options);
        written = snprintf(buffer, size, "parallel(%s, %s)", l1_buf, l2_buf);
    }
    return written;
}

static int helper_dsl_constraint_midpoint(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
    char m_buf[lv_FORMULA_BUF_SMALL] = {0}, a_buf[lv_FORMULA_BUF_SMALL] = {0},
         b_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 3) {
        render_dsl_internal(node->data.constraint.participants[0], m_buf, sizeof(m_buf), options);
        render_dsl_internal(node->data.constraint.participants[1], a_buf, sizeof(a_buf), options);
        render_dsl_internal(node->data.constraint.participants[2], b_buf, sizeof(b_buf), options);
        written = snprintf(buffer, size, "midpoint(%s, %s, %s)", m_buf, a_buf, b_buf);
    }
    return written;
}

/* NODE_GEOM_REGION 区域渲染 */
static int helper_dsl_geom_region(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    const char *name = node->data.geom_region.name ? node->data.geom_region.name : "R";
    written = snprintf(buffer, size, "region %s", name);
    return written;
}

/* NODE_GEOM_ARC 弧渲染 */
static int helper_dsl_geom_arc(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 名称/角度缓冲区 ≤64，半径缓冲区 ≤256 */
    char center_buf[lv_FORMULA_BUF_SMALL] = {0}, radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};
    char start_buf[lv_FORMULA_BUF_SMALL] = {0}, end_buf[lv_FORMULA_BUF_SMALL] = {0};

    if (node->data.geom_arc.center) {
        render_dsl_internal(node->data.geom_arc.center, center_buf, sizeof(center_buf), options);
    }
    if (node->data.geom_arc.radius) {
        render_dsl_internal(node->data.geom_arc.radius, radius_buf, sizeof(radius_buf), options);
    }
    if (node->data.geom_arc.start_angle) {
        render_dsl_internal(node->data.geom_arc.start_angle, start_buf, sizeof(start_buf), options);
    }
    if (node->data.geom_arc.end_angle) {
        render_dsl_internal(node->data.geom_arc.end_angle, end_buf, sizeof(end_buf), options);
    }

    written = snprintf(buffer, size, "arc %s(%s, %s, %s, %s)",
                       node->data.geom_arc.name ? node->data.geom_arc.name : "AB", center_buf, radius_buf,
                       start_buf, end_buf);
    return written;
}

/* NODE_CONSTRAINT_ANGLE 角度约束渲染 */
static int helper_dsl_constraint_angle(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
    char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
         p3_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.constraint.participant_count >= 3) {
        render_dsl_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
        render_dsl_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
        render_dsl_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
        written = snprintf(buffer, size, "angle(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
    }
    return written;
}

static int helper_dsl_compound(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
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

        render_dsl_internal(node->data.compound.statements[i], stmt_buf, lv_FORMULA_BUF_SIZE, options);

        int w = snprintf(ptr, remaining, "%s; ", stmt_buf);
        formula_pool_free(stmt_buf);

        if (w < 0 || (size_t) w >= remaining)
            break;
        ptr += w;
        remaining -= w;
        total += w;
    }
    written = total;
    return written;
}

static const RenderNodeFunc s_render_dsl_funcs[] = {
    [NODE_NUMBER] = helper_dsl_number,
    [NODE_VARIABLE] = helper_dsl_variable,
    [NODE_IDENTIFIER] = helper_dsl_identifier,
    [NODE_BINARY_OP_ADD] = helper_dsl_binary_add,
    [NODE_BINARY_OP_SUB] = helper_dsl_binary_sub,
    [NODE_BINARY_OP_MUL] = helper_dsl_binary_mul,
    [NODE_BINARY_OP_DIV] = helper_dsl_binary_div,
    [NODE_BINARY_OP_POW] = helper_dsl_binary_pow,
    [NODE_UNARY_OP_NEG] = helper_dsl_unary_neg,
    [NODE_UNARY_OP_SQRT] = helper_dsl_unary_sqrt,
    [NODE_UNARY_OP_SIN] = helper_dsl_unary_sin_cos_tan,
    [NODE_UNARY_OP_COS] = helper_dsl_unary_sin_cos_tan,
    [NODE_UNARY_OP_TAN] = helper_dsl_unary_sin_cos_tan,
    [NODE_UNARY_OP_ABS] = helper_dsl_unary_abs,
    [NODE_EQUATION] = helper_dsl_equation,
    [NODE_GEOM_POINT] = helper_dsl_geom_point,
    [NODE_GEOM_SEGMENT] = helper_dsl_geom_segment,
    [NODE_GEOM_CIRCLE] = helper_dsl_geom_circle,
    [NODE_GEOM_TRIANGLE] = helper_dsl_geom_triangle,
    [NODE_COORDINATE_LIST] = helper_dsl_coord_list,
    [NODE_CONSTRAINT_PERPENDICULAR] = helper_dsl_constraint_perpendicular,
    [NODE_CONSTRAINT_PARALLEL] = helper_dsl_constraint_parallel,
    [NODE_CONSTRAINT_MIDPOINT] = helper_dsl_constraint_midpoint,
    [NODE_GEOM_REGION] = helper_dsl_geom_region,
    [NODE_GEOM_ARC] = helper_dsl_geom_arc,
    [NODE_CONSTRAINT_ANGLE] = helper_dsl_constraint_angle,
    [NODE_COMPOUND] = helper_dsl_compound,
};

static int helper_dsl_unknown(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return snprintf(buffer, size, "<unknown>");
}

int render_dsl_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    return dispatch_via(node, buffer, size, options, s_render_dsl_funcs, lv_ARRAY_SIZE(s_render_dsl_funcs),
                        helper_dsl_unknown);
}

