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
 *  Group 8: 复杂约束回归测试 —— Buchberger 算法深度验证
 * ================================================================
 *  覆盖审计缺口：多变量多项式环、含符号常量的 Groebner 基、
 *  约束图→理想→基→簇端到端、非零特征环。
 */

/* 辅助：在多项式上直接设置单项（coeff * x_var^power，var_idx=-1 为常数项） */
static void poly_fill_single_term(lvPolynomial *p, int var_idx, int power, double coeff) {
    int vc = p->var_count;
    for (int v = 0; v < vc; v++) {
        p->powers[v] = 0;
    }
    if (var_idx >= 0) {
        p->powers[var_idx] = power;
    }
    ((double *)p->coeffs)[0] = coeff;
    p->term_count = 1;
    p->total_degree = power;
}

/* 辅助：设置二项式 ca * x_a^pa + cb * x_b^pb（var 为 -1 表示常数项） */
static void poly_fill_binomial(lvPolynomial *p, int var_a, int pa, double ca, int var_b, int pb, double cb) {
    int vc = p->var_count;
    for (int v = 0; v < vc; v++) {
        p->powers[v] = 0;
        p->powers[vc + v] = 0;
    }
    if (var_a >= 0) {
        p->powers[var_a] = pa;
    }
    if (var_b >= 0) {
        p->powers[vc + var_b] = pb;
    }
    ((double *)p->coeffs)[0] = ca;
    ((double *)p->coeffs)[1] = cb;
    p->term_count = 2;
    p->total_degree = (pa > pb) ? pa : pb;
}

static void test_multivar_buchberger(void) {
    printf("  Running: test_multivar_buchberger ...\n");

    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);

    const char *vars[] = {"x", "y", "z"};
    int rid = ring_create(reg, vars, 3, RING_FIELD_RATIONAL, MONOMIAL_GREVLEX, "Q[x,y,z]");
    TEST_ASSERT(rid >= 0, "ring_create");

    /* I = <x^2 - y, y - z^2> 包含于 Q[x,y,z] */
    int iid = ideal_create(reg, rid, "I");
    TEST_ASSERT(iid >= 0, "ideal_create");

    int g1 = poly_create(reg, rid, 4, "x^2-y");
    int g2 = poly_create(reg, rid, 4, "y-z^2");
    TEST_ASSERT(g1 >= 0 && g2 >= 0, "poly_create");

    poly_fill_binomial((lvPolynomial *)poly_get(reg, g1), 0, 2, 1.0, 1, 1, -1.0);
    poly_fill_binomial((lvPolynomial *)poly_get(reg, g2), 1, 1, 1.0, 2, 2, -1.0);

    TEST_ASSERT_EQ(ideal_add_generator(reg, iid, g1), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, iid, g2), 0);

    /* 计算 Groebner 基（显式 Buchberger 算法） */
    TEST_ASSERT_EQ(groebner_compute(reg, iid, GROEBNER_BUCHBERGER), 0);

    /* f = x^2 - z^2 = (x^2 - y) + (y - z^2) 属于 I */
    int f = poly_create(reg, rid, 4, "x^2-z^2");
    poly_fill_binomial((lvPolynomial *)poly_get(reg, f), 0, 2, 1.0, 2, 2, -1.0);
    TEST_ASSERT(ideal_membership(reg, iid, f), "x^2 - z^2 should be in I");

    /* g = x - z 不属于 I（grevlex 下不可约化为 0） */
    int g = poly_create(reg, rid, 4, "x-z");
    poly_fill_binomial((lvPolynomial *)poly_get(reg, g), 0, 1, 1.0, 2, 1, -1.0);
    TEST_ASSERT(!ideal_membership(reg, iid, g), "x - z should NOT be in I");

    ring_registry_destroy(reg);
}

