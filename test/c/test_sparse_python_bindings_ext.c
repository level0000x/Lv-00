/**
 * @file test_sparse_python_bindings_ext.c
 * @brief Sparse linear algebra Python-binding compat layer contract tests
 *
 * Covers the Python FFI compat symbols appended to sparse_linear_algebra.c
 * (referenced by module/python/lv/sparse_la.py but never implemented in C):
 * - sparse_matrix_create/destroy/clone/print/get_dims
 * - sparse_matrix_multiply / transpose
 * - sparse_lu_solve / sparse_cholesky_solve / sparse_qr_solve
 * - graph_to_constraint_matrix / graph_degree_analysis / degree_analysis_free
 * - semiring_propagate_constraints
 *
 * @author Lv-00 Project
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/sparse_linear_algebra.h"
#include "lv/constraint_graph.h"
#include "lv/lv.h"

#include "test_unified.h"

void *sparse_matrix_create(int rows, int cols, int fmt);
void sparse_matrix_destroy(void *m);
void *sparse_matrix_clone(void *m);
void sparse_matrix_print(void *m, const char *name);
int sparse_matrix_get_dims(void *m, int *rows, int *cols);
bool sparse_matrix_multiply(void *a, void *b, void **out);
bool sparse_matrix_transpose(void *m, void **out);
bool sparse_lu_solve(void *m, const double *b, double *x);
bool sparse_cholesky_solve(void *m, const double *b, double *x);
bool sparse_qr_solve(void *m, const double *b, double *x);
bool graph_to_constraint_matrix(const ConstraintGraph *graph, void **out);
bool graph_degree_analysis(const ConstraintGraph *graph, void **out);
void degree_analysis_free(void *p);
int semiring_propagate_constraints(const ConstraintGraph *graph, int semiring, double *x, int max_iter);

typedef struct lvSparseDegreeAnalysis {
    int node_count;
    int max_degree;
    int min_degree;
    double avg_degree;
    int isolated_count;
} lvSparseDegreeAnalysis;

int g_pass_count = 0;
int g_fail_count = 0;

static void test_create_destroy_clone(void) {
    void *m = sparse_matrix_create(3, 4, 0);
    TEST_ASSERT_NOT_NULL(m);

    int rows = 0, cols = 0;
    TEST_ASSERT_EQ(sparse_matrix_get_dims(m, &rows, &cols), 0);
    TEST_ASSERT_EQ(rows, 3);
    TEST_ASSERT_EQ(cols, 4);

    void *c = sparse_matrix_clone(m);
    TEST_ASSERT_NOT_NULL(c);
    int cr = 0, cc = 0;
    sparse_matrix_get_dims(c, &cr, &cc);
    TEST_ASSERT_EQ(cr, 3);
    TEST_ASSERT_EQ(cc, 4);

    sparse_matrix_destroy(c);
    sparse_matrix_destroy(m);
    sparse_matrix_destroy(NULL);
}

static void test_multiply_transpose(void) {
    void *A = sparse_matrix_create(2, 2, 0);
    void *B = sparse_matrix_create(2, 1, 0);
    TEST_ASSERT_NOT_NULL(A);
    TEST_ASSERT_NOT_NULL(B);

    lvSparseMatrix *mA = (lvSparseMatrix *) A;
    lvSparseMatrix *mB = (lvSparseMatrix *) B;
    lv_sparse_set(mA, 0, 0, 1.0);
    lv_sparse_set(mA, 0, 1, 2.0);
    lv_sparse_set(mA, 1, 0, 3.0);
    lv_sparse_set(mA, 1, 1, 4.0);
    lv_sparse_set(mB, 0, 0, 5.0);
    lv_sparse_set(mB, 1, 0, 6.0);

    void *C = NULL;
    TEST_ASSERT_MSG(sparse_matrix_multiply(A, B, &C) == true, "multiply ok");
    TEST_ASSERT_NOT_NULL(C);
    lvSparseMatrix *mC = (lvSparseMatrix *) C;
    TEST_ASSERT_DOUBLE(lv_sparse_get(mC, 0, 0), 17.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(mC, 1, 0), 39.0, 1e-12);
    sparse_matrix_destroy(C);

    void *C2 = (void *) 1;
    TEST_ASSERT_MSG(sparse_matrix_multiply(B, A, &C2) == false, "dim mismatch false");
    TEST_ASSERT_NULL(C2);

    void *T = NULL;
    TEST_ASSERT_MSG(sparse_matrix_transpose(A, &T) == true, "transpose ok");
    TEST_ASSERT_NOT_NULL(T);
    lvSparseMatrix *mT = (lvSparseMatrix *) T;
    int tr = 0, tc = 0;
    sparse_matrix_get_dims(T, &tr, &tc);
    TEST_ASSERT_EQ(tr, 2);
    TEST_ASSERT_EQ(tc, 2);
    TEST_ASSERT_DOUBLE(lv_sparse_get(mT, 1, 0), 2.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(mT, 0, 1), 3.0, 1e-12);
    sparse_matrix_destroy(T);

    sparse_matrix_destroy(B);
    sparse_matrix_destroy(A);
}

static void test_solvers(void) {
    void *A = sparse_matrix_create(2, 2, 0);
    TEST_ASSERT_NOT_NULL(A);
    lvSparseMatrix *mA = (lvSparseMatrix *) A;
    lv_sparse_set(mA, 0, 0, 4.0);
    lv_sparse_set(mA, 0, 1, 1.0);
    lv_sparse_set(mA, 1, 0, 1.0);
    lv_sparse_set(mA, 1, 1, 3.0);
    double b[2] = {5.0, 4.0};
    double x[2] = {0.0, 0.0};

    TEST_ASSERT_MSG(sparse_lu_solve(A, b, x) == true, "LU solve ok");
    TEST_ASSERT_DOUBLE(x[0], 1.0, 1e-9);
    TEST_ASSERT_DOUBLE(x[1], 1.0, 1e-9);

    x[0] = 0.0;
    x[1] = 0.0;
    TEST_ASSERT_MSG(sparse_cholesky_solve(A, b, x) == true, "Cholesky solve ok");
    TEST_ASSERT_DOUBLE(x[0], 1.0, 1e-9);
    TEST_ASSERT_DOUBLE(x[1], 1.0, 1e-9);

    x[0] = 0.0;
    x[1] = 0.0;
    TEST_ASSERT_MSG(sparse_qr_solve(A, b, x) == true, "QR solve ok");
    TEST_ASSERT_DOUBLE(x[0], 1.0, 1e-9);
    TEST_ASSERT_DOUBLE(x[1], 1.0, 1e-9);

    sparse_matrix_destroy(A);
}

/* 真正稀疏场景：4×4 三对角系统（真实稀疏矩阵，非稠密），
 * 验证稀疏 LU/Cholesky/QR 在稀疏消元路径上的正确性。
 *   A = [ 2 -1  0  0 ; -1  2 -1  0 ; 0 -1  2 -1 ; 0  0 -1  2 ]
 *   b = [ 1 ; 2 ; 3 ; 4 ]
 *   解：A 对称正定三对角，x 解析值可预计算。 */
