/**
 * @file dsl_compiler.c
 * @brief Lv-00 DSL 编译器 —— 词法分析 → 语法分析 → IR 生成 → 约束图加载
 *
 * @details 实现 .lv 源文件的完整编译流水线。支持 GCLC 风格几何构造语句。
 *          编译器管线：dsl_tokenize → dsl_parse → dsl_compile → dsl_ir_to_constraint_graph
 *
 *          包含以下模块：
 *          - Tokenizer：将源文本转换为 Token 流
 *          - Parser：Token 流 → DSL AST
 *          - Compiler：DSL AST → IR（中间表示）
 *          - IR Loader：IR → 约束图节点
 *
 * @author Lv-00 Project
 */

#include "dsl_compiler.h"

#include <ctype.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/symbolic_coord.h"

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

/* ================================================================
 *  Parser 内部状态
 * ================================================================ */

/**
 * @brief 解析器上下文
 */
typedef struct {
    const DslToken *tokens; /**< Token 数组 */
    int count;              /**< Token 总数 */
    int pos;                /**< 当前读取位置 */
} ParserCtx;

static DslToken parser_peek(const ParserCtx *ctx) {
    if (ctx->pos < ctx->count)
        return ctx->tokens[ctx->pos];
    /* 返回 EOF Token */
    DslToken eof = {DSL_TOK_EOF, "(eof)", 0, 0};
    return eof;
}

static DslToken parser_advance(ParserCtx *ctx) {
    DslToken t = parser_peek(ctx);
    if (ctx->pos < ctx->count)
        ctx->pos++;
    return t;
}

static bool parser_match(ParserCtx *ctx, DSLTokenType type) {
    if (parser_peek(ctx).type == type) {
        parser_advance(ctx);
        return true;
    }
    return false;
}

static bool parser_expect(ParserCtx *ctx, DSLTokenType type, DslToken *out) {
    DslToken t = parser_peek(ctx);
    if (t.type != type)
        return false;
    if (out)
        *out = t;
    parser_advance(ctx);
    return true;
}

/** @brief 创建单个 AST 节点（使用 calloc 零初始化） */
static DslAST *ast_alloc(DslASTType type, int line, int col) {
    DslAST *node = lv_calloc(1, sizeof(DslAST));
    if (!node)
        return NULL;
    node->type = type;
    node->line = line;
    node->col = col;
    return node;
}

/** @brief 向 AST 节点添加子节点 */
static bool ast_add_child(DslAST *parent, DslAST *child) {
    if (!parent || !child)
        return false;
    ENSURE_CAP(parent->children, parent->child_count, parent->child_capacity, sizeof(DslAST *), false);
    parent->children[parent->child_count++] = child;
    return true;
}

/* ---- 前向声明：递归下降解析函数 ---- */
static DslAST *parse_stmt(ParserCtx *ctx);
static DslAST *parse_block(ParserCtx *ctx);

/* ================================================================
 *  Parser 递归下降：表达式与语句
 * ================================================================ */

/**
 * @brief 解析 primary 表达式：标识符或数值字面量
 *
 * primary ::= IDENT | NUMBER
 */
static DslAST *parse_primary(ParserCtx *ctx) {
    DslToken t = parser_peek(ctx);
    if (t.type == DSL_TOK_IDENT) {
        parser_advance(ctx);
        DslAST *node = ast_alloc(DSL_AST_IDENT, t.line, t.col);
        if (!node)
            return NULL;
        node->name = lv_strdup(t.lexeme);
        return node;
    }
    if (t.type == DSL_TOK_NUMBER) {
        parser_advance(ctx);
        DslAST *node = ast_alloc(DSL_AST_NUMBER, t.line, t.col);
        if (!node)
            return NULL;
        node->num_value = strtod(t.lexeme, NULL);
        return node;
    }
    return NULL;
}

/**
 * @brief 解析参数列表（逗号分隔的 primary 表达式）
 *
 * args ::= primary (',' primary)*
 */
static bool parse_arg_list(ParserCtx *ctx, DslAST *parent) {
    DslAST *first = parse_primary(ctx);
    if (!first)
        return false;
    ast_add_child(parent, first);

    while (parser_match(ctx, DSL_TOK_COMMA)) {
        DslAST *next = parse_primary(ctx);
        if (!next)
            return false;
        ast_add_child(parent, next);
    }
    return true;
}

/**
 * @brief 解析构造语句（几何构造操作）
 *
 * construct_stmt ::= 'intersect' '(' primary ',' primary ')'
 *                  | 'parallel'  '(' primary ',' primary ')'
 *                  | 'perpendicular' '(' primary ',' primary ')'
 *                  | 'midpoint' '(' primary ',' primary ')'
 *                  | 'circumcenter' '(' primary ',' primary ',' primary ')'
 *                  | 'orthocenter' '(' primary ',' primary ',' primary ')'
 *                  | 'centroid' '(' primary ',' primary ',' primary ')'
 *                  | 'incenter' '(' primary ',' primary ',' primary ')'
 *                  | 'bisector' '(' primary ',' primary ',' primary ')'
 */