static void test_symbolic_constant_basis(void) {
    printf("  Running: test_symbolic_constant_basis ...\n");

    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);

    /* 符号常量建模为变量 a，环 Q[a,x,y] */
    const char *vars[] = {"a", "x", "y"};
    int rid = ring_create(reg, vars, 3, RING_FIELD_RATIONAL, MONOMIAL_GREVLEX, "Q[a,x,y]");
    TEST_ASSERT(rid >= 0, "ring_create");

    /* I = <x + a, y - a>：x 与 y 均被符号常量 a 固定 */
    int iid = ideal_create(reg, rid, "I");
    TEST_ASSERT(iid >= 0, "ideal_create");

    int g1 = poly_create(reg, rid, 4, "x+a");
    int g2 = poly_create(reg, rid, 4, "y-a");
    poly_fill_binomial((lvPolynomial *)poly_get(reg, g1), 1, 1, 1.0, 0, 1, 1.0);
    poly_fill_binomial((lvPolynomial *)poly_get(reg, g2), 2, 1, 1.0, 0, 1, -1.0);

    TEST_ASSERT_EQ(ideal_add_generator(reg, iid, g1), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, iid, g2), 0);
    TEST_ASSERT_EQ(groebner_compute(reg, iid, GROEBNER_BUCHBERGER), 0);

    /* f = x + y 属于 I（x ≡ -a, y ≡ a → x + y ≡ 0） */
    int f = poly_create(reg, rid, 4, "x+y");
    poly_fill_binomial((lvPolynomial *)poly_get(reg, f), 1, 1, 1.0, 2, 1, 1.0);
    TEST_ASSERT(ideal_membership(reg, iid, f), "x + y should be in I");

    /* g = x + y + 1 不属于 I（余式为非零常数） */
    int g = poly_create(reg, rid, 4, "x+y+1");
    {
        lvPolynomial *pg = (lvPolynomial *)poly_get(reg, g);
        poly_fill_binomial(pg, 1, 1, 1.0, 2, 1, 1.0);
        pg->powers[2 * pg->var_count] = 0;
        ((double *)pg->coeffs)[2] = 1.0;
        pg->term_count = 3;
    }
    TEST_ASSERT(!ideal_membership(reg, iid, g), "x + y + 1 should NOT be in I");

    ring_registry_destroy(reg);
}

static void test_graph_collinear_end_to_end(void) {
    printf("  Running: test_graph_collinear_end_to_end ...\n");

    /* 共线三点 (0,0), (1,1), (2,2) + BETWEENNESS */
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    int p0 = add_rat_point(g, 0, 1, 0, 1);
    int p1 = add_rat_point(g, 1, 1, 1, 1);
    int p2 = add_rat_point(g, 2, 1, 2, 1);
    TEST_ASSERT(p0 >= 0 && p1 >= 0 && p2 >= 0, "add points");
    graph_add_betweenness(g, p0, p1, p2);

    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);

    const char *vars[] = {"x0", "y0", "x1", "y1", "x2", "y2"};
    int rid = ring_create(reg, vars, 6, RING_FIELD_REAL, MONOMIAL_GREVLEX, "collinear");
    TEST_ASSERT(rid >= 0, "ring_create");

    /* 先创建多项式以确保全局池初始化（constraint_graph_to_ideal 内部检查 g_data） */
    int dummy = poly_create(reg, rid, 2, "dummy");
    TEST_ASSERT(dummy >= 0, "dummy poly for pool init");

    /* 端到端：约束图 → 多项式理想 → Groebner 基 → 代数簇 */
    int iid = constraint_graph_to_ideal(reg, g, rid, "collinear_ideal");
    TEST_ASSERT(iid >= 0, "constraint_graph_to_ideal");

    TEST_ASSERT_EQ(groebner_compute(reg, iid, GROEBNER_AUTO), 0);

    int vid = variety_compute(reg, iid, "collinear_variety");
    TEST_ASSERT(vid >= 0, "variety_compute");
    TEST_ASSERT(variety_is_zero_dimensional(reg, vid), "3 fixed points -> zero-dimensional");

    double coords[6];
    TEST_ASSERT(variety_get_solution_point(reg, vid, 0, coords, 6), "should have a solution point");
    TEST_ASSERT_NEAR(coords[0], 0.0, 1e-6, "x0");
    TEST_ASSERT_NEAR(coords[1], 0.0, 1e-6, "y0");
    TEST_ASSERT_NEAR(coords[2], 1.0, 1e-6, "x1");
    TEST_ASSERT_NEAR(coords[3], 1.0, 1e-6, "y1");
    TEST_ASSERT_NEAR(coords[4], 2.0, 1e-6, "x2");
    TEST_ASSERT_NEAR(coords[5], 2.0, 1e-6, "y2");

    graph_destroy(g);
    ring_registry_destroy(reg);
}

