/**
 * @file formula_renderer_ascii.c
 * @brief ASCII 渲染后端
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
 * ASCII 艺术渲染器
 * ============================================================ */

/**
 * @brief 将 AST 渲染为 ASCII 艺术格式
 *
 * 生成基本的 ASCII 数学表示。
 * options 预留：未来可控制精度、宽度等格式参数。
 */

static int helper_ascii_number(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    if (node->data.number.is_integer) {
        return snprintf(buffer, size, "%lld", (long long) node->data.number.numerator);
    } else {
        return snprintf(buffer, size, "%lld/%llu", (long long) node->data.number.numerator,
                        (unsigned long long) node->data.number.denominator);
    }
}

static int helper_ascii_variable(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return (int) lv_strlcpy(buffer, node->data.variable.name, size);
}

static int helper_ascii_identifier(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return (int) lv_strlcpy(buffer, node->data.identifier.name, size);
}

/* ASCII 二元/一元算子模板表（格式串为外部契约，内容与历史输出逐字一致） */
static const RenderBinarySpec s_ascii_binary_specs[] = {
    [NODE_BINARY_OP_ADD] = {"(%s + %s)", 0},
    [NODE_BINARY_OP_SUB] = {"(%s - %s)", 0},
    [NODE_BINARY_OP_MUL] = {"(%s * %s)", 0},
    [NODE_BINARY_OP_DIV] = {"(%s / %s)", 0},
    [NODE_BINARY_OP_POW] = {"(%s ^ %s)", 0},
};

static const RenderUnarySpec s_ascii_unary_specs[] = {
    [NODE_UNARY_OP_NEG] = {"-(", ")", 0},
    [NODE_UNARY_OP_SQRT] = {"sqrt(", ")", 0},
    [NODE_UNARY_OP_ABS] = {"|", "|", 0},
    [NODE_UNARY_OP_LN] = {"ln(", ")", 0},
    [NODE_UNARY_OP_LOG] = {"log(", ")", 0},
};

/* 前缀自共享一元函数名表构造："sin(" 等（与历史输出逐字一致） */
static const RenderFnNameSpec s_ascii_fn_name_spec = {"%s(", ")", 0, "<unknown>"};

static int helper_ascii_binary(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_binary_spec(node, s_ascii_binary_specs, lv_ARRAY_SIZE(s_ascii_binary_specs), buffer, size, options,
                              render_ascii_internal);
}

static int helper_ascii_unary(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_unary_spec(node, s_ascii_unary_specs, lv_ARRAY_SIZE(s_ascii_unary_specs), buffer, size, options,
                             render_ascii_internal);
}

static int helper_ascii_fn_name(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return render_fn_name_spec(node, &s_ascii_fn_name_spec, buffer, size, options, render_ascii_internal);
}

static int helper_ascii_equation(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    char lhs[lv_FORMULA_BUF_SIZE], rhs[lv_FORMULA_BUF_SIZE];
    render_ascii_internal(node->data.equation.lhs, lhs, sizeof(lhs), options);
    render_ascii_internal(node->data.equation.rhs, rhs, sizeof(rhs), options);
    return snprintf(buffer, size, "%s = %s", lhs, rhs);
}

/* ---------- 坐标列表 ----------
 * 格式："(x, y, z)"，与其它后端坐标列表风格一致。 */
static int helper_ascii_coord_list(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    /* lvStrBuf 动态构建（自动扩容），首遍探测（buffer==NULL）与第二遍写入行为一致 */
    lvStrBuf sb;
    lv_strbuf_init(&sb);
    lv_strbuf_printf(&sb, "(");

    for (int i = 0; i < node->data.coord_list.coord_count; i++) {
        /* STACK_SAFE: 坐标缓冲区 ≤256 字节 */
        char coord_buf[lv_FORMULA_BUF_MEDIUM] = {0};
        render_ascii_internal(node->data.coord_list.coords[i], coord_buf, sizeof(coord_buf), options);
        if (i > 0)
            lv_strbuf_printf(&sb, ", ");
        lv_strbuf_printf(&sb, "%s", coord_buf);
    }
    lv_strbuf_printf(&sb, ")");

    int written = (int) sb.len;
    if (buffer && size > 0) {
        lv_strlcpy(buffer, sb.data, size);
    }
    lv_strbuf_destroy(&sb);
    return written;
}