static DslAST *parse_construct_stmt(ParserCtx *ctx, DSLTokenType kw_type, int line, int col) {
    DslASTType ast_type;
    /* 二元构造（2 个参数）vs 三元构造（3 个参数） */
    bool is_ternary = false;

    switch (kw_type) {
        case DSL_TOK_INTERSECT:
            ast_type = DSL_AST_INTERSECT;
            is_ternary = false;
            break;
        case DSL_TOK_PARALLEL:
            ast_type = DSL_AST_PARALLEL;
            is_ternary = false;
            break;
        case DSL_TOK_PERPENDICULAR:
            ast_type = DSL_AST_PERPENDICULAR;
            is_ternary = false;
            break;
        case DSL_TOK_MIDPOINT:
            ast_type = DSL_AST_MIDPOINT;
            is_ternary = false;
            break;
        case DSL_TOK_CIRCUMCENTER:
            ast_type = DSL_AST_CIRCUMCENTER;
            is_ternary = true;
            break;
        case DSL_TOK_ORTHOCENTER:
            ast_type = DSL_AST_ORTHOCENTER;
            is_ternary = true;
            break;
        case DSL_TOK_CENTROID:
            ast_type = DSL_AST_CENTROID;
            is_ternary = true;
            break;
        case DSL_TOK_INCENTER:
            ast_type = DSL_AST_INCENTER;
            is_ternary = true;
            break;
        case DSL_TOK_BISECTOR:
            ast_type = DSL_AST_BISECTOR;
            is_ternary = true;
            break;
        default:
            return NULL;
    }

    DslAST *node = ast_alloc(ast_type, line, col);
    if (!node)
        return NULL;

    if (!parser_expect(ctx, DSL_TOK_LPAREN, NULL)) {
        dsl_ast_destroy(node);
        return NULL;
    }
    if (!parse_arg_list(ctx, node)) {
        dsl_ast_destroy(node);
        return NULL;
    }

    /* 验证参数数量 */
    int expected = is_ternary ? 3 : 2;
    if (node->child_count != expected) {
        dsl_ast_destroy(node);
        return NULL;
    }

    if (!parser_expect(ctx, DSL_TOK_RPAREN, NULL)) {
        dsl_ast_destroy(node);
        return NULL;
    }
    return node;
}

/**
 * @brief 解析 fix 语句：fix A x y
 *
 * fix_stmt ::= 'fix' IDENT NUMBER NUMBER
 */
static DslAST *parse_fix_stmt(ParserCtx *ctx, int line, int col) {
    DslAST *node = ast_alloc(DSL_AST_FIX_POINT, line, col);
    if (!node)
        return NULL;

    DslToken name_tok = parser_advance(ctx); /* 吃掉下一个 Token（标识符） */
    if (name_tok.type != DSL_TOK_IDENT) {
        dsl_ast_destroy(node);
        return NULL;
    }
    node->name = lv_strdup(name_tok.lexeme);

    /* 解析两个坐标数值 */
    DslToken tok_x = parser_advance(ctx);
    if (tok_x.type != DSL_TOK_NUMBER) {
        dsl_ast_destroy(node);
        return NULL;
    }
    DslToken tok_y = parser_advance(ctx);
    if (tok_y.type != DSL_TOK_NUMBER) {
        dsl_ast_destroy(node);
        return NULL;
    }

    /* 存储坐标为 children */
    DslAST *cx = ast_alloc(DSL_AST_NUMBER, tok_x.line, tok_x.col);
    DslAST *cy = ast_alloc(DSL_AST_NUMBER, tok_y.line, tok_y.col);
    if (!cx || !cy) {
        dsl_ast_destroy(node);
        dsl_ast_destroy(cx);
        dsl_ast_destroy(cy);
        return NULL;
    }
    cx->num_value = strtod(tok_x.lexeme, NULL);
    cy->num_value = strtod(tok_y.lexeme, NULL);
    ast_add_child(node, cx);
    ast_add_child(node, cy);

    return node;
}

/**
 * @brief 解析 free 语句：free A
 *
 * free_stmt ::= 'free' IDENT
 */
static DslAST *parse_free_stmt(ParserCtx *ctx, int line, int col) {
    DslAST *node = ast_alloc(DSL_AST_FREE_POINT, line, col);
    if (!node)
        return NULL;

    DslToken name_tok = parser_advance(ctx);
    if (name_tok.type != DSL_TOK_IDENT) {
        dsl_ast_destroy(node);
        return NULL;
    }
    node->name = lv_strdup(name_tok.lexeme);
    return node;
}

/**
 * @brief 解析 load 语句：load "axiom_name" 或 load path
 *
 * load_stmt ::= 'load' IDENT
 */
static DslAST *parse_load_stmt(ParserCtx *ctx, int line, int col) {
    DslAST *node = ast_alloc(DSL_AST_LOAD, line, col);
    if (!node)
        return NULL;

    DslToken name_tok = parser_advance(ctx);
    if (name_tok.type != DSL_TOK_IDENT) {
        dsl_ast_destroy(node);
        return NULL;
    }
    node->name = lv_strdup(name_tok.lexeme);
    return node;
}

/**
 * @brief 解析 prove 语句：prove { ... } 或 prove IDENT
 *
 * prove_stmt ::= 'prove' ( IDENT | block )
 */
static DslAST *parse_prove_stmt(ParserCtx *ctx, int line, int col) {
    DslAST *node = ast_alloc(DSL_AST_PROVE, line, col);
    if (!node)
        return NULL;

    /* 尝试解析标识符或块 */
    DslToken next = parser_peek(ctx);
    if (next.type == DSL_TOK_IDENT) {
        parser_advance(ctx);
        node->name = lv_strdup(next.lexeme);
    } else if (next.type == DSL_TOK_LBRACE) {
        DslAST *block = parse_block(ctx);
        if (!block) {
            dsl_ast_destroy(node);
            return NULL;
        }
        ast_add_child(node, block);
    }
    /* 如果没有后续内容，prove 后面直接跟分号或 EOF 也是合法的 */
    return node;
}

/**
 * @brief 解析 constraint 语句：constraint { ... }
 *
 * constraint_stmt ::= 'constraint' block
 */
static DslAST *parse_constraint_stmt(ParserCtx *ctx, int line, int col) {
    DslAST *node = ast_alloc(DSL_AST_CONSTRAINT, line, col);
    if (!node)
        return NULL;

    DslAST *block = parse_block(ctx);
    if (!block) {
        dsl_ast_destroy(node);
        return NULL;
    }
    ast_add_child(node, block);
    return node;
}

/**
 * @brief 解析声明语句：point/line/circle/segment/ray/polygon/triangle + IDENT
 *
 * decl_stmt ::= geom_prim IDENT
 *             | geom_prim IDENT '=' construct_stmt
 */