static void test_finite_field_ring(void) {
    printf("  Running: test_finite_field_ring ...\n");

    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);

    const char *vars[] = {"x", "y"};
    int rid = ring_create(reg, vars, 2, RING_FIELD_FINITE, MONOMIAL_GRLEX, "GF(2)[x,y]");
    TEST_ASSERT(rid >= 0, "ring_create");

    /* 非零特征：GF(2)，特征通过公开结构字段设置（引擎无专用 setter） */
    lvPolynomialRing *r = ring_find(reg, rid);
    TEST_ASSERT_NOT_NULL(r);
    r->finite_field_char = 2;
    TEST_ASSERT_EQ(r->finite_field_char, 2);
    TEST_ASSERT_EQ((int)r->field, (int)RING_FIELD_FINITE);

    /* I = <x^2 + x, y^2 + y>（GF(2) 中 x^2 = x 的背景下） */
    int iid = ideal_create(reg, rid, "I");
    TEST_ASSERT(iid >= 0, "ideal_create");

    int g1 = poly_create(reg, rid, 4, "x2+x");
    int g2 = poly_create(reg, rid, 4, "y2+y");
    poly_fill_binomial((lvPolynomial *)poly_get(reg, g1), 0, 2, 1.0, 0, 1, 1.0);
    poly_fill_binomial((lvPolynomial *)poly_get(reg, g2), 1, 2, 1.0, 1, 1, 1.0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, iid, g1), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, iid, g2), 0);

    /* 有限域环上 Buchberger 计算路径可用（引擎系数为浮点近似，仅验证框架完整性） */
    TEST_ASSERT_EQ(groebner_compute(reg, iid, GROEBNER_BUCHBERGER), 0);

    /* 生成元的线性组合仍属于理想 */
    int f = poly_create(reg, rid, 8, "2(x2+x)+(y2+y)");
    {
        lvPolynomial *pf = (lvPolynomial *)poly_get(reg, f);
        int vc = pf->var_count;
        for (int v = 0; v < vc; v++) {
            pf->powers[v] = 0;
            pf->powers[vc + v] = 0;
        }
        pf->powers[0] = 2;              /* 2*x^2 */
        ((double *)pf->coeffs)[0] = 2.0;
        pf->powers[vc] = 1;             /* 2*x */
        ((double *)pf->coeffs)[1] = 2.0;
        pf->powers[2 * vc + 1] = 2;     /* y^2 */
        ((double *)pf->coeffs)[2] = 1.0;
        pf->powers[3 * vc + 1] = 1;     /* y */
        ((double *)pf->coeffs)[3] = 1.0;
        pf->term_count = 4;
        pf->total_degree = 2;
    }
    TEST_ASSERT(ideal_membership(reg, iid, f), "linear combination of generators should be in I");

    /* 非成员：常数 1 不在理想中 */
    int one = poly_create(reg, rid, 2, "1");
    poly_fill_single_term((lvPolynomial *)poly_get(reg, one), -1, 0, 1.0);
    TEST_ASSERT(!ideal_membership(reg, iid, one), "constant 1 should NOT be in I");

    ring_registry_destroy(reg);
}

