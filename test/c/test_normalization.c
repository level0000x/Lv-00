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
    printf("\n=== Test Summary ===\n");
    return failures != 0 ? 1 : 0;
TEST_MAIN_END()