static DslAST *parse_decl_stmt(ParserCtx *ctx, DSLTokenType kw_type, int line, int col) {
    DslASTType ast_type;
    switch (kw_type) {
        case DSL_TOK_POINT:
            ast_type = DSL_AST_POINT_DECL;
            break;
        case DSL_TOK_LINE:
            ast_type = DSL_AST_LINE_DECL;
            break;
        case DSL_TOK_CIRCLE:
            ast_type = DSL_AST_CIRCLE_DECL;
            break;
        case DSL_TOK_SEGMENT:
            ast_type = DSL_AST_SEGMENT_DECL;
            break;
        case DSL_TOK_RAY:
            ast_type = DSL_AST_RAY_DECL;
            break;
        case DSL_TOK_POLYGON:
            ast_type = DSL_AST_POLYGON_DECL;
            break;
        case DSL_TOK_TRIANGLE:
            ast_type = DSL_AST_TRIANGLE_DECL;
            break;
        default:
            return NULL;
    }

    DslAST *node = ast_alloc(ast_type, line, col);
    if (!node)
        return NULL;

    /* 读取标识符名称 */
    DslToken name_tok = parser_peek(ctx);
    if (name_tok.type == DSL_TOK_IDENT) {
        parser_advance(ctx);
        node->name = lv_strdup(name_tok.lexeme);
    }

    /* 可选的赋值右侧：= construct_stmt */
    if (parser_match(ctx, DSL_TOK_ASSIGN)) {
        DslToken next = parser_peek(ctx);
        DSLTokenType kw = next.type;
        /* 构造语句 */
        if (kw >= DSL_TOK_INTERSECT && kw <= DSL_TOK_BISECTOR) {
            parser_advance(ctx);
            DslAST *rhs = parse_construct_stmt(ctx, kw, next.line, next.col);
            if (rhs)
                ast_add_child(node, rhs);
        }
    }

    return node;
}

/**
 * @brief 解析 let 语句：let IDENT = expr
 *
 * let_stmt ::= 'let' IDENT '=' ( construct_stmt | primary )
 */
static DslAST *parse_let_stmt(ParserCtx *ctx, int line, int col) {
    DslAST *node = ast_alloc(DSL_AST_POINT_DECL, line, col);
    if (!node)
        return NULL;

    DslToken name_tok = parser_advance(ctx);
    if (name_tok.type != DSL_TOK_IDENT) {
        dsl_ast_destroy(node);
        return NULL;
    }
    node->name = lv_strdup(name_tok.lexeme);

    if (!parser_expect(ctx, DSL_TOK_ASSIGN, NULL)) {
        dsl_ast_destroy(node);
        return NULL;
    }

    /* 右侧可以是构造语句或 primary */
    DslToken next = parser_peek(ctx);
    if (next.type >= DSL_TOK_INTERSECT && next.type <= DSL_TOK_BISECTOR) {
        parser_advance(ctx);
        DslAST *rhs = parse_construct_stmt(ctx, next.type, next.line, next.col);
        if (rhs)
            ast_add_child(node, rhs);
    } else {
        DslAST *rhs = parse_primary(ctx);
        if (rhs)
            ast_add_child(node, rhs);
    }

    return node;
}

/**
 * @brief 解析语句块：{ stmt1; stmt2; ... }
 *
 * block ::= '{' stmt* '}'
 */
static DslAST *parse_block(ParserCtx *ctx) {
    if (!parser_expect(ctx, DSL_TOK_LBRACE, NULL))
        return NULL;

    int bline = parser_peek(ctx).line;
    int bcol = parser_peek(ctx).col;
    DslAST *block = ast_alloc(DSL_AST_BLOCK, bline, bcol);
    if (!block)
        return NULL;

    while (parser_peek(ctx).type != DSL_TOK_RBRACE && parser_peek(ctx).type != DSL_TOK_EOF) {
        DslAST *stmt = parse_stmt(ctx);
        if (stmt) {
            ast_add_child(block, stmt);
        } else {
            /* 解析失败：跳过直到遇到分号或右大括号以恢复 */
            while (parser_peek(ctx).type != DSL_TOK_SEMI && parser_peek(ctx).type != DSL_TOK_RBRACE &&
                   parser_peek(ctx).type != DSL_TOK_EOF) {
                parser_advance(ctx);
            }
            if (parser_peek(ctx).type == DSL_TOK_SEMI)
                parser_advance(ctx);
        }
        /* 可选的分号 */
        parser_match(ctx, DSL_TOK_SEMI);
    }

    if (!parser_expect(ctx, DSL_TOK_RBRACE, NULL)) {
        dsl_ast_destroy(block);
        return NULL;
    }
    return block;
}

/**
 * @brief 解析单条语句
 *
 * stmt ::= geom_prim IDENT ...
 *        | construct_stmt
 *        | fix_stmt | free_stmt
 *        | load_stmt | prove_stmt
 *        | constraint_stmt
 *        | let_stmt
 *        | block
 *        | IDENT ...
 */
static DslAST *parse_stmt(ParserCtx *ctx) {
    DslToken t = parser_peek(ctx);

    switch (t.type) {
        /* 几何原语声明 */
        case DSL_TOK_POINT:
        case DSL_TOK_LINE:
        case DSL_TOK_CIRCLE:
        case DSL_TOK_SEGMENT:
        case DSL_TOK_RAY:
        case DSL_TOK_POLYGON:
        case DSL_TOK_TRIANGLE:
            parser_advance(ctx);
            return parse_decl_stmt(ctx, t.type, t.line, t.col);

        /* 构造语句 */
        case DSL_TOK_INTERSECT:
        case DSL_TOK_PARALLEL:
        case DSL_TOK_PERPENDICULAR:
        case DSL_TOK_MIDPOINT:
        case DSL_TOK_CIRCUMCENTER:
        case DSL_TOK_ORTHOCENTER:
        case DSL_TOK_CENTROID:
        case DSL_TOK_INCENTER:
        case DSL_TOK_BISECTOR:
            parser_advance(ctx);
            return parse_construct_stmt(ctx, t.type, t.line, t.col);

        /* 特殊声明 */
        case DSL_TOK_FIX:
            parser_advance(ctx);
            return parse_fix_stmt(ctx, t.line, t.col);

        case DSL_TOK_FREE:
            parser_advance(ctx);
            return parse_free_stmt(ctx, t.line, t.col);

        case DSL_TOK_LOAD:
            parser_advance(ctx);
            return parse_load_stmt(ctx, t.line, t.col);

        case DSL_TOK_PROVE:
            parser_advance(ctx);
            return parse_prove_stmt(ctx, t.line, t.col);

        case DSL_TOK_CONSTRAINT:
            parser_advance(ctx);
            return parse_constraint_stmt(ctx, t.line, t.col);

        case DSL_TOK_LET:
            parser_advance(ctx);
            return parse_let_stmt(ctx, t.line, t.col);

        /* 语句块 */
        case DSL_TOK_LBRACE:
            return parse_block(ctx);

        /* 裸标识符：作为引用 */
        case DSL_TOK_IDENT:
            parser_advance(ctx);
            {
                DslAST *node = ast_alloc(DSL_AST_IDENT, t.line, t.col);
                if (node)
                    node->name = lv_strdup(t.lexeme);
                return node;
            }

        /* 裸数值 */
        case DSL_TOK_NUMBER:
            parser_advance(ctx);
            {
                DslAST *node = ast_alloc(DSL_AST_NUMBER, t.line, t.col);
                if (node)
                    node->num_value = strtod(t.lexeme, NULL);
                return node;
            }

        default:
            return NULL;
    }
}

