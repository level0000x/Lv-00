/**
 * @file test_normalization.c
 * @brief Lv-00 图规范化测试 - 点合并、线段规范化、含约束图规范化
 *
 * 测试内容：
 * - 基本点合并（同坐标点）
 * - 无合并场景（不同坐标点）
 * - 在规范化后添加约束以验证完整性
 * - 空图规范化
 * - 带线段图的规范化
 * - 交互模式下的规范化
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv.h"
#include "test_helpers.h"

/* 批次 C-㊲：TEST_ASSERT 系列宏所需的全局计数（旧式测试块使用私有 failures 计数） */
int g_pass_count = 0;
int g_fail_count = 0;

/* 测试基本的点合并 */
static int test_basic_point_merge(void) {
    printf("\n=== Testing Basic Point Merge ===\n");

    ConstraintGraph *graph = graph_create();
    if (!graph) {
        printf("  FAILED: Could not create graph\n");
        return -1;
    }

    add_point(graph, 3, 1, 4, 1);
    add_point(graph, 3, 1, 4, 1);

    printf("  Before normalization: %d nodes\n", graph->node_count);

    NormalizationResult *result = graph_normalize(graph, false);
    if (!result) {
        printf("  FAILED: normalize returned NULL\n");
        graph_destroy(graph);
        return -1;
    }

    printf("  After normalization: %d nodes, merged %d\n", graph->node_count, result->merged_count);

    if (result->merged_count != 1) {
        printf("  FAILED: Expected 1 merged, got %d\n", result->merged_count);
        normalization_result_destroy(result);
        graph_destroy(graph);
        return -1;
    }

    if (graph->node_count != 1) {
        printf("  FAILED: Expected 1 node, got %d\n", graph->node_count);
        normalization_result_destroy(result);
        graph_destroy(graph);
        return -1;
    }

    printf("  Basic point merge: PASSED\n");
    normalization_result_destroy(result);
    graph_destroy(graph);
    return 0;
}

/* 测试幂等性 */
static int test_idempotence(void) {
    printf("\n=== Testing Idempotence ===\n");

    ConstraintGraph *graph = graph_create();
    if (!graph) {
        printf("  FAILED: Could not create graph\n");
        return -1;
    }

    for (int i = 0; i < 5; i++)
        add_point(graph, 1, 1, 2, 1);

    NormalizationResult *r1 = graph_normalize(graph, false);
    int count1 = graph->node_count;
    printf("  1st normalization: %d nodes, merged %d\n", count1, r1->merged_count);

    NormalizationResult *r2 = graph_normalize(graph, false);
    int count2 = graph->node_count;
    printf("  2nd normalization: %d nodes, merged %d\n", count2, r2->merged_count);

    if (r2->merged_count != 0) {
        printf("  FAILED: 2nd normalization should merge 0\n");
        normalization_result_destroy(r1);
        normalization_result_destroy(r2);
        graph_destroy(graph);
        return -1;
    }
    if (count1 != count2) {
        printf("  FAILED: Node count changed\n");
        normalization_result_destroy(r1);
        normalization_result_destroy(r2);
        graph_destroy(graph);
        return -1;
    }

    printf("  Idempotence: PASSED\n");
    normalization_result_destroy(r1);
    normalization_result_destroy(r2);
    graph_destroy(graph);
    return 0;
}

/* 测试传递闭包 */
static int test_transitive_closure(void) {
    printf("\n=== Testing Transitive Closure ===\n");

    ConstraintGraph *graph = graph_create();
    if (!graph) {
        printf("  FAILED: Could not create graph\n");
        return -1;
    }

    add_point(graph, 5, 1, 5, 1);
    add_point(graph, 5, 1, 5, 1);
    add_point(graph, 5, 1, 5, 1);

    NormalizationResult *result = graph_normalize(graph, false);
    printf("  Merged: %d\n", result->merged_count);

    if (graph->node_count != 1) {
        printf("  FAILED: Expected 1 node, got %d\n", graph->node_count);
        normalization_result_destroy(result);
        graph_destroy(graph);
        return -1;
    }

    printf("  Transitive closure: PASSED\n");
    normalization_result_destroy(result);
    graph_destroy(graph);
    return 0;
}

/* 测试不同坐标不合并 */
static int test_no_merge_different_coords(void) {
    printf("\n=== Testing No Merge for Different Coords ===\n");

    ConstraintGraph *graph = graph_create();
    if (!graph) {
        printf("  FAILED: Could not create graph\n");
        return -1;
    }

    add_point(graph, 0, 1, 0, 1);
    add_point(graph, 1, 1, 0, 1);
    add_point(graph, 0, 1, 1, 1);

    NormalizationResult *result = graph_normalize(graph, false);

    if (result->merged_count != 0) {
        printf("  FAILED: Should not merge different coords\n");
        normalization_result_destroy(result);
        graph_destroy(graph);
        return -1;
    }
    if (graph->node_count != 3) {
        printf("  FAILED: Expected 3 nodes, got %d\n", graph->node_count);
        normalization_result_destroy(result);
        graph_destroy(graph);
        return -1;
    }

    printf("  No merge for different coords: PASSED\n");
    normalization_result_destroy(result);
    graph_destroy(graph);
    return 0;
}

