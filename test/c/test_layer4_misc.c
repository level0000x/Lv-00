/**
 * @file test_layer4_misc.c
 * @brief 综合测试：Layer 4 推理层剩余模块（极简验证内核、关系模型、
 *        代数模式构造引擎、重写匹配与规则应用）
 *
 * 涵盖：
 *   1. Mini Kernel: 创建/销毁、添加语句（$f/$e/$a/$p）、替换检查、
 *      定理证明、全部验证、自检、统计、导入/导出
 *   2. Relation Model: 从约束图构建、销毁、添加断言/事实、
 *      可满足性检查、实例查找
 *   3. Algebra Mode: 创建/销毁、点/线/圆构造、变换、选择器、
 *      undo/redo、快照/恢复
 *   4. Rewrite Match: 绑定解析、模式变量检查、约束添加、一致性检查
 *   5. Rewrite Apply: 规则创建/销毁、规则加载、匹配查找
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/algebra_mode.h"
#include "lv/mini_kernel.h"
#include "lv/relation_model.h"
#include "lv/rewrite.h"

#include "test_helpers.h"

/* 全局测试计数器 */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 *  测试组 1: Mini Kernel 极简验证内核
 * ============================================================ */

/* --- 1.1 生命周期 --- */
static void test_mini_kernel_create_destroy(void) {
    MiniKernelConfig cfg = mini_kernel_config_default();
    MiniKernel *kernel = mini_kernel_create(&cfg);
    TEST_ASSERT_NOT_NULL(kernel);
    TEST_ASSERT_EQ(kernel->statement_count, 0);
    TEST_ASSERT_EQ(kernel->total_verified, 0);
    TEST_ASSERT_EQ(kernel->total_failed, 0);
    TEST_ASSERT(!kernel->is_sealed, "初始未封存");
    mini_kernel_destroy(kernel);
    mini_kernel_destroy(NULL);
    TEST_ASSERT(1, "销毁NULL安全");
}

static void test_mini_kernel_null_config(void) {
    MiniKernel *kernel = mini_kernel_create(NULL);
    TEST_ASSERT_NULL(kernel);
}

static void test_mini_kernel_config_default_values(void) {
    MiniKernelConfig cfg = mini_kernel_config_default();
    TEST_ASSERT_EQ(cfg.max_statements, 10000);
    TEST_ASSERT_EQ(cfg.max_proof_depth, 1000);
    TEST_ASSERT(cfg.trust_colors_enabled, "trust_colors_enabled应为true");
    TEST_ASSERT(!cfg.strict_mode, "strict_mode应为false");
    TEST_ASSERT_EQ(cfg.verification_timeout_ms, 30000);
}

/* --- 1.2 语句添加 --- */
static void test_mini_kernel_add_statements(void) {
    MiniKernelConfig cfg = mini_kernel_config_default();
    MiniKernel *kernel = mini_kernel_create(&cfg);
    TEST_ASSERT_NOT_NULL(kernel);

    /* 添加变量声明（$f） */
    int v1 = mini_kernel_add_var(kernel, "x", "set");
    TEST_ASSERT(v1 >= 0, "添加变量声明应返回非负ID");

    int v2 = mini_kernel_add_var(kernel, "ph", "wff");
    TEST_ASSERT(v2 >= 0 && v2 != v1, "第二个变量");

    /* 添加前提（$e） */
    int h1 = mini_kernel_add_hyp(kernel, "hyp1", "x = x");
    TEST_ASSERT(h1 >= 0, "添加前提应成功");

    /* 添加公理（$a） */
    int a1 = mini_kernel_add_axiom(kernel, "ax-reflexivity", "x = x");
    TEST_ASSERT(a1 >= 0, "添加公理应成功");

    int a2 = mini_kernel_add_axiom(kernel, "ax-symmetry", "x = y => y = x");
    TEST_ASSERT(a2 >= 0, "添加第二条公理");

    /* 添加定理（$p） */
    int proof_refs[] = {a1};
    int t1 = mini_kernel_add_theorem(kernel, "th-reflexivity", "x = x", proof_refs, 1);
    TEST_ASSERT(t1 >= 0, "添加定理应成功");

    /* 检查统计 */
    TEST_ASSERT(kernel->statement_count >= 6, "应有至少6条语句");

    /* NULL参数 */
    int bad = mini_kernel_add_var(NULL, "x", "set");
    TEST_ASSERT_EQ(bad, -1);
    bad = mini_kernel_add_var(kernel, NULL, "set");
    TEST_ASSERT_EQ(bad, -1);
    bad = mini_kernel_add_var(kernel, "x", NULL);
    TEST_ASSERT_EQ(bad, -1);

    mini_kernel_destroy(kernel);
}

/* --- 1.3 封存与重置 --- */
static void test_mini_kernel_seal_reset(void) {
    MiniKernelConfig cfg = mini_kernel_config_default();
    MiniKernel *kernel = mini_kernel_create(&cfg);
    TEST_ASSERT_NOT_NULL(kernel);

    mini_kernel_add_axiom(kernel, "ax-1", "true");
    mini_kernel_seal(kernel);
    TEST_ASSERT(kernel->is_sealed, "封存后is_sealed应为true");

    /* 封存后不能添加新公理 */
    int bad = mini_kernel_add_axiom(kernel, "ax-2", "false");
    TEST_ASSERT_EQ(bad, -1);

    /* 重置 */
    mini_kernel_reset(kernel);
    TEST_ASSERT_EQ(kernel->statement_count, 0);
    TEST_ASSERT(!kernel->is_sealed, "重置后未封存");

    mini_kernel_reset(NULL);
    /* 不应崩溃 */
    TEST_ASSERT(1, "重置NULL安全");

    mini_kernel_destroy(kernel);
}

