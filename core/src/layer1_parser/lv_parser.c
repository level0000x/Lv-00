#include "lv/lv_parser.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"

/* 函数/关系/度量/几何调用参数个数上限 */
#define LV_MAX_CALL_ARGS 32

/* ── Parser 结构 ── */
struct LvParser {
    LvLexer *lexer;
    LvToken current;
    int error_count;
    LvParseError errors[64];
};

/* ── 辅助函数 ── */

/** 获取 token 的文本到线程局部 scratch 缓冲区 */
static const char *token_text(const LvToken *tok) {
    char *buf = lv_scratch_buf(128);
    lv_token_text(tok, buf, 128);
    return buf;
}

/** 消费当前 token，前进到下一个 */
static void advance(LvParser *p) {
    p->current = lv_lexer_next(p->lexer);
}

/** 检查当前 token 是否匹配指定类型，匹配则消费 */
static int match(LvParser *p, LvTokenType type) {
    if (p->current.type == type) {
        advance(p);
        return 1;
    }
    return 0;
}

/** 期望当前 token 为指定类型，否则报错 */
static int expect(LvParser *p, LvTokenType type, const char *msg) {
    if (p->current.type == type) {
        advance(p);
        return 1;
    }
    if (p->error_count < 64) {
        int idx = p->error_count++;
        p->errors[idx].loc = p->current.loc;
        lv_snprintf(p->errors[idx].message, sizeof(p->errors[idx].message), "expected %s but got %s: %s",
                    lv_token_type_name(type), lv_token_type_name(p->current.type), msg);
    }
    return 0;
}

/** 检查后面第 lookahead 个 token 是否是指定类型 */
static int check(LvParser *p, int lookahead, LvTokenType type) {
    LvToken t = lv_lexer_peek(p->lexer, lookahead);
    return t.type == type;
}

/** 检查后面第 lookahead 个 token 是否是 identifier 且文本匹配 */
static int check_ident(LvParser *p, int lookahead, const char *text) {
    LvToken t = lv_lexer_peek(p->lexer, lookahead);
    if (t.type != LV_TOKEN_IDENTIFIER)
        return 0;
    size_t len = strlen(text);
    if (t.length != len)
        return 0;
    return strncmp(t.start, text, len) == 0;
}

/** 记录解析错误 */
static void parser_error(LvParser *p, const LvToken *tok, const char *msg) {
    if (p->error_count < 64) {
        int idx = p->error_count++;
        p->errors[idx].loc = tok->loc;
        lv_strncpy(p->errors[idx].message, msg, sizeof(p->errors[idx].message));
    }
}

/** 语句起始关键字查找表前向声明（定义见后文查找表区） */
static const int s_statement_start_tokens[LV_TOKEN_COUNT];

/** 跳过 token 直到遇到分号或语句关键字（错误恢复） */
static void synchronize(LvParser *p) {
    while (p->current.type != LV_TOKEN_EOF) {
        if (p->current.type == LV_TOKEN_SEMICOLON) {
            advance(p);
            return;
        }
        /* 语句起始关键字：查表判定 */
        if (p->current.type >= 0 && p->current.type < LV_TOKEN_COUNT &&
            s_statement_start_tokens[p->current.type]) {
            return;
        }
        advance(p);
    }
}

/* ── 前向声明 ── */
static LvAstNode *parse_statement(LvParser *p);
static LvAstNode *parse_logic_expr(LvParser *p);
static LvAstNode *parse_iff_expr(LvParser *p);
static LvAstNode *parse_implies_expr(LvParser *p);
static LvAstNode *parse_or_expr(LvParser *p);
static LvAstNode *parse_and_expr(LvParser *p);
static LvAstNode *parse_not_expr(LvParser *p);
static LvAstNode *parse_quantified_expr(LvParser *p);
static LvAstNode *parse_predicate_expr(LvParser *p);
static LvAstNode *parse_compare_expr(LvParser *p);
static LvAstNode *parse_add_expr(LvParser *p);
static LvAstNode *parse_mul_expr(LvParser *p);
static LvAstNode *parse_unary_expr(LvParser *p);
static LvAstNode *parse_primary_expr(LvParser *p);

/* ── 查找表 ── */

/** keyword → statement-starting flag 查找表（用于 synchronize 错误恢复） */
static const int s_statement_start_tokens[LV_TOKEN_COUNT] = {
    [LV_TOKEN_KW_POINT] = 1,
    [LV_TOKEN_KW_LINE] = 1,
    [LV_TOKEN_KW_CIRCLE] = 1,
    [LV_TOKEN_KW_SEGMENT] = 1,
    [LV_TOKEN_KW_RAY] = 1,
    [LV_TOKEN_KW_ANGLE] = 1,
    [LV_TOKEN_KW_TRIANGLE] = 1,
    [LV_TOKEN_KW_POLYGON] = 1,
    [LV_TOKEN_KW_SCALAR] = 1,
    [LV_TOKEN_KW_BOOL] = 1,
    [LV_TOKEN_KW_PROPOSITION] = 1,
    [LV_TOKEN_KW_PROOF] = 1,
    [LV_TOKEN_KW_CONSTRAINT] = 1,
    [LV_TOKEN_KW_ASSUME] = 1,
    [LV_TOKEN_KW_ASSERT] = 1,
    [LV_TOKEN_KW_PROVE] = 1,
    [LV_TOKEN_KW_LET] = 1,
    [LV_TOKEN_KW_COMPUTE] = 1,
    [LV_TOKEN_KW_NORMALIZE] = 1,
    [LV_TOKEN_KW_EXPORT] = 1,
    [LV_TOKEN_KW_AXIOM] = 1,
    [LV_TOKEN_KW_THEOREM] = 1,
    [LV_TOKEN_KW_MODULE] = 1,
    [LV_TOKEN_KW_IMPORT] = 1,
};