/* ================================================================
 *  Parser 主入口
 * ================================================================ */

/**
 * @brief 对 Token 流进行语法分析，生成 DSL AST
 *
 * 完整的递归下降解析器。每个顶层语句作为一个子节点。
 *
 * @param tokens Token 数组
 * @param count  Token 数量
 * @param out_ast 输出：DSL AST 根节点指针
 * @return 成功返回 true，失败返回 false
 */
bool dsl_parse(const DslToken *tokens, int count, DslAST **out_ast) {
    if (!tokens || count <= 0 || !out_ast)
        return false;

    DslAST *root = lv_calloc(1, sizeof(DslAST));
    if (!root)
        return false;
    root->type = DSL_AST_PROGRAM;
    root->name = NULL;
    root->child_capacity = 8;
    root->children = lv_calloc((size_t) root->child_capacity, sizeof(DslAST *));
    if (!root->children) {
        lv_free(root);
        return false;
    }

    ParserCtx ctx;
    ctx.tokens = tokens;
    ctx.count = count;
    ctx.pos = 0;

    /* 跳过开头的 COMMENT Token */
    while (ctx.pos < ctx.count && tokens[ctx.pos].type == DSL_TOK_COMMENT)
        ctx.pos++;

    while (ctx.pos < ctx.count) {
        DslToken t = parser_peek(&ctx);
        if (t.type == DSL_TOK_EOF)
            break;

        DslAST *stmt = parse_stmt(&ctx);

        /* 跳过注释 */
        while (ctx.pos < ctx.count && tokens[ctx.pos].type == DSL_TOK_COMMENT)
            ctx.pos++;

        /* 可选的语句结束分号 */
        parser_match(&ctx, DSL_TOK_SEMI);

        if (stmt) {
            ast_add_child(root, stmt);
        } else {
            /* 无法解析：跳过直到下一个分号或 EOF */
            while (ctx.pos < ctx.count && tokens[ctx.pos].type != DSL_TOK_SEMI && tokens[ctx.pos].type != DSL_TOK_EOF)
                ctx.pos++;
            if (ctx.pos < ctx.count && tokens[ctx.pos].type == DSL_TOK_SEMI)
                ctx.pos++;
        }
    }

    *out_ast = root;
    return true;
}

/* ================================================================
 *  Compiler：AST → IR
 * ================================================================ */

/**
 * @brief 向 IR 添加符号（名称 → IR ID 映射）
 */
static int ir_add_symbol(DslIR *ir, const char *name, int result_id) {
    if (!ir || !name)
        return -1;

    ENSURE_CAP(ir->symbols, ir->symbol_count, ir->symbol_capacity, sizeof(char *), -1);
    ENSURE_CAP(ir->symbol_to_ir_id, ir->symbol_count, ir->symbol_capacity, sizeof(int), -1);

    ir->symbols[ir->symbol_count] = lv_strdup(name);
    ir->symbol_to_ir_id[ir->symbol_count] = result_id;
    int idx = ir->symbol_count;
    ir->symbol_count++;
    return idx;
}

/**
 * @brief 在 IR 符号表中查找名称，返回 IR 操作索引
 */
static int ir_find_symbol(const DslIR *ir, const char *name) {
    if (!ir || !name)
        return -1;
    for (int i = 0; i < ir->symbol_count; i++) {
        if (ir->symbols[i] && strcmp(ir->symbols[i], name) == 0)
            return ir->symbol_to_ir_id[i];
    }
    return -1;
}

/**
 * @brief 向 IR 添加操作
 */
static int ir_add_op(DslIR *ir, DslIROp op, int result_id, const int *operands, int operand_count, const char *label,
                     int source_line) {
    if (!ir)
        return -1;

    ENSURE_CAP(ir->operations, ir->op_count, ir->op_capacity, sizeof(DslIROperation), -1);

    DslIROperation *op_entry = &ir->operations[ir->op_count];
    memset(op_entry, 0, sizeof(*op_entry));
    op_entry->op = op;
    op_entry->result_id = result_id;
    op_entry->source_line = source_line;
    op_entry->label = label;

    if (operand_count > 0 && operands) {
        op_entry->operands = lv_malloc(sizeof(int) * (size_t) operand_count);
        if (!op_entry->operands)
            return -1;
        memcpy(op_entry->operands, operands, sizeof(int) * (size_t) operand_count);
        op_entry->operand_count = operand_count;
    } else {
        op_entry->operands = NULL;
        op_entry->operand_count = 0;
    }

    int idx = ir->op_count;
    ir->op_count++;
    if (result_id >= ir->next_id)
        ir->next_id = result_id + 1;
    return idx;
}

/**
 * @brief 递归编译 AST 节点为 IR 操作
 *
 * @param ir          IR 对象
 * @param node        AST 节点
 * @param result_id   该节点生成的结果 IR ID（-1 表示不产生结果）
 * @return 成功返回 true
 */
