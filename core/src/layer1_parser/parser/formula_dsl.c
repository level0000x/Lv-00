/**
 * @file formula_dsl.c
 * @brief DSL 语法解析器
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/formula_parser.h"
#include "lv/lv_arith_safe.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_parse_utils.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"

/* formula_node_copy 实现在 formula_ast.c 中 */

/* ============================================================
 * 安全性辅助函数
 * ============================================================ */

/**
 * @brief 追踪AST节点创建并检查安全限制
 *
 * 每次创建AST节点时调用，递增节点计数器并检查是否超过
 * lv_MAX_AST_NODES 上限。超限时设置错误状态并返回false。
 *
 * @param[in,out] ctx 解析器上下文
 * @param[in]     node 新创建的AST节点
 * @return 新创建的节点（如果超限则释放并返回NULL）
 */
FormulaNode *formula_track_node(ParserContext *ctx, FormulaNode *node) {
    if (!node)
        return NULL;
    ctx->node_count++;
    /* 节点数上限来自 lvConfig.parser.parser_max_ast_nodes（默认 500000） */
    if (ctx->node_count > (int) lv_config_current()->parser.parser_max_ast_nodes) {
        formula_set_error(ctx, "AST节点数超过安全上限");
        formula_node_destroy(node);
        return NULL;
    }
    return node;
}

/* ============================================================
 * 解析数字
 * ============================================================ */

/**
 * @brief 解析数字字面量
 *
 * 解析输入中的数字，支持整数、浮点数和科学计数法表示。
 * 解析策略：
 * 1. 首先扫描数字的字符表示（整数部分、可选小数部分、可选指数部分）
 * 2. 对于浮点数和科学计数法，使用精确分数算法将数值转换为有理数表示
 * 3. 返回分数形式的 NUMBER 节点，整数则转换为 denominator=1 的分数
 *
 * 浮点数转换算法：
 * - 将浮点数表示为整数/分母的分数形式
 * - 例如 3.14 -> (314/100) -> (157/50)（约分后）
 * - 使用欧几里得算法计算 GCD 进行约分
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的数字节点，失败返回 NULL
 */
FormulaNode *formula_parse_number(ParserContext *ctx) {
    size_t start = ctx->pos;
    bool has_dot = false;
    bool has_exponent = false;

    /* 整数部分 */
    while (formula_is_digit(formula_peek(ctx))) {
        formula_consume(ctx);
    }

    /* 小数部分 */
    if (formula_peek(ctx) == '.' && formula_is_digit(formula_peek_next(ctx))) {
        has_dot = true;
        formula_consume(ctx); /* 消费 '.' */
        while (formula_is_digit(formula_peek(ctx))) {
            formula_consume(ctx);
        }
    }

    /* 科学计数法 */
    if (formula_peek(ctx) == 'e' || formula_peek(ctx) == 'E') {
        has_exponent = true;
        formula_consume(ctx);
        if (formula_peek(ctx) == '+' || formula_peek(ctx) == '-') {
            formula_consume(ctx);
        }
        if (!formula_is_digit(formula_peek(ctx))) {
            formula_set_error(ctx, "Expected digit after exponent");
            return NULL;
        }
        while (formula_is_digit(formula_peek(ctx))) {
            formula_consume(ctx);
        }
    }

    /* 提取数字字符串 */
    size_t len = ctx->pos - start;
    char *num_str = lv_malloc(len + 1);
    if (!num_str) {
        formula_set_error(ctx, "Memory allocation failed");
        return NULL;
    }
    /* 使用 lv_strlcpy_n 进行精确长度复制（已分配 len+1 字节，自动零终止） */
    lv_strlcpy_n(num_str, len + 1, ctx->input + start, len);

    /* 转换为数值（失败回退 0.0，与 strtod 无转换时返回 0.0 一致） */
    double value = 0.0;
    if (lv_parse_double(num_str, &value) != 0)
        value = 0.0;
    lv_free((void **) &num_str);

    /* 创建节点 */
    FormulaNode *node = NULL;

    if (has_dot || has_exponent) {
        /**
         * 浮点数精确转换算法：
         * 1. 将浮点数表示为分数形式（整数部分 + 小数部分/分母）
         * 2. 例如 3.14 -> 整数部分 3, 小数 "14" -> 14/100
         * 3. 合并：(3 * 100 + 14) / 100 = 314/100
         * 4. 使用 GCD 约分以获得最简分数：314/100 -> 157/50
         */
        /* 重新提取小数字符串以获得精确的小数位数 */
        int64_t int_part = 0;
        int64_t frac_part = 0;
        int64_t frac_denom = 1;
        bool negative = false;

        /* 从原始输入重新解析 */
        const char *p = ctx->input + start;
        const char *end = ctx->input + ctx->pos;

        if (*p == '-') {
            negative = true;
            p++;
        } else if (*p == '+') {
            p++;
        }

        /* 步骤1：解析符号后的整数部分 */
        while (p < end && formula_is_digit((unsigned char) *p)) {
            /* 溢出保护：int_part * 10 + digit 不得超过 INT64_MAX */
            if (int_part > (INT64_MAX - (*p - '0')) / 10) {
                formula_set_error(ctx, "整数部分溢出");
                return NULL;
            }
            int_part = int_part * 10 + (*p - '0');
            p++;
        }

        /* 步骤2：解析小数点后数字并计算分母 */
        if (p < end && *p == '.') {
            p++;
            while (p < end && formula_is_digit((unsigned char) *p)) {
                /* 溢出保护：frac_part * 10 + digit 不得超过 INT64_MAX */
                if (frac_part > (INT64_MAX - (*p - '0')) / 10) {
                    formula_set_error(ctx, "小数部分溢出");
                    return NULL;
                }
                frac_part = frac_part * 10 + (*p - '0');
                /* 溢出保护：frac_denom * 10 不得超过 INT64_MAX */
                if (frac_denom > INT64_MAX / 10) {
                    formula_set_error(ctx, "小数分母溢出");
                    return NULL;
                }
                frac_denom *= 10; /* 每位小数使分母乘以10 */
                p++;
            }
        }

        /* 步骤3：处理科学计数法指数 */
        if (p < end && (*p == 'e' || *p == 'E')) {
            p++;
            int exp_sign = 1;
            int exp_val = 0;
            if (p < end && *p == '-') {
                exp_sign = -1;
                p++;
            } else if (p < end && *p == '+') {
                p++;
            }
            while (p < end && formula_is_digit((unsigned char) *p)) {
                exp_val = exp_val * 10 + (*p - '0');
                p++;
            }
            /**
             * 步骤4：将指数应用到分母
             * - 正指数：分母缩小（分子乘以10^exp）
             * - 负指数：分母增大
             */
            if (exp_sign > 0) {
                /* 正指数：分母缩小，即分子乘以 10^exp */
                for (int i = 0; i < exp_val; i++) {
                    frac_denom /= 10;
                    if (frac_denom == 0) {
                        frac_denom = 1;
                        break;
                    }
                }
            } else {
                /* 负指数：分母增大 */
                for (int i = 0; i < -exp_sign * exp_val; i++) {
                    frac_denom *= 10;
                }
            }
        }

        /**
         * 步骤5：合并整数和小数部分
         * value = int_part + frac_part/frac_denom
         *        = (int_part * frac_denom + frac_part) / frac_denom
         */
        /* 溢出检查：int_part * frac_denom 可能超出 int64_t 范围 */
        int64_t numerator;
        if (int_part != 0 && frac_denom != 0) {
            if (int_part > 0) {
                if (frac_denom > INT64_MAX / int_part) {
                    /* 乘法溢出，钳位到 INT64_MAX */
                    numerator = INT64_MAX;
                } else {
                    numerator = int_part * frac_denom + frac_part;
                }
            } else {
                /* int_part < 0 */
                if (frac_denom > INT64_MAX / (-int_part)) {
                    /* 乘法溢出，钳位到 INT64_MIN */
                    numerator = INT64_MIN;
                } else {
                    numerator = int_part * frac_denom + frac_part;
                }
            }
        } else {
            numerator = frac_part;
        }
        int64_t denominator = frac_denom;

        if (negative) {
            numerator = -numerator;
        }

        /**
         * 步骤6：使用欧几里得算法计算 GCD 进行约分
         * GCD(a, b) = GCD(b, a % b)，直到 b = 0
         * （复用公共设施 lv_rational_simplify_i64，gcd 采用
         *  uint64 安全语义，正确处理 INT64_MIN 绝对值）
         */
        if (numerator != 0 && denominator > 0) {
            lv_rational_simplify_i64(&numerator, &denominator);
        }

        node = formula_create_number(numerator, (uint64_t) denominator);
        if (node) {
            node->line = ctx->line;
            node->column = ctx->column;
            node->data.number.is_integer = (denominator == 1);
        }
    } else {
        /* 纯整数 */
        node = formula_create_number((int64_t) value, 1);
        if (node) {
            node->line = ctx->line;
            node->column = ctx->column;
        }
    }
    return formula_track_node(ctx, node);
}