/** token → entity type flag 查找表 */
static const int s_is_entity_type_tokens[LV_TOKEN_COUNT] = {
    [LV_TOKEN_KW_POINT] = 1,
    [LV_TOKEN_KW_LINE] = 1,
    [LV_TOKEN_KW_CIRCLE] = 1,
    [LV_TOKEN_KW_SEGMENT] = 1,
    [LV_TOKEN_KW_RAY] = 1,
    [LV_TOKEN_KW_ANGLE] = 1,
    [LV_TOKEN_KW_TRIANGLE] = 1,
    [LV_TOKEN_KW_POLYGON] = 1,
    [LV_TOKEN_KW_SCALAR] = 1,
    [LV_TOKEN_KW_BOOL] = 1,
    [LV_TOKEN_KW_PROPOSITION] = 1,
    [LV_TOKEN_KW_PROOF] = 1,
};

/** token → 比较运算符字符串 查找表 */
static const char *const s_compare_op_strings[LV_TOKEN_COUNT] = {
    [LV_TOKEN_EQEQ] = "==",
    [LV_TOKEN_NEQ] = "!=",
    [LV_TOKEN_LT] = "<",
    [LV_TOKEN_LE] = "<=",
    [LV_TOKEN_GT] = ">",
    [LV_TOKEN_GE] = ">=",
};

/** token → 关键字名称 查找表 */
static const char *const s_keyword_name_strings[LV_TOKEN_COUNT] = {
    [LV_TOKEN_KW_PARALLEL] = "parallel",
    [LV_TOKEN_KW_PERPENDICULAR] = "perpendicular",
    [LV_TOKEN_KW_CONGRUENT] = "congruent",
    [LV_TOKEN_KW_TANGENT] = "tangent",
};


/* ================================================================
 * 语句级解析
 * ================================================================ */

/** EntityType ::= "Point" | "Line" | ... | "Proof" */
static int is_entity_type(LvTokenType t) {
    return t >= 0 && t < LV_TOKEN_COUNT && s_is_entity_type_tokens[t];
}

/** DeclarationStmt ::= EntityType IdentifierList ";" */
static LvAstNode *parse_declaration_stmt(LvParser *p) {
    LvSourceLoc loc = p->current.loc;
    LvEntityType entity = lv_entity_type_from_token(p->current.type);
    advance(p); /* 消费 EntityType */

    /* IdentifierList ::= Identifier ("," Identifier)* */
    /* 收集标识符 */
    char names_buf[1024] = {0};
    size_t pos = 0;

    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        const char *name = token_text(&p->current);
        size_t nlen = strlen(name);
        if (pos + nlen < sizeof(names_buf)) {
            memcpy(names_buf + pos, name, nlen);
            pos += nlen;
        }
        advance(p);
    } else {
        expect(p, LV_TOKEN_IDENTIFIER, "expected identifier in declaration");
    }

    while (match(p, LV_TOKEN_COMMA)) {
        if (pos < sizeof(names_buf))
            names_buf[pos++] = ',';
        if (p->current.type == LV_TOKEN_IDENTIFIER) {
            const char *name = token_text(&p->current);
            size_t nlen = strlen(name);
            if (pos + nlen < sizeof(names_buf)) {
                memcpy(names_buf + pos, name, nlen);
                pos += nlen;
            }
            advance(p);
        } else {
            expect(p, LV_TOKEN_IDENTIFIER, "expected identifier after ','");
            break;
        }
    }
    names_buf[pos] = '\0';

    if (!match(p, LV_TOKEN_SEMICOLON)) {
        expect(p, LV_TOKEN_SEMICOLON, "expected ';' after declaration");
        synchronize(p);
    }

    LvAstNode *node = lv_ast_create(LV_AST_DECLARATION, loc);
    if (node) {
        node->data.decl.entity_type = (int) entity;
        node->data.decl.names = lv_strdup(names_buf);
    }
    return node;
}

/** LetStmt ::= "Let" Identifier ":" Type "=" Expr ";" */
static LvAstNode *parse_let_stmt(LvParser *p) {
    LvSourceLoc loc = p->current.loc;
    advance(p); /* Let */

    char name[128] = {0};
    char type_name[128] = {0};

    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        lv_strncpy(name, token_text(&p->current), sizeof(name));
        advance(p);
    } else {
        expect(p, LV_TOKEN_IDENTIFIER, "expected identifier after Let");
    }

    expect(p, LV_TOKEN_COLON, "expected ':' after identifier in Let");

    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        lv_strncpy(type_name, token_text(&p->current), sizeof(type_name));
        advance(p);
    } else {
        /* 也可能是类型关键字 */
        if (is_entity_type(p->current.type)) {
            lv_strncpy(type_name, token_text(&p->current), sizeof(type_name));
            advance(p);
        } else {
            expect(p, LV_TOKEN_IDENTIFIER, "expected type in Let");
        }
    }

    expect(p, LV_TOKEN_EQUALS, "expected '=' in Let definition");

    LvAstNode *value = parse_logic_expr(p);

    if (!match(p, LV_TOKEN_SEMICOLON)) {
        expect(p, LV_TOKEN_SEMICOLON, "expected ';' after Let");
        synchronize(p);
    }

    LvAstNode *node = lv_ast_create(LV_AST_LET, loc);
    if (node) {
        node->data.let_def.name = lv_strdup(name);
        node->data.let_def.type_name = lv_strdup(type_name);
        node->data.let_def.value = value;
    }
    return node;
}

/** 单表达式语句共同实现：ConstraintStmt / ProveStmt / AssumeStmt / AssertStmt / ComputeStmt */
static LvAstNode *parse_simple_stmt(LvParser *p, LvTokenType kw, LvAstNodeType node_type) {
    LvSourceLoc loc = p->current.loc;
    advance(p); /* 关键字 */
    LvAstNode *expr = parse_logic_expr(p);
    if (!match(p, LV_TOKEN_SEMICOLON)) {
        expect(p, LV_TOKEN_SEMICOLON, "expected ';' after statement");
        synchronize(p);
    }
    LvAstNode *node = lv_ast_create(node_type, loc);
    if (node)
        node->data.stmt.expr = expr;
    return node;
}

