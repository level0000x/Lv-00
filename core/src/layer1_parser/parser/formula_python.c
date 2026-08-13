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
#include "lv/lv_str_utils.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"

FormulaNode *parse_python_expression(ParserContext *ctx);
static FormulaNode *parse_python_term(ParserContext *ctx);
static FormulaNode *parse_python_factor(ParserContext *ctx);
static FormulaNode *parse_python_atom(ParserContext *ctx);

/* ── 数学函数名→节点创建 分发表（与 formula_dsl.c 共享；MathFuncEntry 类型与
 *   formula_apply_math_func 声明见 lv/formula_parser.h） ── */

/** @brief Python 数学函数表（含 pow；Python 不识别 ln/log） */
static const MathFuncEntry kPythonMathFuncTable[] = {
    {"sqrt", 1, NODE_UNARY_OP_SQRT, false},
    {"sin", 1, NODE_UNARY_OP_SIN, false},
    {"cos", 1, NODE_UNARY_OP_COS, false},
    {"tan", 1, NODE_UNARY_OP_TAN, false},
    {"abs", 1, NODE_UNARY_OP_ABS, false},
    {"pow", 2, NODE_BINARY_OP_POW, true},
};

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
    formula_skip_whitespace(ctx);

    char c = formula_peek(ctx);

    /* 数字 */
    if (formula_is_digit(c) || (c == '.' && formula_is_digit(formula_peek_next(ctx)))) {
        return formula_parse_number(ctx);
    }

    /* 括号表达式 */
    if (c == '(') {
        formula_consume(ctx);
        formula_skip_whitespace(ctx);
        FormulaNode *expr = parse_python_expression(ctx);
        if (!expr)
            return NULL;
        formula_skip_whitespace(ctx);
        if (!formula_expect_char(ctx, ')')) {
            formula_node_destroy(expr);
            return NULL;
        }
        return expr;
    }

    /* 负号 */
    if (c == '-') {
        formula_consume(ctx);
        FormulaNode *operand = parse_python_factor(ctx);
        if (!operand)
            return NULL;
        return formula_track_node(ctx, formula_create_unary_op(NODE_UNARY_OP_NEG, operand));
    }

    /* 正号 */
    if (c == '+') {
        formula_consume(ctx);
        return parse_python_factor(ctx);
    }

    /* 标识符 */
    if (formula_is_alpha(c)) {
        char *ident = formula_parse_identifier_str(ctx);
        if (!ident)
            return NULL;

        formula_skip_whitespace(ctx);

        /* 检查布尔值 */
        if (lv_str_eq(ident, "True") || lv_str_eq(ident, "False")) {
            int val = lv_str_eq(ident, "True") ? 1 : 0;
            lv_free((void **) &ident);
            return formula_create_number(val, 1);
        }
        if (lv_str_eq(ident, "None")) {
            lv_free((void **) &ident);
            return formula_create_variable("None");
        }
        if (lv_str_eq(ident, "pi")) {
            lv_free((void **) &ident);
            return formula_create_variable("pi");
        }

        /* 函数调用 */
        if (formula_peek(ctx) == '(') {
            formula_consume(ctx);
            formula_skip_whitespace(ctx);

            /* 解析参数 */
            FormulaNode *args[lv_MAX_ARGUMENTS] = {NULL};
            int arg_count = 0;
            /* 运行时上限来自 lvConfig.parser.parser_max_arguments（默认 16），
               并以编译期数组维度为硬上限，防止配置调大时栈数组越界 */
            const lvConfig *lv_cfg = lv_config_current();
            int arg_cap = lv_cfg->parser.parser_max_arguments;
            if (arg_cap > lv_MAX_ARGUMENTS)
                arg_cap = lv_MAX_ARGUMENTS;
            while (!formula_is_at_end(ctx) && formula_peek(ctx) != ')') {
                if (arg_count >= arg_cap) {
                    formula_set_error(ctx, "Too many arguments");
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

                formula_skip_whitespace(ctx);

                if (formula_peek(ctx) == ',') {
                    formula_consume(ctx);
                    formula_skip_whitespace(ctx);
                }
            }

            if (!formula_expect_char(ctx, ')')) {
                lv_free((void **) &ident);
                for (int i = 0; i < arg_count; i++)
                    formula_node_destroy(args[i]);
                return NULL;
            }

            /* 根据函数名查表创建对应节点（替代 6 分支 strcmp 链） */
            FormulaNode *node = formula_apply_math_func(ident, args, arg_count, kPythonMathFuncTable,
                                                        sizeof(kPythonMathFuncTable) / sizeof(kPythonMathFuncTable[0]));
            if (!node)
                node = formula_create_variable(ident); /* 未知函数，作为变量返回 */

            lv_free((void **) &ident);
            for (int i = 0; i < arg_count; i++)
                formula_node_destroy(args[i]);
            return node;
        }

        FormulaNode *node = formula_create_variable(ident);
        lv_free((void **) &ident);
        return node;
    }

    formula_set_error(ctx, "Unexpected character in Python expression");
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

    formula_skip_whitespace(ctx);

    /* 处理幂运算 ** */
    if (formula_match_string(ctx, "**")) {
        formula_consume(ctx);
        formula_consume(ctx);
        formula_skip_whitespace(ctx);
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
        formula_skip_whitespace(ctx);
        char c = formula_peek(ctx);

        NodeType op_type = NODE_BINARY_OP_MUL;
        bool should_continue = false;

        if (c == '*') {
            if (formula_peek_next(ctx) == '*')
                break; /* 幂运算 */
            formula_consume(ctx);
            should_continue = true;
        } else if (c == '/') {
            formula_consume(ctx);
            op_type = NODE_BINARY_OP_DIV;
            should_continue = true;
        } else if (c == '%') {
            formula_consume(ctx);
            should_continue = true;
        }

        if (!should_continue)
            break;

        formula_skip_whitespace(ctx);
        FormulaNode *right = parse_python_factor(ctx);
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
        FormulaNode *right = parse_python_term(ctx);
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
    if (formula_match_and_consume(ctx, "==")) {
        formula_skip_whitespace(ctx);
        FormulaNode *right = parse_python_expression(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }
        return formula_track_node(ctx, formula_create_equation(left, right));
    }

    return left;
}

/* formula_parse 定义在 formula_parser.c 中，此处不再重复 */
