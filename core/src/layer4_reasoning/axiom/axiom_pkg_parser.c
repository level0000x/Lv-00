/*
 * @file axiom_pkg_parser.c
 * @brief Axiom package system - parser and loading
 * @details Split from axiom_pkg.c
 */

#include "lv/axiom_pkg.h"
#include "axiom_pkg_internal.h"

#include "lv/lv_file.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/sha256.h"

#include "lv/debug.h"
#include "lv/error_codes.h"
#include "lv/lexer_shared.h"
#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"

/* ============== 解析器 ============== */

typedef enum {
    PKG_EOF,
    PKG_LBRACE,     /* { */
    PKG_RBRACE,     /* } */
    PKG_STRING,     /* "..." */
    PKG_NUMBER,     /* 整数 */
    PKG_IDENTIFIER, /* 标识符 */
    PKG_BOOLEAN,    /* true/false */
    PKG_ERROR
} PkgTokenType;

typedef struct {
    PkgTokenType type;
    char *str_value;
    int int_value;
    bool bool_value;
    int line;
    int col;
} Token;

/* Lexer 结构体：使用共享的词法分析器基础设施 */
typedef lvLexer Lexer;

static void lexer_init(Lexer *lex, const char *source) {
    lv_lexer_init(lex, source);
}

static void lexer_skip_whitespace_and_comments(Lexer *lex) {
    lv_lexer_skip_whitespace_and_comments(lex);
}

static Token lexer_next_token(Lexer *lex) {
    Token tok = {0};
    tok.line = lex->line;
    tok.col = lex->col;

    lexer_skip_whitespace_and_comments(lex);

    if (!*lex->pos) {
        tok.type = PKG_EOF;
        return tok;
    }

    /* 大括号 */
    if (*lex->pos == '{') {
        tok.type = PKG_LBRACE;
        lex->pos++;
        lex->col++;
        return tok;
    }

    if (*lex->pos == '}') {
        tok.type = PKG_RBRACE;
        lex->pos++;
        lex->col++;
        return tok;
    }

    /* 字符串字面量 */
    if (*lex->pos == '"') {
        lex->pos++; /* 跳过开引号 */
        lex->col++;

        tok.str_value = lv_lexer_extract_string(lex);
        if (!tok.str_value) {
            tok.type = PKG_ERROR;
            return tok;
        }

        tok.type = PKG_STRING;
        return tok;
    }

    /* 数字 */
    if (isdigit((unsigned char) *lex->pos) || (*lex->pos == '-' && isdigit((unsigned char) *(lex->pos + 1)))) {
        const char *start = lex->pos;
        int sign = 1;

        if (*lex->pos == '-') {
            sign = -1;
            lex->pos++;
            lex->col++;
        }

        int value = 0;
        bool overflow = false;
        while (*lex->pos && isdigit((unsigned char) *lex->pos)) {
            int digit = *lex->pos - '0';
            /* 检查整数溢出：value * 10 + digit 是否超出 INT_MAX 范围 */
            if (value > (INT_MAX - digit) / 10) {
                overflow = true;
                break;
            }
            value = value * 10 + digit;
            lex->pos++;
            lex->col++;
        }

        if (overflow) {
            /* 溢出时设置错误标记，使用 INT_MAX 作为安全回退值 */
            tok.type = PKG_NUMBER;
            tok.int_value = sign == 1 ? INT_MAX : INT_MIN;
            lex->error_msg = "数字字面量超出整数范围";
            return tok;
        }

        tok.type = PKG_NUMBER;
        tok.int_value = sign * value;
        return tok;
    }

    /* 标识符或关键字 */
    if (isalpha((unsigned char) *lex->pos) || *lex->pos == '_') {
        const char *start = lex->pos;

        while (*lex->pos && (isalnum((unsigned char) *lex->pos) || *lex->pos == '_')) {
            lex->pos++;
            lex->col++;
        }

        size_t len = lex->pos - start;
        tok.str_value = lv_malloc(len + 1);
        if (!tok.str_value) {
            tok.type = PKG_ERROR;
            return tok;
        }

        lv_strlcpy_n(tok.str_value, len + 1, start, (size_t) len);

        /* 检查关键字 */
        if (lv_str_eq(tok.str_value, "true")) {
            tok.type = PKG_BOOLEAN;
            tok.bool_value = true;
            lv_free((void **) &tok.str_value);
            tok.str_value = NULL;
        } else if (lv_str_eq(tok.str_value, "false")) {
            tok.type = PKG_BOOLEAN;
            tok.bool_value = false;
            lv_free((void **) &tok.str_value);
            tok.str_value = NULL;
        } else {
            tok.type = PKG_IDENTIFIER;
        }

        return tok;
    }

    /* 未知字符 */
    tok.type = PKG_ERROR;
    lex->error_msg = "意外的字符";
    lex->pos++;
    lex->col++;

    return tok;
}

