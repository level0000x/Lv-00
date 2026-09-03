#include <stdio.h>
#include <string.h>

#include "lv/lv_loader.h"
#include "lv/lv_parser.h"
#include "lv/parser_safety.h" /* lv_check_ast_depth（K28/F54 AST 深度闸门） */

#define TEST_PASS_STATEMENT g_pass_count++
#define TEST_FAIL_STATEMENT g_fail_count++
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

static const char *dbg_ast_type(LvAstNodeType t) {
    switch (t) {
        case LV_AST_PROGRAM:
            return "PROGRAM";
        case LV_AST_DECLARATION:
            return "DECLARATION";
        case LV_AST_LET:
            return "LET";
        case LV_AST_CONSTRAINT_STMT:
            return "CONSTRAINT_STMT";
        case LV_AST_PROVE_STMT:
            return "PROVE_STMT";
        case LV_AST_ASSUME_STMT:
            return "ASSUME_STMT";
        case LV_AST_ASSERT_STMT:
            return "ASSERT_STMT";
        case LV_AST_COMPUTE_STMT:
            return "COMPUTE_STMT";
        case LV_AST_NORMALIZE_STMT:
            return "NORMALIZE_STMT";
        case LV_AST_EXPORT_STMT:
            return "EXPORT_STMT";
        case LV_AST_AXIOM_STMT:
            return "AXIOM_STMT";
        case LV_AST_THEOREM_STMT:
            return "THEOREM_STMT";
        case LV_AST_IDENTIFIER_EXPR:
            return "IDENTIFIER";
        case LV_AST_INTEGER_LITERAL:
            return "INTEGER";
        case LV_AST_RATIONAL_LITERAL:
            return "RATIONAL";
        case LV_AST_DECIMAL_LITERAL:
            return "DECIMAL";
        case LV_AST_STRING_LITERAL:
            return "STRING";
        case LV_AST_BOOL_LITERAL:
            return "BOOL";
        case LV_AST_LOGIC_AND:
            return "AND";
        case LV_AST_LOGIC_OR:
            return "OR";
        case LV_AST_LOGIC_NOT:
            return "NOT";
        case LV_AST_LOGIC_IMPLIES:
            return "IMPLIES";
        case LV_AST_LOGIC_IFF:
            return "IFF";
        case LV_AST_LOGIC_FORALL:
            return "FORALL";
        case LV_AST_LOGIC_EXISTS:
            return "EXISTS";
        case LV_AST_BINARY_OP:
            return "BINARY_OP";
        case LV_AST_UNARY_OP:
            return "UNARY_OP";
        case LV_AST_FUNCTION_CALL:
            return "FUNCTION_CALL";
        case LV_AST_RELATION:
            return "RELATION";
        case LV_AST_MEASURE:
            return "MEASURE";
        case LV_AST_GEOMETRY_EXPR:
            return "GEOMETRY";
        case LV_AST_COMPARE:
            return "COMPARE";
        case LV_AST_STRUCT_LITERAL:
            return "STRUCT_LITERAL";
        case LV_AST_STRUCT_FIELD:
            return "STRUCT_FIELD";
        case LV_AST_UNION:
            return "UNION";
        case LV_AST_PREDICATE_APP:
            return "PREDICATE_APP";
        case LV_AST_MODULE_DECL:
            return "MODULE";
        case LV_AST_IMPORT_DECL:
            return "IMPORT";
        case LV_AST_PROOF_BLOCK:
            return "PROOF_BLOCK";
        default:
            return "???";
    }
}

/* 辅助：解析给定源代码并返回结果 */
static LvParseResult parse_source(const char *src) {
    LvLexer *lex = lv_lexer_create(src, strlen(src));
    LvParser *parser = lv_parser_create(lex);
    LvParseResult result = lv_parser_parse_program(parser);
    lv_parser_destroy(parser);
    lv_lexer_destroy(lex);
    return result;
}

