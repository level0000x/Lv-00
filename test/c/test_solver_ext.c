/**
 * @file test_solver_ext.c
 * @brief 求解器扩展契约测试（批次 C-㊹续：solver.h 零覆盖 API）
 *
 * 覆盖 10 个 ctest 零覆盖 API：
 *   - 方程系统族：equation_system_count / equation_system_destroy /
 *     equation_system_get_var_id / equation_system_get_coord_index
 *   - 提取族：solver_extract_equations_full
 *   - Groebner 族：groebner_basis_compute
 *   - 消元族：eliminate_geometry / analyze_out_of_scope
 *   - 反馈族：solver_feedback_destroy
 *   - 稀疏族：solver_sparse_solve（C-㊹续 补齐实现后）
 *   - 结式族：compute_algebraic_resultant（C-㊹续3，mpz_poly_t 构造）
 *
 * 登记遗留（GMP 专项）：solver_handle_multiple_solutions（需二次方程
 * 系统 + 分支笛卡尔积场景）。
 *
 * 契约要点（与实现核对）：
 *   - solver_extract_equations_full：NULL → -1；空图 → 0。
 *   - groebner_basis_compute：NULL/空系统 → SOLVER_STATUS_OK。
 *   - analyze_out_of_scope：NULL → OUT_OF_SCOPE；空图 → OUT_OF_SCOPE
 *     + suggestion 非 NULL（调用者 free）。
 *   - solver_sparse_solve 委托 solve_algebraic_system（NULL → TIMEOUT）。
 *   - compute_algebraic_resultant：NULL/空多项式 → false；degree > 4
 *     → false（MPZ_RES_INPUT_DEGREE_MAX）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* 构造含约束的图：2 点 + 线段 + INCIDENCE 约束 */
static ConstraintGraph *make_solver_graph(void) {
    ConstraintGraph *g = graph_create();
    if (!g)
        return NULL;
    int p0 = add_point(g, 0, 1, 0, 1);
    int p1 = add_point(g, 1, 1, 0, 1);
    if (graph_add_line_segment(g, p0, p1) != ADD_NODE_OK) {
        graph_destroy(g);
        return NULL;
    }
    int seg = graph_get_last_added_node_id(g);
    if (graph_add_incidence(g, p0, seg) < 0) {
        graph_destroy(g);
        return NULL;
    }
    return g;
}

/* ============== 测试：方程系统访问器 ============== */

static void test_equation_system_api(void) {
    /* create/count/destroy：NULL 契约 */
    EquationSystem *sys = equation_system_create();
    TEST_ASSERT_NOT_NULL(sys);
    TEST_ASSERT_EQ(equation_system_count(NULL), 0);
    TEST_ASSERT_EQ(equation_system_count(sys), 0);
    equation_system_destroy(NULL); /* 不崩溃即通过 */
    equation_system_destroy(sys);

    /* get_var_id / get_coord_index：NULL/越界 → -1 */
    sys = equation_system_create();
    TEST_ASSERT_NOT_NULL(sys);
    TEST_ASSERT_EQ(equation_system_get_var_id(NULL, 0), -1);
    TEST_ASSERT_EQ(equation_system_get_coord_index(NULL, 0), -1);
    TEST_ASSERT_EQ(equation_system_get_var_id(sys, 0), -1); /* 越界 */
    TEST_ASSERT_EQ(equation_system_get_coord_index(sys, 0), -1);

    /* 正路径：从图提取方程 → 访问器返回有效值 */
    ConstraintGraph *g = make_solver_graph();
    TEST_ASSERT_NOT_NULL(g);
    int n = solver_extract_equations_full(g, sys);
    TEST_ASSERT(n >= 1, "提取到方程");
    TEST_ASSERT_EQ(equation_system_count(sys), n);
    int vid = equation_system_get_var_id(sys, 0);
    int cidx = equation_system_get_coord_index(sys, 0);
    TEST_ASSERT(vid >= 0, "变量节点 id 有效");
    TEST_ASSERT(cidx >= 0, "坐标索引有效");

    graph_destroy(g);
    equation_system_destroy(sys);
    printf("  test_equation_system_api: PASSED\n");
}

/* ============== 测试：方程提取 ============== */

static void test_extract_equations_api(void) {
    /* NULL → -1 */
    TEST_ASSERT_EQ(solver_extract_equations_full(NULL, NULL), -1);

    /* 空图 → 0 */
    ConstraintGraph *empty = graph_create();
    TEST_ASSERT_NOT_NULL(empty);
    EquationSystem *sys = equation_system_create();
    TEST_ASSERT_NOT_NULL(sys);
    TEST_ASSERT_EQ(solver_extract_equations_full(empty, sys), 0);

    /* 含约束图 → > 0 */
    ConstraintGraph *g = make_solver_graph();
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT(solver_extract_equations_full(g, sys) >= 1, "提取方程数");

    graph_destroy(g);
    graph_destroy(empty);
    equation_system_destroy(sys);
    printf("  test_extract_equations_api: PASSED\n");
}