/** ConstraintStmt ::= "Constraint" LogicExpr ";" */
static LvAstNode *parse_constraint_stmt(LvParser *p) {
    return parse_simple_stmt(p, LV_TOKEN_KW_CONSTRAINT, LV_AST_CONSTRAINT_STMT);
}

/** ProveStmt ::= "Prove" LogicExpr ";" */
static LvAstNode *parse_prove_stmt(LvParser *p) {
    return parse_simple_stmt(p, LV_TOKEN_KW_PROVE, LV_AST_PROVE_STMT);
}

/** AssumeStmt ::= "Assume" LogicExpr ";" */
static LvAstNode *parse_assume_stmt(LvParser *p) {
    return parse_simple_stmt(p, LV_TOKEN_KW_ASSUME, LV_AST_ASSUME_STMT);
}

/** AssertStmt ::= "Assert" LogicExpr ";" */
static LvAstNode *parse_assert_stmt(LvParser *p) {
    return parse_simple_stmt(p, LV_TOKEN_KW_ASSERT, LV_AST_ASSERT_STMT);
}

/** AxiomStmt ::= "Axiom" Identifier ":" LogicExpr ";" */
static LvAstNode *parse_axiom_stmt(LvParser *p) {
    LvSourceLoc loc = p->current.loc;
    advance(p); /* Axiom */

    char name[128] = {0};
    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        lv_strncpy(name, token_text(&p->current), sizeof(name));
        advance(p);
    } else {
        expect(p, LV_TOKEN_IDENTIFIER, "expected identifier after Axiom");
    }

    expect(p, LV_TOKEN_COLON, "expected ':' in Axiom");

    LvAstNode *expr = parse_logic_expr(p);

    if (!match(p, LV_TOKEN_SEMICOLON)) {
        expect(p, LV_TOKEN_SEMICOLON, "expected ';' after Axiom");
        synchronize(p);
    }

    LvAstNode *node = lv_ast_create(LV_AST_AXIOM_STMT, loc);
    if (node) {
        LvAstNode *ident = lv_ast_create_ident(loc, name);
        node->data.stmt.expr = expr;
        if (ident) {
            /* 把 identifier 作为第一个子节点；expr 作为第二个 */
            node->child = ident;
            node->child_count = 1;
            if (expr) {
                ident->next = expr;
                node->child_count = 2;
            }
        }
    }
    return node;
}

/** TheoremStmt ::= "Theorem" Identifier ":" LogicExpr ProofBlock? ";" */
static LvAstNode *parse_theorem_stmt(LvParser *p) {
    LvSourceLoc loc = p->current.loc;
    advance(p); /* Theorem */

    char name[128] = {0};
    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        lv_strncpy(name, token_text(&p->current), sizeof(name));
        advance(p);
    } else {
        expect(p, LV_TOKEN_IDENTIFIER, "expected identifier after Theorem");
    }

    expect(p, LV_TOKEN_COLON, "expected ':' in Theorem");

    LvAstNode *proposition = parse_logic_expr(p);

    /* ProofBlock? - optional */
    LvAstNode *proof_block = NULL;
    if (p->current.type == LV_TOKEN_LBRACE) {
        LvSourceLoc blk_loc = p->current.loc;
        advance(p); /* { */

        /* 解析 proof block 中的语句列表 */
        LvAstNode *first_stmt = NULL;
        LvAstNode *last_stmt = NULL;
        int stmt_count = 0;

        while (p->current.type != LV_TOKEN_RBRACE && p->current.type != LV_TOKEN_EOF) {
            LvAstNode *stmt = parse_statement(p);
            if (stmt) {
                if (!first_stmt) {
                    first_stmt = stmt;
                } else {
                    last_stmt->next = stmt;
                }
                last_stmt = stmt;
                stmt_count++;
            }
        }

        expect(p, LV_TOKEN_RBRACE, "expected '}' to close proof block");

        proof_block = lv_ast_create(LV_AST_PROOF_BLOCK, blk_loc);
        if (proof_block) {
            proof_block->child = first_stmt;
            proof_block->child_count = stmt_count;
        }
    }

    if (!match(p, LV_TOKEN_SEMICOLON)) {
        expect(p, LV_TOKEN_SEMICOLON, "expected ';' after Theorem");
        synchronize(p);
    }

    LvAstNode *node = lv_ast_create(LV_AST_THEOREM_STMT, loc);
    if (node) {
        node->data.theorem.name = lv_strdup(name);
        node->data.theorem.proposition = proposition;
        node->data.theorem.proof_block = proof_block;
    }
    return node;
}

/** NormalizeStmt ::= "Normalize" (Identifier | "all") ";" */
static LvAstNode *parse_normalize_stmt(LvParser *p) {
    LvSourceLoc loc = p->current.loc;
    advance(p); /* Normalize */

    char target[128] = {0};
    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        lv_strncpy(target, token_text(&p->current), sizeof(target));
        advance(p);
    } else {
        /* "all" might be an identifier */
        const char *txt = token_text(&p->current);
        lv_strncpy(target, txt, sizeof(target));
        advance(p);
    }

    if (!match(p, LV_TOKEN_SEMICOLON)) {
        expect(p, LV_TOKEN_SEMICOLON, "expected ';' after Normalize");
        synchronize(p);
    }

    LvAstNode *node = lv_ast_create(LV_AST_NORMALIZE_STMT, loc);
    if (node) {
        node->data.normalize.target = lv_strdup(target);
    }
    return node;
}

/** ComputeStmt ::= "Compute" Expr ";" */
static LvAstNode *parse_compute_stmt(LvParser *p) {
    return parse_simple_stmt(p, LV_TOKEN_KW_COMPUTE, LV_AST_COMPUTE_STMT);
}