/* ---------- 几何对象 ----------
 * 格式：小写类型名 + 括号内容，与 DSL/约束风格一致：
 *   point(A) / segment(AB) / line(l, A, B) / circle(O, r) /
 *   triangle(ABC) / polygon(P, N) / region(R) / arc(AB, c, r, t1, t2) /
 *   vector(v, A, B)。 */

static int helper_ascii_geom_point(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* HEAP_ALLOCATED: 大型坐标缓冲区使用直接 malloc */
    char *coords_buf = (char *) lv_malloc(lv_FORMULA_BUF_LARGE);
    if (!coords_buf)
        return -1;
    memset(coords_buf, 0, lv_FORMULA_BUF_LARGE);
    if (node->data.geom_point.coords) {
        render_ascii_internal(node->data.geom_point.coords, coords_buf, lv_FORMULA_BUF_LARGE, options);
    }
    written = snprintf(buffer, size, "point(%s)", node->data.geom_point.name ? node->data.geom_point.name : "P");
    lv_free((void **) &coords_buf);
    return written;
}

static int helper_ascii_geom_segment(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 端点名缓冲区 ≤64 字节 */
    char ep1_buf[lv_FORMULA_BUF_SMALL] = {0};
    char ep2_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.geom_segment.endpoint1) {
        render_ascii_internal(node->data.geom_segment.endpoint1, ep1_buf, sizeof(ep1_buf), options);
    }
    if (node->data.geom_segment.endpoint2) {
        render_ascii_internal(node->data.geom_segment.endpoint2, ep2_buf, sizeof(ep2_buf), options);
    }
    if (node->data.geom_segment.name) {
        written = snprintf(buffer, size, "segment(%s)", node->data.geom_segment.name);
    } else {
        written = snprintf(buffer, size, "segment(%s%s)", ep1_buf, ep2_buf);
    }
    return written;
}

static int helper_ascii_geom_line(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 端点名缓冲区 ≤64 字节 */
    char p1_buf[lv_FORMULA_BUF_SMALL] = {0};
    char p2_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.geom_line.point1) {
        render_ascii_internal(node->data.geom_line.point1, p1_buf, sizeof(p1_buf), options);
    }
    if (node->data.geom_line.point2) {
        render_ascii_internal(node->data.geom_line.point2, p2_buf, sizeof(p2_buf), options);
    }
    if (node->data.geom_line.name) {
        written = snprintf(buffer, size, "line(%s)", node->data.geom_line.name);
    } else {
        written = snprintf(buffer, size, "line(%s%s)", p1_buf, p2_buf);
    }
    return written;
}

static int helper_ascii_geom_circle(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 小型缓冲区 ≤256 字节 */
    char center_buf[lv_FORMULA_BUF_SMALL] = {0};
    char radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};
    if (node->data.geom_circle.center) {
        render_ascii_internal(node->data.geom_circle.center, center_buf, sizeof(center_buf), options);
    }
    if (node->data.geom_circle.radius) {
        render_ascii_internal(node->data.geom_circle.radius, radius_buf, sizeof(radius_buf), options);
    }
    written = snprintf(buffer, size, "circle(%s, %s)",
                       node->data.geom_circle.name ? node->data.geom_circle.name : "O", radius_buf);
    return written;
}

static int helper_ascii_geom_triangle(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 顶点名缓冲区 ≤64 字节 */
    char v1_buf[lv_FORMULA_BUF_SMALL] = {0}, v2_buf[lv_FORMULA_BUF_SMALL] = {0},
         v3_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.geom_triangle.vertex1) {
        render_ascii_internal(node->data.geom_triangle.vertex1, v1_buf, sizeof(v1_buf), options);
    }
    if (node->data.geom_triangle.vertex2) {
        render_ascii_internal(node->data.geom_triangle.vertex2, v2_buf, sizeof(v2_buf), options);
    }
    if (node->data.geom_triangle.vertex3) {
        render_ascii_internal(node->data.geom_triangle.vertex3, v3_buf, sizeof(v3_buf), options);
    }
    if (node->data.geom_triangle.name) {
        written = snprintf(buffer, size, "triangle(%s)", node->data.geom_triangle.name);
    } else {
        written = snprintf(buffer, size, "triangle(%s%s%s)", v1_buf, v2_buf, v3_buf);
    }
    return written;
}