/* ============================================================
 * 解析标识符
 * ============================================================ */

/**
 * @brief 解析标识符字符串
 *
 * 从当前位置开始解析一个标识符，标识符由字母、数字和下划线组成，
 * 必须以字母或下划线开头。解析完成后指针会移动到标识符之后的位置。
 *
 * 算法步骤：
 * 1. 首先检查当前字符是否为有效的标识符起始字符（formula_is_alpha）
 * 2. 记录起始位置，然后连续消费所有字母数字字符
 * 3. 计算标识符长度并分配内存
 * 4. 使用 memcpy 复制标识符内容并添加 null 终止符
 *
 * @param ctx 解析器上下文指针
 * @return char* 解析出的标识符字符串（需调用者释放），失败返回 NULL
 * @retval NULL 解析失败，错误信息已设置到上下文中
 */
char *formula_parse_identifier_str(ParserContext *ctx) {
    if (!formula_is_alpha(formula_peek(ctx))) {
        formula_set_error(ctx, "Expected identifier");
        return NULL;
    }

    size_t start = ctx->pos;
    while (formula_is_alnum(formula_peek(ctx))) {
        formula_consume(ctx);
    }

    size_t len = ctx->pos - start;

    /* 安全加固：检查token长度限制（上限来自 lvConfig.parser.parser_max_token_length，默认 4096） */
    if (len > (size_t) lv_config_current()->parser.parser_max_token_length) {
        formula_set_error(ctx, "Identifier too long");
        return NULL;
    }

    char *ident = lv_malloc(len + 1);
    if (!ident) {
        formula_set_error(ctx, "Memory allocation failed");
        return NULL;
    }
    /* 使用 lv_strlcpy_n 进行精确长度复制（已分配 len+1 字节，自动零终止） */
    lv_strlcpy_n(ident, len + 1, ctx->input + start, len);
    return ident;
}

/**
 * @brief 检查字符串是否为 DSL 关键字
 *
 * 在 formula_dsl_keywords 表中进行线性查找，判断给定字符串是否为
 * 有效的 DSL 关键字（如 point, segment, circle 等几何元素
 * 或 perpendicular, parallel 等约束关键字）。
 *
 * @param str 要检查的字符串
 * @return true 字符串是 DSL 关键字
 * @return false 字符串不是 DSL 关键字
 */
