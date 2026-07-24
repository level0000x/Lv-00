/**
 * @file test_solver_submodules.c
 * @brief 求解器子模块单元测试（solver_eq_system / solver_symbolic / solver_linear /
 *        solver_equation_extract / solver_eliminate / solver_result / solver_coord_extract）
 *
 * 测试覆盖：
 *   - EquationSystem 生命周期（创建/销毁/推入/查询）
 *   - 符号求解函数（coord_to_double, double_to_mpz_scaled, is_out_of_scope,
 *     try_factor_polynomial, check_incompatible_distances,
 *     check_contradiction_after_substitution, constraint_weight,
 *     count_point_variables, solve_quadratic_exact, solve_cubic_exact,
 *     solve_equations_pass, poly_eval_symbolic, cleanup_groebner_result）
 *   - 数值求解函数（solve_linear）
 *   - 坐标提取函数（point_coord, line_from_two_points, coord_to_mpz_scaled）
 *   - 方程提取函数（solver_extract_equations_full）
 *   - 消元与超出范围分析（eliminate_geometry, analyze_out_of_scope）
 *   - GroebnerResult 生命周期（groebner_result_destroy）
 *
 * 构建：需链接 core 库（liblv_core）及 GMP 库。
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/solver.h"
#include "lv/symbolic_coord.h"

#include "lv.h"
#include "test_helpers.h"

/* ================================================================== */
/*  全局计数器（test_helpers.h 要求）                                    */
/* ================================================================== */
int g_pass_count = 0;
int g_fail_count = 0;

/* ================================================================== */
/*  内部函数前向声明（各 solver 子模块中非 static 的函数）               */
/* ================================================================== */

/* --- solver_eq_system.c --- */
void equation_system_init(EquationSystem *sys);
int equation_system_push(EquationSystem *sys, mpz_poly_t poly, int var_node_id, int coord_index);
void equation_system_clear(EquationSystem *sys);

/* --- solver_symbolic.c --- */
bool coord_to_double(const SymbolicCoord *c, double *out);
void double_to_mpz_scaled(double val, mpz_t result, int64_t scale);
void substitute_solved(EquationSystem *sys, int var_node_id, int coord_index, double value);
bool is_out_of_scope(const mpz_poly_t *poly);
bool try_factor_polynomial(const mpz_poly_t *poly, mpz_poly_t *factor1, mpz_poly_t *factor2);
bool check_incompatible_distances(const ConstraintGraph *graph);
bool check_contradiction_after_substitution(EquationSystem *sys);
int constraint_weight(const Constraint *c);
int count_point_variables(const ConstraintGraph *graph, int **out_ids);
void solve_equations_pass(EquationSystem *sys, GroebnerResult *result, int *solved_count, int *multiple_solutions,
                          bool *no_solution, bool do_substitute);
int solve_quadratic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions);
int solve_cubic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions);
SymbolicCoord *poly_eval_symbolic(const mpz_poly_t *poly, const SymbolicCoord *value);
void cleanup_groebner_result(GroebnerResult *result);
bool compute_algebraic_resultant(const mpz_poly_t *p, const mpz_poly_t *q, AlgebraicOp op, mpz_poly_t *result);

/* --- solver_linear.c --- */
bool solve_linear(const mpz_poly_t *poly, double *x_out);

/* --- solver_coord_extract.c --- */
bool point_coord(const GeomNode *pt, int idx, double *out);
bool line_from_two_points(GeomNode *p1, GeomNode *p2, void *out);
bool coord_to_mpz_scaled(const SymbolicCoord *c, mpz_t result, int64_t scale);
void extract_equations_from_constraints(const ConstraintGraph *graph, EquationSystem *sys);

/* ================================================================== */
/*  辅助函数：创建带有理坐标的点                                         */
/* ================================================================== */
static inline int add_rpoint(ConstraintGraph *g, int64_t xn, uint64_t xd, int64_t yn, uint64_t yd) {
    if (!g)
        return -1;
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
    AddNodeResult r = graph_add_point(g, coords, 2);
    if (r != ADD_NODE_OK)
        return -1;
    return g->next_node_id - 1;
}

/* ================================================================== */
/*  辅助函数：创建辅助 mpz_poly_t（仅度数为1的线性多项式）               */
/* ================================================================== */
static int make_linear_poly(mpz_poly_t *poly, int64_t a, int64_t b) {
    mpz_poly_init(poly);
    poly->degree = 1;
    poly->coeffs = (mpz_t *) malloc(2 * sizeof(mpz_t));
    if (!poly->coeffs) {
        mpz_poly_clear(poly);
        return -1;
    }
    mpz_init_set_si(poly->coeffs[1], a);
    mpz_init_set_si(poly->coeffs[0], b);
    return 0;
}

/* ================================================================== */
/*  辅助函数：创建二次多项式 a*x^2 + b*x + c                             */
/* ================================================================== */
static int make_quadratic_poly(mpz_poly_t *poly, int64_t a, int64_t b, int64_t c) {
    mpz_poly_init(poly);
    poly->degree = 2;
    poly->coeffs = (mpz_t *) malloc(3 * sizeof(mpz_t));
    if (!poly->coeffs) {
        mpz_poly_clear(poly);
        return -1;
    }
    mpz_init_set_si(poly->coeffs[2], a);
    mpz_init_set_si(poly->coeffs[1], b);
    mpz_init_set_si(poly->coeffs[0], c);
    return 0;
}

/* ================================================================== */
/*  辅助函数：创建三次多项式 a*x^3 + b*x^2 + c*x + d                    */
/* ================================================================== */
static int make_cubic_poly(mpz_poly_t *poly, int64_t a, int64_t b, int64_t c, int64_t d) {
    mpz_poly_init(poly);
    poly->degree = 3;
    poly->coeffs = (mpz_t *) malloc(4 * sizeof(mpz_t));
    if (!poly->coeffs) {
        mpz_poly_clear(poly);
        return -1;
    }
    mpz_init_set_si(poly->coeffs[3], a);
    mpz_init_set_si(poly->coeffs[2], b);
    mpz_init_set_si(poly->coeffs[1], c);
    mpz_init_set_si(poly->coeffs[0], d);
    return 0;
}

/* ================================================================== */
/*  1. EquationSystem 生命周期测试                                       */
/* ================================================================== */