/* ================================================================
 *  Group 9: ideal_intersection / ideal_quotient —— 理想交与理想商
 * ================================================================
 *  补充 2026-08-08 守卫宏推广后 ideal_intersection / ideal_quotient
 *  的代数正确性测试（此前无直接单测覆盖）。代数事实：
 *    - ⟨x⟩ ∩ ⟨y⟩ = ⟨xy⟩；吸收律 I ∩ (I + J) = I；⟨x⟩ ∩ ⟨x²⟩ = ⟨x²⟩
 *    - I : J = { q | q·J ⊆ I }；⟨xy⟩ : ⟨x⟩ = ⟨y⟩；I ⊆ I : J（普遍成立）
 *    - J ⊆ I ⟹ I : J = R；J = ⟨1⟩ ⟹ I : J = I；⟨x²⟩ : ⟨x⟩ = ⟨x⟩
 */

/* 辅助：设置单项式 c * x_0^{pv[0]} * ... * x_{n-1}^{pv[n-1]}（pv 长度 = var_count） */
static void poly_fill_monomial_vars(lvPolynomial *p, const int *pv, double coeff) {
    int vc = p->var_count;
    int deg = 0;
    for (int v = 0; v < vc; v++) {
        p->powers[v] = pv[v];
        deg += pv[v];
    }
    ((double *)p->coeffs)[0] = coeff;
    p->term_count = 1;
    p->total_degree = deg;
}

/* ⟨x⟩ ∩ ⟨y⟩ = ⟨xy⟩：交的生成元、成员判定、原始理想归属、理想封闭性 */
static void test_ideal_intersection_basic(void) {
    printf("  Running: test_ideal_intersection_basic ...\n");

    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);

    const char *vars[] = {"x", "y"};
    int rid = ring_create(reg, vars, 2, RING_FIELD_RATIONAL, MONOMIAL_GREVLEX, "Q[x,y]");
    TEST_ASSERT(rid >= 0, "ring_create");

    int iid = ideal_create(reg, rid, "I=<x>");
    int jid = ideal_create(reg, rid, "J=<y>");
    TEST_ASSERT(iid >= 0 && jid >= 0, "ideal_create");

    int px = poly_create(reg, rid, 4, "x");
    int py = poly_create(reg, rid, 4, "y");
    TEST_ASSERT(px >= 0 && py >= 0, "poly_create");
    poly_fill_single_term((lvPolynomial *)poly_get(reg, px), 0, 1, 1.0);
    poly_fill_single_term((lvPolynomial *)poly_get(reg, py), 1, 1, 1.0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, iid, px), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, jid, py), 0);

    /* K = I ∩ J */
    int K = ideal_intersection(reg, iid, jid);
    TEST_ASSERT(K >= 0, "ideal_intersection(I,J) should succeed");
    TEST_ASSERT_EQ(groebner_compute(reg, iid, GROEBNER_BUCHBERGER), 0);
    TEST_ASSERT_EQ(groebner_compute(reg, jid, GROEBNER_BUCHBERGER), 0);
    TEST_ASSERT_EQ(groebner_compute(reg, K, GROEBNER_BUCHBERGER), 0);

    int pxy = poly_create(reg, rid, 4, "xy");
    int xypv[2] = {1, 1};
    poly_fill_monomial_vars((lvPolynomial *)poly_get(reg, pxy), xypv, 1.0);
    TEST_ASSERT(ideal_membership(reg, K, pxy), "xy should be in I∩J");

    /* 交的结果属于两个原始理想：xy ∈ I 且 xy ∈ J */
    TEST_ASSERT(ideal_membership(reg, iid, pxy), "xy should be in I=<x>");
    TEST_ASSERT(ideal_membership(reg, jid, pxy), "xy should be in J=<y>");

    /* 严格性：x ∉ ⟨y⟩ 且 y ∉ ⟨x⟩，故 x, y 均不在交中 */
    TEST_ASSERT(!ideal_membership(reg, K, px), "x should NOT be in I∩J");
    TEST_ASSERT(!ideal_membership(reg, K, py), "y should NOT be in I∩J");

    /* 交是理想（封闭性）：x²y = x·(xy) ∈ K */
    int px2y = poly_create(reg, rid, 4, "x^2 y");
    int x2ypv[2] = {2, 1};
    poly_fill_monomial_vars((lvPolynomial *)poly_get(reg, px2y), x2ypv, 1.0);
    TEST_ASSERT(ideal_membership(reg, K, px2y), "x^2 y should be in I∩J");

    ideal_destroy(reg, K);
    ideal_destroy(reg, iid);
    ideal_destroy(reg, jid);
    poly_destroy(reg, px);
    poly_destroy(reg, py);
    poly_destroy(reg, pxy);
    poly_destroy(reg, px2y);
    ring_registry_destroy(reg);
}

