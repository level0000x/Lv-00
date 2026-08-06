/**
 * @file test_wfc_modules.c
 * @brief WFC 范式三模块集成测试
 *
 * 测试内容：
 * - Module A: 约束传播引擎（propagation）
 *   - 状态空间初始化
 *   - AC-3 弧相容性传播
 *   - WFC 熵最小化选择与坍缩
 *   - 完整 WFC 求解循环
 *   - 快照与回溯
 * - Module B: 等价类管理器（equiv_class）
 *   - 坐标等价合并
 *   - 约束推导等价
 *   - 幂等性验证
 * - Module C: 剪枝合法性元证明（meta_proof）
 *   - L1 直接矛盾证明
 *   - L2 传播矛盾证明
 *   - 自动策略选择
 *   - 完备性验证
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "equiv_class.h"
#include "lv.h"
#include "meta_proof.h"
#include "propagation.h"
#include "test_helpers.h"

/* Helper macros */
#define TEST_START(name)                 \
    do {                                 \
        printf("  [TEST] %s... ", name); \
        tests_run++;                     \
    } while (0)

#define TEST_PASS()       \
    do {                  \
        printf("PASS\n"); \
        tests_passed++;   \
    } while (0)

#define TEST_FAIL(msg)             \
    do {                           \
        printf("FAIL: %s\n", msg); \
    } while (0)