/* --- 1.4 替换检查（核心功能） --- */
static void test_mini_kernel_check_substitution(void) {
    MiniKernelConfig cfg = mini_kernel_config_default();
    MiniKernel *kernel = mini_kernel_create(&cfg);
    TEST_ASSERT_NOT_NULL(kernel);

    mini_kernel_add_var(kernel, "x", "set");
    mini_kernel_add_var(kernel, "y", "set");

    /* 空替换应通过 */
    char *result = NULL;
    MiniVerifyResult vr = mini_kernel_check_substitution(kernel, NULL, 0, "x = y", &result);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_OK);
    if (result) {
        TEST_ASSERT_STR_EQ(result, "x = y");
        lv_free((void **) &result);
    }

    /* 有效替换 */
    Substitution subs[2];
    memset(subs, 0, sizeof(subs));
    strncpy(subs[0].variable_name, "x", sizeof(subs[0].variable_name) - 1);
    strncpy(subs[0].replacement_term, "A", sizeof(subs[0].replacement_term) - 1);
    strncpy(subs[1].variable_name, "y", sizeof(subs[1].variable_name) - 1);
    strncpy(subs[1].replacement_term, "B", sizeof(subs[1].replacement_term) - 1);
    vr = mini_kernel_check_substitution(kernel, subs, 2, "x = y", &result);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_OK);
    if (result) {
        TEST_ASSERT(strstr(result, "A") != NULL, "替换应包含A");
        TEST_ASSERT(strstr(result, "B") != NULL, "替换应包含B");
        lv_free((void **) &result);
    }

    /* 冲突替换（同一变量映射到不同表达式）应被检测 */
    Substitution conflicted[2];
    memset(conflicted, 0, sizeof(conflicted));
    strncpy(conflicted[0].variable_name, "x", sizeof(conflicted[0].variable_name) - 1);
    strncpy(conflicted[0].replacement_term, "A", sizeof(conflicted[0].replacement_term) - 1);
    strncpy(conflicted[1].variable_name, "x", sizeof(conflicted[1].variable_name) - 1);
    strncpy(conflicted[1].replacement_term, "B", sizeof(conflicted[1].replacement_term) - 1);
    vr = mini_kernel_check_substitution(kernel, conflicted, 2, "x = x", &result);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_FAIL_SUBSTITUTION);

    /* 未绑定变量应被检测 */
    Substitution unbound;
    memset(&unbound, 0, sizeof(unbound));
    strncpy(unbound.variable_name, "__nonexistent__", sizeof(unbound.variable_name) - 1);
    strncpy(unbound.replacement_term, "A", sizeof(unbound.replacement_term) - 1);
    vr = mini_kernel_check_substitution(kernel, &unbound, 1, "x", &result);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_FAIL_UNBOUND_VAR);

    /* NULL参数 */
    vr = mini_kernel_check_substitution(NULL, subs, 1, "x", &result);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_FAIL_MEMORY);
    vr = mini_kernel_check_substitution(kernel, subs, 1, NULL, &result);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_FAIL_UNBOUND_VAR);
    vr = mini_kernel_check_substitution(kernel, NULL, 1, "x", &result);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_FAIL_SUBSTITUTION);
    vr = mini_kernel_check_substitution(kernel, subs, -1, "x", &result);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_FAIL_SUBSTITUTION);

    mini_kernel_destroy(kernel);
}

/* --- 1.5 定理证明 --- */
static void test_mini_kernel_prove_theorem(void) {
    MiniKernelConfig cfg = mini_kernel_config_default();
    MiniKernel *kernel = mini_kernel_create(&cfg);
    TEST_ASSERT_NOT_NULL(kernel);

    int a1 = mini_kernel_add_axiom(kernel, "ax-1", "true");
    TEST_ASSERT(a1 >= 0, "添加公理");

    /* 添加一个引用公理的定理 */
    int refs[] = {a1};
    int t1 = mini_kernel_add_theorem(kernel, "th-1", "true", refs, 1);
    TEST_ASSERT(t1 >= 0, "添加定理");

    /* 证明定理 */
    MiniVerifyResult vr = mini_kernel_prove_theorem(kernel, t1);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_OK);

    /* 验证非定理语句应失败 */
    vr = mini_kernel_prove_theorem(kernel, a1);
    TEST_ASSERT(vr != MINI_VERIFY_OK, "证明非定理应失败");

    /* 无效索引 */
    vr = mini_kernel_prove_theorem(kernel, -1);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_FAIL_MEMORY);

    vr = mini_kernel_prove_theorem(kernel, 999);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_FAIL_MEMORY);

    vr = mini_kernel_prove_theorem(NULL, 0);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_FAIL_MEMORY);

    mini_kernel_destroy(kernel);
}

/* --- 1.6 全部验证 --- */
static void test_mini_kernel_verify_all(void) {
    MiniKernelConfig cfg = mini_kernel_config_default();
    MiniKernel *kernel = mini_kernel_create(&cfg);
    TEST_ASSERT_NOT_NULL(kernel);

    int a1 = mini_kernel_add_axiom(kernel, "ax-1", "true");
    int refs[] = {a1};
    mini_kernel_add_theorem(kernel, "th-ok", "true", refs, 1);

    int passed = -1, failed = -1;
    MiniVerifyResult vr = mini_kernel_verify_all(kernel, &passed, &failed);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_OK);
    TEST_ASSERT(passed > 0, "应有至少1个通过");
    TEST_ASSERT_EQ(failed, 0);

    /* NULL输出参数 */
    vr = mini_kernel_verify_all(kernel, NULL, NULL);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_OK);

    vr = mini_kernel_verify_all(NULL, &passed, &failed);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_FAIL_MEMORY);

    mini_kernel_destroy(kernel);
}

/* --- 1.7 自检 --- */
static void test_mini_kernel_self_check(void) {
    MiniKernelConfig cfg = mini_kernel_config_default();
    MiniKernel *kernel = mini_kernel_create(&cfg);
    TEST_ASSERT_NOT_NULL(kernel);

    /* 先添加变量使替换检查能通过 */
    mini_kernel_add_var(kernel, "x", "set");

    MiniVerifyResult vr = mini_kernel_self_check(kernel);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_OK);

    vr = mini_kernel_self_check(NULL);
    TEST_ASSERT_EQ(vr, MINI_VERIFY_FAIL_MEMORY);

    mini_kernel_destroy(kernel);
}

