/**
 * @file formula_parser.c
 * @brief 公式解析器实现
 *
 * @details 支持 LaTeX、Python 和 DSL 三种语法的公式解析。
 *          生成抽象语法树（AST），支持错误恢复和位置追踪。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - formula_parser.h : 解析器公共接口定义
 *   - lv_internal.h  : 内部数据结构和常量
 *   - lv_utils.h     : 统一内存分配器和工具函数
 *   - lv.h           : 核心类型定义
 */

/* ============================================================
 * 魔法数字常量定义
 * ============================================================ */

#define lv_MAX_COORDINATES 16      /**< 坐标列表最大元素数量 */
#define lv_MAX_VERTICES 32         /**< 顶点列表最大元素数量 */
#define lv_MAX_POLYGON_VERTICES 32 /**< 多边形顶点最大数量 */
#define lv_MAX_STATEMENTS 64       /**< 复合语句最大子语句数量 */
#define lv_MAX_ARGUMENTS 16        /**< 函数参数列表最大元素数量 */
#define lv_MAX_PARTICIPANTS 16     /**< 约束参与者最大数量 */
#define lv_MAX_BUFFER_SIZE 256     /**< 错误消息缓冲区大小 */
#define lv_MAX_TEMP_MSG_SIZE 128   /**< 临时错误消息/诊断缓冲区大小 */

#include "formula_parser.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "parser_safety.h"
#include "stream.h"
#include "stream_context_util.h"
#include "lv/lv_strbuf.h"

/* ============================================================
 * 解析器上下文结构
 * ============================================================ */

lv_DECLARE_STREAM_CTX(formula_parser);

/**
 * @brief 获取解析器最近一次错误信息
 *
 * @return 错误信息字符串指针（内部缓冲区，无需释放），无错误时返回 NULL
 */
const char *formula_parser_get_last_error(void) {
    return lv_get_last_error_message();
}

/* ============================================================
 * DSL 关键字表
 * ============================================================ */

const char *formula_dsl_keywords[] = {"point",         "segment",  "circle",    "triangle", "line",      "region",
                                      "perpendicular", "parallel", "midpoint",  "angle",    "distance",  "area",
                                      "perimeter",     "tangent",  "intersect", "equal",    "collinear", "bisector",
                                      "congruent",     "polygon",  "vector",    NULL};

/* ============================================================
 * LaTeX 命令表
 * ============================================================ */

const char *formula_latex_commands[] = {
    "\\frac",   "\\sqrt",   "\\sin",    "\\cos",     "\\tan",    "\\cot",    "\\pi",    "\\theta", "\\alpha",
    "\\beta",   "\\gamma",  "\\delta",  "\\epsilon", "\\lambda", "\\mu",     "\\sigma", "\\omega", "\\leq",
    "\\geq",    "\\neq",    "\\approx", "\\equiv",   "\\cdot",   "\\times",  "\\div",   "\\left",  "\\right",
    "\\langle", "\\rangle", "\\begin",  "\\end",     "\\text",   "\\mathrm", "\\ln",    "\\log",   NULL};

/* ============================================================
 * Python 特征表
 * ============================================================ */

const char *formula_python_features[] = {"**",    "==",   "!=",     "<=",      ">=",      "and ", "or ",  "not ",
                                         "sqrt(", "sin(", "cos(",   "tan(",    "abs(",    "pow(", "True", "False",
                                         "None",  "pi",   "e)",     "import ", "from ",   "def ", "if ",  "else ",
                                         "elif ", "for ", "while ", "return ", "lambda ", NULL};


/* ============================================================
 * 辅助函数实现
 * ============================================================ */

/**
 * @brief 跳过空白字符和注释
 *
 * 在输入流中跳过所有空白字符（空格、制表符、回车符、换行符）
 * 以及以 # 开头的注释行。处理完成后指针定位到第一个非空白字符处。
 *
 * @param ctx 解析器上下文指针
 */
void formula_skip_whitespace(ParserContext *ctx) {
    while (ctx->pos < ctx->length) {
        char c = ctx->input[ctx->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            ctx->pos++;
            ctx->column++;
        } else if (c == '\n') {
            ctx->pos++;
            ctx->line++;
            ctx->column = 1;
        } else if (c == '#') {
            /* 跳过注释直到行尾 */
            while (ctx->pos < ctx->length && ctx->input[ctx->pos] != '\n') {
                ctx->pos++;
            }
        } else {
            break;
        }
    }
}