static void test_eq_system_create_destroy(void) {
    /* 正常创建/销毁 */
    EquationSystem *sys = equation_system_create();
    TEST_ASSERT_NOT_NULL(sys);
    TEST_ASSERT_EQ(equation_system_count(sys), 0);

    /* 查询空系统 */
    TEST_ASSERT_NULL(equation_system_get_poly(sys, 0));
    TEST_ASSERT_EQ(equation_system_get_var_id(sys, 0), -1);
    TEST_ASSERT_EQ(equation_system_get_coord_index(sys, 0), -1);

    equation_system_destroy(sys);

    /* NULL 销毁安全 */
    equation_system_destroy(NULL);

    /* count(NULL) 返回 0 */
    TEST_ASSERT_EQ(equation_system_count(NULL), 0);

    /* getters(NULL) */
    TEST_ASSERT_NULL(equation_system_get_poly(NULL, 0));
    TEST_ASSERT_EQ(equation_system_get_var_id(NULL, 0), -1);
    TEST_ASSERT_EQ(equation_system_get_coord_index(NULL, 0), -1);
}

static void test_eq_system_init_clear(void) {
    EquationSystem sys;
    equation_system_init(&sys);
    TEST_ASSERT_EQ(sys.count, 0);
    TEST_ASSERT_EQ(sys.capacity, 0);
    TEST_ASSERT_NULL(sys.eqs);

    equation_system_clear(&sys);
    TEST_ASSERT_EQ(sys.count, 0);
    TEST_ASSERT_EQ(sys.capacity, 0);
    TEST_ASSERT_NULL(sys.eqs);

    /* clear(NULL) 安全 */
    equation_system_clear(NULL);
}

static void test_eq_system_push(void) {
    EquationSystem sys;
    equation_system_init(&sys);

    mpz_poly_t poly;
    TEST_ASSERT_EQ(make_linear_poly(&poly, 2, -4), 0); /* 2x - 4 = 0 */

    /* 第一次 push（触发初始容量分配） */
    int ret = equation_system_push(&sys, poly, 100, 0);
    TEST_ASSERT_EQ(ret, 0);
    TEST_ASSERT_EQ(sys.count, 1);
    TEST_ASSERT(sys.capacity >= 1, "capacity should be valid");
    TEST_ASSERT_EQ(equation_system_get_var_id(&sys, 0), 100);
    TEST_ASSERT_EQ(equation_system_get_coord_index(&sys, 0), 0);
    TEST_ASSERT_NOT_NULL(equation_system_get_poly(&sys, 0));

    /* 第二次 push */
    mpz_poly_t poly2;
    TEST_ASSERT_EQ(make_linear_poly(&poly2, 1, 5), 0);
    ret = equation_system_push(&sys, poly2, 101, 1);
    TEST_ASSERT_EQ(ret, 0);
    TEST_ASSERT_EQ(sys.count, 2);
    TEST_ASSERT_EQ(equation_system_get_var_id(&sys, 1), 101);
    TEST_ASSERT_EQ(equation_system_get_coord_index(&sys, 1), 1);

    mpz_poly_clear(&poly);
    mpz_poly_clear(&poly2);
    equation_system_clear(&sys);
}

static void test_eq_system_push_many(void) {
    /* 压入 20 个方程，验证扩容逻辑 */
    EquationSystem sys;
    equation_system_init(&sys);

    for (int i = 0; i < 20; i++) {
        mpz_poly_t poly;
        TEST_ASSERT_EQ(make_linear_poly(&poly, 1, -i), 0);
        int ret = equation_system_push(&sys, poly, i, (i % 2));
        TEST_ASSERT_EQ(ret, 0);
        mpz_poly_clear(&poly);
    }

    TEST_ASSERT_EQ(sys.count, 20);
    TEST_ASSERT(sys.capacity >= 20, "capacity should be valid");

    /* 验证每个条目 */
    for (int i = 0; i < 20; i++) {
        TEST_ASSERT_EQ(equation_system_get_var_id(&sys, i), i);
        TEST_ASSERT_EQ(equation_system_get_coord_index(&sys, i), (i % 2));
        const mpz_poly_t *p = equation_system_get_poly(&sys, i);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_EQ(p->degree, 1);
    }

    equation_system_clear(&sys);
}

static void test_eq_system_getters_edge_cases(void) {
    EquationSystem *sys = equation_system_create();
    TEST_ASSERT_NOT_NULL(sys);

    /* 负数索引 */
    TEST_ASSERT_NULL(equation_system_get_poly(sys, -1));
    TEST_ASSERT_EQ(equation_system_get_var_id(sys, -1), -1);
    TEST_ASSERT_EQ(equation_system_get_coord_index(sys, -1), -1);

    /* 超出范围索引 */
    TEST_ASSERT_NULL(equation_system_get_poly(sys, 999));
    TEST_ASSERT_EQ(equation_system_get_var_id(sys, 999), -1);
    TEST_ASSERT_EQ(equation_system_get_coord_index(sys, 999), -1);

    equation_system_destroy(sys);
}

/* ================================================================== */
/*  2. solve_linear 测试（solver_linear.c / solver_coord_extract.c）     */
/* ================================================================== */

static void test_solve_linear_basic(void) {
    mpz_poly_t poly;
    double x;

    /* 2x - 4 = 0 -> x = 2 */
    TEST_ASSERT_EQ(make_linear_poly(&poly, 2, -4), 0);
    TEST_ASSERT(solve_linear(&poly, &x), "solve linear should succeed");
    TEST_ASSERT(fabs(x - 2.0) < 1e-12, "fabs should succeed");
    mpz_poly_clear(&poly);

    /* 3x + 6 = 0 -> x = -2 */
    TEST_ASSERT_EQ(make_linear_poly(&poly, 3, 6), 0);
    TEST_ASSERT(solve_linear(&poly, &x), "solve linear should succeed");
    TEST_ASSERT(fabs(x + 2.0) < 1e-12, "fabs should succeed");
    mpz_poly_clear(&poly);

    /* -5x + 10 = 0 -> x = 2 */
    TEST_ASSERT_EQ(make_linear_poly(&poly, -5, 10), 0);
    TEST_ASSERT(solve_linear(&poly, &x), "solve linear should succeed");
    TEST_ASSERT(fabs(x - 2.0) < 1e-12, "fabs should succeed");
    mpz_poly_clear(&poly);
}

static void test_solve_linear_edge_cases(void) {
    mpz_poly_t poly;
    double x;

    /* 0*x + 3 = 0 -> a ~= 0, should fail */
    TEST_ASSERT_EQ(make_linear_poly(&poly, 0, 3), 0);
    TEST_ASSERT(!solve_linear(&poly, &x), "solve linear should fail for invalid input");
    mpz_poly_clear(&poly);

    /* 次数为 2 的非线性 -> should fail */
    TEST_ASSERT_EQ(make_quadratic_poly(&poly, 1, 0, -4), 0);
    TEST_ASSERT(!solve_linear(&poly, &x), "solve linear should fail for invalid input");
    mpz_poly_clear(&poly);

    /* degree < 0 的多项式 */
    mpz_poly_init(&poly);
    poly.degree = -1;
    poly.coeffs = NULL;
    TEST_ASSERT(!solve_linear(&poly, &x), "solve linear should fail for invalid input");
    mpz_poly_clear(&poly);

    /* NULL 指针 */
    TEST_ASSERT(!solve_linear(NULL, &x), "solve linear should fail for invalid input");
}

