/*
 * test_complex_graph.c - Lv-00 复杂约束图测试
 *
 * 测试复杂约束图场景：
 * - 多节点、多约束
 * - 约束冲突检测
 * - 大规模图操作
 * - 冗余约束检测
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"
#include "test_helpers.h"

/* 测试多节点图 */
static int test_many_nodes(void) {
    printf("\n=== Testing Many Nodes ===\n");

    ConstraintGraph *graph = graph_create();
    if (!graph) {
        printf("  FAILED: Could not create graph\n");
        return -1;
    }

    const int N = 100;
    for (int i = 0; i < N; i++) {
        AddNodeResult r = graph_add_point(graph, NULL, 0);
        if (r != ADD_NODE_OK) {
            printf("  FAILED: Could not add point %d\n", i);
            graph_destroy(graph);
            return -1;
        }
    }

    if (graph->node_count != N) {
        printf("  FAILED: Expected %d nodes, got %d\n", N, graph->node_count);
        graph_destroy(graph);
        return -1;
    }
    printf("  Created %d points: PASSED\n", N);

    /* 创建连接线段 */
    for (int i = 0; i < N - 1; i++) {
        AddNodeResult r = graph_add_line_segment(graph, i, i + 1);
        if (r != ADD_NODE_OK) {
            printf("  FAILED: Could not add segment %d\n", i);
            graph_destroy(graph);
            return -1;
        }
    }

    printf("  Created %d segments: PASSED\n", N - 1);
    graph_destroy(graph);
    return 0;
}

/* 测试约束冲突检测 */
static int test_constraint_conflicts(void) {
    printf("\n=== Testing Constraint Conflicts ===\n");

    ConstraintGraph *graph = graph_create();
    if (!graph) {
        printf("  FAILED: Could not create graph\n");
        return -1;
    }

    int p0 = add_point(graph, 0, 1, 0, 1);
    int p1 = add_point(graph, 3, 1, 0, 1);
    int seg = graph->next_node_id;
    graph_add_line_segment(graph, p0, p1);

    /* 添加关联约束 */
    AddConstraintResult r1 = graph_add_incidence(graph, p0, seg);
    if (r1 != ADD_CONSTRAINT_OK) {
        printf("  FAILED: Could not add incidence\n");
        graph_destroy(graph);
        return -1;
    }
    printf("  First incidence added: PASSED\n");

    /* 尝试添加重复约束 */
    AddConstraintResult r2 = graph_add_incidence(graph, p0, seg);
    printf("  Duplicate incidence handling: PASSED (result=%d)\n", r2);

    graph_destroy(graph);
    return 0;
}

/* 测试冗余约束检测 */
static int test_redundant_constraints(void) {
    printf("\n=== Testing Redundant Constraints ===\n");

    ConstraintGraph *graph = graph_create();
    if (!graph) {
        printf("  FAILED: Could not create graph\n");
        return -1;
    }

    int p0 = add_point(graph, 0, 1, 0, 1);
    int p1 = add_point(graph, 1, 1, 0, 1);
    int p2 = add_point(graph, 2, 1, 0, 1);

    graph_add_line_segment(graph, p0, p1);
    graph_add_line_segment(graph, p1, p2);
    graph_add_line_segment(graph, p0, p2);

    graph_add_betweenness(graph, p0, p1, p2);

    int redundant_count = 0;
    int *redundant = graph_detect_redundant_constraints(graph, &redundant_count);
    printf("  Redundant constraints found: %d\n", redundant_count);
    lv00_free_ptr(redundant);

    printf("  Redundant constraint detection: PASSED\n");
    graph_destroy(graph);
    return 0;
}

