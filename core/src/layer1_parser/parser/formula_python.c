/**
 * @file formula_python.c
 * @brief Python 语法解析器
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

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"

FormulaNode *parse_python_expression(ParserContext *ctx);
static FormulaNode *parse_python_term(ParserContext *ctx);
static FormulaNode *parse_python_factor(ParserContext *ctx);
static FormulaNode *parse_python_atom(ParserContext *ctx);

/**
 * @brief 解析 Python 原子
 *
 * 解析 Python 语法中的最小语法单元，包括：
 * - 数字字面量
 * - 括号表达式 (...)
 * - 一元正负号
 * - 标识符（变量名、布尔值 True/False/None、常量 pi）
 * - 函数调用（如 sqrt(x), sin(x) 等）
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的原子节点，失败返回 NULL
 */
static FormulaNode *parse_python_atom(ParserContext *ctx) {
    skip_whitespace(ctx);

    char c = peek(ctx);

    /* 数字 */
    if (is_digit(c) || (c == '.' && is_digit(peek_next(ctx)))) {
        return parse_number(ctx);
    }

    /* 括号表达式 */
    if (c == '(') {
        consume(ctx);
        skip_whitespace(ctx);
        FormulaNode *expr = parse_python_expression(ctx);
        if (!expr)
            return NULL;
        skip_whitespace(ctx);
        if (!expect_char(ctx, ')')) {
            formula_node_destroy(expr);
            return NULL;
        }
        return expr;
    }

    /* 负号 */
    if (c == '-') {
        consume(ctx);
        FormulaNode *operand = parse_python_factor(ctx);
        if (!operand)
            return NULL;
        return track_node(ctx, formula_create_unary_op(NODE_UNARY_OP_NEG, operand));
    }

    /* 正号 */
    if (c == '+') {
        consume(ctx);
        return parse_python_factor(ctx);
    }

    /* 标识符 */
    if (is_alpha(c)) {
        char *ident = parse_identifier_str(ctx);
        if (!ident)
            return NULL;

        skip_whitespace(ctx);

        /* 检查布尔值 */
        if (strcmp(ident, "True") == 0 || strcmp(ident, "False") == 0) {
            int val = (strcmp(ident, "True") == 0) ? 1 : 0;
            lv_free((void **) &ident);
            return formula_create_number(val, 1);
        }
        if (strcmp(ident, "None") == 0) {
            lv_free((void **) &ident);
            return formula_create_variable("None");
        }
        if (strcmp(ident, "pi") == 0) {
            lv_free((void **) &ident);
            return formula_create_variable("pi");
        }

        /* 函数调用 */
        if (peek(ctx) == '(') {
            consume(ctx);
            skip_whitespace(ctx);

            /* 解析参数 */
            FormulaNode *args[lv_MAX_ARGUMENTS] = {NULL};
            int arg_count = 0;
            while (!is_at_end(ctx) && peek(ctx) != ')') {
                if (arg_count >= lv_MAX_ARGUMENTS) {
                    set_error(ctx, "Too many arguments");
                    lv_free((void **) &ident);
                    for (int i = 0; i < arg_count; i++)
                        formula_node_destroy(args[i]);
                    return NULL;
                }

                args[arg_count] = parse_python_expression(ctx);
                if (!args[arg_count]) {
                    lv_free((void **) &ident);
                    for (int i = 0; i < arg_count; i++)
                        formula_node_destroy(args[i]);
                    return NULL;
                }
                arg_count++;

                skip_whitespace(ctx);

                if (peek(ctx) == ',') {
                    consume(ctx);
                    skip_whitespace(ctx);
                }
            }

            if (!expect_char(ctx, ')')) {
                lv_free((void **) &ident);
                for (int i = 0; i < arg_count; i++)
                    formula_node_destroy(args[i]);
                return NULL;
            }

            /* 根据函数名创建对应节点 */
            FormulaNode *node = NULL;
            if (strcmp(ident, "sqrt") == 0 && arg_count == 1) {
                node = formula_create_unary_op(NODE_UNARY_OP_SQRT, args[0]);
            } else if (strcmp(ident, "sin") == 0 && arg_count == 1) {
                node = formula_create_unary_op(NODE_UNARY_OP_SIN, args[0]);
            } else if (strcmp(ident, "cos") == 0 && arg_count == 1) {
                node = formula_create_unary_op(NODE_UNARY_OP_COS, args[0]);
            } else if (strcmp(ident, "tan") == 0 && arg_count == 1) {
                node = formula_create_unary_op(NODE_UNARY_OP_TAN, args[0]);
            } else if (strcmp(ident, "abs") == 0 && arg_count == 1) {
                node = formula_create_unary_op(NODE_UNARY_OP_ABS, args[0]);
            } else if (strcmp(ident, "pow") == 0 && arg_count == 2) {
                node = formula_create_binary_op(NODE_BINARY_OP_POW, args[0], args[1]);
            } else {
                node = formula_create_variable(ident);
            }

            lv_free((void **) &ident);
            for (int i = 0; i < arg_count; i++)
                formula_node_destroy(args[i]);
            return node;
        }

        FormulaNode *node = formula_create_variable(ident);
        lv_free((void **) &ident);
        return node;
    }

    set_error(ctx, "Unexpected character in Python expression");
    return NULL;
}