/* ================================================================== */
/*  3. coord_to_double 测试（solver_symbolic.c / solver_coord_extract.c） */
/* ================================================================== */

static void test_coord_to_double(void) {
    double val;

    /* RATIONAL 类型 */
    SymbolicCoord *c = symbolic_coord_create_rational(3, 1);
    TEST_ASSERT(coord_to_double(c, &val), "coord to double should succeed");
    TEST_ASSERT(fabs(val - 3.0) < 1e-12, "fabs should succeed");
    symbolic_coord_destroy(c);

    /* 负有理数 */
    c = symbolic_coord_create_rational(-5, 2);
    TEST_ASSERT(coord_to_double(c, &val), "coord to double should succeed");
    TEST_ASSERT(fabs(val - (-2.5)) < 1e-12, "fabs should succeed");
    symbolic_coord_destroy(c);

    /* 零 */
    c = symbolic_coord_create_rational(0, 1);
    TEST_ASSERT(coord_to_double(c, &val), "coord to double should succeed");
    TEST_ASSERT(fabs(val) < 1e-12, "fabs should succeed");
    symbolic_coord_destroy(c);

    /* NULL 输入 */
    TEST_ASSERT(!coord_to_double(NULL, &val), "coord to double should fail for invalid input");
}

/* ================================================================== */
/*  4. double_to_mpz_scaled 测试（solver_symbolic.c / solver_coord_extract.c） */
/* ================================================================== */

static void test_double_to_mpz_scaled(void) {
    mpz_t result;
    mpz_init(result);

    /* 3.0 * 1000 = 3000 */
    double_to_mpz_scaled(3.0, result, 1000);
    TEST_ASSERT(mpz_cmp_si(result, 3000) == 0, "mpz cmp si should succeed");

    /* -2.5 * 1000 = -2500 */
    double_to_mpz_scaled(-2.5, result, 1000);
    TEST_ASSERT(mpz_cmp_si(result, -2500) == 0, "mpz cmp si should succeed");

    /* 0.0 * 1000 = 0 */
    double_to_mpz_scaled(0.0, result, 1000);
    TEST_ASSERT(mpz_cmp_si(result, 0) == 0, "mpz cmp si should succeed");

    /* 无穷大 -> 应为 0（防御性处理） */
    double_to_mpz_scaled(INFINITY, result, 1000);
    TEST_ASSERT(mpz_cmp_si(result, 0) == 0, "mpz cmp si should succeed");

    /* NaN -> 应为 0 */
    double_to_mpz_scaled(NAN, result, 1000);
    TEST_ASSERT(mpz_cmp_si(result, 0) == 0, "mpz cmp si should succeed");

    /* 极小值接近零 */
    double_to_mpz_scaled(1e-15, result, 1000);
    TEST_ASSERT(mpz_cmp_si(result, 0) == 0, "mpz cmp si should succeed");

    mpz_clear(result);
}

/* ================================================================== */
/*  5. coord_to_mpz_scaled 测试（solver_coord_extract.c）                */
/* ================================================================== */

static void test_coord_to_mpz_scaled(void) {
    mpz_t result;
    mpz_init(result);

    /* RATIONAL: 3/1 * 1000 = 3000 */
    SymbolicCoord *c = symbolic_coord_create_rational(3, 1);
    TEST_ASSERT(coord_to_mpz_scaled(c, result, 1000), "coord to mpz scaled should succeed");
    TEST_ASSERT(mpz_cmp_si(result, 3000) == 0, "mpz cmp si should succeed");
    symbolic_coord_destroy(c);

    /* RATIONAL: -5/2 * 1000 = -2500 */
    c = symbolic_coord_create_rational(-5, 2);
    TEST_ASSERT(coord_to_mpz_scaled(c, result, 1000), "coord to mpz scaled should succeed");
    TEST_ASSERT(mpz_cmp_si(result, -2500) == 0, "mpz cmp si should succeed");
    symbolic_coord_destroy(c);

    /* NULL 输入 */
    TEST_ASSERT(!coord_to_mpz_scaled(NULL, result, 1000), "coord to mpz scaled should fail for invalid input");

    mpz_clear(result);
}

/* ================================================================== */
/*  6. is_out_of_scope 测试（solver_symbolic.c）                         */
/* ================================================================== */

static void test_is_out_of_scope(void) {
    mpz_poly_t poly;

    /* degree 1 -> in scope */
    TEST_ASSERT_EQ(make_linear_poly(&poly, 1, 0), 0);
    TEST_ASSERT(!is_out_of_scope(&poly), "is out of scope should fail for invalid input");
    mpz_poly_clear(&poly);

    /* degree 2 -> in scope */
    TEST_ASSERT_EQ(make_quadratic_poly(&poly, 1, 0, -4), 0);
    TEST_ASSERT(!is_out_of_scope(&poly), "is out of scope should fail for invalid input");
    mpz_poly_clear(&poly);

    /* degree 3 -> in scope */
    TEST_ASSERT_EQ(make_cubic_poly(&poly, 1, 0, 0, -8), 0);
    TEST_ASSERT(!is_out_of_scope(&poly), "is out of scope should fail for invalid input");
    mpz_poly_clear(&poly);

    /* degree 4 -> in scope */
    mpz_poly_init(&poly);
    poly.degree = 4;
    poly.coeffs = (mpz_t *) malloc(5 * sizeof(mpz_t));
    for (int i = 0; i <= 4; i++)
        mpz_init_set_si(poly.coeffs[i], 0);
    mpz_set_si(poly.coeffs[4], 1);
    mpz_set_si(poly.coeffs[0], -16);
    TEST_ASSERT(!is_out_of_scope(&poly), "is out of scope should fail for invalid input");
    mpz_poly_clear(&poly);

    /* degree 5 -> out of scope */
    mpz_poly_init(&poly);
    poly.degree = 5;
    poly.coeffs = (mpz_t *) malloc(6 * sizeof(mpz_t));
    for (int i = 0; i <= 5; i++)
        mpz_init_set_si(poly.coeffs[i], 0);
    mpz_set_si(poly.coeffs[5], 1);
    TEST_ASSERT(is_out_of_scope(&poly), "is out of scope should succeed");
    mpz_poly_clear(&poly);
}

/* ================================================================== */
/*  7. try_factor_polynomial 测试（solver_symbolic.c）                   */
/* ================================================================== */

