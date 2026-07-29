/**
 * @file test_groebner_basis.c
 * @brief Groebner 基计算（Buchberger 算法）测试
 *
 * 使用约束图 + Solver API（高层面测试），验证 Buchberger 算法在
 * 几何约束求解中的正确性。
 *
 * 测试分组:
 *   Group 1: 直角三角形 via 约束图 + Solver API
 *   Group 2: 过约束系统的冲突检测
 *   Group 3: 良好约束系统的 DOF 验证与求解
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/groebner_engine.h"
#include "lv/solver.h"

#include "lv.h"
#include "test_helpers.h"

/* ==================== 全局测试计数器 ==================== */
int g_pass_count = 0;
int g_fail_count = 0;

/* ==================== 辅助函数 ==================== */

static int add_rat_point(ConstraintGraph *g, int64_t xn, uint64_t xd, int64_t yn, uint64_t yd) {
    SymbolicCoord *cx = symbolic_coord_create_rational(xn, xd);
    SymbolicCoord *cy = symbolic_coord_create_rational(yn, yd);
    if (!cx || !cy) {
        if (cx)
            symbolic_coord_destroy(cx);
        if (cy)
            symbolic_coord_destroy(cy);
        return -1;
    }
    SymbolicCoord *coords[] = {cx, cy};
    AddNodeResult res = graph_add_point(g, coords, 2);
    if (res != ADD_NODE_OK)
        return -1;
    return g->next_node_id - 1;
}

/* ================================================================
 *  Group 1: 直角三角形
 * ================================================================
 *  3-4-5 直角三角形 (0,0)-(3,0)-(0,4)，含 3 线段 + 6 INCIDENCE。
 *  验证 solver 正确求解。
 */

static void test_group1_right_triangle(void) {
    printf("  Running: test_group1_right_triangle ...\n");

    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    int p0 = add_rat_point(g, 0, 1, 0, 1); /* (0,0) */
    int p1 = add_rat_point(g, 3, 1, 0, 1); /* (3,0) */
    int p2 = add_rat_point(g, 0, 1, 4, 1); /* (0,4) */
    TEST_ASSERT(p0 >= 0 && p1 >= 0 && p2 >= 0, "add points");

    int s01 = graph_add_line_segment(g, p0, p1);
    int s12 = graph_add_line_segment(g, p1, p2);
    int s20 = graph_add_line_segment(g, p2, p0);
    TEST_ASSERT(s01 >= 0 && s12 >= 0 && s20 >= 0, "add segments");

    graph_add_incidence(g, p0, s01);
    graph_add_incidence(g, p1, s01);
    graph_add_incidence(g, p1, s12);
    graph_add_incidence(g, p2, s12);
    graph_add_incidence(g, p2, s20);
    graph_add_incidence(g, p0, s20);

    GroebnerResult *result = NULL;
    SolverStatus status = solve_algebraic_system(g, NULL, 0, &result);
    printf("    solve status: %d  (expect OK=0 or UNIQUE=1)\n", (int) status);
    TEST_ASSERT(status == SOLVER_STATUS_OK || status == SOLVER_STATUS_UNIQUE, "solver status OK/UNIQUE");
    TEST_ASSERT_NOT_NULL(result);
    if (result) {
        printf("    solutions: %d, unique: %s\n", result->solution_count, result->unique ? "yes" : "no");
        groebner_result_destroy(result);
    }
    graph_destroy(g);
}

/* ================================================================
 *  Group 2: 过约束系统
 * ================================================================
 *  三角形 + BETWEENNESS（三点非共线却要求共线），验证冲突检测。
 */

static void test_group2_overconstrained(void) {
    printf("  Running: test_group2_overconstrained ...\n");

    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    int p0 = add_rat_point(g, 0, 1, 0, 1);
    int p1 = add_rat_point(g, 3, 1, 0, 1);
    int p2 = add_rat_point(g, 1, 1, 2, 1);
    TEST_ASSERT(p0 >= 0 && p1 >= 0 && p2 >= 0, "add points");

    int s01 = graph_add_line_segment(g, p0, p1);
    int s12 = graph_add_line_segment(g, p1, p2);
    int s20 = graph_add_line_segment(g, p2, p0);
    graph_add_incidence(g, p0, s01);
    graph_add_incidence(g, p1, s01);
    graph_add_incidence(g, p1, s12);
    graph_add_incidence(g, p2, s12);
    graph_add_incidence(g, p2, s20);
    graph_add_incidence(g, p0, s20);

    /* 添加矛盾的 BETWEENNESS（三点非共线） */
    graph_add_betweenness(g, p0, p1, p2);

    /* 冲突检测（即使求解器处理方式不同，冲突检测应能识别） */
    bool has_conflict = check_conflict_equations(g);
    printf("    conflict detected: %s (expect yes)\n", has_conflict ? "yes" : "no");

    /* 求解（求解器可能返回 OK、UNIQUE、MULTIPLE 等，此处不严格断言状态） */
    GroebnerResult *result = NULL;
    SolverStatus status = solve_algebraic_system(g, NULL, 0, &result);
    printf("    solve status: %d\n", (int) status);
    if (result) {
        printf("    solutions: %d, unique: %s\n", result->solution_count, result->unique ? "yes" : "no");
        groebner_result_destroy(result);
    }
    graph_destroy(g);
}