static void test_sparse_tridiagonal_solvers(void) {
    void *A = sparse_matrix_create(4, 4, 0);
    TEST_ASSERT_NOT_NULL(A);
    lvSparseMatrix *mA = (lvSparseMatrix *) A;
    lv_sparse_set(mA, 0, 0, 2.0);
    lv_sparse_set(mA, 0, 1, -1.0);
    lv_sparse_set(mA, 1, 0, -1.0);
    lv_sparse_set(mA, 1, 1, 2.0);
    lv_sparse_set(mA, 1, 2, -1.0);
    lv_sparse_set(mA, 2, 1, -1.0);
    lv_sparse_set(mA, 2, 2, 2.0);
    lv_sparse_set(mA, 2, 3, -1.0);
    lv_sparse_set(mA, 3, 2, -1.0);
    lv_sparse_set(mA, 3, 3, 2.0);

    double b[4] = {1.0, 2.0, 3.0, 4.0};
    /* 解析解（代入验证）：
     * 2x0-x1=1 → x0=(1+x1)/2
     * -x0+2x1-x2=2
     * -x1+2x2-x3=3
     * -x2+2x3=4  → x2=2x3-4
     * 回代得 x = [4 ; 7 ; 8 ; 6]（全部满足原方程） */
    const double expect[4] = {4.0, 7.0, 8.0, 6.0};
    double x[4];

    memset(x, 0, sizeof(x));
    TEST_ASSERT_MSG(sparse_lu_solve(A, b, x) == true, "tridiag LU ok");
    for (int i = 0; i < 4; i++)
        TEST_ASSERT_DOUBLE(x[i], expect[i], 1e-6);

    memset(x, 0, sizeof(x));
    TEST_ASSERT_MSG(sparse_cholesky_solve(A, b, x) == true, "tridiag Cholesky ok");
    for (int i = 0; i < 4; i++)
        TEST_ASSERT_DOUBLE(x[i], expect[i], 1e-6);

    memset(x, 0, sizeof(x));
    TEST_ASSERT_MSG(sparse_qr_solve(A, b, x) == true, "tridiag QR ok");
    for (int i = 0; i < 4; i++)
        TEST_ASSERT_DOUBLE(x[i], expect[i], 1e-6);

    sparse_matrix_destroy(A);
}

