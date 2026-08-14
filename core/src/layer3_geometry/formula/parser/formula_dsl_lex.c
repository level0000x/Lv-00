/**
 * @file formula_dsl_lex.c
 * @brief DSL 词法层与语法检测（由 formula_dsl.c 拆分子模块）
 *
 * @details 节点跟踪、数字/标识符解析、DSL 关键字判定与语法检测。
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

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* formula_node_copy 实现在 formula_ast.c 中 */

#include "formula_dsl_internal.h"

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
bool is_dsl_keyword(const char *str) {
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