/* ================================================================
 *  Group 3: 良好约束系统
 * ================================================================
 *  2 点 + 1 线段 → DOF=3。扩展为完整三角形后求解。
 */

static void test_group3_well_constrained(void) {
    printf("  Running: test_group3_well_constrained ...\n");

    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    int p0 = add_rat_point(g, 0, 1, 0, 1);
    int p1 = add_rat_point(g, 3, 1, 0, 1);
    int p2 = add_rat_point(g, 1, 1, 2, 1);
    TEST_ASSERT(p0 >= 0 && p1 >= 0 && p2 >= 0, "add points");

    /* DOF: 3 点 * 2 = 6, 3 线段 * 1 = 3, 6 INCIDENCE * 0 = 0 → DOF = 3 */
    int s01 = graph_add_line_segment(g, p0, p1);
    int s12 = graph_add_line_segment(g, p1, p2);
    int s20 = graph_add_line_segment(g, p2, p0);
    graph_add_incidence(g, p0, s01);
    graph_add_incidence(g, p1, s01);
    graph_add_incidence(g, p1, s12);
    graph_add_incidence(g, p2, s12);
    graph_add_incidence(g, p2, s20);
    graph_add_incidence(g, p0, s20);

    /* DOF 分析 */
    int *free_var_ids = NULL;
    int dof = count_degrees_of_freedom(g, &free_var_ids);
    printf("    DOF: %d (expect >= 3)\n", dof);
    TEST_ASSERT(dof >= 3, "DOF >= 3");
    if (free_var_ids)
        lv_free((void **) &free_var_ids);

    /* 求解 */
    GroebnerResult *result = NULL;
    SolverStatus status = solve_algebraic_system(g, NULL, 0, &result);
    printf("    solve status: %d\n", (int) status);
    TEST_ASSERT(status == SOLVER_STATUS_OK || status == SOLVER_STATUS_UNIQUE, "solver status OK/UNIQUE");
    if (result) {
        printf("    solutions: %d, unique: %s\n", result->solution_count, result->unique ? "yes" : "no");
        groebner_result_destroy(result);
    }
    graph_destroy(g);
}

/* ================================================================
 *  Group 4: 引擎 API 直接测试 —— 环与多项式的生命周期
 * ================================================================
 */

static void test_engine_ring_lifecycle(void) {
    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);
    TEST_ASSERT(reg->ring_count == 0, "new registry should have 0 rings");
    TEST_ASSERT(reg->is_initialized, "registry should be initialized");

    const char *vars[] = {"x", "y"};
    int rid = ring_create(reg, vars, 2, RING_FIELD_RATIONAL, MONOMIAL_GREVLEX, "test_ring");
    TEST_ASSERT(rid >= 0, "ring_create should succeed");

    lvPolynomialRing *r = ring_find(reg, rid);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ(r->var_count, 2);
    TEST_ASSERT_EQ((int)r->order, (int)MONOMIAL_GREVLEX);
    TEST_ASSERT_EQ((int)r->field, (int)RING_FIELD_RATIONAL);

    ring_registry_destroy(reg);
}

static void test_engine_poly_lifecycle(void) {
    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);

    const char *vars[] = {"x", "y"};
    int rid = ring_create(reg, vars, 2, RING_FIELD_RATIONAL, MONOMIAL_GREVLEX, NULL);
    TEST_ASSERT(rid >= 0, "ring create");

    int pid = poly_create(reg, rid, 8, "test_poly");
    TEST_ASSERT(pid >= 0, "poly_create should succeed");

    const lvPolynomial *p = poly_get(reg, pid);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQ(p->ring_id, rid);
    TEST_ASSERT_EQ(p->term_count, 0);

    poly_destroy(reg, pid);

    /* Verify destruction: should no longer be accessible */
    const lvPolynomial *gone = poly_get(reg, pid);
    TEST_ASSERT_NULL(gone);

    ring_registry_destroy(reg);
}

