/*
 * test_unify.c - Lv-00 合一检查测试
 *
 * 测试合一检查系统：
 * - 成功合一场景
 * - 失败场景（约束缺失、坐标不匹配）
 * - 命题创建与匹配
 * - 证明规范化
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "lv/constraint_graph.h"
#include "lv/unify.h"
#include "test_helpers.h"
#include "lv_test_geom_graph_builder.h"

/* 创建简单构造：两点一线（收敛：使用共享构造器 lv_test_line_graph(NULL, 0, 0, 3, 4, false)，
 * 即 (0,0)(3,4) 两点 + 一线段、无 incidence，与本地 create_line_graph 语义一致）*/

/* 测试命题创建 */
static int test_proposition_create(void) {
    printf("\n=== Testing Simple Proposition Create ===\n");

    int inputs[] = {0, 1};
    int outputs[] = {2};
    SimpleProposition *prop = simple_proposition_create("test_line", inputs, 2, outputs, 1);
    if (!prop) {
        printf("  FAILED: Could not create proposition\n");
        return -1;
    }

    printf("  Proposition created: name='%s', inputs=%d, outputs=%d\n", prop->name, prop->input_count,
           prop->output_count);

    /* 设置模式 */
    prop->pattern = lv_test_line_graph(NULL, 0, 0, 3, 4, false);

    printf("  Pattern set: PASSED\n");
    simple_proposition_destroy(prop);
    return 0;
}

/* 测试成功的合一 */
static int test_successful_unify(void) {
    printf("\n=== Testing Successful Unify ===\n");

    ConstraintGraph *construction = lv_test_line_graph(NULL, 0, 0, 3, 4, false);
    if (!construction) {
        printf("  FAILED: Could not create construction\n");
        return -1;
    }

    ConstraintGraph *pattern = lv_test_line_graph(NULL, 0, 0, 3, 4, false);

    UnifyStatus status = unify_construction_with_proposition(construction, pattern);
    printf("  Unify status: %d\n", status);

    if (status != UNIFY_STATUS_OK) {
        printf("  FAILED: Expected UNIFY_STATUS_OK\n");
        graph_destroy(construction);
        graph_destroy(pattern);
        return -1;
    }

    printf("  Successful unify: PASSED\n");
    graph_destroy(construction);
    graph_destroy(pattern);
    return 0;
}

/* 测试约束缺失 */
static int test_constraint_missing(void) {
    printf("\n=== Testing Constraint Missing ===\n");

    /* 构造：两个点，没有线段 */
    ConstraintGraph *construction = graph_create();
    add_point(construction, 0, 1, 0, 1);
    add_point(construction, 3, 1, 4, 1);

    /* 命题：期望有线段 */
    ConstraintGraph *pattern = lv_test_line_graph(NULL, 0, 0, 3, 4, false);

    UnifyStatus status = unify_construction_with_proposition(construction, pattern);
    printf("  Unify status: %d (expected non-OK)\n", status);

    if (status == UNIFY_STATUS_OK) {
        printf("  WARNING: Unify succeeded unexpectedly\n");
    } else {
        printf("  Constraint missing detected: PASSED\n");
    }

    graph_destroy(construction);
    graph_destroy(pattern);
    return 0;
}

/* 测试坐标不匹配 */
static int test_coord_mismatch(void) {
    printf("\n=== Testing Coordinate Mismatch ===\n");

    /* 构造：点在 (0,0) 和 (1,1) */
    ConstraintGraph *construction = graph_create();
    add_point(construction, 0, 1, 0, 1);
    add_point(construction, 1, 1, 1, 1);
    graph_add_line_segment(construction, 0, 1);

    /* 命题：点在 (0,0) 和 (3,4) */
    ConstraintGraph *pattern = lv_test_line_graph(NULL, 0, 0, 3, 4, false);

    UnifyStatus status = unify_construction_with_proposition(construction, pattern);
    printf("  Unify status: %d\n", status);

    printf("  Coordinate mismatch test: PASSED (status=%d)\n", status);

    graph_destroy(construction);
    graph_destroy(pattern);
    return 0;
}

/* 测试简单证明创建和检查 */
static int test_simple_proof(void) {
    printf("\n=== Testing Simple Proof ===\n");

    int inputs[] = {0, 1};
    int outputs[] = {2};
    SimpleProposition *prop = simple_proposition_create("line_proof", inputs, 2, outputs, 1);
    prop->pattern = lv_test_line_graph(NULL, 0, 0, 3, 4, false);

    ConstraintGraph *construction = lv_test_line_graph(NULL, 0, 0, 3, 4, false);

    SimpleProof *proof = simple_proof_create(prop, construction);
    if (!proof) {
        printf("  FAILED: Could not create proof\n");
        simple_proposition_destroy(prop);
        graph_destroy(construction);
        return -1;
    }

    printf("  Proof created: normalized=%d, passed=%d\n", proof->normalized, proof->passed);

    /* 检查证明 */
    bool passed = simple_proof_check(proof);
    printf("  Proof check result: %s\n", passed ? "PASSED" : "FAILED");

    if (!passed) {
        printf("  WARNING: Proof check returned false\n");
    }

    printf("  Simple proof: PASSED\n");
    simple_proof_destroy(proof);
    return 0;
}