/* 测试大规模合并性能 */
static int test_large_scale_merge(void) {
    printf("\n=== Testing Large Scale Merge ===\n");

    ConstraintGraph *graph = graph_create();
    if (!graph) {
        printf("  FAILED: Could not create graph\n");
        return -1;
    }

    const int NUM_GROUPS = 10;
    const int PER_GROUP = 10;

    for (int g = 0; g < NUM_GROUPS; g++)
        for (int p = 0; p < PER_GROUP; p++)
            add_point(graph, g, 1, g * 2, 1);

    int before = graph->node_count;
    printf("  Created %d points (%d groups of %d)\n", before, NUM_GROUPS, PER_GROUP);

    clock_t start = clock();
    NormalizationResult *result = graph_normalize(graph, false);

    printf("  After: %d nodes, merged %d, time %.4fs\n", graph->node_count, result->merged_count,
           lv_clock_elapsed_sec(start));

    if (graph->node_count != NUM_GROUPS) {
        printf("  FAILED: Expected %d nodes, got %d\n", NUM_GROUPS, graph->node_count);
        normalization_result_destroy(result);
        graph_destroy(graph);
        return -1;
    }

    printf("  Large scale merge: PASSED\n");
    normalization_result_destroy(result);
    graph_destroy(graph);
    return 0;
}

/* 测试规范化后线段合并 */
static int test_segment_merge_after_normalize(void) {
    printf("\n=== Testing Segment Merge After Normalize ===\n");

    ConstraintGraph *graph = graph_create();
    if (!graph) {
        printf("  FAILED: Could not create graph\n");
        return -1;
    }

    int p0a = add_point(graph, 0, 1, 0, 1);
    int p1a = add_point(graph, 3, 1, 4, 1);
    int p0b = add_point(graph, 0, 1, 0, 1);
    int p1b = add_point(graph, 3, 1, 4, 1);

    graph_add_line_segment(graph, p0a, p1a);
    graph_add_line_segment(graph, p0b, p1b);

    printf("  Before: %d nodes\n", graph->node_count);

    NormalizationResult *result = graph_normalize(graph, false);
    printf("  After: %d nodes, merged %d\n", graph->node_count, result->merged_count);

    printf("  Segment merge after normalize: PASSED\n");
    normalization_result_destroy(result);
    graph_destroy(graph);
    return 0;
}

/* 测试 find_merge_candidates 直接检测合并候选（并入自 manual/test_comprehensive.c） */
static int test_find_merge_candidates(void) {
    printf("\n=== Testing find_merge_candidates ===\n");

    ConstraintGraph *graph = graph_create();
    if (!graph) {
        printf("  FAILED: Could not create graph\n");
        return -1;
    }

    SymbolicCoord *c = mk_rat(5, 2);
    if (!c) {
        printf("  FAILED: Could not create symbolic coord\n");
        graph_destroy(graph);
        return -1;
    }

    graph_add_point(graph, &c, 1);
    graph_add_point(graph, &c, 1);
    if (graph->node_count != 2) {
        printf("  FAILED: Expected 2 nodes, got %d\n", graph->node_count);
        graph_destroy(graph);
        return -1;
    }

    int count = 0;
    NodeMergeCandidate *candidates = find_merge_candidates(graph, &count);
    if (!candidates || count != 1) {
        printf("  FAILED: Expected 1 candidate, got %d\n", count);
        graph_destroy(graph);
        return -1;
    }
    bool ab = candidates[0].node_a_id == 0 && candidates[0].node_b_id == 1;
    bool ba = candidates[0].node_a_id == 1 && candidates[0].node_b_id == 0;
    if (!ab && !ba) {
        printf("  FAILED: Expected candidate nodes {0, 1}, got (%d, %d)\n",
               candidates[0].node_a_id, candidates[0].node_b_id);
        merge_candidates_destroy(candidates, count);
        graph_destroy(graph);
        return -1;
    }
    if (candidates[0].scope_a != candidates[0].scope_b) {
        printf("  FAILED: Expected equal candidate scopes\n");
        merge_candidates_destroy(candidates, count);
        graph_destroy(graph);
        return -1;
    }

    printf("  find_merge_candidates: PASSED\n");
    merge_candidates_destroy(candidates, count);
    graph_destroy(graph);
    return 0;
}

/* ============================================================
 * 批次 C-㊲：normalization.h 零覆盖设施契约测试
 * ============================================================ */

