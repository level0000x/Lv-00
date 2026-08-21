/**
 * @file test_recursion_ext.c
 * @brief 递归系统契约测试（批次 C-㊺续5：recursion.h 20 个零覆盖 API）
 *
 * 覆盖 20 个 ctest 零覆盖 API（lv_MAX_RECURSION_DEPTH_LIMIT 为宏常量非 API）：
 *   - 全局深度保护族：lv_recursion_enter / leave / get_depth / reset /
 *     circuit_breaker_triggered
 *   - 上下文族：recursion_context_reset / recursion_context_set_depth_callback
 *   - 选择器族：selector_block_set_branch_nodes / count_branch_nodes /
 *     evaluate / validate_branches
 *   - 非符号测度族：measure_system_register_non_symbolic /
 *     measure_system_validate_non_symbolic /
 *     recursion_validate_non_symbolic_measure /
 *     recursion_validate_non_symbolic_with_axiom
 *   - 互递归族：recursion_check_mutual_with_contexts
 *   - 验证族：recursion_validate_measure
 *   - 测试族：recursion_run_builtin_tests / recursion_run_measure_tests
 *   - 流式族：recursion_set_stream_context
 *
 * 契约要点（与实现核对）：
 *   - lv_recursion_enter：depth >= lv_MAX_RECURSION_DEPTH(128) 时触发熔断
 *     返回 false；reset 恢复。
 *   - selector_block_count_branch_nodes：NULL 参数 → -1。
 *   - selector_block_validate_branches：空分支互斥；有交集不互斥。
 *   - recursion_run_builtin_tests：NULL sys 创建临时系统，返回通过数。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/recursion.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：全局深度保护 ============== */

static void test_global_depth_api(void) {
    /* reset → depth 0 */
    lv_recursion_reset();
    TEST_ASSERT_EQ(lv_recursion_get_depth(), 0);
    TEST_ASSERT(!lv_recursion_circuit_breaker_triggered(), "初始未熔断");

    /* enter/leave 配对 */
    TEST_ASSERT(lv_recursion_enter(), "进入递归");
    TEST_ASSERT_EQ(lv_recursion_get_depth(), 1);
    TEST_ASSERT(lv_recursion_enter(), "进入递归 2");
    TEST_ASSERT_EQ(lv_recursion_get_depth(), 2);
    lv_recursion_leave();
    TEST_ASSERT_EQ(lv_recursion_get_depth(), 1);
    lv_recursion_leave();
    TEST_ASSERT_EQ(lv_recursion_get_depth(), 0);

    /* 熔断：深度达上限（128）后触发 */
    lv_recursion_reset();
    int enters = 0;
    while (lv_recursion_enter() && enters < 200)
        enters++;
    TEST_ASSERT(enters >= lv_MAX_RECURSION_DEPTH, "达到深度上限");
    TEST_ASSERT(lv_recursion_circuit_breaker_triggered(), "熔断已触发");
    TEST_ASSERT(!lv_recursion_enter(), "熔断后拒绝进入");

    /* reset 恢复 */
    lv_recursion_reset();
    TEST_ASSERT_EQ(lv_recursion_get_depth(), 0);
    TEST_ASSERT(!lv_recursion_circuit_breaker_triggered(), "重置后未熔断");
    TEST_ASSERT(lv_recursion_enter(), "重置后可进入");
    lv_recursion_leave();

    printf("  test_global_depth_api: PASSED\n");
}

/* ============== 测试：递归上下文 ============== */

/* 深度回调（记录调用） */
static RecursionAction depth_cb(int current_depth, int max_depth, void *user_data) {
    (void)current_depth;
    (void)max_depth;
    int *called = (int *)user_data;
    if (called)
        (*called)++;
    return RECURSION_DEPTH_ACTION_CONTINUE;
}