static int helper_ascii_geom_polygon(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    written = snprintf(buffer, size, "polygon(%s, %d)",
                       node->data.geom_polygon.name ? node->data.geom_polygon.name : "P",
                       node->data.geom_polygon.vertex_count);
    return written;
}

static int helper_ascii_geom_region(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    written = snprintf(buffer, size, "region(%s)", node->data.geom_region.name ? node->data.geom_region.name : "R");
    return written;
}

static int helper_ascii_geom_arc(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 名称/角度缓冲区 ≤64，半径缓冲区 ≤256 */
    char center_buf[lv_FORMULA_BUF_SMALL] = {0}, radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};
    char start_buf[lv_FORMULA_BUF_SMALL] = {0}, end_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.geom_arc.center) {
        render_ascii_internal(node->data.geom_arc.center, center_buf, sizeof(center_buf), options);
    }
    if (node->data.geom_arc.radius) {
        render_ascii_internal(node->data.geom_arc.radius, radius_buf, sizeof(radius_buf), options);
    }
    if (node->data.geom_arc.start_angle) {
        render_ascii_internal(node->data.geom_arc.start_angle, start_buf, sizeof(start_buf), options);
    }
    if (node->data.geom_arc.end_angle) {
        render_ascii_internal(node->data.geom_arc.end_angle, end_buf, sizeof(end_buf), options);
    }
    written = snprintf(buffer, size, "arc(%s, %s, %s, %s, %s)",
                       node->data.geom_arc.name ? node->data.geom_arc.name : "AB", center_buf, radius_buf,
                       start_buf, end_buf);
    return written;
}

static int helper_ascii_geom_vector(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    int written = 0;
    /* STACK_SAFE: 端点名缓冲区 ≤64 字节 */
    char s_buf[lv_FORMULA_BUF_SMALL] = {0};
    char e_buf[lv_FORMULA_BUF_SMALL] = {0};
    if (node->data.geom_vector.start) {
        render_ascii_internal(node->data.geom_vector.start, s_buf, sizeof(s_buf), options);
    }
    if (node->data.geom_vector.end) {
        render_ascii_internal(node->data.geom_vector.end, e_buf, sizeof(e_buf), options);
    }
    if (node->data.geom_vector.name) {
        written = snprintf(buffer, size, "vector(%s)", node->data.geom_vector.name);
    } else {
        written = snprintf(buffer, size, "vector(%s%s)", s_buf, e_buf);
    }
    return written;
}

/* ---------- 几何约束（统一 participants 遍历模板） ----------
 * 格式：小写约束名 + 括号逗号分隔参与者，与 DSL 后端一致。 */

static int helper_ascii_constraint(const FormulaNode *node, const char *kind, char *buffer, size_t size,
                                   const RenderOptions *options)
{
    /* 参与者渲染为逗号分隔列表（首项无前缀分隔符） */
    lvStrBuf sb;
    lv_strbuf_init(&sb);
    lv_strbuf_printf(&sb, "%s(", kind);
    for (int i = 0; i < node->data.constraint.participant_count; i++) {
        /* STACK_SAFE: 参与者名缓冲区 ≤64 字节 */
        char p_buf[lv_FORMULA_BUF_SMALL] = {0};
        render_ascii_internal(node->data.constraint.participants[i], p_buf, sizeof(p_buf), options);
        if (i > 0)
            lv_strbuf_printf(&sb, ", ");
        lv_strbuf_printf(&sb, "%s", p_buf);
    }
    lv_strbuf_printf(&sb, ")");

    int written = (int) sb.len;
    if (buffer && size > 0) {
        lv_strlcpy(buffer, sb.data, size);
    }
    lv_strbuf_destroy(&sb);
    return written;
}

/* 8 个约束名称包装器由 LV_CONSTRAINT_NAME_X 派生（判据 D 单源） */
#define LV_ASCII_CONSTRAINT_WRAPPER(ENUM, NAME, IDENT) \
    static int helper_ascii_constraint_##IDENT(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) \
    { \
        return helper_ascii_constraint(node, NAME, buffer, size, options); \
    }
