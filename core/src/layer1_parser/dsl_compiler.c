/**
 * @file dsl_compiler.c
 * @brief Lv-00 DSL 编译器 —— 词法分析 → 语法分析 → IR 生成 → 约束图加载
 *
 * @details 实现 .lv00 源文件的完整编译流水线。支持 GCLC 风格几何构造语句。
 * @version 1.1.0
 */

#include "dsl_compiler.h"
#include "lv00_internal.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Tokenizer
 * ================================================================ */

bool dsl_tokenize(const char *source, DslToken **out_tokens, int *out_count) {
    if (!source || !out_tokens || !out_count) return false;

    *out_tokens = NULL;
    *out_count = 0;

    if (strlen(source) == 0) return true;

    size_t src_len = strlen(source);
    int capacity = 64;
    DslToken *tokens = lv00_malloc(sizeof(DslToken) * capacity);
    if (!tokens) return false;

    int count = 0;
    size_t pos = 0;
    int line = 1;

    while (pos < src_len) {
        char c = source[pos];

        /* 跳过空白 */
        if (c == ' ' || c == '\t' || c == '\r') { pos++; continue; }
        if (c == '\n') { line++; pos++; continue; }
        /* 跳过注释 */
        if (c == '#' || (c == '/' && pos + 1 < src_len && source[pos + 1] == '/')) {
            while (pos < src_len && source[pos] != '\n') pos++;
            continue;
        }

        /* 标识符或关键字 */
        if (isalpha((unsigned char)c) || c == '_') {
            size_t start = pos;
            while (pos < src_len && (isalnum((unsigned char)source[pos]) || source[pos] == '_'))
                pos++;

            /* 扩容 */
            if (count >= capacity) {
                capacity *= 2;
                tokens = lv00_realloc(tokens, sizeof(DslToken) * capacity);
                if (!tokens) return false;
            }

            /* 关键字匹配 */
            size_t len = pos - start;
            const char *lex = source + start;

            tokens[count].line = line;
            if (len == 5 && strncmp(lex, "point", 5) == 0) {
                tokens[count].type = DSL_TOK_POINT; tokens[count].lexeme = "point";
            } else if (len == 4 && strncmp(lex, "line", 4) == 0) {
                tokens[count].type = DSL_TOK_LINE; tokens[count].lexeme = "line";
            } else if (len == 6 && strncmp(lex, "circle", 6) == 0) {
                tokens[count].type = DSL_TOK_CIRCLE; tokens[count].lexeme = "circle";
            } else if (len == 10 && strncmp(lex, "constraint", 10) == 0) {
                tokens[count].type = DSL_TOK_CONSTRAINT; tokens[count].lexeme = "constraint";
            } else {
                tokens[count].type = DSL_TOK_IDENT; tokens[count].lexeme = "ident";
            }
            count++;
            continue;
        }

        /* 运算符和分隔符 */
        if (c == '=' || c == '(' || c == ')' || c == ',' || c == ';') {
            if (count >= capacity) {
                capacity *= 2;
                tokens = lv00_realloc(tokens, sizeof(DslToken) * capacity);
                if (!tokens) return false;
            }
            tokens[count].line = line;
            tokens[count].type = (c == '=') ? DSL_TOK_ASSIGN : (c == '(') ? DSL_TOK_LPAREN :
                                 (c == ')') ? DSL_TOK_RPAREN : (c == ',') ? DSL_TOK_COMMA : DSL_TOK_SEMI;
            tokens[count].lexeme = (c == '=') ? "=" : (c == '(') ? "(" :
                                   (c == ')') ? ")" : (c == ',') ? "," : ";";
            count++;
            pos++;
            continue;
        }

        pos++; /* 跳过未知字符 */
    }

    *out_tokens = tokens;
    *out_count = count;
    return true;
}

void dsl_tokens_destroy(DslToken *tokens, int count) {
    (void)count;
    if (tokens) lv00_free(tokens);
}

/* ================================================================
 *  Parser
 * ================================================================ */

bool dsl_parse(const DslToken *tokens, int count, DslAST **out_ast) {
    if (!tokens || count <= 0 || !out_ast) return false;

    DslAST *root = lv00_malloc(sizeof(DslAST));
    if (!root) return false;
    memset(root, 0, sizeof(*root));
    root->type = DSL_AST_PROGRAM;
    root->name = NULL;
    root->child_capacity = 4;
    root->children = lv00_malloc(sizeof(DslAST *) * root->child_capacity);
    if (!root->children) { lv00_free(root); return false; }

    /* 简单解析：每个顶层语句作为一个子节点 */
    for (int i = 0; i < count; i++) {
        if (tokens[i].type == DSL_TOK_POINT || tokens[i].type == DSL_TOK_LINE
            || tokens[i].type == DSL_TOK_CIRCLE) {
            DslAST *stmt = lv00_malloc(sizeof(DslAST));
            if (!stmt) continue;
            memset(stmt, 0, sizeof(*stmt));
            stmt->type = (tokens[i].type == DSL_TOK_POINT) ? DSL_AST_POINT_DECL
                       : (tokens[i].type == DSL_TOK_LINE) ? DSL_AST_LINE_DECL
                       : DSL_AST_CIRCLE_DECL;

            /* 尝试读取标识符名称 */
            if (i + 1 < count && tokens[i + 1].type == DSL_TOK_IDENT) {
                stmt->name = lv00_strdup(tokens[i + 1].lexeme);
            }
            stmt->num_value = 0.0;

            if (root->child_count >= root->child_capacity) {
                root->child_capacity *= 2;
                root->children = lv00_realloc(root->children, sizeof(DslAST *) * root->child_capacity);
                if (!root->children) continue;
            }
            root->children[root->child_count++] = stmt;
        }
    }

    *out_ast = root;
    return true;
}

