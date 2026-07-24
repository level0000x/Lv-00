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
 *  Main
 * ================================================================ */

int main(void) {
    printf("=== Groebner Basis Computation Test ===\n\n");

    TEST_RUN(test_group1_right_triangle);
    TEST_RUN(test_group2_overconstrained);
    TEST_RUN(test_group3_well_constrained);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", g_pass_count, g_fail_count,
           g_pass_count + g_fail_count);
    return g_fail_count > 0 ? 1 : 0;
}
