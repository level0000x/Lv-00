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
int render_ascii_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options);

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
    return snprintf(buffer, size, "%s", node->data.variable.name);
}

static int helper_ascii_identifier(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    return snprintf(buffer, size, "%s", node->data.identifier.name);
}

static int helper_ascii_binary_add(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    char left[lv_FORMULA_BUF_SIZE], right[lv_FORMULA_BUF_SIZE];
    render_ascii_internal(node->data.binary_op.left, left, sizeof(left), options);
    render_ascii_internal(node->data.binary_op.right, right, sizeof(right), options);
    return snprintf(buffer, size, "(%s + %s)", left, right);
}

static int helper_ascii_binary_sub(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    char left[lv_FORMULA_BUF_SIZE], right[lv_FORMULA_BUF_SIZE];
    render_ascii_internal(node->data.binary_op.left, left, sizeof(left), options);
    render_ascii_internal(node->data.binary_op.right, right, sizeof(right), options);
    return snprintf(buffer, size, "(%s - %s)", left, right);
}

static int helper_ascii_binary_mul(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    char left[lv_FORMULA_BUF_SIZE], right[lv_FORMULA_BUF_SIZE];
    render_ascii_internal(node->data.binary_op.left, left, sizeof(left), options);
    render_ascii_internal(node->data.binary_op.right, right, sizeof(right), options);
    return snprintf(buffer, size, "(%s * %s)", left, right);
}

static int helper_ascii_binary_div(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    char left[lv_FORMULA_BUF_SIZE], right[lv_FORMULA_BUF_SIZE];
    render_ascii_internal(node->data.binary_op.left, left, sizeof(left), options);
    render_ascii_internal(node->data.binary_op.right, right, sizeof(right), options);
    return snprintf(buffer, size, "(%s / %s)", left, right);
}

static int helper_ascii_binary_pow(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    char left[lv_FORMULA_BUF_SIZE], right[lv_FORMULA_BUF_SIZE];
    render_ascii_internal(node->data.binary_op.left, left, sizeof(left), options);
    render_ascii_internal(node->data.binary_op.right, right, sizeof(right), options);
    return snprintf(buffer, size, "(%s ^ %s)", left, right);
}

static int helper_ascii_unary_neg(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    char operand[lv_FORMULA_BUF_SIZE];
    render_ascii_internal(node->data.unary_op.operand, operand, sizeof(operand), options);
    return snprintf(buffer, size, "-(%s)", operand);
}

static int helper_ascii_unary_sqrt(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    char operand[lv_FORMULA_BUF_SIZE];
    render_ascii_internal(node->data.unary_op.operand, operand, sizeof(operand), options);
    return snprintf(buffer, size, "sqrt(%s)", operand);
}

static int helper_ascii_unary_sin_cos_tan(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    const char *fn = (node->type == NODE_UNARY_OP_SIN)   ? "sin"
                     : (node->type == NODE_UNARY_OP_COS) ? "cos"
                                                         : "tan";
    char operand[lv_FORMULA_BUF_SIZE];
    render_ascii_internal(node->data.unary_op.operand, operand, sizeof(operand), options);
    return snprintf(buffer, size, "%s(%s)", fn, operand);
}

static int helper_ascii_unary_abs(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    char operand[lv_FORMULA_BUF_SIZE];
    render_ascii_internal(node->data.unary_op.operand, operand, sizeof(operand), options);
    return snprintf(buffer, size, "|%s|", operand);
}

static int helper_ascii_unary_ln(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    char operand[lv_FORMULA_BUF_SIZE];
    render_ascii_internal(node->data.unary_op.operand, operand, sizeof(operand), options);
    return snprintf(buffer, size, "ln(%s)", operand);
}

static int helper_ascii_unary_log(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    char operand[lv_FORMULA_BUF_SIZE];
    render_ascii_internal(node->data.unary_op.operand, operand, sizeof(operand), options);
    return snprintf(buffer, size, "log(%s)", operand);
}

static int helper_ascii_equation(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)
{
    char lhs[lv_FORMULA_BUF_SIZE], rhs[lv_FORMULA_BUF_SIZE];
    render_ascii_internal(node->data.equation.lhs, lhs, sizeof(lhs), options);
    render_ascii_internal(node->data.equation.rhs, rhs, sizeof(rhs), options);
    return snprintf(buffer, size, "%s = %s", lhs, rhs);
}

static const RenderNodeFunc s_render_ascii_funcs[] = {
    [NODE_NUMBER] = helper_ascii_number,
    [NODE_VARIABLE] = helper_ascii_variable,
    [NODE_IDENTIFIER] = helper_ascii_identifier,
    [NODE_BINARY_OP_ADD] = helper_ascii_binary_add,
    [NODE_BINARY_OP_SUB] = helper_ascii_binary_sub,
    [NODE_BINARY_OP_MUL] = helper_ascii_binary_mul,
    [NODE_BINARY_OP_DIV] = helper_ascii_binary_div,
    [NODE_BINARY_OP_POW] = helper_ascii_binary_pow,
    [NODE_UNARY_OP_NEG] = helper_ascii_unary_neg,
    [NODE_UNARY_OP_SQRT] = helper_ascii_unary_sqrt,
    [NODE_UNARY_OP_SIN] = helper_ascii_unary_sin_cos_tan,
    [NODE_UNARY_OP_COS] = helper_ascii_unary_sin_cos_tan,
    [NODE_UNARY_OP_TAN] = helper_ascii_unary_sin_cos_tan,
    [NODE_UNARY_OP_ABS] = helper_ascii_unary_abs,
    [NODE_UNARY_OP_LN] = helper_ascii_unary_ln,
    [NODE_UNARY_OP_LOG] = helper_ascii_unary_log,
    [NODE_EQUATION] = helper_ascii_equation,
};

int render_ascii_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    if (!node || !buffer || size == 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "invalid params for ascii render");

    /* options->precision 控制浮点数输出精度 */
    int prec = (options && options->precision > 0) ? options->precision : 6;
    (void) prec; /* 精度参数由 render_number_internal 使用 */


    if ((unsigned)node->type < lv_ARRAY_SIZE(s_render_ascii_funcs)
        && s_render_ascii_funcs[node->type]) {
        return s_render_ascii_funcs[node->type](node, buffer, size, options);
    }

    /* fallback for unhandled node types */
    return render_latex_internal(node, buffer, size, options);
}