static void token_free(Token *tok) {
    if (tok->str_value) {
        lv_free((void **) &tok->str_value);
        tok->str_value = NULL;
    }
}

/* 解析器上下文 */
typedef struct {
    Lexer lexer;
    Token current;
    bool has_error;
} Parser;

static void parser_init(Parser *p, const char *source) {
    lexer_init(&p->lexer, source);
    p->has_error = false;
    memset(&p->current, 0, sizeof(Token));
}

static void parser_advance(Parser *p) {
    token_free(&p->current);
    p->current = lexer_next_token(&p->lexer);
}

static bool parser_expect(Parser *p, PkgTokenType type) {
    if (p->current.type != type) {
        lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望 %d, 得到 %d", p->current.line, p->current.col, type,
                     p->current.type);
        p->has_error = true;
        return false;
    }
    return true;
}

static bool parser_expect_identifier(Parser *p, const char *name) {
    if (p->current.type != PKG_IDENTIFIER || lv_str_ne(p->current.str_value, name)) {
        lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望关键字 '%s'", p->current.line, p->current.col, name);
        p->has_error = true;
        return false;
    }
    return true;
}

/* 前向声明 */
static bool parse_package_body(Parser *p, AxiomPackage *pkg);

/**
 * @brief 清理 KnownUnconstructible 结构体的动态资源
 */
static void unconstructible_desc_cleanup(KnownUnconstructible *uc) {
    if (!uc)
        return;
    lv_free((void **) &uc->name);
    lv_free((void **) &uc->reduces_to);
    lv_free((void **) &uc->external_ref);
    for (int i = 0; i < uc->dependency_chain.count; i++) {
        lv_free((void **) lv_darray_get(&uc->dependency_chain, i));
    }
    lv_darray_free(&uc->dependency_chain);
    uc->name = NULL;
    uc->reduces_to = NULL;
    uc->external_ref = NULL;
}

/* 解析不可构造问题 */
static bool parse_unconstructible(Parser *p, AxiomPackage *pkg) {
    parser_advance(p); /* 跳过 'unconstructible' */

    /* 期望字符串 (问题名称) */
    if (!parser_expect(p, PKG_STRING))
        return false;

    KnownUnconstructible uc = {0};
    lv_darray_init(&uc.dependency_chain, sizeof(char *));
    uc.name = lv_strdup_safe(p->current.str_value);
    uc.green_verified = false;

    parser_advance(p);

    /* 期望左大括号 */
    if (!parser_expect(p, PKG_LBRACE)) {
        lv_free((void **) &uc.name);
        return false;
    }
    parser_advance(p);

    /* 解析内容直到右大括号 */
    while (p->current.type != PKG_RBRACE && p->current.type != PKG_EOF && !p->has_error) {
        if (p->current.type != PKG_IDENTIFIER) {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望属性名", p->current.line);
            p->has_error = true;
            break;
        }

        const char *prop = lv_strdup_safe(p->current.str_value);
        parser_advance(p);

        if (lv_str_eq(prop, "reduces_to")) {
            if (!parser_expect(p, PKG_STRING)) {
                lv_free((void **) &prop);
                p->has_error = true;
                break;
            }
            uc.reduces_to = lv_strdup_safe(p->current.str_value);
            parser_advance(p);
        } else if (lv_str_eq(prop, "dependency")) {
            if (!parser_expect(p, PKG_STRING)) {
                lv_free((void **) &prop);
                p->has_error = true;
                break;
            }

            /* 添加到依赖链 */
            char *dep = lv_strdup_safe(p->current.str_value);
            lv_darray_push(&uc.dependency_chain, &dep);
            parser_advance(p);
        } else if (lv_str_eq(prop, "external_ref")) {
            if (!parser_expect(p, PKG_STRING)) {
                lv_free((void **) &prop);
                p->has_error = true;
                break;
            }
            uc.external_ref = lv_strdup_safe(p->current.str_value);
            parser_advance(p);
        } else if (lv_str_eq(prop, "green_verified")) {
            if (!parser_expect(p, PKG_BOOLEAN)) {
                lv_free((void **) &prop);
                p->has_error = true;
                break;
            }
            uc.green_verified = p->current.bool_value;
            parser_advance(p);
        } else {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 未知属性 '%s'", p->current.line, prop);
            lv_free((void **) &prop);
            p->has_error = true;
            break;
        }
        lv_free((void **) &prop);
    }

    if (p->has_error) {
        unconstructible_desc_cleanup(&uc);
        return false;
    }

    /* 期望右大括号 */
    if (!parser_expect(p, PKG_RBRACE)) {
        unconstructible_desc_cleanup(&uc);
        return false;
    }

    /* 添加到包 */
    if (!axiom_package_add_known_unconstructible(pkg, &uc)) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "内存分配失败");
        unconstructible_desc_cleanup(&uc);
        return false;
    }

    parser_advance(p);
    return true;
}

