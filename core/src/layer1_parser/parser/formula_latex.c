/**
 * @file formula_latex.c
 * @brief LaTeX 语法解析器
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/formula_parser.h"
#include "lv/lv_str_utils.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"

FormulaNode *parse_latex_expression(ParserContext *ctx);
static FormulaNode *parse_latex_term(ParserContext *ctx);
static FormulaNode *parse_latex_factor(ParserContext *ctx);
static FormulaNode *parse_latex_atom(ParserContext *ctx);

/**
 * @brief 解析 LaTeX 分数 \frac{a}{b}
 *
 * 解析 LaTeX 分数命令，格式为 \frac{分子}{分母}。
 * 分子和分母各自可以包含完整的 LaTeX 表达式。
 * 返回 DIV 类型的二元运算节点。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 分数节点（DIV 运算），失败返回 NULL
 */
static FormulaNode *parse_latex_frac(ParserContext *ctx) {
    formula_skip_whitespace(ctx);

    /* 期望 '{' */
    if (!formula_expect_char(ctx, '{')) {
        return NULL;
    }

    formula_skip_whitespace(ctx);
    FormulaNode *numerator = parse_latex_expression(ctx);
    if (!numerator)
        return NULL;
    formula_skip_whitespace(ctx);

    if (!formula_expect_char(ctx, '}')) {
        formula_node_destroy(numerator);
        return NULL;
    }

    formula_skip_whitespace(ctx);
    if (!formula_expect_char(ctx, '{')) {
        formula_node_destroy(numerator);
        return NULL;
    }

    formula_skip_whitespace(ctx);
    FormulaNode *denominator = parse_latex_expression(ctx);
    if (!denominator) {
        formula_node_destroy(numerator);
        return NULL;
    }
    formula_skip_whitespace(ctx);

    if (!formula_expect_char(ctx, '}')) {
        formula_node_destroy(numerator);
        formula_node_destroy(denominator);
        return NULL;
    }

    return formula_track_node(ctx, formula_create_binary_op(NODE_BINARY_OP_DIV, numerator, denominator));
}

/**
 * @brief 解析 LaTeX 根号 \sqrt{x}
 *
 * 解析 LaTeX 根号命令，支持两种形式：
 * - \sqrt{表达式}：带花括号的参数
 * - \sqrt 原子：不带花括号，取下一个原子作为参数
 * 返回 SQRT 类型的一元运算节点。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 根号节点（SQRT 运算），失败返回 NULL
 */
static FormulaNode *parse_latex_sqrt(ParserContext *ctx) {
    formula_skip_whitespace(ctx);

    if (formula_peek(ctx) == '{') {
        formula_consume(ctx);
        formula_skip_whitespace(ctx);
        FormulaNode *operand = parse_latex_expression(ctx);
        if (!operand)
            return NULL;
        formula_skip_whitespace(ctx);
        if (!formula_expect_char(ctx, '}')) {
            formula_node_destroy(operand);
            return NULL;
        }
        return formula_create_unary_op(NODE_UNARY_OP_SQRT, operand);
    }

    FormulaNode *operand = parse_latex_atom(ctx);
    if (!operand)
        return NULL;
    return formula_create_unary_op(NODE_UNARY_OP_SQRT, operand);
}

/**
 * @brief 解析 LaTeX 命令
 *
 * 在反斜杠 \ 之后解析 LaTeX 命令名称，并分派到对应的处理函数。
 * 支持的命令包括：\frac（分式）、\sqrt（根号）、三角函数
 * （\sin, \cos, \tan, \cot）、\ln、\log、\pi 等。
 * 未知命令作为变量名处理。
 *
 * @param ctx 解析器上下文指针（反斜杠已被消费）
 * @return FormulaNode* 命令对应的 AST 节点，失败返回 NULL
 */