/* ============== 测试：Groebner 基计算 ============== */

static void test_groebner_api(void) {
    /* NULL / 空系统 → OK */
    TEST_ASSERT_EQ(groebner_basis_compute(NULL), SOLVER_STATUS_OK);
    EquationSystem *sys = equation_system_create();
    TEST_ASSERT_NOT_NULL(sys);
    TEST_ASSERT_EQ(groebner_basis_compute(sys), SOLVER_STATUS_OK);

    /* 含约束系统：状态为 OK / UNIQUE / OUT_OF_SCOPE 之一（不崩溃） */
    ConstraintGraph *g = make_solver_graph();
    TEST_ASSERT_NOT_NULL(g);
    solver_extract_equations_full(g, sys);
    SolverStatus st = groebner_basis_compute(sys);
    TEST_ASSERT(st == SOLVER_STATUS_OK || st == SOLVER_STATUS_UNIQUE || st == SOLVER_STATUS_OUT_OF_SCOPE,
                "Groebner 计算状态合法");

    graph_destroy(g);
    equation_system_destroy(sys);
    printf("  test_groebner_api: PASSED\n");
}

/* ============== 测试：几何消元 ============== */

static void test_eliminate_api(void) {
    /* NULL 契约 → OK（实现静默成功） */
    TEST_ASSERT_EQ(eliminate_geometry(NULL, 0, NULL, 0), SOLVER_STATUS_OK);
    ConstraintGraph *g = make_solver_graph();
    TEST_ASSERT_NOT_NULL(g);
    int elim[1] = {0};
    TEST_ASSERT_EQ(eliminate_geometry(g, 0, elim, 1), SOLVER_STATUS_OK);

    graph_destroy(g);
    printf("  test_eliminate_api: PASSED\n");
}

/* ============== 测试：超出范围分析 ============== */

static void test_out_of_scope_api(void) {
    /* NULL 契约 → OUT_OF_SCOPE */
    TEST_ASSERT_EQ(analyze_out_of_scope(NULL, 0, NULL), SOLVER_STATUS_OUT_OF_SCOPE);

    /* 空图：无高次方程 → OUT_OF_SCOPE + suggestion 非 NULL（调用者 free） */
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    char *suggestion = NULL;
    TEST_ASSERT_EQ(analyze_out_of_scope(g, 1, &suggestion), SOLVER_STATUS_OUT_OF_SCOPE);
    TEST_ASSERT_NOT_NULL(suggestion);
    TEST_ASSERT(strlen(suggestion) > 0, "建议非空");
    lv_free((void **)&suggestion);

    /* 含约束图：执行不崩溃 */
    ConstraintGraph *sg = make_solver_graph();
    TEST_ASSERT_NOT_NULL(sg);
    suggestion = NULL;
    TEST_ASSERT_EQ(analyze_out_of_scope(sg, 0, &suggestion), SOLVER_STATUS_OUT_OF_SCOPE);
    if (suggestion)
        lv_free((void **)&suggestion);

    graph_destroy(sg);
    graph_destroy(g);
    printf("  test_out_of_scope_api: PASSED\n");
}

/* ============== 测试：求解反馈 ============== */

static void test_feedback_api(void) {
    /* destroy：NULL 安全 */
    solver_feedback_destroy(NULL);

    /* create → destroy（message 生命周期由 destroy 统一释放） */
    SolverFeedback *fb = solver_feedback_create(SOLVER_FEEDBACK_TYPE_CONSTRAINT_ADDED, "constraint added");
    TEST_ASSERT_NOT_NULL(fb);
    TEST_ASSERT_EQ(fb->type, SOLVER_FEEDBACK_TYPE_CONSTRAINT_ADDED);
    TEST_ASSERT(fb->message && strcmp(fb->message, "constraint added") == 0, "消息复制");
    solver_feedback_destroy(fb);

    /* create(NULL message) → destroy */
    SolverFeedback *fb2 = solver_feedback_create(SOLVER_FEEDBACK_TYPE_VARIABLE_FREE, NULL);
    TEST_ASSERT_NOT_NULL(fb2);
    solver_feedback_destroy(fb2);

    printf("  test_feedback_api: PASSED\n");
}

/* ============== 测试：稀疏求解（C-㊹续 补齐） ============== */