/** ExportStmt ::= "Export" ExportTarget "as" ExportFormat String? ";" */
static LvAstNode *parse_export_stmt(LvParser *p) {
    LvSourceLoc loc = p->current.loc;
    advance(p); /* Export */

    char target[128] = {0};
    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        lv_strncpy(target, token_text(&p->current), sizeof(target));
        advance(p);
    } else {
        expect(p, LV_TOKEN_IDENTIFIER, "expected export target");
    }

    /* "as" is a regular identifier, not a keyword */
    if (p->current.type == LV_TOKEN_IDENTIFIER && strcmp(token_text(&p->current), "as") == 0) {
        advance(p); /* consume "as" */
    }

    char format[128] = {0};
    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        lv_strncpy(format, token_text(&p->current), sizeof(format));
        advance(p);
    } else {
        expect(p, LV_TOKEN_IDENTIFIER, "expected export format");
    }

    char path[256] = {0};
    if (p->current.type == LV_TOKEN_STRING) {
        const char *txt = token_text(&p->current);
        /* strip quotes */
        size_t len = strlen(txt);
        if (len >= 2) {
            size_t plen = len - 2;
            if (plen >= sizeof(path))
                plen = sizeof(path) - 1;
            memcpy(path, txt + 1, plen);
            path[plen] = '\0';
        }
        advance(p);
    }

    if (!match(p, LV_TOKEN_SEMICOLON)) {
        expect(p, LV_TOKEN_SEMICOLON, "expected ';' after Export");
        synchronize(p);
    }

    LvAstNode *node = lv_ast_create(LV_AST_EXPORT_STMT, loc);
    if (node) {
        node->data.export_stmt.target = lv_strdup(target);
        node->data.export_stmt.format = lv_strdup(format);
        node->data.export_stmt.path = path[0] ? lv_strdup(path) : NULL;
    }
    return node;
}

/** ModuleDecl ::= "module" QualifiedName ";" */
static LvAstNode *parse_module_decl(LvParser *p) {
    LvSourceLoc loc = p->current.loc;
    advance(p); /* module */

    char qname[256] = {0};
    size_t pos = 0;
    while (p->current.type == LV_TOKEN_IDENTIFIER) {
        const char *txt = token_text(&p->current);
        size_t tlen = strlen(txt);
        if (pos + tlen + 1 < sizeof(qname)) {
            if (pos > 0)
                qname[pos++] = '.';
            memcpy(qname + pos, txt, tlen);
            pos += tlen;
        }
        advance(p);
        if (p->current.type == LV_TOKEN_DOT) {
            advance(p);
        } else {
            break;
        }
    }
    qname[pos] = '\0';

    if (!match(p, LV_TOKEN_SEMICOLON)) {
        expect(p, LV_TOKEN_SEMICOLON, "expected ';' after module");
        synchronize(p);
    }

    LvAstNode *node = lv_ast_create(LV_AST_MODULE_DECL, loc);
    if (node)
        node->data.module_import.qualified_name = lv_strdup(qname);
    return node;
}

/** ImportDecl ::= "import" QualifiedName ("as" Identifier)? ";" */
static LvAstNode *parse_import_decl(LvParser *p) {
    LvSourceLoc loc = p->current.loc;
    advance(p); /* import */

    char qname[256] = {0};
    size_t pos = 0;
    while (p->current.type == LV_TOKEN_IDENTIFIER) {
        const char *txt = token_text(&p->current);
        size_t tlen = strlen(txt);
        if (pos + tlen + 1 < sizeof(qname)) {
            if (pos > 0)
                qname[pos++] = '.';
            memcpy(qname + pos, txt, tlen);
            pos += tlen;
        }
        advance(p);
        if (p->current.type == LV_TOKEN_DOT) {
            advance(p);
        } else {
            break;
        }
    }
    qname[pos] = '\0';

    /* optional "as" alias */
    /* "as" is not a keyword, so check for identifier "as" */
    if (p->current.type == LV_TOKEN_IDENTIFIER && strcmp(token_text(&p->current), "as") == 0) {
        advance(p); /* consume "as" */
        if (p->current.type == LV_TOKEN_IDENTIFIER) {
            advance(p); /* consume alias */
        }
    }

    if (!match(p, LV_TOKEN_SEMICOLON)) {
        expect(p, LV_TOKEN_SEMICOLON, "expected ';' after import");
        synchronize(p);
    }

    LvAstNode *node = lv_ast_create(LV_AST_IMPORT_DECL, loc);
    if (node)
        node->data.module_import.qualified_name = lv_strdup(qname);
    return node;
}

/** Statement parse handler 函数指针类型 */
typedef LvAstNode *(*StatementParseHandler)(LvParser *p);

/** 语句解析 VTable 查找表 */
static const StatementParseHandler kStatementParseHandlers[LV_TOKEN_COUNT] = {
    [LV_TOKEN_KW_CONSTRAINT] = parse_constraint_stmt,
    [LV_TOKEN_KW_PROVE]      = parse_prove_stmt,
    [LV_TOKEN_KW_ASSUME]     = parse_assume_stmt,
    [LV_TOKEN_KW_ASSERT]     = parse_assert_stmt,
    [LV_TOKEN_KW_LET]        = parse_let_stmt,
    [LV_TOKEN_KW_COMPUTE]    = parse_compute_stmt,
    [LV_TOKEN_KW_NORMALIZE]  = parse_normalize_stmt,
    [LV_TOKEN_KW_EXPORT]     = parse_export_stmt,
    [LV_TOKEN_KW_AXIOM]      = parse_axiom_stmt,
    [LV_TOKEN_KW_THEOREM]    = parse_theorem_stmt,
    [LV_TOKEN_KW_MODULE]     = parse_module_decl,
    [LV_TOKEN_KW_IMPORT]     = parse_import_decl,
};

/** Statement ::= DeclarationStmt | ConstraintStmt | ... */
static LvAstNode *parse_statement(LvParser *p) {
    if (is_entity_type(p->current.type)) {
        return parse_declaration_stmt(p);
    }

    if (p->current.type >= 0 && p->current.type < LV_TOKEN_COUNT) {
        StatementParseHandler handler = kStatementParseHandlers[p->current.type];
        if (handler)
            return handler(p);
    }
    return NULL;
}

/* ================================================================
 * 表达式解析（递归下降）
 * ================================================================
 *
 * 优先级（从低到高）：
 *   iff  <->          (最低)
 *   implies ->
 *   or    \/
 *   and   /\
 *   not
 *   forall / exists
 *   == != < <= > >=
 *   + -
 *   * /
 *   unary + -
 *   primary (literals, idents, calls, (), measure, geometry)
 */