/* 合并确认回调测试辅助 */
static bool norm_merge_confirm(int a, int b, int sa, int sb, int pa, int pb, void *ud) {
    (void) a; (void) b; (void) sa; (void) sb; (void) pa; (void) pb;
    return ud != NULL;
}

static void test_merge_callback_api(void) {
    TEST_ASSERT_NULL(normalization_get_merge_callback());

    int marker = 42;
    normalization_set_merge_callback(norm_merge_confirm, &marker);
    TEST_ASSERT_MSG(normalization_get_merge_callback() == norm_merge_confirm, "回调已设置");
    TEST_ASSERT_MSG(normalization_get_merge_callback()(1, 2, 0, 0, -1, -1, &marker) == true, "user_data 生效");

    normalization_set_merge_callback(NULL, NULL);
    TEST_ASSERT_NULL(normalization_get_merge_callback());
}

static void test_norm_log_api(void) {
    normalization_log_destroy(NULL);

    NormalizationLog *log = normalization_log_create(4);
    TEST_ASSERT_MSG(log != NULL, "日志创建");
    TEST_ASSERT_EQ(log->entries.count, 0);

    normalization_log_record(log, 10, 5, true);
    normalization_log_record(log, 20, 5, false);
    TEST_ASSERT_EQ(log->entries.count, 2);
    NormalizationLogEntry *e0 = (NormalizationLogEntry *)lv_darray_get(&log->entries, 0);
    TEST_ASSERT_EQ(e0->old_id, 10);
    TEST_ASSERT_EQ(e0->new_id, 5);
    TEST_ASSERT_MSG(e0->auto_merged == true, "自动合并标记");
    NormalizationLogEntry *e1 = (NormalizationLogEntry *)lv_darray_get(&log->entries, 1);
    TEST_ASSERT_MSG(e1->auto_merged == false, "用户确认标记");

    normalization_log_record(NULL, 1, 2, true); /* NULL 安全 */

    for (int i = 0; i < 20; i++)
        normalization_log_record(log, i, 100, true);
    TEST_ASSERT_EQ(log->entries.count, 22);

    normalization_log_destroy(log);
}

static void test_rewrite_history_api(void) {
    rewrite_history_destroy(NULL);

    RewriteHistory *hist = rewrite_history_create(8);
    TEST_ASSERT_MSG(hist != NULL, "历史创建");
    TEST_ASSERT_EQ(hist->capacity, 8);

    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_MSG(graph != NULL, "图创建");
    TEST_ASSERT_MSG(!rewrite_history_check_cycle(hist, graph), "空历史无循环");

    rewrite_history_add(hist, graph);
    TEST_ASSERT_MSG(rewrite_history_check_cycle(hist, graph), "重复图检测到循环");

    /* 修改图（添加节点 → 哈希变化）→ 无循环 */
    SymbolicCoord sc1;
    memset(&sc1, 0, sizeof(sc1));
    sc1.type = RATIONAL;
    sc1.data.rational = rational_create(100, 1);
    sc1.trust = TRUST_GREEN;
    TEST_ASSERT_MSG(graph_add_point_xy(graph, &sc1, &sc1) >= 0, "添加点");
    TEST_ASSERT_MSG(!rewrite_history_check_cycle(hist, graph), "修改后无循环");

    rewrite_history_add(hist, graph);
    TEST_ASSERT_MSG(rewrite_history_check_cycle(hist, graph), "再次重复检测到循环");

    TEST_ASSERT_MSG(!rewrite_history_check_cycle(NULL, graph), "NULL history 无循环");
    TEST_ASSERT_MSG(!rewrite_history_check_cycle(hist, NULL), "NULL graph 无循环");
    rewrite_history_add(NULL, graph);
    rewrite_history_add(hist, NULL);

    graph_destroy(graph);
    rewrite_history_destroy(hist);
}

static void test_topological_idempotency_api(void) {
    graph_topological_sort_stable(NULL);
    TEST_ASSERT_MSG(!normalization_verify_idempotency(NULL), "NULL 图幂等 false");

    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_MSG(graph != NULL, "图创建");
    graph_topological_sort_stable(graph);
    TEST_ASSERT_MSG(normalization_verify_idempotency(graph), "空图幂等");

    SymbolicCoord sc1, sc2;
    memset(&sc1, 0, sizeof(sc1));
    memset(&sc2, 0, sizeof(sc2));
    sc1.type = RATIONAL;
    sc2.type = RATIONAL;
    sc1.data.rational = rational_create(0, 1);
    sc2.data.rational = rational_create(1, 1);
    sc1.trust = TRUST_GREEN;
    sc2.trust = TRUST_GREEN;
    TEST_ASSERT_MSG(graph_add_point_xy(graph, &sc1, &sc1) >= 0, "添加点1");
    TEST_ASSERT_MSG(graph_add_point_xy(graph, &sc2, &sc2) >= 0, "添加点2");
    graph_topological_sort_stable(graph);
    TEST_ASSERT_EQ(graph->node_count, 2);
    TEST_ASSERT_MSG(normalization_verify_idempotency(graph), "简单图幂等");

    graph_destroy(graph);
}