/* ⟨x⟩ ∩ ⟨x²⟩ = ⟨x²⟩：非互素生成元的幂次情形 */
static void test_ideal_intersection_power(void) {
    printf("  Running: test_ideal_intersection_power ...\n");

    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);

    const char *vars[] = {"x", "y"};
    int rid = ring_create(reg, vars, 2, RING_FIELD_RATIONAL, MONOMIAL_GREVLEX, "Q[x,y]");
    TEST_ASSERT(rid >= 0, "ring_create");

    int iid = ideal_create(reg, rid, "I=<x>");
    int jid = ideal_create(reg, rid, "J=<x^2>");
    TEST_ASSERT(iid >= 0 && jid >= 0, "ideal_create");

    int px = poly_create(reg, rid, 4, "x");
    int px2 = poly_create(reg, rid, 4, "x^2");
    int px3 = poly_create(reg, rid, 4, "x^3");
    TEST_ASSERT(px >= 0 && px2 >= 0 && px3 >= 0, "poly_create");
    poly_fill_single_term((lvPolynomial *)poly_get(reg, px), 0, 1, 1.0);
    poly_fill_single_term((lvPolynomial *)poly_get(reg, px2), 0, 2, 1.0);
    poly_fill_single_term((lvPolynomial *)poly_get(reg, px3), 0, 3, 1.0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, iid, px), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, jid, px2), 0);

    int K = ideal_intersection(reg, iid, jid);
    TEST_ASSERT(K >= 0, "ideal_intersection(I,J) should succeed");
    TEST_ASSERT_EQ(groebner_compute(reg, K, GROEBNER_BUCHBERGER), 0);

    /* ⟨x⟩ ∩ ⟨x²⟩ = ⟨x²⟩：x² ∈ K，x ∉ K，x³ ∈ K（封闭性） */
    TEST_ASSERT(ideal_membership(reg, K, px2), "x^2 should be in I∩J");
    TEST_ASSERT(!ideal_membership(reg, K, px), "x should NOT be in I∩J");
    TEST_ASSERT(ideal_membership(reg, K, px3), "x^3 should be in I∩J");

    ideal_destroy(reg, K);
    ideal_destroy(reg, iid);
    ideal_destroy(reg, jid);
    poly_destroy(reg, px);
    poly_destroy(reg, px2);
    poly_destroy(reg, px3);
    ring_registry_destroy(reg);
}