/**
 * @brief 解析 Python 幂运算
 *
 * 在原子基础上处理 Python 幂运算符 **，采用右结合递归解析。
 * 例如：2**3**2 解析为 2**(3**2) = 512。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的幂运算节点，失败返回 NULL
 */
static FormulaNode *parse_python_power(ParserContext *ctx) {
    FormulaNode *left = parse_python_atom(ctx);
    if (!left)
        return NULL;

    skip_whitespace(ctx);

    /* 处理幂运算 ** */
    if (match_string(ctx, "**")) {
        consume(ctx);
        consume(ctx);
        skip_whitespace(ctx);
        FormulaNode *right = parse_python_factor(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }
        return formula_create_binary_op(NODE_BINARY_OP_POW, left, right);
    }

    return left;
}

/**
 * @brief 解析 Python 因子
 *
 * Python 因子直接委托给幂运算解析（parse_python_power），
 * 因为 Python 中一元运算符的优先级低于幂运算。
 * 例如：-x**2 解析为 -(x**2)。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的因子节点，失败返回 NULL
 */
static FormulaNode *parse_python_factor(ParserContext *ctx) {
    return parse_python_power(ctx);
}

/**
 * @brief 解析 Python 项
 *
 * 在因子基础上处理乘法（*）、除法（/）和取模（%）运算。
 * 注意：** 被识别为幂运算符，在此层停止解析。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的项节点，失败返回 NULL
 */
static FormulaNode *parse_python_term(ParserContext *ctx) {
    FormulaNode *left = parse_python_factor(ctx);
    if (!left)
        return NULL;

    while (true) {
        skip_whitespace(ctx);
        char c = peek(ctx);

        NodeType op_type = NODE_BINARY_OP_MUL;
        bool should_continue = false;

        if (c == '*') {
            if (peek_next(ctx) == '*')
                break; /* 幂运算 */
            consume(ctx);
            should_continue = true;
        } else if (c == '/') {
            consume(ctx);
            op_type = NODE_BINARY_OP_DIV;
            should_continue = true;
        } else if (c == '%') {
            consume(ctx);
            should_continue = true;
        }

        if (!should_continue)
            break;

        skip_whitespace(ctx);
        FormulaNode *right = parse_python_factor(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }

        left = track_node(ctx, formula_create_binary_op(op_type, left, right));
        if (!left)
            return NULL;
    }

    return left;
}

/**
 * @brief 解析 Python 表达式
 *
 * 在项基础上处理加法（+）和减法（-）运算，以及等式（==）。
 * 这是 Python 表达式解析的最高优先级层。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的表达式节点，失败返回 NULL
 */
FormulaNode *parse_python_expression(ParserContext *ctx) {
    FormulaNode *left = parse_python_term(ctx);
    if (!left)
        return NULL;

    while (true) {
        skip_whitespace(ctx);
        char c = peek(ctx);

        NodeType op_type;
        bool should_continue = false;

        if (c == '+') {
            consume(ctx);
            op_type = NODE_BINARY_OP_ADD;
            should_continue = true;
        } else if (c == '-') {
            consume(ctx);
            op_type = NODE_BINARY_OP_SUB;
            should_continue = true;
        }

        if (!should_continue)
            break;

        skip_whitespace(ctx);
        FormulaNode *right = parse_python_term(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }

        left = track_node(ctx, formula_create_binary_op(op_type, left, right));
        if (!left)
            return NULL;
    }

    /* 检查等式 */
    skip_whitespace(ctx);
    if (match_and_consume(ctx, "==")) {
        skip_whitespace(ctx);
        FormulaNode *right = parse_python_expression(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }
        return track_node(ctx, formula_create_equation(left, right));
    }

    return left;
}

/* formula_parse 定义在 formula_parser.c 中，此处不再重复 */