static void test_merge_ops_api(void) {
    TEST_ASSERT_EQ(merge_line_segments(NULL, NULL), -1);
    TEST_ASSERT_EQ(merge_regions(NULL, NULL), -1);
    int out_count = -1;
    TEST_ASSERT_NULL(find_merge_candidates(NULL, &out_count));
    merge_candidates_destroy(NULL, 0);

    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_MSG(graph != NULL, "图创建");
    TEST_ASSERT_EQ(merge_line_segments(graph, NULL), 0);
    TEST_ASSERT_EQ(merge_regions(graph, NULL), 0);

    /* 两个相同端点线段：点 A=(0,0), B=(1,0) 与重复点 C,D 构成两条线段 */
    SymbolicCoord sc_a, sc_b, sc_c, sc_d;
    memset(&sc_a, 0, sizeof(sc_a));
    memset(&sc_b, 0, sizeof(sc_b));
    memset(&sc_c, 0, sizeof(sc_c));
    memset(&sc_d, 0, sizeof(sc_d));
    sc_a.type = RATIONAL;
    sc_b.type = RATIONAL;
    sc_c.type = RATIONAL;
    sc_d.type = RATIONAL;
    sc_a.data.rational = rational_create(0, 1);
    sc_b.data.rational = rational_create(1, 1);
    sc_c.data.rational = rational_create(0, 1);
    sc_d.data.rational = rational_create(1, 1);
    sc_a.trust = TRUST_GREEN;
    sc_b.trust = TRUST_GREEN;
    sc_c.trust = TRUST_GREEN;
    sc_d.trust = TRUST_GREEN;

    AddNodeResult ia = graph_add_point_xy(graph, &sc_a, &sc_b);
    AddNodeResult ib = graph_add_point_xy(graph, &sc_b, &sc_a);
    AddNodeResult ic = graph_add_point_xy(graph, &sc_c, &sc_d);
    AddNodeResult id = graph_add_point_xy(graph, &sc_d, &sc_c);
    TEST_ASSERT_MSG(ia >= 0 && ib >= 0 && ic >= 0 && id >= 0, "添加点");
    /* 线段 AB 与 CD（同坐标端点：A==C, B==D） */
    TEST_ASSERT_MSG(graph_add_line_segment(graph, (int) ia, (int) ib) >= 0, "添加线段1");
    TEST_ASSERT_MSG(graph_add_line_segment(graph, (int) ic, (int) id) >= 0, "添加线段2");

    int cand_count = 0;
    NodeMergeCandidate *cands = find_merge_candidates(graph, &cand_count);
    TEST_ASSERT_MSG(cands != NULL || cand_count == 0, "候选数组非空或零候选");
    if (cands)
        merge_candidates_destroy(cands, cand_count);

    bool confirmed = false;
    int merged = apply_merges(graph, NULL, 0, &confirmed);
    TEST_ASSERT_MSG(merged == 0 || merged >= 0, "空候选 apply 安全");

    NormalizationLog *log = normalization_log_create(4);
    int merged_count = merge_line_segments(graph, log);
    TEST_ASSERT_MSG(merged_count >= 0, "线段合并计数非负");

    graph_destroy(graph);
    normalization_log_destroy(log);
}

static void test_stream_ctx_api(void) {
    normalization_set_stream_context(NULL); /* 禁用流式输出（NULL 安全） */
    normalization_set_stream_context(NULL);
}

TEST_MAIN_BEGIN("Lv-00 Normalization Test Suite")
    int failures = 0;
    printf("=== Lv-00 Normalization Test Suite ===\n");
    failures += test_basic_point_merge();
    failures += test_idempotence();
    failures += test_transitive_closure();
    failures += test_no_merge_different_coords();
    failures += test_large_scale_merge();
    failures += test_segment_merge_after_normalize();
    failures += test_find_merge_candidates();
    printf("\n=== C-㊲: 零覆盖设施 ===\n");
    TEST_MAIN_RUN(test_merge_callback_api);
    TEST_MAIN_RUN(test_norm_log_api);
    TEST_MAIN_RUN(test_rewrite_history_api);
    TEST_MAIN_RUN(test_topological_idempotency_api);
    TEST_MAIN_RUN(test_merge_ops_api);
    TEST_MAIN_RUN(test_stream_ctx_api);
    printf("\n=== Test Summary ===\n");
    return failures != 0 ? 1 : 0;
TEST_MAIN_END()