static void test_try_factor_polynomial(void) {
    mpz_poly_t poly, f1, f2;

    /* x^2 - 1 = (x-1)(x+1), degree=2, should fail (degree < 3) */
    TEST_ASSERT_EQ(make_quadratic_poly(&poly, 1, 0, -1), 0);
    TEST_ASSERT(!try_factor_polynomial(&poly, &f1, &f2), "try factor polynomial should fail for invalid input");
    mpz_poly_clear(&poly);

    /* x^3 - x = x*(x^2 - 1) = x*(x-1)*(x+1), 常数项为 0  -> 提取 x */
    mpz_poly_init(&poly);
    poly.degree = 3;
    poly.coeffs = (mpz_t *) malloc(4 * sizeof(mpz_t));
    mpz_init_set_si(poly.coeffs[3], 1);  /* x^3 */
    mpz_init_set_si(poly.coeffs[2], 0);  /* 0 */
    mpz_init_set_si(poly.coeffs[1], -1); /* -x */
    mpz_init_set_si(poly.coeffs[0], 0);  /* 0 */
    TEST_ASSERT(try_factor_polynomial(&poly, &f1, &f2), "try factor polynomial should succeed");
    /* f1 = x (coeffs: [0, 1]) */
    TEST_ASSERT_EQ(f1.degree, 1);
    TEST_ASSERT(mpz_cmp_si(f1.coeffs[0], 0) == 0, "mpz cmp si should succeed");
    TEST_ASSERT(mpz_cmp_si(f1.coeffs[1], 1) == 0, "mpz cmp si should succeed");
    /* f2 = x^2 - 1 */
    TEST_ASSERT_EQ(f2.degree, 2);
    mpz_poly_clear(&f1);
    mpz_poly_clear(&f2);
    mpz_poly_clear(&poly);

    /* x^3 - 8 = (x-2)(x^2+2x+4), 根 x=2 */
    mpz_poly_init(&poly);
    poly.degree = 3;
    poly.coeffs = (mpz_t *) malloc(4 * sizeof(mpz_t));
    mpz_init_set_si(poly.coeffs[3], 1);  /* x^3 */
    mpz_init_set_si(poly.coeffs[2], 0);  /* 0 */
    mpz_init_set_si(poly.coeffs[1], 0);  /* 0 */
    mpz_init_set_si(poly.coeffs[0], -8); /* -8 */
    TEST_ASSERT(try_factor_polynomial(&poly, &f1, &f2), "try factor polynomial should succeed");
    TEST_ASSERT_EQ(f1.degree, 1);
    /* f1 = (x - 2) -> coeffs: [-2, 1] */
    TEST_ASSERT(mpz_cmp_si(f1.coeffs[0], -2) == 0, "mpz cmp si should succeed");
    TEST_ASSERT(mpz_cmp_si(f1.coeffs[1], 1) == 0, "mpz cmp si should succeed");
    mpz_poly_clear(&f1);
    mpz_poly_clear(&f2);
    mpz_poly_clear(&poly);

    /* x^3 + x^2 + x + 1 = (x+1)(x^2+1), 根 x=-1 */
    mpz_poly_init(&poly);
    poly.degree = 3;
    poly.coeffs = (mpz_t *) malloc(4 * sizeof(mpz_t));
    mpz_init_set_si(poly.coeffs[3], 1);
    mpz_init_set_si(poly.coeffs[2], 1);
    mpz_init_set_si(poly.coeffs[1], 1);
    mpz_init_set_si(poly.coeffs[0], 1);
    TEST_ASSERT(try_factor_polynomial(&poly, &f1, &f2), "try factor polynomial should succeed");
    TEST_ASSERT_EQ(f1.degree, 1);
    /* f1 = (x + 1) -> coeffs: [1, 1] 即 (x - (-1)) */
    TEST_ASSERT(mpz_cmp_si(f1.coeffs[0], 1) == 0, "mpz cmp si should succeed"); /* -(-1) = 1 */
    TEST_ASSERT(mpz_cmp_si(f1.coeffs[1], 1) == 0, "mpz cmp si should succeed");
    mpz_poly_clear(&f1);
    mpz_poly_clear(&f2);
    mpz_poly_clear(&poly);

    /* 不可分解的三次: x^3 + x + 1（整数根不存在，常数项为 1，除数 +/-1 都不是根） */
    mpz_poly_init(&poly);
    poly.degree = 3;
    poly.coeffs = (mpz_t *) malloc(4 * sizeof(mpz_t));
    mpz_init_set_si(poly.coeffs[3], 1);
    mpz_init_set_si(poly.coeffs[2], 0);
    mpz_init_set_si(poly.coeffs[1], 1);
    mpz_init_set_si(poly.coeffs[0], 1);
    TEST_ASSERT(!try_factor_polynomial(&poly, &f1, &f2), "try factor polynomial should fail for invalid input");
    mpz_poly_clear(&poly);
}

/* ================================================================== */
/*  8. check_incompatible_distances 测试                                 */
/* ================================================================== */

static void test_check_incompatible_distances(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    /* 空图 -> 无矛盾 */
    TEST_ASSERT(!check_incompatible_distances(g), "check incompatible distances should fail for invalid input");

    /* 创建一条线段 */
    int p1 = add_rpoint(g, 0, 1, 0, 1);
    int p2 = add_rpoint(g, 3, 1, 0, 1);
    int seg1 = graph_add_line_segment(g, p1, p2);
    (void) seg1;

    /* 为线段设置 numeric_assumption_declaration */
    GeomNode *seg_node = graph_get_node(g, g->next_node_id - 1);
    TEST_ASSERT_NOT_NULL(seg_node);
    if (seg_node) {
        seg_node->numeric_assumption_declaration = strdup("distance=3.0");
    }

    /* 创建另一条同端点线段但不同距离 */
    int seg2 = graph_add_line_segment(g, p1, p2);
    GeomNode *seg_node2 = graph_get_node(g, g->next_node_id - 1);
    if (seg_node2) {
        seg_node2->numeric_assumption_declaration = strdup("distance=5.0");
        /* 复制端点坐标以便端点比较 */
        if (seg_node && seg_node->coord_count >= 4 && seg_node->symbolic_coords) {
            seg_node2->coord_count = seg_node->coord_count;
            seg_node2->symbolic_coords = seg_node->symbolic_coords;
        }
    }

    graph_destroy(g);
}

/* ================================================================== */
/*  9. check_contradiction_after_substitution 测试                      */
/* ================================================================== */