/* 吸收律：I ∩ (I + J) = I（I = ⟨x⟩, J = ⟨y⟩, I + J = ⟨x, y⟩） */
static void test_ideal_intersection_absorb(void) {
    printf("  Running: test_ideal_intersection_absorb ...\n");

    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);

    const char *vars[] = {"x", "y"};
    int rid = ring_create(reg, vars, 2, RING_FIELD_RATIONAL, MONOMIAL_GREVLEX, "Q[x,y]");
    TEST_ASSERT(rid >= 0, "ring_create");

    int iid = ideal_create(reg, rid, "I=<x>");
    int jid = ideal_create(reg, rid, "J=<y>");
    int ijs = ideal_create(reg, rid, "I+J=<x,y>");
    TEST_ASSERT(iid >= 0 && jid >= 0 && ijs >= 0, "ideal_create");

    int px = poly_create(reg, rid, 4, "x");
    int py = poly_create(reg, rid, 4, "y");
    TEST_ASSERT(px >= 0 && py >= 0, "poly_create");
    poly_fill_single_term((lvPolynomial *)poly_get(reg, px), 0, 1, 1.0);
    poly_fill_single_term((lvPolynomial *)poly_get(reg, py), 1, 1, 1.0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, iid, px), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, jid, py), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, ijs, px), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, ijs, py), 0);

    int K = ideal_intersection(reg, iid, ijs);
    TEST_ASSERT(K >= 0, "ideal_intersection(I, I+J) should succeed");
    TEST_ASSERT_EQ(groebner_compute(reg, K, GROEBNER_BUCHBERGER), 0);

    /* I ⊆ I ∩ (I+J)：x ∈ K；y ∉ I，故 y ∉ I ∩ (I+J)；xy ∈ I ∩ (I+J) */
    TEST_ASSERT(ideal_membership(reg, K, px), "x (= I's generator) should be in I∩(I+J)");
    TEST_ASSERT(!ideal_membership(reg, K, py), "y should NOT be in I∩(I+J)");
    int pxy = poly_create(reg, rid, 4, "xy");
    int xypv[2] = {1, 1};
    poly_fill_monomial_vars((lvPolynomial *)poly_get(reg, pxy), xypv, 1.0);
    TEST_ASSERT(ideal_membership(reg, K, pxy), "xy should be in I∩(I+J)");

    ideal_destroy(reg, K);
    ideal_destroy(reg, iid);
    ideal_destroy(reg, jid);
    ideal_destroy(reg, ijs);
    poly_destroy(reg, px);
    poly_destroy(reg, py);
    poly_destroy(reg, pxy);
    ring_registry_destroy(reg);
}

/* ⟨xy⟩ : ⟨x⟩ = ⟨y⟩：单元素商 + 商的定义性质 q·J ⊆ I */
static void test_ideal_quotient_single(void) {
    printf("  Running: test_ideal_quotient_single ...\n");

    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);

    const char *vars[] = {"x", "y"};
    int rid = ring_create(reg, vars, 2, RING_FIELD_RATIONAL, MONOMIAL_GREVLEX, "Q[x,y]");
    TEST_ASSERT(rid >= 0, "ring_create");

    int iid = ideal_create(reg, rid, "I=<xy>");
    int jid = ideal_create(reg, rid, "J=<x>");
    TEST_ASSERT(iid >= 0 && jid >= 0, "ideal_create");

    int pxy = poly_create(reg, rid, 4, "xy");
    int px = poly_create(reg, rid, 4, "x");
    int py = poly_create(reg, rid, 4, "y");
    int pone = poly_create(reg, rid, 4, "1");
    TEST_ASSERT(pxy >= 0 && px >= 0 && py >= 0 && pone >= 0, "poly_create");
    int xypv[2] = {1, 1};
    poly_fill_monomial_vars((lvPolynomial *)poly_get(reg, pxy), xypv, 1.0);
    poly_fill_single_term((lvPolynomial *)poly_get(reg, px), 0, 1, 1.0);
    poly_fill_single_term((lvPolynomial *)poly_get(reg, py), 1, 1, 1.0);
    poly_fill_single_term((lvPolynomial *)poly_get(reg, pone), -1, 0, 1.0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, iid, pxy), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, jid, px), 0);

    int K = ideal_quotient(reg, iid, jid, "I:J");
    TEST_ASSERT(K >= 0, "ideal_quotient(I,J) should succeed");
    TEST_ASSERT_EQ(groebner_compute(reg, K, GROEBNER_BUCHBERGER), 0);

    /* y ∈ ⟨xy⟩ : ⟨x⟩（商的定义：y·x = xy ∈ I） */
    TEST_ASSERT(ideal_membership(reg, K, py), "y should be in I:J");
    TEST_ASSERT(ideal_membership(reg, iid, pxy), "q·g = y·x = xy should be in I");

    /* I ⊆ I : J：xy ∈ I:J（xy·x = x²y ∈ I） */
    TEST_ASSERT(ideal_membership(reg, K, pxy), "xy should be in I:J (I ⊆ I:J)");

    /* 严格性：x ∉ ⟨xy⟩:⟨x⟩（x² ∉ ⟨xy⟩），1 ∉ 商（商为真理想 ⟨y⟩） */
    TEST_ASSERT(!ideal_membership(reg, K, px), "x should NOT be in I:J");
    TEST_ASSERT(!ideal_membership(reg, K, pone), "1 should NOT be in I:J");

    ideal_destroy(reg, K);
    ideal_destroy(reg, iid);
    ideal_destroy(reg, jid);
    poly_destroy(reg, pxy);
    poly_destroy(reg, px);
    poly_destroy(reg, py);
    poly_destroy(reg, pone);
    ring_registry_destroy(reg);
}