/** LogicExpr ::= IffExpr */
static LvAstNode *parse_logic_expr(LvParser *p) {
    return parse_iff_expr(p);
}

/** IffExpr ::= ImpliesExpr (("iff" | "<->") ImpliesExpr)* */
static LvAstNode *parse_iff_expr(LvParser *p) {
    LvAstNode *left = parse_implies_expr(p);
    if (!left)
        return NULL;

    while (1) {
        int is_iff = 0;
        /* Check for "iff" identifier */
        if (p->current.type == LV_TOKEN_IDENTIFIER && strcmp(token_text(&p->current), "iff") == 0) {
            is_iff = 1;
            /* consume "iff" after all checks */
        }
        /* Check for "<->" - three tokens: LT, MINUS, GT */
        if (!is_iff && p->current.type == LV_TOKEN_LT && check(p, 0, LV_TOKEN_MINUS) && check(p, 1, LV_TOKEN_GT)) {
            is_iff = 1;
            advance(p); /* consume < */
            advance(p); /* consume - */
            advance(p); /* consume > */
        }

        if (!is_iff)
            break;

        if (is_iff && p->current.type == LV_TOKEN_IDENTIFIER) {
            advance(p); /* consume "iff" */
        }

        LvAstNode *right = parse_implies_expr(p);
        if (!right)
            break;

        left = lv_ast_create_logic_binary(LV_AST_LOGIC_IFF, left->loc, "iff", left, right);
    }

    return left;
}

/** ImpliesExpr ::= OrExpr (("implies" | "->") OrExpr)* */
static LvAstNode *parse_implies_expr(LvParser *p) {
    LvAstNode *left = parse_or_expr(p);
    if (!left)
        return NULL;

    while (1) {
        int is_implies = 0;
        if (p->current.type == LV_TOKEN_IDENTIFIER && strcmp(token_text(&p->current), "implies") == 0) {
            is_implies = 1;
        }
        if (p->current.type == LV_TOKEN_ARROW) {
            is_implies = 1;
        }

        if (!is_implies)
            break;

        if (p->current.type == LV_TOKEN_IDENTIFIER) {
            advance(p); /* consume "implies" */
        } else {
            advance(p); /* consume -> */
        }

        LvAstNode *right = parse_or_expr(p);
        if (!right)
            break;

        left = lv_ast_create_logic_binary(LV_AST_LOGIC_IMPLIES, left->loc, "->", left, right);
    }

    return left;
}

/** OrExpr ::= AndExpr (("or" | "\/") AndExpr)* */
static LvAstNode *parse_or_expr(LvParser *p) {
    LvAstNode *left = parse_and_expr(p);
    if (!left)
        return NULL;

    while (1) {
        int is_or = 0;
        if (p->current.type == LV_TOKEN_KW_OR) {
            is_or = 1;
        }
        /* "\/" is not a single token, would be SLASH, CARET... actually not used in practice */
        if (!is_or)
            break;

        advance(p); /* consume "or" */

        LvAstNode *right = parse_and_expr(p);
        if (!right)
            break;

        left = lv_ast_create_logic_binary(LV_AST_LOGIC_OR, left->loc, "or", left, right);
    }

    return left;
}

/** AndExpr ::= NotExpr (("and" | "/\") NotExpr)* */
static LvAstNode *parse_and_expr(LvParser *p) {
    LvAstNode *left = parse_not_expr(p);
    if (!left)
        return NULL;

    while (1) {
        int is_and = 0;
        if (p->current.type == LV_TOKEN_KW_AND) {
            is_and = 1;
        }
        if (!is_and)
            break;

        advance(p); /* consume "and" */

        LvAstNode *right = parse_not_expr(p);
        if (!right)
            break;

        left = lv_ast_create_logic_binary(LV_AST_LOGIC_AND, left->loc, "and", left, right);
    }

    return left;
}

/** NotExpr ::= ("not") NotExpr | QuantifiedExpr */
static LvAstNode *parse_not_expr(LvParser *p) {
    if (p->current.type == LV_TOKEN_KW_NOT) {
        LvSourceLoc loc = p->current.loc;
        advance(p); /* not */
        LvAstNode *operand = parse_not_expr(p);
        LvAstNode *node = lv_ast_create(LV_AST_LOGIC_NOT, loc);
        if (node) {
            node->data.unary.operand = operand;
            lv_strncpy(node->data.unary.op, "not", sizeof(node->data.unary.op));
        }
        return node;
    }
    return parse_quantified_expr(p);
}