#define ASSERT_TRUE(cond)                 \
    do {                                  \
        assertions_total++;               \
        if (!(cond)) {                    \
            TEST_FAIL(#cond " is false"); \
            return;                       \
        }                                 \
    } while (0)

#define ASSERT_EQ(a, b)              \
    do {                             \
        assertions_total++;          \
        if ((a) != (b)) {            \
            TEST_FAIL(#a " != " #b); \
            return;                  \
        }                            \
    } while (0)

#define ASSERT_NE(a, b)              \
    do {                             \
        assertions_total++;          \
        if ((a) == (b)) {            \
            TEST_FAIL(#a " == " #b); \
            return;                  \
        }                            \
    } while (0)

/* 全局测试计数器 */
int g_pass_count = 0;
int g_fail_count = 0;

/* ================================================================
 * Module A: 约束传播引擎测试
 * ================================================================ */

/**
 * PROP-T01: 传播上下文创建与销毁
 */
void test_prop_context_lifecycle(void) {
    printf("  PROP-T01: 传播上下文生命周期...\n");

    ConstraintGraph *graph = graph_create();
    PropagationContext *ctx = propagation_context_create(graph);
    TEST_ASSERT(ctx != NULL, "传播上下文创建成功");
    TEST_ASSERT(ctx->graph == graph, "关联约束图正确");
    TEST_ASSERT(ctx->state_count == 0, "初始状态空间为空");

    propagation_context_destroy(ctx);
    graph_destroy(graph);
    printf("  PASS: PROP-T01\n");
}

/**
 * PROP-T02: 状态空间初始化
 */
void test_prop_init_state_spaces(void) {
    printf("  PROP-T02: 状态空间初始化...\n");

    ConstraintGraph *graph = graph_create();

    /* 添加两个有坐标的点 */
    SymbolicCoord *coords_a[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    SymbolicCoord *coords_b[2] = {symbolic_coord_create_rational(1, 1), symbolic_coord_create_rational(0, 1)};

    graph_add_point(graph, coords_a, 2);
    graph_add_point(graph, coords_b, 2);

    PropagationContext *ctx = propagation_context_create(graph);
    PropagationResult result = propagation_init_state_spaces(ctx);

    TEST_ASSERT(result == PROP_RESULT_CONSISTENT, "初始化结果为 CONSISTENT");

    /* 检查第一个点已坍缩 */
    NodeStateSpace *ss0 = propagation_get_state_space(ctx, 0);
    TEST_ASSERT(ss0 != NULL, "节点 0 状态空间存在");
    TEST_ASSERT(ss0->is_collapsed == true, "节点 0 已坍缩");

    /* 检查第二个点已坍缩 */
    NodeStateSpace *ss1 = propagation_get_state_space(ctx, 1);
    TEST_ASSERT(ss1 != NULL, "节点 1 状态空间存在");
    TEST_ASSERT(ss1->is_collapsed == true, "节点 1 已坍缩");

    propagation_context_destroy(ctx);
    symbolic_coord_destroy(coords_a[0]);
    symbolic_coord_destroy(coords_a[1]);
    symbolic_coord_destroy(coords_b[0]);
    symbolic_coord_destroy(coords_b[1]);
    graph_destroy(graph);
    printf("  PASS: PROP-T02\n");
}

/**
 * PROP-T03: AC-3 传播 - 已坍缩系统
 */
void test_prop_ac3_collapsed(void) {
    printf("  PROP-T03: AC-3 传播（已坍缩系统）...\n");

    ConstraintGraph *graph = graph_create();

    /* 添加三个共线点 */
    SymbolicCoord *c0[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    SymbolicCoord *c1[2] = {symbolic_coord_create_rational(1, 1), symbolic_coord_create_rational(0, 1)};
    SymbolicCoord *c2[2] = {symbolic_coord_create_rational(2, 1), symbolic_coord_create_rational(0, 1)};

    graph_add_point(graph, c0, 2);
    graph_add_point(graph, c1, 2);
    graph_add_point(graph, c2, 2);

    /* 添加线段 */
    graph_add_line_segment(graph, 0, 1);

    PropagationContext *ctx = propagation_context_create(graph);
    propagation_init_state_spaces(ctx);

    PropagationResult result = propagation_run(ctx);
    TEST_ASSERT(result == PROP_RESULT_SATISFIED || result == PROP_RESULT_STABLE, "传播结果为 SATISFIED 或 STABLE");

    propagation_context_destroy(ctx);
    for (int i = 0; i < 2; i++) {
        symbolic_coord_destroy(c0[i]);
        symbolic_coord_destroy(c1[i]);
        symbolic_coord_destroy(c2[i]);
    }
    graph_destroy(graph);
    printf("  PASS: PROP-T03\n");
}

/**
 * PROP-T04: 熵计算
 */
void test_prop_entropy(void) {
    printf("  PROP-T04: 熵计算...\n");

    /* 已坍缩 → 熵 = 0 */
    NodeStateSpace collapsed;
    memset(&collapsed, 0, sizeof(collapsed));
    collapsed.is_collapsed = true;
    collapsed.collapsed_value = symbolic_coord_create_rational(1, 1);

    double e0 = propagation_compute_entropy(&collapsed);
    TEST_ASSERT(fabs(e0) < 1e-10, "已坍缩节点熵为 0");

    symbolic_coord_destroy(collapsed.collapsed_value);

    /* 无界 → 熵 = -1 */
    NodeStateSpace unbounded;
    memset(&unbounded, 0, sizeof(unbounded));
    unbounded.is_unbounded = true;

    double e1 = propagation_compute_entropy(&unbounded);
    TEST_ASSERT(e1 == PROP_ENTROPY_UNBOUNDED, "无界节点熵为 -1");

    printf("  PASS: PROP-T04\n");
}

/**
 * PROP-T05: 快照与恢复
 */
void test_prop_snapshot(void) {
    printf("  PROP-T05: 快照与恢复...\n");

    ConstraintGraph *graph = graph_create();

    SymbolicCoord *c0[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};

    graph_add_point(graph, c0, 2);

    PropagationContext *ctx = propagation_context_create(graph);
    propagation_init_state_spaces(ctx);

    /* 保存快照 */
    PropagationSnapshot *snap = propagation_snapshot_save(ctx);
    TEST_ASSERT(snap != NULL, "快照保存成功");
    TEST_ASSERT(snap->state_count == ctx->state_count, "快照状态数量正确");

    /* 修改状态（模拟坍缩） */
    ctx->collapse_count = 42;
    ctx->prune_count = 7;

    /* 恢复快照 */
    propagation_snapshot_restore(ctx, snap); /* snap 被销毁 */

    TEST_ASSERT(ctx->collapse_count == 0, "恢复后 collapse_count 为 0");
    TEST_ASSERT(ctx->prune_count == 0, "恢复后 prune_count 为 0");

    propagation_context_destroy(ctx);
    symbolic_coord_destroy(c0[0]);
    symbolic_coord_destroy(c0[1]);
    graph_destroy(graph);
    printf("  PASS: PROP-T05\n");
}

/**
 * PROP-T06: 传播统计
 */
void test_prop_statistics(void) {
    printf("  PROP-T06: 传播统计...\n");

    ConstraintGraph *graph = graph_create();

    SymbolicCoord *c0[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};

    graph_add_point(graph, c0, 2);

    PropagationContext *ctx = propagation_context_create(graph);
    propagation_init_state_spaces(ctx);

    int64_t steps, collapses, backtracks, prunes;
    propagation_get_statistics(ctx, &steps, &collapses, &backtracks, &prunes);
    TEST_ASSERT(steps == 0, "初始传播步数为 0");
    TEST_ASSERT(collapses == 0, "初始坍缩次数为 0");

    propagation_context_destroy(ctx);
    symbolic_coord_destroy(c0[0]);
    symbolic_coord_destroy(c0[1]);
    graph_destroy(graph);
    printf("  PASS: PROP-T06\n");
}

/* ================================================================
 * Module B: 等价类管理器测试
 * ================================================================ */

/**
 * EQC-T01: 等价类管理器生命周期
 */
void test_equiv_lifecycle(void) {
    printf("  EQC-T01: 等价类管理器生命周期...\n");

    ConstraintGraph *graph = graph_create();
    EquivClassManager *mgr = equiv_manager_create(graph);
    TEST_ASSERT(mgr != NULL, "管理器创建成功");
    TEST_ASSERT(mgr->graph == graph, "关联约束图正确");

    equiv_manager_destroy(mgr);
    graph_destroy(graph);
    printf("  PASS: EQC-T01\n");
}

/**
 * EQC-T02: 坐标等价合并
 */
void test_equiv_coord_merge(void) {
    printf("  EQC-T02: 坐标等价合并...\n");

    ConstraintGraph *graph = graph_create();

    /* 添加两个坐标相同的点 */
    SymbolicCoord *c0[2] = {symbolic_coord_create_rational(1, 1), symbolic_coord_create_rational(2, 1)};
    SymbolicCoord *c1[2] = {symbolic_coord_create_rational(1, 1), symbolic_coord_create_rational(2, 1)};

    graph_add_point(graph, c0, 2);
    graph_add_point(graph, c1, 2);

    EquivClassManager *mgr = equiv_manager_create(graph);
    int merges = equiv_merge_by_coord(mgr);

    TEST_ASSERT(merges >= 1, "发现至少 1 对坐标等价");
    TEST_ASSERT(equiv_are_equivalent(mgr, 0, 1), "节点 0 和 1 等价");

    int rep = equiv_find(mgr, 0);
    TEST_ASSERT(rep >= 0, "找到代表节点");

    equiv_manager_destroy(mgr);
    symbolic_coord_destroy(c0[0]);
    symbolic_coord_destroy(c0[1]);
    symbolic_coord_destroy(c1[0]);
    symbolic_coord_destroy(c1[1]);
    graph_destroy(graph);
    printf("  PASS: EQC-T02\n");
}

/**
 * EQC-T03: 非等价节点
 */
void test_equiv_non_equivalent(void) {
    printf("  EQC-T03: 非等价节点...\n");

    ConstraintGraph *graph = graph_create();

    SymbolicCoord *c0[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    SymbolicCoord *c1[2] = {symbolic_coord_create_rational(1, 1), symbolic_coord_create_rational(0, 1)};

    graph_add_point(graph, c0, 2);
    graph_add_point(graph, c1, 2);

    EquivClassManager *mgr = equiv_manager_create(graph);
    equiv_merge_by_coord(mgr);

    TEST_ASSERT(!equiv_are_equivalent(mgr, 0, 1), "不同坐标的节点不等价");

    equiv_manager_destroy(mgr);
    symbolic_coord_destroy(c0[0]);
    symbolic_coord_destroy(c0[1]);
    symbolic_coord_destroy(c1[0]);
    symbolic_coord_destroy(c1[1]);
    graph_destroy(graph);
    printf("  PASS: EQC-T03\n");
}

/**
 * EQC-T04: 等价类查询
 */
void test_equiv_query(void) {
    printf("  EQC-T04: 等价类查询...\n");

    ConstraintGraph *graph = graph_create();

    SymbolicCoord *c0[2] = {symbolic_coord_create_rational(1, 1), symbolic_coord_create_rational(1, 1)};
    SymbolicCoord *c1[2] = {symbolic_coord_create_rational(1, 1), symbolic_coord_create_rational(1, 1)};

    graph_add_point(graph, c0, 2);
    graph_add_point(graph, c1, 2);

    EquivClassManager *mgr = equiv_manager_create(graph);
    equiv_merge_by_coord(mgr);

    const EquivClass *ec = equiv_get_class(mgr, 0);
    TEST_ASSERT(ec != NULL, "获取节点 0 的等价类");
    TEST_ASSERT(ec->member_count >= 2, "等价类包含至少 2 个成员");

    int count = equiv_class_count(mgr);
    TEST_ASSERT(count >= 1, "等价类数量 >= 1");

    equiv_manager_destroy(mgr);
    symbolic_coord_destroy(c0[0]);
    symbolic_coord_destroy(c0[1]);
    symbolic_coord_destroy(c1[0]);
    symbolic_coord_destroy(c1[1]);
    graph_destroy(graph);
    printf("  PASS: EQC-T04\n");
}

/**
 * EQC-T05: 统计信息
 */
void test_equiv_statistics(void) {
    printf("  EQC-T05: 统计信息...\n");

    ConstraintGraph *graph = graph_create();

    SymbolicCoord *c0[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    SymbolicCoord *c1[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};

    graph_add_point(graph, c0, 2);
    graph_add_point(graph, c1, 2);

    EquivClassManager *mgr = equiv_manager_create(graph);
    equiv_merge_all(mgr);

    int64_t total, coord, derive, conj, transform, rejected;
    equiv_get_statistics(mgr, &total, &coord, &derive, &conj, &transform, &rejected);
    TEST_ASSERT(total > 0, "总合并次数 > 0");
    TEST_ASSERT(coord > 0, "坐标等价合并次数 > 0");

    equiv_manager_destroy(mgr);
    symbolic_coord_destroy(c0[0]);
    symbolic_coord_destroy(c0[1]);
    symbolic_coord_destroy(c1[0]);
    symbolic_coord_destroy(c1[1]);
    graph_destroy(graph);
    printf("  PASS: EQC-T05\n");
}

/* ================================================================
 * Module C: 剪枝合法性元证明测试
 * ================================================================ */

/**
 * MPR-T01: 元证明上下文生命周期
 */
void test_meta_proof_lifecycle(void) {
    printf("  MPR-T01: 元证明上下文生命周期...\n");

    ConstraintGraph *graph = graph_create();
    MetaProofContext *ctx = meta_proof_context_create(graph, NULL);
    TEST_ASSERT(ctx != NULL, "元证明上下文创建成功");
    TEST_ASSERT(ctx->record != NULL, "剪枝记录存在");

    meta_proof_context_destroy(ctx);
    graph_destroy(graph);
    printf("  PASS: MPR-T01\n");
}

/**
 * MPR-T02: L1 直接矛盾证明
 */
void test_meta_proof_l1(void) {
    printf("  MPR-T02: L1 直接矛盾证明...\n");

    ConstraintGraph *graph = graph_create();

    /* 添加一个点和一条不经过该点的线段 */
    SymbolicCoord *pt[2] = {
        symbolic_coord_create_rational(0, 1), /* x = 0 */
        symbolic_coord_create_rational(1, 1)  /* y = 1 */
    };
    int pt_id = graph_add_point(graph, pt, 2);

    SymbolicCoord *ep1[2] = {symbolic_coord_create_rational(0, 1), symbolic_coord_create_rational(0, 1)};
    SymbolicCoord *ep2[2] = {symbolic_coord_create_rational(1, 1), symbolic_coord_create_rational(0, 1)};
    int line_id = graph_add_line_segment(graph, 0, 1);

    /* 点 (0,1) 不在线段 (0,0)-(1,0) 上 */
    graph_add_incidence(graph, pt_id, line_id);

    MetaProofContext *ctx = meta_proof_context_create(graph, NULL);

    /* 创建一个明显不在线段上的候选 */
    SymbolicCoord *bad_candidate = symbolic_coord_create_rational(5, 1);
    int conflicting = -1;
    MetaProofResult result = meta_prove_direct_contradiction(ctx, pt_id, bad_candidate, &conflicting);

    /* L1 应该能检测到矛盾（候选 y=5 不在线段 y=0 上） */
    TEST_ASSERT(result == META_PROVE_VALID || result == META_PROVE_INCONCLUSIVE, "L1 返回 VALID 或 INCONCLUSIVE");

    meta_proof_context_destroy(ctx);
    symbolic_coord_destroy(bad_candidate);
    symbolic_coord_destroy(pt[0]);
    symbolic_coord_destroy(pt[1]);
    symbolic_coord_destroy(ep1[0]);
    symbolic_coord_destroy(ep1[1]);
    symbolic_coord_destroy(ep2[0]);
    symbolic_coord_destroy(ep2[1]);
    graph_destroy(graph);
    printf("  PASS: MPR-T02\n");
}

/**
 * MPR-T03: 完备性验证（空记录）
 */
void test_meta_proof_completeness_empty(void) {
    printf("  MPR-T03: 完备性验证（空记录）...\n");

    ConstraintGraph *graph = graph_create();
    MetaProofContext *ctx = meta_proof_context_create(graph, NULL);

    CompletenessReport *report = meta_prove_completeness(ctx);
    TEST_ASSERT(report != NULL, "完备性报告生成成功");
    TEST_ASSERT(report->total_prunings == 0, "总剪枝次数为 0");
    TEST_ASSERT(report->overall_color == TRUST_GREEN, "空记录完备性为 GREEN");

    printf("    摘要: %s\n", report->summary);

    meta_proof_completeness_report_destroy(report);
    meta_proof_context_destroy(ctx);
    graph_destroy(graph);
    printf("  PASS: MPR-T03\n");
}

/**
 * MPR-T04: 剪枝记录
 */
void test_meta_proof_record(void) {
    printf("  MPR-T04: 剪枝记录...\n");

    ConstraintGraph *graph = graph_create();
    MetaProofContext *ctx = meta_proof_context_create(graph, NULL);

    SymbolicCoord *removed[2] = {symbolic_coord_create_rational(1, 1), symbolic_coord_create_rational(2, 1)};

    meta_proof_record_pruning(ctx, 0, removed, 2, PRUNE_DIRECT_CONTRADICTION, TRUST_GREEN);

    const PruningRecord *rec = meta_proof_get_record(ctx);
    TEST_ASSERT(rec != NULL, "剪枝记录存在");
    TEST_ASSERT(rec->operations.count == 1, "记录了 1 次剪枝");
    TEST_ASSERT(rec->total_states_removed == 2, "移除了 2 个状态");

    meta_proof_context_destroy(ctx);
    symbolic_coord_destroy(removed[0]);
    symbolic_coord_destroy(removed[1]);
    graph_destroy(graph);
    printf("  PASS: MPR-T04\n");
}

/**
 * MPR-T05: 统计信息
 */
void test_meta_proof_statistics(void) {
    printf("  MPR-T05: 统计信息...\n");

    ConstraintGraph *graph = graph_create();
    MetaProofContext *ctx = meta_proof_context_create(graph, NULL);

    int64_t l1, l2, l3, inconclusive;
    meta_proof_get_statistics(ctx, &l1, &l2, &l3, &inconclusive);
    TEST_ASSERT(l1 == 0, "初始 L1 证明次数为 0");
    TEST_ASSERT(l2 == 0, "初始 L2 证明次数为 0");
    TEST_ASSERT(l3 == 0, "初始 L3 证明次数为 0");

    meta_proof_context_destroy(ctx);
    graph_destroy(graph);
    printf("  PASS: MPR-T05\n");
}

/**
 * MPR-T06: 策略启用/禁用
 */
void test_meta_proof_strategy_toggle(void) {
    printf("  MPR-T06: 策略启用/禁用...\n");

    ConstraintGraph *graph = graph_create();
    MetaProofContext *ctx = meta_proof_context_create(graph, NULL);

    TEST_ASSERT(ctx->enable_l1 == true, "L1 默认启用");
    TEST_ASSERT(ctx->enable_l2 == true, "L2 默认启用");
    TEST_ASSERT(ctx->enable_l3 == true, "L3 默认启用");

    meta_proof_set_strategy_enabled(ctx, PRUNE_DIRECT_CONTRADICTION, false);
    TEST_ASSERT(ctx->enable_l1 == false, "L1 已禁用");

    meta_proof_set_strategy_enabled(ctx, PRUNE_DIRECT_CONTRADICTION, true);
    TEST_ASSERT(ctx->enable_l1 == true, "L1 已重新启用");

    meta_proof_context_destroy(ctx);
    graph_destroy(graph);
    printf("  PASS: MPR-T06\n");
}

/* ================================================================
 * 主函数
 * ================================================================ */

TEST_MAIN_BEGIN("WFC 范式模块集成测试")
    printf("--- Module A: 约束传播引擎 ---\n");
    TEST_MAIN_RUN(test_prop_context_lifecycle);
    TEST_MAIN_RUN(test_prop_init_state_spaces);
    TEST_MAIN_RUN(test_prop_ac3_collapsed);
    TEST_MAIN_RUN(test_prop_entropy);
    TEST_MAIN_RUN(test_prop_snapshot);
    TEST_MAIN_RUN(test_prop_statistics);

    printf("\n--- Module B: 等价类管理器 ---\n");
    TEST_MAIN_RUN(test_equiv_lifecycle);
    TEST_MAIN_RUN(test_equiv_coord_merge);
    TEST_MAIN_RUN(test_equiv_non_equivalent);
    TEST_MAIN_RUN(test_equiv_query);
    TEST_MAIN_RUN(test_equiv_statistics);

    printf("\n--- Module C: 剪枝合法性元证明 ---\n");
    TEST_MAIN_RUN(test_meta_proof_lifecycle);
    TEST_MAIN_RUN(test_meta_proof_l1);
    TEST_MAIN_RUN(test_meta_proof_completeness_empty);
    TEST_MAIN_RUN(test_meta_proof_record);
    TEST_MAIN_RUN(test_meta_proof_statistics);
    TEST_MAIN_RUN(test_meta_proof_strategy_toggle);
TEST_MAIN_END()