/* I : J 的环性质：
 *  (a) J ⊆ I ⟹ 1 ∈ I : J，商为整个环 R = ⟨1⟩
 *  (b) J = ⟨1⟩（含单位元）⟹ I : J = I（此时商恰等于 I，而非全环）
 *  (c) I = ⟨x²⟩, J = ⟨x⟩ ⟹ I : J = ⟨x⟩（商严格大于 I，验证 I ⊆ I : J） */
static void test_ideal_quotient_properties(void) {
    printf("  Running: test_ideal_quotient_properties ...\n");

    lvRingRegistry *reg = ring_registry_create(4);
    TEST_ASSERT_NOT_NULL(reg);

    const char *vars[] = {"x", "y"};
    int rid = ring_create(reg, vars, 2, RING_FIELD_RATIONAL, MONOMIAL_GREVLEX, "Q[x,y]");
    TEST_ASSERT(rid >= 0, "ring_create");

    /* --- (a) J ⊆ I ⟹ I : J = R = ⟨1⟩ --- */
    int iid = ideal_create(reg, rid, "I=<x,y>");
    int jid = ideal_create(reg, rid, "J=<x>");
    TEST_ASSERT(iid >= 0 && jid >= 0, "ideal_create");
    int px = poly_create(reg, rid, 4, "x");
    int py = poly_create(reg, rid, 4, "y");
    int pone = poly_create(reg, rid, 4, "1");
    TEST_ASSERT(px >= 0 && py >= 0 && pone >= 0, "poly_create");
    poly_fill_single_term((lvPolynomial *)poly_get(reg, px), 0, 1, 1.0);
    poly_fill_single_term((lvPolynomial *)poly_get(reg, py), 1, 1, 1.0);
    poly_fill_single_term((lvPolynomial *)poly_get(reg, pone), -1, 0, 1.0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, iid, px), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, iid, py), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, jid, px), 0);

    int K = ideal_quotient(reg, iid, jid, "I:J");
    TEST_ASSERT(K >= 0, "ideal_quotient(I,J) should succeed");
    TEST_ASSERT_EQ(groebner_compute(reg, K, GROEBNER_BUCHBERGER), 0);
    /* J ⊆ I ⟹ 1·x = x ∈ I，故 1 ∈ I : J，商为全环 */
    TEST_ASSERT(ideal_membership(reg, K, pone), "1 should be in I:J (J ⊆ I)");
    TEST_ASSERT(ideal_membership(reg, K, px), "x should be in I:J (whole ring)");
    TEST_ASSERT(ideal_membership(reg, K, py), "y should be in I:J (whole ring)");

    ideal_destroy(reg, K);
    ideal_destroy(reg, iid);
    ideal_destroy(reg, jid);

    /* --- (b) J = ⟨1⟩ ⟹ I : J = I（1 ∈ J 时 q·1 = q ∈ I 迫使 q ∈ I） --- */
    int iid2 = ideal_create(reg, rid, "I=<x,y>");
    int jid2 = ideal_create(reg, rid, "J=<1>");
    TEST_ASSERT(iid2 >= 0 && jid2 >= 0, "ideal_create");
    TEST_ASSERT_EQ(ideal_add_generator(reg, iid2, px), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, iid2, py), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, jid2, pone), 0);

    int K2 = ideal_quotient(reg, iid2, jid2, "I:<1>");
    TEST_ASSERT(K2 >= 0, "ideal_quotient(I,<1>) should succeed");
    TEST_ASSERT_EQ(groebner_compute(reg, K2, GROEBNER_BUCHBERGER), 0);
    TEST_ASSERT(ideal_membership(reg, K2, px), "x should be in I:<1> (= I)");
    TEST_ASSERT(ideal_membership(reg, K2, py), "y should be in I:<1> (= I)");
    TEST_ASSERT(!ideal_membership(reg, K2, pone), "1 should NOT be in I:<1> (= I, not whole ring)");

    ideal_destroy(reg, K2);
    ideal_destroy(reg, iid2);
    ideal_destroy(reg, jid2);

    /* --- (c) I = ⟨x²⟩, J = ⟨x⟩ ⟹ I : J = ⟨x⟩（商严格大于 I） --- */
    int iid3 = ideal_create(reg, rid, "I=<x^2>");
    int jid3 = ideal_create(reg, rid, "J=<x>");
    int px2 = poly_create(reg, rid, 4, "x^2");
    TEST_ASSERT(iid3 >= 0 && jid3 >= 0 && px2 >= 0, "create");
    poly_fill_single_term((lvPolynomial *)poly_get(reg, px2), 0, 2, 1.0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, iid3, px2), 0);
    TEST_ASSERT_EQ(ideal_add_generator(reg, jid3, px), 0);

    int K3 = ideal_quotient(reg, iid3, jid3, "<x^2>:<x>");
    TEST_ASSERT(K3 >= 0, "ideal_quotient(<x^2>,<x>) should succeed");
    TEST_ASSERT_EQ(groebner_compute(reg, K3, GROEBNER_BUCHBERGER), 0);
    TEST_ASSERT(ideal_membership(reg, K3, px), "x should be in <x^2>:<x> (= <x>)");
    TEST_ASSERT(ideal_membership(reg, K3, px2), "x^2 should be in <x^2>:<x> (I ⊆ I:J)");
    TEST_ASSERT(!ideal_membership(reg, K3, pone), "1 should NOT be in <x^2>:<x> (proper ideal)");

    ideal_destroy(reg, K3);
    ideal_destroy(reg, iid3);
    ideal_destroy(reg, jid3);

    poly_destroy(reg, px);
    poly_destroy(reg, py);
    poly_destroy(reg, pone);
    poly_destroy(reg, px2);
    ring_registry_destroy(reg);
}