/** QuantifiedExpr ::= ("forall" | "exists") BinderList "." LogicExpr | PredicateExpr */
static LvAstNode *parse_quantified_expr(LvParser *p) {
    if (p->current.type == LV_TOKEN_KW_FORALL || p->current.type == LV_TOKEN_KW_EXISTS) {
        int is_forall = (p->current.type == LV_TOKEN_KW_FORALL);
        LvSourceLoc loc = p->current.loc;
        advance(p);

        /* BinderList ::= Binder ("," Binder)* */
        /* Binder ::= Identifier ":" Type */
        LvAstNode *first_binder = NULL;
        LvAstNode *last_binder = NULL;
        int binder_count = 0;

        while (1) {
            char var_name[128] = {0};
            char var_type[128] = {0};

            if (p->current.type == LV_TOKEN_IDENTIFIER) {
                lv_strncpy(var_name, token_text(&p->current), sizeof(var_name));
                advance(p);
            } else {
                expect(p, LV_TOKEN_IDENTIFIER, "expected variable name in quantifier binder");
                break;
            }

            expect(p, LV_TOKEN_COLON, "expected ':' in quantifier binder");

            if (p->current.type == LV_TOKEN_IDENTIFIER) {
                lv_strncpy(var_type, token_text(&p->current), sizeof(var_type));
                advance(p);
            } else if (is_entity_type(p->current.type)) {
                lv_strncpy(var_type, token_text(&p->current), sizeof(var_type));
                advance(p);
            } else {
                expect(p, LV_TOKEN_IDENTIFIER, "expected type in quantifier binder");
                break;
            }

            /* 创建 quantifier 节点表示这个 binder */
            LvAstNode *bnode = lv_ast_create(is_forall ? LV_AST_LOGIC_FORALL : LV_AST_LOGIC_EXISTS, loc);
            if (bnode) {
                bnode->data.quantifier.var_name = lv_strdup(var_name);
                bnode->data.quantifier.var_type = lv_strdup(var_type);
                bnode->data.quantifier.body = NULL;
            }

            if (!first_binder) {
                first_binder = bnode;
            } else {
                last_binder->next = bnode;
            }
            last_binder = bnode;
            binder_count++;

            if (p->current.type == LV_TOKEN_COMMA) {
                advance(p);
            } else {
                break;
            }
        }

        expect(p, LV_TOKEN_DOT, "expected '.' after quantifier binders");

        LvAstNode *body = parse_logic_expr(p);

        /* 如果有多个 binder，嵌套它们 */
        if (binder_count > 0) {
            /* 找到最后一个 binder */
            LvAstNode *inner = first_binder;
            while (inner->next)
                inner = inner->next;
            inner->data.quantifier.body = body;

            /* 如果有多个 binder，嵌套它们：forall x: T. forall y: U. body */
            if (binder_count > 1) {
                /* 嵌套：forall x: T. forall y: U. body */
                LvAstNode *prev = NULL;
                LvAstNode *cur = first_binder;
                while (cur->next) {
                    prev = cur;
                    cur = cur->next;
                }
                cur->data.quantifier.body = body;
                /* cur 现在是最后一个 binder，把前面的链到它 */
                if (prev) {
                    /* 重新链接：把每个 binder 的 body 指向下一个 */
                    LvAstNode *b = first_binder;
                    LvAstNode *next_b = b->next;
                    while (next_b) {
                        b->data.quantifier.body = next_b;
                        b->next = NULL;
                        b = next_b;
                        next_b = next_b->next;
                    }
                }
                return first_binder;
            } else {
                first_binder->data.quantifier.body = body;
                return first_binder;
            }
        }

        return NULL;
    }
    return parse_predicate_expr(p);
}

/** PredicateExpr ::= RelationExpr | CompareExpr | "true" | "false" | "bottom" */
static LvAstNode *parse_predicate_expr(LvParser *p) {
    /* Handle true / false / bottom as boolean literals */
    if (p->current.type == LV_TOKEN_KW_TRUE) {
        LvAstNode *node = lv_ast_create_bool(p->current.loc, 1);
        advance(p);
        return node;
    }
    if (p->current.type == LV_TOKEN_KW_FALSE) {
        LvAstNode *node = lv_ast_create_bool(p->current.loc, 0);
        advance(p);
        return node;
    }
    if (p->current.type == LV_TOKEN_KW_BOTTOM) {
        LvAstNode *node = lv_ast_create_bool(p->current.loc, 0);
        advance(p);
        return node;
    }

    return parse_compare_expr(p);
}

/** CompareExpr ::= AddExpr (("==" | "!=" | "<" | "<=" | ">" | ">=") AddExpr)? */
static const char *kCompareOpNames[] = {
    [LV_TOKEN_EQEQ] = "==",
    [LV_TOKEN_NEQ]  = "!=",
    [LV_TOKEN_LT]   = "<",
    [LV_TOKEN_LE]   = "<=",
    [LV_TOKEN_GT]   = ">",
    [LV_TOKEN_GE]   = ">=",
};

static LvAstNode *parse_compare_expr(LvParser *p) {
    LvAstNode *left = parse_add_expr(p);
    if (!left)
        return NULL;

    LvSourceLoc op_loc = p->current.loc;

    const char *op = NULL;
    if (p->current.type >= 0 && p->current.type < (int)(sizeof(kCompareOpNames)/sizeof(kCompareOpNames[0])))
        op = kCompareOpNames[p->current.type];
    if (!op) return left;

    advance(p); /* consume operator */

    LvAstNode *right = parse_add_expr(p);
    if (!right)
        return left;

    return lv_ast_create_compare(op_loc, op, left, right);
}

/** AddExpr ::= MulExpr (("+" | "-") MulExpr)* */
static LvAstNode *parse_add_expr(LvParser *p) {
    LvAstNode *left = parse_mul_expr(p);
    if (!left)
        return NULL;

    while (p->current.type == LV_TOKEN_PLUS || p->current.type == LV_TOKEN_MINUS) {
        const char *op = (p->current.type == LV_TOKEN_PLUS) ? "+" : "-";
        LvSourceLoc loc = p->current.loc;
        advance(p);
        LvAstNode *right = parse_mul_expr(p);
        if (!right)
            break;
        left = lv_ast_create_binary(loc, op, left, right);
    }

    return left;
}

/** MulExpr ::= UnaryExpr (("*" | "/") UnaryExpr)* */
static LvAstNode *parse_mul_expr(LvParser *p) {
    LvAstNode *left = parse_unary_expr(p);
    if (!left)
        return NULL;

    while (p->current.type == LV_TOKEN_STAR || p->current.type == LV_TOKEN_SLASH) {
        const char *op = (p->current.type == LV_TOKEN_STAR) ? "*" : "/";
        LvSourceLoc loc = p->current.loc;
        advance(p);
        LvAstNode *right = parse_unary_expr(p);
        if (!right)
            break;
        left = lv_ast_create_binary(loc, op, left, right);
    }

    return left;
}

/** UnaryExpr ::= ("+" | "-") UnaryExpr | PrimaryExpr */
static LvAstNode *parse_unary_expr(LvParser *p) {
    if (p->current.type == LV_TOKEN_PLUS || p->current.type == LV_TOKEN_MINUS) {
        const char *op = (p->current.type == LV_TOKEN_PLUS) ? "+" : "-";
        LvSourceLoc loc = p->current.loc;
        advance(p);
        LvAstNode *operand = parse_unary_expr(p);
        return lv_ast_create_unary(loc, op, operand);
    }
    return parse_primary_expr(p);
}