static void test_check_contradiction(void) {
    EquationSystem sys;
    equation_system_init(&sys);

    /* 空系统 -> 无矛盾 */
    TEST_ASSERT(!check_contradiction_after_substitution(&sys), "check contradiction after substitution should fail for invalid input");

    /* degree 0 且系数非零 -> 矛盾 */
    {
        mpz_poly_t poly;
        mpz_poly_init(&poly);
        poly.degree = 0;
        poly.coeffs = (mpz_t *) malloc(1 * sizeof(mpz_t));
        mpz_init_set_si(poly.coeffs[0], 5);
        equation_system_push(&sys, poly, 1, 0);
        mpz_poly_clear(&poly);
    }
    TEST_ASSERT(check_contradiction_after_substitution(&sys), "check contradiction after substitution should succeed");
    equation_system_clear(&sys);

    /* degree 0 且系数为零 -> 无矛盾（0=0 是恒等式） */
    {
        mpz_poly_t poly;
        mpz_poly_init(&poly);
        poly.degree = 0;
        poly.coeffs = (mpz_t *) malloc(1 * sizeof(mpz_t));
        mpz_init_set_si(poly.coeffs[0], 0);
        equation_system_push(&sys, poly, 1, 0);
        mpz_poly_clear(&poly);
    }
    TEST_ASSERT(!check_contradiction_after_substitution(&sys), "check contradiction after substitution should fail for invalid input");

    /* degree 1 且系数不为零 -> 无矛盾（有解） */
    {
        mpz_poly_t poly;
        mpz_poly_init(&poly);
        poly.degree = 1;
        poly.coeffs = (mpz_t *) malloc(2 * sizeof(mpz_t));
        mpz_init_set_si(poly.coeffs[1], 2);
        mpz_init_set_si(poly.coeffs[0], -4);
        equation_system_push(&sys, poly, 1, 0);
        mpz_poly_clear(&poly);
    }
    TEST_ASSERT(!check_contradiction_after_substitution(&sys), "check contradiction after substitution should fail for invalid input");

    equation_system_clear(&sys);
}

/* ================================================================== */
/*  10. constraint_weight / count_point_variables 测试                  */
/* ================================================================== */

static void test_constraint_weight(void) {
    Constraint c;

    c.type = INCIDENCE;
    TEST_ASSERT_EQ(constraint_weight(&c), 1);

    c.type = BETWEENNESS;
    TEST_ASSERT_EQ(constraint_weight(&c), 2);

    c.type = INTERSECTION;
    TEST_ASSERT_EQ(constraint_weight(&c), 2);

    c.type = CONTAINMENT;
    TEST_ASSERT_EQ(constraint_weight(&c), 1);

    c.type = CONNECTION;
    TEST_ASSERT_EQ(constraint_weight(&c), 1);

    c.type = (ConstraintType) 999;
    TEST_ASSERT_EQ(constraint_weight(&c), 1);
}

static void test_count_point_variables(void) {
    ConstraintGraph *g = graph_create();

    /* 空图 */
    int *ids = NULL;
    TEST_ASSERT_EQ(count_point_variables(g, &ids), 0);
    TEST_ASSERT_NULL(ids);

    /* 添加两个点和一个线段 */
    add_rpoint(g, 0, 1, 0, 1);
    add_rpoint(g, 1, 1, 1, 1);
    graph_add_line_segment(g, 1, 2);

    int cnt = count_point_variables(g, &ids);
    TEST_ASSERT_EQ(cnt, 2);
    TEST_ASSERT_NOT_NULL(ids);
    lv_free((void **) &ids);

    /* out_ids 为 NULL */
    cnt = count_point_variables(g, NULL);
    TEST_ASSERT_EQ(cnt, 2);

    graph_destroy(g);
}

/* ================================================================== */
/*  11. solve_quadratic_exact 测试（solver_symbolic.c）                  */
/* ================================================================== */

static void test_quadratic_exact_basic(void) {
    SymbolicCoord *solutions[2] = {NULL, NULL};
    mpz_poly_t poly;
    int n;

    /* x^2 - 4 = 0 -> x = +/-2 */
    TEST_ASSERT_EQ(make_quadratic_poly(&poly, 1, 0, -4), 0);
    n = solve_quadratic_exact(&poly, solutions, 2);
    TEST_ASSERT_EQ(n, 2);
    if (n >= 1 && solutions[0])
        symbolic_coord_destroy(solutions[0]);
    if (n >= 2 && solutions[1])
        symbolic_coord_destroy(solutions[1]);
    mpz_poly_clear(&poly);

    /* x^2 - 2x + 1 = (x-1)^2 = 0 -> 唯一解 x=1 */
    TEST_ASSERT_EQ(make_quadratic_poly(&poly, 1, -2, 1), 0);
    n = solve_quadratic_exact(&poly, solutions, 2);
    TEST_ASSERT_EQ(n, 1);
    if (n >= 1 && solutions[0])
        symbolic_coord_destroy(solutions[0]);
    mpz_poly_clear(&poly);

    /* x^2 + 1 = 0 -> 无实根 */
    TEST_ASSERT_EQ(make_quadratic_poly(&poly, 1, 0, 1), 0);
    n = solve_quadratic_exact(&poly, solutions, 2);
    TEST_ASSERT_EQ(n, 0);
    mpz_poly_clear(&poly);

    /* NULL 输入 */
    n = solve_quadratic_exact(NULL, solutions, 2);
    TEST_ASSERT_EQ(n, 0);
}

static void test_quadratic_exact_overflow(void) {
    /* 测试超出 int64 范围的大系数 */
    SymbolicCoord *solutions[2] = {NULL, NULL};
    mpz_poly_t poly;
    mpz_poly_init(&poly);
    poly.degree = 2;
    poly.coeffs = (mpz_t *) malloc(3 * sizeof(mpz_t));
    mpz_init_set_str(poly.coeffs[2], "1000000000000", 10);
    mpz_init_set_str(poly.coeffs[1], "0", 10);
    mpz_init_set_str(poly.coeffs[0], "-4000000000000", 10);
    int n = solve_quadratic_exact(&poly, solutions, 2);
    /* 至少应返回 0（安全失败）或正常解 */
    TEST_ASSERT(n >= 0, "n should be valid");
    if (n >= 1 && solutions[0])
        symbolic_coord_destroy(solutions[0]);
    if (n >= 2 && solutions[1])
        symbolic_coord_destroy(solutions[1]);
    mpz_poly_clear(&poly);
}

/* ================================================================== */
/*  12. solve_cubic_exact 测试（solver_symbolic.c）                      */
/* ================================================================== */

