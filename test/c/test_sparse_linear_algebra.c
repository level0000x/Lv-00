/**
 * @file test_sparse_linear_algebra.c
 * @brief Test suite for the sparse linear algebra module
 *
 * Covers the CSR sparse matrix implementation (sparse_linear_algebra.h):
 * - lv_sparse_create / set / get (including growth beyond initial capacity
 *   and out-of-bounds error paths)
 * - lv_sparse_solve Jacobi convergence (strictly diagonally dominant system)
 *   and error codes (-2 non-square, -3 zero diagonal)
 * - lv_sparse_matvec / zero / copy / scale (backend support ops)
 * - lv_matrix_create(..., sparse=true) backend integration:
 *   set/get round-trip and matvec consistency with the dense path
 *
 * @version 1.0.0
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sparse_linear_algebra.h"
#include "numerical_backend.h"
#include "test_helpers.h"

/* ============================================================
 * Global test counters
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Test: create / destroy lifecycle
 * ============================================================ */

static void test_sparse_create_basic(void) {
    lvSparseMatrix *m = lv_sparse_create(3, 4);
    TEST_ASSERT_NOT_NULL(m);
    /* 新矩阵所有元素为 0 */
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 0, 0), 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 2, 3), 0.0, 1e-12);
    lv_sparse_destroy(m);
    lv_sparse_destroy(NULL); /* NULL 安全 */
}

static void test_sparse_create_invalid(void) {
    TEST_ASSERT_NULL(lv_sparse_create(0, 3));
    TEST_ASSERT_NULL(lv_sparse_create(3, 0));
    TEST_ASSERT_NULL(lv_sparse_create(-1, 3));
    TEST_ASSERT_NULL(lv_sparse_create(3, -2));
}

/* ============================================================
 * Test: set / get
 * ============================================================ */

static void test_sparse_set_get_basic(void) {
    lvSparseMatrix *m = lv_sparse_create(3, 3);
    TEST_ASSERT_NOT_NULL(m);

    TEST_ASSERT_EQ(lv_sparse_set(m, 0, 0, 4.0), 0);
    TEST_ASSERT_EQ(lv_sparse_set(m, 1, 0, 1.0), 0);
    TEST_ASSERT_EQ(lv_sparse_set(m, 1, 1, 3.0), 0);
    TEST_ASSERT_EQ(lv_sparse_set(m, 1, 2, 1.0), 0);
    TEST_ASSERT_EQ(lv_sparse_set(m, 2, 2, 4.0), 0);

    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 0, 0), 4.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 1, 0), 1.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 1, 1), 3.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 1, 2), 1.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 2, 2), 4.0, 1e-12);

    /* 未存储元素返回 0 */
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 0, 1), 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 2, 0), 0.0, 1e-12);

    /* 写入 0.0 时不存储（静默成功） */
    TEST_ASSERT_EQ(lv_sparse_set(m, 0, 0, 0.0), 0);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 0, 0), 0.0, 1e-12);

    /* 覆盖已存储元素 */
    TEST_ASSERT_EQ(lv_sparse_set(m, 1, 1, 5.0), 0);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 1, 1), 5.0, 1e-12);

    lv_sparse_destroy(m);
}

static void test_sparse_set_get_out_of_bounds(void) {
    lvSparseMatrix *m = lv_sparse_create(3, 4);
    TEST_ASSERT_NOT_NULL(m);

    /* set 越界返回 -1 */
    TEST_ASSERT_EQ(lv_sparse_set(m, -1, 0, 1.0), -1);
    TEST_ASSERT_EQ(lv_sparse_set(m, 3, 0, 1.0), -1);
    TEST_ASSERT_EQ(lv_sparse_set(m, 0, -1, 1.0), -1);
    TEST_ASSERT_EQ(lv_sparse_set(m, 0, 4, 1.0), -1);
    TEST_ASSERT_EQ(lv_sparse_set(m, 5, 5, 1.0), -1);
    TEST_ASSERT_EQ(lv_sparse_set(NULL, 0, 0, 1.0), -1);

    /* get 越界返回 0.0（不崩溃） */
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, -1, 0), 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 3, 0), 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 0, 4), 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(NULL, 0, 0), 0.0, 1e-12);

    lv_sparse_destroy(m);
}

