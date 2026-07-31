/**
 * @file dsl_lexer.c
 * @brief Lv-00 DSL 词法分析器（从 dsl_compiler.c 拆分）
 *
 * @details 将 DSL 源文本转换为 Token 流。公共 API：dsl_tokenize / dsl_tokens_destroy。
 *
 * @author Lv-00 Project
 */

#include "dsl_compiler.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"

/* ================================================================
 *  内部辅助宏
 * ================================================================ */

/** @brief 检查两 Token 类型是否匹配并前进 */
#define TOKEN_IS(tok, tp) ((tok).type == (tp))

/** @brief 安全扩容宏：通用动态数组扩容 */
#define ENSURE_CAP(arr, count, cap, elem_sz, ret_on_fail)          \
    do {                                                           \
        if ((count) >= (cap)) {                                    \
            size_t _new_cap = (cap) == 0 ? 8 : (size_t) (cap) * 2; \
            void *_np = lv_realloc((arr), _new_cap * (elem_sz));   \
            if (!_np)                                              \
                return (ret_on_fail);                              \
            (arr) = _np;                                           \
            (cap) = (int) _new_cap;                                \
        }                                                          \
    } while (0)

/* ================================================================
 *  Tokenizer 内部辅助
 * ================================================================ */

/**
 * @brief 向 Token 数组追加一个 Token
 */
static bool token_append(DslToken **tokens, int *count, int *capacity, DSLTokenType type, const char *lexeme, int line,
                         int col) {
    ENSURE_CAP(*tokens, *count, *capacity, sizeof(DslToken), false);
    DslToken *t = &(*tokens)[*count];
    t->type = type;
    t->lexeme = lexeme;
    t->line = line;
    t->col = col;
    (*count)++;
    return true;
}

/**
 * @brief 创建字符串副本存储在内部，供 token lexeme 使用
 *
 * 用静态表降低内存开销：对于简单运算符使用预先定义的字符串字面量，
 * 关键字和标识符使用 lv_strdup 动态分配（由 tokens_destroy 统一释放）。
 */
static const char *token_lexeme_dup(const char *s, size_t len) {
    char *dup = lv_malloc(len + 1);
    if (!dup)
        return "(out of memory)";
    memcpy(dup, s, len);
    dup[len] = '\0';
    return dup;
}

/* ================================================================
 *  Tokenizer
 * ================================================================ */

/**
 * @brief 对 DSL 源代码进行词法分析
 *
 * 将源字符串转换为 DslToken 数组。支持所有 DSLTokenType 枚举类型的识别。
 * 自动扩展 Token 数组容量。输出 tokens 由调用者通过 dsl_tokens_destroy 释放。
 *
 * @param source    源字符串
 * @param out_tokens 输出：Token 数组指针
 * @param out_count  输出：Token 数量
 * @return 成功返回 true，失败返回 false
 */
