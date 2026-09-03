#include "lv/lv_parser.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_str_utils.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* 函数/关系/度量/几何调用参数个数上限 */
#define LV_MAX_CALL_ARGS 32

/* ── Parser 结构 ── */
struct LvParser {
    LvLexer *lexer;
    LvToken current;
    int error_count;
    LvParseError errors[64];
    int paren_depth; /* K28/F54：括号嵌套递归深度计数（防纯括号嵌套爆栈，
                      * 上限读 lvConfig.parser.parser_max_ast_depth，不硬编码） */
    int auto_name_seq; /* S7：自动命名计数器（裸构造语句生成 P<n> 等） */
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

/** 期望当前 token 为指定类型，否则报错（R4：填 severity=ERROR + fix_hint） */
static int expect(LvParser *p, LvTokenType type, const char *msg) {
    if (p->current.type == type) {
        advance(p);
        return 1;
    }
    if (p->error_count < 64) {
        int idx = p->error_count++;
        p->errors[idx].loc = p->current.loc;
        p->errors[idx].severity = LV_DIAG_ERROR;
        lv_snprintf(p->errors[idx].message, sizeof(p->errors[idx].message), "expected %s but got %s: %s",
                    lv_token_type_name(type), lv_token_type_name(p->current.type), msg);
        /* 修复方向提示：期望分号/括号/标识符等给出可执行建议 */
        lv_snprintf(p->errors[idx].fix_hint, sizeof(p->errors[idx].fix_hint), "在当前位置补 %s",
                    lv_token_type_name(type));
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

/** 记录解析错误（R4：填 severity=ERROR，fix_hint 留空由调用方 msg 描述） */
static void parser_error(LvParser *p, const LvToken *tok, const char *msg) {
    if (p->error_count < 64) {
        int idx = p->error_count++;
        p->errors[idx].loc = tok->loc;
        p->errors[idx].severity = LV_DIAG_ERROR;
        lv_strlcpy(p->errors[idx].message, msg, sizeof(p->errors[idx].message));
        p->errors[idx].fix_hint[0] = '\0';
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
static int is_geometry_func(const char *name); /* S7：前向声明（定义在查找表区） */
static int geom_func_to_entity(const char *name);

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

/** token → 比较运算符字符串 查找表（EQEQ/NEQ/LT/LE/GT/GE/EQUALS 单一事实源） */
static const char *const s_compare_op_strings[LV_TOKEN_COUNT] = {
    [LV_TOKEN_EQEQ] = "==",
    [LV_TOKEN_NEQ] = "!=",
    [LV_TOKEN_LT] = "<",
    [LV_TOKEN_LE] = "<=",
    [LV_TOKEN_GT] = ">",
    [LV_TOKEN_GE] = ">=",
    [LV_TOKEN_EQUALS] = "=", /* 命题相等（规格写法），如 verify(o,s,v) = Pass(_) */
};

/** token → 关键字名称 查找表（与共享关系词表 lv_geometry_relation_keywords 对齐） */
static const char *const s_keyword_name_strings[LV_TOKEN_COUNT] = {
    [LV_TOKEN_KW_COLLINEAR] = "collinear",
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

/** 在声明值上下文中识别 "F(args) -> Type" 的返回类型标注。
 *  判定条件（保守，避免与逻辑蕴含 P -> Q 冲突）：
 *  1. 整个声明值恰好是单层 LV_AST_LOGIC_IMPLIES（parse_logic_expr 已把 "->" 吞成蕴含）；
 *  2. 左端为调用表达式（FUNCTION_CALL / RELATION / MEASURE / GEOMETRY_EXPR）；
 *  3. 右端为裸标识符（类型名）。
 *  满足则返回复制的类型名（调用者负责释放）；否则返回 NULL，保持逻辑蕴含语义。
 *  边界（诚实）：右端仅支持裸类型名，不支持泛型返回类型（如 -> List<X>），
 *  该类场景仍解析为逻辑蕴含；仅 ":= 声明值" 上下文启用本识别。 */
static char *extract_return_type(LvAstNode *value) {
    if (!value || value->type != LV_AST_LOGIC_IMPLIES)
        return NULL;
    LvAstNode *left = value->data.binary.left;
    LvAstNode *right = value->data.binary.right;
    if (!left || !right || right->type != LV_AST_IDENTIFIER_EXPR)
        return NULL;
    if (left->type != LV_AST_FUNCTION_CALL && left->type != LV_AST_RELATION &&
        left->type != LV_AST_MEASURE && left->type != LV_AST_GEOMETRY_EXPR)
        return NULL;
    return lv_strdup(right->data.ident.name);
}

/** DeclarationStmt ::= EntityType IdentifierList ";" */

/* ── S1 坐标字面量辅助：将 (NUM, NUM) 解析为结构字面量 {x, y} ── */

/** 解析单个数值 token（INTEGER / DECIMAL / 前导负号），返回字面量节点或 NULL */
static LvAstNode *parse_coord_number(LvParser *p) {
    LvSourceLoc loc = p->current.loc;
    if (p->current.type == LV_TOKEN_MINUS) {
        advance(p);
        /* 负数：-N → unary(-, N)（与 parse_unary_expr 同一表示） */
        LvAstNode *val = parse_coord_number(p);
        if (!val)
            return NULL;
        return lv_ast_create_unary(loc, "-", val);
    }
    if (p->current.type == LV_TOKEN_INTEGER) {
        const char *txt = token_text(&p->current);
        char *end = NULL;
        errno = 0;
        long long v = strtoll(txt, &end, 10);
        if (errno != 0 || end == txt) {
            v = 0;
        }
        advance(p);
        return lv_ast_create_int(loc, v);
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
        double val;
        if (lv_parse_double(txt, &val) != 0) {
            val = 0.0;
        }
        advance(p);
        return lv_ast_create_decimal(loc, val);
    }
    return NULL;
}

/**
 * @brief 解析声明值（S1：坐标字面量 (NUM, NUM) → 结构字面量 {x,y}；
 *        其它形态回落 parse_logic_expr 普通表达式）
 */
static LvAstNode *parse_decl_value(LvParser *p, LvSourceLoc *out_loc) {
    LvSourceLoc loc = p->current.loc;
    /* 坐标字面量前置检查：( 且内容恰为 坐标对 才走坐标分支。
     * 坐标格式：( <x> , <y> )，其中 <x>/<y> ∈ { NUM | -NUM }，
     * NUM ∈ INTEGER / RATIONAL / DECIMAL。 */
    int is_coord = 0;
    if (p->current.type == LV_TOKEN_LPAREN) {
        LvToken t[7];
        for (int i = 0; i < 7; i++)
            t[i] = lv_lexer_peek(p->lexer, i);

        /* 从 t[0] 起消费 可选的 '-' + NUM，遇 ',' 继续，再消费 可选 '-' + NUM，遇 ')' 匹配 */
        int pos = 0;
        if (t[pos].type == LV_TOKEN_MINUS)
            pos++;
        if (pos < 7 &&
            (t[pos].type == LV_TOKEN_INTEGER || t[pos].type == LV_TOKEN_RATIONAL ||
             t[pos].type == LV_TOKEN_DECIMAL)) {
            pos++;
            if (pos < 7 && t[pos].type == LV_TOKEN_COMMA) {
                pos++;
                if (pos < 7 && t[pos].type == LV_TOKEN_MINUS)
                    pos++;
                if (pos < 7 &&
                    (t[pos].type == LV_TOKEN_INTEGER || t[pos].type == LV_TOKEN_RATIONAL ||
                     t[pos].type == LV_TOKEN_DECIMAL)) {
                    pos++;
                    if (pos < 7 && t[pos].type == LV_TOKEN_RPAREN)
                        is_coord = 1;
                }
            }
        }
    }

    if (!is_coord) {
        return parse_logic_expr(p);
    }

    advance(p); /* ( */
    LvAstNode *vx = parse_coord_number(p);
    expect(p, LV_TOKEN_COMMA, "expected ',' in coordinate literal");
    LvAstNode *vy = parse_coord_number(p);
    expect(p, LV_TOKEN_RPAREN, "expected ')' after coordinate literal");

    if (!vx || !vy) {
        lv_ast_destroy(vx);
        lv_ast_destroy(vy);
        return NULL;
    }

    /* 构造 {x: vx, y: vy} 结构字面量（复用 loader_extract_struct_coords） */
    LvAstNode *fx = lv_ast_create(LV_AST_STRUCT_FIELD, loc);
    LvAstNode *fy = lv_ast_create(LV_AST_STRUCT_FIELD, loc);
    if (!fx || !fy) {
        lv_ast_destroy(vx);
        lv_ast_destroy(vy);
        lv_ast_destroy(fx);
        lv_ast_destroy(fy);
        return NULL;
    }
    fx->data.field.name = lv_strdup("x");
    fx->data.field.value = vx;
    fy->data.field.name = lv_strdup("y");
    fy->data.field.value = vy;
    fx->next = fy;

    LvAstNode *node = lv_ast_create(LV_AST_STRUCT_LITERAL, loc);
    if (!node) {
        lv_ast_destroy(fx);
        lv_ast_destroy(fy);
        return NULL;
    }
    node->child = fx;
    node->child_count = 2;
    if (out_loc)
        *out_loc = loc;
    return node;
}

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

    LvAstNode *node = lv_ast_create(LV_AST_DECLARATION, loc);
    LvAstNode *decl_value = NULL;
    if (node) {
        node->data.decl.entity_type = (int) entity;
        node->data.decl.names = lv_strdup(names_buf);
    }

    /* EntityType Identifier ":=" Expr ";" — 声明值（如 "Point Spec := {...}"）。
     * 注意：":=" 由 COLON + EQUALS 两个 token 组成。
     * S1 语法糖：单 "=" 亦接受（Point A = (1, 2);），二者等价。
     * 返回类型标注："F(args) -> Type" 中 "-> Type" 记录到 decl.return_type
     *（识别规则见 extract_return_type，与逻辑蕴含 P -> Q 的区分条件见该函数注释）。 */
    int has_decl_value = 0;
    if (p->current.type == LV_TOKEN_COLON && check(p, 0, LV_TOKEN_EQUALS)) {
        advance(p); /* ':' */
        advance(p); /* '=' */
        has_decl_value = 1;
    } else if (p->current.type == LV_TOKEN_EQUALS) {
        advance(p); /* '='（S1：单等号声明值） */
        has_decl_value = 1;
    }
    if (has_decl_value) {
        /* S1 坐标字面量：声明值为 (NUM, NUM) → 结构字面量 {x: NUM, y: NUM}
         * （复用 loader_extract_struct_coords 消费路径）。仅当以 '(' 开头且
         * 内容恰为 两个数值 时走坐标分支；否则回落普通表达式解析。 */
        decl_value = parse_decl_value(p, node ? &node->loc : NULL);
        if (node && decl_value) {
            node->data.decl.value = decl_value;
            node->data.decl.return_type = extract_return_type(decl_value);
            if (node->data.decl.return_type) {
                /* 剥离 IMPLIES 外壳：left 为调用表达式（作为声明值），right 类型名已复制 */
                LvAstNode *call = decl_value->data.binary.left;
                LvAstNode *rt_ident = decl_value->data.binary.right;
                decl_value->data.binary.left = NULL;
                decl_value->data.binary.right = NULL;
                lv_ast_destroy(decl_value); /* 释放 IMPLIES 外壳节点 */
                if (rt_ident)
                    lv_ast_destroy(rt_ident); /* 释放右端标识符节点及其名字 */
                node->data.decl.value = call;
            }
        }
    }

    if (!match(p, LV_TOKEN_SEMICOLON)) {
        expect(p, LV_TOKEN_SEMICOLON, "expected ';' after declaration");
        synchronize(p);
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
        lv_strlcpy(name, token_text(&p->current), sizeof(name));
        advance(p);
    } else {
        expect(p, LV_TOKEN_IDENTIFIER, "expected identifier after Let");
    }

    expect(p, LV_TOKEN_COLON, "expected ':' after identifier in Let");

    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        lv_strlcpy(type_name, token_text(&p->current), sizeof(type_name));
        advance(p);
    } else {
        /* 也可能是类型关键字 */
        if (is_entity_type(p->current.type)) {
            lv_strlcpy(type_name, token_text(&p->current), sizeof(type_name));
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

/** ConstraintStmt ::= "Constraint" Identifier? ":"? LogicExpr ";" */
static LvAstNode *parse_constraint_stmt(LvParser *p) {
    LvSourceLoc loc = p->current.loc;
    advance(p); /* Constraint */

    char name[128] = {0};
    /* 命名约束: "Constraint Name: formula;"（Name 为 Identifier 且后随冒号） */
    if (p->current.type == LV_TOKEN_IDENTIFIER && check(p, 0, LV_TOKEN_COLON)) {
        lv_strlcpy(name, token_text(&p->current), sizeof(name));
        advance(p);
        advance(p); /* 消费 ':' */
    }

    LvAstNode *expr = parse_logic_expr(p);
    if (!match(p, LV_TOKEN_SEMICOLON)) {
        expect(p, LV_TOKEN_SEMICOLON, "expected ';' after statement");
        synchronize(p);
    }

    LvAstNode *node = lv_ast_create(LV_AST_CONSTRAINT_STMT, loc);
    if (node) {
        node->data.stmt.name = name[0] ? lv_strdup(name) : NULL;
        node->data.stmt.expr = expr;
    }
    return node;
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
        lv_strlcpy(name, token_text(&p->current), sizeof(name));
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
        lv_strlcpy(name, token_text(&p->current), sizeof(name));
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

/** NormalizeStmt ::= "Normalize" (Identifier | "all")? ";" */
static LvAstNode *parse_normalize_stmt(LvParser *p) {
    LvSourceLoc loc = p->current.loc;
    advance(p); /* Normalize */

    char target[128] = {0};
    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        lv_strlcpy(target, token_text(&p->current), sizeof(target));
        advance(p);
    } else {
        /* 无参形式 "Normalize;"：target 语义为"全部"（"all"）。
         * 注意：不能消费当前 token（分号/EOF），否则后续 match(SEMICOLON) 必然失败。 */
        lv_strlcpy(target, "all", sizeof(target));
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
        lv_strlcpy(target, token_text(&p->current), sizeof(target));
        advance(p);
    } else {
        expect(p, LV_TOKEN_IDENTIFIER, "expected export target");
    }

    /* "as" is a regular identifier, not a keyword */
    if (p->current.type == LV_TOKEN_IDENTIFIER && lv_str_eq(token_text(&p->current), "as")) {
        advance(p); /* consume "as" */
    }

    char format[128] = {0};
    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        lv_strlcpy(format, token_text(&p->current), sizeof(format));
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
            lv_strlcpy_n(path, sizeof(path), txt + 1, plen);
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
    if (p->current.type == LV_TOKEN_IDENTIFIER && lv_str_eq(token_text(&p->current), "as")) {
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

/* S7 自动命名（匿名构造）：裸几何构造语句
 *   midpoint(A, B);  segment(A, B);  circle(A, B); ...
 * → 自动生成命名声明 "P<N> <func>(args)" 或 "<name> <func>(args)"。
 * 构造名 → 实体类型 映射（与 loader 端 decl handler 一致）。
 */
static int geom_func_to_entity(const char *name) {
    if (lv_str_eq(name, "point")) return LV_ENTITY_POINT;
    if (lv_str_eq(name, "line")) return LV_ENTITY_LINE;
    if (lv_str_eq(name, "segment")) return LV_ENTITY_SEGMENT;
    if (lv_str_eq(name, "circle")) return LV_ENTITY_CIRCLE;
    if (lv_str_eq(name, "ray")) return LV_ENTITY_RAY;
    if (lv_str_eq(name, "triangle")) return LV_ENTITY_TRIANGLE;
    return -1;
}

/** S7：裸构造语句 → 自动命名 DECLARATION（名 P<N>，类型由构造名推断）。
 *  仅当语句以几何构造关键字调用开头时触发；其余回落 NULL（正常报错路径）。 */
static LvAstNode *parse_auto_named_stmt(LvParser *p) {
    LvSourceLoc loc = p->current.loc;
    if (p->current.type != LV_TOKEN_IDENTIFIER)
        return NULL;
    const char *name = token_text(&p->current);
    if (!is_geometry_func(name))
        return NULL;
    int entity = geom_func_to_entity(name);
    if (entity < 0)
        return NULL;

    /* 解析完整构造调用表达式 */
    LvAstNode *expr = parse_primary_expr(p);
    if (!expr)
        return NULL;
    if (expr->type != LV_AST_GEOMETRY_EXPR) {
        /* 不是构造调用（如裸关系/度量），释放并回落 */
        lv_ast_destroy(expr);
        return NULL;
    }

    /* 自动命名 P<N>（后续可引用；命名序列贯穿整个文件） */
    char auto_name[32];
    lv_snprintf(auto_name, sizeof(auto_name), "P%d", ++p->auto_name_seq);

    LvAstNode *node = lv_ast_create(LV_AST_DECLARATION, loc);
    if (!node) {
        lv_ast_destroy(expr);
        return NULL;
    }
    node->data.decl.entity_type = entity;
    node->data.decl.names = lv_strdup(auto_name);
    node->data.decl.value = expr;
    node->data.decl.return_type = NULL;

    if (!match(p, LV_TOKEN_SEMICOLON)) {
        expect(p, LV_TOKEN_SEMICOLON, "expected ';' after auto-named construct");
        synchronize(p);
    }
    return node;
}

/** Statement ::= DeclarationStmt | ConstraintStmt | ... */
static LvAstNode *parse_statement(LvParser *p) {
    if (is_entity_type(p->current.type)) {
        return parse_declaration_stmt(p);
    }

    /* S7：裸几何构造调用（自动命名），如 "midpoint(A, B);"（若以标识符构造开头） */
    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        const char *nm = token_text(&p->current);
        if (is_geometry_func(nm)) {
            LvAstNode *auto_stmt = parse_auto_named_stmt(p);
            if (auto_stmt)
                return auto_stmt;
        }
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
        if (p->current.type == LV_TOKEN_IDENTIFIER && lv_str_eq(token_text(&p->current), "iff")) {
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
        if (p->current.type == LV_TOKEN_IDENTIFIER && lv_str_eq(token_text(&p->current), "implies")) {
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
        /* 类型联合 |：与 or 同优先级（规格写法，如 Point Output := AST | IR） */
        if (p->current.type == LV_TOKEN_PIPE) {
            is_or = 1;
        }
        /* "\/" is not a single token, would be SLASH, CARET... actually not used in practice */
        if (!is_or)
            break;

        int is_pipe = (p->current.type == LV_TOKEN_PIPE);
        advance(p); /* consume "or" / "|" */

        LvAstNode *right = parse_and_expr(p);
        if (!right)
            break;

        if (is_pipe)
            left = lv_ast_create_logic_binary(LV_AST_UNION, left->loc, "|", left, right);
        else
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
            lv_strlcpy(node->data.unary.op, "not", sizeof(node->data.unary.op));
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
                lv_strlcpy(var_name, token_text(&p->current), sizeof(var_name));
                advance(p);
            } else {
                expect(p, LV_TOKEN_IDENTIFIER, "expected variable name in quantifier binder");
                break;
            }

            expect(p, LV_TOKEN_COLON, "expected ':' in quantifier binder");

            if (p->current.type == LV_TOKEN_IDENTIFIER) {
                lv_strlcpy(var_type, token_text(&p->current), sizeof(var_type));
                advance(p);
            } else if (is_entity_type(p->current.type)) {
                lv_strlcpy(var_type, token_text(&p->current), sizeof(var_type));
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

/** PredicateExpr ::= CompareExpr
 *                   | CompareExpr Identifier（无符号中缀谓词）
 * true/false/bottom 字面量由 parse_primary_expr 处理（表达式任意位置）。 */
static LvAstNode *parse_predicate_expr(LvParser *p) {
    LvAstNode *expr = parse_compare_expr(p);
    if (!expr)
        return NULL;

    /* 无符号中缀谓词: expr predicate（如 verify(o,s,v) terminates_in_finite_steps）。
     * 排除 implies/iff 标识符——它们由上层 parse_implies_expr 处理。 */
    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        const char *txt = token_text(&p->current);
        if (lv_str_ne(txt, "implies") && lv_str_ne(txt, "iff")) {
            char pname[128];
            lv_strlcpy(pname, txt, sizeof(pname));
            LvSourceLoc ploc = p->current.loc;
            advance(p);
            LvAstNode *pnode = lv_ast_create(LV_AST_PREDICATE_APP, ploc);
            if (pnode) {
                pnode->data.call.func_name = lv_strdup(pname);
                pnode->data.call.args = expr;
            }
            return pnode ? pnode : expr;
        }
    }

    return expr;
}

/** CompareExpr ::= AddExpr (("==" | "!=" | "<" | "<=" | ">" | ">=") AddExpr)? */
/* 比较运算符字符串统一查上表 s_compare_op_strings（EQUALS 为表尾附加项）。 */
static LvAstNode *parse_compare_expr(LvParser *p) {
    LvAstNode *left = parse_add_expr(p);
    if (!left)
        return NULL;

    LvSourceLoc op_loc = p->current.loc;

    const char *op = NULL;
    if (p->current.type >= 0 && p->current.type < LV_TOKEN_COUNT)
        op = s_compare_op_strings[p->current.type];
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

/** MulExpr ::= UnaryExpr (("*" | "/" | "^") UnaryExpr)* */
static LvAstNode *parse_mul_expr(LvParser *p) {
    LvAstNode *left = parse_unary_expr(p);
    if (!left)
        return NULL;

    while (p->current.type == LV_TOKEN_STAR || p->current.type == LV_TOKEN_SLASH ||
           p->current.type == LV_TOKEN_CARET) {
        const char *op;
        if (p->current.type == LV_TOKEN_STAR) {
            op = "*";
        } else if (p->current.type == LV_TOKEN_SLASH) {
            op = "/";
        } else {
            op = "^";
        }
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

/* 几何关系/度量函数名查找表（精确匹配，strcmp 语义）。
 * 词表收敛：由 lv_lexer.h 共享表 lv_geometry_relation_keywords /
 * lv_measurement_keywords（NULL 结尾）提供，与 lv_sema.c 共用单一事实源。 */

/* 几何对象构造函数名查找表（精确匹配，strcmp 语义）。
 * 词表收敛：由 lv_lexer.h 共享表 lv_geometry_constructor_keywords（NULL 结尾）提供，
 * 与 lv_sema.c 共用单一事实源。 */

/** 检查 identifier 是否为关系/度量/几何函数名 */
static int is_relation_func(const char *name) {
    for (size_t i = 0; lv_geometry_relation_keywords[i] != NULL; i++) {
        if (lv_str_eq(name, lv_geometry_relation_keywords[i]))
            return 1;
    }
    return 0;
}

static int is_measure_func(const char *name) {
    for (size_t i = 0; lv_measurement_keywords[i] != NULL; i++) {
        if (lv_str_eq(name, lv_measurement_keywords[i]))
            return 1;
    }
    return 0;
}

static int is_geometry_func(const char *name) {
    for (size_t i = 0; lv_geometry_constructor_keywords[i] != NULL; i++) {
        if (lv_str_eq(name, lv_geometry_constructor_keywords[i]))
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
            /* 命名参数: "name: value"（如 verify(output: Output, ...)）。
             * 保留参数名：包装为 LV_AST_NAMED_ARG(name, value)，value 为类型/值表达式。 */
            char arg_name[128] = {0};
            int has_name = 0;
            if (p->current.type == LV_TOKEN_IDENTIFIER && check(p, 0, LV_TOKEN_COLON)) {
                lv_strlcpy(arg_name, token_text(&p->current), sizeof(arg_name));
                has_name = 1;
                advance(p); /* name */
                advance(p); /* ':' */
            }
            LvAstNode *arg = parse_logic_expr(p);
            if (!arg)
                break;

            if (has_name) {
                LvAstNode *named = lv_ast_create(LV_AST_NAMED_ARG, arg->loc);
                if (named) {
                    named->data.field.name = lv_strdup(arg_name);
                    named->data.field.value = arg;
                    arg = named;
                }
            }

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

/** 解析记录字面量: { Field (, Field)* }；Field ::= Identifier : Expr */
static LvAstNode *parse_struct_literal(LvParser *p) {
    LvSourceLoc loc = p->current.loc;
    advance(p); /* { */

    LvAstNode *first_field = NULL;
    LvAstNode *last_field = NULL;
    int field_count = 0;

    while (p->current.type != LV_TOKEN_RBRACE && p->current.type != LV_TOKEN_EOF) {
        char fname[128] = {0};
        if (p->current.type == LV_TOKEN_IDENTIFIER) {
            lv_strlcpy(fname, token_text(&p->current), sizeof(fname));
            advance(p);
        } else {
            expect(p, LV_TOKEN_IDENTIFIER, "expected field name in record literal");
            break;
        }

        expect(p, LV_TOKEN_COLON, "expected ':' after field name in record literal");

        LvAstNode *value = parse_logic_expr(p);

        LvAstNode *field = lv_ast_create(LV_AST_STRUCT_FIELD, loc);
        if (field) {
            field->data.field.name = lv_strdup(fname);
            field->data.field.value = value;
        }
        if (!first_field) {
            first_field = field;
        } else {
            last_field->next = field;
        }
        last_field = field;
        field_count++;

        if (p->current.type == LV_TOKEN_COMMA) {
            advance(p);
        } else {
            break;
        }
    }

    expect(p, LV_TOKEN_RBRACE, "expected '}' to close record literal");

    LvAstNode *node = lv_ast_create(LV_AST_STRUCT_LITERAL, loc);
    if (node) {
        node->child = first_field;
        node->child_count = field_count;
    }
    return node;
}

/* ── 嵌套泛型辅助（前瞻扫描 + 消费式拼接） ── */

static int scan_generic_arg(LvParser *p, int i, char *buf, size_t cap, size_t *pos);
static int scan_generic_list(LvParser *p, int i, char *buf, size_t cap, size_t *pos);

/** 追加 token 文本到缓冲区（截断安全），返回新的 pos */
static size_t generic_append_text(const LvToken *tok, char *buf, size_t cap, size_t pos) {
    if (!tok || !buf || pos >= cap - 1)
        return pos;
    size_t copy = tok->length < cap - 1 - pos ? tok->length : cap - 1 - pos;
    memcpy(buf + pos, tok->start, copy);
    return pos + copy;
}

/** 前瞻扫描一个泛型参数（offset i 处应为 identifier 或 identifier<...>）。
 *  成功返回下一 offset；失败返回 -1。文本追加到 buf（供拼接）。 */
static int scan_generic_arg(LvParser *p, int i, char *buf, size_t cap, size_t *pos) {
    LvToken t = lv_lexer_peek(p->lexer, i);
    if (t.type != LV_TOKEN_IDENTIFIER)
        return -1;
    *pos = generic_append_text(&t, buf, cap, *pos);
    i++;
    LvToken n = lv_lexer_peek(p->lexer, i);
    if (n.type == LV_TOKEN_LT) {
        i = scan_generic_list(p, i, buf, cap, pos);
        if (i < 0)
            return -1;
    }
    return i;
}

/** 前瞻扫描 '<' Arg (',' Arg)* '>'（offset i 处应为 '<'）。成功返回下一 offset，失败返回 -1。 */
static int scan_generic_list(LvParser *p, int i, char *buf, size_t cap, size_t *pos) {
    LvToken t = lv_lexer_peek(p->lexer, i);
    if (t.type != LV_TOKEN_LT)
        return -1;
    if (*pos + 1 < cap)
        buf[(*pos)++] = '<';
    i++;
    i = scan_generic_arg(p, i, buf, cap, pos);
    if (i < 0)
        return -1;
    while (1) {
        t = lv_lexer_peek(p->lexer, i);
        if (t.type == LV_TOKEN_COMMA) {
            if (*pos + 1 < cap)
                buf[(*pos)++] = ',';
            i++;
            i = scan_generic_arg(p, i, buf, cap, pos);
            if (i < 0)
                return -1;
        } else {
            break;
        }
    }
    t = lv_lexer_peek(p->lexer, i);
    if (t.type != LV_TOKEN_GT)
        return -1;
    if (*pos + 1 < cap)
        buf[(*pos)++] = '>';
    return i + 1;
}

/** 尝试把当前 token（应为 '<'）之后的泛型参数序列解析并拼接到 name（如 List<List<Formula>>）。
 *  先前瞻验证（不消费 token），验证通过后消费 '<' 及参数 token 并拼接 "name<...>"；
 *  验证失败不消费任何 token（"<" 留给比较运算符）。
 *  返回 1 表示成功消费并拼接，返回 0 表示失败（未消费）。
 *  边界（诚实）：泛型参数须为普通标识符（关键字实体名如 Point 不支持）；
 *  拼接缓冲 256 字节上限；"a < b > c" 形似泛型的比较仍会被拼接为 "a<b>"（与既有行为一致）。 */
static int try_parse_generic_type(LvParser *p, char *name, size_t name_cap) {
    char tmp[256];
    size_t pos = 0;

    /* 前瞻验证（不消费） */
    int i = scan_generic_arg(p, 0, tmp, sizeof(tmp), &pos);
    if (i < 0)
        return 0;
    while (1) {
        LvToken t = lv_lexer_peek(p->lexer, i);
        if (t.type == LV_TOKEN_COMMA) {
            if (pos + 1 < sizeof(tmp))
                tmp[pos++] = ',';
            i++;
            i = scan_generic_arg(p, i, tmp, sizeof(tmp), &pos);
            if (i < 0)
                return 0;
        } else {
            break;
        }
    }
    LvToken gt = lv_lexer_peek(p->lexer, i);
    if (gt.type != LV_TOKEN_GT)
        return 0;
    tmp[pos] = '\0';

    /* 拼接 "name<tmp>"；缓冲区不足则放弃（不消费 token） */
    size_t nlen = strlen(name);
    size_t tlen = strlen(tmp);
    if (nlen + tlen + 3 > name_cap)
        return 0;
    name[nlen++] = '<';
    memcpy(name + nlen, tmp, tlen);
    nlen += tlen;
    name[nlen++] = '>';
    name[nlen] = '\0';

    /* 消费 '<'（p->current）及 peek(0..i) 共 i+1 个 token */
    advance(p); /* < */
    for (int k = 0; k <= i; k++)
        advance(p);
    return 1;
}

/** PrimaryExpr ::= Literal | Identifier | FunctionCall | MeasureExpr |
 *                  GeometryExpr | "(" Expr ")" */
static LvAstNode *parse_primary_expr(LvParser *p) {
    LvSourceLoc loc = p->current.loc;

    /* 布尔字面量关键字（表达式任意位置，如 Prove x == true; / not false） */
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
        double val;
        if (lv_parse_double(txt, &val) != 0) {
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

    /* 带括号的表达式 —— K28/F54：括号嵌套是解析器唯一不受限的递归源
     * （AST 深度测不出被展平的括号），此处显式计数并对照配置上限拒绝 */
    if (p->current.type == LV_TOKEN_LPAREN) {
        const lvConfig *cfg = lv_config_current();
        p->paren_depth++;
        if (p->paren_depth > cfg->parser.parser_max_ast_depth) {
            if (p->error_count < 64) {
                int idx = p->error_count++;
                p->errors[idx].loc = p->current.loc;
                lv_snprintf(p->errors[idx].message, sizeof(p->errors[idx].message),
                            "括号嵌套深度 %d 超过上限 %d（lvConfig.parser.parser_max_ast_depth）",
                            p->paren_depth, cfg->parser.parser_max_ast_depth);
            }
            p->paren_depth--;
            return NULL;
        }
        advance(p); /* ( */
        LvAstNode *expr = parse_logic_expr(p);
        expect(p, LV_TOKEN_RPAREN, "expected ')' after sub-expression");
        p->paren_depth--;
        return expr;
    }

    /* 记录字面量: { field: value, ... }（规格文件，如 Point Spec := { a: T }） */
    if (p->current.type == LV_TOKEN_LBRACE) {
        return parse_struct_literal(p);
    }

    /* 标识符（可能是函数调用、关系、度量、几何表达式） */
    if (p->current.type == LV_TOKEN_IDENTIFIER) {
        char name[128];
        lv_strlcpy(name, token_text(&p->current), sizeof(name));
        LvSourceLoc ident_loc = p->current.loc;
        advance(p);

        /* 成员访问: ident . ident（如 v.mode）→ 拼接为 v.mode */
        if (p->current.type == LV_TOKEN_DOT && check(p, 0, LV_TOKEN_IDENTIFIER)) {
            /* member 文本取自 peek 到的下一个 token（p->current 此刻是 '.'） */
            LvToken member_tok = lv_lexer_peek(p->lexer, 0);
            char mbuf[128];
            lv_token_text(&member_tok, mbuf, sizeof(mbuf));
            size_t nlen = strlen(name);
            if (nlen < sizeof(name) - 1) {
                name[nlen++] = '.';
                size_t mlen = strlen(mbuf);
                if (mlen > sizeof(name) - nlen - 1)
                    mlen = sizeof(name) - nlen - 1;
                memcpy(name + nlen, mbuf, mlen);
                name[nlen + mlen] = '\0';
            }
            advance(p); /* 弹出 member token 到 current */
            advance(p); /* 再消费，使 current 指向 member 之后的 token */
        }

        /* 泛型类型应用: T<Arg> / T<A, B> / T<T<...>>（嵌套，如 List<List<Formula>>）。
         * 前瞻验证（不消费）通过后才消费并拼接名字；失败则 "<" 留给比较运算符。
         * 边界见 try_parse_generic_type：形似泛型的比较 "a < b > c" 仍会被拼接为 "a<b>"（与既有行为一致）。 */
        if (p->current.type == LV_TOKEN_LT) {
            try_parse_generic_type(p, name, sizeof(name));
        }

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
        lv_strlcpy(func_name, token_text(&p->current), sizeof(func_name));

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
        lv_strlcpy(result.errors[0].message, "parser is NULL", sizeof(result.errors[0].message));
        return result;
    }

    LvSourceLoc program_loc = {1, 1, 0};
    LvAstNode *program = lv_ast_create(LV_AST_PROGRAM, program_loc);
    if (!program) {
        result.error_count = 1;
        lv_strlcpy(result.errors[0].message, "out of memory", sizeof(result.errors[0].message));
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
