/**
 * @file dsl_compiler_parse.c
 * @brief Lv-00 DSL 编译器 —— Parser 阶段：Token 流 → DSL AST
 *
 * @details 由 dsl_compiler.c 按编译流水线阶段拆分而来。
 *          编译器管线：dsl_tokenize → dsl_parse → dsl_compile
 *          → dsl_ir_to_constraint_graph
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#include "lv/dsl_compiler.h"
#include "dsl_compiler_internal.h"

#include <ctype.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_xmacro.h"
#include "lv/lv_parse_utils.h"

#include "lv/lv_internal.h"

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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate AST node");
    node->type = type;
    node->line = line;
    node->col = col;
    return node;
}

/** @brief 向 AST 节点添加子节点 */
static bool ast_add_child(DslAST *parent, DslAST *child) {
    if (!parent || !child)
        return false;
    /* 扩容子节点数组（统一走 lv_ENSURE_ARRAY_CAP） */
    lv_ENSURE_ARRAY_CAP(parent->children, parent->child_count, parent->child_capacity, false);
    parent->children[parent->child_count++] = child;
    return true;
}

/* ---- 前向声明：递归下降解析函数 ---- */
static DslAST *parse_stmt(ParserCtx *ctx);
static DslAST *parse_block(ParserCtx *ctx);

/* ================================================================
 *  Lookup tables for strategy pattern (replacing switch statements)
 * ================================================================ */

typedef DslAST *(*StmtParseFn)(ParserCtx *ctx);

/** @brief 构造语句类型查找表：DSL_TOK_* → {ast_type, is_ternary} */
static const struct {
    DslASTType ast_type;
    bool is_ternary;
} kConstructTypeTable[] = {
    [DSL_TOK_INTERSECT]    = {DSL_AST_INTERSECT, false},
    [DSL_TOK_PARALLEL]     = {DSL_AST_PARALLEL, false},
    [DSL_TOK_PERPENDICULAR] = {DSL_AST_PERPENDICULAR, false},
    [DSL_TOK_MIDPOINT]     = {DSL_AST_MIDPOINT, false},
    [DSL_TOK_CIRCUMCENTER] = {DSL_AST_CIRCUMCENTER, true},
    [DSL_TOK_ORTHOCENTER]  = {DSL_AST_ORTHOCENTER, true},
    [DSL_TOK_CENTROID]     = {DSL_AST_CENTROID, true},
    [DSL_TOK_INCENTER]     = {DSL_AST_INCENTER, true},
    [DSL_TOK_BISECTOR]     = {DSL_AST_BISECTOR, true},
};

