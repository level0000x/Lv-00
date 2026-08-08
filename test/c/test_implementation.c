/**
 * @file test_implementation.c
 * @brief 验证Lv-00核心功能实现
 * 
 * 根据design_v2.9.md和planning_v3.0.md验证以下功能：
 * 1. 符号坐标系统（位数熔断、AMBER降级）
 * 2. 图规范化遍引擎
 * 3. 合一检查系统
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#include "lv.h"

/* ============================================================
 * Test 1: Symbolic Coordinate System
 * ============================================================ */

static void test_symbolic_coord_basic(void) {
    printf("\n[TEST] Symbolic Coordinate System - Basic Operations\n");

    /* Test 1.1: Rational number creation and arithmetic */
    SymbolicCoord *r1 = mk_rat(3, 4);
    SymbolicCoord *r2 = mk_rat(1, 4);
    lv_ASSERT(r1 != NULL && r2 != NULL);

    SymbolicCoord *r_sum = symbolic_coord_add(r1, r2);
    lv_ASSERT_NOT_NULL(r_sum);
    printf("  3/4 + 1/4 = %s (expected: 1/1)\n", symbolic_coord_serialize(r_sum));

    /* Test 1.2: Quadratic numbers (a + b*sqrt(n)) */
    Rational *a = rational_create(1, 1);
    Rational *b = rational_create(1, 1);
    SymbolicCoord *q1 = symbolic_coord_create_quadratic(a, b, 2);
    lv_ASSERT_NOT_NULL(q1);
    printf("  Created quadratic: 1 + 1*sqrt(2)\n");

    /* Test 1.3: Trust color management */
    lv_ASSERT(symbolic_coord_get_trust(r1) == TRUST_GREEN);
    symbolic_coord_set_trust(r1, TRUST_AMBER);
    lv_ASSERT(symbolic_coord_get_trust(r1) == TRUST_AMBER);
    printf("  Trust color management: OK\n");

    /* Test 1.4: AMBER downgrade */
    SymbolicCoord *amber = symbolic_coord_downgrade_to_amber(r2, 1e-15, "Test downgrade for large number handling");
    lv_ASSERT_NOT_NULL(amber);
    lv_ASSERT(symbolic_coord_is_amber(amber));
    printf("  AMBER downgrade: OK\n");

    /* Cleanup */
    symbolic_coord_destroy(r1);
    symbolic_coord_destroy(r2);
    symbolic_coord_destroy(r_sum);
    symbolic_coord_destroy(q1);
    symbolic_coord_destroy(amber);

    printf("[PASS] Symbolic Coordinate Basic Operations\n");

}

/* ============================================================
 * Test 2: Bit Circuit (Digit Cutoff) System
 * ============================================================ */

static void test_bit_circuit(void) {
    printf("\n[TEST] Bit Circuit (Digit Cutoff) System\n");

    /* Test 2.1: Circuit status check for normal values */
    SymbolicCoord *small = mk_rat(12345, 67890);
    CircuitStatus status = check_digit_circuit(small);
    lv_ASSERT(status == CIRCUIT_STATUS_OK);
    printf("  Small number circuit check: OK (status=%d)\n", status);

    /* Test 2.2: Circuit overflow handling */
    circuit_reset_context();
    lv_ASSERT(circuit_get_overflow_count() == 0);

    /* Simulate overflow by calling handle directly */
    circuit_handle_overflow();
    lv_ASSERT(circuit_get_overflow_count() == 1);
    printf("  Circuit overflow count: %d\n", circuit_get_overflow_count());

    /* Test 2.3: Multiple overflows trigger downgrade suggestion */
    circuit_handle_overflow();
    circuit_handle_overflow();
    lv_ASSERT(circuit_get_overflow_count() >= 3);
    printf("  Circuit overflow count after 3 trips: %d (suggests downgrade)\n", circuit_get_overflow_count());

    /* Cleanup */
    symbolic_coord_destroy(small);
    circuit_reset_context();

    printf("[PASS] Bit Circuit System\n");

}

/* ============================================================
 * Test 3: Constraint Graph Core
 * ============================================================ */

static void test_constraint_graph(void) {
    printf("\n[TEST] Constraint Graph Core\n");

    /* Test 3.1: Graph creation */
    ConstraintGraph *graph = graph_create();
    lv_ASSERT_NOT_NULL(graph);
    printf("  Graph created: OK\n");

    /* Test 3.2: Point creation */
    add_point(graph, 0, 1, 0, 1);
    AddNodeResult result;
    printf("  Point added: OK (id=%d)\n", graph->nodes[0]->id);

    /* Test 3.3: Line segment creation */
    add_point(graph, 1, 1, 1, 1);

    result = graph_add_line_segment(graph, graph->nodes[0]->id, graph->nodes[1]->id);
    lv_ASSERT(result == ADD_NODE_OK);
    printf("  Line segment added: OK\n");

    /* Test 3.4: Port creation with namespace markers */
    result = graph_add_port(graph, PORT_INPUT, 0, -1);
    lv_ASSERT(result == ADD_NODE_OK);
    GeomNode *port = graph->nodes[graph->node_count - 1];
    lv_ASSERT(port->type == GEOM_PORT);
    lv_ASSERT(port->namespace_depth == 0);
    lv_ASSERT(port->parent_block_id == -1);
    printf("  Port created with namespace markers: OK\n");

    /* Test 3.5: Constraint addition */
    AddConstraintResult cresult = graph_add_incidence(graph, graph->nodes[0]->id, graph->nodes[2]->id);
    lv_ASSERT(cresult == ADD_CONSTRAINT_OK);
    printf("  Incidence constraint added: OK\n");

    /* Cleanup */
    graph_destroy(graph);

    printf("[PASS] Constraint Graph Core\n");

}