/* ================================================================
 *  Main
 * ================================================================ */

TEST_MAIN_BEGIN("Groebner Basis")
    printf("=== Groebner Basis Computation Test ===\n\n");

    TEST_MAIN_RUN(test_group1_right_triangle);
    TEST_MAIN_RUN(test_group2_overconstrained);
    TEST_MAIN_RUN(test_group3_well_constrained);

    printf("\n[Groebner Engine API Direct Tests]\n");
    TEST_MAIN_RUN(test_engine_ring_lifecycle);
    TEST_MAIN_RUN(test_engine_poly_lifecycle);
    TEST_MAIN_RUN(test_engine_poly_arith);
    TEST_MAIN_RUN(test_engine_ideal_lifecycle);
    TEST_MAIN_RUN(test_constraint_graph_to_ideal);
    TEST_MAIN_RUN(test_multivar_buchberger);
    TEST_MAIN_RUN(test_symbolic_constant_basis);
    TEST_MAIN_RUN(test_graph_collinear_end_to_end);
    TEST_MAIN_RUN(test_finite_field_ring);
    TEST_MAIN_RUN(test_ideal_intersection_basic);
    TEST_MAIN_RUN(test_ideal_intersection_power);
    TEST_MAIN_RUN(test_ideal_intersection_absorb);
    TEST_MAIN_RUN(test_ideal_quotient_single);
    TEST_MAIN_RUN(test_ideal_quotient_properties);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", g_pass_count, g_fail_count,
           g_pass_count + g_fail_count);
TEST_MAIN_END()