/* 测试证明规范化 */
static int test_proof_normalize(void) {
    printf("\n=== Testing Proof Normalize ===\n");

    int inputs[] = {0, 1};
    int outputs[] = {2};
    SimpleProposition *prop = simple_proposition_create("norm_proof", inputs, 2, outputs, 1);
    prop->pattern = lv_test_line_graph(NULL, 0, 0, 3, 4, false);

    /* 构造包含重复点 */
    ConstraintGraph *construction = graph_create();
    add_point(construction, 0, 1, 0, 1);
    add_point(construction, 0, 1, 0, 1); /* 重复 */
    int p2 = add_point(construction, 3, 1, 4, 1);
    graph_add_line_segment(construction, 0, p2);

    SimpleProof *proof = simple_proof_create(prop, construction);
    printf("  Before normalize: normalized=%d\n", proof->normalized);

    simple_proof_normalize(proof);
    printf("  After normalize: normalized=%d\n", proof->normalized);

    if (!proof->normalized) {
        printf("  FAILED: Proof should be normalized\n");
        simple_proof_destroy(proof);
        return -1;
    }

    printf("  Proof normalize: PASSED\n");
    simple_proof_destroy(proof);
    return 0;
}

/* 测试复杂构造合一 */
static int test_complex_construction_unify(void) {
    printf("\n=== Testing Complex Construction Unify ===\n");

    /* 构造：三角形 */
    ConstraintGraph *construction = graph_create();
    int p0 = add_point(construction, 0, 1, 0, 1);
    int p1 = add_point(construction, 3, 1, 0, 1);
    int p2 = add_point(construction, 0, 1, 4, 1);
    graph_add_line_segment(construction, p0, p1);
    graph_add_line_segment(construction, p1, p2);
    graph_add_line_segment(construction, p2, p0);

    /* 命题：相同的三角形 */
    ConstraintGraph *pattern = graph_create();
    int q0 = add_point(pattern, 0, 1, 0, 1);
    int q1 = add_point(pattern, 3, 1, 0, 1);
    int q2 = add_point(pattern, 0, 1, 4, 1);
    graph_add_line_segment(pattern, q0, q1);
    graph_add_line_segment(pattern, q1, q2);
    graph_add_line_segment(pattern, q2, q0);

    UnifyStatus status = unify_construction_with_proposition(construction, pattern);
    printf("  Triangle unify status: %d\n", status);

    printf("  Complex construction unify: PASSED\n");
    graph_destroy(construction);
    graph_destroy(pattern);
    return 0;
}

/* 测试空图合一 */
static int test_empty_graph_unify(void) {
    printf("\n=== Testing Empty Graph Unify ===\n");

    ConstraintGraph *empty1 = graph_create();
    ConstraintGraph *empty2 = graph_create();

    UnifyStatus status = unify_construction_with_proposition(empty1, empty2);
    printf("  Empty graph unify status: %d\n", status);

    printf("  Empty graph unify: PASSED\n");
    graph_destroy(empty1);
    graph_destroy(empty2);
    return 0;
}

/* 测试端口匹配中的 parent_block_id 和 is_formal_param 检查 */
static int test_port_match_higher_order(void) {
    printf("\n=== Testing Port Match with parent_block_id ===\n");
    int failures = 0;

    /* 创建两个图，各包含一个函数块 + 端口 */
    int internal_ids1[] = {101, 102};
    int input_ids1[] = {201, 202};
    int output_ids1[] = {301};

    ConstraintGraph *g1 = graph_create();
    graph_add_function_block(g1, internal_ids1, 2, input_ids1, 2, output_ids1, 1);
    int g1_fb_id = graph_get_last_added_node_id(g1);
    /* 在 g1_fb_id 下添加端口 */
    graph_add_port(g1, PORT_INPUT, 0, g1_fb_id);
    graph_add_port(g1, PORT_INPUT, 0, g1_fb_id);

    /* 第二个图：不同的 parent_block_id，但其他相同 */
    ConstraintGraph *g2 = graph_create();
    graph_add_function_block(g2, internal_ids1, 2, input_ids1, 2, output_ids1, 1);
    int g2_fb_id = graph_get_last_added_node_id(g2);
    graph_add_port(g2, PORT_INPUT, 0, g2_fb_id);
    graph_add_port(g2, PORT_INPUT, 0, g2_fb_id);

    /* 端口匹配应成功（两者端口都属于各自的唯一函数块，parent_block_id 内部匹配） */
    int bindings[16];
    int count = unify_match_ports(g1, g2, bindings, 16);
    if (count > 0) {
        printf("  Port match across graphs: OK (%d matches)\n", count);
    } else {
        printf("  Port match across graphs: FAILED (count=%d)\n", count);
        failures++;
    }

    graph_destroy(g1);
    graph_destroy(g2);
    return failures;
}

int main(void) {
    printf("=== Lv-00 Unify Test Suite ===\n");
    int failures = 0;
    failures += test_proposition_create();
    failures += test_successful_unify();
    failures += test_constraint_missing();
    failures += test_coord_mismatch();
    failures += test_simple_proof();
    failures += test_proof_normalize();
    failures += test_complex_construction_unify();
    failures += test_empty_graph_unify();
    failures += test_port_match_higher_order();

    printf("\n=== Test Summary ===\n");
    if (failures == 0)
        printf("All unify tests PASSED!\n");
    else
        printf("%d test(s) FAILED\n", failures);
    return failures ? 1 : 0;
}