static void test_cubic_exact_basic(void) {
    SymbolicCoord *solutions[3] = {NULL, NULL, NULL};
    mpz_poly_t poly;
    int n;

    /* x^3 - 8 = 0 -> 一个实根 x~=2 */
    TEST_ASSERT_EQ(make_cubic_poly(&poly, 1, 0, 0, -8), 0);
    n = solve_cubic_exact(&poly, solutions, 3);
    TEST_ASSERT(n >= 1, "n should be valid");
    for (int i = 0; i < n; i++) {
        if (solutions[i])
            symbolic_coord_destroy(solutions[i]);
    }
    mpz_poly_clear(&poly);

    /* NULL 输入 */
    n = solve_cubic_exact(NULL, solutions, 3);
    TEST_ASSERT_EQ(n, 0);

    /* 次数不为 3 */
    TEST_ASSERT_EQ(make_quadratic_poly(&poly, 1, 0, -4), 0);
    n = solve_cubic_exact(&poly, solutions, 3);
    TEST_ASSERT_EQ(n, 0);
    mpz_poly_clear(&poly);
}

/* ================================================================== */
/*  13. solve_equations_pass 测试                                       */
/* ================================================================== */

static void test_solve_equations_pass(void) {
    EquationSystem sys;
    equation_system_init(&sys);

    /* 添加一个线性方程：2x - 4 = 0 -> x=2 */
    {
        mpz_poly_t poly;
        TEST_ASSERT_EQ(make_linear_poly(&poly, 2, -4), 0);
        equation_system_push(&sys, poly, 1, 0);
        mpz_poly_clear(&poly);
    }

    GroebnerResult *result = (GroebnerResult *) lv_calloc(1, sizeof(GroebnerResult));
    TEST_ASSERT_NOT_NULL(result);

    int solved = 0, multi = 0;
    bool no_sol = false;
    solve_equations_pass(&sys, result, &solved, &multi, &no_sol, false);

    /* 应至少解出一个 */
    TEST_ASSERT(solved >= 1, "solved should be valid");

    cleanup_groebner_result(result);
    lv_free((void **) &result);
    equation_system_clear(&sys);
}

/* ================================================================== */
/*  14. poly_eval_symbolic 测试                                         */
/* ================================================================== */

static void test_poly_eval_symbolic(void) {
    mpz_poly_t poly;
    /* f(x) = 2x - 4, 求值 f(3) = 2 */
    TEST_ASSERT_EQ(make_linear_poly(&poly, 2, -4), 0);

    SymbolicCoord *val = symbolic_coord_create_rational(3, 1);
    TEST_ASSERT_NOT_NULL(val);

    SymbolicCoord *result = poly_eval_symbolic(&poly, val);
    TEST_ASSERT_NOT_NULL(result);

    double d;
    TEST_ASSERT(coord_to_double(result, &d), "coord to double should succeed");
    TEST_ASSERT(fabs(d - 2.0) < 1e-6, "fabs should succeed");

    symbolic_coord_destroy(val);
    symbolic_coord_destroy(result);
    mpz_poly_clear(&poly);

    /* NULL 输入 */
    TEST_ASSERT_NULL(poly_eval_symbolic(NULL, val));
    symbolic_coord_destroy(val);
}

/* ================================================================== */
/*  15. point_coord 测试（solver_coord_extract.c）                       */
/* ================================================================== */

static void test_point_coord(void) {
    ConstraintGraph *g = graph_create();
    int pid = add_rpoint(g, 3, 1, 7, 1);
    GeomNode *pt = graph_get_node(g, pid);
    TEST_ASSERT_NOT_NULL(pt);

    double x, y;
    TEST_ASSERT(point_coord(pt, 0, &x), "point coord should succeed");
    TEST_ASSERT(fabs(x - 3.0) < 1e-12, "fabs should succeed");
    TEST_ASSERT(point_coord(pt, 1, &y), "point coord should succeed");
    TEST_ASSERT(fabs(y - 7.0) < 1e-12, "fabs should succeed");

    /* 超出范围的索引 */
    TEST_ASSERT(!point_coord(pt, 5, &x), "point coord should fail for invalid input");

    /* NULL 输入 */
    TEST_ASSERT(!point_coord(NULL, 0, &x), "point coord should fail for invalid input");

    graph_destroy(g);
}

/* ================================================================== */
/*  16. line_from_two_points 测试（solver_coord_extract.c）              */
/* ================================================================== */

static void test_line_from_two_points(void) {
    ConstraintGraph *g = graph_create();
    int p1 = add_rpoint(g, 0, 1, 0, 1);
    int p2 = add_rpoint(g, 1, 1, 0, 1);

    GeomNode *pt1 = graph_get_node(g, p1);
    GeomNode *pt2 = graph_get_node(g, p2);
    TEST_ASSERT_NOT_NULL(pt1);
    TEST_ASSERT_NOT_NULL(pt2);

    /* LineEquation 栈上结构（与 solver_coord_extract.c 中的定义匹配） */
    struct {
        double a, b, c;
    } le;
    TEST_ASSERT(line_from_two_points(pt1, pt2, &le), "line from two points should succeed");
    /* 水平线 y=0 -> a=0, b=-1, c=0 */
    TEST_ASSERT(fabs(le.a) < 1e-12, "fabs should succeed");
    TEST_ASSERT(fabs(le.b + 1.0) < 1e-12, "fabs should succeed");

    /* 垂直线：两个点 x 相同 */
    int p3 = add_rpoint(g, 0, 1, 0, 1);
    int p4 = add_rpoint(g, 0, 1, 1, 1);
    GeomNode *pt3 = graph_get_node(g, p3);
    GeomNode *pt4 = graph_get_node(g, p4);
    TEST_ASSERT(line_from_two_points(pt3, pt4, &le), "line from two points should succeed");
    /* 垂直线 x=0 -> a=1, b=0, c=0 */
    TEST_ASSERT(fabs(le.a - 1.0) < 1e-12, "fabs should succeed");
    TEST_ASSERT(fabs(le.b) < 1e-12, "fabs should succeed");

    /* 重合点 -> 失败 */
    TEST_ASSERT(!line_from_two_points(pt1, pt1, &le), "line from two points should fail for invalid input");

    /* NULL 输入 */
    TEST_ASSERT(!line_from_two_points(NULL, pt2, &le), "line from two points should fail for invalid input");
    TEST_ASSERT(!line_from_two_points(pt1, NULL, &le), "line from two points should fail for invalid input");

    graph_destroy(g);
}

/* ================================================================== */
/*  17. solver_extract_equations_full 测试                               */
/* ================================================================== */