static FormulaNode *parse_latex_command(ParserContext *ctx) {
    /* 已经匹配了 '\' */
    char *cmd = formula_parse_identifier_str(ctx);
    if (!cmd) {
        formula_set_error(ctx, "Expected LaTeX command after '\\'");
        return NULL;
    }

    FormulaNode *result = NULL;

    if (lv_str_eq(cmd, "frac")) {
        lv_free((void **) &cmd);
        return parse_latex_frac(ctx);
    }
    if (lv_str_eq(cmd, "sqrt")) {
        lv_free((void **) &cmd);
        return parse_latex_sqrt(ctx);
    }
    if (lv_str_eq(cmd, "sin")) {
        lv_free((void **) &cmd);
        formula_skip_whitespace(ctx);
        if (formula_peek(ctx) == '{') {
            formula_consume(ctx);
            formula_skip_whitespace(ctx);
            FormulaNode *arg = parse_latex_expression(ctx);
            if (arg) {
                formula_skip_whitespace(ctx);
                if (formula_expect_char(ctx, '}')) {
                    result = formula_create_unary_op(NODE_UNARY_OP_SIN, arg);
                } else {
                    formula_node_destroy(arg);
                }
            }
        } else {
            FormulaNode *arg = parse_latex_atom(ctx);
            if (arg)
                result = formula_create_unary_op(NODE_UNARY_OP_SIN, arg);
        }
        return result;
    }
    if (lv_str_eq(cmd, "cos")) {
        lv_free((void **) &cmd);
        formula_skip_whitespace(ctx);
        if (formula_peek(ctx) == '{') {
            formula_consume(ctx);
            formula_skip_whitespace(ctx);
            FormulaNode *arg = parse_latex_expression(ctx);
            if (arg) {
                formula_skip_whitespace(ctx);
                if (formula_expect_char(ctx, '}')) {
                    result = formula_create_unary_op(NODE_UNARY_OP_COS, arg);
                } else {
                    formula_node_destroy(arg);
                }
            }
        } else {
            FormulaNode *arg = parse_latex_atom(ctx);
            if (arg)
                result = formula_create_unary_op(NODE_UNARY_OP_COS, arg);
        }
        return result;
    }
    if (lv_str_eq(cmd, "tan")) {
        lv_free((void **) &cmd);
        formula_skip_whitespace(ctx);
        if (formula_peek(ctx) == '{') {
            formula_consume(ctx);
            formula_skip_whitespace(ctx);
            FormulaNode *arg = parse_latex_expression(ctx);
            if (arg) {
                formula_skip_whitespace(ctx);
                if (formula_expect_char(ctx, '}')) {
                    result = formula_create_unary_op(NODE_UNARY_OP_TAN, arg);
                } else {
                    formula_node_destroy(arg);
                }
            }
        } else {
            FormulaNode *arg = parse_latex_atom(ctx);
            if (arg)
                result = formula_create_unary_op(NODE_UNARY_OP_TAN, arg);
        }
        return result;
    }
    if (lv_str_eq(cmd, "pi")) {
        lv_free((void **) &cmd);
        return formula_create_variable("pi");
    }

    /* 其他命令作为变量 */
    result = formula_create_variable(cmd);
    lv_free((void **) &cmd);
    return result;
}

/**
 * @brief 解析 LaTeX 原子
 *
 * 解析 LaTeX 语法中的最小语法单元，包括：
 * - 数字字面量
 * - 括号表达式 (...)
 * - LaTeX 命令（\frac, \sqrt, \sin 等）
 * - 花括号分组 {...}
 * - 一元正负号
 * - 标识符/变量名
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的原子节点，失败返回 NULL
 */
static FormulaNode *parse_latex_atom(ParserContext *ctx) {
    formula_skip_whitespace(ctx);

    char c = formula_peek(ctx);

    /* 数字 */
    if (formula_is_digit(c) || (c == '.' && formula_is_digit(formula_peek_next(ctx)))) {
        return formula_parse_number(ctx);
    }

    /* 括号表达式 */
    if (c == '(') {
        formula_consume(ctx);
        FormulaNode *expr = parse_latex_expression(ctx);
        if (!expr)
            return NULL;
        formula_skip_whitespace(ctx);
        if (!formula_expect_char(ctx, ')')) {
            formula_node_destroy(expr);
            return NULL;
        }
        return expr;
    }

    /* LaTeX 命令 */
    if (c == '\\') {
        formula_consume(ctx);
        return parse_latex_command(ctx);
    }

    /* 花括号分组 */
    if (c == '{') {
        formula_consume(ctx);
        formula_skip_whitespace(ctx);
        FormulaNode *expr = parse_latex_expression(ctx);
        if (!expr)
            return NULL;
        formula_skip_whitespace(ctx);
        if (!formula_expect_char(ctx, '}')) {
            formula_node_destroy(expr);
            return NULL;
        }
        return expr;
    }

    /* 负号 */
    if (c == '-') {
        formula_consume(ctx);
        FormulaNode *operand = parse_latex_factor(ctx);
        if (!operand)
            return NULL;
        return formula_track_node(ctx, formula_create_unary_op(NODE_UNARY_OP_NEG, operand));
    }

    /* 正号 */
    if (c == '+') {
        formula_consume(ctx);
        return parse_latex_factor(ctx);
    }

    /* 标识符 */
    if (formula_is_alpha(c)) {
        char *ident = formula_parse_identifier_str(ctx);
        if (!ident)
            return NULL;

        FormulaNode *node = formula_track_node(ctx, formula_create_variable(ident));
        lv_free((void **) &ident);
        return node;
    }

    formula_set_error(ctx, "Unexpected character in LaTeX expression");
    return NULL;
}