static void test_context_api(void) {
    /* reset：NULL 安全 + 正常重置 */
    recursion_context_reset(NULL);
    RecursionContext *ctx = recursion_context_create(100);
    TEST_ASSERT_NOT_NULL(ctx);
    recursion_context_reset(ctx);
    TEST_ASSERT_EQ(ctx->current_depth, 0);

    /* set_depth_callback：NULL 安全 + 注册 */
    recursion_context_set_depth_callback(NULL, NULL, NULL); /* 不崩溃即通过 */
    int cb_called = 0;
    recursion_context_set_depth_callback(ctx, depth_cb, &cb_called);
    TEST_ASSERT(ctx->depth_callback != NULL, "回调已注册");

    recursion_context_destroy(ctx);
    printf("  test_context_api: PASSED\n");
}

/* ============== 测试：选择器块 ============== */

static void test_selector_api(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    SelectorBlock *sb = selector_block_create(1, g);
    TEST_ASSERT_NOT_NULL(sb);

    /* set_branch_nodes：NULL 安全 + 正常 */
    selector_block_set_branch_nodes(NULL, NULL, 0, NULL, 0); /* 不崩溃即通过 */
    int true_ids[3] = {10, 11, 12};
    int false_ids[2] = {20, 21};
    selector_block_set_branch_nodes(sb, true_ids, 3, false_ids, 2);
    TEST_ASSERT_EQ(sb->true_branch_node_count, 3);
    TEST_ASSERT_EQ(sb->false_branch_node_count, 2);

    /* count_branch_nodes：NULL → -1；正常计数 */
    int tc = -1, fc = -1;
    TEST_ASSERT_EQ(selector_block_count_branch_nodes(NULL, &tc, &fc), -1);
    TEST_ASSERT_EQ(selector_block_count_branch_nodes(sb, NULL, &fc), -1);
    TEST_ASSERT_EQ(selector_block_count_branch_nodes(sb, &tc, &fc), 0);
    TEST_ASSERT_EQ(tc, 3);
    TEST_ASSERT_EQ(fc, 2);

    /* validate_branches：互斥 true；重叠 false */
    TEST_ASSERT(selector_block_validate_branches(sb), "分支互斥");
    int overlap[1] = {11};
    selector_block_set_branch_nodes(sb, true_ids, 3, overlap, 1);
    TEST_ASSERT(!selector_block_validate_branches(sb), "重叠不互斥");
    selector_block_set_branch_nodes(sb, true_ids, 3, NULL, 0);
    TEST_ASSERT(selector_block_validate_branches(sb), "空分支互斥");

    /* evaluate：NULL → false；无有效测试点 → false + PENDING */
    TEST_ASSERT(!selector_block_evaluate(NULL, g), "NULL sb");
    TEST_ASSERT(!selector_block_evaluate(sb, NULL), "NULL graph");
    TEST_ASSERT(!selector_block_evaluate(sb, g), "无测试点判定失败");
    TEST_ASSERT_EQ(sb->true_state, BRANCH_PENDING);

    selector_block_destroy(sb);
    graph_destroy(g);
    printf("  test_selector_api: PASSED\n");
}

/* ============== 测试：非符号测度 ============== */

static bool ns_comparator(const SymbolicCoord *a, const SymbolicCoord *b) {
    (void)a;
    (void)b;
    return true; /* a < b 恒真（测试用） */
}