/* 几何关系函数名查找表（精确匹配，strcmp 语义） */
static const char *const kGeometryRelations[] = {
    "collinear", "parallel", "perpendicular", "congruent", "tangent"
};

/* 几何度量函数名查找表（精确匹配，strcmp 语义） */
static const char *const kMeasureFuncs[] = {
    "length", "distance", "angle", "measure", "area", "radius"
};

/* 几何对象构造函数名查找表（精确匹配，strcmp 语义） */
static const char *const kGeometryFuncs[] = {
    "point", "line", "circle", "segment", "ray", "triangle"
};

/** 检查 identifier 是否为关系/度量/几何函数名 */
static int is_relation_func(const char *name) {
    for (size_t i = 0; i < sizeof(kGeometryRelations) / sizeof(kGeometryRelations[0]); i++) {
        if (strcmp(name, kGeometryRelations[i]) == 0)
            return 1;
    }
    return 0;
}

static int is_measure_func(const char *name) {
    for (size_t i = 0; i < sizeof(kMeasureFuncs) / sizeof(kMeasureFuncs[0]); i++) {
        if (strcmp(name, kMeasureFuncs[i]) == 0)
            return 1;
    }
    return 0;
}

static int is_geometry_func(const char *name) {
    for (size_t i = 0; i < sizeof(kGeometryFuncs) / sizeof(kGeometryFuncs[0]); i++) {
        if (strcmp(name, kGeometryFuncs[i]) == 0)
            return 1;
    }
    return 0;
}

/** 解析参数列表: "(" Expr ("," Expr)* ")" */
static LvAstNode *parse_arg_list(LvParser *p) {
    LvAstNode *first_arg = NULL;
    LvAstNode *last_arg = NULL;

    if (!match(p, LV_TOKEN_LPAREN)) {
        expect(p, LV_TOKEN_LPAREN, "expected '(' in argument list");
        return NULL;
    }

    if (p->current.type != LV_TOKEN_RPAREN) {
        while (1) {
            LvAstNode *arg = parse_logic_expr(p);
            if (!arg)
                break;

            if (!first_arg) {
                first_arg = arg;
            } else {
                last_arg->next = arg;
            }
            last_arg = arg;

            if (p->current.type == LV_TOKEN_COMMA) {
                advance(p);
            } else {
                break;
            }
        }
    }

    expect(p, LV_TOKEN_RPAREN, "expected ')' to close argument list");
    return first_arg;
}

/** PrimaryExpr ::= Literal | Identifier | FunctionCall | MeasureExpr |
 *                  GeometryExpr | "(" Expr ")" */