/* 解析模板声明 */
static bool parse_template(Parser *p, AxiomPackage *pkg) {
    parser_advance(p); /* 跳过 'template' */

    /* 期望字符串 (模板名称) */
    if (!parser_expect(p, PKG_STRING))
        return false;

    ConstraintTemplate tmpl = {0};
    tmpl.name = lv_strdup_safe(p->current.str_value);

    parser_advance(p);

    /* 期望参数数量 (数字) */
    if (!parser_expect(p, PKG_NUMBER)) {
        lv_free((void **) &tmpl.name);
        return false;
    }
    tmpl.param_count = p->current.int_value;

    parser_advance(p);

    /* 可选 verified 字段（新格式为第三个数字；旧格式文件无此字段，缺省 false） */
    if (p->current.type == PKG_NUMBER) {
        tmpl.verified = p->current.int_value != 0;
        parser_advance(p);
    } else {
        tmpl.verified = false;
    }

    /* 添加到包 */
    if (!axiom_package_register_template(pkg, &tmpl)) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "内存分配失败");
        lv_free((void **) &tmpl.name);
        return false;
    }

    return true;
}

/* 解析包体 */
static bool parse_package_body(Parser *p, AxiomPackage *pkg) {
    while (p->current.type != PKG_RBRACE && p->current.type != PKG_EOF && !p->has_error) {
        if (p->current.type != PKG_IDENTIFIER) {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望声明", p->current.line);
            p->has_error = true;
            break;
        }

        const char *keyword = p->current.str_value;

        if (lv_str_eq(keyword, "template")) {
            if (!parse_template(p, pkg)) {
                p->has_error = true;
                break;
            }
        } else if (lv_str_eq(keyword, "unconstructible")) {
            if (!parse_unconstructible(p, pkg)) {
                p->has_error = true;
                break;
            }
        } else if (lv_str_eq(keyword, "bottom_geometry")) {
            parser_advance(p);
            if (!parser_expect(p, PKG_STRING)) {
                p->has_error = true;
                break;
            }
            lv_free((void **) &pkg->bottom_geometry);
            pkg->bottom_geometry = lv_strdup_safe(p->current.str_value);
            parser_advance(p);
        } else if (lv_str_eq(keyword, "negation_encoding")) {
            parser_advance(p);
            if (!parser_expect(p, PKG_STRING)) {
                p->has_error = true;
                break;
            }
            lv_free((void **) &pkg->negation_encoding);
            pkg->negation_encoding = lv_strdup_safe(p->current.str_value);
            parser_advance(p);
        } else if (lv_str_eq(keyword, "contradiction_behavior")) {
            parser_advance(p);
            if (!parser_expect(p, PKG_STRING)) {
                p->has_error = true;
                break;
            }

            const char *behavior = p->current.str_value;
            if (lv_str_eq(behavior, "explosion_principle")) {
                pkg->contradiction_behavior = EXPLOSION_PRINCIPLE;
            } else if (lv_str_eq(behavior, "constructive")) {
                pkg->contradiction_behavior = CONSTRUCTIVE;
            } else if (lv_str_eq(behavior, "non_constructive_oracle")) {
                pkg->contradiction_behavior = NON_CONSTRUCTIVE_ORACLE;
            } else {
                lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 未知的矛盾行为 '%s'", p->current.line, behavior);
                p->has_error = true;
                break;
            }
            parser_advance(p);
        } else {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 未知的关键字 '%s'", p->current.line, keyword);
            p->has_error = true;
            break;
        }
    }

    return !p->has_error;
}