/* ============================================================
 * Test: capacity growth (initial cap = 128, fill 400 elements)
 * ============================================================ */

static void test_sparse_grow(void) {
    const int n = 20;
    lvSparseMatrix *m = lv_sparse_create(n, n);
    TEST_ASSERT_NOT_NULL(m);

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            TEST_ASSERT_EQ(lv_sparse_set(m, i, j, (double) (i * n + j + 1)), 0);
        }
    }

    /* 超过初始容量 128 后仍能正确回读 */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double expect = (double) (i * n + j + 1);
            TEST_ASSERT_DOUBLE(lv_sparse_get(m, i, j), expect, 1e-12);
        }
    }

    lv_sparse_destroy(m);
}

/* ============================================================
 * Test: matvec / zero / copy / scale (backend support ops)
 * ============================================================ */

static void test_sparse_matvec(void) {
    /* A = [2 0 1; 0 3 0; 1 0 4], x = (1, 2, 3) => y = (5, 6, 13) */
    lvSparseMatrix *m = lv_sparse_create(3, 3);
    TEST_ASSERT_NOT_NULL(m);
    lv_sparse_set(m, 0, 0, 2.0);
    lv_sparse_set(m, 0, 2, 1.0);
    lv_sparse_set(m, 1, 1, 3.0);
    lv_sparse_set(m, 2, 0, 1.0);
    lv_sparse_set(m, 2, 2, 4.0);

    double x[3] = {1.0, 2.0, 3.0};
    double y[3] = {0.0, 0.0, 0.0};

    TEST_ASSERT_EQ(lv_sparse_matvec(m, x, y), 0);
    TEST_ASSERT_DOUBLE(y[0], 5.0, 1e-12);
    TEST_ASSERT_DOUBLE(y[1], 6.0, 1e-12);
    TEST_ASSERT_DOUBLE(y[2], 13.0, 1e-12);

    TEST_ASSERT_EQ(lv_sparse_matvec(NULL, x, y), -1);
    TEST_ASSERT_EQ(lv_sparse_matvec(m, NULL, y), -1);
    TEST_ASSERT_EQ(lv_sparse_matvec(m, x, NULL), -1);

    lv_sparse_destroy(m);
}

static void test_sparse_zero(void) {
    lvSparseMatrix *m = lv_sparse_create(3, 3);
    TEST_ASSERT_NOT_NULL(m);
    lv_sparse_set(m, 0, 0, 1.0);
    lv_sparse_set(m, 1, 1, 2.0);
    lv_sparse_set(m, 2, 2, 3.0);

    lv_sparse_zero(m);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 0, 0), 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 1, 1), 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 2, 2), 0.0, 1e-12);

    /* 清零后可继续写入 */
    lv_sparse_set(m, 0, 0, 7.0);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 0, 0), 7.0, 1e-12);

    lv_sparse_zero(NULL); /* NULL 安全 */
    lv_sparse_destroy(m);
}

static void test_sparse_copy(void) {
    lvSparseMatrix *src = lv_sparse_create(4, 4);
    lvSparseMatrix *dst = lv_sparse_create(4, 4);
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_NOT_NULL(dst);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if ((i + j) % 2 == 0)
                lv_sparse_set(src, i, j, (double) (i * 4 + j + 1));
        }
    }

    TEST_ASSERT_EQ(lv_sparse_copy(dst, src), 0);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            TEST_ASSERT_DOUBLE(lv_sparse_get(dst, i, j), lv_sparse_get(src, i, j), 1e-12);
        }
    }

    /* 维度不一致返回 -1 */
    lvSparseMatrix *wrong = lv_sparse_create(3, 4);
    TEST_ASSERT_NOT_NULL(wrong);
    TEST_ASSERT_EQ(lv_sparse_copy(wrong, src), -1);
    TEST_ASSERT_EQ(lv_sparse_copy(NULL, src), -1);
    TEST_ASSERT_EQ(lv_sparse_copy(dst, NULL), -1);
    lv_sparse_destroy(wrong);

    lv_sparse_destroy(src);
    lv_sparse_destroy(dst);
}

