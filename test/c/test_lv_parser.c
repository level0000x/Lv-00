#include <stdio.h>
#include <string.h>

#include "lv/lv_parser.h"

#define TEST(n) printf("  [TEST] %s ... ", n)
#define PASS()            \
    do {                  \
        printf("PASS\n"); \
        P++;              \
    } while (0)
#define FAIL(m)                  \
    do {                         \
        printf("FAIL: %s\n", m); \
        F++;                     \
    } while (0)

static int P = 0, F = 0;

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

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lv parser test ===\n\n");

    test_ast_creation();
    printf("\n");
    test_declaration();
    printf("\n");
    test_constraint();
    printf("\n");
    test_prove();
    printf("\n");
    test_quantifier();
    printf("\n");
    test_let();
    printf("\n");
    test_module_import();
    printf("\n");
    test_normalize();
    printf("\n");
    test_theorem();
    printf("\n");
    test_logical_ops();
    printf("\n");
    test_axiom();
    printf("\n");
    test_compute_export();
    printf("\n");
    test_error_recovery();
    printf("\n");
    test_full_program();

    printf("\n=== %d passed, %d failed ===\n", P, F);
    return F > 0 ? 1 : 0;
}
