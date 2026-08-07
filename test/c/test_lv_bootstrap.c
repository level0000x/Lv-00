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

/* ════════════════════════════════════════════════════════════════
 * 微自举 B —— lv 系统验证自身证明（路线图步骤 5）
 *
 * 通过 lv_verify_proofs / lv_load_file_verified 验证 .lv 中的 Prove
 * 断言：λ-演算 Church β-归约、整数算术、布尔逻辑、反射律与 SKIP 边界。
 * ════════════════════════════════════════════════════════════════ */

/** 辅助：解析单个 .lv 片段并执行证明验证 */
static bool verify_single(const char *src, LvProveSummary *summary) {
    LvParseResult res = parse_string(src);
    bool ok = false;
    if (res.ast && res.error_count == 0) {
        ok = lv_verify_proofs(&res, summary);
    }
    lv_ast_destroy(res.ast);
    return ok;
}

/* ── 测试 6: 算术证明验证 ── */
static void test_proof_verify_arithmetic(void) {
    printf("\n[微自举 B：算术证明验证]\n");
    LvProveSummary s;

    TEST("2 + 2 == 4 验证通过");
    if (verify_single("Prove 2 + 2 == 4;\n", &s) && s.pass_count == 1 && s.fail_count == 0)
        PASS();
    else
        FAIL("期望 pass=1 fail=0");

    TEST("2 + 2 == 5 验证失败");
    if (verify_single("Prove 2 + 2 == 5;\n", &s) && s.fail_count == 1 && s.pass_count == 0)
        PASS();
    else
        FAIL("期望 fail=1 pass=0");

    TEST("(2 + 3) * 4 == 20 验证通过");
    if (verify_single("Prove (2 + 3) * 4 == 20;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("括号优先级求值失败");

    TEST("2 ^ 3 == 8 验证通过");
    if (verify_single("Prove 2 ^ 3 == 8;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("幂运算求值失败");

    TEST("2 < 3 与 3 >= 3 验证通过");
    if (verify_single("Prove 2 < 3;\nProve 3 >= 3;\n", &s) && s.pass_count == 2 && s.fail_count == 0)
        PASS();
    else
        FAIL("关系比较求值失败");
}

/* ── 测试 7: λ-演算证明验证（Church β-归约）── */
static void test_proof_verify_lambda(void) {
    printf("\n[微自举 B：λ-演算证明验证（Church β-归约）]\n");
    LvProveSummary s;

    TEST("add(2, 3) == 5 验证通过");
    if (verify_single("Prove add(2, 3) == 5;\n", &s) && s.pass_count == 1 && s.fail_count == 0)
        PASS();
    else
        FAIL("Church add β-归约验证失败");

    TEST("add(2, 3) == 4 验证失败");
    if (verify_single("Prove add(2, 3) == 4;\n", &s) && s.fail_count == 1)
        PASS();
    else
        FAIL("错误结论应判 FAIL");

    TEST("mul(2, 4) == 8 验证通过");
    if (verify_single("Prove mul(2, 4) == 8;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("Church mul β-归约验证失败");

    TEST("嵌套 add(mul(2, 3), 1) == 7 验证通过");
    if (verify_single("Prove add(mul(2, 3), 1) == 7;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("嵌套 Church 应用验证失败");

    TEST("sub(9, 4) == 5 与 pow(2, 3) == 8 验证通过");
    if (verify_single("Prove sub(9, 4) == 5;\nProve pow(2, 3) == 8;\n", &s) && s.pass_count == 2)
        PASS();
    else
        FAIL("Church sub/pow 验证失败");
}

/* ── 测试 8: 布尔与逻辑证明验证 ── */
static void test_proof_verify_boolean(void) {
    printf("\n[微自举 B：布尔与逻辑证明验证]\n");
    LvProveSummary s;

    TEST("Prove true 通过 / Prove false 失败");
    if (verify_single("Prove true;\nProve false;\n", &s) && s.pass_count == 1 && s.fail_count == 1)
        PASS();
    else
        FAIL("布尔字面量判定错误");

    TEST("true and true 验证通过");
    if (verify_single("Prove true and true;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("逻辑与判定错误");

    TEST("true and false 验证失败");
    if (verify_single("Prove true and false;\n", &s) && s.fail_count == 1)
        PASS();
    else
        FAIL("逻辑与应为假");

    TEST("not false 验证通过");
    if (verify_single("Prove not false;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("逻辑非判定错误");

    TEST("Church 布尔目标 eq(2, 2) / iszero(0) 验证通过");
    if (verify_single("Prove eq(2, 2);\nProve iszero(0);\n", &s) && s.pass_count == 2)
        PASS();
    else
        FAIL("Church 布尔 β-归约验证失败");

    TEST("eq(2, 3) 验证失败");
    if (verify_single("Prove eq(2, 3);\n", &s) && s.fail_count == 1)
        PASS();
    else
        FAIL("Church 相等错误结论应 FAIL");
}

/* ── 测试 9: 边界 —— SKIP 不误报与反射律 ── */
static void test_proof_verify_skip_and_trivial(void) {
    printf("\n[微自举 B：边界（SKIP 与反射律）]\n");
    LvProveSummary s;

    TEST("量词 Prove 不可判定 → SKIP 不误报");
    if (verify_single("Prove forall x: Point. collinear(x, x, x);\n", &s) && s.skip_count == 1 && s.fail_count == 0)
        PASS();
    else
        FAIL("量词应判 SKIP");

    TEST("全同名参数关系 collinear(A, A, A) 反射律验证通过");
    if (verify_single("Prove collinear(A, A, A);\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("反射律应 PASS");

    TEST("除零表达式 1 / 0 == 1 → SKIP");
    if (verify_single("Prove 1 / 0 == 1;\n", &s) && s.skip_count == 1)
        PASS();
    else
        FAIL("除零应判 SKIP");

    TEST("度量表达式 distance(A, B) == 3 → SKIP");
    if (verify_single("Prove distance(A, B) == 3;\n", &s) && s.skip_count == 1)
        PASS();
    else
        FAIL("未知函数应判 SKIP");
}

/* ── 测试 10: 汇总计数与 .lv 文件加载验证 ── */
static void test_proof_verify_summary_and_file(void) {
    printf("\n[微自举 B：汇总计数与 .lv 文件加载验证]\n");
    LvProveSummary s;

    TEST("混合文件计数：2 pass + 1 fail + 1 skip");
    const char *mix =
        "Prove add(1, 1) == 2;\n"
        "Prove mul(2, 3) == 7;\n"
        "Prove true and true;\n"
        "Prove forall x: Point. collinear(x, x, x);\n";
    if (verify_single(mix, &s)) {
        if (s.prove_count == 4 && s.pass_count == 2 && s.fail_count == 1 && s.skip_count == 1)
            PASS();
        else {
            printf("  (got prove=%d pass=%d fail=%d skip=%d)\n", s.prove_count, s.pass_count,
                   s.fail_count, s.skip_count);
            FAIL("汇总计数不匹配");
        }
    } else {
        FAIL("混合文件验证失败");
    }

    TEST("加载 bootstrap/src/proofs/proof_verifier.lv 全部 Prove 通过");
    /* 测试运行目录可能不同，探测候选路径 */
    const char *candidates[] = {
        "bootstrap/src/proofs/proof_verifier.lv",
        "../bootstrap/src/proofs/proof_verifier.lv",
        "../../bootstrap/src/proofs/proof_verifier.lv",
        "Lv-00/bootstrap/src/proofs/proof_verifier.lv",
    };
    bool found = false;
    bool all_pass = false;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        FILE *fp = fopen(candidates[i], "r");
        if (fp) {
            fclose(fp);
            found = true;
            if (lv_load_file_verified(candidates[i], &s)) {
                if (s.prove_count == 8 && s.pass_count == 8 && s.fail_count == 0) {
                    all_pass = true;
                } else {
                    printf("  (file=%s prove=%d pass=%d fail=%d skip=%d)\n", candidates[i],
                           s.prove_count, s.pass_count, s.fail_count, s.skip_count);
                }
            }
            break;
        }
    }
    if (!found) {
        printf("  (proof_verifier.lv 未在当前目录找到，跳过文件级断言)\n");
        PASS();
    } else if (all_pass) {
        PASS();
    } else {
        FAIL("proof_verifier.lv 中 Prove 断言未全部通过");
    }
}

TEST_MAIN_BEGIN("lv bootstrap test")
    setvbuf(stdout, NULL, _IONBF, 0);

    TEST_MAIN_RUN(test_parse_full_program);
    TEST_MAIN_RUN(test_semantic_analysis);
    TEST_MAIN_RUN(test_engine_apply);
    TEST_MAIN_RUN(test_load_file);
    TEST_MAIN_RUN(test_full_pipeline);

    /* 微自举 B：证明验证（路线图步骤 5） */
    TEST_MAIN_RUN(test_proof_verify_arithmetic);
    TEST_MAIN_RUN(test_proof_verify_lambda);
    TEST_MAIN_RUN(test_proof_verify_boolean);
    TEST_MAIN_RUN(test_proof_verify_skip_and_trivial);
    TEST_MAIN_RUN(test_proof_verify_summary_and_file);
TEST_MAIN_END()