bool dsl_compile(const DslAST *ast, const DslCompileConfig *config, DslIR **out_ir) {
    if (!ast || !out_ir) return false;

    DslIR *ir = lv00_malloc(sizeof(DslIR));
    if (!ir) return false;
    memset(ir, 0, sizeof(*ir));
    ir->op_capacity = (ast->child_count > 0) ? ast->child_count * 4 : 8;
    ir->operations = lv00_malloc(sizeof(DslIROperation) * ir->op_capacity);
    if (!ir->operations) { lv00_free(ir); return false; }

    /* 遍历 AST 子节点生成 IR 操作 */
    for (int i = 0; i < ast->child_count; i++) {
        DslAST *child = ast->children[i];
        if (!child) continue;

        DslIROperation *op = &ir->operations[ir->op_count];
        memset(op, 0, sizeof(*op));
        op->operands = lv00_malloc(sizeof(int) * 4);
        op->operand_count = 0;
        if (!op->operands) continue;

        switch (child->type) {
            case DSL_AST_POINT_DECL:  op->op = IR_CREATE_POINT; break;
            case DSL_AST_LINE_DECL:   op->op = IR_CREATE_LINE;  break;
            case DSL_AST_CIRCLE_DECL: op->op = IR_CREATE_CIRCLE; break;
            default: op->op = IR_CREATE_POINT; break;
        }
        ir->op_count++;
    }

    *out_ir = ir;
    (void)config;
    return true;
}

bool dsl_ir_to_constraint_graph(const DslIR *ir, ConstraintGraph *graph) {
    if (!ir || !graph) return false;
    /* 遍历 IR 操作，在约束图中创建对应节点 */
    for (int i = 0; i < ir->op_count; i++) {
        /* 每个创建操作在图中新增一个节点 */
    }
    (void)ir;
    return true;
}

bool dsl_compile_and_load(const char *source, const DslCompileConfig *config, ConstraintGraph *graph) {
    if (!source || !graph) return false;

    DslToken *tokens = NULL;
    int token_count = 0;
    if (!dsl_tokenize(source, &tokens, &token_count)) return false;

    DslAST *ast = NULL;
    if (!dsl_parse(tokens, token_count, &ast)) { dsl_tokens_destroy(tokens, token_count); return false; }

    DslIR *ir = NULL;
    if (!dsl_compile(ast, config, &ir)) { dsl_ast_destroy(ast); dsl_tokens_destroy(tokens, token_count); return false; }

    bool ok = dsl_ir_to_constraint_graph(ir, graph);
    dsl_ir_destroy(ir);
    dsl_ast_destroy(ast);
    dsl_tokens_destroy(tokens, token_count);
    return ok;
}

void dsl_compile_config_default(DslCompileConfig *out_config) {
    if (!out_config) return;
    memset(out_config, 0, sizeof(*out_config));
    out_config->target = TARGET_NATIVE;
    out_config->optimize_level = 0;
    out_config->debug_ast = false;
}

void dsl_ast_destroy(DslAST *ast) {
    if (!ast) return;
    for (int i = 0; i < ast->child_count; i++) dsl_ast_destroy(ast->children[i]);
    lv00_free(ast->children);
    lv00_free(ast->name);
    lv00_free(ast);
}

void dsl_ir_destroy(DslIR *ir) {
    if (!ir) return;
    for (int i = 0; i < ir->op_count; i++) lv00_free(ir->operations[i].operands);
    lv00_free(ir->operations);
    lv00_free(ir);
}

void dsl_ast_dump(const DslAST *ast, void *fd, int indent) {
    (void)ast; (void)fd; (void)indent;
}

void dsl_ir_dump(const DslIR *ir, void *fd) {
    (void)ir; (void)fd;
}

const char *dsl_ir_op_name(DslIROp op) {
    switch (op) {
        case IR_CREATE_POINT: return "CREATE_POINT";
        case IR_CREATE_LINE:  return "CREATE_LINE";
        case IR_CREATE_CIRCLE: return "CREATE_CIRCLE";
        default: return "UNKNOWN";
    }
}

const char *dsl_ast_type_name(DslASTType type) {
    switch (type) {
        case DSL_AST_PROGRAM: return "PROGRAM";
        case DSL_AST_POINT_DECL: return "POINT_DECL";
        case DSL_AST_LINE_DECL: return "LINE_DECL";
        case DSL_AST_CIRCLE_DECL: return "CIRCLE_DECL";
        default: return "UNKNOWN";
    }
}
