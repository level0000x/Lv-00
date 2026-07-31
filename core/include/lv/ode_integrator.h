/**
 * @file ode_integrator.h
 * @brief 共享 ODE 单步积分器（RK4 / Euler / AB4 系数）
 *
 * @details 统一封装经典四阶 Runge-Kutta 与显式 Euler 单步积分，
 *          以及 Adams-Bashforth 4 阶（AB4）系数表，供 ode_solver.c、
 *          geom_evol.c 等模块复用，消除跨文件的重复实现。
 *
 *          右端函数回调返回 int：0 表示成功，非 0 表示失败。
 *          - geom_evol.c 的 rhs_func 本身返回 int，可直接适配；
 *          - ode_solver.c 的 lvODERhsFn 为 void 返回，由调用方包装适配。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date   2026-07-31
 */
#ifndef lv_ODE_INTEGRATOR_H
#define lv_ODE_INTEGRATOR_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>

#include "lv.h"

/* ============================================================
 * ODE 右端函数回调
 * ============================================================ */
/**
 * @brief 右端函数回调：dydt = f(t, y, ctx)
 *
 * @param t    当前时间
 * @param y    当前状态向量（n 维）
 * @param dydt 输出导数向量（调用方分配的 n 维缓冲区）
 * @param ctx  调用方上下文（透传）
 * @return 0 表示成功，非 0 表示失败（错误码由调用方约定）
 */
typedef int (*lvOdeDerivFn)(double t, const double *y, double *dydt, void *ctx);

/* ============================================================
 * 单步积分器
 * ============================================================ */
/**
 * @brief 经典四阶 Runge-Kutta 单步
 *
 *   k1 = f(t, y)
 *   k2 = f(t + h/2, y + h/2*k1)
 *   k3 = f(t + h/2, y + h/2*k2)
 *   k4 = f(t + h, y + h*k3)
 *   yout = y + h/6*(k1 + 2*k2 + 2*k3 + k4)
 *
 * @param t     当前时间
 * @param y     当前状态向量（n 维）
 * @param n     状态维度
 * @param h     步长
 * @param yout  输出状态向量（调用方分配的 n 维缓冲区）
 * @param deriv 右端函数回调
 * @param ctx   透传给 deriv 的上下文
 * @return 0 成功；deriv 失败时返回其错误码；分配失败返回 lv_ERROR_ALLOCATION_FAILED
 */
lv_PUBLIC_API int lv_ode_rk4_step(double t, const double *y, size_t n, double h, double *yout,
                                  lvOdeDerivFn deriv, void *ctx);

/**
 * @brief 显式 Euler 单步
 *
 *   yout = y + h * f(t, y)
 *
 * @param t     当前时间
 * @param y     当前状态向量（n 维）
 * @param n     状态维度
 * @param h     步长
 * @param yout  输出状态向量（调用方分配的 n 维缓冲区）
 * @param deriv 右端函数回调
 * @param ctx   透传给 deriv 的上下文
 * @return 0 成功；deriv 失败时返回其错误码；分配失败返回 lv_ERROR_ALLOCATION_FAILED
 */
lv_PUBLIC_API int lv_ode_euler_step(double t, const double *y, size_t n, double h, double *yout,
                                    lvOdeDerivFn deriv, void *ctx);

/* ============================================================
 * AB4 系数表
 * ============================================================ */
/**
 * @brief AB4 系数表：{55/24, -59/24, 37/24, -9/24}
 *
 * y_{n+1} = y_n + h * (beta[0]*f_n + beta[1]*f_{n-1} + beta[2]*f_{n-2} + beta[3]*f_{n-3})
 */
lv_PUBLIC_API extern const double lv_ode_ab4_coeffs[4];

#ifdef __cplusplus
}
#endif
#endif /* lv_ODE_INTEGRATOR_H */