/**
 * @brief 查看当前字符（不消费）
 *
 * 返回当前位置的字符，但不移动解析位置。
 *
 * @param ctx 解析器上下文指针
 * @return char 当前字符，如果已到达末尾则返回 '\0'
 */
char formula_peek(ParserContext *ctx) {
    if (ctx->pos >= ctx->length) {
        return '\0';
    }
    return ctx->input[ctx->pos];
}

/**
 * @brief 查看下一个字符（不消费）
 *
 * 返回下一个位置的字符，但不移动解析位置。
 * 用于向前查看一个字符以辅助决策（如判断是否为小数点后的数字）。
 *
 * @param ctx 解析器上下文指针
 * @return char 下一个字符，如果接近末尾则返回 '\0'
 */
char formula_peek_next(ParserContext *ctx) {
    if (ctx->pos + 1 >= ctx->length) {
        return '\0';
    }
    return ctx->input[ctx->pos + 1];
}

/**
 * @brief 消费当前字符并前进到下一个位置
 *
 * 返回当前位置的字符并将解析位置向前移动一个字符。
 * 同时更新行号和列号追踪，遇到换行符时重置列号并增加行号。
 *
 * @param ctx 解析器上下文指针
 * @return char 被消费的字符，如果已到达末尾则返回 '\0'
 */
char formula_consume(ParserContext *ctx) {
    if (ctx->pos >= ctx->length) {
        return '\0';
    }
    char c = ctx->input[ctx->pos];
    ctx->pos++;
    if (c == '\n') {
        ctx->line++;
        ctx->column = 1;
    } else {
        ctx->column++;
    }
    return c;
}

/**
 * @brief 检查当前位置是否匹配指定字符串（不消费）
 *
 * 在不移动解析位置的情况下，检查从当前位置开始是否匹配给定字符串。
 * 用于向前查看以决定下一步解析动作。
 *
 * @param ctx 解析器上下文指针
 * @param str 要匹配的字符串
 * @return true 当前位置匹配给定字符串
 * @return false 当前位置不匹配或输入长度不足
 */
bool formula_match_string(ParserContext *ctx, const char *str) {
    size_t len = strlen(str);
    if (ctx->pos + len > ctx->length) {
        return false;
    }
    return strncmp(ctx->input + ctx->pos, str, len) == 0;
}

/**
 * @brief 匹配并消费字符串
 *
 * 检查当前位置是否匹配给定字符串，如果匹配则消费整个字符串
 * 并将解析位置向前移动。
 *
 * @param ctx 解析器上下文指针
 * @param str 要匹配并消费的字符串
 * @return true 成功匹配并消费字符串
 * @return false 匹配失败，解析位置不变
 */
bool formula_match_and_consume(ParserContext *ctx, const char *str) {
    if (!formula_match_string(ctx, str)) {
        return false;
    }
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        consume(ctx);
    }
    return true;
}

/**
 * @brief 期望并消费指定字符
 *
 * 检查当前字符是否匹配预期字符，如果匹配则消费该字符；
 * 如果不匹配则设置错误信息并标记解析器为错误状态。
 *
 * @param ctx 解析器上下文指针
 * @param c 期望的字符
 * @return true 成功匹配并消费字符
 * @return false 字符不匹配或已到达末尾，错误状态已设置
 */
bool formula_expect_char(ParserContext *ctx, char c) {
    if (formula_peek(ctx) != c) {
        lvStrBuf sb = {0};
        lv_strbuf_printf(&sb, "Expected '%c' but got '%s'", c, peek(ctx) ? "unexpected char" : "EOF");
        /* 使用 lv_strlcpy 替代不安全的 strncpy */
        lv_strlcpy(ctx->error_message, sb.data, sizeof(ctx->error_message));
        ctx->has_error = true;
        lv_strbuf_destroy(&sb);
        return false;
    }
    consume(ctx);
    return true;
}

/**
 * @brief 设置解析错误信息
 *
 * 在解析上下文中设置错误消息和错误状态，同时将错误信息
 * 同步到全局错误缓冲区。只有在尚未出错时才会设置新的错误，
 * 以保留第一个错误的信息。
 *
 * @param ctx 解析器上下文指针
 * @param msg 错误消息字符串
 */
