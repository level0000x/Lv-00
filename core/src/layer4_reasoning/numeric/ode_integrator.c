/**
 * @file ode_integrator.c
 * @brief 共享 ODE 单步积分器实现（RK4 / Euler / AB4 系数）
 *
 * @details 经典 RK4 与显式 Euler 单步、AB4 系数表的统一实现。
 *          k1-k4 的计算顺序与原 ode_solver.c / geom_evol.c 中的
 *          各自实现逐位一致：
 *
 *          k1 = f(t, y)
 *          k2 = f(t + h/2, y + h/2*k1)
 *          k3 = f(t + h/2, y + h/2*k2)
 *          k4 = f(t + h, y + h*k3)
 *          yout = y + h/6*(k1 + 2*k2 + 2*k3 + k4)
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date   2026-07-31
 */

#include "lv/ode_integrator.h"

#include <string.h>

#include "lv/error_codes.h"
#include "lv/lv_utils.h"
#include "lv/lv_lifecycle.h"

/* ============================================================
 * AB4 系数表
 * ============================================================ */

/** @brief AB4 系数：y_{n+1} = y_n + h*(55/24*f_n - 59/24*f_{n-1} + 37/24*f_{n-2} - 9/24*f_{n-3}) */
const double lv_ode_ab4_coeffs[4] = {55.0 / 24.0, -59.0 / 24.0, 37.0 / 24.0, -9.0 / 24.0};

/* ============================================================
 * Euler 单步
 * ============================================================ */

int lv_ode_euler_step(double t, const double *y, size_t n, double h, double *yout,
                      lvOdeDerivFn deriv, void *ctx) {
    if (!y || !yout || !deriv) {
        return lv_ERROR_INVALID_PARAM;
    }

    double *dydt = (double *) lv_calloc(n, sizeof(double));
    if (!dydt) {
        /* 分配失败：清零输出并返回 */
        memset(yout, 0, n * sizeof(double));
        return lv_ERROR_ALLOCATION_FAILED;
    }

    int ret = deriv(t, y, dydt, ctx);
    if (ret == 0) {
        for (size_t i = 0; i < n; i++) {
            yout[i] = y[i] + h * dydt[i];
        }
    }

    lv_free((void **) &dydt);
    return ret;
}

/* ============================================================
 * RK4 单步
 * ============================================================ */

int lv_ode_rk4_step(double t, const double *y, size_t n, double h, double *yout,
                    lvOdeDerivFn deriv, void *ctx) {
    if (!y || !yout || !deriv) {
        return lv_ERROR_INVALID_PARAM;
    }

    double *k1 = (double *) lv_calloc(n, sizeof(double));
    double *k2 = (double *) lv_calloc(n, sizeof(double));
    double *k3 = (double *) lv_calloc(n, sizeof(double));
    double *k4 = (double *) lv_calloc(n, sizeof(double));
    double *ytmp = (double *) lv_calloc(n, sizeof(double));
    /* 逐分配注册 lv_DEFER 守卫：任一分配失败 / deriv 失败路径（goto cleanup 位置）
     * 自动释放全部已分配缓冲，替代重复 5 连 lv_free 样板 */
    lv_DEFER_FREE(k1);
    lv_DEFER_FREE(k2);
    lv_DEFER_FREE(k3);
    lv_DEFER_FREE(k4);
    lv_DEFER_FREE(ytmp);

    if (!k1 || !k2 || !k3 || !k4 || !ytmp) {
        /* 分配失败：清零输出（已分配的缓冲由守卫自动释放） */
        memset(yout, 0, n * sizeof(double));
        return lv_ERROR_ALLOCATION_FAILED;
    }

    double half_h = 0.5 * h;
    double sixth_h = h / 6.0;
    int ret = 0;

    /* k1 = f(t, y) */
    ret = deriv(t, y, k1, ctx);
    if (ret != 0)
        return ret;

    /* k2 = f(t + h/2, y + h/2 * k1) */
    for (size_t i = 0; i < n; i++) {
        ytmp[i] = y[i] + half_h * k1[i];
    }
    ret = deriv(t + half_h, ytmp, k2, ctx);
    if (ret != 0)
        return ret;

    /* k3 = f(t + h/2, y + h/2 * k2) */
    for (size_t i = 0; i < n; i++) {
        ytmp[i] = y[i] + half_h * k2[i];
    }
    ret = deriv(t + half_h, ytmp, k3, ctx);
    if (ret != 0)
        return ret;

    /* k4 = f(t + h, y + h * k3) */
    for (size_t i = 0; i < n; i++) {
        ytmp[i] = y[i] + h * k3[i];
    }
    ret = deriv(t + h, ytmp, k4, ctx);
    if (ret != 0)
        return ret;

    /* yout = y + h/6 * (k1 + 2*k2 + 2*k3 + k4) */
    for (size_t i = 0; i < n; i++) {
        yout[i] = y[i] + sixth_h * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }

    return ret;
}