static void test_graph_to_matrix(void) {
    lv_init();
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    AddNodeResult r0 = graph_add_point_xy(g, NULL, NULL);
    AddNodeResult r1 = graph_add_point_xy(g, NULL, NULL);
    AddNodeResult r2 = graph_add_point_xy(g, NULL, NULL);
    TEST_ASSERT_MSG(r0 == ADD_NODE_OK && r1 == ADD_NODE_OK && r2 == ADD_NODE_OK, "points ok");
    /* explicit constraints linking nodes 0-1 and 1-2 */
    int p01[2] = {0, 1};
    int p12[2] = {1, 2};
    TEST_ASSERT_NOT_NULL(graph_add_constraint_with_id(g, 1, INCIDENCE, p01, 2));
    TEST_ASSERT_NOT_NULL(graph_add_constraint_with_id(g, 2, INCIDENCE, p12, 2));

    void *out = NULL;
    TEST_ASSERT_MSG(graph_to_constraint_matrix(g, &out) == true, "g2m ok");
    TEST_ASSERT_NOT_NULL(out);
    lvSparseMatrix *m = (lvSparseMatrix *) out;
    int rows = 0, cols = 0;
    sparse_matrix_get_dims(out, &rows, &cols);
    TEST_ASSERT_EQ(rows, 3);
    TEST_ASSERT_EQ(cols, 3);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 0, 1), 1.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 1, 0), 1.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 1, 2), 1.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 2, 1), 1.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 0, 2), 0.0, 1e-12);

    sparse_matrix_destroy(out);
    graph_destroy(g);
    lv_cleanup();
}

static void test_degree_analysis(void) {
    lv_init();
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    graph_add_point_xy(g, NULL, NULL);
    graph_add_point_xy(g, NULL, NULL);
    graph_add_point_xy(g, NULL, NULL);
    graph_add_point_xy(g, NULL, NULL);
    /* explicit constraints: 0-1, 1-2; node 3 isolated */
    int q01[2] = {0, 1};
    int q12[2] = {1, 2};
    TEST_ASSERT_NOT_NULL(graph_add_constraint_with_id(g, 1, INCIDENCE, q01, 2));
    TEST_ASSERT_NOT_NULL(graph_add_constraint_with_id(g, 2, INCIDENCE, q12, 2));

    void *out = NULL;
    TEST_ASSERT_MSG(graph_degree_analysis(g, &out) == true, "gda ok");
    TEST_ASSERT_NOT_NULL(out);
    lvSparseDegreeAnalysis *a = (lvSparseDegreeAnalysis *) out;
    TEST_ASSERT_EQ(a->node_count, 4);
    TEST_ASSERT_EQ(a->max_degree, 2); /* node 1 joins 2 constraints */
    TEST_ASSERT_EQ(a->min_degree, 0);
    TEST_ASSERT_EQ(a->isolated_count, 1);
    TEST_ASSERT_DOUBLE(a->avg_degree, 1.0, 1e-12); /* (1+2+1+0)/4 */

    degree_analysis_free(out);
    graph_destroy(g);
    lv_cleanup();
}

static void test_semiring(void) {
    lv_init();
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    graph_add_point_xy(g, NULL, NULL);
    graph_add_point_xy(g, NULL, NULL);
    graph_add_point_xy(g, NULL, NULL);
    /* explicit constraint chain 0-1-2 */
    int r01[2] = {0, 1};
    int r12[2] = {1, 2};
    TEST_ASSERT_NOT_NULL(graph_add_constraint_with_id(g, 1, INCIDENCE, r01, 2));
    TEST_ASSERT_NOT_NULL(graph_add_constraint_with_id(g, 2, INCIDENCE, r12, 2));

    /* seed node 0 = 1, rest 0 -> OR_AND propagates to all */
    double x[3] = {1.0, 0.0, 0.0};
    int iters = semiring_propagate_constraints(g, 3, x, 100);
    TEST_ASSERT_MSG(iters > 0, "OR_AND converge");
    TEST_ASSERT_DOUBLE(x[0], 1.0, 1e-12);
    TEST_ASSERT_DOUBLE(x[1], 1.0, 1e-12);
    TEST_ASSERT_DOUBLE(x[2], 1.0, 1e-12);

    TEST_ASSERT_EQ(semiring_propagate_constraints(NULL, 3, x, 100), -1);

    graph_destroy(g);
    lv_cleanup();
}

TEST_MAIN_BEGIN("Sparse Python Bindings Ext Test Suite")
    printf("=== Sparse Python Bindings Ext Test Suite (Python FFI compat) ===\n\n");
    TEST_MAIN_RUN(test_create_destroy_clone);
    TEST_MAIN_RUN(test_multiply_transpose);
    TEST_MAIN_RUN(test_solvers);
    TEST_MAIN_RUN(test_sparse_tridiagonal_solvers);
    TEST_MAIN_RUN(test_graph_to_matrix);
    TEST_MAIN_RUN(test_degree_analysis);
    TEST_MAIN_RUN(test_semiring);
TEST_MAIN_END()