/** @brief 声明语句类型查找表：DSL_TOK_* → DslASTType */
static const DslASTType kDeclTypeTable[] = {
    [DSL_TOK_POINT]   = DSL_AST_POINT_DECL,
    [DSL_TOK_LINE]    = DSL_AST_LINE_DECL,
    [DSL_TOK_CIRCLE]  = DSL_AST_CIRCLE_DECL,
    [DSL_TOK_SEGMENT] = DSL_AST_SEGMENT_DECL,
    [DSL_TOK_RAY]     = DSL_AST_RAY_DECL,
    [DSL_TOK_POLYGON] = DSL_AST_POLYGON_DECL,
    [DSL_TOK_TRIANGLE]= DSL_AST_TRIANGLE_DECL,
};

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
        if (lv_parse_double(t.lexeme, &node->num_value) != 0)
            node->num_value = 0.0;
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
    bool is_ternary;

    /* 二元构造（2 个参数）vs 三元构造（3 个参数） */
    if (kw_type >= DSL_TOK_INTERSECT && kw_type <= DSL_TOK_BISECTOR) {
        ast_type = kConstructTypeTable[kw_type].ast_type;
        is_ternary = kConstructTypeTable[kw_type].is_ternary;
    } else {
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
    if (lv_parse_double(tok_x.lexeme, &cx->num_value) != 0)
        cx->num_value = 0.0;
    if (lv_parse_double(tok_y.lexeme, &cy->num_value) != 0)
        cy->num_value = 0.0;
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
    if (kw_type >= DSL_TOK_POINT && kw_type <= DSL_TOK_TRIANGLE) {
        ast_type = kDeclTypeTable[kw_type];
    } else {
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

/* ================================================================
 *  Wrapper functions & handler table for parse_stmt strategy pattern
 * ================================================================ */

static DslAST *parse_geom_decl_stmt(ParserCtx *ctx) {
    DslToken t = parser_peek(ctx);
    parser_advance(ctx);
    return parse_decl_stmt(ctx, t.type, t.line, t.col);
}

static DslAST *parse_construct_stmt_wrapper(ParserCtx *ctx) {
    DslToken t = parser_peek(ctx);
    parser_advance(ctx);
    return parse_construct_stmt(ctx, t.type, t.line, t.col);
}

static DslAST *parse_fix_stmt_wrapper(ParserCtx *ctx) {
    DslToken t = parser_peek(ctx);
    parser_advance(ctx);
    return parse_fix_stmt(ctx, t.line, t.col);
}

static DslAST *parse_free_stmt_wrapper(ParserCtx *ctx) {
    DslToken t = parser_peek(ctx);
    parser_advance(ctx);
    return parse_free_stmt(ctx, t.line, t.col);
}

static DslAST *parse_load_stmt_wrapper(ParserCtx *ctx) {
    DslToken t = parser_peek(ctx);
    parser_advance(ctx);
    return parse_load_stmt(ctx, t.line, t.col);
}

static DslAST *parse_prove_stmt_wrapper(ParserCtx *ctx) {
    DslToken t = parser_peek(ctx);
    parser_advance(ctx);
    return parse_prove_stmt(ctx, t.line, t.col);
}

static DslAST *parse_constraint_stmt_wrapper(ParserCtx *ctx) {
    DslToken t = parser_peek(ctx);
    parser_advance(ctx);
    return parse_constraint_stmt(ctx, t.line, t.col);
}

static DslAST *parse_let_stmt_wrapper(ParserCtx *ctx) {
    DslToken t = parser_peek(ctx);
    parser_advance(ctx);
    return parse_let_stmt(ctx, t.line, t.col);
}

static DslAST *parse_block_wrapper(ParserCtx *ctx) {
    return parse_block(ctx);
}

static DslAST *parse_ident_stmt(ParserCtx *ctx) {
    DslToken t = parser_peek(ctx);
    parser_advance(ctx);
    DslAST *node = ast_alloc(DSL_AST_IDENT, t.line, t.col);
    if (node)
        node->name = lv_strdup(t.lexeme);
    return node;
}

static DslAST *parse_number_stmt(ParserCtx *ctx) {
    DslToken t = parser_peek(ctx);
    parser_advance(ctx);
    DslAST *node = ast_alloc(DSL_AST_NUMBER, t.line, t.col);
    if (node) {
        if (lv_parse_double(t.lexeme, &node->num_value) != 0)
            node->num_value = 0.0;
    }
    return node;
}

/** @brief 语句解析函数指针表：DSL_TOK_* → StmtParseFn */
static const StmtParseFn kParseStmtHandlers[] = {
    [DSL_TOK_POINT]       = parse_geom_decl_stmt,
    [DSL_TOK_LINE]        = parse_geom_decl_stmt,
    [DSL_TOK_CIRCLE]      = parse_geom_decl_stmt,
    [DSL_TOK_SEGMENT]     = parse_geom_decl_stmt,
    [DSL_TOK_RAY]         = parse_geom_decl_stmt,
    [DSL_TOK_POLYGON]     = parse_geom_decl_stmt,
    [DSL_TOK_TRIANGLE]    = parse_geom_decl_stmt,
    [DSL_TOK_INTERSECT]   = parse_construct_stmt_wrapper,
    [DSL_TOK_PARALLEL]    = parse_construct_stmt_wrapper,
    [DSL_TOK_PERPENDICULAR] = parse_construct_stmt_wrapper,
    [DSL_TOK_MIDPOINT]    = parse_construct_stmt_wrapper,
    [DSL_TOK_CIRCUMCENTER]= parse_construct_stmt_wrapper,
    [DSL_TOK_ORTHOCENTER] = parse_construct_stmt_wrapper,
    [DSL_TOK_CENTROID]    = parse_construct_stmt_wrapper,
    [DSL_TOK_INCENTER]    = parse_construct_stmt_wrapper,
    [DSL_TOK_BISECTOR]    = parse_construct_stmt_wrapper,
    [DSL_TOK_FIX]         = parse_fix_stmt_wrapper,
    [DSL_TOK_FREE]        = parse_free_stmt_wrapper,
    [DSL_TOK_LOAD]        = parse_load_stmt_wrapper,
    [DSL_TOK_PROVE]       = parse_prove_stmt_wrapper,
    [DSL_TOK_CONSTRAINT]  = parse_constraint_stmt_wrapper,
    [DSL_TOK_LET]         = parse_let_stmt_wrapper,
    [DSL_TOK_LBRACE]      = parse_block_wrapper,
    [DSL_TOK_IDENT]       = parse_ident_stmt,
    [DSL_TOK_NUMBER]      = parse_number_stmt,
};

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

    /* 使用策略模式查找表替换 switch 语句 */
    return LV_DISPATCH(kParseStmtHandlers, t.type, NULL, ctx);
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
        lv_free((void **) &root);
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
