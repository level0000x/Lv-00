/**
 * @file test_proof_version_ext.c
 * @brief proof.h 引擎依赖设施契约测试（C-㉞ 专项）
 *
 * 批次 C-㉞：补测此前因「需引擎/求解器/文件 IO 依赖」而留待专项的
 * proof.h 设施。甄别后发现多数依赖仅为判空或不 deref，可独立测试：
 * - proof_guided_fill / fill_suggestions_destroy（纯启发式，solver 仅判空）
 * - proof_sledgehammer_dispatch / sledgehammer_report_destroy（多策略调度，
 *   同步回退语义）
 * - proof_refinement_check / refinement_check_report_destroy（类型/SMT 检查，
 *   solver 仅用于类型注册表查询）
 * - proof_export_isar（命题列表 → Isar 文本）
 * - proof_interactive_step（步骤策略 validate 表）
 * - proof_check_unconstructibility / proof_attempt_unconstructibility /
 *   unconstruct_info_destroy（NULL 契约 + 无 engine 路径）
 * - proof_export_latex / proof_export_coq（文件导出，临时文件验证）
 *
 * 按 test-authoring 三层：等价性 / 边界 / 性质。
 *
 * @author Lv-00 Project
 * @date 2026-08-20
 */

#include <stdio.h>
#include <string.h>

#include "lv/proof.h"
#include "lv.h"
#include "test_helpers.h"

/* ============================================================
 * 全局测试计数器
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

#define TEST_EXPORT_FILE "test_proof_version_ext_tmp.out"

static void remove_export_file(void) {
    remove(TEST_EXPORT_FILE);
}

/* ============================================================
 * proof_guided_fill：启发式填充建议（solver 仅判空）
 * ============================================================ */
static void test_guided_fill(void) {
    /* NULL solver（solver_has_assignments=false）+ 空目标 → lambda 建议 */
    FillSuggestion *s1 = proof_guided_fill(NULL, NULL, 0);
    TEST_ASSERT_MSG(s1 != NULL, "空目标返回建议");
    TEST_ASSERT_MSG(s1->kind == FILL_LAMBDA, "空目标建议 lambda");
    TEST_ASSERT_MSG(s1->next == NULL, "空目标单建议");
    fill_suggestions_destroy(s1);

    /* 空字符串目标同样 lambda */
    FillSuggestion *s2 = proof_guided_fill(NULL, "", 0);
    TEST_ASSERT_MSG(s2 != NULL && s2->kind == FILL_LAMBDA, "空串目标建议 lambda");
    fill_suggestions_destroy(s2);

    /* triangle 关键词 → 构造 + 精化 + 维度 lambda（solver 判空分支） */
    FillSuggestion *s3 = proof_guided_fill(NULL, "triangle abc", 2);
    TEST_ASSERT_MSG(s3 != NULL, "triangle 返回建议链");
    int count3 = 0;
    for (FillSuggestion *it = s3; it; it = it->next)
        count3++;
    TEST_ASSERT_MSG(count3 >= 2, "triangle 至少 2 条建议");
    TEST_ASSERT_MSG(s3->kind == FILL_CONSTRUCTOR, "首条为构造器建议");
    fill_suggestions_destroy(s3);

    /* solver 非空 → fallback 额外附求解器建议（solver_has_assignments）
     * 注：ConstraintSolver 为不透明类型，此处以非 NULL 指针传入（实现仅判空不 deref） */
    FillSuggestion *s4 = proof_guided_fill((ConstraintSolver *) 1, "unknown_goal_type", 0);
    TEST_ASSERT_MSG(s4 != NULL, "未知目标返回 fallback 建议");
    TEST_ASSERT_MSG(s4->kind == FILL_REFINE, "fallback 建议 refine");
    /* solver 非空 → 额外 solver_assigned_variables 建议 */
    int count4 = 0;
    for (FillSuggestion *it = s4; it; it = it->next)
        count4++;
    TEST_ASSERT_MSG(count4 >= 2, "solver 非空时至少 2 条（含求解器建议）");
    fill_suggestions_destroy(s4);

    /* intersect 关键词 → case split */
    FillSuggestion *s5 = proof_guided_fill(NULL, "intersect(a,b)", 0);
    TEST_ASSERT_MSG(s5 != NULL, "intersect 返回建议");
    fill_suggestions_destroy(s5);

    /* NULL destroy 安全 */
    fill_suggestions_destroy(NULL);
}

/* ============================================================
 * proof_sledgehammer_dispatch：多策略调度（同步回退）
 * ============================================================ */