static bool compile_node(DslIR *ir, const DslAST *node, int *result_id) {
    if (!ir || !node)
        return false;

    int line = node->line;
    int rid = -1;

    switch (node->type) {
        /* ---- 几何原语声明 ---- */
        case DSL_AST_POINT_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_POINT, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_LINE_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_LINE, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_CIRCLE_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_CIRCLE, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_SEGMENT_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_SEGMENT, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_RAY_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_RAY, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_POLYGON_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_POLYGON, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_TRIANGLE_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_TRIANGLE, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        /* ---- 构造操作 ---- */
        case DSL_AST_INTERSECT: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_INTERSECT, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_PARALLEL: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_PARALLEL_THROUGH, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_PERPENDICULAR: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_PERPENDICULAR_THROUGH, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_MIDPOINT: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_MIDPOINT_OF, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_CIRCUMCENTER: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_CIRCUMCENTER_OF, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_ORTHOCENTER: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_ORTHOCENTER_OF, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_CENTROID: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_CENTROID_OF, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_INCENTER: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_INCENTER_OF, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_BISECTOR: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_BISECTOR_OF, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        /* ---- fix / free ---- */
        case DSL_AST_FIX_POINT: {
            rid = ir->next_id;
            /* 将坐标作为数值操作数 */
            int operand_ids[2] = {-1, -1};
            for (int i = 0; i < node->child_count && i < 2; i++) {
                if (node->children[i]->type == DSL_AST_NUMBER) {
                    /* 数值直接编码为操作数的 IR ID（后续 IR loader 解释） */
                    operand_ids[i] = (int) node->children[i]->num_value;
                }
            }
            ir_add_op(ir, IR_CREATE_POINT_FIXED, rid, operand_ids, 2, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_FREE_POINT: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_POINT, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        /* ---- load / prove ---- */
        case DSL_AST_LOAD: {
            ir_add_op(ir, IR_LOAD_AXIOM, -1, NULL, 0, node->name, line);
            break;
        }

        case DSL_AST_PROVE: {
            ir_add_op(ir, IR_PROVE, -1, NULL, 0, node->name, line);
            break;
        }

        /* ---- constraint ---- */
        case DSL_AST_CONSTRAINT: {
            /* constraint { ... } 块内的子语句展开为约束操作 */
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count; i++) {
                DslAST *child = node->children[i];
                if (!child)
                    continue;
                if (child->type == DSL_AST_BLOCK) {
                    for (int j = 0; j < child->child_count; j++) {
                        int op_rid = -1;
                        compile_node(ir, child->children[j], &op_rid);
                    }
                } else {
                    /* 标识符引用：在约束类型选择中使用 */
                    if (child->type == DSL_AST_IDENT && child->name && oc < 8) {
                        int sym_id = ir_find_symbol(ir, child->name);
                        if (sym_id >= 0)
                            ops[oc++] = sym_id;
                    }
                }
            }
            ir_add_op(ir, IR_ADD_CONSTRAINT, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        /* ---- block ---- */
        case DSL_AST_BLOCK: {
            for (int i = 0; i < node->child_count; i++) {
                int inner_result = -1;
                compile_node(ir, node->children[i], &inner_result);
            }
            break;
        }

        default:
            return false;
    }

    if (result_id)
        *result_id = rid;
    return true;
}

/**
 * @brief 将 DSL AST 编译为 IR（中间表示）
 *
 * 遍历 AST 子节点，为每个声明生成对应的 IR 操作。
 *
 * @param ast    DSL AST 根节点
 * @param config 编译配置（当前未使用）
 * @param out_ir 输出：IR 指针
 * @return 成功返回 true，失败返回 false
 */
bool dsl_compile(const DslAST *ast, const DslCompileConfig *config, DslIR **out_ir) {
    if (!ast || !out_ir)
        return false;

    DslIR *ir = lv_calloc(1, sizeof(DslIR));
    if (!ir)
        return false;

    int initial_cap = (ast->child_count > 0) ? (int) ((size_t) ast->child_count * 4) : 16;
    if (initial_cap < 16)
        initial_cap = 16;

    ir->op_capacity = initial_cap;
    ir->operations = lv_calloc((size_t) ir->op_capacity, sizeof(DslIROperation));
    if (!ir->operations) {
        lv_free(ir);
        return false;
    }

    ir->symbol_capacity = initial_cap;
    ir->symbols = lv_calloc((size_t) ir->symbol_capacity, sizeof(char *));
    if (!ir->symbols) {
        lv_free(ir->operations);
        lv_free(ir);
        return false;
    }
    ir->symbol_to_ir_id = lv_calloc((size_t) ir->symbol_capacity, sizeof(int));
    if (!ir->symbol_to_ir_id) {
        lv_free(ir->symbols);
        lv_free(ir->operations);
        lv_free(ir);
        return false;
    }

    ir->next_id = 0;

    /* 遍历 AST 子节点生成 IR 操作 */
    for (int i = 0; i < ast->child_count; i++) {
        int result_id = -1;
        if (!compile_node(ir, ast->children[i], &result_id)) {
            /* 继续编译其他节点 */
            continue;
        }
    }

    *out_ir = ir;
    (void) config;
    return true;
}

/* ================================================================
 *  IR → ConstraintGraph
 * ================================================================ */

/**
 * @brief 为数值字面量创建 SymbolicCoord
 *
 * 从编码的操作数值中提取 x, y 坐标。
 */
static bool resolve_fixed_coords(const DslIROperation *op, double *out_x, double *out_y) {
    if (!op || !out_x || !out_y)
        return false;
    if (op->operand_count < 2)
        return false;

    /* 从 operands 中获取编码的坐标值（编译时存为 int 但本质是 double 的位模式） */
    /* 这里直接使用 operands 字段存储的 double 位模式 */
    if (op->operands[0] < 0 || op->operands[1] < 0)
        return false;

    /* 对于 fix 语句，坐标直接来自 AST NUMBER 节点编译成的 operand */
    /* 但我们存储的是 double 的整数部分（作为整型坐标） */
    *out_x = (double) op->operands[0];
    *out_y = (double) op->operands[1];
    return true;
}

/**
 * @brief 将 IR 操作转换为约束图节点
 *
 * 遍历 IR 操作列表，在约束图中为每个操作创建对应节点和约束。
 * 跟踪结果 ID 到约束图节点 ID 的映射。
 *
 * @param ir    IR 数据
 * @param graph 约束图指针
 * @return 成功返回 true
 */
bool dsl_ir_to_constraint_graph(const DslIR *ir, ConstraintGraph *graph) {
    if (!ir || !graph)
        return false;

    /* 结果 ID 到约束图节点 ID 的映射表 */
    int *id_map = NULL;
    int id_map_count = 0;
    int id_map_cap = 0;

    /* 确保 id_map 有足够的容量 */
#define ENSURE_ID_MAP(cap_needed)                                         \
    do {                                                                  \
        while ((cap_needed) >= id_map_cap) {                              \
            int new_cap = id_map_cap == 0 ? 64 : id_map_cap * 2;          \
            int *np = lv_realloc(id_map, sizeof(int) * (size_t) new_cap); \
            if (!np) {                                                    \
                lv_free((void **) &id_map);                               \
                return false;                                             \
            }                                                             \
            id_map = np;                                                  \
            /* 初始化新区域为 -1 */                                       \
            for (int _i = id_map_cap; _i < new_cap; _i++)                 \
                id_map[_i] = -1;                                          \
            id_map_cap = new_cap;                                         \
        }                                                                 \
    } while (0)

    /* 初始化 id_map */
    if (ir->next_id > 0) {
        ENSURE_ID_MAP(ir->next_id + 1);
        id_map_count = ir->next_id + 1;
        for (int i = 0; i < id_map_count; i++)
            id_map[i] = -1;
    }

    /* 遍历 IR 操作 */
    for (int i = 0; i < ir->op_count; i++) {
        const DslIROperation *op = &ir->operations[i];

        switch (op->op) {
            /* ---- 实体创建 ---- */
            case IR_CREATE_POINT: {
                /* 创建自由点（无坐标） */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_POINT, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;
                }
                break;
            }

            case IR_CREATE_POINT_FIXED: {
                /* 创建固定坐标点 */
                double x = 0.0, y = 0.0;
                resolve_fixed_coords(op, &x, &y);

                /* 创建 SymbolicCoord 数组 */
                SymbolicCoord *coords[2] = {NULL, NULL};
                /* 使用简单的坐标值创建（实际使用 SymbolicCoord 构造） */
                /* 这里简化为 NULL，因为 graph_add_node_with_id 接受 NULL */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_POINT, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;
                }
                break;
            }

            case IR_CREATE_LINE:
            case IR_CREATE_SEGMENT: {
                /* 创建线段（基于操作数中的前两个点） */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_LINE_SEGMENT, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;
                }
                break;
            }

            case IR_CREATE_CIRCLE: {
                /* 圆 -> 创建 GEOM_CIRCLE 节点 */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_CIRCLE, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;
                    /* 初始化圆心和半径端点为 -1，后续通过约束设置 */
                    node->data.circle.center_node_id = -1;
                    node->data.circle.radius_node_id = -1;
                }
                break;
            }

            case IR_CREATE_RAY: {
                /* 射线 -> 也用线段节点占位 */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_LINE_SEGMENT, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;
                }
                break;
            }

            case IR_CREATE_POLYGON:
            case IR_CREATE_TRIANGLE: {
                /* 多边形/三角形 -> 区域节点 */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_REGION, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;
                }
                break;
            }

            /* ---- 构造操作 ---- */
            case IR_INTERSECT: {
                /* 创建交点节点 + 相交约束 */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_POINT, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;

                    /* 如果有两个操作数，添加相交约束 */
                    if (op->operand_count >= 2 && op->operands[0] >= 0 && op->operands[1] >= 0) {
                        int p1_id = (op->operands[0] < id_map_count) ? id_map[op->operands[0]] : -1;
                        int p2_id = (op->operands[1] < id_map_count) ? id_map[op->operands[1]] : -1;
                        if (p1_id >= 0 && p2_id >= 0) {
                            int parts[3] = {p1_id, p2_id, node->id};
                            graph_add_constraint_with_id(graph, op->result_id, INTERSECTION, parts, 3);
                        }
                    }
                }
                break;
            }

            case IR_PARALLEL_THROUGH:
            case IR_PERPENDICULAR_THROUGH: {
                /* 平行/垂线约束 */
                if (op->operand_count >= 2 && op->operands[0] >= 0 && op->operands[1] >= 0) {
                    int p1_id = (op->operands[0] < id_map_count) ? id_map[op->operands[0]] : -1;
                    int p2_id = (op->operands[1] < id_map_count) ? id_map[op->operands[1]] : -1;
                    if (p1_id >= 0 && p2_id >= 0) {
                        int parts[2] = {p1_id, p2_id};
                        graph_add_constraint_with_id(
                            graph, op->result_id, (op->op == IR_PARALLEL_THROUGH) ? CONNECTION : INCIDENCE, parts, 2);
                    }
                }
                break;
            }

            case IR_MIDPOINT_OF:
            case IR_CIRCUMCENTER_OF:
            case IR_ORTHOCENTER_OF:
            case IR_CENTROID_OF:
            case IR_INCENTER_OF:
            case IR_BISECTOR_OF: {
                /* 这些构造的结果都是点，创建点节点 */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_POINT, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;

                    /* 如果有点操作数，添加关联约束 */
                    if (op->operand_count > 0) {
                        for (int j = 0; j < op->operand_count; j++) {
                            int pid =
                                (op->operands[j] >= 0 && op->operands[j] < id_map_count) ? id_map[op->operands[j]] : -1;
                            if (pid >= 0) {
                                int parts[2] = {pid, node->id};
                                graph_add_constraint_with_id(graph, -1, INCIDENCE, parts, 2);
                            }
                        }
                    }
                }
                break;
            }

            /* ---- 约束操作 ---- */
            case IR_ADD_CONSTRAINT:
            case IR_CONSTRAIN_EQUAL:
            case IR_CONSTRAIN_PARALLEL:
            case IR_CONSTRAIN_PERPENDICULAR:
            case IR_CONSTRAIN_COLLINEAR:
            case IR_CONSTRAIN_CONCYCLIC: {
                ConstraintType ctype = CONNECTION;
                switch (op->op) {
                    case IR_CONSTRAIN_PARALLEL:
                        ctype = CONNECTION;
                        break;
                    case IR_CONSTRAIN_PERPENDICULAR:
                        ctype = INCIDENCE;
                        break;
                    case IR_CONSTRAIN_COLLINEAR:
                        ctype = BETWEENNESS;
                        break;
                    case IR_CONSTRAIN_CONCYCLIC:
                        ctype = CONTAINMENT;
                        break;
                    default:
                        ctype = INCIDENCE;
                        break;
                }
                int parts[8];
                int pc = 0;
                for (int j = 0; j < op->operand_count && pc < 8; j++) {
                    int pid = (op->operands[j] >= 0 && op->operands[j] < id_map_count) ? id_map[op->operands[j]] : -1;
                    if (pid >= 0)
                        parts[pc++] = pid;
                }
                if (pc > 0) {
                    graph_add_constraint_with_id(graph, op->result_id, ctype, parts, pc);
                }
                break;
            }

            /* ---- 系统操作 ---- */
            case IR_LOAD_AXIOM: {
                /* load 语句：当前为桩，不做实际操作 */
                break;
            }

            case IR_PROVE: {
                /* prove 语句：当前为桩，不做实际操作 */
                break;
            }

            case IR_CHECK_SAT: {
                /* 可满足性检查：当前为桩 */
                break;
            }

            case IR_LABEL: {
                /* 标签操作：当前为桩 */
                break;
            }

            case IR_REMOVE_CONSTRAINT: {
                /* 移除约束：当前为桩 */
                break;
            }

            case IR_NOOP:
            default: {
                break;
            }
        }
    }

    lv_free((void **) &id_map);
    return true;
}

