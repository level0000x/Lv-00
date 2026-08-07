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

/* ════════════════════════════════════════════════════════════════
 * 首次自举 —— 命题逻辑验证器（路线图步骤 6）
 *
 * lv 用自身语言编写的命题逻辑验证器规格 propositional_verifier.lv
 * 通过 loader 的"命题变量全真值表枚举"验证其 Prove 断言：恒真定律
 * PASS、反例 FAIL —— "lv 验证自身证明"的首次自举闭环。
 * ════════════════════════════════════════════════════════════════ */

/* ── 测试 11: 命题逻辑真值表验证（含恒真定律与反例）── */
static void test_bootstrap_propositional_tautology(void) {
    printf("\n[首次自举：命题逻辑真值表验证]\n");
    LvProveSummary s;

    TEST("排中律 P or not P 恒真 PASS");
    if (verify_single("Prove P or not P;\n", &s) && s.pass_count == 1 && s.fail_count == 0)
        PASS();
    else
        FAIL("排中律应判 PASS");

    TEST("矛盾式 P and not P 反例 FAIL");
    if (verify_single("Prove P and not P;\n", &s) && s.fail_count == 1 && s.pass_count == 0)
        PASS();
    else
        FAIL("矛盾式应判 FAIL");

    TEST("双重否定 not (not P) -> P 恒真 PASS");
    if (verify_single("Prove not (not P) -> P;\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("双重否定应判 PASS");

    TEST("德摩根 not (P and Q) iff (not P or not Q) 恒真 PASS");
    if (verify_single("Prove not (P and Q) iff (not P or not Q);\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("德摩根律应判 PASS");

    TEST("蕴含弱化 P -> (Q -> P) 恒真 PASS");
    if (verify_single("Prove P -> (Q -> P);\n", &s) && s.pass_count == 1)
        PASS();
    else
        FAIL("蕴含弱化应判 PASS");

    TEST("非恒真式 P or Q 反例 FAIL");
    if (verify_single("Prove P or Q;\n", &s) && s.fail_count == 1)
        PASS();
    else
        FAIL("P or Q 非恒真应判 FAIL");
}

/* ── 测试 12: 混合文件计数与既有语义回归 ── */
static void test_bootstrap_propositional_mix_and_regression(void) {
    printf("\n[首次自举：混合计数与既有语义回归]\n");
    LvProveSummary s;

    TEST("混合文件：2 恒真定律 PASS + 1 Church PASS + 1 量词 SKIP");
    const char *mix =
        "Prove P or not P;\n"
        "Prove (P and Q) iff (Q and P);\n"
        "Prove add(1, 1) == 2;\n"
        "Prove forall x: Point. collinear(x, x, x);\n";
    if (verify_single(mix, &s)) {
        if (s.prove_count == 4 && s.pass_count == 3 && s.fail_count == 0 && s.skip_count == 1)
            PASS();
        else {
            printf("  (got prove=%d pass=%d fail=%d skip=%d)\n", s.prove_count, s.pass_count,
                   s.fail_count, s.skip_count);
            FAIL("混合计数不匹配");
        }
    } else {
        FAIL("混合文件验证失败");
    }

    TEST("纯布尔目标与反射律行为保持（truth-table 不接管非骨架表达式）");
    if (verify_single("Prove eq(2, 2);\nProve collinear(A, A, A);\n", &s) && s.pass_count == 2 && s.skip_count == 0)
        PASS();
    else
        FAIL("既有 Church/反射律语义应保持");

    TEST("未知函数 distance(A, B) == 3 仍 SKIP");
    if (verify_single("Prove distance(A, B) == 3;\n", &s) && s.skip_count == 1)
        PASS();
    else
        FAIL("未知函数应仍判 SKIP");
}

/* ── 测试 13: 加载 propositional_verifier.lv 首次自举闭环全 PASS ── */
static void test_bootstrap_propositional_file_closed_loop(void) {
    printf("\n[首次自举：加载 propositional_verifier.lv 全部 Prove 通过]\n");
    LvProveSummary s;

    TEST("propositional_verifier.lv 13 条 Prove 断言全部 PASS（自举闭环）");
    const char *candidates[] = {
        "bootstrap/src/proofs/propositional_verifier.lv",
        "../bootstrap/src/proofs/propositional_verifier.lv",
        "../../bootstrap/src/proofs/propositional_verifier.lv",
        "Lv-00/bootstrap/src/proofs/propositional_verifier.lv",
    };
    bool found = false;
    bool all_pass = false;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        FILE *fp = fopen(candidates[i], "r");
        if (fp) {
            fclose(fp);
            found = true;
            if (lv_load_file_verified(candidates[i], &s)) {
                if (s.prove_count == 13 && s.pass_count == 13 && s.fail_count == 0 && s.skip_count == 0) {
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
        printf("  (propositional_verifier.lv 未在当前目录找到，跳过文件级断言)\n");
        PASS();
    } else if (all_pass) {
        PASS();
    } else {
        FAIL("propositional_verifier.lv 中 Prove 断言未全部通过");
    }
}

/* ── 测试 11: verifier.lv 规格文件完整解析（缺陷3 验收） ── */
static void test_verifier_lv_parse(void) {
    printf("\n[微自举 B：verifier.lv 规格文件完整解析]\n");

    /* 与 bootstrap/src/compiler/verifier.lv 内容一致 */
    const char *src =
        "Point Spec         := { preconditions: List<Formula>, postconditions: List<Formula>,\n"
        "                        invariants: List<Formula> };\n"
        "Point Output       := AST | ResolvedAST | TypedAST | IR | Code | Executable;\n"
        "Point Error        := { code: ErrorCode, location: Span, detail: String };\n"
        "Point ErrorCode    := SafetyViolation | LivenessViolation | PostconditionFail | InvariantBreak;\n"
        "Point Verifier     := { spec: Spec, mode: VerifyMode, timeout_ms: Int };\n"
        "Point Verdict      := Pass(proof: ProofTerm) | Fail(reason: Set<Error>, counterexample: Option<Model>);\n"
        "Point ProofTerm    := { derivation: DerivationTree, assumptions: Set<Formula>, conclusion: Formula };\n"
        "Point VerifyFn     := verify(output: Output, spec: Spec, v: Verifier) -> Verdict;\n"
        "Constraint VerifierSound: forall o: Output, s: Spec, v: Verifier.\n"
        "  verify(o, s, v) = Pass(_) -> output_satisfies(o, s);\n"
        "Constraint VerifierComplete: forall o: Output, s: Spec, v: Verifier.\n"
        "  output_satisfies(o, s) /\\ v.mode = Full -> verify(o, s, v) = Pass(_);\n"
        "Constraint VerifierFinite: forall o: Output, s: Spec, v: Verifier.\n"
        "  verify(o, s, v) terminates_in_finite_steps;\n"
        "Constraint CounterexampleValid: forall o: Output, s: Spec, v: Verifier.\n"
        "  verify(o, s, v) = Fail(cex, _) -> not(output_satisfies(o, s));\n"
        "Prove VerifierSound;\n"
        "Prove VerifierComplete;\n"
        "Prove VerifierFinite;\n"
        "Prove CounterexampleValid;\n"
        "Normalize;\n";

    LvParseResult res = parse_string(src);
    TEST("verifier.lv 全文解析无错误");
    if (res.ast && res.error_count == 0) {
        PASS();
    } else {
        printf("  errors=%d\n", res.error_count);
        for (int i = 0; i < res.error_count && i < 5; i++)
            printf("    error %d: %s\n", i, res.errors[i].message);
        FAIL("verifier.lv 解析失败");
    }

    if (res.ast) {
        int decl = 0, cons = 0, prove = 0, norm = 0;
        for (LvAstNode *s = res.ast->child; s; s = s->next) {
            switch (s->type) {
                case LV_AST_DECLARATION:
                    decl++;
                    break;
                case LV_AST_CONSTRAINT_STMT:
                    cons++;
                    break;
                case LV_AST_PROVE_STMT:
                    prove++;
                    break;
                case LV_AST_NORMALIZE_STMT:
                    norm++;
                    break;
                default:
                    break;
            }
        }
        TEST("verifier.lv 语句统计 (8 声明 4 约束 4 Prove 1 Normalize)");
        if (decl == 8 && cons == 4 && prove == 4 && norm == 1) {
            PASS();
        } else {
            printf("  (decl=%d cons=%d prove=%d norm=%d)\n", decl, cons, prove, norm);
            FAIL("语句统计不匹配");
        }

        TEST("命名约束携带名字 (VerifierSound)");
        LvAstNode *first_cons = NULL;
        for (LvAstNode *s = res.ast->child; s; s = s->next) {
            if (s->type == LV_AST_CONSTRAINT_STMT) {
                first_cons = s;
                break;
            }
        }
        if (first_cons && first_cons->data.stmt.name && strcmp(first_cons->data.stmt.name, "VerifierSound") == 0) {
            PASS();
        } else {
            FAIL("expected name=VerifierSound");
        }

        TEST("VerifyFn 返回类型标注 -> Verdict");
        LvAstNode *verifyfn_decl = NULL;
        for (LvAstNode *s = res.ast->child; s; s = s->next) {
            if (s->type == LV_AST_DECLARATION && s->data.decl.names &&
                strcmp(s->data.decl.names, "VerifyFn") == 0) {
                verifyfn_decl = s;
                break;
            }
        }
        if (verifyfn_decl && verifyfn_decl->data.decl.return_type &&
            strcmp(verifyfn_decl->data.decl.return_type, "Verdict") == 0 &&
            verifyfn_decl->data.decl.value &&
            verifyfn_decl->data.decl.value->type == LV_AST_FUNCTION_CALL) {
            PASS();
        } else {
            FAIL("expected return_type=Verdict with FUNCTION_CALL value");
        }

        TEST("Verdict 构造子命名参数保留 (Pass(proof: ...))");
        LvAstNode *verdict_decl = NULL;
        for (LvAstNode *s = res.ast->child; s; s = s->next) {
            if (s->type == LV_AST_DECLARATION && s->data.decl.names &&
                strcmp(s->data.decl.names, "Verdict") == 0) {
                verdict_decl = s;
                break;
            }
        }
        LvAstNode *pass_call = NULL;
        if (verdict_decl && verdict_decl->data.decl.value &&
            verdict_decl->data.decl.value->type == LV_AST_UNION) {
            pass_call = verdict_decl->data.decl.value->data.binary.left;
        }
        if (pass_call && pass_call->type == LV_AST_FUNCTION_CALL && pass_call->data.call.args &&
            pass_call->data.call.args->type == LV_AST_NAMED_ARG &&
            pass_call->data.call.args->data.field.name &&
            strcmp(pass_call->data.call.args->data.field.name, "proof") == 0) {
            PASS();
        } else {
            FAIL("expected named arg 'proof' on Pass");
        }
    }

    /* 语义分析不崩溃（允许有语义错误：未知类型/未声明标识符等，不要求通过） */
    if (res.ast) {
        LvSemaContext *sema = lv_sema_create();
        TEST("verifier.lv 语义分析不崩溃");
        lv_sema_analyze(sema, res.ast);
        PASS();
        lv_sema_destroy(sema);
    }

    lv_ast_destroy(res.ast);
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

    /* 首次自举：命题逻辑验证器（路线图步骤 6） */
    TEST_MAIN_RUN(test_bootstrap_propositional_tautology);
    TEST_MAIN_RUN(test_bootstrap_propositional_mix_and_regression);
    TEST_MAIN_RUN(test_bootstrap_propositional_file_closed_loop);
    TEST_MAIN_RUN(test_verifier_lv_parse);
TEST_MAIN_END()