static void test_extract_equations_full_basic(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    /* 创建三个点形成三角形 */
    int p1 = add_rpoint(g, 0, 1, 0, 1);
    int p2 = add_rpoint(g, 3, 1, 0, 1);
    int p3 = add_rpoint(g, 0, 1, 4, 1);
    TEST_ASSERT(p1 >= 0 && p2 >= 0 && p3 >= 0, "p1 should be valid");

    /* 创建线段 */
    int s1 = graph_add_line_segment(g, p1, p2);
    int s2 = graph_add_line_segment(g, p2, p3);
    int s3 = graph_add_line_segment(g, p3, p1);

    /* 添加关联约束 */
    graph_add_incidence(g, p1, s1);
    graph_add_incidence(g, p2, s1);
    graph_add_incidence(g, p2, s2);
    graph_add_incidence(g, p3, s2);
    graph_add_incidence(g, p3, s3);
    graph_add_incidence(g, p1, s3);

    /* 提取方程 */
    EquationSystem *sys = equation_system_create();
    int count = solver_extract_equations_full(g, sys);
    TEST_ASSERT(count >= 0, "count should be valid");
    TEST_ASSERT_EQ(equation_system_count(sys), count);

    equation_system_destroy(sys);

    /* NULL 输入 */
    int null_ret = solver_extract_equations_full(NULL, NULL);
    TEST_ASSERT_EQ(null_ret, -1);

    null_ret = solver_extract_equations_full(g, NULL);
    TEST_ASSERT_EQ(null_ret, -1);

    graph_destroy(g);
}

static void test_extract_equations_full_connection(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    /* 创建两个点和线段，设置距离约束 */
    int p1 = add_rpoint(g, 0, 1, 0, 1);
    int p2 = add_rpoint(g, 3, 1, 4, 1);
    int seg = graph_add_line_segment(g, p1, p2);

    /* 为线段设置数值假设声明（距离） */
    GeomNode *seg_node = graph_get_node(g, seg);
    if (seg_node) {
        seg_node->numeric_assumption_declaration = strdup("distance=5.0");
    }

    EquationSystem *sys = equation_system_create();
    int count = solver_extract_equations_full(g, sys);
    TEST_ASSERT(count >= 0, "count should be valid");

    equation_system_destroy(sys);
    graph_destroy(g);
}

/* ================================================================== */
/*  18. eliminate_geometry 测试                                          */
/* ================================================================== */

static void test_eliminate_geometry_basic(void) {
    ConstraintGraph *g = graph_create();
    int p1 = add_rpoint(g, 0, 1, 0, 1);
    int p2 = add_rpoint(g, 3, 1, 0, 1);
    graph_add_line_segment(g, p1, p2);

    int elim_ids[] = {p2};
    SolverStatus st = eliminate_geometry(g, p1, elim_ids, 1);
    TEST_ASSERT(st == SOLVER_STATUS_OK, "st should equal SOLVER_STATUS_OK");

    /* NULL / 空输入 */
    st = eliminate_geometry(NULL, p1, elim_ids, 1);
    TEST_ASSERT(st == SOLVER_STATUS_OK, "st should equal SOLVER_STATUS_OK");

    st = eliminate_geometry(g, p1, NULL, 1);
    TEST_ASSERT(st == SOLVER_STATUS_OK, "st should equal SOLVER_STATUS_OK");

    st = eliminate_geometry(g, p1, elim_ids, 0);
    TEST_ASSERT(st == SOLVER_STATUS_OK, "st should equal SOLVER_STATUS_OK");

    graph_destroy(g);
}

/* ================================================================== */
/*  19. analyze_out_of_scope 测试                                        */
/* ================================================================== */

static void test_analyze_out_of_scope_basic(void) {
    ConstraintGraph *g = graph_create();
    int p1 = add_rpoint(g, 0, 1, 0, 1);
    int p2 = add_rpoint(g, 1, 1, 1, 1);
    graph_add_line_segment(g, p1, p2);

    char *suggestion = NULL;
    SolverStatus st = analyze_out_of_scope(g, p1, &suggestion);
    /* 对简单的图没有可因式分解的高次方程 */
    TEST_ASSERT(st == SOLVER_STATUS_OUT_OF_SCOPE, "st should equal SOLVER_STATUS_OUT_OF_SCOPE");
    if (suggestion) {
        lv_free((void **) &suggestion);
    }

    /* NULL 输入 */
    st = analyze_out_of_scope(NULL, p1, &suggestion);
    TEST_ASSERT(st == SOLVER_STATUS_OUT_OF_SCOPE, "st should equal SOLVER_STATUS_OUT_OF_SCOPE");
    if (suggestion) {
        lv_free((void **) &suggestion);
    }

    graph_destroy(g);
}

/* ================================================================== */
/*  20. GroebnerResult 生命周期测试                                      */
/* ================================================================== */

static void test_groebner_result_destroy(void) {
    /* 空结果 */
    GroebnerResult *r = (GroebnerResult *) lv_calloc(1, sizeof(GroebnerResult));
    TEST_ASSERT_NOT_NULL(r);
    r->solutions = NULL;
    r->solution_count = 0;
    groebner_result_destroy(r);

    /* NULL 安全 */
    groebner_result_destroy(NULL);

    /* 带解的结果 */
    r = (GroebnerResult *) lv_calloc(1, sizeof(GroebnerResult));
    TEST_ASSERT_NOT_NULL(r);
    r->solution_count = 2;
    r->solutions = (SymbolicCoord **) lv_malloc(2 * sizeof(SymbolicCoord *));
    r->solutions[0] = symbolic_coord_create_rational(1, 1);
    r->solutions[1] = symbolic_coord_create_rational(2, 1);
    groebner_result_destroy(r);
}

static void test_cleanup_groebner_result(void) {
    GroebnerResult r;
    memset(&r, 0, sizeof(r));
    r.solution_count = 2;
    r.solutions = (SymbolicCoord **) lv_malloc(2 * sizeof(SymbolicCoord *));
    r.solutions[0] = symbolic_coord_create_rational(10, 1);
    r.solutions[1] = symbolic_coord_create_rational(20, 1);

    cleanup_groebner_result(&r);
    TEST_ASSERT_NULL(r.solutions);
    TEST_ASSERT_EQ(r.solution_count, 0);

    /* NULL 安全 */
    cleanup_groebner_result(NULL);
}

/* ================================================================== */
/*  21. Groebner 基计算（通过公开 API）测试                               */
/* ================================================================== */

static void test_groebner_basis_compute(void) {
    /* 空系统 */
    EquationSystem *sys = equation_system_create();
    SolverStatus st = groebner_basis_compute(sys);
    TEST_ASSERT(st == SOLVER_STATUS_OK, "st should equal SOLVER_STATUS_OK");
    equation_system_destroy(sys);

    /* NULL 输入 */
    st = groebner_basis_compute(NULL);
    TEST_ASSERT(st == SOLVER_STATUS_OK, "st should equal SOLVER_STATUS_OK");
}

/* ================================================================== */
/*  22. compute_algebraic_resultant 测试                                 */
/* ================================================================== */