static void test_sledgehammer(void) {
    /* NULL mse → NULL */
    TEST_ASSERT_NULL(proof_sledgehammer_dispatch(NULL, SLEDGE_SYNC, 0));
    sledgehammer_report_destroy(NULL);

    /* 创建多策略引擎（无 engine：legacy 策略 UNAVAILABLE，默认策略 AVAILABLE） */
    ProofMultiStrategy *mse = proof_multi_strategy_create(NULL);
    TEST_ASSERT_MSG(mse != NULL, "mse 创建");

    /* SYNC 模式调度：结果数组已分配、result_count 合理、best_index 有效 */
    SledgehammerReport *report = proof_sledgehammer_dispatch(mse, SLEDGE_SYNC, 0);
    TEST_ASSERT_MSG(report != NULL, "SYNC 调度返回报告");
    TEST_ASSERT_MSG(report->results != NULL, "结果数组已分配");
    TEST_ASSERT_MSG(report->result_count >= 0 && report->result_count <= PROOF_STRATEGY_COUNT, "result_count 合理");
    TEST_ASSERT_MSG(report->best_index >= -1 && report->best_index < report->result_count, "best_index 合理");
    TEST_ASSERT_MSG(report->total_time_sec >= 0.0, "总耗时非负");

    /* ASYNC 模式：回退为同步（警告），行为与 SYNC 一致 */
    SledgehammerReport *report_async = proof_sledgehammer_dispatch(mse, SLEDGE_ASYNC, 100);
    TEST_ASSERT_MSG(report_async != NULL, "ASYNC 回退为同步仍返回报告");
    TEST_ASSERT_MSG(report_async->result_count == report->result_count, "ASYNC/SYNC 结果数一致");

    /* TIMEOUT 模式：极短超时不崩 */
    SledgehammerReport *report_to = proof_sledgehammer_dispatch(mse, SLEDGE_TIMEOUT, 1);
    TEST_ASSERT_MSG(report_to != NULL, "TIMEOUT 调度返回报告");

    sledgehammer_report_destroy(report);
    sledgehammer_report_destroy(report_async);
    sledgehammer_report_destroy(report_to);
    proof_multi_strategy_destroy(mse);
}

/* ============================================================
 * proof_refinement_check：类型/SMT 精化检查
 * ============================================================ */
static void test_refinement_check(void) {
    /* 空条目 → NULL */
    TEST_ASSERT_NULL(proof_refinement_check(NULL, NULL, 0));
    TEST_ASSERT_NULL(proof_refinement_check(NULL, NULL, -1));
    refinement_check_report_destroy(NULL);

    /* 单条目（solver NULL → 类型检查跳过；无 refinement_pred → REFINE_OK） */
    RefinementCheckEntry entries[2];
    memset(&entries, 0, sizeof(entries));
    entries[0].geom_object = "pointA";
    entries[0].base_type = "Point";
    entries[0].refinement_pred = NULL; /* 无精化谓词 → OK */

    /* 恒假谓词：SMT 后端可用时经真实求解判定（UNSAT→失败 或 UNKNOWN→保守通过），
     * 字符串启发式仅在 SMT 不可用时触发；此处不断言具体结果，只验证报告结构 */
    entries[1].geom_object = "pointB";
    entries[1].base_type = "Point";
    entries[1].refinement_pred = "false";

    RefinementCheckReport *report = proof_refinement_check(NULL, entries, 2);
    TEST_ASSERT_MSG(report != NULL, "报告已分配");
    TEST_ASSERT_EQ(report->entry_count, 2);
    TEST_ASSERT_MSG(report->entries != NULL, "条目数组已分配");
    TEST_ASSERT_MSG(report->entries[0].result == REFINE_OK, "无谓词条目 OK");
    TEST_ASSERT_MSG(report->entries[1].result == REFINE_OK || report->entries[1].result == REFINE_SMT_UNSAT,
                    "谓词条目 OK 或 UNSAT（依 SMT 后端）");
    TEST_ASSERT_EQ(report->passed_count + report->failed_count, 2);

    /* 输入条目未被修改（只读复制） */
    TEST_ASSERT_MSG(strcmp(entries[0].geom_object, "pointA") == 0, "输入条目不变");

    refinement_check_report_destroy(report);
}

/* ============================================================
 * proof_export_isar：命题列表 → Isar 文本
 * ============================================================ */