static LvAstNode *parse_primary_expr(LvParser *p) {
    LvSourceLoc loc = p->current.loc;

    /* 整数/有理数/小数 */
    if (p->current.type == LV_TOKEN_INTEGER) {
        const char *txt = token_text(&p->current);
        char *end = NULL;
        errno = 0;
        long long val = strtoll(txt, &end, 10);
        if (errno != 0 || end == txt) {
            val = 0;
        }
        advance(p);
        return lv_ast_create_int(loc, val);
    }

    if (p->current.type == LV_TOKEN_RATIONAL) {
        const char *txt = token_text(&p->current);
        long long num = 0, den = 1;
        char *end = NULL;
        errno = 0;
        num = strtoll(txt, &end, 10);
        if (errno == 0 && end != txt && *end == '/') {
            errno = 0;
            den = strtoll(end + 1, &end, 10);
            if (errno != 0 || den == 0)
                den = 1;
        }
        advance(p);
        return lv_ast_create_rational(loc, num, den);
    }

    if (p->current.type == LV_TOKEN_DECIMAL) {
        const char *txt = token_text(&p->current);
        char *end = NULL;
        errno = 0;
        double val = strtod(txt, &end);
        if (errno != 0 || end == txt) {
            val = 0.0;
        }
        advance(p);
        return lv_ast_create_decimal(loc, val);
    }

    /* 字符串 */
    if (p->current.type == LV_TOKEN_STRING) {
        const char *txt = token_text(&p->current);
        size_t len = strlen(txt);
        char *val = NULL;
        if (len >= 2) {
            val = (char *) lv_malloc(len - 1);
            if (val) {
                memcpy(val, txt + 1, len - 2);
                val[len - 2] = '\0';
            }
        }
        advance(p);
        LvAstNode *node = lv_ast_create_string(loc, val ? val : "");
        lv_free((void **) &val);
        return node;
    }

    /* 带括号的表达式 */
    if (p->current.type == LV_TOKEN_LPAREN) {
        advance(p); /* ( */
        LvAstNode *expr = parse_logic_expr(p);
        expect(p, LV_TOKEN_RPAREN, "expected ')' after sub-expression");
        return expr;
    }

    /* 标识符（可能是函数调用、关系、度量、几何表达式） */
    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        char name[128];
        lv_strncpy(name, token_text(&p->current), sizeof(name));
        LvSourceLoc ident_loc = p->current.loc;
        advance(p);

        /* 检查后面是不是 "(" --- FunctionCall, Relation, Measure, Geometry */
        if (p->current.type == LV_TOKEN_LPAREN) {
            LvAstNode *args = parse_arg_list(p);

            LvAstNodeType call_type = LV_AST_FUNCTION_CALL;
            if (is_relation_func(name)) {
                call_type = LV_AST_RELATION;
            } else if (is_measure_func(name)) {
                call_type = LV_AST_MEASURE;
            } else if (is_geometry_func(name)) {
                call_type = LV_AST_GEOMETRY_EXPR;
            }

            LvAstNode *args_arr[LV_MAX_CALL_ARGS];
            int arg_count = 0;
            for (LvAstNode *c = args; c && arg_count < LV_MAX_CALL_ARGS; c = c->next)
                args_arr[arg_count++] = c;
            return lv_ast_create_call_typed(call_type, ident_loc, name, args_arr, arg_count);
        }

        /* 单纯的标识符 */
        return lv_ast_create_ident(ident_loc, name);
    }

    /* 关键字也可以是几何函数调用：length, distance, angle, area, radius, measure, point, line, etc. */
    if (p->current.type == LV_TOKEN_KW_LENGTH || p->current.type == LV_TOKEN_KW_DISTANCE ||
        p->current.type == LV_TOKEN_KW_AREA || p->current.type == LV_TOKEN_KW_RADIUS ||
        p->current.type == LV_TOKEN_KW_MEASURE) {
        /* Copy immediately — token_text uses a static buffer */
        char func_name[128];
        lv_strncpy(func_name, token_text(&p->current), sizeof(func_name));

        LvSourceLoc kw_loc = p->current.loc;
        advance(p);

        if (p->current.type == LV_TOKEN_LPAREN) {
            LvAstNode *args = parse_arg_list(p);
            LvAstNode *args_arr[LV_MAX_CALL_ARGS];
            int arg_count = 0;
            for (LvAstNode *c = args; c && arg_count < LV_MAX_CALL_ARGS; c = c->next)
                args_arr[arg_count++] = c;
            return lv_ast_create_call_typed(LV_AST_MEASURE, kw_loc, func_name, args_arr, arg_count);
        }

        /* Just an identifier-like usage */
        LvAstNode *node = lv_ast_create_ident(kw_loc, func_name);
        return node;
    }

    /* Handle "angle" keyword - it's both an entity and could be measure */
    if (p->current.type == LV_TOKEN_KW_ANGLE) {
        LvSourceLoc kw_loc = p->current.loc;
        advance(p);

        if (p->current.type == LV_TOKEN_LPAREN) {
            LvAstNode *args = parse_arg_list(p);
            LvAstNode *args_arr[LV_MAX_CALL_ARGS];
            int arg_count = 0;
            for (LvAstNode *c = args; c && arg_count < LV_MAX_CALL_ARGS; c = c->next)
                args_arr[arg_count++] = c;
            return lv_ast_create_call_typed(LV_AST_MEASURE, kw_loc, "angle", args_arr, arg_count);
        }
        return lv_ast_create_ident(kw_loc, "Angle");
    }

    /* Handle "collinear" as keyword */
    if (p->current.type == LV_TOKEN_KW_COLLINEAR) {
        LvSourceLoc kw_loc = p->current.loc;
        advance(p);
        LvAstNode *args = NULL;
        if (p->current.type == LV_TOKEN_LPAREN) {
            args = parse_arg_list(p);
        }
        LvAstNode *args_arr[LV_MAX_CALL_ARGS];
        int arg_count = 0;
        for (LvAstNode *c = args; c && arg_count < LV_MAX_CALL_ARGS; c = c->next)
            args_arr[arg_count++] = c;
        return lv_ast_create_call_typed(LV_AST_RELATION, kw_loc, "collinear", args_arr, arg_count);
    }

    /* Handle "parallel", "perpendicular", "congruent", "tangent" as keywords */
    if (p->current.type == LV_TOKEN_KW_PARALLEL || p->current.type == LV_TOKEN_KW_PERPENDICULAR ||
        p->current.type == LV_TOKEN_KW_CONGRUENT || p->current.type == LV_TOKEN_KW_TANGENT) {
        const char *name = "";
        if (p->current.type >= 0 && p->current.type < LV_TOKEN_COUNT)
            name = s_keyword_name_strings[p->current.type];
        LvSourceLoc kw_loc = p->current.loc;
        advance(p);
        LvAstNode *args = NULL;
        if (p->current.type == LV_TOKEN_LPAREN) {
            args = parse_arg_list(p);
        }
        LvAstNode *args_arr[LV_MAX_CALL_ARGS];
        int arg_count = 0;
        for (LvAstNode *c = args; c && arg_count < LV_MAX_CALL_ARGS; c = c->next)
            args_arr[arg_count++] = c;
        return lv_ast_create_call_typed(LV_AST_RELATION, kw_loc, name, args_arr, arg_count);
    }

    /* 无法识别的 token — 尝试忽略并前进 */
    parser_error(p, &p->current, "unexpected token in expression");
    advance(p);
    return NULL;
}

/* ================================================================
 * 公共 API
 * ================================================================ */

LvParser *lv_parser_create(LvLexer *lexer) {
    if (!lexer)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lexer is NULL");
    LvParser *p = (LvParser *) lv_calloc(1, sizeof(LvParser));
    if (!p)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate parser");
    p->lexer = lexer;
    p->error_count = 0;
    /* 预读第一个 token */
    advance(p);
    return p;
}

void lv_parser_destroy(LvParser *parser) {
    if (!parser)
        return;
    lv_free((void **) &parser);
}

LvParseResult lv_parser_parse_program(LvParser *p) {
    LvParseResult result;
    memset(&result, 0, sizeof(result));

    if (!p) {
        result.error_count = 1;
        result.errors[0].loc.line = 0;
        lv_strncpy(result.errors[0].message, "parser is NULL", sizeof(result.errors[0].message));
        return result;
    }

    LvSourceLoc program_loc = {1, 1, 0};
    LvAstNode *program = lv_ast_create(LV_AST_PROGRAM, program_loc);
    if (!program) {
        result.error_count = 1;
        lv_strncpy(result.errors[0].message, "out of memory", sizeof(result.errors[0].message));
        return result;
    }

    LvAstNode *first_stmt = NULL;
    LvAstNode *last_stmt = NULL;

    while (p->current.type != LV_TOKEN_EOF) {
        LvAstNode *stmt = parse_statement(p);
        if (stmt) {
            if (!first_stmt) {
                first_stmt = stmt;
            } else {
                last_stmt->next = stmt;
            }
            last_stmt = stmt;
            program->child_count++;
        } else {
            /* 无法解析的 token — 尝试跳过 */
            if (p->current.type != LV_TOKEN_EOF) {
                parser_error(p, &p->current, "unexpected token");
                advance(p);
            }
        }
    }

    program->child = first_stmt;

    result.ast = program;
    result.error_count = p->error_count;
    for (int i = 0; i < p->error_count && i < 64; i++) {
        result.errors[i] = p->errors[i];
    }

    return result;
}