static void test_sparse_scale(void) {
    lvSparseMatrix *m = lv_sparse_create(3, 3);
    TEST_ASSERT_NOT_NULL(m);
    lv_sparse_set(m, 0, 0, 1.0);
    lv_sparse_set(m, 1, 1, 2.0);
    lv_sparse_set(m, 2, 2, 3.0);

    TEST_ASSERT_EQ(lv_sparse_scale(m, 2.0), 0);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 0, 0), 2.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 1, 1), 4.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 2, 2), 6.0, 1e-12);

    /* c == 0 等效于清零 */
    TEST_ASSERT_EQ(lv_sparse_scale(m, 0.0), 0);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 0, 0), 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_sparse_get(m, 1, 1), 0.0, 1e-12);

    TEST_ASSERT_EQ(lv_sparse_scale(NULL, 2.0), -1);
    lv_sparse_destroy(m);
}

/* ============================================================
 * Test: lv_sparse_solve (Jacobi iteration)
 * ============================================================ */

static void test_sparse_solve_converges(void) {
    /* 严格对角占优 3x3 系统：
     * A = [4 1 0; 1 3 1; 0 1 4]，已知解 x = (1, 2, 3)，b = (6, 10, 14) */
    lvSparseMatrix *m = lv_sparse_create(3, 3);
    TEST_ASSERT_NOT_NULL(m);
    lv_sparse_set(m, 0, 0, 4.0);
    lv_sparse_set(m, 0, 1, 1.0);
    lv_sparse_set(m, 1, 0, 1.0);
    lv_sparse_set(m, 1, 1, 3.0);
    lv_sparse_set(m, 1, 2, 1.0);
    lv_sparse_set(m, 2, 1, 1.0);
    lv_sparse_set(m, 2, 2, 4.0);

    double b[3] = {6.0, 10.0, 14.0};
    double x[3] = {0.0, 0.0, 0.0};

    int iter = lv_sparse_solve(m, b, x);
    TEST_ASSERT(iter > 0, "Jacobi 应返回正迭代次数");
    TEST_ASSERT(iter <= 100, "迭代次数不应超过上限 100");

    TEST_ASSERT_DOUBLE(x[0], 1.0, 1e-4);
    TEST_ASSERT_DOUBLE(x[1], 2.0, 1e-4);
    TEST_ASSERT_DOUBLE(x[2], 3.0, 1e-4);

    lv_sparse_destroy(m);
}

static void test_sparse_solve_non_square(void) {
    lvSparseMatrix *m = lv_sparse_create(2, 3); /* 非方阵 */
    TEST_ASSERT_NOT_NULL(m);
    lv_sparse_set(m, 0, 0, 2.0);
    lv_sparse_set(m, 1, 1, 2.0);

    double b[2] = {1.0, 1.0};
    double x[2] = {0.0, 0.0};
    TEST_ASSERT_EQ(lv_sparse_solve(m, b, x), -2); /* 非方阵错误码 */

    lv_sparse_destroy(m);
}

static void test_sparse_solve_zero_diagonal(void) {
    /* A = [0 1; 1 1]：对角线含零元素 */
    /* 回归钉：-3 错误路径不得泄漏内部迭代缓冲。
     * 曾因 lv_free(x_next) 传值而非取址（lv_free 契约是 void**），
     * 行为为 UB：按堆内容不同表现为崩溃或静默泄漏。 */
    MemoryStats mem_before;
    lv_get_memory_stats(&mem_before);

    lvSparseMatrix *m = lv_sparse_create(2, 2);
    TEST_ASSERT_NOT_NULL(m);
    lv_sparse_set(m, 0, 0, 0.0); /* 0.0 不存储，对角线缺失 */
    lv_sparse_set(m, 0, 1, 1.0);
    lv_sparse_set(m, 1, 0, 1.0);
    lv_sparse_set(m, 1, 1, 1.0);

    double b[2] = {1.0, 2.0};
    double x[2] = {0.0, 0.0};

    TEST_ASSERT_EQ(lv_sparse_solve(m, b, x), -3); /* 零对角线错误码 */
    lv_sparse_destroy(m);
    MemoryStats mem_after;
    lv_get_memory_stats(&mem_after);
    TEST_ASSERT(mem_after.current_used == mem_before.current_used,
                "零对角线错误路径不得泄漏内存");
}