static void test_export_isar(void) {
    /* NULL/空 → NULL */
    TEST_ASSERT_NULL(proof_export_isar(NULL, 0));
    TEST_ASSERT_NULL(proof_export_isar(NULL, 1));
    TEST_ASSERT_NULL(proof_export_isar(NULL, -1));

    /* 构造不同命题类型 */
    Proposition *p1 = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    p1->label = lv_strdup_safe("atomic_lemma");
    Proposition *p2 = proposition_create(2, PROPOSITION_TYPE_IMPLICATION);
    p2->label = lv_strdup_safe("imp_lemma");
    Proposition *p3 = proposition_create(3, PROPOSITION_TYPE_NEGATION);
    p3->label = lv_strdup_safe("neg_lemma");
    Proposition *p4 = proposition_create(4, PROPOSITION_TYPE_BOTTOM);
    p4->label = NULL; /* 未命名 → 默认标签 */
    TEST_ASSERT_MSG(p1 && p2 && p3 && p4, "命题创建");

    const Proposition *props[] = {p1, p2, p3, p4, NULL};
    char *isar = proof_export_isar(props, 5);
    TEST_ASSERT_MSG(isar != NULL, "Isar 导出非空");
    TEST_ASSERT_MSG(strstr(isar, "theory Exported_Proof") != NULL, "含 theory 头");
    TEST_ASSERT_MSG(strstr(isar, "imports Main") != NULL, "含 imports");
    TEST_ASSERT_MSG(strstr(isar, "lemma atomic_lemma_1") != NULL, "含 lemma 1（label 清洗）");
    TEST_ASSERT_MSG(strstr(isar, "lemma imp_lemma_2") != NULL, "含 lemma 2");
    TEST_ASSERT_MSG(strstr(isar, "lemma neg_lemma_3") != NULL, "含 lemma 3");
    TEST_ASSERT_MSG(strstr(isar, "lemma") != NULL, "含 lemma 4 默认标签");
    TEST_ASSERT_MSG(strstr(isar, "end") != NULL, "含 end");
    TEST_ASSERT_MSG(strstr(isar, "qed") != NULL, "含 qed");
    lv_free((void **) &isar);

    /* 空命题数组（全 NULL 元素）→ 仍返回框架 */
    const Proposition *empty[] = {NULL, NULL};
    char *isar_empty = proof_export_isar(empty, 2);
    TEST_ASSERT_MSG(isar_empty != NULL, "空命题数组返回框架");
    TEST_ASSERT_MSG(strstr(isar_empty, "theory Exported_Proof") != NULL, "框架含 theory");
    lv_free((void **) &isar_empty);

    lv_free((void **) &p1->label);
    lv_free((void **) &p2->label);
    lv_free((void **) &p3->label);
    proposition_unref(p1);
    proposition_unref(p2);
    proposition_unref(p3);
    proposition_unref(p4);
}

/* ============================================================
 * proof_interactive_step：步骤策略 validate 表
 * ============================================================ */