static void test_compute_algebraic_resultant(void) {
    mpz_poly_t p, q, result;

    /* p(x) = x - 2 -> 根为 2
       q(x) = x - 3 -> 根为 3
       alpha+beta = 5 的最小多项式应为 x - 5 */
    TEST_ASSERT_EQ(make_linear_poly(&p, 1, -2), 0); /* x - 2 */
    TEST_ASSERT_EQ(make_linear_poly(&q, 1, -3), 0); /* x - 3 */

    TEST_ASSERT(compute_algebraic_resultant(&p, &q, ALG_OP_SUM, &result), "compute algebraic resultant should succeed");
    if (result.degree >= 0) {
        mpz_poly_clear(&result);
    }
    mpz_poly_clear(&p);
    mpz_poly_clear(&q);

    /* NULL 输入的处理 */
    TEST_ASSERT_EQ(make_linear_poly(&p, 1, -2), 0);
    TEST_ASSERT(!compute_algebraic_resultant(NULL, &p, ALG_OP_SUM, &result), "compute algebraic resultant should fail for invalid input");
    mpz_poly_clear(&p);
}

/* ================================================================== */
/*  23. substitute_solved 测试（solver_symbolic.c）                      */
/* ================================================================== */

static void test_substitute_solved(void) {
    EquationSystem sys;
    equation_system_init(&sys);

    /* 添加方程: 2x - 4 = 0, var_node_id=1, coord_index=0 */
    mpz_poly_t poly;
    TEST_ASSERT_EQ(make_linear_poly(&poly, 2, -4), 0);
    equation_system_push(&sys, poly, 1, 0);
    mpz_poly_clear(&poly);

    /* 代入 x=2 */
    substitute_solved(&sys, 1, 0, 2.0);

    /* 代入后方程应变为 0=0（或清零） */
    const mpz_poly_t *p = equation_system_get_poly(&sys, 0);
    TEST_ASSERT_NOT_NULL(p);
    if (p->degree >= 0) {
        double val = mpz_get_d(p->coeffs[0]);
        TEST_ASSERT(fabs(val) < 1e-9, "fabs should succeed");
    }

    equation_system_clear(&sys);
}

/* ================================================================== */
/*  24. 综合测试：通过 extract_equations_from_constraints 间接测试推入逻辑 */
/* ================================================================== */

static void test_extract_from_constraints(void) {
    ConstraintGraph *g = graph_create();
    int p1 = add_rpoint(g, 0, 1, 0, 1);
    int p2 = add_rpoint(g, 1, 1, 0, 1);
    int seg = graph_add_line_segment(g, p1, p2);
    graph_add_incidence(g, p1, seg);
    graph_add_incidence(g, p2, seg);

    EquationSystem sys;
    equation_system_init(&sys);
    extract_equations_from_constraints(g, &sys);
    TEST_ASSERT(sys.count >= 0, "count should be valid");

    equation_system_clear(&sys);
    graph_destroy(g);
}

/* ================================================================== */
/*  25. QUADRATIC 类型坐标的 coord_to_double 测试（序列化回退路径）      */
/* ================================================================== */

static void test_coord_to_double_quadratic(void) {
    /* 创建 QUADRATIC 类型坐标: 1 + 0*sqrt(2) = 1 */
    Rational *qa = rational_create(1, 1);
    Rational *qb = rational_create(0, 1);
    SymbolicCoord *c = symbolic_coord_create_quadratic(qa, qb, 2);
    /* 所有权已转移给 c，不再单独释放 qa/qb */
    TEST_ASSERT_NOT_NULL(c);

    double val;
    TEST_ASSERT(coord_to_double(c, &val), "quadratic coord should be convertible to double");
    TEST_ASSERT(fabs(val - 1.0) < 1e-6, "quadratic coord value should be close to 1.0");

    symbolic_coord_destroy(c);
}

/* ================================================================== */
/*  26. count_point_variables 空图测试（out_ids 为 NULL 时）             */
/* ================================================================== */

static void test_count_point_variables_null_out(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    /* 空图 */
    int cnt = count_point_variables(g, NULL);
    TEST_ASSERT_EQ(cnt, 0);

    graph_destroy(g);
}

/* ================================================================== */
/*  main - 运行所有测试                                                  */
/* ================================================================== */

int main(void) {
    TEST_SUITE_BEGIN("Solver Submodules");

    /* --- EquationSystem 生命周期 --- */
    TEST_RUN(test_eq_system_create_destroy);
    TEST_RUN(test_eq_system_init_clear);
    TEST_RUN(test_eq_system_push);
    TEST_RUN(test_eq_system_push_many);
    TEST_RUN(test_eq_system_getters_edge_cases);

    /* --- 数值线性求解 --- */
    TEST_RUN(test_solve_linear_basic);
    TEST_RUN(test_solve_linear_edge_cases);

    /* --- 坐标转换 --- */
    TEST_RUN(test_coord_to_double);
    TEST_RUN(test_double_to_mpz_scaled);
    TEST_RUN(test_coord_to_mpz_scaled);
    TEST_RUN(test_coord_to_double_quadratic);

    /* --- 符号求解辅助函数 --- */
    TEST_RUN(test_is_out_of_scope);
    TEST_RUN(test_try_factor_polynomial);
    TEST_RUN(test_check_incompatible_distances);
    TEST_RUN(test_check_contradiction);
    TEST_RUN(test_constraint_weight);
    TEST_RUN(test_count_point_variables);
    TEST_RUN(test_count_point_variables_null_out);

    /* --- 精确二次/三次求解 --- */
    TEST_RUN(test_quadratic_exact_basic);
    TEST_RUN(test_quadratic_exact_overflow);
    TEST_RUN(test_cubic_exact_basic);

    /* --- 多遍求解 --- */
    TEST_RUN(test_solve_equations_pass);

    /* --- 多项式求值 --- */
    TEST_RUN(test_poly_eval_symbolic);

    /* --- 坐标/直线提取 --- */
    TEST_RUN(test_point_coord);
    TEST_RUN(test_line_from_two_points);

    /* --- 方程提取 --- */
    TEST_RUN(test_extract_equations_full_basic);
    TEST_RUN(test_extract_equations_full_connection);
    TEST_RUN(test_extract_from_constraints);

    /* --- 消元与超出范围分析 --- */
    TEST_RUN(test_eliminate_geometry_basic);
    TEST_RUN(test_analyze_out_of_scope_basic);

    /* --- 代数结式 --- */
    TEST_RUN(test_compute_algebraic_resultant);

    /* --- 代入 --- */
    TEST_RUN(test_substitute_solved);

    /* --- GroebnerResult 生命周期 --- */
    TEST_RUN(test_groebner_result_destroy);
    TEST_RUN(test_cleanup_groebner_result);

    /* --- Groebner 基计算（公开 API） --- */
    TEST_RUN(test_groebner_basis_compute);

    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}