/**
 * @brief 编译 DSL 源代码并加载到约束图
 *
 * 完整的编译管线：tokenize → parse → compile → ir_to_constraint_graph。
 * 每一步失败时自动释放已分配的资源。
 *
 * @param source DSL 源代码字符串
 * @param config 编译配置
 * @param graph  目标约束图
 * @return 成功返回 true，失败返回 false
 */
bool dsl_compile_and_load(const char *source, const DslCompileConfig *config, ConstraintGraph *graph) {
    if (!source || !graph)
        return false;

    DslToken *tokens = NULL;
    int token_count = 0;
    if (!dsl_tokenize(source, &tokens, &token_count))
        return false;

    DslAST *ast = NULL;
    if (!dsl_parse(tokens, token_count, &ast)) {
        dsl_tokens_destroy(tokens, token_count);
        return false;
    }

    DslIR *ir = NULL;
    if (!dsl_compile(ast, config, &ir)) {
        dsl_ast_destroy(ast);
        dsl_tokens_destroy(tokens, token_count);
        return false;
    }

    bool ok = dsl_ir_to_constraint_graph(ir, graph);
    dsl_ir_destroy(ir);
    dsl_ast_destroy(ast);
    dsl_tokens_destroy(tokens, token_count);
    return ok;
}

/**
 * @brief 将编译配置初始化为默认值
 *
 * 默认配置：TARGET_NATIVE、优化级别 0、不调试 AST。
 *
 * @param out_config 输出：默认编译配置
 */