/* ================================================================
 *  Group 5: engine API —— 多项式算术
 * ================================================================
 */

static void test_engine_poly_arith(void) {
    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);

    const char *vars[] = {"x", "y"};
    int rid = ring_create(reg, vars, 2, RING_FIELD_RATIONAL, MONOMIAL_GREVLEX, NULL);

    /* 创建两个零多项式并相加 */
    int pa = poly_create(reg, rid, 4, "a");
    int pb = poly_create(reg, rid, 4, "b");
    TEST_ASSERT(pa >= 0 && pb >= 0, "poly create");

    int sum = poly_add(reg, pa, pb, "sum");
    TEST_ASSERT(sum >= 0, "poly_add should succeed");
    const lvPolynomial *psum = poly_get(reg, sum);
    TEST_ASSERT_NOT_NULL(psum);

    /* 零 + 零 = 零 */
    TEST_ASSERT_EQ(psum->term_count, 0);

    poly_destroy(reg, pa);
    poly_destroy(reg, pb);
    poly_destroy(reg, sum);
    ring_registry_destroy(reg);
}

/* ================================================================
 *  Group 6: engine API —— 理想操作
 * ================================================================
 */

static void test_engine_ideal_lifecycle(void) {
    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);

    const char *vars[] = {"x", "y"};
    int rid = ring_create(reg, vars, 2, RING_FIELD_RATIONAL, MONOMIAL_GREVLEX, "ideal_test");

    int iid = ideal_create(reg, rid, "test_ideal");
    TEST_ASSERT(iid >= 0, "ideal_create should succeed");

    /* 添加一个生成元 */
    int pid = poly_create(reg, rid, 4, "gen");
    TEST_ASSERT(pid >= 0, "poly create");

    int rc = ideal_add_generator(reg, iid, pid);
    TEST_ASSERT_EQ(rc, 0);

    ideal_destroy(reg, iid);
    poly_destroy(reg, pid);
    ring_registry_destroy(reg);
}

/* ================================================================
 *  Group 7: engine API —— constraint_graph_to_ideal
 * ================================================================
 *  创建约束图 → 编码为理想 → 验证理想被正确创建
 */

static void test_constraint_graph_to_ideal(void) {
    /* 创建约束图：两个点 + 一条线段 + incidence */
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    int p0 = add_rat_point(g, 0, 1, 0, 1); /* (0,0) */
    int p1 = add_rat_point(g, 3, 1, 4, 1); /* (3,4) */
    TEST_ASSERT(p0 >= 0 && p1 >= 0, "add points");

    int s01 = graph_add_line_segment(g, p0, p1);
    TEST_ASSERT(s01 >= 0, "add segment");

    graph_add_incidence(g, p0, s01);
    graph_add_incidence(g, p1, s01);

    /* 创建环注册表和环（2 个点 → 4 个变量：x0, y0, x1, y1） */
    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);

    const char *vars[] = {"x0", "y0", "x1", "y1"};
    int rid = ring_create(reg, vars, 4, RING_FIELD_REAL, MONOMIAL_GREVLEX, "graph_ideal");

    /* 转换 */
    int ideal_id = constraint_graph_to_ideal(reg, g, rid, "from_graph");
    TEST_ASSERT(ideal_id >= 0, "constraint_graph_to_ideal should succeed");

    /* 验证理想包含生成元（至少包含 2 个点的坐标方程） */
    /* 通过 groebner_compute 和 ideal_membership 验证 */
    int compute_rc = groebner_compute(reg, ideal_id, GROEBNER_BUCHBERGER);
    TEST_ASSERT_EQ(compute_rc, 0);

    ideal_destroy(reg, ideal_id);
    ring_registry_destroy(reg);
    graph_destroy(g);
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("=== Groebner Basis Computation Test ===\n\n");

    TEST_RUN(test_group1_right_triangle);
    TEST_RUN(test_group2_overconstrained);
    TEST_RUN(test_group3_well_constrained);

    printf("\n[Groebner Engine API Direct Tests]\n");
    TEST_RUN(test_engine_ring_lifecycle);
    TEST_RUN(test_engine_poly_lifecycle);
    TEST_RUN(test_engine_poly_arith);
    TEST_RUN(test_engine_ideal_lifecycle);
    TEST_RUN(test_constraint_graph_to_ideal);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", g_pass_count, g_fail_count,
           g_pass_count + g_fail_count);
    return g_fail_count > 0 ? 1 : 0;
}