static bool is_dsl_keyword(const char *str) {
    for (int i = 0; formula_dsl_keywords[i] != NULL; i++) {
        if (lv_str_eq(str, formula_dsl_keywords[i])) {
            return true;
        }
    }
    return false;
}

/* ============================================================
 * 语法检测
 * ============================================================ */

/**
 * @brief 自动检测输入公式的语法类型
 *
 * 通过检查输入字符串中的特征关键字来判断语法类型：
 *   - 包含 LaTeX 命令（如 \\frac, \\sqrt）-> "latex"
 *   - 包含 Python 特征（如 **, ==, sqrt(）-> "python"
 *   - 包含 DSL 关键字（如 point, segment）-> "dsl"
 *   - 无法识别 -> "unknown"
 *
 * @param input 输入公式字符串
 * @return 语法类型字符串（"latex"/"python"/"dsl"/"unknown"）
 */
const char *formula_detect_syntax(const char *input) {
    if (!input || !*input) {
        return "unknown";
    }

    /* 检测 LaTeX 命令 */
    if (lv_str_match_any(input, formula_latex_commands) >= 0) {
        return "latex";
    }

    /* 检测 DSL 关键字（逐词扫描 + 边界校验） */
    const char *p = input;
    while (*p) {
        /* 跳过空白（统一 lv_str_ltrim，lv_str_ltrim 不修改原串） */
        p = lv_str_ltrim((char *) p);
        if (!*p)
            break;

        /* 检查是否为关键字（命中后必须为空白或分隔符） */
        if (lv_str_match_delimited(p, formula_dsl_keywords) >= 0) {
            return "dsl";
        }

        /* 移动到下一个单词 */
        while (*p && !isspace((unsigned char) *p))
            p++;
    }

    /* 检测 Python 特征 */
    if (lv_str_match_any(input, formula_python_features) >= 0) {
        return "python";
    }

    /* 默认返回 DSL */
    return "dsl";
}

/* ============================================================
 * DSL 解析器
 * ============================================================ */

/* 前向声明 */
static FormulaNode *parse_dsl_expression(ParserContext *ctx);
static FormulaNode *parse_dsl_term(ParserContext *ctx);
static FormulaNode *parse_dsl_factor(ParserContext *ctx);
static FormulaNode *parse_dsl_atom(ParserContext *ctx);
static FormulaNode *parse_dsl_statement(ParserContext *ctx);