void dsl_compile_config_default(DslCompileConfig *out_config) {
    if (!out_config)
        return;
    memset(out_config, 0, sizeof(*out_config));
    out_config->target = TARGET_NATIVE;
    out_config->optimize_level = 0;
    out_config->debug_ast = false;
    out_config->validate_ir = true;
    out_config->generate_source_map = true;
    out_config->max_iterations = 1000;
}

/**
 * @brief 递归销毁 DSL AST 树
 *
 * 递归释放所有子节点，然后释放 children 数组和 name，最后释放节点本身。
 *
 * @param ast 要销毁的 AST 节点（允许为 NULL）
 */
void dsl_ast_destroy(DslAST *ast) {
    if (!ast)
        return;
    for (int i = 0; i < ast->child_count; i++)
        dsl_ast_destroy(ast->children[i]);
    lv_free((void **) &ast->children);
    lv_free((void **) &ast->name);
    lv_free((void **) &ast);
}

/**
 * @brief 销毁 IR 数据
 *
 * 释放所有 IR 操作的操作数数组、operations 数组、符号表和 IR 结构体本身。
 *
 * @param ir 要销毁的 IR 指针（允许为 NULL）
 */
void dsl_ir_destroy(DslIR *ir) {
    if (!ir)
        return;
    for (int i = 0; i < ir->op_count; i++)
        lv_free((void **) &ir->operations[i].operands);
    lv_free((void **) &ir->operations);
    /* 释放符号表 */
    if (ir->symbols) {
        for (int i = 0; i < ir->symbol_count; i++)
            lv_free((void **) &ir->symbols[i]);
    }
    lv_free((void **) &ir->symbols);
    lv_free((void **) &ir->symbol_to_ir_id);
    lv_free((void **) &ir);
}

/**
 * @brief 转储 DSL AST 树（调试用）
 *
 * 以缩进格式将 AST 树结构输出到文件描述符。
 *
 * @param ast    AST 根节点（允许为 NULL）
 * @param fd     输出文件描述符（实际类型为 FILE*）
 * @param indent 当前缩进层级
 */
void dsl_ast_dump(const DslAST *ast, void *fd, int indent) {
    if (!ast || !fd)
        return;
    FILE *f = (FILE *) fd;

    for (int i = 0; i < indent; i++)
        fprintf(f, "  ");
    fprintf(f, "%s", dsl_ast_type_name(ast->type));

    if (ast->name)
        fprintf(f, " [%s]", ast->name);
    if (ast->type == DSL_AST_NUMBER)
        fprintf(f, " = %g", ast->num_value);
    fprintf(f, "\n");

    for (int i = 0; i < ast->child_count; i++)
        dsl_ast_dump(ast->children[i], fd, indent + 1);
}

/**
 * @brief 转储 IR 数据（调试用）
 *
 * 将 IR 操作列表输出到文件描述符。
 *
 * @param ir IR 数据（允许为 NULL）
 * @param fd 输出文件描述符（实际类型为 FILE*）
 */