static void test_sparse_solve_null_args(void) {
    lvSparseMatrix *m = lv_sparse_create(2, 2);
    TEST_ASSERT_NOT_NULL(m);
    double b[2] = {1.0, 1.0};
    double x[2] = {0.0, 0.0};
    TEST_ASSERT_EQ(lv_sparse_solve(NULL, b, x), -1);
    TEST_ASSERT_EQ(lv_sparse_solve(m, NULL, x), -1);
    TEST_ASSERT_EQ(lv_sparse_solve(m, b, NULL), -1);
    lv_sparse_destroy(m);
}

/* ============================================================
 * Test: lv_matrix_create(..., sparse=true) 后端集成
 * ============================================================ */

static void test_backend_sparse_set_get(void) {
    lvMatrix *A = lv_matrix_create(lv_BACKEND_SERIAL, 3, 3, true);
    TEST_ASSERT_NOT_NULL(A);
    TEST_ASSERT_EQ(A->sparse, true);
    TEST_ASSERT_EQ(A->format, lv_MATRIX_SPARSE_CSR);

    A->ops->set_element(A, 0, 0, 4.0);
    A->ops->set_element(A, 1, 1, 3.0);
    A->ops->set_element(A, 2, 2, 4.0);

    TEST_ASSERT_DOUBLE(A->ops->get_element(A, 0, 0), 4.0, 1e-12);
    TEST_ASSERT_DOUBLE(A->ops->get_element(A, 1, 1), 3.0, 1e-12);
    TEST_ASSERT_DOUBLE(A->ops->get_element(A, 2, 2), 4.0, 1e-12);
    /* 未设置元素返回 0 */
    TEST_ASSERT_DOUBLE(A->ops->get_element(A, 0, 1), 0.0, 1e-12);
    /* 越界静默忽略（与稠密 default_matrix_set_element 一致） */
    A->ops->set_element(A, 5, 5, 9.0);
    TEST_ASSERT_DOUBLE(A->ops->get_element(A, 5, 5), 0.0, 1e-12);

    A->ops->destroy(A);
}

static void test_backend_sparse_matvec_dense_consistency(void) {
    const double vals[3][3] = {{4.0, 1.0, 0.0}, {1.0, 3.0, 1.0}, {0.0, 1.0, 4.0}};

    lvMatrix *A_sparse = lv_matrix_create(lv_BACKEND_SERIAL, 3, 3, true);
    lvMatrix *A_dense = lv_matrix_create(lv_BACKEND_SERIAL, 3, 3, false);
    lvVector *x = lv_vector_create(lv_BACKEND_SERIAL, 3);
    lvVector *y_sparse = lv_vector_create(lv_BACKEND_SERIAL, 3);
    lvVector *y_dense = lv_vector_create(lv_BACKEND_SERIAL, 3);
    TEST_ASSERT_NOT_NULL(A_sparse);
    TEST_ASSERT_NOT_NULL(A_dense);
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_NOT_NULL(y_sparse);
    TEST_ASSERT_NOT_NULL(y_dense);

    for (int64_t i = 0; i < 3; i++) {
        for (int64_t j = 0; j < 3; j++) {
            A_sparse->ops->set_element(A_sparse, i, j, vals[i][j]);
            A_dense->ops->set_element(A_dense, i, j, vals[i][j]);
        }
    }
    x->data[0] = 1.0;
    x->data[1] = 2.0;
    x->data[2] = 3.0;

    TEST_ASSERT_EQ(A_sparse->ops->matvec(A_sparse, x, y_sparse), lv_BACKEND_OK);
    TEST_ASSERT_EQ(A_dense->ops->matvec(A_dense, x, y_dense), lv_BACKEND_OK);

    /* 稀疏与稠密 matvec 结果一致，且等于手算值 y = A*x = (6, 10, 14) */
    for (int64_t i = 0; i < 3; i++) {
        TEST_ASSERT_DOUBLE(y_sparse->data[i], y_dense->data[i], 1e-12);
    }
    TEST_ASSERT_DOUBLE(y_sparse->data[0], 6.0, 1e-12);
    TEST_ASSERT_DOUBLE(y_sparse->data[1], 10.0, 1e-12);
    TEST_ASSERT_DOUBLE(y_sparse->data[2], 14.0, 1e-12);

    /* 稀疏矩阵维度校验：错误向量长度应报 INVALID_ARGS */
    lvVector *x_wrong = lv_vector_create(lv_BACKEND_SERIAL, 2);
    TEST_ASSERT_NOT_NULL(x_wrong);
    TEST_ASSERT_EQ(A_sparse->ops->matvec(A_sparse, x_wrong, y_sparse), lv_BACKEND_INVALID_ARGS);
    x_wrong->ops->destroy(x_wrong);

    x->ops->destroy(x);
    y_sparse->ops->destroy(y_sparse);
    y_dense->ops->destroy(y_dense);
    A_sparse->ops->destroy(A_sparse);
    A_dense->ops->destroy(A_dense);
}

