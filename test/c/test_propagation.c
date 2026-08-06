/**
 * @file test_propagation.c
 * @brief 约束传播引擎 (propagation) 单元测试
 *
 * 测试内容：
 * - 传播上下文生命周期管理
 * - 状态空间初始化与查询
 * - AC-3 弧相容性约束传播
 * - WFC 熵最小化节点选择
 * - 节点坍缩
 * - 熵计算
 * - 完整 WFC 求解循环
 * - 快照保存/恢复/销毁
 * - NULL 输入安全性
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "propagation.h"
#include "test_helpers.h"

/* 全局测试计数器 */
int g_pass_count = 0;
int g_fail_count = 0;

/* ================================================================
 * 测试 1: 传播上下文创建与销毁生命周期
 * ================================================================ */
void test_prop_lifecycle(void) {
    printf("  TEST: propagation_context_create/destroy lifecycle...\n");

    /* 正常创建/销毁 */
    ConstraintGraph *graph = graph_create();
    PropagationContext *ctx = propagation_context_create(graph);
    TEST_ASSERT(ctx != NULL, "propagation_context_create with valid graph");
    TEST_ASSERT(ctx->graph == graph, "ctx->graph matches input graph");
    TEST_ASSERT(ctx->state_count == 0, "empty graph -> state_count == 0");
    TEST_ASSERT(ctx->strategy == PROP_STRATEGY_MIN_ENTROPY, "default strategy is MIN_ENTROPY");
    propagation_context_destroy(ctx);
    graph_destroy(graph);

    /* NULL graph -> 返回 NULL */
    PropagationContext *null_ctx = propagation_context_create(NULL);
    TEST_ASSERT(null_ctx == NULL, "propagation_context_create(NULL) returns NULL");

    /* NULL safety on destroy */
    propagation_context_destroy(NULL);

    printf("  PASS: lifecycle\n");
}

/* ================================================================
 * 测试 2: 在空图上初始化状态空间
 * ================================================================ */
void test_prop_init_empty(void) {
    printf("  TEST: propagation_init_state_spaces on empty graph...\n");

    ConstraintGraph *graph = graph_create();
    PropagationContext *ctx = propagation_context_create(graph);

    PropagationResult result = propagation_init_state_spaces(ctx);
    TEST_ASSERT(result == PROP_RESULT_CONSISTENT, "init on empty graph returns CONSISTENT");

    propagation_context_destroy(ctx);
    graph_destroy(graph);

    /* NULL input */
    PropagationResult null_result = propagation_init_state_spaces(NULL);
    TEST_ASSERT(null_result == PROP_RESULT_CONTRADICTION, "init_state_spaces(NULL) returns CONTRADICTION");

    printf("  PASS: init on empty graph\n");
}

/* ================================================================
 * 测试 3: propagation_get_state_space 查询
 * ================================================================ */