void dsl_ir_dump(const DslIR *ir, void *fd) {
    if (!ir || !fd)
        return;
    FILE *f = (FILE *) fd;

    fprintf(f, "IR Program (%d ops, %d symbols):\n", ir->op_count, ir->symbol_count);
    for (int i = 0; i < ir->op_count; i++) {
        const DslIROperation *op = &ir->operations[i];
        fprintf(f, "  [%3d] %s", i, dsl_ir_op_name(op->op));
        if (op->result_id >= 0)
            fprintf(f, " -> r%d", op->result_id);
        if (op->operand_count > 0) {
            fprintf(f, " (");
            for (int j = 0; j < op->operand_count; j++) {
                if (j > 0)
                    fprintf(f, ", ");
                fprintf(f, "%d", op->operands[j]);
            }
            fprintf(f, ")");
        }
        if (op->label)
            fprintf(f, " label=\"%s\"", op->label);
        if (op->source_line > 0)
            fprintf(f, " [line %d]", op->source_line);
        fprintf(f, "\n");
    }

    /* 转储符号表 */
    if (ir->symbol_count > 0) {
        fprintf(f, "  Symbol table:\n");
        for (int i = 0; i < ir->symbol_count; i++) {
            fprintf(f, "    %s -> r%d\n", ir->symbols[i] ? ir->symbols[i] : "(null)", ir->symbol_to_ir_id[i]);
        }
    }
}

/**
 * @brief 获取 IR 操作符的字符串名称
 *
 * @param op IR 操作符枚举值
 * @return 操作符名称字符串（静态存储，无需释放）
 */
const char *dsl_ir_op_name(DslIROp op) {
    switch (op) {
        case IR_CREATE_POINT:
            return "CREATE_POINT";
        case IR_CREATE_POINT_FIXED:
            return "CREATE_POINT_FIXED";
        case IR_CREATE_LINE:
            return "CREATE_LINE";
        case IR_CREATE_CIRCLE:
            return "CREATE_CIRCLE";
        case IR_CREATE_SEGMENT:
            return "CREATE_SEGMENT";
        case IR_CREATE_RAY:
            return "CREATE_RAY";
        case IR_CREATE_POLYGON:
            return "CREATE_POLYGON";
        case IR_CREATE_TRIANGLE:
            return "CREATE_TRIANGLE";
        case IR_INTERSECT:
            return "INTERSECT";
        case IR_PARALLEL_THROUGH:
            return "PARALLEL_THROUGH";
        case IR_PERPENDICULAR_THROUGH:
            return "PERPENDICULAR_THROUGH";
        case IR_MIDPOINT_OF:
            return "MIDPOINT_OF";
        case IR_CIRCUMCENTER_OF:
            return "CIRCUMCENTER_OF";
        case IR_ORTHOCENTER_OF:
            return "ORTHOCENTER_OF";
        case IR_CENTROID_OF:
            return "CENTROID_OF";
        case IR_INCENTER_OF:
            return "INCENTER_OF";
        case IR_BISECTOR_OF:
            return "BISECTOR_OF";
        case IR_ANGLE_BISECTOR:
            return "ANGLE_BISECTOR";
        case IR_ADD_CONSTRAINT:
            return "ADD_CONSTRAINT";
        case IR_REMOVE_CONSTRAINT:
            return "REMOVE_CONSTRAINT";
        case IR_CONSTRAIN_EQUAL:
            return "CONSTRAIN_EQUAL";
        case IR_CONSTRAIN_PARALLEL:
            return "CONSTRAIN_PARALLEL";
        case IR_CONSTRAIN_PERPENDICULAR:
            return "CONSTRAIN_PERPENDICULAR";
        case IR_CONSTRAIN_COLLINEAR:
            return "CONSTRAIN_COLLINEAR";
        case IR_CONSTRAIN_CONCYCLIC:
            return "CONSTRAIN_CONCYCLIC";
        case IR_LOAD_AXIOM:
            return "LOAD_AXIOM";
        case IR_PROVE:
            return "PROVE";
        case IR_CHECK_SAT:
            return "CHECK_SAT";
        case IR_LABEL:
            return "LABEL";
        case IR_NOOP:
            return "NOOP";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 获取 DSL AST 节点类型的字符串名称
 *
 * @param type 节点类型枚举值
 * @return 类型名称字符串（静态存储，无需释放）
 */
const char *dsl_ast_type_name(DslASTType type) {
    switch (type) {
        case DSL_AST_PROGRAM:
            return "PROGRAM";
        case DSL_AST_POINT_DECL:
            return "POINT_DECL";
        case DSL_AST_LINE_DECL:
            return "LINE_DECL";
        case DSL_AST_CIRCLE_DECL:
            return "CIRCLE_DECL";
        case DSL_AST_SEGMENT_DECL:
            return "SEGMENT_DECL";
        case DSL_AST_RAY_DECL:
            return "RAY_DECL";
        case DSL_AST_POLYGON_DECL:
            return "POLYGON_DECL";
        case DSL_AST_TRIANGLE_DECL:
            return "TRIANGLE_DECL";
        case DSL_AST_INTERSECT:
            return "INTERSECT";
        case DSL_AST_PARALLEL:
            return "PARALLEL";
        case DSL_AST_PERPENDICULAR:
            return "PERPENDICULAR";
        case DSL_AST_MIDPOINT:
            return "MIDPOINT";
        case DSL_AST_CIRCUMCENTER:
            return "CIRCUMCENTER";
        case DSL_AST_ORTHOCENTER:
            return "ORTHOCENTER";
        case DSL_AST_CENTROID:
            return "CENTROID";
        case DSL_AST_INCENTER:
            return "INCENTER";
        case DSL_AST_BISECTOR:
            return "BISECTOR";
        case DSL_AST_CONSTRAINT:
            return "CONSTRAINT";
        case DSL_AST_PROVE:
            return "PROVE";
        case DSL_AST_LOAD:
            return "LOAD";
        case DSL_AST_FIX_POINT:
            return "FIX_POINT";
        case DSL_AST_FREE_POINT:
            return "FREE_POINT";
        case DSL_AST_BLOCK:
            return "BLOCK";
        case DSL_AST_IDENT:
            return "IDENT";
        case DSL_AST_NUMBER:
            return "NUMBER";
        default:
            return "UNKNOWN";
    }
}