bool dsl_tokenize(const char *source, DslToken **out_tokens, int *out_count) {
    if (!source || !out_tokens || !out_count)
        return false;

    *out_tokens = NULL;
    *out_count = 0;

    size_t src_len = strlen(source);
    if (src_len == 0)
        return true;

    int capacity = 64;
    DslToken *tokens = lv_calloc((size_t) capacity, sizeof(DslToken));
    if (!tokens)
        return false;

    int count = 0;
    size_t pos = 0;
    int line = 1;
    int col = 1;

    /* 预定义关键字表，支持二分查找 */
    typedef struct {
        const char *word;
        int len;
        DSLTokenType type;
    } KwEntry;

    /* 按字母序排列（memcmp 顺序）以支持 O(log N) 查找 */
    static const KwEntry keywords[] = {
        {"bisector", 8, DSL_TOK_BISECTOR},
        {"centroid", 8, DSL_TOK_CENTROID},
        {"circle", 6, DSL_TOK_CIRCLE},
        {"circumcenter", 11, DSL_TOK_CIRCUMCENTER},
        {"constraint", 10, DSL_TOK_CONSTRAINT},
        {"fix", 3, DSL_TOK_FIX},
        {"free", 4, DSL_TOK_FREE},
        {"incenter", 8, DSL_TOK_INCENTER},
        {"intersect", 9, DSL_TOK_INTERSECT},
        {"let", 3, DSL_TOK_LET},
        {"line", 4, DSL_TOK_LINE},
        {"load", 4, DSL_TOK_LOAD},
        {"midpoint", 8, DSL_TOK_MIDPOINT},
        {"orthocenter", 11, DSL_TOK_ORTHOCENTER},
        {"parallel", 8, DSL_TOK_PARALLEL},
        {"perpendicular", 13, DSL_TOK_PERPENDICULAR},
        {"point", 5, DSL_TOK_POINT},
        {"polygon", 7, DSL_TOK_POLYGON},
        {"prove", 5, DSL_TOK_PROVE},
        {"ray", 3, DSL_TOK_RAY},
        {"segment", 7, DSL_TOK_SEGMENT},
        {"triangle", 8, DSL_TOK_TRIANGLE},
    };
#define KW_COUNT (sizeof(keywords) / sizeof(keywords[0]))

    while (pos < src_len) {
        char c = source[pos];

        /* 列号记录（简化处理，baseline 为本 token 起始列） */
        int start_col = col;

        /* ---- 跳过空白 ---- */
        if (c == ' ' || c == '\t' || c == '\r') {
            pos++;
            col++;
            continue;
        }
        if (c == '\n') {
            line++;
            col = 1;
            pos++;
            continue;
        }

        /* ---- 跳过注释 (# 和 //) ---- */
        if (c == '#' || (c == '/' && pos + 1 < src_len && source[pos + 1] == '/')) {
            while (pos < src_len && source[pos] != '\n') {
                pos++;
                col++;
            }
            /* 添加注释 Token（便于 AST 溯源） */
            if (!token_append(&tokens, &count, &capacity, DSL_TOK_COMMENT, "#", line, start_col))
                goto fail;
            continue;
        }

        /* ---- 多字符运算符：-> (箭头) ---- */
        if (c == '-' && pos + 1 < src_len && source[pos + 1] == '>') {
            if (!token_append(&tokens, &count, &capacity, DSL_TOK_ARROW, "->", line, start_col))
                goto fail;
            pos += 2;
            col += 2;
            continue;
        }

        /* ---- 数值字面量（整数和浮点数）---- */
        if (c == '.' || isdigit((unsigned char) c)) {
            size_t start_pos = pos;
            int start_col_num = start_col;

            /* 整数部分 */
            while (pos < src_len && isdigit((unsigned char) source[pos])) {
                pos++;
                col++;
            }
            /* 小数部分 */
            bool is_float = false;
            if (pos < src_len && source[pos] == '.') {
                /* 确保不是类似 ".." 的情况 */
                if (pos + 1 < src_len && isdigit((unsigned char) source[pos + 1])) {
                    is_float = true;
                    pos++;
                    col++;
                    while (pos < src_len && isdigit((unsigned char) source[pos])) {
                        pos++;
                        col++;
                    }
                }
            }
            /* 科学计数法 */
            if (pos < src_len && (source[pos] == 'e' || source[pos] == 'E')) {
                is_float = true;
                pos++;
                col++;
                if (pos < src_len && (source[pos] == '+' || source[pos] == '-')) {
                    pos++;
                    col++;
                }
                while (pos < src_len && isdigit((unsigned char) source[pos])) {
                    pos++;
                    col++;
                }
            }

            size_t num_len = pos - start_pos;
            /* 创建一个长度为 1 的静态字符串，实际用 lexeme 较长但安全 */
            char *lex_buf = lv_malloc(num_len + 1);
            if (!lex_buf)
                goto fail;
            memcpy(lex_buf, source + start_pos, num_len);
            lex_buf[num_len] = '\0';

            if (!token_append(&tokens, &count, &capacity, DSL_TOK_NUMBER, lex_buf, line, start_col_num))
                goto fail;
            continue;
        }

        /* ---- 标识符或关键字 ---- */
        if (isalpha((unsigned char) c) || c == '_') {
            size_t start_pos = pos;
            int start_col_id = start_col;
            while (pos < src_len && (isalnum((unsigned char) source[pos]) || source[pos] == '_')) {
                pos++;
                col++;
            }
            size_t len = pos - start_pos;

            /* 关键字查找（二分） */
            DSLTokenType tok_type = DSL_TOK_IDENT;
            const char *lex = source + start_pos;

            /* 线性扫描（关键字列表较小，线性查找足够） */
            for (size_t i = 0; i < KW_COUNT; i++) {
                if (len == (size_t) keywords[i].len && memcmp(lex, keywords[i].word, len) == 0) {
                    tok_type = keywords[i].type;
                    break;
                }
            }

            /* 重复使用已有的字符串常量避免额外分配 */
            const char *lexeme_str = NULL;
            if (tok_type != DSL_TOK_IDENT) {
                /* 关键字：从预知表中取字面量 */
                for (size_t i = 0; i < KW_COUNT; i++) {
                    if (keywords[i].type == tok_type) {
                        lexeme_str = keywords[i].word;
                        break;
                    }
                }
            }
            if (!lexeme_str) {
                /* 标识符：动态分配 */
                lexeme_str = token_lexeme_dup(lex, len);
                if (!lexeme_str)
                    goto fail;
            }

            if (!token_append(&tokens, &count, &capacity, tok_type, lexeme_str, line, start_col_id))
                goto fail;
            continue;
        }

        /* ---- 单字符运算符和分隔符 ---- */
        DSLTokenType single_type = DSL_TOK_ERROR;
        const char *single_lex = NULL;
        int advance = 1;

        switch (c) {
            case '=':
                single_type = DSL_TOK_ASSIGN;
                single_lex = "=";
                break;
            case '(':
                single_type = DSL_TOK_LPAREN;
                single_lex = "(";
                break;
            case ')':
                single_type = DSL_TOK_RPAREN;
                single_lex = ")";
                break;
            case '{':
                single_type = DSL_TOK_LBRACE;
                single_lex = "{";
                break;
            case '}':
                single_type = DSL_TOK_RBRACE;
                single_lex = "}";
                break;
            case '[':
                single_type = DSL_TOK_LBRACKET;
                single_lex = "[";
                break;
            case ']':
                single_type = DSL_TOK_RBRACKET;
                single_lex = "]";
                break;
            case ',':
                single_type = DSL_TOK_COMMA;
                single_lex = ",";
                break;
            case ';':
                single_type = DSL_TOK_SEMI;
                single_lex = ";";
                break;
            case ':':
                single_type = DSL_TOK_COLON;
                single_lex = ":";
                break;
            default: /* 无法识别的字符：跳过 */
                pos++;
                col++;
                continue;
        }

        if (!token_append(&tokens, &count, &capacity, single_type, single_lex, line, start_col))
            goto fail;
        pos += advance;
        col += advance;
    }

    /* 追加 EOF Token */
    if (!token_append(&tokens, &count, &capacity, DSL_TOK_EOF, "(eof)", line, col))
        goto fail;

    *out_tokens = tokens;
    *out_count = count;
    return true;

fail:
    dsl_tokens_destroy(tokens, count);
    return false;
}

/**
 * @brief 销毁 Token 数组
 *
 * 释放通过 dsl_tokenize 分配的 Token 数组及其动态分配的 lexeme 字符串。
 *
 * @param tokens Token 数组指针（允许为 NULL）
 * @param count  Token 数量
 */
void dsl_tokens_destroy(DslToken *tokens, int count) {
    if (!tokens)
        return;
    /* 释放动态分配的 lexeme（关键字使用静态字符串，通过检查指针范围来区分） */
    for (int i = 0; i < count; i++) {
        /* 如果 lexeme 指向 tokens 内部或静态字符串，则不释放 */
        if (tokens[i].lexeme && tokens[i].type == DSL_TOK_IDENT) {
            lv_free((void **) &tokens[i].lexeme);
        }
        if (tokens[i].lexeme && tokens[i].type == DSL_TOK_NUMBER) {
            lv_free((void **) &tokens[i].lexeme);
        }
    }
    lv_free((void **) &tokens);
}