void test_prop_get_state_space(void) {
    printf("  TEST: propagation_get_state_space query...\n");

    /* 有点的图：有效节点返回非 NULL */
    ConstraintGraph *graph = graph_create();
    SymbolicCoord *c0[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    graph_add_point(graph, c0, 2);

    PropagationContext *ctx = propagation_context_create(graph);
    propagation_init_state_spaces(ctx);

    /* 有效节点返回非 NULL */
    NodeStateSpace *ss = propagation_get_state_space(ctx, 0);
    TEST_ASSERT(ss != NULL, "get_state_space for valid node returns non-NULL");
    TEST_ASSERT(ss->node_id == 0, "state space node_id == 0");

    /* 无效节点返回 NULL */
    NodeStateSpace *ss_invalid = propagation_get_state_space(ctx, 99);
    TEST_ASSERT(ss_invalid == NULL, "get_state_space for invalid node returns NULL");

    propagation_context_destroy(ctx);
    symbolic_coord_destroy(c0[0]);
    symbolic_coord_destroy(c0[1]);
    graph_destroy(graph);

    /* NULL input */
    NodeStateSpace *ss_null = propagation_get_state_space(NULL, 0);
    TEST_ASSERT(ss_null == NULL, "get_state_space(NULL, ...) returns NULL");

    printf("  PASS: get_state_space\n");
}

/* ================================================================
 * 测试 4: propagation_run 在简单图上运行
 * ================================================================ */
void test_prop_run_simple(void) {
    printf("  TEST: propagation_run on simple graph...\n");

    /* 创建两个点 + 一条线段 + 关联约束 */
    ConstraintGraph *graph = graph_create();

    SymbolicCoord *c0[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    SymbolicCoord *c1[2] = {symbolic_coord_create_rational(1, 1), symbolic_coord_create_rational(0, 1)};
    graph_add_point(graph, c0, 2);
    graph_add_point(graph, c1, 2);

    PropagationContext *ctx = propagation_context_create(graph);
    propagation_init_state_spaces(ctx);

    /* graph_add_line_segment 可能依赖于图中有适当的节点 */
    /* 使用 graph_add_line_segment API — 该 API 期望两个点节点 ID */
    /* 在现有测试中这是标准用法 */
    graph_add_line_segment(graph, 0, 1);

    /* 确保至少有一个约束可以在传播中使用 */
    /* 再次初始化状态空间以反映线段 */
    propagation_init_state_spaces(ctx);

    /* 运行传播 - 应该不会崩溃 */
    PropagationResult result = propagation_run(ctx);
    TEST_ASSERT(result == PROP_RESULT_SATISFIED || result == PROP_RESULT_STABLE || result == PROP_RESULT_CONSISTENT,
                "propagation_run on simple graph returns valid result");

    propagation_context_destroy(ctx);
    symbolic_coord_destroy(c0[0]);
    symbolic_coord_destroy(c0[1]);
    symbolic_coord_destroy(c1[0]);
    symbolic_coord_destroy(c1[1]);
    graph_destroy(graph);

    /* NULL input */
    PropagationResult null_result = propagation_run(NULL);
    TEST_ASSERT(null_result == PROP_RESULT_CONTRADICTION, "propagation_run(NULL) returns CONTRADICTION");

    printf("  PASS: run on simple graph\n");
}

/* ================================================================
 * 测试 5: propagation_select_node 节点选择
 * ================================================================ */
void test_prop_select_node(void) {
    printf("  TEST: propagation_select_node...\n");

    /* 全坍缩图：select 应返回 -1 */
    ConstraintGraph *graph = graph_create();

    SymbolicCoord *c0[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    graph_add_point(graph, c0, 2);

    PropagationContext *ctx = propagation_context_create(graph);
    propagation_init_state_spaces(ctx);

    /* 点有坐标 → 已坍缩 → select 返回 -1 */
    int node = propagation_select_node(ctx);
    TEST_ASSERT(node == -1, "select_node on fully collapsed graph returns -1");

    propagation_context_destroy(ctx);
    symbolic_coord_destroy(c0[0]);
    symbolic_coord_destroy(c0[1]);
    graph_destroy(graph);

    /* NULL input */
    int null_result = propagation_select_node(NULL);
    TEST_ASSERT(null_result == -1, "select_node(NULL) returns -1");

    printf("  PASS: select_node\n");
}

/* ================================================================
 * 测试 6: propagation_collapse 坍缩
 * ================================================================ */
void test_prop_collapse(void) {
    printf("  TEST: propagation_collapse...\n");

    ConstraintGraph *graph = graph_create();

    SymbolicCoord *c0[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(1, 1)};
    graph_add_point(graph, c0, 2);

    PropagationContext *ctx = propagation_context_create(graph);
    propagation_init_state_spaces(ctx);

    /* 点已有坐标 → 已坍缩 → collapse 返回 false */
    bool collapsed = propagation_collapse(ctx, 0);
    TEST_ASSERT(!collapsed, "collapse on already-collapsed node returns false");

    /* 无效节点 → 返回 false */
    bool invalid = propagation_collapse(ctx, 99);
    TEST_ASSERT(!invalid, "collapse on invalid node returns false");

    propagation_context_destroy(ctx);
    symbolic_coord_destroy(c0[0]);
    symbolic_coord_destroy(c0[1]);
    graph_destroy(graph);

    /* NULL input */
    bool null_collapse = propagation_collapse(NULL, 0);
    TEST_ASSERT(!null_collapse, "collapse(NULL, ...) returns false");

    printf("  PASS: collapse\n");
}

/* ================================================================
 * 测试 7: propagation_compute_entropy 熵计算
 * ================================================================ */
void test_prop_entropy(void) {
    printf("  TEST: propagation_compute_entropy...\n");

    /* 已坍缩 → 熵 = 0 */
    NodeStateSpace collapsed;
    memset(&collapsed, 0, sizeof(collapsed));
    collapsed.is_collapsed = true;
    collapsed.collapsed_value = NULL;

    double e0 = propagation_compute_entropy(&collapsed);
    TEST_ASSERT(fabs(e0) < 1e-12, "collapsed node entropy == 0");

    /* 无界 → PROP_ENTROPY_UNBOUNDED (-1.0) */
    NodeStateSpace unbounded;
    memset(&unbounded, 0, sizeof(unbounded));
    unbounded.is_unbounded = true;

    double e1 = propagation_compute_entropy(&unbounded);
    TEST_ASSERT(e1 == PROP_ENTROPY_UNBOUNDED, "unbounded node entropy == -1.0");

    /* 2 个候选 → entropy = log2(2) = 1.0 */
    NodeStateSpace two_candidates;
    memset(&two_candidates, 0, sizeof(two_candidates));
    two_candidates.candidates_da.count = 2;

    double e2 = propagation_compute_entropy(&two_candidates);
    TEST_ASSERT(fabs(e2 - 1.0) < 1e-12, "2 candidates => entropy == 1.0");

    /* NULL 输入 */
    double e_null = propagation_compute_entropy(NULL);
    TEST_ASSERT(e_null == PROP_ENTROPY_UNBOUNDED, "compute_entropy(NULL) returns -1.0");

    printf("  PASS: entropy\n");
}

/* ================================================================
 * 测试 8: propagation_wfc_solve 完整 WFC 求解
 * ================================================================ */
void test_prop_wfc_solve(void) {
    printf("  TEST: propagation_wfc_solve...\n");

    ConstraintGraph *graph = graph_create();

    /* 两个已坍缩的点 */
    SymbolicCoord *c0[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    SymbolicCoord *c1[2] = {symbolic_coord_create_rational(1, 1), symbolic_coord_create_rational(0, 1)};
    graph_add_point(graph, c0, 2);
    graph_add_point(graph, c1, 2);

    PropagationContext *ctx = propagation_context_create(graph);
    propagation_init_state_spaces(ctx);

    PropagationResult result = propagation_wfc_solve(ctx);
    TEST_ASSERT(result == PROP_RESULT_SATISFIED || result == PROP_RESULT_STABLE,
                "wfc_solve on simple graph returns SATISFIED or STABLE");

    propagation_context_destroy(ctx);
    symbolic_coord_destroy(c0[0]);
    symbolic_coord_destroy(c0[1]);
    symbolic_coord_destroy(c1[0]);
    symbolic_coord_destroy(c1[1]);
    graph_destroy(graph);

    /* NULL input */
    PropagationResult null_result = propagation_wfc_solve(NULL);
    TEST_ASSERT(null_result == PROP_RESULT_CONTRADICTION, "wfc_solve(NULL) returns CONTRADICTION");

    printf("  PASS: wfc_solve\n");
}

/* ================================================================
 * 测试 9: 快照保存/恢复/销毁
 * ================================================================ */
void test_prop_snapshot(void) {
    printf("  TEST: propagation_snapshot_save/restore/destroy...\n");

    ConstraintGraph *graph = graph_create();

    SymbolicCoord *c0[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    graph_add_point(graph, c0, 2);

    PropagationContext *ctx = propagation_context_create(graph);
    propagation_init_state_spaces(ctx);

    /* 保存快照 */
    PropagationSnapshot *snap = propagation_snapshot_save(ctx);
    TEST_ASSERT(snap != NULL, "snapshot_save returns non-NULL");
    TEST_ASSERT(snap->state_count == ctx->state_count, "snapshot state_count matches");
    TEST_ASSERT(snap->propagation_steps == ctx->propagation_steps, "snapshot steps match");

    /* 修改上下文 */
    ctx->propagation_steps = 42;
    ctx->collapse_count = 7;
    ctx->prune_count = 3;

    /* 恢复（snap 所有权转移，被销毁） */
    propagation_snapshot_restore(ctx, snap);

    TEST_ASSERT(ctx->propagation_steps == 0, "restored propagation_steps == 0");
    TEST_ASSERT(ctx->collapse_count == 0, "restored collapse_count == 0");
    TEST_ASSERT(ctx->prune_count == 0, "restored prune_count == 0");

    /* create+dump 模式测试：保存后立即销毁（不恢复） */
    PropagationSnapshot *snap2 = propagation_snapshot_save(ctx);
    TEST_ASSERT(snap2 != NULL, "second snapshot save succeeds");
    propagation_snapshot_destroy(snap2);

    propagation_context_destroy(ctx);
    symbolic_coord_destroy(c0[0]);
    symbolic_coord_destroy(c0[1]);
    graph_destroy(graph);

    /* NULL safety */
    PropagationSnapshot *null_snap = propagation_snapshot_save(NULL);
    TEST_ASSERT(null_snap == NULL, "snapshot_save(NULL) returns NULL");
    propagation_snapshot_restore(NULL, NULL);
    propagation_snapshot_destroy(NULL);

    printf("  PASS: snapshot\n");
}

/* ================================================================
 * 测试 10: 所有公共函数的 NULL 输入安全性
 * ================================================================ */
void test_prop_null_safety(void) {
    printf("  TEST: NULL input safety for all public functions...\n");

    /* propagation_context_create — 已在 test_prop_lifecycle 中测试 */
    /* propagation_context_destroy — 已在 test_prop_lifecycle 中测试 */
    /* propagation_init_state_spaces — 已在 test_prop_init_empty 中测试 */
    /* propagation_get_state_space — 已在 test_prop_get_state_space 中测试 */
    /* propagation_run — 已在 test_prop_run_simple 中测试 */
    /* propagation_select_node — 已在 test_prop_select_node 中测试 */
    /* propagation_collapse — 已在 test_prop_collapse 中测试 */
    /* propagation_compute_entropy — 已在 test_prop_entropy 中测试 */
    /* propagation_wfc_solve — 已在 test_prop_wfc_solve 中测试 */
    /* propagation_snapshot_save/restore/destroy — 已在 test_prop_snapshot 中测试 */

    /* 剩余需要测试的 NULL 安全函数 */
    /* propagation_set_strategy */
    propagation_set_strategy(NULL, PROP_STRATEGY_MIN_ENTROPY);
    TEST_ASSERT(1, "propagation_set_strategy(NULL) does not crash");

    /* propagation_set_collapse_strategy */
    propagation_set_collapse_strategy(NULL, PROP_COLLAPSE_FIRST);
    TEST_ASSERT(1, "propagation_set_collapse_strategy(NULL) does not crash");

    /* propagation_set_stream_context */
    propagation_set_stream_context(NULL, NULL);
    TEST_ASSERT(1, "propagation_set_stream_context(NULL) does not crash");

    /* propagation_set_max_iterations */
    propagation_set_max_iterations(NULL, 100);
    TEST_ASSERT(1, "propagation_set_max_iterations(NULL) does not crash");

    /* propagation_set_max_backtracks */
    propagation_set_max_backtracks(NULL, 10);
    TEST_ASSERT(1, "propagation_set_max_backtracks(NULL) does not crash");

    /* propagation_get_statistics */
    propagation_get_statistics(NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT(1, "propagation_get_statistics(NULL) does not crash");

    /* propagation_count_uncollapsed */
    int unc = propagation_count_uncollapsed(NULL);
    TEST_ASSERT(unc == 0, "propagation_count_uncollapsed(NULL) returns 0");

    /* propagation_is_fully_collapsed */
    bool full = propagation_is_fully_collapsed(NULL);
    TEST_ASSERT(!full, "propagation_is_fully_collapsed(NULL) returns false");

    printf("  PASS: NULL safety\n");
}

/* ================================================================
 * 主函数
 * ================================================================ */
TEST_MAIN_BEGIN("Propagation (WFC约束传播) 单元测试")
    TEST_MAIN_RUN(test_prop_lifecycle);
    TEST_MAIN_RUN(test_prop_init_empty);
    TEST_MAIN_RUN(test_prop_get_state_space);
    TEST_MAIN_RUN(test_prop_run_simple);
    TEST_MAIN_RUN(test_prop_select_node);
    TEST_MAIN_RUN(test_prop_collapse);
    TEST_MAIN_RUN(test_prop_entropy);
    TEST_MAIN_RUN(test_prop_wfc_solve);
    TEST_MAIN_RUN(test_prop_snapshot);
    TEST_MAIN_RUN(test_prop_null_safety);
TEST_MAIN_END()