void formula_set_error(ParserContext *ctx, const char *msg) {
    if (!ctx->has_error) {
        snprintf(ctx->error_message, sizeof(ctx->error_message), "Error at line %d, column %d: %s", ctx->line,
                 ctx->column, msg);
        ctx->has_error = true;
        lv_set_error(lv_ERROR_PARSE, "%s", ctx->error_message);
    }
}

/**
 * @brief 检查是否到达输入末尾
 *
 * @param ctx 解析器上下文指针
 * @return true 已到达输入末尾
 * @return false 尚未到达输入末尾
 */
bool formula_is_at_end(ParserContext *ctx) {
    return ctx->pos >= ctx->length;
}

/**
 * @brief 检查字符是否为字母
 *
 * 判断字符是否为英文字母（a-z, A-Z）或下划线（_）。
 *
 * @param c 要检查的字符
 * @return true 字符是字母或下划线
 * @return false 字符不是字母或下划线
 */
bool formula_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/**
 * @brief 检查字符是否为字母或数字
 *
 * 判断字符是否为英文字母（a-z, A-Z）、数字（0-9）或下划线（_）。
 * 实质上等价于 is_alpha() 与数字检查的组合。
 *
 * @param c 要检查的字符
 * @return true 字符是字母、数字或下划线
 * @return false 字符不是上述字符
 */
bool formula_is_alnum(char c) {
    return formula_is_alpha(c) || (c >= '0' && c <= '9');
}

/**
 * @brief 检查字符是否为数字
 *
 * 判断字符是否为十进制数字（0-9）。
 *
 * @param c 要检查的字符
 * @return true 字符是数字
 * @return false 字符不是数字
 */
bool formula_is_digit(char c) {
    return c >= '0' && c <= '9';
}

/* ============================================================
 * AST 节点管理
 * ============================================================ */

/**
 * @brief 创建数值 AST 节点
 *
 * @param numerator   分子
 * @param denominator 分母（不能为 0）
 * @return 新分配的 AST 节点指针，失败返回 NULL
 */
FormulaNode *formula_parse(const char *input, const char *syntax) {
    if (!input) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "Input is NULL");
    }

    lv_clear_error();

    /* ──── 安全加固：输入验证 ──── */
    size_t input_len = strlen(input);
    lvErrorCode validate_err = lv_input_validate(input, input_len);
    if (validate_err != lv_OK) {
        /* lv_input_validate 已通过 lv_set_error 设置详细错误信息 */
        lv_RETURN_ERROR_NULL(validate_err, "input validation failed");
    }

    if (formula_parser_stream_ctx) {
        stream_emit_info(formula_parser_stream_ctx, "公式解析开始", 0);
    }

    /* 初始化解析上下文（node_count/current_depth 用于安全限制追踪） */
    ParserContext ctx = {0};
    ctx.input = input;
    ctx.pos = 0;
    ctx.length = input_len;
    ctx.line = 1;
    ctx.column = 1;
    ctx.has_error = false;
    ctx.node_count = 0;
    ctx.current_depth = 0;

    FormulaNode *ast = NULL;

    /* 根据语法类型选择解析器 */
    if (syntax == NULL || strcmp(syntax, "auto") == 0) {
        syntax = formula_detect_syntax(input);
    }

    if (strcmp(syntax, "dsl") == 0) {
        ast = parse_dsl_compound(&ctx);
    } else if (strcmp(syntax, "latex") == 0) {
        ast = parse_latex_expression(&ctx);
    } else if (strcmp(syntax, "python") == 0) {
        ast = parse_python_expression(&ctx);
    } else {
        /* 默认尝试 DSL */
        ast = parse_dsl_compound(&ctx);
    }

    if (ctx.has_error) {
        /* 如果已通过安全函数设置了错误，保留原有错误码；
           否则使用通用解析错误码 */
        if (lv_get_last_error_code() == lv_OK) {
            lv_set_error(lv_ERROR_PARSE, "%s", ctx.error_message);
        }
        if (formula_parser_stream_ctx) {
            stream_emit_error(formula_parser_stream_ctx, "公式解析错误", 0);
        }
    } else {
        if (formula_parser_stream_ctx) {
            stream_emit_progress(formula_parser_stream_ctx, 1.0, "公式解析完成", 1, 1);
        }
    }

    return ast;
}
