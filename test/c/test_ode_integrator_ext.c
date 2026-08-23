/**
 * @file test_ode_integrator_ext.c
 * @brief 共享 ODE 单步积分器契约测试（批次 C-㊺续32：ode_integrator.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（2 个）：
 *   lv_ode_rk4_step / lv_ode_euler_step
 *
 * 契约要点（与 ode_integrator.c 核对）：
 *   - euler：yout = y + h*f(t,y)；deriv 失败返回其错误码；NULL 契约返回
 *     lv_ERROR_INVALID_PARAM。
 *   - rk4：经典四阶；deriv 失败提前返回错误码。
 *   - ab4 系数表 {55/24, -59/24, 37/24, -9/24}（已覆盖，附带断言）。
 *
 * @author Lv-00 Project
 */

#include <math.h>
#include <stdio.h>

#include "lv/ode_integrator.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#define TOL 1e-9

/* ============== 测试辅助：右端函数 ============== */

/** dy/dt = y（指数增长） */
static int deriv_exp(double t, const double *y, double *dydt, void *ctx) {
    (void) t;
    (void) ctx;
    dydt[0] = y[0];
    return 0;
}

/** 常数导数 dy/dt = 2 */
static int deriv_const(double t, const double *y, double *dydt, void *ctx) {
    (void) t;
    (void) y;
    (void) ctx;
    dydt[0] = 2.0;
    return 0;
}

/** 失败右端函数 */
static int deriv_fail(double t, const double *y, double *dydt, void *ctx) {
    (void) t;
    (void) y;
    (void) dydt;
    (void) ctx;
    return -7;
}

/* ============== 测试：Euler 单步 ============== */

static void test_euler_step(void) {
    double y[1] = {1.0};
    double yout[1] = {0.0};

    /* dy/dt = y, h=0.1：y1 = 1 + 0.1*1 = 1.1 */
    int rc = lv_ode_euler_step(0.0, y, 1, 0.1, yout, deriv_exp, NULL);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_DOUBLE(yout[0], 1.1, TOL);

    /* 常数导数：yout = 1 + 0.5*2 = 2.0（Euler 精确） */
    y[0] = 1.0;
    rc = lv_ode_euler_step(0.0, y, 1, 0.5, yout, deriv_const, NULL);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_DOUBLE(yout[0], 2.0, TOL);

    /* deriv 失败：返回其错误码 */
    rc = lv_ode_euler_step(0.0, y, 1, 0.1, yout, deriv_fail, NULL);
    TEST_ASSERT_EQ(rc, -7);

    /* NULL 契约 */
    TEST_ASSERT(lv_ode_euler_step(0.0, NULL, 1, 0.1, yout, deriv_exp, NULL) != 0, "y NULL");
    TEST_ASSERT(lv_ode_euler_step(0.0, y, 1, 0.1, NULL, deriv_exp, NULL) != 0, "yout NULL");
    TEST_ASSERT(lv_ode_euler_step(0.0, y, 1, 0.1, yout, NULL, NULL) != 0, "deriv NULL");
}

/* ============== 测试：RK4 单步 ============== */

static void test_rk4_step(void) {
    double y[1] = {1.0};
    double yout[1] = {0.0};

    /* 常数导数：RK4 对线性精确，yout = 1 + 0.5*2 = 2.0 */
    int rc = lv_ode_rk4_step(0.0, y, 1, 0.5, yout, deriv_const, NULL);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_DOUBLE(yout[0], 2.0, TOL);

    /* dy/dt = y：RK4 单步精确值（标准四阶，h=0.1 局部误差 O(h^5)）：
     * k1=1, k2=1.05, k3=1.0525, k4=1.10525 -> yout = 1 + h/6*(k1+2k2+2k3+k4)
     * = 1.105170833333... */
    y[0] = 1.0;
    rc = lv_ode_rk4_step(0.0, y, 1, 0.1, yout, deriv_exp, NULL);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_DOUBLE(yout[0], 1.105170833333, 1e-9);

    /* deriv 失败：提前返回错误码 */
    rc = lv_ode_rk4_step(0.0, y, 1, 0.1, yout, deriv_fail, NULL);
    TEST_ASSERT_EQ(rc, -7);

    /* NULL 契约 */
    TEST_ASSERT(lv_ode_rk4_step(0.0, NULL, 1, 0.1, yout, deriv_exp, NULL) != 0, "y NULL");
    TEST_ASSERT(lv_ode_rk4_step(0.0, y, 1, 0.1, NULL, deriv_exp, NULL) != 0, "yout NULL");
    TEST_ASSERT(lv_ode_rk4_step(0.0, y, 1, 0.1, yout, NULL, NULL) != 0, "deriv NULL");
}

/* ============== 测试：AB4 系数表 ============== */

static void test_ab4_coeffs(void) {
    TEST_ASSERT_DOUBLE(lv_ode_ab4_coeffs[0], 55.0 / 24.0, TOL);
    TEST_ASSERT_DOUBLE(lv_ode_ab4_coeffs[1], -59.0 / 24.0, TOL);
    TEST_ASSERT_DOUBLE(lv_ode_ab4_coeffs[2], 37.0 / 24.0, TOL);
    TEST_ASSERT_DOUBLE(lv_ode_ab4_coeffs[3], -9.0 / 24.0, TOL);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("OdeIntegratorExt")

    printf("\n--- ode_integrator (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_euler_step);
    TEST_MAIN_RUN(test_rk4_step);
    TEST_MAIN_RUN(test_ab4_coeffs);

TEST_MAIN_END()