/**
 * @brief 解析 DSL 点定义
 *
 * 解析 DSL 语法中的点定义，格式为：point Name(x, y, ...)
 * 支持任意维度的坐标点。
 *
 * 语法：
 * @code
 * point A(1, 2)        // 2D 点
 * point B(1, 2, 3)     // 3D 点
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何点节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_point(ParserContext *ctx) {
    formula_skip_whitespace(ctx);

    /* 解析点名称 */
    char *name = formula_parse_identifier_str(ctx);
    if (!name) {
        formula_set_error(ctx, "Expected point name");
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 '(' */
    if (!formula_expect_char(ctx, '(')) {
        lv_free((void **) &name);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 解析坐标列表 */
    FormulaNode *coords[lv_MAX_COORDINATES] = {NULL};
    int coord_count = 0;
    /* 运行时上限来自 lvConfig.parser.parser_max_coordinates（默认 16），
       并以编译期数组维度为硬上限，防止配置调大时栈数组越界 */
    const lvConfig *lv_cfg = lv_config_current();
    int coord_cap = lv_cfg->parser.parser_max_coordinates;
    if (coord_cap > lv_MAX_COORDINATES)
        coord_cap = lv_MAX_COORDINATES;

    while (!formula_is_at_end(ctx) && formula_peek(ctx) != ')') {
        formula_skip_whitespace(ctx);

        if (coord_count >= coord_cap) {
            formula_set_error(ctx, "Too many coordinates");
            lv_free((void **) &name);
            for (int i = 0; i < coord_count; i++)
                formula_node_destroy(coords[i]);
            return NULL;
        }

        coords[coord_count] = parse_dsl_expression(ctx);
        if (!coords[coord_count]) {
            lv_free((void **) &name);
            for (int i = 0; i < coord_count; i++)
                formula_node_destroy(coords[i]);
            return NULL;
        }
        coord_count++;

        formula_skip_whitespace(ctx);

        if (formula_peek(ctx) == ',') {
            formula_consume(ctx);
        } else if (formula_peek(ctx) != ')') {
            formula_set_error(ctx, "Expected ',' or ')'");
            lv_free((void **) &name);
            for (int i = 0; i < coord_count; i++)
                formula_node_destroy(coords[i]);
            return NULL;
        }
    }

    /* 期望 ')' */
    if (!formula_expect_char(ctx, ')')) {
        lv_free((void **) &name);
        for (int i = 0; i < coord_count; i++)
            formula_node_destroy(coords[i]);
        return NULL;
    }

    FormulaNode *coord_list = formula_create_coord_list(coords, coord_count);
    for (int i = 0; i < coord_count; i++)
        formula_node_destroy(coords[i]);

    FormulaNode *node = formula_create_geom_point(name, coord_list);
    lv_free((void **) &name);
    return node;
}

/**
 * @brief 解析 DSL 线段定义
 *
 * 解析 DSL 语法中的线段定义，格式为：segment Name(P1, P2)
 * 其中 P1 和 P2 是已定义的点标识符。
 *
 * 语法：
 * @code
 * segment AB(A, B)
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何线段节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_segment(ParserContext *ctx) {
    formula_skip_whitespace(ctx);

    /* 解析线段名称 */
    char *name = formula_parse_identifier_str(ctx);
    if (!name) {
        formula_set_error(ctx, "Expected segment name");
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 '(' */
    if (!formula_expect_char(ctx, '(')) {
        lv_free((void **) &name);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 解析起点 */
    FormulaNode *ep1 = parse_dsl_atom(ctx);
    if (!ep1) {
        lv_free((void **) &name);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 ',' */
    if (!formula_expect_char(ctx, ',')) {
        lv_free((void **) &name);
        formula_node_destroy(ep1);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 解析终点 */
    FormulaNode *ep2 = parse_dsl_atom(ctx);
    if (!ep2) {
        lv_free((void **) &name);
        formula_node_destroy(ep1);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 ')' */
    if (!formula_expect_char(ctx, ')')) {
        lv_free((void **) &name);
        formula_node_destroy(ep1);
        formula_node_destroy(ep2);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_segment(name, ep1, ep2);
    lv_free((void **) &name);
    return node;
}

/**
 * @brief 解析 DSL 圆定义
 *
 * 解析 DSL 语法中的圆定义，格式为：circle Name(Center, Radius)
 * 其中 Center 是圆心点标识符，Radius 是半径表达式。
 *
 * 语法：
 * @code
 * circle O(center, 5)
 * circle C(center, r)
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何圆节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_circle(ParserContext *ctx) {
    formula_skip_whitespace(ctx);

    /* 解析圆名称 */
    char *name = formula_parse_identifier_str(ctx);
    if (!name) {
        formula_set_error(ctx, "Expected circle name");
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 '(' */
    if (!formula_expect_char(ctx, '(')) {
        lv_free((void **) &name);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 解析圆心 */
    FormulaNode *center = parse_dsl_atom(ctx);
    if (!center) {
        lv_free((void **) &name);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 ',' */
    if (!formula_expect_char(ctx, ',')) {
        lv_free((void **) &name);
        formula_node_destroy(center);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 解析半径 */
    FormulaNode *radius = parse_dsl_expression(ctx);
    if (!radius) {
        lv_free((void **) &name);
        formula_node_destroy(center);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 ')' */
    if (!formula_expect_char(ctx, ')')) {
        lv_free((void **) &name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_circle(name, center, radius);
    lv_free((void **) &name);
    return node;
}

/**
 * @brief 解析 DSL 三角形定义
 *
 * 解析 DSL 语法中的三角形定义，格式为：triangle Name(V1, V2, V3)
 * 其中 V1、V2、V3 是三角形的三个顶点标识符。
 *
 * 语法：
 * @code
 * triangle ABC(A, B, C)
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何三角形节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_triangle(ParserContext *ctx) {
    formula_skip_whitespace(ctx);

    /* 解析三角形名称 */
    char *name = formula_parse_identifier_str(ctx);
    if (!name) {
        formula_set_error(ctx, "Expected triangle name");
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 '(' */
    if (!formula_expect_char(ctx, '(')) {
        lv_free((void **) &name);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 解析三个顶点 */
    FormulaNode *vertices[3] = {NULL, NULL, NULL};
    for (int i = 0; i < 3; i++) {
        vertices[i] = parse_dsl_atom(ctx);
        if (!vertices[i]) {
            lv_free((void **) &name);
            for (int j = 0; j < i; j++)
                formula_node_destroy(vertices[j]);
            formula_set_error(ctx, "Expected vertex");
            return NULL;
        }

        formula_skip_whitespace(ctx);

        if (i < 2) {
            if (!formula_expect_char(ctx, ',')) {
                lv_free((void **) &name);
                for (int j = 0; j <= i; j++)
                    formula_node_destroy(vertices[j]);
                return NULL;
            }
            formula_skip_whitespace(ctx);
        }
    }

    /* 期望 ')' */
    if (!formula_expect_char(ctx, ')')) {
        lv_free((void **) &name);
        for (int i = 0; i < 3; i++)
            formula_node_destroy(vertices[i]);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_triangle(name, vertices[0], vertices[1], vertices[2]);
    lv_free((void **) &name);
    for (int i = 0; i < 3; i++)
        formula_node_destroy(vertices[i]);
    return node;
}

/**
 * @brief 解析 DSL 弧定义
 *
 * 解析 DSL 语法中的弧定义，格式为：arc Name(Center, Radius, StartAngle, EndAngle)
 * 用于定义一段圆弧，包含起始和结束角度。
 *
 * 语法：
 * @code
 * arc A(center, 5, 0, pi/2)
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何弧节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_arc(ParserContext *ctx) {
    formula_skip_whitespace(ctx);

    /* 解析弧名称 */
    char *name = formula_parse_identifier_str(ctx);
    if (!name) {
        formula_set_error(ctx, "Expected arc name");
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 '(' */
    if (!formula_expect_char(ctx, '(')) {
        lv_free((void **) &name);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 解析圆心 */
    FormulaNode *center = parse_dsl_atom(ctx);
    if (!center) {
        lv_free((void **) &name);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 ',' */
    if (!formula_expect_char(ctx, ',')) {
        lv_free((void **) &name);
        formula_node_destroy(center);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 解析半径 */
    FormulaNode *radius = parse_dsl_expression(ctx);
    if (!radius) {
        lv_free((void **) &name);
        formula_node_destroy(center);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 ',' */
    if (!formula_expect_char(ctx, ',')) {
        lv_free((void **) &name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 解析起始角度 */
    FormulaNode *start_angle = parse_dsl_expression(ctx);
    if (!start_angle) {
        lv_free((void **) &name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 ',' */
    if (!formula_expect_char(ctx, ',')) {
        lv_free((void **) &name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        formula_node_destroy(start_angle);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 解析结束角度 */
    FormulaNode *end_angle = parse_dsl_expression(ctx);
    if (!end_angle) {
        lv_free((void **) &name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        formula_node_destroy(start_angle);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 ')' */
    if (!formula_expect_char(ctx, ')')) {
        lv_free((void **) &name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        formula_node_destroy(start_angle);
        formula_node_destroy(end_angle);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_arc(name, center, radius, start_angle, end_angle);
    lv_free((void **) &name);
    formula_node_destroy(center);
    formula_node_destroy(radius);
    formula_node_destroy(start_angle);
    formula_node_destroy(end_angle);
    return node;
}

/**
 * @brief 解析 DSL 多边形定义
 *
 * 解析 DSL 语法中的多边形定义，格式为：polygon Name([V1, V2, V3, ...])
 * 支持任意顶点数量的多边形。
 *
 * 语法：
 * @code
 * polygon P([A, B, C, D])  // 四边形
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何多边形节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_polygon(ParserContext *ctx) {
    formula_skip_whitespace(ctx);

    /* 解析多边形名称 */
    char *name = formula_parse_identifier_str(ctx);
    if (!name) {
        formula_set_error(ctx, "Expected polygon name");
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 '(' */
    if (!formula_expect_char(ctx, '(')) {
        lv_free((void **) &name);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 '[' */
    if (!formula_expect_char(ctx, '[')) {
        lv_free((void **) &name);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 解析顶点列表 */
    FormulaNode *vertices[lv_MAX_POLYGON_VERTICES] = {NULL};
    int vertex_count = 0;
    /* 运行时上限来自 lvConfig.parser.parser_max_polygon_vertices（默认 32），
       并以编译期数组维度为硬上限，防止配置调大时栈数组越界 */
    const lvConfig *lv_cfg = lv_config_current();
    int vertex_cap = lv_cfg->parser.parser_max_polygon_vertices;
    if (vertex_cap > lv_MAX_POLYGON_VERTICES)
        vertex_cap = lv_MAX_POLYGON_VERTICES;

    while (!formula_is_at_end(ctx) && formula_peek(ctx) != ']') {
        if (vertex_count >= vertex_cap) {
            formula_set_error(ctx, "Too many vertices in polygon");
            lv_free((void **) &name);
            for (int i = 0; i < vertex_count; i++)
                formula_node_destroy(vertices[i]);
            return NULL;
        }

        vertices[vertex_count] = parse_dsl_atom(ctx);
        if (!vertices[vertex_count]) {
            lv_free((void **) &name);
            for (int i = 0; i < vertex_count; i++)
                formula_node_destroy(vertices[i]);
            return NULL;
        }
        vertex_count++;

        formula_skip_whitespace(ctx);

        if (formula_peek(ctx) == ',') {
            formula_consume(ctx);
            formula_skip_whitespace(ctx);
        } else if (formula_peek(ctx) != ']') {
            formula_set_error(ctx, "Expected ',' or ']'");
            lv_free((void **) &name);
            for (int i = 0; i < vertex_count; i++)
                formula_node_destroy(vertices[i]);
            return NULL;
        }
    }

    /* 期望 ']' */
    if (!formula_expect_char(ctx, ']')) {
        lv_free((void **) &name);
        for (int i = 0; i < vertex_count; i++)
            formula_node_destroy(vertices[i]);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 ')' */
    if (!formula_expect_char(ctx, ')')) {
        lv_free((void **) &name);
        for (int i = 0; i < vertex_count; i++)
            formula_node_destroy(vertices[i]);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_polygon(name, vertices, vertex_count);
    lv_free((void **) &name);
    for (int i = 0; i < vertex_count; i++)
        formula_node_destroy(vertices[i]);
    return node;
}

/**
 * @brief 解析 DSL 区域定义
 *
 * 解析 DSL 语法中的区域定义，格式为：region Name([seg1, seg2, ...])
 * 区域由边界线段列表定义。
 *
 * 语法：
 * @code
 * region R([AB, BC, CD, DA])
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何区域节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_region(ParserContext *ctx) {
    formula_skip_whitespace(ctx);

    /* 解析区域名称 */
    char *name = formula_parse_identifier_str(ctx);
    if (!name) {
        formula_set_error(ctx, "Expected region name");
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 '(' */
    if (!formula_expect_char(ctx, '(')) {
        lv_free((void **) &name);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 '[' */
    if (!formula_expect_char(ctx, '[')) {
        lv_free((void **) &name);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 解析边界线段列表 */
    FormulaNode *segments[lv_MAX_POLYGON_VERTICES] = {NULL};
    int segment_count = 0;
    /* 运行时上限来自 lvConfig.parser.parser_max_polygon_vertices（默认 32），
       并以编译期数组维度为硬上限，防止配置调大时栈数组越界 */
    const lvConfig *lv_cfg = lv_config_current();
    int segment_cap = lv_cfg->parser.parser_max_polygon_vertices;
    if (segment_cap > lv_MAX_POLYGON_VERTICES)
        segment_cap = lv_MAX_POLYGON_VERTICES;

    while (!formula_is_at_end(ctx) && formula_peek(ctx) != ']') {
        if (segment_count >= segment_cap) {
            formula_set_error(ctx, "Too many segments in region");
            lv_free((void **) &name);
            for (int i = 0; i < segment_count; i++)
                formula_node_destroy(segments[i]);
            return NULL;
        }

        segments[segment_count] = parse_dsl_atom(ctx);
        if (!segments[segment_count]) {
            lv_free((void **) &name);
            for (int i = 0; i < segment_count; i++)
                formula_node_destroy(segments[i]);
            return NULL;
        }
        segment_count++;

        formula_skip_whitespace(ctx);

        if (formula_peek(ctx) == ',') {
            formula_consume(ctx);
            formula_skip_whitespace(ctx);
        } else if (formula_peek(ctx) != ']') {
            formula_set_error(ctx, "Expected ',' or ']'");
            lv_free((void **) &name);
            for (int i = 0; i < segment_count; i++)
                formula_node_destroy(segments[i]);
            return NULL;
        }
    }

    /* 期望 ']' */
    if (!formula_expect_char(ctx, ']')) {
        lv_free((void **) &name);
        for (int i = 0; i < segment_count; i++)
            formula_node_destroy(segments[i]);
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 期望 ')' */
    if (!formula_expect_char(ctx, ')')) {
        lv_free((void **) &name);
        for (int i = 0; i < segment_count; i++)
            formula_node_destroy(segments[i]);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_region(name, segments, segment_count);
    lv_free((void **) &name);
    for (int i = 0; i < segment_count; i++)
        formula_node_destroy(segments[i]);
    return node;
}

/** @brief DSL 几何元素关键字→解析函数 分发表（替代 parse_dsl_atom 的 7 分支 strcmp 链，
 *         风格与同文件 kConstraintTypes 一致） */
typedef struct {
    const char *name; /**< 关键字 */
    FormulaNode *(*parse)(ParserContext *ctx); /**< 解析函数 */
} DslGeometryEntry;

static const DslGeometryEntry kDslGeometryKeywords[] = {
    {"point", parse_dsl_point},
    {"segment", parse_dsl_segment},
    {"circle", parse_dsl_circle},
    {"triangle", parse_dsl_triangle},
    {"arc", parse_dsl_arc},
    {"polygon", parse_dsl_polygon},
    {"region", parse_dsl_region},
};

/* 约束名称 -> 节点类型映射条目 */
typedef struct {
    const char *name; /**< DSL 约束名称 */
    NodeType type;    /**< 对应的 AST 节点类型 */
} ConstraintTypeEntry;

/* 约束名称查找表（精确匹配，strcmp 语义） */
static const ConstraintTypeEntry kConstraintTypes[] = {
    {"perpendicular", NODE_CONSTRAINT_PERPENDICULAR},
    {"parallel", NODE_CONSTRAINT_PARALLEL},
    {"midpoint", NODE_CONSTRAINT_MIDPOINT},
    {"bisector", NODE_CONSTRAINT_BISECTOR},
    {"collinear", NODE_CONSTRAINT_COLLINEAR},
    {"tangent", NODE_CONSTRAINT_TANGENT},
    {"congruent", NODE_CONSTRAINT_CONGRUENT},
};

/**
 * @brief 根据名称获取几何约束类型
 *
 * 将 DSL 约束名称字符串转换为对应的 AST 节点类型。
 * 支持的约束类型包括：perpendicular（垂直）、parallel（平行）、
 * midpoint（中点）、bisector（角平分线）、collinear（共线）、
 * tangent（相切）、congruent（全等）。
 *
 * @param name 约束名称字符串
 * @return NodeType 对应的节点类型，如果未知则返回 (NodeType)-1
 * @note 返回 (NodeType)-1 时表示未知约束类型，调用方应处理此情况
 */
static NodeType get_constraint_type(const char *name) {
    for (size_t i = 0; i < sizeof(kConstraintTypes) / sizeof(kConstraintTypes[0]); i++) {
        if (lv_str_eq(name, kConstraintTypes[i].name))
            return kConstraintTypes[i].type;
    }
    return (NodeType) -1; /* 未知约束类型，由调用方处理 */
}

/**
 * @brief 解析 DSL 几何约束
 *
 * 解析 DSL 语法中的几何约束定义，格式为：ConstraintName(P1, P2, ...)
 *
 * 支持的约束类型：
 * - perpendicular(AB, CD): AB 垂直于 CD
 * - parallel(AB, CD): AB 平行于 CD
 * - midpoint(M, AB): M 是线段 AB 的中点
 * - bisector(L, A, B, C): L 是角 ABC 的角平分线
 * - collinear(A, B, C): 点 A、B、C 共线
 * - tangent(C, L): 圆 C 与直线 L 相切
 * - congruent(AB, CD): 线段 AB 与 CD 全等
 *
 * 语法：
 * @code
 * perpendicular(AB, CD)
 * midpoint(M, AB)
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的约束节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_constraint(ParserContext *ctx) {
    formula_skip_whitespace(ctx);

    /* 解析约束类型 */
    char *constraint_name = formula_parse_identifier_str(ctx);
    if (!constraint_name) {
        formula_set_error(ctx, "Expected constraint type");
        return NULL;
    }

    NodeType constraint_type = get_constraint_type(constraint_name);
    if ((int) constraint_type < 0) {
        char err_buf[lv_MAX_TEMP_MSG_SIZE];
        snprintf(err_buf, sizeof(err_buf), "未知的约束类型: %s", constraint_name);
        formula_set_error(ctx, err_buf);
        lv_free((void **) &constraint_name);
        return NULL;
    }
    lv_free((void **) &constraint_name);

    formula_skip_whitespace(ctx);

    /* 期望 '(' */
    if (!formula_expect_char(ctx, '(')) {
        return NULL;
    }

    formula_skip_whitespace(ctx);

    /* 解析参数列表 */
    FormulaNode *participants[lv_MAX_PARTICIPANTS] = {NULL};
    int participant_count = 0;
    /* 运行时上限来自 lvConfig.parser.parser_max_participants（默认 16），
       并以编译期数组维度为硬上限，防止配置调大时栈数组越界 */
    const lvConfig *lv_cfg = lv_config_current();
    int participant_cap = lv_cfg->parser.parser_max_participants;
    if (participant_cap > lv_MAX_PARTICIPANTS)
        participant_cap = lv_MAX_PARTICIPANTS;

    while (!formula_is_at_end(ctx) && formula_peek(ctx) != ')') {
        formula_skip_whitespace(ctx);

        if (participant_count >= participant_cap) {
            formula_set_error(ctx, "Too many participants");
            for (int i = 0; i < participant_count; i++)
                formula_node_destroy(participants[i]);
            return NULL;
        }

        participants[participant_count] = parse_dsl_atom(ctx);
        if (!participants[participant_count]) {
            for (int i = 0; i < participant_count; i++)
                formula_node_destroy(participants[i]);
            return NULL;
        }
        participant_count++;

        formula_skip_whitespace(ctx);

        if (formula_peek(ctx) == ',') {
            formula_consume(ctx);
        } else if (formula_peek(ctx) != ')') {
            formula_set_error(ctx, "Expected ',' or ')'");
            for (int i = 0; i < participant_count; i++)
                formula_node_destroy(participants[i]);
            return NULL;
        }
    }

    /* 期望 ')' */
    if (!formula_expect_char(ctx, ')')) {
        for (int i = 0; i < participant_count; i++)
            formula_node_destroy(participants[i]);
        return NULL;
    }

    FormulaNode *node = formula_create_constraint(constraint_type, participants, participant_count);
    for (int i = 0; i < participant_count; i++)
        formula_node_destroy(participants[i]);
    return node;
}

/* ── 数学函数名→节点创建 分发表（formula_dsl / formula_python 共享；
 *   MathFuncEntry 类型与 formula_apply_math_func 声明见 lv/formula_parser.h） ── */

/** @brief DSL 数学函数表（含 ln/log；DSL 不识别 pow） */
static const MathFuncEntry kDslMathFuncTable[] = {
    {"sqrt", 1, NODE_UNARY_OP_SQRT, false},
    {"sin", 1, NODE_UNARY_OP_SIN, false},
    {"cos", 1, NODE_UNARY_OP_COS, false},
    {"tan", 1, NODE_UNARY_OP_TAN, false},
    {"abs", 1, NODE_UNARY_OP_ABS, false},
    {"ln", 1, NODE_UNARY_OP_LN, false},
    {"log", 1, NODE_UNARY_OP_LOG, false},
};

/**
 * @brief 按函数名查表创建数学函数节点（formula_dsl/formula_python 共享，
 *        替代两处几乎一致的手写 strcmp 分发链）
 * @param ident      函数名（如 "sqrt"、"pow"）
 * @param args       已解析的参数节点数组
 * @param arg_count  参数个数
 * @param table      数学函数分发表（按解析器各自声明）
 * @param table_size 表大小
 * @return 命中且参数个数匹配时返回新创建的节点，否则返回 NULL（调用方回退为标识符/变量）
 */
FormulaNode *formula_apply_math_func(const char *ident, FormulaNode **args, int arg_count,
                                     const MathFuncEntry *table, size_t table_size) {
    for (size_t i = 0; i < table_size; i++) {
        if (lv_str_eq(ident, table[i].name) && arg_count == table[i].arg_count) {
            if (table[i].is_binary)
                return formula_create_binary_op(table[i].op, args[0], args[1]);
            return formula_create_unary_op(table[i].op, args[0]);
        }
    }
    return NULL;
}

/**
 * @brief 解析 DSL 原子表达式
 *
 * 解析 DSL 语法中的最小语法单元（原子），包括：
 * - 数字字面量
 * - 括号表达式
 * - 一元负号/正号
 * - 几何元素定义（point, segment, circle, triangle, arc, polygon, region）
 * - 几何约束（perpendicular, parallel 等）
 * - 函数调用
 * - 变量/标识符引用
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的原子节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_atom(ParserContext *ctx) {
    formula_skip_whitespace(ctx);

    char c = formula_peek(ctx);

    /* 数字 */
    if (formula_is_digit(c) || (c == '.' && formula_is_digit(formula_peek_next(ctx)))) {
        return formula_parse_number(ctx);
    }

    /* 括号表达式 */
    if (c == '(') {
        formula_consume(ctx);
        FormulaNode *expr = parse_dsl_expression(ctx);
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
        FormulaNode *operand = parse_dsl_factor(ctx);
        if (!operand)
            return NULL;
        return formula_track_node(ctx, formula_create_unary_op(NODE_UNARY_OP_NEG, operand));
    }

    /* 正号 */
    if (c == '+') {
        formula_consume(ctx);
        return parse_dsl_factor(ctx);
    }

    /* 标识符或关键字 */
    if (formula_is_alpha(c)) {
        size_t start = ctx->pos;

        /* 读取标识符 */
        char *ident = formula_parse_identifier_str(ctx);
        if (!ident)
            return NULL;

        formula_skip_whitespace(ctx);

        /* 几何元素关键字查表分发（替代 7 分支 strcmp 链） */
        for (size_t i = 0; i < sizeof(kDslGeometryKeywords) / sizeof(kDslGeometryKeywords[0]); i++) {
            if (lv_str_eq(ident, kDslGeometryKeywords[i].name)) {
                lv_free((void **) &ident);
                return kDslGeometryKeywords[i].parse(ctx);
            }
        }
        if (is_dsl_keyword(ident)) {
            /* 其他约束关键字 */
            ctx->pos = start; /* 回退 */
            lv_free((void **) &ident);
            return parse_dsl_constraint(ctx);
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

                args[arg_count] = parse_dsl_expression(ctx);
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

            /* 根据函数名查表创建对应节点（替代 7 分支 strcmp 链） */
            FormulaNode *node = formula_apply_math_func(ident, args, arg_count, kDslMathFuncTable,
                                                        sizeof(kDslMathFuncTable) / sizeof(kDslMathFuncTable[0]));
            if (!node)
                node = formula_create_identifier(ident); /* 未知函数，作为标识符返回 */

            lv_free((void **) &ident);
            for (int i = 0; i < arg_count; i++)
                formula_node_destroy(args[i]);
            return node;
        }

        /* 普通标识符 */
        FormulaNode *node = formula_create_identifier(ident);
        lv_free((void **) &ident);
        return node;
    }

    formula_set_error(ctx, "Unexpected character");
    return NULL;
}

/**
 * @brief 解析 DSL 因子（处理幂运算）
 *
 * 在原子表达式基础上处理幂运算（^ 或 **），采用右结合递归解析。
 * 例如：a^b^c 解析为 a^(b^c)。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的因子节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_factor(ParserContext *ctx) {
    FormulaNode *left = parse_dsl_atom(ctx);
    if (!left)
        return NULL;

    formula_skip_whitespace(ctx);

    /* 处理幂运算 */
    if (formula_peek(ctx) == '^' || formula_match_string(ctx, "**")) {
        if (formula_match_string(ctx, "**")) {
            formula_consume(ctx);
            formula_consume(ctx);
        } else {
            formula_consume(ctx);
        }
        formula_skip_whitespace(ctx);
        FormulaNode *right = parse_dsl_factor(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }
        return formula_track_node(ctx, formula_create_binary_op(NODE_BINARY_OP_POW, left, right));
    }

    return left;
}

/**
 * @brief 解析 DSL 项（处理乘除）
 *
 * 在因子基础上处理乘法（*）和除法（/）运算，采用左结合循环解析。
 * 注意：** 被识别为幂运算符，在此层停止解析。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的项节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_term(ParserContext *ctx) {
    FormulaNode *left = parse_dsl_factor(ctx);
    if (!left)
        return NULL;

    while (true) {
        formula_skip_whitespace(ctx);
        char c = formula_peek(ctx);

        NodeType op_type;
        bool should_continue = false;

        if (c == '*') {
            if (formula_peek_next(ctx) == '*')
                break; /* 幂运算 */
            formula_consume(ctx);
            op_type = NODE_BINARY_OP_MUL;
            should_continue = true;
        } else if (c == '/') {
            formula_consume(ctx);
            op_type = NODE_BINARY_OP_DIV;
            should_continue = true;
        }

        if (!should_continue)
            break;

        formula_skip_whitespace(ctx);
        FormulaNode *right = parse_dsl_factor(ctx);
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
 * @brief 解析 DSL 表达式（处理加减）
 *
 * 在项基础上处理加法（+）和减法（-）运算，采用左结合循环解析。
 * 这是 DSL 表达式解析的最高优先级层。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的表达式节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_expression(ParserContext *ctx) {
    FormulaNode *left = parse_dsl_term(ctx);
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
        FormulaNode *right = parse_dsl_term(ctx);
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
 * @brief 解析 DSL 语句
 *
 * 解析单个 DSL 语句，即一个表达式或等式（lhs = rhs）。
 * 等式使用单等号 =（双等号 == 不视为等式）。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的语句节点，失败或到达末尾返回 NULL
 */
static FormulaNode *parse_dsl_statement(ParserContext *ctx) {
    formula_skip_whitespace(ctx);

    if (formula_is_at_end(ctx))
        return NULL;

    FormulaNode *left = parse_dsl_expression(ctx);
    if (!left)
        return NULL;

    formula_skip_whitespace(ctx);

    /* 检查等式 */
    if (formula_peek(ctx) == '=' && formula_peek_next(ctx) != '=') {
        formula_consume(ctx);
        formula_skip_whitespace(ctx);
        FormulaNode *right = parse_dsl_expression(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }
        return formula_track_node(ctx, formula_create_equation(left, right));
    }

    return left;
}

/**
 * @brief 解析 DSL 复合语句
 *
 * 持续解析 DSL 语句直到输入结束，支持分号和换行作为语句分隔符。
 * 如果只有一个语句则直接返回该节点，多个语句则包装为 COMPOUND 节点。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的复合节点或单个语句节点，无语句返回 NULL
 */
FormulaNode *parse_dsl_compound(ParserContext *ctx) {
    FormulaNode *statements[lv_MAX_STATEMENTS] = {NULL};
    int statement_count = 0;
    /* 运行时上限来自 lvConfig.parser.parser_max_statements（默认 64），
       并以编译期数组维度为硬上限，防止配置调大时栈数组越界 */
    const lvConfig *lv_cfg = lv_config_current();
    int statement_cap = lv_cfg->parser.parser_max_statements;
    if (statement_cap > lv_MAX_STATEMENTS)
        statement_cap = lv_MAX_STATEMENTS;

    while (!formula_is_at_end(ctx)) {
        formula_skip_whitespace(ctx);
        if (formula_is_at_end(ctx))
            break;

        if (statement_count >= statement_cap) {
            formula_set_error(ctx, "Too many statements");
            for (int i = 0; i < statement_count; i++)
                formula_node_destroy(statements[i]);
            return NULL;
        }

        FormulaNode *stmt = parse_dsl_statement(ctx);
        if (!stmt) {
            if (ctx->has_error) {
                for (int i = 0; i < statement_count; i++)
                    formula_node_destroy(statements[i]);
                return NULL;
            }
            break;
        }

        statements[statement_count++] = stmt;

        formula_skip_whitespace(ctx);

        /* 语句分隔符 */
        if (formula_peek(ctx) == ';' || formula_peek(ctx) == '\n') {
            formula_consume(ctx);
        }
    }

    if (statement_count == 0) {
        return NULL;
    }

    if (statement_count == 1) {
        return statements[0];
    }

    return formula_create_compound(statements, statement_count);
}

/* ============================================================
 * LaTeX 解析器
 * ============================================================ */

/* parse_latex_expression 已在 formula_parser.h 中声明 */
