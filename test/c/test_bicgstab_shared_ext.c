/**
 * @file test_bicgstab_shared_ext.c
 * @brief BiCGSTAB 共享内核契约测试（批次 C-㊺续37：bicgstab_shared.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（2 个）：
 *   lv_bicgstab_solve / lv_linsol_default_params
 *
 * 契约要点（与 bicgstab_shared.c 核对）：
 *   - default_params：max_iters=200、tol=lv_EPSILON_HIGH(1e-10)；NULL 参数安全。
 *   - solve：NULL/非法参数 → lv_BACKEND_INVALID_ARGS；对角系统求解。
 *
 * @author Lv-00 Project
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "lv/bicgstab_shared.h"
#include "lv/numerical_backend.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试辅助：算子 ============== */

static double ctx_dot(void *ctx, const double *a, const double *b, int64_t n) {
    (void) ctx;
    double s = 0.0;
    for (int64_t i = 0; i < n; i++)
        s += a[i] * b[i];
    return s;
}

static double ctx_norm(void *ctx, const double *v, int64_t n) {
    return sqrt(ctx_dot(ctx, v, v, n));
}

static void ctx_matvec(void *ctx, const lvMatrix *a, const double *x, double *y, int64_t n) {
    (void) ctx;
    const double *data = (const double *) a->data;
    for (int64_t i = 0; i < n; i++) {
        double s = 0.0;
        for (int64_t j = 0; j < n; j++)
            s += data[i * n + j] * x[j];
        y[i] = s;
    }
}

/* ============== 测试：默认参数 ============== */

static void test_default_params(void) {
    int it = 0;
    double tol = 0.0;
    lv_linsol_default_params(&it, &tol);
    TEST_ASSERT_EQ(it, 200);
    TEST_ASSERT_DOUBLE(tol, 1e-10, 1e-15);

    /* NULL 参数安全 */
    lv_linsol_default_params(NULL, &tol);
    lv_linsol_default_params(&it, NULL);
    lv_linsol_default_params(NULL, NULL);
}

/* ============== 测试：NULL 契约 ============== */

static void test_null_contract(void) {
    lvBicgstabOps ops = {NULL, ctx_dot, ctx_norm, ctx_matvec};
    lvMatrix a;
    memset(&a, 0, sizeof(a));
    double b[2] = {1, 1};
    double x[2] = {0, 0};

    TEST_ASSERT_EQ(lv_bicgstab_solve(NULL, &a, b, x, 2, 10, 1e-9, 1e-12), (int) lv_BACKEND_INVALID_ARGS);
    TEST_ASSERT_EQ(lv_bicgstab_solve(&ops, NULL, b, x, 2, 10, 1e-9, 1e-12), (int) lv_BACKEND_INVALID_ARGS);
    TEST_ASSERT_EQ(lv_bicgstab_solve(&ops, &a, NULL, x, 2, 10, 1e-9, 1e-12), (int) lv_BACKEND_INVALID_ARGS);
    TEST_ASSERT_EQ(lv_bicgstab_solve(&ops, &a, b, NULL, 2, 10, 1e-9, 1e-12), (int) lv_BACKEND_INVALID_ARGS);
    TEST_ASSERT_EQ(lv_bicgstab_solve(&ops, &a, b, x, 0, 10, 1e-9, 1e-12), (int) lv_BACKEND_INVALID_ARGS);

    /* 缺算子 */
    lvBicgstabOps bad = {NULL, ctx_dot, ctx_norm, NULL};
    TEST_ASSERT_EQ(lv_bicgstab_solve(&bad, &a, b, x, 2, 10, 1e-9, 1e-12), (int) lv_BACKEND_INVALID_ARGS);
}

/* ============== 测试：对角系统求解 ============== */

static void test_solve(void) {
    /* A = diag(2, 3)，b = [4, 9]，解 [2, 3] */
    double data[4] = {2.0, 0.0, 0.0, 3.0};
    lvMatrix a;
    memset(&a, 0, sizeof(a));
    a.rows = 2;
    a.cols = 2;
    a.sparse = false;
    a.data = data;

    double b[2] = {4.0, 9.0};
    double x[2] = {0.0, 0.0};
    lvBicgstabOps ops = {NULL, ctx_dot, ctx_norm, ctx_matvec};

    int rc = lv_bicgstab_solve(&ops, &a, b, x, 2, 200, 1e-8, 1e-12);
    TEST_ASSERT_EQ(rc, (int) lv_BACKEND_OK);
    TEST_ASSERT_DOUBLE(x[0], 2.0, 1e-6);
    TEST_ASSERT_DOUBLE(x[1], 3.0, 1e-6);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("BicgstabSharedExt")

    printf("\n--- bicgstab_shared (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_default_params);
    TEST_MAIN_RUN(test_null_contract);
    TEST_MAIN_RUN(test_solve);

TEST_MAIN_END()