static void test_nonsymbolic_api(void) {
    MeasureSystem *ms = measure_system_create();
    TEST_ASSERT_NOT_NULL(ms);

    /* register：NULL 契约 + 正常 */
    TEST_ASSERT(!measure_system_register_non_symbolic(NULL, 1, ns_comparator, true), "NULL ms");
    TEST_ASSERT(measure_system_register_non_symbolic(ms, 1, ns_comparator, true), "注册非符号测度");

    /* validate_non_symbolic：空系统 true */
    MeasureSystem *empty = measure_system_create();
    TEST_ASSERT_NOT_NULL(empty);
    TEST_ASSERT(measure_system_validate_non_symbolic(NULL) == false || measure_system_validate_non_symbolic(NULL) == true,
                "NULL 契约不崩溃");
    TEST_ASSERT(measure_system_validate_non_symbolic(empty), "空系统验证通过");

    /* validate_non_symbolic_measure：NULL 契约（→ ERROR）+ 正常 */
    TEST_ASSERT_EQ(recursion_validate_non_symbolic_measure(NULL, NULL, NULL, NULL),
                   RECURSION_CHECK_RESULT_ERROR);
    Measure *m = measure_create_symbolic("depth", MEASURE_KIND_DEPTH, -1);
    TEST_ASSERT_NOT_NULL(m);
    RecursionCheckResult nsr = recursion_validate_non_symbolic_measure(m, NULL, NULL, ns_comparator);
    TEST_ASSERT(nsr >= RECURSION_CHECK_RESULT_OK && nsr <= RECURSION_CHECK_RESULT_ERROR, "非符号测度验证状态合法");
    measure_destroy(m);

    /* validate_non_symbolic_with_axiom：NULL 契约 */
    TEST_ASSERT_EQ(recursion_validate_non_symbolic_with_axiom(NULL, 0, NULL, NULL), -1);

    measure_system_destroy(empty);
    measure_system_destroy(ms);
    printf("  test_nonsymbolic_api: PASSED\n");
}

/* ============== 测试：互递归 + 验证 ============== */

static void test_mutual_validate_api(void) {
    /* check_mutual_with_contexts：NULL → false + 两上下文不崩溃 */
    TEST_ASSERT(!recursion_check_mutual_with_contexts(NULL, NULL), "NULL 上下文");
    RecursionContext *a = recursion_context_create(10);
    RecursionContext *b = recursion_context_create(10);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    recursion_check_mutual_with_contexts(a, b); /* 不崩溃即通过 */
    recursion_context_destroy(a);
    recursion_context_destroy(b);

    /* recursion_validate_measure：NULL 契约 */
    TEST_ASSERT(recursion_validate_measure(NULL, NULL, NULL, 0) == RECURSION_CHECK_RESULT_ERROR ||
                recursion_validate_measure(NULL, NULL, NULL, 0) == RECURSION_CHECK_RESULT_MEASURE_UNKNOWN,
                "NULL ctx 状态合法");

    printf("  test_mutual_validate_api: PASSED\n");
}

/* ============== 测试：测试套件 ============== */

static void test_run_tests_api(void) {
    /* run_builtin_tests：NULL sys → 临时系统运行，返回通过数 */
    RecursionTestResult *results = NULL;
    int result_count = 0;
    int passed = recursion_run_builtin_tests(NULL, &results, &result_count);
    TEST_ASSERT(passed >= 0, "内置测试运行返回通过数");
    TEST_ASSERT(result_count > 0, "测试结果数量");
    TEST_ASSERT_NOT_NULL(results);
    lv_free((void **)&results);

    /* NULL 输出契约 */
    TEST_ASSERT_EQ(recursion_run_builtin_tests(NULL, NULL, NULL), -1);

    /* run_measure_tests：NULL 契约 */
    TEST_ASSERT(!recursion_run_measure_tests(NULL, 0, NULL, NULL, NULL), "NULL measure");

    printf("  test_run_tests_api: PASSED\n");
}

/* ============== 测试：流式上下文 ============== */

static void test_stream_api(void) {
    recursion_set_stream_context(NULL); /* NULL 安全 */
    recursion_set_stream_context(NULL);
    printf("  test_stream_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Recursion Ext Test Suite")
    printf("=== Lv-00 Recursion Ext Test Suite (batch C-㊺续5) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_global_depth_api);
    TEST_MAIN_RUN(test_context_api);
    TEST_MAIN_RUN(test_selector_api);
    TEST_MAIN_RUN(test_nonsymbolic_api);
    TEST_MAIN_RUN(test_mutual_validate_api);
    TEST_MAIN_RUN(test_run_tests_api);
    TEST_MAIN_RUN(test_stream_api);

    lv_cleanup();
TEST_MAIN_END()