/**
 * @brief 发出公理包加载提示
 *
 * 检查公理包是否包含非经典逻辑特征，向用户发出加载提示。
 * 对应设计文档 5.3 节：非经典逻辑公理包加载提示机制。
 */
static void axiom_package_emit_load_hints(AxiomPackage *pkg) {
    if (!pkg)
        return;

    /* 1. 检查是否覆盖 ⊥ 定义或矛盾行为 —— 非标准否定语义 */
    if (pkg->contradiction_behavior != CONSTRUCTIVE ||
        (pkg->bottom_geometry && lv_str_ne(pkg->bottom_geometry, "default"))) {
        LOG_WARN("axiom", "公理包 '%s' 使用了非标准否定语义", pkg->name ? pkg->name : "unnamed");
        if (axiom_stream_ctx) {
            stream_emit_warning(axiom_stream_ctx, "该公理包使用了非标准否定语义", 0);
        }
    }

    /* 2. 检查是否包含非构造性 Oracle */
    if (pkg->contradiction_behavior == NON_CONSTRUCTIVE_ORACLE) {
        LOG_WARN("axiom", "公理包 '%s' 包含非构造性初始证物", pkg->name ? pkg->name : "unnamed");
        if (axiom_stream_ctx) {
            stream_emit_warning(axiom_stream_ctx, "该公理包包含非构造性初始证物", 0);
        }
    }

    /* 3. 检查是否包含爆炸原理 */
    if (pkg->contradiction_behavior == EXPLOSION_PRINCIPLE) {
        LOG_WARN("axiom", "公理包 '%s' 包含从矛盾推导任意命题的规则", pkg->name ? pkg->name : "unnamed");
        if (axiom_stream_ctx) {
            stream_emit_warning(axiom_stream_ctx, "该公理包包含从矛盾推导任意命题的规则", 0);
        }
    }
}

/* 完整的包加载函数 */
AxiomLoadStatus axiom_package_load(AxiomPackage *pkg, const char *filepath) {
    if (!pkg || !filepath) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "无效参数");
        return AXIOM_LOAD_PARSE_ERROR;
    }

    /* 清除之前的错误 */
    lv_clear_error();

    /* 读取文件（统一走 lv_file_read_all_limited；buf 已保证以 '\0' 结尾，
     * 且已内置 64MB 大小上限即 AXIOM_MAX_FILE_SIZE 检查） */
    size_t len = 0;
    char *buf = (char *) lv_file_read_all_limited(filepath, &len, AXIOM_MAX_FILE_SIZE);
    if (!buf) {
        /* 与原实现一致：打开失败 → FILE_NOT_FOUND，空文件/超限/读取异常 → PARSE_ERROR */
        lv_set_error(lv_ERROR_IO, "无法读取文件: %s", filepath);
        return lv_file_exists(filepath) ? AXIOM_LOAD_PARSE_ERROR : AXIOM_LOAD_FILE_NOT_FOUND;
    }

    /* 初始化解析器 */
    Parser parser;
    parser_init(&parser, buf);

    /* 获取第一个 token */
    parser_advance(&parser);

    /* 期望 'axiom' 关键字 */
    if (!parser_expect_identifier(&parser, "axiom")) {
        lv_free((void **) &buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    parser_advance(&parser);

    /* 期望包名 (字符串) */
    if (!parser_expect(&parser, PKG_STRING)) {
        lv_free((void **) &buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    lv_free((void **) &pkg->name);
    pkg->name = lv_strdup_safe(parser.current.str_value);
    parser_advance(&parser);

    /* 期望版本 (字符串) */
    if (!parser_expect(&parser, PKG_STRING)) {
        lv_free((void **) &buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    lv_free((void **) &pkg->version);
    pkg->version = lv_strdup_safe(parser.current.str_value);
    parser_advance(&parser);

    /* 期望左大括号 */
    if (!parser_expect(&parser, PKG_LBRACE)) {
        lv_free((void **) &buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    parser_advance(&parser);

    /* 解析包体 */
    if (!parse_package_body(&parser, pkg)) {
        lv_free((void **) &buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }

    /* 期望右大括号 */
    if (!parser_expect(&parser, PKG_RBRACE)) {
        lv_free((void **) &buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }

    lv_free((void **) &buf);

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "公理包加载成功", 0);
    }

    axiom_package_emit_load_hints(pkg);

    return AXIOM_LOAD_OK;
}