LV_CONSTRAINT_NAME_X(LV_ASCII_CONSTRAINT_WRAPPER)
#undef LV_ASCII_CONSTRAINT_WRAPPER

/* ---------- 复合语句 ----------
 * 格式：语句以 "; " 分隔（与 DSL 后端一致）。 */
static int helper_ascii_compound(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    /* lvStrBuf 动态构建（自动扩容），首遍探测（buffer==NULL）与第二遍写入行为一致 */
    lvStrBuf sb;
    lv_strbuf_init(&sb);

    for (int i = 0; i < node->data.compound.statement_count; i++) {
        /* HEAP_ALLOCATED: 池分配语句缓冲区 */
        char *stmt_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
        if (!stmt_buf) {
            lv_strbuf_destroy(&sb);
            return 0;
        }
        render_ascii_internal(node->data.compound.statements[i], stmt_buf, lv_FORMULA_BUF_SIZE, options);
        if (i > 0)
            lv_strbuf_printf(&sb, "; ");
        lv_strbuf_printf(&sb, "%s", stmt_buf);
        formula_pool_free(stmt_buf);
    }

    int written = (int) sb.len;
    if (buffer && size > 0) {
        lv_strlcpy(buffer, sb.data, size);
    }
    lv_strbuf_destroy(&sb);
    return written;
}

static const RenderNodeFunc s_render_ascii_funcs[] = {
    [NODE_NUMBER] = helper_ascii_number,
    [NODE_VARIABLE] = helper_ascii_variable,
    [NODE_IDENTIFIER] = helper_ascii_identifier,
    [NODE_BINARY_OP_ADD] = helper_ascii_binary,
    [NODE_BINARY_OP_SUB] = helper_ascii_binary,
    [NODE_BINARY_OP_MUL] = helper_ascii_binary,
    [NODE_BINARY_OP_DIV] = helper_ascii_binary,
    [NODE_BINARY_OP_POW] = helper_ascii_binary,
    [NODE_UNARY_OP_NEG] = helper_ascii_unary,
    [NODE_UNARY_OP_SQRT] = helper_ascii_unary,
    [NODE_UNARY_OP_SIN] = helper_ascii_fn_name,
    [NODE_UNARY_OP_COS] = helper_ascii_fn_name,
    [NODE_UNARY_OP_TAN] = helper_ascii_fn_name,
    [NODE_UNARY_OP_ABS] = helper_ascii_unary,
    [NODE_UNARY_OP_LN] = helper_ascii_unary,
    [NODE_UNARY_OP_LOG] = helper_ascii_unary,
    [NODE_EQUATION] = helper_ascii_equation,
    [NODE_COORDINATE_LIST] = helper_ascii_coord_list,
    [NODE_GEOM_POINT] = helper_ascii_geom_point,
    [NODE_GEOM_SEGMENT] = helper_ascii_geom_segment,
    [NODE_GEOM_LINE] = helper_ascii_geom_line,
    [NODE_GEOM_CIRCLE] = helper_ascii_geom_circle,
    [NODE_GEOM_TRIANGLE] = helper_ascii_geom_triangle,
    [NODE_GEOM_POLYGON] = helper_ascii_geom_polygon,
    [NODE_GEOM_REGION] = helper_ascii_geom_region,
    [NODE_GEOM_ARC] = helper_ascii_geom_arc,
    [NODE_GEOM_VECTOR] = helper_ascii_geom_vector,
#define LV_ASCII_CONSTRAINT_DISPATCH(ENUM, NAME, IDENT) [ENUM] = helper_ascii_constraint_##IDENT,
    LV_CONSTRAINT_NAME_X(LV_ASCII_CONSTRAINT_DISPATCH)
#undef LV_ASCII_CONSTRAINT_DISPATCH
    [NODE_COMPOUND] = helper_ascii_compound,
};

static int helper_ascii_unknown(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    (void) node;
    (void) options;
    return snprintf(buffer, size, "<unknown>");
}

int render_ascii_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    /* 与其它后端一致：仅校验 node，buffer 可为 NULL（formula_render_ex 两遍法首遍探测长度） */
    if (!node)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "invalid params for ascii render");

    return dispatch_via(node, buffer, size, options, s_render_ascii_funcs, lv_ARRAY_SIZE(s_render_ascii_funcs),
                        helper_ascii_unknown);
}