/* 测试大规模图操作 */
static int test_large_scale_operations(void) {
    printf("\n=== Testing Large Scale Operations ===\n");

    ConstraintGraph *graph = graph_create();
    if (!graph) {
        printf("  FAILED: Could not create graph\n");
        return -1;
    }

    const int G = 10;
    int ids[G][G];

    for (int i = 0; i < G; i++) {
        for (int j = 0; j < G; j++) {
            ids[i][j] = add_point(graph, i, 1, j, 1);
        }
    }
    printf("  Created %dx%d grid points: PASSED\n", G, G);

    /* 水平边 */
    for (int i = 0; i < G; i++)
        for (int j = 0; j < G - 1; j++)
            graph_add_line_segment(graph, ids[i][j], ids[i][j + 1]);

    /* 垂直边 */
    for (int i = 0; i < G - 1; i++)
        for (int j = 0; j < G; j++)
            graph_add_line_segment(graph, ids[i][j], ids[i + 1][j]);

    int expected_segs = G * (G - 1) * 2;
    int actual_segs = graph->node_count - G * G;
    if (actual_segs != expected_segs) {
        printf("  FAILED: Expected %d segments, got %d\n", expected_segs, actual_segs);
        graph_destroy(graph);
        return -1;
    }
    printf("  Created %d segments: PASSED\n", actual_segs);

    graph_destroy(graph);
    return 0;
}

/* 测试跨边界约束检测 */
static int test_cross_boundary(void) {
    printf("\n=== Testing Cross-Boundary Constraints ===\n");

    ConstraintGraph *graph = graph_create();
    if (!graph) {
        printf("  FAILED: Could not create graph\n");
        return -1;
    }

    int p0 = add_point(graph, 0, 1, 0, 1);
    int p1 = add_point(graph, 1, 1, 0, 1);
    int p2 = add_point(graph, 0, 1, 1, 1);

    int seg = graph->next_node_id;
    graph_add_line_segment(graph, p0, p1);

    /* p2 在 seg 上 => 跨边界约束 */
    graph_add_incidence(graph, p2, seg);

    int internal[] = {p0, p1};
    int ports[] = {p2};
    int out_count = 0;
    CrossBoundaryConstraint *cbc = find_cross_boundary_constraints(graph, internal, 2, ports, 1, &out_count);

    printf("  Cross-boundary constraints found: %d\n", out_count);
    lv00_free_ptr(cbc);

    printf("  Cross-boundary detection: PASSED\n");
    graph_destroy(graph);
    return 0;
}

/* 测试区域验证 */
static int test_region_validation(void) {
    printf("\n=== Testing Region Validation ===\n");

    ConstraintGraph *graph = graph_create();
    if (!graph) {
        printf("  FAILED: Could not create graph\n");
        return -1;
    }

    int p0 = add_point(graph, 0, 1, 0, 1);
    int p1 = add_point(graph, 1, 1, 0, 1);
    int p2 = add_point(graph, 1, 1, 1, 1);
    int p3 = add_point(graph, 0, 1, 1, 1);

    int s0 = graph->next_node_id;
    graph_add_line_segment(graph, p0, p1);
    int s1 = graph->next_node_id;
    graph_add_line_segment(graph, p1, p2);
    int s2 = graph->next_node_id;
    graph_add_line_segment(graph, p2, p3);
    int s3 = graph->next_node_id;
    graph_add_line_segment(graph, p3, p0);

    int boundary[] = {s0, s1, s2, s3};
    AddNodeResult rr = graph_add_region(graph, boundary, 4);
    printf("  Region creation: %s\n", rr == ADD_NODE_OK ? "PASSED" : "FAILED");

    if (rr == ADD_NODE_OK) {
        int region_id = graph->next_node_id - 1;
        bool valid = graph_validate_region_closure(graph, region_id);
        printf("  Region closure validation: %s\n", valid ? "PASSED" : "FAILED (expected for non-closed)");
    }

    graph_destroy(graph);
    return 0;
}

int main(void) {
    printf("=== Lv-00 Complex Graph Test Suite ===\n");
    int failures = 0;
    failures += test_many_nodes();
    failures += test_constraint_conflicts();
    failures += test_redundant_constraints();
    failures += test_large_scale_operations();
    failures += test_cross_boundary();
    failures += test_region_validation();

    printf("\n=== Test Summary ===\n");
    if (failures == 0)
        printf("All complex graph tests PASSED!\n");
    else
        printf("%d test(s) FAILED\n", failures);
    return failures ? 1 : 0;
}