static void test_declaration(void) {
    printf("[声明语句]\n");

    {
        const char *src = "Point A, B, C;";
        LvParseResult res = parse_source(src);
        TEST("Point A, B, C");
        if (res.ast && res.ast->type == LV_AST_PROGRAM && res.ast->child &&
            res.ast->child->type == LV_AST_DECLARATION && res.error_count == 0) {
            LvAstNode *decl = res.ast->child;
            if (decl->data.decl.entity_type == LV_ENTITY_POINT && strcmp(decl->data.decl.names, "A,B,C") == 0) {
                PASS();
            } else {
                FAIL("entity type or names mismatch");
            }
        } else {
            FAIL("parse failed or unexpected AST structure");
        }
        lv_ast_destroy(res.ast);
    }

    {
        const char *src = "Line L1;";
        LvParseResult res = parse_source(src);
        TEST("Line L1");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_DECLARATION && res.error_count == 0) {
            LvAstNode *decl = res.ast->child;
            if (decl->data.decl.entity_type == LV_ENTITY_LINE && strcmp(decl->data.decl.names, "L1") == 0) {
                PASS();
            } else {
                FAIL("entity type or names mismatch");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    {
        const char *src = "Circle C1, C2; Triangle T1, T2, T3;";
        LvParseResult res = parse_source(src);
        TEST("Circle + Triangle");
        if (res.ast && res.error_count == 0) {
            LvAstNode *c = res.ast->child;
            if (c && c->type == LV_AST_DECLARATION && c->data.decl.entity_type == LV_ENTITY_CIRCLE && c->next &&
                c->next->type == LV_AST_DECLARATION && c->next->data.decl.entity_type == LV_ENTITY_TRIANGLE) {
                PASS();
            } else {
                FAIL("node structure mismatch");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }
}

/* S1 坐标字面量：Point A = (1, 2); Point B = (3.5, -2); Point C = (1/2, 3); */
static void test_coord_literal_decl(void) {
    printf("[S1 坐标字面量声明]\n");

    {
        const char *src = "Point A = (1, 2);";
        LvParseResult res = parse_source(src);
        TEST("Point A = (1, 2)");
        if (res.ast && res.ast->child && res.error_count == 0) {
            LvAstNode *decl = res.ast->child;
            if (decl->type == LV_AST_DECLARATION && decl->data.decl.value &&
                decl->data.decl.value->type == LV_AST_STRUCT_LITERAL) {
                PASS();
            } else {
                FAIL("expected decl with STRUCT_LITERAL value");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    {
        const char *src = "Point B = (3.5, -2); Point C = (1/2, 3);";
        LvParseResult res = parse_source(src);
        TEST("Point B = (3.5, -2); Point C = (1/2, 3)");
        if (res.ast && res.error_count == 0) {
            int structs = 0;
            for (LvAstNode *s = res.ast->child; s; s = s->next) {
                if (s->type == LV_AST_DECLARATION && s->data.decl.value &&
                    s->data.decl.value->type == LV_AST_STRUCT_LITERAL) {
                    structs++;
                }
            }
            if (structs == 2) {
                PASS();
            } else {
                FAIL("expected 2 coordinate-literal declarations");
            }
        } else {
            FAIL("parse failed (decimal/negative/rational coords)");
        }
        lv_ast_destroy(res.ast);
    }

    {
        /* 坐标字面量仅限声明值：普通表达式 (1,2) 仍走子表达式路径不应误判 */
        const char *src = "Point D := point(1, 2);";
        LvParseResult res = parse_source(src);
        TEST("Point D := point(1, 2) (:= 保持)");
        if (res.ast && res.error_count == 0) {
            PASS();
        } else {
            FAIL(":= declaration regressed");
        }
        lv_ast_destroy(res.ast);
    }
}

/* S7 自动命名：裸几何构造语句 → 自动 P<N> 声明 */
static void test_auto_named_stmt(void) {
    printf("[S7 自动命名构造]\n");

    {
        const char *src = "Point A = (1, 2); circle(A, A);";
        LvParseResult res = parse_source(src);
        TEST("裸 circle(A, A) 自动命名");
        if (res.ast && res.error_count == 0) {
            int decls = 0;
            int has_circle = 0;
            const char *names = NULL;
            for (LvAstNode *s = res.ast->child; s; s = s->next) {
                if (s->type != LV_AST_DECLARATION)
                    continue;
                decls++;
                if (s->data.decl.entity_type == LV_ENTITY_CIRCLE && s->data.decl.value) {
                    has_circle = 1;
                    names = s->data.decl.names;
                }
            }
            if (decls == 2 && has_circle && names && strncmp(names, "P", 1) == 0) {
                PASS();
            } else {
                printf("  decls=%d has_circle=%d names=%s\n", decls, has_circle, names ? names : "(null)");
                FAIL("expected 2 decls incl auto-named circle");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    {
        /* 连续裸构造：自动名递增 P1, P2 */
        const char *src = "circle(A, A); circle(B, B);";
        LvParseResult res = parse_source(src);
        TEST("连续裸构造自动名递增");
        if (res.ast && res.error_count == 0) {
            int circles = 0;
            char prev[16] = "";
            int incr_ok = 1;
            for (LvAstNode *s = res.ast->child; s; s = s->next) {
                if (s->type == LV_AST_DECLARATION && s->data.decl.entity_type == LV_ENTITY_CIRCLE &&
                    s->data.decl.names) {
                    circles++;
                    if (prev[0] && strcmp(prev, s->data.decl.names) >= 0)
                        incr_ok = 0;
                    lv_strlcpy(prev, s->data.decl.names, sizeof(prev));
                }
            }
            if (circles == 2 && incr_ok) {
                PASS();
            } else {
                FAIL("expected 2 auto-named circles with increasing names");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_constraint(void) {
    printf("[约束/关系语句]\n");

    {
        const char *src = "Constraint collinear(A, B, C);";
        LvParseResult res = parse_source(src);
        TEST("Constraint collinear(A,B,C)");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_CONSTRAINT_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_RELATION && strcmp(expr->data.call.func_name, "collinear") == 0) {
                PASS();
            } else {
                FAIL("expected RELATION with collinear");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    {
        const char *src = "Constraint congruent(A, B, C, D);";
        LvParseResult res = parse_source(src);
        TEST("Constraint congruent(A,B,C,D)");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_CONSTRAINT_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_RELATION && strcmp(expr->data.call.func_name, "congruent") == 0) {
                PASS();
            } else {
                FAIL("expected RELATION with congruent");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_prove(void) {
    printf("[Prove 语句]\n");

    {
        const char *src = "Prove length(A,B) == length(A,C);";
        LvParseResult res = parse_source(src);
        TEST("Prove length(A,B) == length(A,C)");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_PROVE_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_COMPARE && strcmp(expr->data.compare.op, "==") == 0) {
                LvAstNode *left = expr->data.compare.left;
                LvAstNode *right = expr->data.compare.right;
                if (left && left->type == LV_AST_MEASURE && strcmp(left->data.call.func_name, "length") == 0 && right &&
                    right->type == LV_AST_MEASURE && strcmp(right->data.call.func_name, "length") == 0) {
                    PASS();
                } else {
                    printf("  <- left type=%d(%s) func='%s', right type=%d(%s) func='%s'\n",
                           left ? (int) left->type : -1, left ? dbg_ast_type(left->type) : "NULL",
                           left && left->type == LV_AST_MEASURE ? left->data.call.func_name : "N/A",
                           right ? (int) right->type : -1, right ? dbg_ast_type(right->type) : "NULL",
                           right && right->type == LV_AST_MEASURE ? right->data.call.func_name : "N/A");
                    FAIL("expected length measure on both sides");
                }
            } else {
                if (expr)
                    printf("  <- expr type=%d (%s)\n", expr->type, dbg_ast_type(expr->type));
                FAIL("expected COMPARE with ==");
            }
        } else {
            printf("  errors=%d, ast=%p, child=%p\n", res.error_count, (void *) res.ast,
                   (void *) (res.ast ? res.ast->child : NULL));
            if (res.ast && res.ast->child)
                printf("  child type=%d (%s)\n", res.ast->child->type, dbg_ast_type(res.ast->child->type));
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    {
        const char *src = "Prove true;";
        LvParseResult res = parse_source(src);
        TEST("Prove true");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_PROVE_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_BOOL_LITERAL && expr->data.literal.bool_value == 1) {
                PASS();
            } else {
                FAIL("expected BOOL_LITERAL true");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_quantifier(void) {
    printf("[量词表达式]\n");

    {
        const char *src = "Constraint forall x: Point. collinear(A, B, x);";
        LvParseResult res = parse_source(src);
        TEST("forall x: Point. collinear(A,B,x)");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_CONSTRAINT_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_LOGIC_FORALL) {
                if (strcmp(expr->data.quantifier.var_name, "x") == 0 &&
                    strcmp(expr->data.quantifier.var_type, "Point") == 0 && expr->data.quantifier.body &&
                    expr->data.quantifier.body->type == LV_AST_RELATION &&
                    strcmp(expr->data.quantifier.body->data.call.func_name, "collinear") == 0) {
                    PASS();
                } else {
                    FAIL("quantifier details mismatch");
                }
            } else {
                FAIL("expected LOGIC_FORALL");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    {
        const char *src = "Constraint exists p: Point. distance(A, p) < 1;";
        LvParseResult res = parse_source(src);
        TEST("exists p: Point. distance(A,p) < 1");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_CONSTRAINT_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_LOGIC_EXISTS && strcmp(expr->data.quantifier.var_name, "p") == 0 &&
                strcmp(expr->data.quantifier.var_type, "Point") == 0 && expr->data.quantifier.body &&
                expr->data.quantifier.body->type == LV_AST_COMPARE) {
                PASS();
            } else {
                FAIL("quantifier details mismatch");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_let(void) {
    printf("[Let 语句]\n");

    {
        const char *src = "Let s: Segment = segment(A, B);";
        LvParseResult res = parse_source(src);
        TEST("Let s: Segment = segment(A,B)");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_LET && res.error_count == 0) {
            LvAstNode *let = res.ast->child;
            if (strcmp(let->data.let_def.name, "s") == 0 && strcmp(let->data.let_def.type_name, "Segment") == 0 &&
                let->data.let_def.value && let->data.let_def.value->type == LV_AST_GEOMETRY_EXPR &&
                strcmp(let->data.let_def.value->data.call.func_name, "segment") == 0) {
                PASS();
            } else {
                FAIL("let details mismatch");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_module_import(void) {
    printf("[Module/Import 语句]\n");

    {
        const char *src = "module Geometry.Base;";
        LvParseResult res = parse_source(src);
        TEST("module Geometry.Base");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_MODULE_DECL && res.error_count == 0) {
            LvAstNode *m = res.ast->child;
            if (strcmp(m->data.module_import.qualified_name, "Geometry.Base") == 0) {
                PASS();
            } else {
                FAIL("qualified name mismatch");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    {
        const char *src = "import Geometry.Base as G;";
        LvParseResult res = parse_source(src);
        TEST("import Geometry.Base as G");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_IMPORT_DECL && res.error_count == 0) {
            LvAstNode *imp = res.ast->child;
            if (strcmp(imp->data.module_import.qualified_name, "Geometry.Base") == 0) {
                PASS();
            } else {
                FAIL("qualified name mismatch");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_normalize(void) {
    printf("[Normalize 语句]\n");

    {
        const char *src = "Normalize;";
        LvParseResult res = parse_source(src);
        TEST("Normalize (no target)");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_NORMALIZE_STMT && res.error_count == 0) {
            LvAstNode *n = res.ast->child;
            if (strcmp(n->data.normalize.target, "all") == 0) {
                PASS();
            } else {
                printf("  <- target='%s'\n", n->data.normalize.target ? n->data.normalize.target : "NULL");
                FAIL("target mismatch, expected 'all'");
            }
        } else {
            printf("  errors=%d\n", res.error_count);
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    {
        const char *src = "Normalize all;";
        LvParseResult res = parse_source(src);
        TEST("Normalize all");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_NORMALIZE_STMT && res.error_count == 0) {
            LvAstNode *n = res.ast->child;
            if (strcmp(n->data.normalize.target, "all") == 0) {
                PASS();
            } else {
                FAIL("target mismatch");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    {
        const char *src = "Normalize myVar;";
        LvParseResult res = parse_source(src);
        TEST("Normalize myVar");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_NORMALIZE_STMT && res.error_count == 0) {
            LvAstNode *n = res.ast->child;
            if (strcmp(n->data.normalize.target, "myVar") == 0) {
                PASS();
            } else {
                FAIL("target mismatch");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_theorem(void) {
    printf("[Theorem 语句]\n");

    {
        const char *src = "Theorem my_theorem: collinear(A, B, C);";
        LvParseResult res = parse_source(src);
        TEST("Theorem without proof block");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_THEOREM_STMT && res.error_count == 0) {
            LvAstNode *t = res.ast->child;
            if (strcmp(t->data.theorem.name, "my_theorem") == 0 && t->data.theorem.proposition &&
                t->data.theorem.proposition->type == LV_AST_RELATION && t->data.theorem.proof_block == NULL) {
                PASS();
            } else {
                FAIL("theorem details mismatch");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_logical_ops(void) {
    printf("[逻辑运算]\n");

    {
        const char *src = "Prove collinear(A,B,C) and collinear(A,B,D);";
        LvParseResult res = parse_source(src);
        TEST("Prove ... and ...");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_PROVE_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_LOGIC_AND && expr->data.binary.left && expr->data.binary.right) {
                PASS();
            } else {
                FAIL("expected LOGIC_AND");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    {
        const char *src = "Assume collinear(A,B,C) or collinear(A,B,D);";
        LvParseResult res = parse_source(src);
        TEST("Assume ... or ...");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_ASSUME_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_LOGIC_OR) {
                PASS();
            } else {
                FAIL("expected LOGIC_OR");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    {
        const char *src = "Assert not collinear(A,B,C);";
        LvParseResult res = parse_source(src);
        TEST("Assert not ...");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_ASSERT_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_LOGIC_NOT && expr->data.unary.operand &&
                expr->data.unary.operand->type == LV_AST_RELATION) {
                PASS();
            } else {
                FAIL("expected LOGIC_NOT wrapping RELATION");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    {
        /* "implies" is a regular identifier used as binary operator */
        const char *src = "Prove collinear(A,B,C) implies collinear(A,B,D);";
        LvParseResult res = parse_source(src);
        TEST("Prove ... implies ...");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_PROVE_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_LOGIC_IMPLIES) {
                PASS();
            } else {
                FAIL("expected LOGIC_IMPLIES");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_axiom(void) {
    printf("[Axiom 语句]\n");

    {
        const char *src = "Axiom trans: collinear(A,B,C) implies collinear(C,B,A);";
        LvParseResult res = parse_source(src);
        TEST("Axiom with implies");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_AXIOM_STMT && res.error_count == 0) {
            PASS();
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_compute_export(void) {
    printf("[Compute/Export 语句]\n");

    {
        const char *src = "Compute 2 + 3 * 4;";
        LvParseResult res = parse_source(src);
        TEST("Compute 2 + 3 * 4");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_COMPUTE_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_BINARY_OP && strcmp(expr->data.binary.op, "+") == 0) {
                PASS();
            } else {
                FAIL("expected BINARY_OP +");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_error_recovery(void) {
    printf("[错误恢复]\n");

    {
        /* Missing semicolon between statements */
        const char *src = "Point A B;";
        LvParseResult res = parse_source(src);
        TEST("Point A B (missing comma)");
        /* Should still produce some AST even with errors */
        if (res.ast != NULL) {
            PASS();
        } else {
            FAIL("should return AST despite errors");
        }
        /* But there should be errors */
        if (res.error_count > 0) {
            printf("  (got %d errors, as expected)\n", res.error_count);
        }
        /* R4：severity 通道——解析错误默认 ERROR（memset 后 0=ERROR 语义） */
        TEST_ASSERT(res.error_count == 0 || res.errors[0].severity == LV_DIAG_ERROR,
                    "parse error severity defaults to ERROR");
        lv_ast_destroy(res.ast);
    }

    {
        const char *src = "Prove  ;";
        LvParseResult res = parse_source(src);
        TEST("Prove ; (empty expr)");
        if (res.ast != NULL) {
            PASS();
        } else {
            FAIL("should handle gracefully");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_bool_literal_expr(void) {
    printf("[布尔字面量在表达式位置]\n");

    /* Prove x == true; 中 true 作为布尔字面量（缺陷2） */
    {
        const char *src = "Point A;\nProve A == true;";
        LvParseResult res = parse_source(src);
        TEST("Prove x == true");
        if (res.ast && res.error_count == 0) {
            LvAstNode *prove = res.ast->child;
            while (prove && prove->type != LV_AST_PROVE_STMT)
                prove = prove->next;
            if (prove && prove->data.stmt.expr && prove->data.stmt.expr->type == LV_AST_COMPARE &&
                strcmp(prove->data.stmt.expr->data.compare.op, "==") == 0 &&
                prove->data.stmt.expr->data.compare.right &&
                prove->data.stmt.expr->data.compare.right->type == LV_AST_BOOL_LITERAL) {
                PASS();
            } else {
                FAIL("expected COMPARE(==) with right BOOL_LITERAL");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    /* true 在比较左侧 */
    {
        const char *src = "Prove true == false;";
        LvParseResult res = parse_source(src);
        TEST("Prove true == false");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_PROVE_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_COMPARE && expr->data.compare.left &&
                expr->data.compare.left->type == LV_AST_BOOL_LITERAL && expr->data.compare.right &&
                expr->data.compare.right->type == LV_AST_BOOL_LITERAL) {
                PASS();
            } else {
                FAIL("expected COMPARE with BOOL_LITERAL operands");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    /* true and true（and 关键字左右字面量） */
    {
        const char *src = "Prove true and true;";
        LvParseResult res = parse_source(src);
        TEST("Prove true and true");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_PROVE_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_LOGIC_AND) {
                PASS();
            } else {
                FAIL("expected LOGIC_AND");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    /* not false（前缀逻辑非 + 字面量操作数） */
    {
        const char *src = "Prove not false;";
        LvParseResult res = parse_source(src);
        TEST("Prove not false");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_PROVE_STMT && res.error_count == 0) {
            LvAstNode *expr = res.ast->child->data.stmt.expr;
            if (expr && expr->type == LV_AST_LOGIC_NOT && expr->data.unary.operand &&
                expr->data.unary.operand->type == LV_AST_BOOL_LITERAL) {
                PASS();
            } else {
                FAIL("expected LOGIC_NOT wrapping BOOL_LITERAL");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_spec_extensions(void) {
    printf("[规格文件扩展：命名约束 / 记录字面量 / 类型联合]\n");

    /* 命名约束: Constraint Name: formula;（缺陷3） */
    {
        const char *src = "Constraint VerifierSound: forall o: Output, s: Spec. verify(o, s) = Pass(_) -> output_satisfies(o, s);";
        LvParseResult res = parse_source(src);
        TEST("Constraint Name: formula");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_CONSTRAINT_STMT && res.error_count == 0) {
            LvAstNode *c = res.ast->child;
            if (c->data.stmt.name && strcmp(c->data.stmt.name, "VerifierSound") == 0 && c->data.stmt.expr &&
                c->data.stmt.expr->type == LV_AST_LOGIC_FORALL) {
                PASS();
            } else {
                printf("  <- name='%s' expr_type=%s\n", c->data.stmt.name ? c->data.stmt.name : "NULL",
                       c->data.stmt.expr ? dbg_ast_type(c->data.stmt.expr->type) : "NULL");
                FAIL("named constraint mismatch");
            }
        } else {
            printf("  errors=%d\n", res.error_count);
            for (int i = 0; i < res.error_count && i < 5; i++)
                printf("    error %d: %s\n", i, res.errors[i].message);
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    /* 普通约束不带名字（兼容） */
    {
        const char *src = "Constraint collinear(A, B, C);";
        LvParseResult res = parse_source(src);
        TEST("Constraint without name");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_CONSTRAINT_STMT && res.error_count == 0) {
            LvAstNode *c = res.ast->child;
            if (c->data.stmt.name == NULL && c->data.stmt.expr && c->data.stmt.expr->type == LV_AST_RELATION) {
                PASS();
            } else {
                FAIL("expected unnamed constraint with RELATION");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    /* 记录字面量: Point Spec := { field: value, ... };（缺陷3） */
    {
        const char *src = "Point Spec := { preconditions: List<Formula>, postconditions: List<Formula> };";
        LvParseResult res = parse_source(src);
        TEST("Point Spec := { ... }");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_DECLARATION && res.error_count == 0) {
            LvAstNode *d = res.ast->child;
            if (d->data.decl.value && d->data.decl.value->type == LV_AST_STRUCT_LITERAL &&
                d->data.decl.value->child && d->data.decl.value->child->type == LV_AST_STRUCT_FIELD &&
                d->data.decl.value->child->data.field.name &&
                strcmp(d->data.decl.value->child->data.field.name, "preconditions") == 0) {
                PASS();
            } else {
                FAIL("expected STRUCT_LITERAL with preconditions field");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    /* 类型联合: Point Output := A | B | C;（缺陷3） */
    {
        const char *src = "Point Output := AST | ResolvedAST | TypedAST;";
        LvParseResult res = parse_source(src);
        TEST("Point Output := A | B | C");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_DECLARATION && res.error_count == 0) {
            LvAstNode *v = res.ast->child->data.decl.value;
            if (v && v->type == LV_AST_UNION && v->data.binary.left && v->data.binary.left->type == LV_AST_UNION &&
                v->data.binary.left->data.binary.left &&
                v->data.binary.left->data.binary.left->type == LV_AST_IDENTIFIER_EXPR) {
                PASS();
            } else {
                FAIL("expected nested UNION chain");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    /* 构造子 + 命名参数 + 联合: Point Verdict := Pass(proof: ProofTerm) | Fail(...); */
    {
        const char *src = "Point Verdict := Pass(proof: ProofTerm) | Fail(reason: Set<Error>, cex: Option<Model>);";
        LvParseResult res = parse_source(src);
        TEST("constructor with named args + union");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_DECLARATION && res.error_count == 0) {
            LvAstNode *v = res.ast->child->data.decl.value;
            if (v && v->type == LV_AST_UNION && v->data.binary.left &&
                v->data.binary.left->type == LV_AST_FUNCTION_CALL &&
                strcmp(v->data.binary.left->data.call.func_name, "Pass") == 0) {
                PASS();
            } else {
                FAIL("expected UNION(FUNCTION_CALL Pass | ...)");
            }
        } else {
            printf("  errors=%d\n", res.error_count);
            for (int i = 0; i < res.error_count && i < 5; i++)
                printf("    error %d: %s\n", i, res.errors[i].message);
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    /* 无符号中缀谓词 + 成员访问 + 命题相等 "=" + 蕴含箭头（verifier.lv 风格） */
    {
        const char *src =
            "Constraint VerifierComplete: forall o: Output, s: Spec, v: Verifier.\n"
            "  output_satisfies(o, s) /\\ v.mode = Full -> verify(o, s, v) = Pass(_);\n"
            "Constraint VerifierFinite: forall o: Output, s: Spec, v: Verifier.\n"
            "  verify(o, s, v) terminates_in_finite_steps;\n";
        LvParseResult res = parse_source(src);
        TEST("verifier.lv 风格约束（/\\ 成员访问 中缀谓词）");
        if (res.ast && res.error_count == 0) {
            int count = 0;
            for (LvAstNode *s = res.ast->child; s; s = s->next)
                count++;
            if (count == 2) {
                PASS();
            } else {
                printf("  (got %d statements)\n", count);
                FAIL("statement count mismatch");
            }
        } else {
            printf("  errors=%d\n", res.error_count);
            for (int i = 0; i < res.error_count && i < 5; i++)
                printf("    error %d: %s\n", i, res.errors[i].message);
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    /* 命名参数名保留: Pass(proof: ProofTerm) 的参数名 "proof" 进入 AST（缺陷1） */
    {
        const char *src = "Point Verdict := Pass(proof: ProofTerm) | Fail(reason: Set<Error>);";
        LvParseResult res = parse_source(src);
        TEST("named argument name preserved");
        int ok = 0;
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_DECLARATION && res.error_count == 0) {
            LvAstNode *v = res.ast->child->data.decl.value;
            if (v && v->type == LV_AST_UNION && v->data.binary.left &&
                v->data.binary.left->type == LV_AST_FUNCTION_CALL) {
                LvAstNode *pass = v->data.binary.left;
                if (strcmp(pass->data.call.func_name, "Pass") == 0 && pass->data.call.args &&
                    pass->data.call.args->type == LV_AST_NAMED_ARG &&
                    pass->data.call.args->data.field.name &&
                    strcmp(pass->data.call.args->data.field.name, "proof") == 0 &&
                    pass->data.call.args->data.field.value &&
                    pass->data.call.args->data.field.value->type == LV_AST_IDENTIFIER_EXPR &&
                    strcmp(pass->data.call.args->data.field.value->data.ident.name, "ProofTerm") == 0) {
                    ok = 1;
                }
            }
        }
        if (ok) {
            PASS();
        } else {
            FAIL("named arg 'proof' not preserved");
        }
        lv_ast_destroy(res.ast);
    }

    /* "-> 类型名" 返回类型标注: VerifyFn := verify(...) -> Verdict（缺陷2） */
    {
        const char *src = "Point VerifyFn := verify(output: Output, spec: Spec, v: Verifier) -> Verdict;";
        LvParseResult res = parse_source(src);
        TEST("-> return type annotation");
        int ok = 0;
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_DECLARATION && res.error_count == 0) {
            LvAstNode *d = res.ast->child;
            if (d->data.decl.value && d->data.decl.value->type == LV_AST_FUNCTION_CALL &&
                d->data.decl.value->data.call.func_name &&
                strcmp(d->data.decl.value->data.call.func_name, "verify") == 0 &&
                d->data.decl.return_type && strcmp(d->data.decl.return_type, "Verdict") == 0) {
                ok = 1;
            }
        }
        if (ok) {
            PASS();
        } else {
            printf("  value_type=%s return_type=%s\n",
                   res.ast && res.ast->child ? dbg_ast_type(res.ast->child->type) : "NULL",
                   res.ast && res.ast->child && res.ast->child->type == LV_AST_DECLARATION
                       ? (res.ast->child->data.decl.return_type ? res.ast->child->data.decl.return_type : "NULL")
                       : "NULL");
            FAIL("expected FUNCTION_CALL value with return_type=Verdict (not IMPLIES)");
        }
        lv_ast_destroy(res.ast);
    }

    /* 边界：非调用左端的 "->" 保持逻辑蕴含（P -> Q 不被误判为返回类型） */
    {
        const char *src = "Point Impl := P -> Q;";
        LvParseResult res = parse_source(src);
        TEST("P -> Q stays LOGIC_IMPLIES in decl value");
        int ok = 0;
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_DECLARATION && res.error_count == 0) {
            LvAstNode *v = res.ast->child->data.decl.value;
            if (v && v->type == LV_AST_LOGIC_IMPLIES && res.ast->child->data.decl.return_type == NULL) {
                ok = 1;
            }
        }
        if (ok) {
            PASS();
        } else {
            FAIL("expected LOGIC_IMPLIES, not return_type");
        }
        lv_ast_destroy(res.ast);
    }

    /* 嵌套泛型: List<List<Formula>> 递归拼接（缺陷3） */
    {
        const char *src = "Point Spec := { nested: List<List<Formula>> };";
        LvParseResult res = parse_source(src);
        TEST("nested generic List<List<Formula>>");
        int ok = 0;
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_DECLARATION && res.error_count == 0) {
            LvAstNode *v = res.ast->child->data.decl.value;
            if (v && v->type == LV_AST_STRUCT_LITERAL && v->child &&
                v->child->type == LV_AST_STRUCT_FIELD && v->child->data.field.value &&
                v->child->data.field.value->type == LV_AST_IDENTIFIER_EXPR &&
                v->child->data.field.value->data.ident.name &&
                strcmp(v->child->data.field.value->data.ident.name, "List<List<Formula>>") == 0) {
                ok = 1;
            }
        }
        if (ok) {
            PASS();
        } else {
            FAIL("expected ident name 'List<List<Formula>>'");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_full_program(void) {
    printf("[完整程序解析]\n");

    {
        const char *src =
            "// 三角形定义\n"
            "Point A, B, C;\n"
            "Constraint collinear(A, B, C);\n"
            "Prove length(A, B) == length(A, C);\n";
        LvParseResult res = parse_source(src);
        TEST("完整的 .lv 程序");
        if (res.ast && res.ast->type == LV_AST_PROGRAM && res.error_count == 0) {
            /* 检查是否解析出 3 条语句 */
            int count = 0;
            for (LvAstNode *s = res.ast->child; s; s = s->next)
                count++;
            if (count == 3) {
                PASS();
            } else {
                printf("  (got %d statements, expected 3)\n", count);
                FAIL("statement count mismatch");
            }
        } else {
            FAIL("parse failed");
        }
        lv_ast_destroy(res.ast);
    }

    {
        const char *src =
            "Theorem example:\n"
            "  forall A: Point.\n"
            "    forall B: Point.\n"
            "      collinear(A, B, A) iff collinear(B, A, B)\n"
            "{\n"
            "  Prove collinear(A, B, A) implies collinear(B, A, B);\n"
            "}\n"
            ";\n";
        LvParseResult res = parse_source(src);
        TEST("Theorem with proof block (complex)");
        if (res.ast && res.ast->child && res.ast->child->type == LV_AST_THEOREM_STMT && res.error_count == 0) {
            LvAstNode *t = res.ast->child;
            if (t->data.theorem.proof_block && t->data.theorem.proof_block->type == LV_AST_PROOF_BLOCK &&
                t->data.theorem.proposition) {
                /* proposition is: forall A: Point. forall B: Point. (collinear(...) iff collinear(...)) */
                /* The outermost node is FORALL */
                LvAstNode *inner = t->data.theorem.proposition;
                if (inner->type == LV_AST_LOGIC_FORALL && inner->data.quantifier.body &&
                    inner->data.quantifier.body->type == LV_AST_LOGIC_FORALL &&
                    inner->data.quantifier.body->data.quantifier.body &&
                    inner->data.quantifier.body->data.quantifier.body->type == LV_AST_LOGIC_IFF) {
                    PASS();
                } else {
                    printf("  prop type=%s, body type=%s\n", dbg_ast_type(inner->type),
                           inner->data.quantifier.body ? dbg_ast_type(inner->data.quantifier.body->type) : "NULL");
                    FAIL("proposition structure mismatch");
                }
            } else {
                printf("  theorem name='%s', proof_block=%p, proposition type=%s\n",
                       t->data.theorem.name ? t->data.theorem.name : "NULL", (void *) t->data.theorem.proof_block,
                       t->data.theorem.proposition ? dbg_ast_type(t->data.theorem.proposition->type) : "NULL");
                FAIL("proof block or proposition structure mismatch");
            }
        } else {
            printf("  errors: %d, child type=%s\n", res.error_count,
                   res.ast && res.ast->child ? dbg_ast_type(res.ast->child->type) : "NULL");
            for (int i = 0; i < res.error_count && i < 5; i++) {
                printf("    error %d: %s\n", i, res.errors[i].message);
            }
            FAIL("parse failed or has errors");
        }
        lv_ast_destroy(res.ast);
    }
}

static void test_ast_creation(void) {
    printf("[AST 创建函数]\n");

    /* 测试 lv_entity_type_from_token */
    {
        TEST("lv_entity_type_from_token");
        if (lv_entity_type_from_token(LV_TOKEN_KW_POINT) == LV_ENTITY_POINT &&
            lv_entity_type_from_token(LV_TOKEN_KW_LINE) == LV_ENTITY_LINE &&
            lv_entity_type_from_token(LV_TOKEN_KW_CIRCLE) == LV_ENTITY_CIRCLE &&
            lv_entity_type_from_token(LV_TOKEN_KW_SEGMENT) == LV_ENTITY_SEGMENT &&
            lv_entity_type_from_token(LV_TOKEN_KW_TRIANGLE) == LV_ENTITY_TRIANGLE &&
            lv_entity_type_from_token(LV_TOKEN_KW_SCALAR) == LV_ENTITY_SCALAR &&
            lv_entity_type_from_token(LV_TOKEN_KW_BOOL) == LV_ENTITY_BOOL &&
            lv_entity_type_from_token(LV_TOKEN_KW_PROPOSITION) == LV_ENTITY_PROPOSITION &&
            lv_entity_type_from_token(LV_TOKEN_KW_PROOF) == LV_ENTITY_PROOF &&
            lv_entity_type_from_token(LV_TOKEN_KW_ANGLE) == LV_ENTITY_ANGLE &&
            lv_entity_type_from_token(LV_TOKEN_KW_RAY) == LV_ENTITY_RAY &&
            lv_entity_type_from_token(LV_TOKEN_KW_POLYGON) == LV_ENTITY_POLYGON) {
            PASS();
        } else {
            FAIL("type mapping mismatch");
        }
    }

    /* 测试 lv_entity_type_name */
    {
        TEST("lv_entity_type_name");
        if (strcmp(lv_entity_type_name(LV_ENTITY_POINT), "Point") == 0 &&
            strcmp(lv_entity_type_name(LV_ENTITY_LINE), "Line") == 0 &&
            strcmp(lv_entity_type_name(LV_ENTITY_CIRCLE), "Circle") == 0 &&
            strcmp(lv_entity_type_name(LV_ENTITY_SEGMENT), "Segment") == 0 &&
            strcmp(lv_entity_type_name(LV_ENTITY_TRIANGLE), "Triangle") == 0 &&
            strcmp(lv_entity_type_name(LV_ENTITY_SCALAR), "Scalar") == 0 &&
            strcmp(lv_entity_type_name(LV_ENTITY_BOOL), "Bool") == 0 &&
            strcmp(lv_entity_type_name(LV_ENTITY_PROPOSITION), "Proposition") == 0 &&
            strcmp(lv_entity_type_name(LV_ENTITY_PROOF), "Proof") == 0) {
            PASS();
        } else {
            FAIL("name mismatch");
        }
    }

    /* 测试 AST 节点创建和销毁 */
    {
        LvSourceLoc loc = {1, 1, 0};
        LvAstNode *node = lv_ast_create(LV_AST_PROGRAM, loc);
        TEST("lv_ast_create + destroy");
        if (node && node->type == LV_AST_PROGRAM) {
            lv_ast_destroy(node);
            PASS();
        } else {
            FAIL("creation failed");
            lv_ast_destroy(node);
        }
    }

    /* 测试 lv_ast_create_ident */
    {
        LvSourceLoc loc = {1, 5, 0};
        LvAstNode *node = lv_ast_create_ident(loc, "testIdent");
        TEST("lv_ast_create_ident");
        if (node && node->type == LV_AST_IDENTIFIER_EXPR && strcmp(node->data.ident.name, "testIdent") == 0) {
            lv_ast_destroy(node);
            PASS();
        } else {
            FAIL("ident creation failed");
            lv_ast_destroy(node);
        }
    }

    /* 测试 lv_ast_append_child */
    {
        LvSourceLoc loc = {1, 1, 0};
        LvAstNode *parent = lv_ast_create(LV_AST_PROGRAM, loc);
        LvAstNode *child1 = lv_ast_create(LV_AST_DECLARATION, loc);
        LvAstNode *child2 = lv_ast_create(LV_AST_CONSTRAINT_STMT, loc);
        lv_ast_append_child(parent, child1);
        lv_ast_append_child(parent, child2);
        TEST("lv_ast_append_child");
        if (parent->child == child1 && parent->child->next == child2 && parent->child_count == 2) {
            PASS();
        } else {
            FAIL("append_child failed");
        }
        lv_ast_destroy(parent);
    }

    /* 测试 lv_ast_create_compare */
    {
        LvSourceLoc loc = {1, 1, 0};
        LvAstNode *left = lv_ast_create_int(loc, 5);
        LvAstNode *right = lv_ast_create_int(loc, 3);
        LvAstNode *cmp = lv_ast_create_compare(loc, ">=", left, right);
        TEST("lv_ast_create_compare");
        if (cmp && cmp->type == LV_AST_COMPARE && strcmp(cmp->data.compare.op, ">=") == 0 &&
            cmp->data.compare.left == left && cmp->data.compare.right == right) {
            PASS();
        } else {
            FAIL("compare creation failed");
        }
        lv_ast_destroy(cmp);
    }

    /* 测试 lv_ast_create_binary */
    {
        LvSourceLoc loc = {1, 1, 0};
        LvAstNode *l = lv_ast_create_int(loc, 1);
        LvAstNode *r = lv_ast_create_int(loc, 2);
        LvAstNode *bin = lv_ast_create_binary(loc, "+", l, r);
        TEST("lv_ast_create_binary");
        if (bin && bin->type == LV_AST_BINARY_OP && strcmp(bin->data.binary.op, "+") == 0) {
            PASS();
        } else {
            FAIL("binary creation failed");
        }
        lv_ast_destroy(bin);
    }

    /* 测试 lv_ast_create_call */
    {
        LvSourceLoc loc = {1, 1, 0};
        LvAstNode *args = lv_ast_create_ident(loc, "A");
        LvAstNode *call = lv_ast_create_call(loc, "collinear", args);
        TEST("lv_ast_create_call");
        if (call && call->type == LV_AST_FUNCTION_CALL && strcmp(call->data.call.func_name, "collinear") == 0 &&
            call->data.call.args == args) {
            PASS();
        } else {
            FAIL("call creation failed");
        }
        lv_ast_destroy(call);
    }
}

/* ── VTable 化求值/折叠分发表测试（lv_loader.c 5 处 switch 收敛后的行为验证）── */

/** 辅助：解析单个 .lv 片段并执行证明验证（经 lv_verify_proofs 走 AST 求值分发表） */
static bool verify_prove_src(const char *src, LvProveSummary *summary) {
    LvParseResult res = parse_source(src);
    bool ok = false;
    if (res.ast && res.error_count == 0)
        ok = lv_verify_proofs(&res, summary);
    lv_ast_destroy(res.ast);
    return ok;
}

/**
 * @brief AST 求值/折叠分发表测试
 *
 * 覆盖 lv_loader.c 由 5 处 switch 收敛而来的 AST 求值分发表：
 * - fold 分发表：整数/布尔字面量、BINARY_OP（含除零短路）、UNARY_OP、FUNCTION_CALL（Church 表）；
 * - eval_proposition 分发表：COMPARE（数值/布尔比较）、逻辑运算、RELATION 反射律、
 *   default 纯布尔目标回退；
 * - is_pure/collect/eval_skeleton 分发表：纯命题骨架真值表（恒真 PASS / 反例 FAIL）。
 */
static void test_fold_eval_vtable(void) {
    printf("[AST 求值/折叠分发表]\n");
    LvProveSummary s;

    TEST("fold: 整数算术 2 + 2 == 4 通过");
    if (verify_prove_src("Prove 2 + 2 == 4;\n", &s) && s.pass_count == 1 && s.fail_count == 0)
        PASS();
    else
        FAIL("2+2==4 应 PASS");

    TEST("fold: 错误结论 2 + 2 == 5 失败");
    if (verify_prove_src("Prove 2 + 2 == 5;\n", &s) && s.fail_count == 1 && s.pass_count == 0)
        PASS();
    else
        FAIL("2+2==5 应 FAIL");

    TEST("fold: 括号优先级 (2 + 3) * 4 == 20 通过");
    if (verify_prove_src("Prove (2 + 3) * 4 == 20;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("括号优先级求值失败");

    TEST("fold: 一元负号 -5 + 3 == -2 通过");
    if (verify_prove_src("Prove -5 + 3 == -2;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("一元运算求值失败");

    TEST("fold: 除零短路 SKIP");
    if (verify_prove_src("Prove 1 / 0 == 0;\n", &s) && s.skip_count == 1)
        PASS();
    else
        FAIL("除零应 SKIP");

    TEST("fold: Church 函数 add(2, 3) == 5 通过");
    if (verify_prove_src("Prove add(2, 3) == 5;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("Church add 求值失败");

    TEST("eval_proposition: 布尔比较 true != false 通过");
    if (verify_prove_src("Prove true != false;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("true != false 应 PASS");

    TEST("eval_proposition: 逻辑 not false 通过");
    if (verify_prove_src("Prove not false;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("not false 应 PASS");

    TEST("eval_proposition: 反射律 collinear(A, A, A) 通过");
    if (verify_prove_src("Prove collinear(A, A, A);\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("反射律应 PASS");

    TEST("eval_proposition: 纯布尔目标 eq(2, 2) 通过（default 回退）");
    if (verify_prove_src("Prove eq(2, 2);\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("eq(2,2) 应 PASS");

    TEST("真值表: 恒真 A or not A 通过");
    if (verify_prove_src("Prove A or not A;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("A or not A 应恒真 PASS");

    TEST("真值表: 非恒真 A and B 失败");
    if (verify_prove_src("Prove A and B;\n", &s) && s.fail_count == 1)
        PASS();
    else
        FAIL("A and B 应 FAIL");

    TEST("真值表: 蕴含恒真 A -> A 通过");
    if (verify_prove_src("Prove A -> A;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("A -> A 应恒真 PASS");
}

/* K28/F54：AST 深度闸门 —— lv_ast_max_depth 计算正确 +
 * lv_load_file 接线后深 AST 被拒绝（上限读 lvConfig.parser.parser_max_ast_depth） */
static void test_ast_depth_gate(void) {
    printf("[AST 深度闸门]\n");

    TEST("lv_ast_max_depth: 单节点深度 0");
    {
        LvSourceLoc loc = {1, 1, 0};
        LvAstNode *leaf = lv_ast_create(LV_AST_INTEGER_LITERAL, loc);
        if (leaf && lv_ast_max_depth(leaf) == 0)
            PASS();
        else
            FAIL("单节点深度应为 0");
        lv_ast_destroy(leaf);
    }

    TEST("lv_ast_max_depth: 二元嵌套深度正确");
    {
        LvSourceLoc loc = {1, 1, 0};
        /* 构造 a + (b + (c + d)) 三层二元嵌套 */
        LvAstNode *d = lv_ast_create(LV_AST_INTEGER_LITERAL, loc);
        LvAstNode *c = lv_ast_create(LV_AST_INTEGER_LITERAL, loc);
        LvAstNode *b = lv_ast_create(LV_AST_INTEGER_LITERAL, loc);
        LvAstNode *a = lv_ast_create(LV_AST_INTEGER_LITERAL, loc);
        LvAstNode *cd = lv_ast_create_binary(loc, "+", c, d);
        LvAstNode *bcd = lv_ast_create_binary(loc, "+", b, cd);
        LvAstNode *root = lv_ast_create_binary(loc, "+", a, bcd);
        int depth = root ? lv_ast_max_depth(root) : -1;
        if (depth == 3)
            PASS();
        else
            FAIL("三层二元嵌套深度应为 3");
        lv_ast_destroy(root);
    }

    TEST("lv_load_file 接线: 深括号嵌套被拒绝（上限可配置）");
    {
        /* 构造超过默认 256 的括号嵌套：parse_primary_expr 对括号直接返回
         * 内层表达式（AST 深度测不出），真正的防线是解析器 paren_depth
         * 计数对照 lvConfig.parser.parser_max_ast_depth（可配置，不硬编码）。 */
        const lvConfig *cfg = lv_config_current();
        int limit = cfg->parser.parser_max_ast_depth;
        char deep_src[8192];
        int pos = 0;
        pos += sprintf(deep_src + pos, "Point A, B;\n");
        pos += sprintf(deep_src + pos, "Let deep = ");
        for (int i = 0; i < limit + 2 && pos < (int)sizeof(deep_src) - 16; i++)
            pos += sprintf(deep_src + pos, "(");
        pos += sprintf(deep_src + pos, "1");
        for (int i = 0; i < limit + 2 && pos < (int)sizeof(deep_src) - 8; i++)
            pos += sprintf(deep_src + pos, ")");
        pos += sprintf(deep_src + pos, ";\n");

        /* 走完整解析链：括号深度超限应产生解析错误（paren_depth 闸门） */
        LvParseResult res = parse_source(deep_src);
        if (res.error_count > 0) {
            PASS();
        } else {
            FAIL("深括号嵌套应被解析器拒绝");
        }
        lv_ast_destroy(res.ast);
    }

    TEST("lv_load_file 接线: 常规源码不受影响");
    {
        const char *src = "Point A, B, C;\nSegment S := segment(A, B);\n";
        LvParseResult res = parse_source(src);
        int max_d = (res.ast) ? lv_ast_max_depth(res.ast) : -1;
        if (max_d >= 0 && lv_check_ast_depth(max_d) == lv_OK)
            PASS();
        else
            FAIL("常规源码深度应在上限内");
        lv_ast_destroy(res.ast);
    }
}

/* F16/G1：输入校验闸门 —— lv_load_file 解析链对超长输入/非法字符拒绝 */
static void test_input_validation_gate(void) {
    printf("[输入校验闸门]\n");

    TEST("lv_input_validate: 正常输入通过");
    {
        const char *s = "Point A;\n";
        if (lv_input_validate(s, strlen(s)) == lv_OK)
            PASS();
        else
            FAIL("正常输入应通过");
    }

    TEST("lv_input_validate: NULL 拒绝");
    {
        if (lv_input_validate(NULL, 10) != lv_OK)
            PASS();
        else
            FAIL("NULL 应拒绝");
    }

    TEST("lv_input_validate: 非法控制字符拒绝");
    {
        const char s[] = {'P', 'o', 'i', 'n', 't', ' ', 0x01, '\n'};
        if (lv_input_validate(s, sizeof(s)) != lv_OK)
            PASS();
        else
            FAIL("非法控制字符应拒绝");
    }

    TEST("lv_input_validate: 超长输入拒绝（上限可配置）");
    {
        /* 直接构造超过 parser_max_input_length 的缓冲（不走文件）——
         * 验证 lv_input_validate 对超限长度返回错误 */
        const lvConfig *cfg = lv_config_current();
        int limit = cfg->parser.parser_max_input_length;
        size_t big = (size_t) limit + 1024;
        char *bigbuf = (char *) lv_malloc(big);
        if (bigbuf) {
            memset(bigbuf, 'A', big - 1);
            bigbuf[big - 1] = '\0';
            if (lv_input_validate(bigbuf, big - 1) != lv_OK)
                PASS();
            else
                FAIL("超长输入应拒绝");
            lv_free((void **) &bigbuf);
        } else {
            FAIL("bigbuf 分配失败");
        }
    }

    TEST("lv_ast_node_count: 计数正确");
    {
        const char *src = "Point A, B, C;\nSegment S := segment(A, B);\n";
        LvParseResult res = parse_source(src);
        if (res.ast) {
            int n = lv_ast_node_count(res.ast);
            if (n >= 3) /* program + 2 语句节点起 */
                PASS();
            else
                FAIL("AST 节点计数应 >= 3");
            lv_ast_destroy(res.ast);
        } else {
            FAIL("解析失败");
        }
    }

    TEST("lv_check_ast_node_count: 超限拒绝（上限可配置）");
    {
        const lvConfig *cfg = lv_config_current();
        int limit = cfg->parser.parser_max_ast_nodes;
        if (lv_check_ast_node_count(limit + 1) != lv_OK)
            PASS();
        else
            FAIL("节点数超限应拒绝");
        if (lv_check_ast_node_count(1) == lv_OK)
            PASS();
        else
            FAIL("正常节点数应通过");
    }

    TEST("lv_check_token_length: 超长拒绝（上限可配置）");
    {
        const lvConfig *cfg = lv_config_current();
        int limit = cfg->parser.parser_max_token_length;
        if (lv_check_token_length((size_t) limit + 1) != lv_OK)
            PASS();
        else
            FAIL("token 超长应拒绝");
        if (lv_check_token_length(8) == lv_OK)
            PASS();
        else
            FAIL("正常 token 应通过");
    }
}

TEST_MAIN_BEGIN("lv parser test")
    setvbuf(stdout, NULL, _IONBF, 0);

    TEST_MAIN_RUN(test_ast_creation);
    printf("\n");
    TEST_MAIN_RUN(test_fold_eval_vtable);
    printf("\n");
    TEST_MAIN_RUN(test_declaration);
    printf("\n");
    TEST_MAIN_RUN(test_coord_literal_decl);
    printf("\n");
    TEST_MAIN_RUN(test_auto_named_stmt);
    printf("\n");
    TEST_MAIN_RUN(test_constraint);
    printf("\n");
    TEST_MAIN_RUN(test_prove);
    printf("\n");
    TEST_MAIN_RUN(test_quantifier);
    printf("\n");
    TEST_MAIN_RUN(test_let);
    printf("\n");
    TEST_MAIN_RUN(test_module_import);
    printf("\n");
    TEST_MAIN_RUN(test_normalize);
    printf("\n");
    TEST_MAIN_RUN(test_theorem);
    printf("\n");
    TEST_MAIN_RUN(test_logical_ops);
    printf("\n");
    TEST_MAIN_RUN(test_axiom);
    printf("\n");
    TEST_MAIN_RUN(test_compute_export);
    printf("\n");
    TEST_MAIN_RUN(test_error_recovery);
    printf("\n");
    TEST_MAIN_RUN(test_bool_literal_expr);
    printf("\n");
    TEST_MAIN_RUN(test_spec_extensions);
    printf("\n");
    TEST_MAIN_RUN(test_full_program);
    printf("\n");
    TEST_MAIN_RUN(test_ast_depth_gate);
    printf("\n");
    TEST_MAIN_RUN(test_input_validation_gate); /* F16/G1 */
TEST_MAIN_END()