/**
 * @brief 解析 LaTeX 因子
 *
 * 在原子基础上处理上标幂运算（^），支持花括号分组和多字符指数。
 * 例如：x^{2n} 或 x^2。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的因子节点，失败返回 NULL
 */
static FormulaNode *parse_latex_factor(ParserContext *ctx) {
    FormulaNode *left = parse_latex_atom(ctx);
    if (!left)
        return NULL;

    formula_skip_whitespace(ctx);

    /* 处理上标 */
    if (formula_peek(ctx) == '^') {
        formula_consume(ctx);
        formula_skip_whitespace(ctx);
        FormulaNode *exponent = NULL;
        if (formula_peek(ctx) == '{') {
            formula_consume(ctx);
            formula_skip_whitespace(ctx);
            exponent = parse_latex_expression(ctx);
            if (exponent) {
                formula_skip_whitespace(ctx);
                if (!formula_expect_char(ctx, '}')) {
                    formula_node_destroy(left);
                    formula_node_destroy(exponent);
                    return NULL;
                }
            }
        } else {
            exponent = parse_latex_atom(ctx);
        }
        if (!exponent) {
            formula_node_destroy(left);
            return NULL;
        }
        return formula_track_node(ctx, formula_create_binary_op(NODE_BINARY_OP_POW, left, exponent));
    }

    return left;
}

/**
 * @brief 解析 LaTeX 项
 *
 * 在因子基础上处理乘法和除法运算，支持以下运算符：
 * - *（星号乘法）
 * - /（斜杠除法）
 * - \cdot（LaTeX 点乘）
 * - \times（LaTeX 叉乘）
 * - \div（LaTeX 除号）
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的项节点，失败返回 NULL
 */
static FormulaNode *parse_latex_term(ParserContext *ctx) {
    FormulaNode *left = parse_latex_factor(ctx);
    if (!left)
        return NULL;

    while (true) {
        formula_skip_whitespace(ctx);
        char c = formula_peek(ctx);

        NodeType op_type = NODE_BINARY_OP_MUL;
        bool should_continue = false;

        if (c == '*') {
            formula_consume(ctx);
            should_continue = true;
        } else if (c == '/') {
            formula_consume(ctx);
            op_type = NODE_BINARY_OP_DIV;
            should_continue = true;
        } else if (formula_match_and_consume(ctx, "\\cdot")) {
            should_continue = true;
        } else if (formula_match_and_consume(ctx, "\\times")) {
            should_continue = true;
        } else if (formula_match_and_consume(ctx, "\\div")) {
            op_type = NODE_BINARY_OP_DIV;
            should_continue = true;
        }

        if (!should_continue)
            break;

        formula_skip_whitespace(ctx);
        FormulaNode *right = parse_latex_factor(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }

        left = formula_track_node(ctx, formula_create_binary_op(op_type, left, right));
        if (!left)
            return NULL;
    }

    return left;
}

/**
 * @brief 解析 LaTeX 表达式
 *
 * 在项基础上处理加法（+）和减法（-）运算，以及等式（=）。
 * 这是 LaTeX 表达式解析的最高优先级层。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的表达式节点，失败返回 NULL
 */
FormulaNode *parse_latex_expression(ParserContext *ctx) {
    FormulaNode *left = parse_latex_term(ctx);
    if (!left)
        return NULL;

    while (true) {
        formula_skip_whitespace(ctx);
        char c = formula_peek(ctx);

        NodeType op_type;
        bool should_continue = false;

        if (c == '+') {
            formula_consume(ctx);
            op_type = NODE_BINARY_OP_ADD;
            should_continue = true;
        } else if (c == '-') {
            formula_consume(ctx);
            op_type = NODE_BINARY_OP_SUB;
            should_continue = true;
        }

        if (!should_continue)
            break;

        formula_skip_whitespace(ctx);
        FormulaNode *right = parse_latex_term(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }

        left = formula_track_node(ctx, formula_create_binary_op(op_type, left, right));
        if (!left)
            return NULL;
    }

    /* 检查等式 */
    formula_skip_whitespace(ctx);
    if (formula_peek(ctx) == '=' && formula_peek_next(ctx) != '=') {
        formula_consume(ctx);
        formula_skip_whitespace(ctx);
        FormulaNode *right = parse_latex_expression(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }
        return formula_track_node(ctx, formula_create_equation(left, right));
    }

    return left;
}

/* ============================================================
 * Python 解析器
 * ============================================================ */

/* parse_python_expression 已在 formula_parser.h 中声明 */
