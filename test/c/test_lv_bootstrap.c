#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_ast.h"
#include "lv/lv_lexer.h"
#include "lv/lv_loader.h"
#include "lv/lv_parser.h"
#include "lv/lv_sema.h"

#include "engine.h"
#include "lv.h"

#define TEST_PASS_STATEMENT g_pass_count++
#define TEST_FAIL_STATEMENT g_fail_count++
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ── 辅助：解析字符串并返回结果 ── */
static LvParseResult parse_string(const char *src) {
    LvLexer *lex = lv_lexer_create(src, strlen(src));
    LvParser *parser = lv_parser_create(lex);
    LvParseResult result = lv_parser_parse_program(parser);
    lv_parser_destroy(parser);
    lv_lexer_destroy(lex);
    return result;
}

/* ── 测试 1: 解析完整的 .lv 程序 ── */
static void test_parse_full_program(void) {
    printf("[解析完整 .lv 程序]\n");

    const char *src =
        "Point A, B, C;\n"
        "Constraint collinear(A, B, C);\n"
        "Prove distance(A, B) == distance(A, C);\n";

    LvParseResult res = parse_string(src);
    TEST("解析成功，无错误");
    if (res.ast && res.error_count == 0) {
        PASS();
    } else {
        FAIL("解析失败或存在错误");
        for (int i = 0; i < res.error_count; i++) {
            printf("    error %d: %s\n", i, res.errors[i].message);
        }
    }

    if (res.ast) {
        TEST("AST 结构：3 条语句");
        int count = 0;
        for (LvAstNode *s = res.ast->child; s; s = s->next)
            count++;
        if (count == 3) {
            PASS();
        } else {
            printf("  (got %d, expected 3)\n", count);
            FAIL("语句数量不匹配");
        }

        TEST("第一条语句是 Declaration(Point)");
        LvAstNode *first = res.ast->child;
        if (first && first->type == LV_AST_DECLARATION && first->data.decl.entity_type == LV_ENTITY_POINT) {
            PASS();
        } else {
            FAIL("期望 Declaration(Point)");
        }

        TEST("第二条语句是 Constraint(collinear)");
        LvAstNode *second = first ? first->next : NULL;
        if (second && second->type == LV_AST_CONSTRAINT_STMT) {
            LvAstNode *expr = second->data.stmt.expr;
            if (expr && expr->type == LV_AST_RELATION && strcmp(expr->data.call.func_name, "collinear") == 0) {
                PASS();
            } else {
                FAIL("期望 RELATION(collinear)");
            }
        } else {
            FAIL("期望 CONSTRAINT_STMT");
        }

        TEST("第三条语句是 Prove(compare)");
        LvAstNode *third = second ? second->next : NULL;
        if (third && third->type == LV_AST_PROVE_STMT) {
            LvAstNode *expr = third->data.stmt.expr;
            if (expr && expr->type == LV_AST_COMPARE) {
                PASS();
            } else {
                FAIL("期望 COMPARE 表达式");
            }
        } else {
            FAIL("期望 PROVE_STMT");
        }
    }

    lv_ast_destroy(res.ast);
}

/* ── 测试 2: 语义分析 ── */
static void test_semantic_analysis(void) {
    printf("\n[语义分析]\n");

    /* 2.1 正常程序 */
    {
        const char *src =
            "Point A, B, C;\n"
            "Constraint collinear(A, B, C);\n";
        LvParseResult res = parse_string(src);
        LvSemaContext *sema = lv_sema_create();

        TEST("正常程序语义分析");
        bool ok = lv_sema_analyze(sema, res.ast);
        if (ok && lv_sema_error_count(sema) == 0) {
            PASS();
        } else {
            for (int i = 0; i < lv_sema_error_count(sema); i++) {
                printf("  sema error %d: %s\n", i, lv_sema_error_msg(sema, i));
            }
            FAIL("期望无语义错误");
        }

        lv_sema_destroy(sema);
        lv_ast_destroy(res.ast);
    }

    /* 2.2 未声明的标识符 */
    {
        const char *src = "Constraint collinear(A, B, C);\n";
        LvParseResult res = parse_string(src);
        LvSemaContext *sema = lv_sema_create();

        TEST("未声明标识符检测");
        bool ok = lv_sema_analyze(sema, res.ast);
        if (!ok && lv_sema_error_count(sema) > 0) {
            PASS();
        } else {
            FAIL("期望至少一个语义错误（未声明标识符）");
        }

        lv_sema_destroy(sema);
        lv_ast_destroy(res.ast);
    }

    /* 2.3 重复声明 */
    {
        const char *src = "Point A;\nPoint A;\n";
        LvParseResult res = parse_string(src);
        LvSemaContext *sema = lv_sema_create();

        TEST("重复声明检测");
        bool ok = lv_sema_analyze(sema, res.ast);
        if (!ok && lv_sema_error_count(sema) > 0) {
            PASS();
        } else {
            FAIL("期望重复声明错误");
        }

        lv_sema_destroy(sema);
        lv_ast_destroy(res.ast);
    }

    /* 2.4 量词表达式 */
    {
        const char *src = "Constraint forall x: Point. collinear(x, x, x);\n";
        LvParseResult res = parse_string(src);
        LvSemaContext *sema = lv_sema_create();

        TEST("量词表达式语义分析");
        bool ok = lv_sema_analyze(sema, res.ast);
        if (ok && lv_sema_error_count(sema) == 0) {
            PASS();
        } else {
            for (int i = 0; i < lv_sema_error_count(sema); i++) {
                printf("  sema error %d: %s\n", i, lv_sema_error_msg(sema, i));
            }
            FAIL("量词分析失败");
        }

        lv_sema_destroy(sema);
        lv_ast_destroy(res.ast);
    }

    /* 2.5 逻辑运算 */
    {
        const char *src =
            "Point A, B;\n"
            "Prove collinear(A, A, B) and collinear(B, B, A);\n";
        LvParseResult res = parse_string(src);
        LvSemaContext *sema = lv_sema_create();

        TEST("逻辑运算语义分析");
        bool ok = lv_sema_analyze(sema, res.ast);
        if (ok && lv_sema_error_count(sema) == 0) {
            PASS();
        } else {
            for (int i = 0; i < lv_sema_error_count(sema); i++) {
                printf("  sema error %d: %s\n", i, lv_sema_error_msg(sema, i));
            }
            FAIL("逻辑运算分析失败");
        }

        lv_sema_destroy(sema);
        lv_ast_destroy(res.ast);
    }
}