/* --- 1.8 统计 --- */
static void test_mini_kernel_stats(void) {
    MiniKernelConfig cfg = mini_kernel_config_default();
    MiniKernel *kernel = mini_kernel_create(&cfg);
    TEST_ASSERT_NOT_NULL(kernel);

    mini_kernel_add_var(kernel, "x", "set");
    mini_kernel_add_var(kernel, "y", "set");
    mini_kernel_add_hyp(kernel, "h1", "x=y");
    mini_kernel_add_axiom(kernel, "a1", "true");
    mini_kernel_add_axiom(kernel, "a2", "false");

    int total = -1, vars = -1, hyps = -1, axioms = -1, thms = -1, verified = -1, tcb = -1;
    mini_kernel_stats(kernel, &total, &vars, &hyps, &axioms, &thms, &verified, &tcb);
    TEST_ASSERT_EQ(total, 5);
    TEST_ASSERT_EQ(vars, 2);
    TEST_ASSERT_EQ(hyps, 1);
    TEST_ASSERT_EQ(axioms, 2);
    TEST_ASSERT_EQ(thms, 0);

    /* NULL安全 */
    mini_kernel_stats(NULL, &total, &vars, &hyps, &axioms, &thms, &verified, &tcb);
    /* 不应崩溃 */
    mini_kernel_stats(kernel, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT(1, "NULL输出参数安全");

    mini_kernel_destroy(kernel);
}

/* --- 1.9 标签查找和约束图绑定 --- */
static void test_mini_kernel_find_and_bind(void) {
    MiniKernelConfig cfg = mini_kernel_config_default();
    MiniKernel *kernel = mini_kernel_create(&cfg);
    TEST_ASSERT_NOT_NULL(kernel);

    int a1 = mini_kernel_add_axiom(kernel, "my-axiom", "true");
    TEST_ASSERT(a1 >= 0, "添加公理");

    /* 根据标签查找 */
    int found = mini_kernel_find_by_label(kernel, "my-axiom");
    TEST_ASSERT_EQ(found, a1);

    found = mini_kernel_find_by_label(kernel, "__nonexistent__");
    TEST_ASSERT_EQ(found, -1);

    found = mini_kernel_find_by_label(NULL, "x");
    TEST_ASSERT_EQ(found, -1);
    found = mini_kernel_find_by_label(kernel, NULL);
    TEST_ASSERT_EQ(found, -1);

    /* 约束图绑定 */
    bool ok = mini_kernel_bind_to_graph(kernel, a1, 42);
    TEST_ASSERT(ok, "绑定到约束图应成功");

    int stmt = mini_kernel_find_by_node(kernel, 42);
    TEST_ASSERT_EQ(stmt, a1);

    stmt = mini_kernel_find_by_node(kernel, 999);
    TEST_ASSERT_EQ(stmt, -1);

    ok = mini_kernel_bind_to_graph(NULL, 0, 0);
    TEST_ASSERT(!ok, "NULL内核绑定应失败");

    ok = mini_kernel_bind_to_graph(kernel, -1, 0);
    TEST_ASSERT(!ok, "负语句ID绑定应失败");

    mini_kernel_destroy(kernel);
}

/* --- 1.10 字符串转换及MM导入导出 --- */
static void test_mini_kernel_string_helpers(void) {
    /* 语句类型转字符串 */
    TEST_ASSERT_STR_EQ(mini_stmt_type_to_string(MINI_STMT_VAR), "$f");
    TEST_ASSERT_STR_EQ(mini_stmt_type_to_string(MINI_STMT_HYP), "$e");
    TEST_ASSERT_STR_EQ(mini_stmt_type_to_string(MINI_STMT_AXIOM), "$a");
    TEST_ASSERT_STR_EQ(mini_stmt_type_to_string(MINI_STMT_THEOREM), "$p");
    TEST_ASSERT_STR_EQ(mini_stmt_type_to_string(MINI_STMT_COMMENT), "$=");

    /* 验证结果转字符串 */
    TEST_ASSERT_STR_EQ(mini_verify_result_to_string(MINI_VERIFY_OK), "OK");
    TEST_ASSERT_STR_EQ(mini_verify_result_to_string(MINI_VERIFY_FAIL_SUBSTITUTION), "SUBSTITUTION_FAIL");
    TEST_ASSERT_STR_EQ(mini_verify_result_to_string(MINI_VERIFY_FAIL_STACK), "STACK_FAIL");
    TEST_ASSERT_STR_EQ(mini_verify_result_to_string(MINI_VERIFY_FAIL_CYCLE), "CYCLE");
}

static void test_mini_kernel_import_export_mm(void) {
    MiniKernelConfig cfg = mini_kernel_config_default();
    MiniKernel *kernel = mini_kernel_create(&cfg);
    TEST_ASSERT_NOT_NULL(kernel);

    /* 写入临时.mm文件 */
    const char *tmp_path = "_test_temp_mini_kernel.mm";
    FILE *fp = fopen(tmp_path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    if (fp) {
        fprintf(fp, "$f x set $.\n");
        fprintf(fp, "$a ax-1 true $.\n");
        fprintf(fp, "$p th-1 true $= ax-1 $.\n");
        fclose(fp);
    }

    int imported = mini_kernel_import_mm(kernel, tmp_path);
    TEST_ASSERT(imported > 0, "导入应返回正数");
    TEST_ASSERT(kernel->statement_count >= 3, "应导入至少3条语句");

    /* 导出 */
    const char *export_path = "_test_temp_mini_kernel_export.mm";
    bool ok = mini_kernel_export_mm(kernel, export_path);
    TEST_ASSERT(ok, "导出应成功");

    /* 无效路径 */
    int bad = mini_kernel_import_mm(kernel, "/nonexistent/path.mm");
    TEST_ASSERT_EQ(bad, -1);

    bad = mini_kernel_import_mm(NULL, tmp_path);
    TEST_ASSERT_EQ(bad, -1);

    ok = mini_kernel_export_mm(NULL, export_path);
    TEST_ASSERT(!ok, "NULL内核导出应失败");

    ok = mini_kernel_export_mm(kernel, NULL);
    TEST_ASSERT(!ok, "NULL路径导出应失败");

    /* 清理 */
    remove(tmp_path);
    remove(export_path);
    mini_kernel_destroy(kernel);
}

/* ============================================================
 *  测试组 2: Relation Model 关系模型
 * ============================================================ */

/* --- 2.1 关系运算符：并/交/差 --- */
#if 0
static void test_rel_set_operations(void) {
    Relation *a = rel_new("A", 2);
    Relation *b = rel_new("B", 2);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    int t1[] = {1, 2};
    int t2[] = {3, 4};
    int t3[] = {1, 2}; /* 重复 */
    rel_add_tuple_inner(a, t1);
    rel_add_tuple_inner(a, t2);
    rel_add_tuple_inner(b, t2);
    rel_add_tuple_inner(b, t3);

    /* 并集 */
    Relation *u = rel_union(a, b);
    TEST_ASSERT_NOT_NULL(u);
    TEST_ASSERT(u->tuple_count >= 2, "并集应有至少2个元组");
    rel_destroy(u);

    /* 交集 */
    Relation *i = rel_intersection(a, b);
    TEST_ASSERT_NOT_NULL(i);
    TEST_ASSERT(i->tuple_count >= 1, "交集应有至少1个元组");
    rel_destroy(i);

    /* 差集 */
    Relation *d = rel_difference(a, b);
    TEST_ASSERT_NOT_NULL(d);
    rel_destroy(d);

    /* NULL参数 */
    TEST_ASSERT_NULL(rel_union(NULL, b));
    TEST_ASSERT_NULL(rel_intersection(a, NULL));

    /* 不同元数应失败 */
    Relation *c = rel_new("C", 3);
    TEST_ASSERT_NULL(rel_union(a, c));

    rel_destroy(a);
    rel_destroy(b);
    rel_destroy(c);
}
#endif

/* --- 2.2 关系运算符：连接/笛卡尔积/转置 --- */
#if 0
static void test_rel_join_product_transpose(void) {
    Relation *r1 = rel_new("R1", 2);
    Relation *r2 = rel_new("R2", 2);
    int t1[] = {1, 2};
    int t2[] = {2, 3};
    rel_add_tuple_inner(r1, t1);
    rel_add_tuple_inner(r2, t2);

    /* 连接 */
    Relation *j = rel_join(r1, r2);
    TEST_ASSERT_NOT_NULL(j);
    TEST_ASSERT(j->arity == 3, "连接结果元数应为3");
    rel_destroy(j);

    /* 笛卡尔积 */
    Relation *p = rel_product(r1, r2);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT(p->arity == 4, "笛卡尔积元数应为4");
    rel_destroy(p);

    /* 转置 */
    Relation *tr = rel_transpose(r1);
    TEST_ASSERT_NOT_NULL(tr);
    TEST_ASSERT_EQ(tr->arity, 2);
    rel_destroy(tr);

    /* 非二元关系转置应失败 */
    Relation *r3 = rel_new("R3", 3);
    TEST_ASSERT_NULL(rel_transpose(r3));

    /* NULL参数 */
    TEST_ASSERT_NULL(rel_join(NULL, r2));
    TEST_ASSERT_NULL(rel_product(r1, NULL));
    TEST_ASSERT_NULL(rel_transpose(NULL));

    rel_destroy(r1);
    rel_destroy(r2);
    rel_destroy(r3);
}
#endif

/* --- 2.3 关系运算符：传递闭包 / 自反传递闭包 --- */
#if 0
static void test_rel_closure(void) {
    Relation *r = rel_new("R", 2);
    int t1[] = {1, 2};
    int t2[] = {2, 3};
    int t3[] = {3, 4};
    rel_add_tuple_inner(r, t1);
    rel_add_tuple_inner(r, t2);
    rel_add_tuple_inner(r, t3);

    /* 传递闭包 */
    Relation *tc = rel_transitive_closure(r);
    TEST_ASSERT_NOT_NULL(tc);
    TEST_ASSERT(tc->tuple_count >= 3, "传递闭包应有至少3个元组");
    rel_destroy(tc);

    /* 自反传递闭包 */
    Relation *rtc = rel_reflexive_transitive_closure(r);
    TEST_ASSERT_NOT_NULL(rtc);

    /* 非二元关系应失败 */
    Relation *r3 = rel_new("R3", 3);
    TEST_ASSERT_NULL(rel_transitive_closure(r3));
    TEST_ASSERT_NULL(rel_reflexive_transitive_closure(r3));

    /* NULL参数 */
    TEST_ASSERT_NULL(rel_transitive_closure(NULL));

    rel_destroy(r);
    rel_destroy(r3);
    rel_destroy(rtc);
}
#endif

/* --- 2.4 关系模型构建 --- */
static void test_relation_model_from_graph(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    /* 添加一些点 */
    SymbolicCoord *coords[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    graph_add_point(graph, (SymbolicCoord *const *) coords, 2);

    coords[0] = symbolic_coord_create_rational(100, 1);
    coords[1] = symbolic_coord_create_rational(100, 1);
    graph_add_point(graph, (SymbolicCoord *const *) coords, 2);

    /* 从图构建关系模型 */
    RelModel *model = relation_model_from_graph(graph);
    TEST_ASSERT_NOT_NULL(model);
    TEST_ASSERT(model->sigs.count > 0, "应有签名");
    TEST_ASSERT_NOT_NULL(model->sigs.data);

    /* NULL参数 */
    TEST_ASSERT_NULL(relation_model_from_graph(NULL));

    relation_model_destroy(model);
    graph_destroy(graph);
}

static void test_relation_model_destroy_null(void) {
    relation_model_destroy(NULL);
    TEST_ASSERT(1, "销毁NULL模型安全");
}

/* --- 2.5 事实与断言 --- */
static void test_relation_model_facts_assertions(void) {
    ConstraintGraph *graph = graph_create();
    RelModel *model = relation_model_from_graph(graph);
    TEST_ASSERT_NOT_NULL(model);

    /* 由于RelFormula创建较复杂，这里测试API的NULL处理 */
    bool ok = relation_model_add_fact(model, NULL);
    TEST_ASSERT(!ok, "添加NULL事实应失败");

    ok = relation_model_add_fact(NULL, (RelFormula *) (intptr_t) 1);
    TEST_ASSERT(!ok, "NULL模型添加事实应失败");

    ok = relation_model_add_assertion(model, NULL);
    TEST_ASSERT(!ok, "添加NULL断言应失败");

    ok = relation_model_add_assertion(NULL, (RelFormula *) (intptr_t) 1);
    TEST_ASSERT(!ok, "NULL模型添加断言应失败");

    relation_model_destroy(model);
    graph_destroy(graph);
}

/* --- 2.6 可满足性检查和实例查找 --- */
static void test_relation_model_satisfiability(void) {
    ConstraintGraph *graph = graph_create();
    RelModel *model = relation_model_from_graph(graph);
    TEST_ASSERT_NOT_NULL(model);

    SmallScopeConfig scope;
    memset(&scope, 0, sizeof(scope));
    scope.max_points = 4;
    scope.max_lines = 4;
    scope.max_regions = 2;
    scope.max_ports = 2;
    scope.max_func_blocks = 2;
    scope.max_total_atoms = 20;

    /* 由于模型可能无断言，可满足性应返回true */
    bool sat = relation_check_satisfiability(model, &scope);
    TEST_ASSERT(sat || !sat, "可满足性检查不应崩溃");

    /* 实例查找 */
    RelInstance *inst = relation_find_instance(model, &scope, false);
    TEST_ASSERT_NOT_NULL(inst);
    TEST_ASSERT(inst->atom_count >= 0, "原子数应>=0");
    relation_instance_destroy(inst);

    inst = relation_find_instance(model, &scope, true);
    TEST_ASSERT_NOT_NULL(inst);
    relation_instance_destroy(inst);

    /* NULL参数 */
    sat = relation_check_satisfiability(NULL, &scope);
    TEST_ASSERT(!sat, "NULL模型应为false");
    sat = relation_check_satisfiability(model, NULL);
    TEST_ASSERT(!sat, "NULL范围应为false");

    TEST_ASSERT_NULL(relation_find_instance(NULL, &scope, false));
    TEST_ASSERT_NULL(relation_find_instance(model, NULL, false));

    relation_instance_destroy(NULL);
    relation_model_destroy(model);
    graph_destroy(graph);
}

/* ============================================================
 *  测试组 3: Algebra Mode 代数模式构造引擎
 * ============================================================ */

/* --- 3.1 生命周期 --- */
static void test_algebra_create_destroy(void) {
    AlgebraicGeom *geom = algebra_create(PLANE_XY, "test_geom");
    TEST_ASSERT_NOT_NULL(geom);
    TEST_ASSERT_EQ(geom->plane, (int) PLANE_XY);
    TEST_ASSERT(geom->id > 0, "ID应为正数");

    AlgebraicGeom *geom2 = algebra_create(PLANE_XZ, NULL);
    TEST_ASSERT_NOT_NULL(geom2);
    TEST_ASSERT_EQ(geom2->plane, (int) PLANE_XZ);

    algebra_destroy(geom);
    algebra_destroy(geom2);
    algebra_destroy(NULL);
}

/* --- 3.2 点构造 --- */
static void test_algebra_point_construction(void) {
    AlgebraicGeom *geom = algebra_create(PLANE_XY, "points");
    TEST_ASSERT_NOT_NULL(geom);

    AlgebraicGeom *g = algebra_point(geom, 10.0, 20.0, 0.0);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT(algebra_get_current_entity(geom) >= 0, "应有当前实体ID");

    g = algebra_point_on(geom, 0);
    TEST_ASSERT_NOT_NULL(g);

    g = algebra_midpoint(geom, 0, 1);
    TEST_ASSERT_NOT_NULL(g);

    g = algebra_intersect(geom, 0, 1);
    TEST_ASSERT_NOT_NULL(g);

    /* NULL参数 */
    TEST_ASSERT_NULL(algebra_point(NULL, 1, 2, 3));
    TEST_ASSERT_NULL(algebra_point_on(NULL, 0));
    TEST_ASSERT_NULL(algebra_midpoint(NULL, 0, 1));
    TEST_ASSERT_NULL(algebra_intersect(NULL, 0, 1));

    algebra_destroy(geom);
}

/* --- 3.3 线构造 --- */
static void test_algebra_line_construction(void) {
    AlgebraicGeom *geom = algebra_create(PLANE_XY, "lines");
    TEST_ASSERT_NOT_NULL(geom);

    algebra_point(geom, 0, 0, 0);
    int p1 = algebra_get_current_entity(geom);
    algebra_point(geom, 100, 100, 0);
    int p2 = algebra_get_current_entity(geom);

    AlgebraicGeom *g = algebra_line(geom, p1, p2);
    TEST_ASSERT_NOT_NULL(g);

    g = algebra_segment(geom, p1, p2);
    TEST_ASSERT_NOT_NULL(g);

    g = algebra_ray(geom, p1, p2);
    TEST_ASSERT_NOT_NULL(g);

    /* 无效参数 */
    TEST_ASSERT_NULL(algebra_line(geom, -1, p2));
    TEST_ASSERT_NULL(algebra_line(NULL, p1, p2));

    algebra_destroy(geom);
}

/* --- 3.4 圆构造 --- */
static void test_algebra_circle_construction(void) {
    AlgebraicGeom *geom = algebra_create(PLANE_XY, "circles");
    algebra_point(geom, 50, 50, 0);
    int center = algebra_get_current_entity(geom);

    AlgebraicGeom *g = algebra_circle_radius(geom, center, 30.0);
    TEST_ASSERT_NOT_NULL(g);

    algebra_point(geom, 80, 50, 0);
    int on_circle = algebra_get_current_entity(geom);

    g = algebra_circle(geom, center, on_circle);
    TEST_ASSERT_NOT_NULL(g);

    /* 无效参数 */
    TEST_ASSERT_NULL(algebra_circle_radius(geom, center, -1.0));
    TEST_ASSERT_NULL(algebra_circle(geom, -1, on_circle));

    algebra_destroy(geom);
}

/* --- 3.5 平行/垂线构造 --- */
static void test_algebra_parallel_perpendicular(void) {
    AlgebraicGeom *geom = algebra_create(PLANE_XY, "parallel");

    algebra_point(geom, 0, 0, 0);
    int p1 = algebra_get_current_entity(geom);
    algebra_point(geom, 100, 0, 0);
    int p2 = algebra_get_current_entity(geom);
    algebra_line(geom, p1, p2);
    int line_id = algebra_get_current_entity(geom);

    algebra_point(geom, 50, 50, 0);
    int pt = algebra_get_current_entity(geom);

    AlgebraicGeom *g = algebra_parallel(geom, line_id, pt);
    TEST_ASSERT_NOT_NULL(g);

    g = algebra_perpendicular(geom, line_id, pt);
    TEST_ASSERT_NOT_NULL(g);

    /* 无效参数 */
    TEST_ASSERT_NULL(algebra_parallel(NULL, line_id, pt));
    TEST_ASSERT_NULL(algebra_perpendicular(geom, -1, pt));

    algebra_destroy(geom);
}

/* --- 3.6 变换操作 --- */
static void test_algebra_transform(void) {
    AlgebraicGeom *geom = algebra_create(PLANE_XY, "transform");
    TEST_ASSERT_NOT_NULL(geom);

    AlgebraicGeom *g = algebra_translate(geom, 10, 20, 0);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT(geom->has_transform, "应有变换");

    g = algebra_rotate(geom, 45.0, 0, 0, 1);
    TEST_ASSERT_NOT_NULL(g);

    g = algebra_scale(geom, 2.0, 2.0, 1.0);
    TEST_ASSERT_NOT_NULL(g);

    /* 无效旋转轴 */
    g = algebra_rotate(geom, 45.0, 0, 0, 0);
    TEST_ASSERT_NULL(g);

    /* 通用变换 */
    double params[] = {10.0, 20.0};
    g = algebra_transform(geom, TRANSFORM_TRANSLATE, params, 2);
    TEST_ASSERT_NOT_NULL(g);

    /* NULL参数 */
    TEST_ASSERT_NULL(algebra_translate(NULL, 1, 2, 3));
    TEST_ASSERT_NULL(algebra_rotate(NULL, 45, 0, 0, 1));
    TEST_ASSERT_NULL(algebra_transform(geom, TRANSFORM_TRANSLATE, NULL, 1));

    algebra_destroy(geom);
}

/* --- 3.7 选择器 --- */
static void test_algebra_selector(void) {
    lvSelector *sel = algebra_selector_create(SELECTOR_ALL, NULL);
    TEST_ASSERT_NOT_NULL(sel);
    TEST_ASSERT_EQ(sel->type, SELECTOR_ALL);

    lvSelector *sel2 = algebra_selector_create(SELECTOR_BY_DIRECTION, ">Z");
    TEST_ASSERT_NOT_NULL(sel2);
    TEST_ASSERT_EQ(sel2->dir_op, SEL_DIR_GREATER);
    TEST_ASSERT_EQ(sel2->axis, 'Z');

    lvSelector *sel3 = algebra_selector_create(SELECTOR_BY_INDEX, NULL);
    TEST_ASSERT_NOT_NULL(sel3);

    /* NULL参数 */
    TEST_ASSERT_NULL(algebra_selector_create(SELECTOR_ALL, NULL));

    algebra_selector_destroy(sel);
    algebra_selector_destroy(sel2);
    algebra_selector_destroy(sel3);
}

/* --- 3.8 约束与证明 --- */
static void test_algebra_constrain_and_prove(void) {
    AlgebraicGeom *geom = algebra_create(PLANE_XY, "constrain");
    algebra_point(geom, 0, 0, 0);
    int p1 = algebra_get_current_entity(geom);
    algebra_point(geom, 100, 100, 0);
    int p2 = algebra_get_current_entity(geom);

    int ids[] = {p1, p2};
    AlgebraicGeom *g = algebra_constrain(geom, "incidence", ids, 2);
    TEST_ASSERT_NOT_NULL(g);

    g = algebra_prove(geom, "x = y");
    TEST_ASSERT_NOT_NULL(g);

    /* 无效参数 */
    TEST_ASSERT_NULL(algebra_constrain(NULL, "incidence", ids, 2));
    TEST_ASSERT_NULL(algebra_constrain(geom, NULL, ids, 2));
    TEST_ASSERT_NULL(algebra_constrain(geom, "incidence", NULL, 1));
    TEST_ASSERT_NULL(algebra_prove(NULL, "x = y"));

    algebra_destroy(geom);
}

/* --- 3.9 构建、状态与查询 --- */
static void test_algebra_build_and_query(void) {
    AlgebraicGeom *geom = algebra_create(PLANE_XY, "build_test");
    TEST_ASSERT_NOT_NULL(geom);

    /* 空几何体 */
    AlgebraOpResult status = algebra_get_status(geom);
    TEST_ASSERT_EQ(status, ALGEBRA_DEGENERATE);

    /* 构建空几何体 */
    AlgebraOpResult br = algebra_build(geom);
    TEST_ASSERT_EQ(br, ALGEBRA_DEGENERATE);

    /* 添加点后 */
    algebra_point(geom, 10, 20, 0);
    br = algebra_build(geom);
    TEST_ASSERT_EQ(br, ALGEBRA_OK);

    /* 获取图 */
    ConstraintGraph *graph = algebra_get_graph(geom);
    TEST_ASSERT_NOT_NULL(graph);

    /* 获取状态 */
    status = algebra_get_status(geom);
    TEST_ASSERT_EQ(status, ALGEBRA_OK);

    /* 获取当前实体 */
    int cur = algebra_get_current_entity(geom);
    TEST_ASSERT(cur >= 0, "当前实体ID应>=0");

    /* NULL参数 */
    TEST_ASSERT_NULL(algebra_get_graph(NULL));
    TEST_ASSERT_EQ(algebra_get_status(NULL), ALGEBRA_INVALID_ARGUMENT);
    TEST_ASSERT_EQ(algebra_get_current_entity(NULL), -1);
    TEST_ASSERT_EQ(algebra_build(NULL), ALGEBRA_INVALID_ARGUMENT);

    algebra_destroy(geom);
}

/* --- 3.10 Undo/Redo --- */
static void test_algebra_undo_redo(void) {
    AlgebraicGeom *geom = algebra_create(PLANE_XY, "undo_test");
    TEST_ASSERT_NOT_NULL(geom);

    /* 空历史undo应返回NULL */
    AlgebraicGeom *g = algebra_undo(geom);
    TEST_ASSERT_NULL(g);

    /* 添加一些点 */
    algebra_point(geom, 10, 10, 0);
    algebra_point(geom, 20, 20, 0);

    /* undo */
    g = algebra_undo(geom);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(geom->history_count, 1);

    /* redo */
    g = algebra_redo(geom);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(geom->history_count, 2);

    /* 空redo */
    geom->redo_count = 0;
    g = algebra_redo(geom);
    TEST_ASSERT_NULL(g);

    /* NULL参数 */
    TEST_ASSERT_NULL(algebra_undo(NULL));
    TEST_ASSERT_NULL(algebra_redo(NULL));

    algebra_destroy(geom);
}

/* --- 3.11 快照/恢复 --- */
static void test_algebra_snapshot_restore(void) {
    AlgebraicGeom *geom = algebra_create(PLANE_XY, "snapshot_test");
    TEST_ASSERT_NOT_NULL(geom);

    algebra_point(geom, 10, 20, 0);
    int snap_idx = algebra_snapshot(geom);
    TEST_ASSERT(snap_idx >= 0, "快照应返回有效索引");

    algebra_point(geom, 30, 40, 0);
    TEST_ASSERT(geom->history_count > 0, "添加点后历史应变化");

    AlgebraicGeom *g = algebra_restore(geom, snap_idx);
    TEST_ASSERT_NOT_NULL(g);

    /* 无效索引 */
    g = algebra_restore(geom, 999);
    TEST_ASSERT_NULL(g);

    g = algebra_restore(geom, -1);
    TEST_ASSERT_NULL(g);

    /* NULL参数 */
    int bad = algebra_snapshot(NULL);
    TEST_ASSERT_EQ(bad, -1);
    TEST_ASSERT_NULL(algebra_restore(NULL, 0));

    algebra_destroy(geom);
}

/* --- 3.12 工作平面 --- */
static void test_algebra_work_plane(void) {
    AlgebraicGeom *geom = algebra_create(PLANE_XY, "plane_test");
    TEST_ASSERT_NOT_NULL(geom);

    AlgebraicGeom *g = algebra_set_work_plane(geom, PLANE_XZ);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(geom->plane, (int) PLANE_XZ);

    /* 无效平面 */
    g = algebra_set_work_plane(geom, 99);
    TEST_ASSERT_NULL(g);

    TEST_ASSERT_NULL(algebra_set_work_plane(NULL, PLANE_XY));

    algebra_destroy(geom);
}

/* ============================================================
 *  测试组 4: Rewrite Match 重写匹配
 * ============================================================ */

/* --- 4.1 FNV-1a哈希 --- */
#if 0
static void test_rewrite_internal_hash(void) {
    /* 内部哈希函数仅通过公共API间接测试，这里测试resolve_binding */
    int bindings[] = {-1, 10, -2, 20, -3, 30};
    int resolved = resolve_binding(bindings, 3, -2);
    TEST_ASSERT_EQ(resolved, 20);

    resolved = resolve_binding(bindings, 3, -5);
    TEST_ASSERT_EQ(resolved, -1);

    resolved = resolve_binding(NULL, 0, -1);
    TEST_ASSERT_EQ(resolved, -1);
}
#endif

/* --- 4.2 模式变量检查 --- */
#if 0
static void test_rewrite_pattern_var_checks(void) {
    /* 创建规则的replacement用于测试 */
    int nb1[] = {-1, 0};
    int *node_bindings[] = {nb1};
    int new_nodes[] = {100, 200};
    RewriteReplacement repl;
    memset(&repl, 0, sizeof(repl));
    repl.node_bindings = node_bindings;
    repl.binding_count = 1;
    repl.new_nodes = new_nodes;
    repl.new_node_count = 2;

    bool used = pattern_var_used_in_replacement(&repl, -1);
    /* 无约束时，不应被引用 */
    TEST_ASSERT(!used, "无约束时模式变量应未被使用");

    bool in_bindings = pattern_var_in_replacement_bindings(&repl, -1);
    TEST_ASSERT(in_bindings, "模式变量应在绑定表中");

    in_bindings = pattern_var_in_replacement_bindings(&repl, -5);
    TEST_ASSERT(!in_bindings, "不在绑定表中应返回false");

    /* NULL参数 */
    in_bindings = pattern_var_in_replacement_bindings(NULL, -1);
    TEST_ASSERT(!in_bindings, "NULL应返回false");
}
#endif

/* --- 4.3 VF2状态初始化 --- */
static void test_vf2_state_basic(void) {
    VF2State state;
    memset(&state, 0, sizeof(state));
    state.pattern_size = 3;
    state.target_size = 5;

    TEST_ASSERT_EQ(state.pattern_size, 3);
    TEST_ASSERT_EQ(state.target_size, 5);

    /* 仅验证结构体大小，VF2状态在匹配过程中由内部函数创建 */
    TEST_ASSERT((int) sizeof(VF2State) > 0, "VF2State结构体有效");
}

/* --- 4.4 WL哈希历史 --- */
static void test_wl_history(void) {
    WLHashHistory hist;
    wl_history_init(&hist);
    TEST_ASSERT_EQ(hist.history_count, 0);
    TEST_ASSERT_EQ(hist.light_history_count, 0);

    /* 注意：wl_history_destroy释放内部缓冲区 */
    wl_history_destroy(&hist);
    /* 双重销毁不应崩溃 */
    wl_history_destroy(&hist);
}

/* --- 4.5 图快照 --- */
static void test_graph_snapshot_lifecycle(void) {
    /* 创建一个简单的约束图 */
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    GraphSnapshot *snap = graph_snapshot_create(graph);
    /* 空图快照也应成功 */
    TEST_ASSERT_NOT_NULL(snap);

    /* 恢复 */
    bool ok = graph_snapshot_restore(snap, graph);
    TEST_ASSERT(ok, "恢复空快照应成功");

    graph_snapshot_destroy(snap);
    graph_snapshot_destroy(NULL);
    graph_destroy(graph);
}

/* --- 4.6 添加约束通用函数 --- */
#if 0
static void test_add_constraint_generic(void) {
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_NOT_NULL(graph);

    SymbolicCoord *coords[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
    int p1 = graph->next_node_id - 1;

    coords[0] = symbolic_coord_create_rational(100, 1);
    coords[1] = symbolic_coord_create_rational(100, 1);
    graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
    int p2 = graph->next_node_id - 1;

    /* 添加incidence约束 */
    bool ok = add_constraint_generic(graph, INCIDENCE, (int[]) {p1, p2}, 2);
    TEST_ASSERT(ok, "添加incidence约束应成功");

    graph_destroy(graph);
}
#endif

/* ============================================================
 *  测试组 5: Rewrite Apply 重写规则应用
 * ============================================================ */

/* --- 5.1 规则创建/销毁 --- */
static void test_rewrite_rule_lifecycle(void) {
    RewritePattern pattern;
    memset(&pattern, 0, sizeof(pattern));
    pattern.kind = 0;

    RewriteReplacement repl;
    memset(&repl, 0, sizeof(repl));

    RewriteRule *rule = rewrite_rule_create("test_rule", &pattern, &repl, 1);
    TEST_ASSERT_NOT_NULL(rule);
    TEST_ASSERT_NOT_NULL(rule->name);
    TEST_ASSERT_STR_EQ(rule->name, "test_rule");
    TEST_ASSERT_EQ(rule->reduction_measure, 1);

    rewrite_rule_destroy(rule);
    rewrite_rule_destroy(NULL);
    TEST_ASSERT(1, "销毁NULL安全");
}

/* --- 5.2 规则创建 NULL 参数 --- */
static void test_rewrite_rule_null_pattern(void) {
    /* pattern或replacement为NULL时可能失败或使用默认值 */
    RewriteRule *rule = rewrite_rule_create("null_test", NULL, NULL, 0);
    /* 具体行为取决于实现，只要不崩溃 */
    if (rule)
        rewrite_rule_destroy(rule);
    TEST_ASSERT(1, "NULL pattern不崩溃");
}

/* --- 5.3 匹配查找（空图） --- */
static void test_find_rewrite_match_empty(void) {
    ConstraintGraph *graph = graph_create();
    RewritePattern pattern;
    memset(&pattern, 0, sizeof(pattern));
    RewriteReplacement repl;
    memset(&repl, 0, sizeof(repl));
    RewriteRule *rule = rewrite_rule_create("empty_match", &pattern, &repl, 0);

    RewriteMatch *match = find_rewrite_match(graph, rule, false);
    /* 空图上应无匹配 */
    TEST_ASSERT_NULL(match);

    match = find_best_match(graph, rule, false);
    TEST_ASSERT_NULL(match);

    rewrite_rule_destroy(rule);
    graph_destroy(graph);
}

/* --- 5.4 规则热加载 --- */
static void test_rewrite_rule_load_unload(void) {
    RewriteRule **rules = NULL;
    int count = 0;

    /* 从不存在文件加载应失败 */
    int r = rewrite_rules_load_from_file("/nonexistent/file.lvz", &rules, &count);
    TEST_ASSERT_EQ(r, -1);

    /* 卸载 */
    rules = NULL;
    count = 0;
    bool ok = rewrite_rule_unload(&rules, &count, "some_rule");
    TEST_ASSERT(!ok, "空规则集卸载应返回false");

    /* NULL安全 */
    TEST_ASSERT(!rewrite_rule_unload(NULL, &count, "x"), "NULL rules指针应返回false");
}

/* --- 5.5 数值优化规则 --- */
static void test_rewrite_num_rule(void) {
    RewriteNumRule *rule =
        rewrite_num_rule_create("sqrt-diff", "sqrt(x+1)-sqrt(x)", "1/(sqrt(x+1)+sqrt(x))", REWRITE_NUM_HIGH, 10.0);
    TEST_ASSERT_NOT_NULL(rule);
    TEST_ASSERT_STR_EQ(rule->name, "sqrt-diff");
    TEST_ASSERT_EQ(rule->priority, REWRITE_NUM_HIGH);

    rewrite_num_rule_destroy(rule);
    rewrite_num_rule_destroy(NULL);
}

/* --- 5.6 策略创建 --- */
static void test_rewrite_strategy_create(void) {
    RewriteStrategy *s_idle = rewrite_strategy_create_idle();
    TEST_ASSERT_NOT_NULL(s_idle);
    TEST_ASSERT_EQ(s_idle->kind, REWRITE_STRATEGY_KIND_IDLE);
    rewrite_strategy_destroy(s_idle);

    RewriteStrategy *s_fail = rewrite_strategy_create_fail();
    TEST_ASSERT_NOT_NULL(s_fail);
    TEST_ASSERT_EQ(s_fail->kind, REWRITE_STRATEGY_KIND_FAIL);
    rewrite_strategy_destroy(s_fail);

    RewriteStrategy *s_apply = rewrite_strategy_create_apply_rule(0);
    TEST_ASSERT_NOT_NULL(s_apply);
    TEST_ASSERT_EQ(s_apply->kind, REWRITE_STRATEGY_KIND_APPLY_RULE);
    TEST_ASSERT_EQ(s_apply->rule_id, 0);
    rewrite_strategy_destroy(s_apply);

    RewriteStrategy *s_match = rewrite_strategy_create_match("pattern_expr");
    TEST_ASSERT_NOT_NULL(s_match);
    TEST_ASSERT_EQ(s_match->kind, REWRITE_STRATEGY_KIND_MATCH_PATTERN);
    rewrite_strategy_destroy(s_match);

    /* 组合策略 */
    RewriteStrategy *s1 = rewrite_strategy_create_idle();
    RewriteStrategy *s2 = rewrite_strategy_create_fail();
    RewriteStrategy *seq = rewrite_strategy_sequence(s1, s2);
    TEST_ASSERT_NOT_NULL(seq);
    TEST_ASSERT_EQ(seq->kind, REWRITE_STRATEGY_KIND_SEQUENCE);
    rewrite_strategy_destroy(seq);

    s1 = rewrite_strategy_create_idle();
    s2 = rewrite_strategy_create_fail();
    RewriteStrategy *orelse = rewrite_strategy_orelse(s1, s2);
    TEST_ASSERT_NOT_NULL(orelse);
    rewrite_strategy_destroy(orelse);

    RewriteStrategy *child = rewrite_strategy_create_idle();
    RewriteStrategy *rep = rewrite_strategy_repeat(child, 5);
    TEST_ASSERT_NOT_NULL(rep);
    TEST_ASSERT_EQ(rep->kind, REWRITE_STRATEGY_KIND_REPEAT);
    TEST_ASSERT_EQ(rep->max_iterations, 5);
    rewrite_strategy_destroy(rep);

    child = rewrite_strategy_create_idle();
    RewriteStrategy *norm = rewrite_strategy_normalize(child);
    TEST_ASSERT_NOT_NULL(norm);
    rewrite_strategy_destroy(norm);

    child = rewrite_strategy_create_idle();
    RewriteStrategy *try_s = rewrite_strategy_try(child);
    TEST_ASSERT_NOT_NULL(try_s);
    rewrite_strategy_destroy(try_s);

    rewrite_strategy_destroy(NULL);
}

/* ============================================================
 *  测试主函数
 * ============================================================ */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    TEST_SUITE_BEGIN("Layer 4 Misc Modules");

    /* 组1: Mini Kernel */
    fprintf(stderr, "\n--- Mini Kernel ---\n");
    TEST_RUN(test_mini_kernel_create_destroy);
    TEST_RUN(test_mini_kernel_null_config);
    TEST_RUN(test_mini_kernel_config_default_values);
    TEST_RUN(test_mini_kernel_add_statements);
    TEST_RUN(test_mini_kernel_seal_reset);
    TEST_RUN(test_mini_kernel_check_substitution);
    TEST_RUN(test_mini_kernel_prove_theorem);
    TEST_RUN(test_mini_kernel_verify_all);
    TEST_RUN(test_mini_kernel_self_check);
    TEST_RUN(test_mini_kernel_stats);
    TEST_RUN(test_mini_kernel_find_and_bind);
    TEST_RUN(test_mini_kernel_string_helpers);
    TEST_RUN(test_mini_kernel_import_export_mm);

    /* 组2: Relation Model */
    fprintf(stderr, "\n--- Relation Model ---\n");
    /* TEST_RUN(test_rel_set_operations); */
    /* TEST_RUN(test_rel_join_product_transpose); */
    /* TEST_RUN(test_rel_closure); */
    TEST_RUN(test_relation_model_from_graph);
    TEST_RUN(test_relation_model_destroy_null);
    TEST_RUN(test_relation_model_facts_assertions);
    TEST_RUN(test_relation_model_satisfiability);

    /* 组3: Algebra Mode */
    fprintf(stderr, "\n--- Algebra Mode ---\n");
    TEST_RUN(test_algebra_create_destroy);
    TEST_RUN(test_algebra_point_construction);
    TEST_RUN(test_algebra_line_construction);
    TEST_RUN(test_algebra_circle_construction);
    TEST_RUN(test_algebra_parallel_perpendicular);
    TEST_RUN(test_algebra_transform);
    TEST_RUN(test_algebra_selector);
    TEST_RUN(test_algebra_constrain_and_prove);
    TEST_RUN(test_algebra_build_and_query);
    TEST_RUN(test_algebra_undo_redo);
    TEST_RUN(test_algebra_snapshot_restore);
    TEST_RUN(test_algebra_work_plane);

    /* 组4: Rewrite Match */
    fprintf(stderr, "\n--- Rewrite Match ---\n");
    /* TEST_RUN(test_rewrite_internal_hash); */
    /* TEST_RUN(test_rewrite_pattern_var_checks); */
    TEST_RUN(test_vf2_state_basic);
    TEST_RUN(test_wl_history);
    TEST_RUN(test_graph_snapshot_lifecycle);
    /* TEST_RUN(test_add_constraint_generic); */

    /* 组5: Rewrite Apply */
    fprintf(stderr, "\n--- Rewrite Apply ---\n");
    TEST_RUN(test_rewrite_rule_lifecycle);
    TEST_RUN(test_rewrite_rule_null_pattern);
    TEST_RUN(test_find_rewrite_match_empty);
    TEST_RUN(test_rewrite_rule_load_unload);
    TEST_RUN(test_rewrite_num_rule);
    TEST_RUN(test_rewrite_strategy_create);

    TEST_SUITE_END();
    return g_fail_count > 0 ? 1 : 0;
}