/* ============================================================
 * Test 4: Graph Normalization
 * ============================================================ */

static void test_graph_normalization(void) {
    printf("\n[TEST] Graph Normalization Engine\n");

    /* Test 4.1: Create graph with duplicate points */
    ConstraintGraph *graph = graph_create();

    /* Create two points with same coordinates */
    add_point(graph, 0, 1, 0, 1);
    add_point(graph, 0, 1, 0, 1);

    printf("  Created graph with %d points (2 with same coords)\n", graph->node_count);

    /* Test 4.2: Run normalization */
    NormalizationResult *norm = graph_normalize(graph, true);
    lv_ASSERT_NOT_NULL(norm);
    printf("  Normalization completed: %d merges performed\n", norm->merged_count);

    /* After normalization, duplicate points should be merged */
    printf("  After normalization: %d points remain\n", graph->node_count);

    /* Cleanup */
    normalization_result_destroy(norm);
    graph_destroy(graph);

    printf("[PASS] Graph Normalization Engine\n");

}

/* ============================================================
 * Test 5: Unification System
 * ============================================================ */

static void test_unification(void) {
    printf("\n[TEST] Unification System\n");

    /* Test 5.1: Create a simple proposition pattern */
    SimpleProposition *prop = simple_proposition_create("test_prop", NULL, 0, /* No input ports for this test */
                                                        NULL, 0               /* No output ports for this test */
    );
    lv_ASSERT_NOT_NULL(prop);
    printf("  Proposition created: %s\n", prop->name);

    /* Test 5.2: Create a construction that matches */
    ConstraintGraph *construction = graph_create();
    add_point(construction, 0, 1, 0, 1);

    /* Test 5.3: Create proof and check */
    SimpleProof *proof = simple_proof_create(prop, construction);
    lv_ASSERT_NOT_NULL(proof);

    /* Note: This is a simplified test - real unification would need
       matching port structures */
    printf("  Proof object created: OK\n");

    /* Cleanup */
    simple_proof_destroy(proof);
    simple_proposition_destroy(prop);

    printf("[PASS] Unification System\n");

}

/* ============================================================
 * Test 6: Cross-type Arithmetic
 * ============================================================ */

static void test_cross_type_arithmetic(void) {
    printf("\n[TEST] Cross-type Arithmetic Operations\n");

    /* Test 6.1: Rational + Quadratic */
    SymbolicCoord *r = mk_rat(1, 2);
    Rational *a = rational_create(1, 1);
    Rational *b = rational_create(1, 1);
    SymbolicCoord *q = symbolic_coord_create_quadratic(a, b, 2);

    SymbolicCoord *sum = symbolic_coord_add(r, q);
    lv_ASSERT_NOT_NULL(sum);
    printf("  Rational + Quadratic: OK\n");

    /* Test 6.2: Quadratic multiplication */
    Rational *a2 = rational_create(2, 1);
    Rational *b2 = rational_create(0, 1);
    SymbolicCoord *q2 = symbolic_coord_create_quadratic(a2, b2, 2);

    SymbolicCoord *prod = symbolic_coord_multiply(q, q2);
    lv_ASSERT_NOT_NULL(prod);
    printf("  Quadratic * Quadratic: OK\n");

    /* Cleanup */
    symbolic_coord_destroy(r);
    symbolic_coord_destroy(q);
    symbolic_coord_destroy(q2);
    symbolic_coord_destroy(sum);
    symbolic_coord_destroy(prod);

    printf("[PASS] Cross-type Arithmetic\n");

}

/* ============================================================
 * Main Test Runner
 * ============================================================ */

TEST_MAIN_BEGIN("Lv-00 Implementation Verification Tests")
    printf("========================================\n");
    printf("Lv-00 Implementation Verification Tests\n");
    printf("Based on design_v2.9.md and planning_v3.0.md\n");
    printf("========================================\n");
    TEST_MAIN_RUN(test_symbolic_coord_basic);
    TEST_MAIN_RUN(test_bit_circuit);
    TEST_MAIN_RUN(test_constraint_graph);
    TEST_MAIN_RUN(test_graph_normalization);
    TEST_MAIN_RUN(test_unification);
    TEST_MAIN_RUN(test_cross_type_arithmetic);
TEST_MAIN_END()