/* ── 测试 3: 应用到引擎 ── */
static void test_engine_apply(void) {
    printf("\n[应用到引擎]\n");

    const char *src =
        "Point A, B, C;\n"
        "Constraint collinear(A, B, C);\n"
        "Prove distance(A, B) == distance(A, C);\n";

    LvParseResult res = parse_string(src);
    LvSemaContext *sema = lv_sema_create();
    lv_sema_analyze(sema, res.ast);

    lvEngine *engine = engine_create();
    TEST("引擎创建成功");
    if (engine) {
        PASS();
    } else {
        FAIL("引擎创建失败");
    }

    if (engine) {
        TEST("应用解析结果到引擎");
        bool applied = lv_apply_parse_result(engine, &res, sema);
        if (applied) {
            PASS();
        } else {
            FAIL("应用失败");
        }

        TEST("引擎仍在有效状态");
        EngineStatus st = engine_get_last_status(engine);
        if (st == ENGINE_STATUS_OK || st == ENGINE_STATUS_INVALID_STATE) {
            PASS();
        } else {
            printf("  status=%d\n", (int) st);
            FAIL("引擎状态异常");
        }

        engine_destroy(engine);
    }

    lv_sema_destroy(sema);
    lv_ast_destroy(res.ast);
}

/* ── 测试 4: 加载 .lv 文件 ── */
static void test_load_file(void) {
    printf("\n[加载 .lv 文件]\n");

    /* 创建临时 .lv 文件 */
    const char *tmp_path = "_test_temp_bootstrap.lv";
    const char *content =
        "Point X, Y;\n"
        "Constraint collinear(X, X, Y);\n";

    FILE *fp = fopen(tmp_path, "w");
    if (fp) {
        fputs(content, fp);
        fclose(fp);
    }

    TEST("lv_load_file 加载临时文件");
    LvParseResult res = lv_load_file(tmp_path);
    if (res.ast && res.error_count == 0) {
        PASS();
    } else {
        printf("  errors=%d:", res.error_count);
        for (int i = 0; i < res.error_count; i++) {
            printf(" %s", res.errors[i].message);
        }
        printf("\n");
        FAIL("加载文件失败");
    }

    /* 对加载结果进行语义分析 */
    if (res.ast) {
        LvSemaContext *sema = lv_sema_create();
        TEST("从文件加载的 AST 语义分析");
        bool ok = lv_sema_analyze(sema, res.ast);
        if (ok) {
            PASS();
        } else {
            for (int i = 0; i < lv_sema_error_count(sema); i++) {
                printf("  sema error %d: %s\n", i, lv_sema_error_msg(sema, i));
            }
            FAIL("语义分析失败");
        }

        lv_sema_destroy(sema);
        lv_ast_destroy(res.ast);
    }

    /* 清理临时文件 */
    remove(tmp_path);
}

/* ── 测试 5: 完整端到端流程 ── */
static void test_full_pipeline(void) {
    printf("\n[端到端管线]\n");

    const char *src =
        "Point P, Q, R;\n"
        "Constraint collinear(P, Q, R);\n";

    /* 解析 */
    LvParseResult res = parse_string(src);
    TEST("解析阶段");
    if (res.ast && res.error_count == 0) {
        PASS();
    } else {
        FAIL("解析失败");
    }

    /* 语义分析 */
    LvSemaContext *sema = NULL;
    if (res.ast) {
        sema = lv_sema_create();
        TEST("语义分析阶段");
        if (lv_sema_analyze(sema, res.ast)) {
            PASS();
        } else {
            for (int i = 0; i < lv_sema_error_count(sema); i++) {
                printf("  sema error %d: %s\n", i, lv_sema_error_msg(sema, i));
            }
            FAIL("语义分析失败");
        }
    }

    /* 引擎应用 */
    lvEngine *engine = engine_create();
    if (engine && res.ast) {
        TEST("引擎应用阶段");
        bool ok = lv_apply_parse_result(engine, &res, sema);
        if (ok) {
            PASS();
        } else {
            FAIL("引擎应用失败");
        }
    }

    if (sema)
        lv_sema_destroy(sema);
    if (res.ast)
        lv_ast_destroy(res.ast);
    if (engine)
        engine_destroy(engine);
}

TEST_MAIN_BEGIN("lv bootstrap test")
    setvbuf(stdout, NULL, _IONBF, 0);

    TEST_MAIN_RUN(test_parse_full_program);
    TEST_MAIN_RUN(test_semantic_analysis);
    TEST_MAIN_RUN(test_engine_apply);
    TEST_MAIN_RUN(test_load_file);
    TEST_MAIN_RUN(test_full_pipeline);
TEST_MAIN_END()