static void test_backend_sparse_solve_via_linsol(void) {
    /* 经线性求解器路径（DIRECT_SPARSE）求解稀疏系统：
     * A = [4 1 0; 1 3 1; 0 1 4]，b = (6, 10, 14)，解 x = (1, 2, 3) */
    lvMatrix *A = lv_matrix_create(lv_BACKEND_SERIAL, 3, 3, true);
    TEST_ASSERT_NOT_NULL(A);
    A->ops->set_element(A, 0, 0, 4.0);
    A->ops->set_element(A, 0, 1, 1.0);
    A->ops->set_element(A, 1, 0, 1.0);
    A->ops->set_element(A, 1, 1, 3.0);
    A->ops->set_element(A, 1, 2, 1.0);
    A->ops->set_element(A, 2, 1, 1.0);
    A->ops->set_element(A, 2, 2, 4.0);

    lvVector *b = lv_vector_create(lv_BACKEND_SERIAL, 3);
    lvVector *x = lv_vector_create(lv_BACKEND_SERIAL, 3);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(x);
    b->data[0] = 6.0;
    b->data[1] = 10.0;
    b->data[2] = 14.0;

    lvLinearSolver *LS = lv_linsol_create(lv_BACKEND_SERIAL, lv_LINSOL_DIRECT_SPARSE);
    TEST_ASSERT_NOT_NULL(LS);

    int ret = LS->ops->setup(LS, A);
    TEST_ASSERT_EQ(ret, lv_BACKEND_OK);
    ret = LS->ops->solve(LS, A, b, x);
    TEST_ASSERT_EQ(ret, lv_BACKEND_OK);

    TEST_ASSERT_DOUBLE(x->data[0], 1.0, 1e-4);
    TEST_ASSERT_DOUBLE(x->data[1], 2.0, 1e-4);
    TEST_ASSERT_DOUBLE(x->data[2], 3.0, 1e-4);

    LS->ops->destroy(LS);
    b->ops->destroy(b);
    x->ops->destroy(x);
    A->ops->destroy(A);
}

/* ============================================================
 * Main
 * ============================================================ */
TEST_MAIN_BEGIN("SparseLinearAlgebra")

    /* Lifecycle */
    TEST_MAIN_RUN(test_sparse_create_basic);
    TEST_MAIN_RUN(test_sparse_create_invalid);

    /* set / get */
    TEST_MAIN_RUN(test_sparse_set_get_basic);
    TEST_MAIN_RUN(test_sparse_set_get_out_of_bounds);
    TEST_MAIN_RUN(test_sparse_grow);

    /* 后端支撑操作 */
    TEST_MAIN_RUN(test_sparse_matvec);
    TEST_MAIN_RUN(test_sparse_zero);
    TEST_MAIN_RUN(test_sparse_copy);
    TEST_MAIN_RUN(test_sparse_scale);

    /* 求解器 */
    TEST_MAIN_RUN(test_sparse_solve_converges);
    TEST_MAIN_RUN(test_sparse_solve_non_square);
    TEST_MAIN_RUN(test_sparse_solve_zero_diagonal);
    TEST_MAIN_RUN(test_sparse_solve_null_args);

    /* numerical_backend 集成 */
    TEST_MAIN_RUN(test_backend_sparse_set_get);
    TEST_MAIN_RUN(test_backend_sparse_matvec_dense_consistency);
    TEST_MAIN_RUN(test_backend_sparse_solve_via_linsol);

TEST_MAIN_END()
