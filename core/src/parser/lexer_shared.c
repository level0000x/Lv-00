/**
 * @file lexer_shared.c
 * @brief 共享词法分析器基础设施实现
 *
 * @details 实现 axiom_pkg 和 module 共用的词法分析器辅助函数。
 *          包括词法分析器初始化、空白和注释跳过、以及字符串字面量提取。
 *
 *          两个模块各自实现自己的 lexer_next_token() 函数，因为：
 *          - axiom_pkg 使用整数数字（int），支持布尔关键字 true/false
 *          - module 使用浮点数（double），标识符允许连字符和点号
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lexer_shared.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"

/* ================================================================
 *  词法分析器初始化
 * ================================================================ */

/**
 * @brief 初始化词法分析器
 *
 * 设置词法分析器的源字符串指针、当前位置、行列号和错误状态。
 * 调用者需确保 source 在分析器使用期间保持有效。
 *
 * @param lex    词法分析器指针，不能为 NULL
 * @param source 源字符串指针，不能为 NULL
 */
void lv00_lexer_init(Lv00Lexer *lex, const char *source) {
    if (!lex || !source)
        return;
    lex->source = source;
    lex->pos = source;
    lex->line = 1;
    lex->col = 1;
    lex->error_msg = NULL;
}

/**
 * @brief 重置/清除词法分析器状态
 *
 * 释放 error_msg 堆分配内存并将所有字段归零，
 * 使词法分析器可安全重用。
 *
 * @param lex 词法分析器指针，不能为 NULL
 */
void lv00_lexer_clear(Lv00Lexer *lex) {
    if (!lex)
        return;
    if (lex->error_msg) {
        lv00_free((void **) &lex->error_msg);
    }
    memset(lex, 0, sizeof(Lv00Lexer));
}

/* ================================================================
 *  跳过空白字符和注释
 * ================================================================ */

/**
 * @brief 跳过空白字符和注释
 *
 * 从当前位置跳过所有空白字符（空格、制表符、换行符等）和
 * 从 '#' 开始到行尾的注释。跳过过程中自动更新行号和列号。
 *
 * @param lex 词法分析器指针，不能为 NULL
 */
void lv00_lexer_skip_whitespace_and_comments(Lv00Lexer *lex) {
    if (!lex || !lex->pos)
        return;
    while (*lex->pos) {
        /* 跳过空白字符（空格、制表符、换行符、回车符等） */
        if (isspace((unsigned char) *lex->pos)) {
            if (*lex->pos == '\n') {
                lex->line++;
                lex->col = 1;
            } else {
                lex->col++;
            }
            lex->pos++;
            continue;
        }

        /* 跳过注释：从 '#' 到行尾 */
        if (*lex->pos == '#') {
            while (*lex->pos && *lex->pos != '\n') {
                lex->pos++;
            }
            continue;
        }

        break;
    }
}

/* ================================================================
 *  提取字符串字面量（含转义处理）
 * ================================================================ */

/**
 * @brief 从词法分析器当前位置提取字符串字面量
 *
 * 从当前引号位置开始，解析字符串内容直到闭合引号。
 * 支持常见转义序列（\n, \t, \r, \", \\）。
 * 成功时推进词法分析器位置到闭合引号之后。
 *
 * @param lex 词法分析器指针，不能为 NULL
 * @return 新分配的解码后字符串（堆分配，调用者负责释放），失败返回 NULL
 */
char *lv00_lexer_extract_string(Lv00Lexer *lex) {
    if (!lex || !lex->pos)
        return NULL;
    const char *start = lex->pos;
    size_t len = 0;

    /* 第一遍：计算解码后的字符串长度（含边界检查，防止过读到 null 终止符之后） */
    while (*lex->pos && *lex->pos != '"') {
        if (*lex->pos == '\\') {
            /* 边界检查：确保转义字符后还有至少一个字符可读，
             * 避免 *(lex->pos + 1) 读取到 null 终止符之后的内存 */
            if (!*(lex->pos + 1)) {
                /* 转义序列不完整（反斜杠后无字符），安全退出 */
                goto extract_fail;
            }
            lex->pos += 2; /* 跳过转义序列的两个字符 */
            len += 1;      /* 转义序列解码为单个字符 */
        } else {
            lex->pos++;
            len++;
        }
    }

    /* 若未找到闭合引号即到达字符串末尾，解析失败 */
    if (*lex->pos != '"') {
        goto extract_fail;
    }

    /* 分配结果缓冲区（使用 lv00_malloc 统一内存管理） */
    char *result = NULL; /* 修复：初始化为 NULL，使 extract_fail 中的释放操作安全 */
    result = (char *) lv00_malloc(len + 1);
    if (!result) {
        /* 修复：内存不足时设置错误信息并返回 NULL */
        goto extract_fail;
    }

    /* 第二遍：解码转义序列到结果缓冲区 */
    const char *src = start;
    char *dst = result;
    const char *dst_end = result + len; /* 缓冲区末尾边界，防止溢出 */
    while (src < lex->pos && dst < dst_end) {
        if (*src == '\\' && src + 1 < lex->pos) {
            src++;
            switch (*src) {
                case 'n':
                    *dst++ = '\n';
                    break;
                case 't':
                    *dst++ = '\t';
                    break;
                case 'r':
                    *dst++ = '\r';
                    break;
                case '"':
                    *dst++ = '"';
                    break;
                case '\\':
                    *dst++ = '\\';
                    break;
                default:
                    *dst++ = *src;
                    break; /* 未识别的转义，保留原字符 */
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    /* 消费闭合引号 */
    if (*lex->pos == '"') {
        lex->pos++;
        lex->col++;
    }

    return result;

extract_fail:
    /* 修复：防御性清理——如果 result 已分配但未返回，则释放它防止内存泄漏。
     * result 已初始化为 NULL，因此 lv00_free 对未分配的情况也是安全的。 */
    lv00_free((void **) &result);

    /* 修复：使用 lv00_strdup_safe 分配错误信息字符串，避免将字符串字面量
     * 直接赋值给 char*（字符串字面量存储在只读数据段，不应通过 char* 修改）。
     * 这与头文件注释"堆分配，调用者负责释放"保持一致。 */
    lex->error_msg = lv00_strdup_safe("字符串字面量解析失败：未找到闭合引号或转义序列不完整");
    return NULL;
}
