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
#include "lv/lv_lifecycle.h"

/* ================================================================
 *  内部辅助宏
 * ================================================================ */

/** @brief 检查两 Token 类型是否匹配并前进 */
#define TOKEN_IS(tok, tp) ((tok).type == (tp))

/* 注：动态数组扩容统一使用 lv/lv_utils.h 中的 lv_ENSURE_ARRAY_CAP，
 * 不再在此重复定义 ENSURE_CAP（原定义已移除）。 */

/* ================================================================
 *  Tokenizer 内部辅助
 * ================================================================ */

/**
 * @brief 向 Token 数组追加一个 Token
 */
static bool token_append(DslToken **tokens, int *count, int *capacity, DSLTokenType type, const char *lexeme, int line,
                         int col) {
    /* 扩容 Token 数组（统一走 lv_ENSURE_ARRAY_CAP） */
    lv_ENSURE_ARRAY_CAP(*tokens, *count, *capacity, false);
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

/* ---- lv_DEFER 作用域守卫：token 数组的 defer 清理（替代 goto fail 样板的销毁调用） ---- */

typedef struct {
    DslToken **tokens; /* 指向 tokens 指针变量的地址（置 NULL 即解除守卫） */
    int *count;        /* 指向当前 token 数量的地址（动态获取最新值） */
} DslTokenGuard;

static void dsl_tokens_guard_cleanup(void *p) {
    DslTokenGuard *g = (DslTokenGuard *) p;
    if (*g->tokens)
        dsl_tokens_destroy(*g->tokens, *g->count);
}

/* ================================================================
 *  单字符运算符/分隔符查找表
 * ================================================================ */

/** 单字符运算符/分隔符映射条目 */
typedef struct {
    DSLTokenType type; /**< 对应的 Token 类型 */
    const char *lex;   /**< 词素字符串 */
} SingleCharToken;

/**
 * @brief 单字符运算符和分隔符查找表
 *
 * 按 ASCII 下标索引；lex 为 NULL 表示未映射（跳过该字符）。
 * 用于替代 dsl_tokenize 中的单字符 switch。
 */
static const SingleCharToken s_single_char_tokens[256] = {
    ['='] = {DSL_TOK_ASSIGN, "="},
    ['('] = {DSL_TOK_LPAREN, "("},
    [')'] = {DSL_TOK_RPAREN, ")"},
    ['{'] = {DSL_TOK_LBRACE, "{"},
    ['}'] = {DSL_TOK_RBRACE, "}"},
    ['['] = {DSL_TOK_LBRACKET, "["},
    [']'] = {DSL_TOK_RBRACKET, "]"},
    [','] = {DSL_TOK_COMMA, ","},
    [';'] = {DSL_TOK_SEMI, ";"},
    [':'] = {DSL_TOK_COLON, ":"},
};

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
    /* 注册 lv_DEFER 守卫：任一 token_append 扩容失败时自动销毁已收集的 token 数组
     * （逐 token 释放动态 lexeme + 数组外壳），替代 goto fail 样板的销毁调用 */
    DslTokenGuard tokens_guard = {&tokens, &count};
    lv_DEFER(dsl_tokens_guard_cleanup, &tokens_guard);
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
                return false;
            continue;
        }

        /* ---- 多字符运算符：-> (箭头) ---- */
        if (c == '-' && pos + 1 < src_len && source[pos + 1] == '>') {
            if (!token_append(&tokens, &count, &capacity, DSL_TOK_ARROW, "->", line, start_col))
                return false;
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
                return false;
            memcpy(lex_buf, source + start_pos, num_len);
            lex_buf[num_len] = '\0';

            if (!token_append(&tokens, &count, &capacity, DSL_TOK_NUMBER, lex_buf, line, start_col_num))
                return false;
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
                    return false;
            }

            if (!token_append(&tokens, &count, &capacity, tok_type, lexeme_str, line, start_col_id))
                return false;
            continue;
        }

        /* ---- 单字符运算符和分隔符 ---- */
        /* 查表分发：未映射的字符直接跳过 */
        const SingleCharToken *single = &s_single_char_tokens[(unsigned char) c];
        if (single->lex == NULL) {
            pos++;
            col++;
            continue;
        }

        if (!token_append(&tokens, &count, &capacity, single->type, single->lex, line, start_col))
            return false;
        pos++;
        col++;
    }

    /* 追加 EOF Token */
    if (!token_append(&tokens, &count, &capacity, DSL_TOK_EOF, "(eof)", line, col))
        return false;

    *out_tokens = tokens;
    *out_count = count;
    tokens = NULL; /* 守卫解除：结果移交调用方 */
    return true;
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