static void test_sparse_solve_api(void) {
    /* NULL 契约（委托 solve_algebraic_system）→ TIMEOUT */
    TEST_ASSERT_EQ(solver_sparse_solve(NULL, NULL), SOLVER_STATUS_TIMEOUT);

    /* 正路径：含约束图 → 合法状态码 + out_result 可释放
     * （委托 solve_algebraic_system，可能返回 OK/UNIQUE/MULTIPLE/
     * NO_SOLUTION/OVERCONSTRAINED 等任一合法状态） */
    ConstraintGraph *g = make_solver_graph();
    TEST_ASSERT_NOT_NULL(g);
    GroebnerResult *result = NULL;
    SolverStatus st = solver_sparse_solve(g, &result);
    TEST_ASSERT(st >= SOLVER_STATUS_OK && st <= SOLVER_STATUS_OUT_OF_MEMORY, "稀疏求解状态合法");
    if (result) {
        TEST_ASSERT(result->solution_count >= 0, "解计数非负");
        groebner_result_destroy(result);
    }

    graph_destroy(g);
    printf("  test_sparse_solve_api: PASSED\n");
}

/* 构造 mpz_poly_t（coeffs[i] = 第 i 次幂系数）；deg < 0 返回空多项式 */
static mpz_poly_t make_mpz_poly(int deg, const int *coeffs) {
    mpz_poly_t p;
    mpz_poly_init(&p);
    if (deg < 0)
        return p;
    p.degree = deg;
    p.coeffs = lv_malloc(((size_t) deg + 1) * sizeof(mpz_t));
    for (int i = 0; i <= deg; i++)
        mpz_init_set_si(p.coeffs[i], coeffs[i]);
    return p;
}

/* ============== 测试：代数结式（compute_algebraic_resultant） ============== */

static void test_resultant_api(void) {
    /* NULL 契约 → false */
    TEST_ASSERT(!compute_algebraic_resultant(NULL, NULL, ALG_OP_SUM, NULL), "全 NULL");
    TEST_ASSERT(!compute_algebraic_resultant(NULL, NULL, ALG_OP_PRODUCT, NULL), "全 NULL product");

    mpz_poly_t result;
    mpz_poly_init(&result);

    /* 空多项式（degree=-1）→ false */
    mpz_poly_t empty_p;
    mpz_poly_init(&empty_p);
    int lin1_c[2] = {1, 1};  /* x + 1 */
    mpz_poly_t p = make_mpz_poly(1, lin1_c);
    TEST_ASSERT(!compute_algebraic_resultant(&empty_p, &p, ALG_OP_SUM, &result), "空多项式");

    /* 线性多项式 SUM 结式：x+1 与 x-1 → true + 结果非空 */
    int lin2_c[2] = {-1, 1}; /* x - 1 */
    mpz_poly_t q = make_mpz_poly(1, lin2_c);
    TEST_ASSERT(compute_algebraic_resultant(&p, &q, ALG_OP_SUM, &result), "线性 SUM 结式");
    TEST_ASSERT(result.degree >= 0, "结式结果非空");
    TEST_ASSERT_NOT_NULL(result.coeffs);
    /* 直接读系数验证（mpz_poly_get_str 对非负 degree 多项式返回 NULL 为
     * 独立观察缺陷，另行登记，本测试不依赖序列化路径） */
    char *s = mpz_get_str(NULL, 10, result.coeffs[0]);
    TEST_ASSERT_NOT_NULL(s);
    lv_free_external((void **)&s);

    /* PRODUCT 结式同样可计算 */
    mpz_poly_clear(&result);
    mpz_poly_init(&result);
    TEST_ASSERT(compute_algebraic_resultant(&p, &q, ALG_OP_PRODUCT, &result), "线性 PRODUCT 结式");

    /* 高次（degree 5 > MAX=4）→ false */
    int deg5_c[6] = {1, 2, 3, 4, 5, 6};
    mpz_poly_t d5 = make_mpz_poly(5, deg5_c);
    mpz_poly_clear(&result);
    mpz_poly_init(&result);
    TEST_ASSERT(!compute_algebraic_resultant(&d5, &p, ALG_OP_SUM, &result), "高次越界失败");

    mpz_poly_clear(&result);
    mpz_poly_clear(&d5);
    mpz_poly_clear(&q);
    mpz_poly_clear(&p);
    mpz_poly_clear(&empty_p);
    printf("  test_resultant_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Solver Ext Test Suite")
    printf("=== Lv-00 Solver Ext Test Suite (batch C-㊹续) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_equation_system_api);
    TEST_MAIN_RUN(test_extract_equations_api);
    TEST_MAIN_RUN(test_groebner_api);
    TEST_MAIN_RUN(test_eliminate_api);
    TEST_MAIN_RUN(test_out_of_scope_api);
    TEST_MAIN_RUN(test_feedback_api);
    TEST_MAIN_RUN(test_sparse_solve_api);
    TEST_MAIN_RUN(test_resultant_api);

    lv_cleanup();
TEST_MAIN_END()