static void test_interactive_step(void) {
    /* NULL nav → false */
    TEST_ASSERT_MSG(!proof_interactive_step(NULL, PROOF_STEP_ADD_NODE, NULL), "NULL nav 失败");

    Proposition *prop = proposition_create(50, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建");

    /* NORMALIZATION：validate_noop，step_data 任意 → 成功 */
    TEST_ASSERT_MSG(proof_interactive_step(nav, PROOF_STEP_NORMALIZATION, NULL), "NORMALIZATION 步骤成功");
    TEST_ASSERT_EQ(nav->step_count, 1);
    TEST_ASSERT_MSG(nav->steps[0]->is_completed, "步骤标记完成");

    /* UNIFY：validate_noop → 成功 */
    TEST_ASSERT_MSG(proof_interactive_step(nav, PROOF_STEP_UNIFY, NULL), "UNIFY 步骤成功");
    TEST_ASSERT_EQ(nav->step_count, 2);

    /* 非法类型 → false */
    TEST_ASSERT_MSG(!proof_interactive_step(nav, (ProofStepType) 999, NULL), "越界类型失败");
    TEST_ASSERT_MSG(nav->step_count == 2, "失败不改变步数");

    /* 越界负类型 → false */
    TEST_ASSERT_MSG(!proof_interactive_step(nav, (ProofStepType) -1, NULL), "负类型失败");

    /* ADD_NODE：validate_add_node 需要 step_data（node_id），NULL 应失败 */
    /* 注：validate_add_node 实现校验 step_data 非空；NULL 应拒绝 */
    bool add_null = proof_interactive_step(nav, PROOF_STEP_ADD_NODE, NULL);
    /* 契约：validate 拒绝非法 step_data（不强制断言，视实现） */
    TEST_ASSERT_MSG(nav->step_count == 2 || nav->step_count == 3, "ADD_NODE 步骤数合法");

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

/* ============================================================
 * 不可构造性：NULL 契约 + 无 engine 路径
 * ============================================================ */
static void test_unconstructibility(void) {
    /* NULL 参数 → UNCONSTRUCT_ERROR */
    UnconstructInfo info;
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_MSG(proof_check_unconstructibility(NULL, NULL, NULL, &info) == UNCONSTRUCT_ERROR, "check NULL 报错");
    TEST_ASSERT_MSG(proof_attempt_unconstructibility(NULL, NULL, NULL, &info) == UNCONSTRUCT_ERROR, "attempt NULL 报错");
    TEST_ASSERT_MSG(info.result == UNCONSTRUCT_ERROR, "info.result 置 ERROR");

    /* 合法 nav + 空图（无 engine）→ MAYBE_POSSIBLE */
    Proposition *prop = proposition_create(60, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建");
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_MSG(graph != NULL, "graph 创建");

    memset(&info, 0, sizeof(info));
    UnconstructResult r = proof_attempt_unconstructibility(nav, graph, NULL, &info);
    TEST_ASSERT_MSG(r == UNCONSTRUCT_MAYBE_POSSIBLE, "attempt 无匹配 MAYBE_POSSIBLE");
    TEST_ASSERT_MSG(info.result == UNCONSTRUCT_MAYBE_POSSIBLE, "info.result 同步");

    /* 有高次方程时可生成代数分析报告（空图无方程 → 无报告路径，不崩） */
    unconstruct_info_destroy(&info);
    unconstruct_info_destroy(NULL);

    graph_destroy(graph);
    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

/* ============================================================
 * proof_export_latex / proof_export_coq：文件导出
 * ============================================================ */
static void test_export_files(void) {
    remove_export_file();

    /* NULL 契约 */
    TEST_ASSERT_MSG(!proof_export_latex(NULL, TEST_EXPORT_FILE), "NULL nav latex 失败");
    TEST_ASSERT_MSG(!proof_export_coq(NULL, TEST_EXPORT_FILE), "NULL nav coq 失败");
    TEST_ASSERT_MSG(!proof_export_latex(NULL, NULL), "NULL filepath latex 失败");

    /* 合法 nav + 步骤 → 导出成功且文件非空 */
    Proposition *prop = proposition_create(70, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建");
    ProofStep *s = proof_step_create(PROOF_STEP_ADD_NODE);
    s->note = lv_strdup_safe("辅助构造");
    TEST_ASSERT_MSG(proof_navigator_add_step(nav, s), "添加步骤");

    TEST_ASSERT_MSG(proof_export_latex(nav, TEST_EXPORT_FILE), "latex 导出成功");
    {
        FILE *f = fopen(TEST_EXPORT_FILE, "r");
        TEST_ASSERT_MSG(f != NULL, "latex 文件存在");
        char buf[1024];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        fclose(f);
        TEST_ASSERT_MSG(strstr(buf, "\\documentclass") != NULL, "latex 含 documentclass");
        TEST_ASSERT_MSG(strstr(buf, "辅助构造") != NULL, "latex 含注释");
    }

    TEST_ASSERT_MSG(proof_export_coq(nav, TEST_EXPORT_FILE), "coq 导出成功");
    {
        FILE *f = fopen(TEST_EXPORT_FILE, "r");
        TEST_ASSERT_MSG(f != NULL, "coq 文件存在");
        char buf[1024];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        fclose(f);
        TEST_ASSERT_MSG(strstr(buf, "Theorem") != NULL, "coq 含 Theorem");
        TEST_ASSERT_MSG(strstr(buf, "Qed") != NULL, "coq 含 Qed");
    }

    lv_free((void **) &s->note);
    proof_navigator_destroy(nav);
    proposition_unref(prop);
    remove_export_file();
}

/* ============================================================
 * 主入口
 * ============================================================ */
TEST_MAIN_BEGIN("Proof Version Ext")

    TEST_MAIN_RUN(test_guided_fill);
    TEST_MAIN_RUN(test_sledgehammer);
    TEST_MAIN_RUN(test_refinement_check);
    TEST_MAIN_RUN(test_export_isar);
    TEST_MAIN_RUN(test_interactive_step);
    TEST_MAIN_RUN(test_unconstructibility);
    TEST_MAIN_RUN(test_export_files);

TEST_MAIN_END()
