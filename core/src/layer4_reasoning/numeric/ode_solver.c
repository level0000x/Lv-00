/**
 * @file ode_solver.c
 * @brief Implementation of the ODE solver module.
 *
 * @details Implements explicit Euler, classical RK4, and Adams-Bashforth
 *          multistep methods for solving initial value problems dy/dt = f(t, y).
 *
 *          Euler method (1st order):
 *            y_{n+1} = y_n + dt * f(t_n, y_n)
 *
 *          RK4 method (4th order):
 *            k1 = f(t_n, y_n)
 *            k2 = f(t_n + dt/2, y_n + dt/2 * k1)
 *            k3 = f(t_n + dt/2, y_n + dt/2 * k2)
 *            k4 = f(t_n + dt, y_n + dt * k3)
 *            y_{n+1} = y_n + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
 *
 *          Adams-Bashforth 4-step (AB4, 4th order):
 *            y_{n+4} = y_{n+3} + dt * (55/24 * f_{n+3} - 59/24 * f_{n+2}
 *                                       + 37/24 * f_{n+1} - 9/24 * f_n)
 *            Startup uses RK4 for the first 3 steps.
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include "lv/ode_solver.h"
#include "lv/ode_integrator.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lv_utils.h" /* lv_calloc / lv_malloc / lv_free */

/* ============================================================
 * ODE 右端函数适配器（lvODERhsFn(void 返回) -> lvOdeDerivFn(int 返回)）
 * ============================================================ */

/** @brief 适配器上下文：携带 RHS 函数与用户参数 */
typedef struct {
    lvODERhsFn rhs;
    void *params;
} lvOdeRhsCtx;

/** @brief 将 void 返回的 lvODERhsFn 包装为 int 返回的 lvOdeDerivFn */
static int ode_solver_rhs_adapter(double t, const double *y, double *dydt, void *ctx) {
    lvOdeRhsCtx *c = (lvOdeRhsCtx *) ctx;
    c->rhs(t, y, c->params, dydt);
    return 0;
}

/* ============================================================
 * Internal helpers
 * ============================================================ */

/**
 * @brief Compute the number of steps needed for the integration interval.
 */
static size_t compute_num_steps(const lvODEProblem *problem, const lvODEConfig *config) {
    double t_start = problem->t_span[0];
    double t_end = problem->t_span[1];
    double dt = config->dt;

    if (dt <= 0.0 || t_end <= t_start) {
        return 0;
    }

    size_t n = (size_t) ceil((t_end - t_start) / dt);
    /* Respect max_steps limit */
    if (config->max_steps > 0 && n > config->max_steps) {
        n = config->max_steps;
    }
    return n;
}

/**
 * @brief Perform one step of the explicit Euler method.
 *
 * 复用共享积分器 lv_ode_euler_step（与 geom_evol.c 共用同一实现）。
 */
static void euler_step(lvODERhsFn rhs, double t, const double *y, double dt, size_t dim, void *params, double *y_next) {
    lvOdeRhsCtx ctx = {rhs, params};
    lv_ode_euler_step(t, y, dim, dt, y_next, ode_solver_rhs_adapter, &ctx);
}

/**
 * @brief Perform one step of the classical RK4 method.
 *
 * 复用共享积分器 lv_ode_rk4_step（与 geom_evol.c 共用同一实现，
 * k1-k4 计算顺序逐位一致）。
 */
static void rk4_step(lvODERhsFn rhs, double t, const double *y, double dt, size_t dim, void *params, double *y_next) {
    lvOdeRhsCtx ctx = {rhs, params};
    lv_ode_rk4_step(t, y, dim, dt, y_next, ode_solver_rhs_adapter, &ctx);
}

/**
 * @brief Perform one step of the 4th-order Adams-Bashforth multistep method.
 *
 * Uses a circular buffer of the 4 most recent derivatives f_{n-3}, f_{n-2},
 * f_{n-1}, f_n. Before enough history is available (first 3 steps), the
 * caller should use RK4 startup.
 *
 * AB4 formula:
 *   y_{n+1} = y_n + dt * (55/24 * f_n - 59/24 * f_{n-1}
 *                          + 37/24 * f_{n-2} - 9/24 * f_{n-3})
 */
static void ab4_step(lvODERhsFn rhs, double t, const double *y, double dt, size_t dim, void *params, double *y_next,
                     double **f_history, size_t history_idx) {
    if (!f_history || !f_history[0] || !f_history[1] || !f_history[2] || !f_history[3]) {
        /* Insufficient history — use Euler fallback */
        euler_step(rhs, t, y, dt, dim, params, y_next);
        return;
    }

    /* f_n = rhs(t, y) */
    double *f_n = (double *) lv_calloc(dim, sizeof(double));
    if (!f_n) {
        memset(y_next, 0, dim * sizeof(double));
        return;
    }
    rhs(t, y, params, f_n);

    /* Compute y_{n+1} = y_n + dt * sum(beta_i * f_{n-i}) */
    const double *beta = lv_ode_ab4_coeffs;
    size_t hist_idx[4];
    hist_idx[0] = history_idx;
    hist_idx[1] = (history_idx == 0) ? 3 : history_idx - 1;
    hist_idx[2] = (hist_idx[1] == 0) ? 3 : hist_idx[1] - 1;
    hist_idx[3] = (hist_idx[2] == 0) ? 3 : hist_idx[2] - 1;

    for (size_t j = 0; j < dim; j++) {
        double sum = beta[0] * f_n[j] + beta[1] * f_history[hist_idx[1]][j] + beta[2] * f_history[hist_idx[2]][j] +
                     beta[3] * f_history[hist_idx[3]][j];
        y_next[j] = y[j] + dt * sum;
    }

    /* Rotate history: store f_n at history_idx+1 */
    size_t next_idx = (history_idx + 1) & 3;
    memcpy(f_history[next_idx], f_n, dim * sizeof(double));

    lv_free((void **) &f_n);
}

/* ============================================================
 * API: Solve
 * ============================================================ */

lvODESolution *ode_solve(const lvODEProblem *problem, const lvODEConfig *config) {
    if (!problem || !config || !problem->rhs_fn || !problem->y0) {
        return NULL;
    }

    size_t dim = problem->dim;
    if (dim == 0) {
        return NULL;
    }

    size_t n_steps = compute_num_steps(problem, config);
    if (n_steps == 0) {
        return NULL;
    }

    /* Allocate solution */
    lvODESolution *sol = (lvODESolution *) lv_calloc(1, sizeof(lvODESolution));
    if (!sol)
        return NULL;

    sol->n_steps = n_steps + 1; /* Include initial condition */
    sol->dim = dim;

    sol->t_values = (double *) lv_calloc(sol->n_steps, sizeof(double));
    sol->y_values = (double *) lv_calloc(sol->n_steps * dim, sizeof(double));

    if (!sol->t_values || !sol->y_values) {
        lv_free((void **) &sol->t_values);
        lv_free((void **) &sol->y_values);
        lv_free((void **) &sol);
        return NULL;
    }

    /* Set initial condition */
    double t = problem->t_span[0];
    double dt = config->dt;

    sol->t_values[0] = t;
    for (size_t j = 0; j < dim; j++) {
        sol->y_values[j] = problem->y0[j];
    }

    /* Integration loop */
    double *y_curr = (double *) lv_calloc(dim, sizeof(double));
    double *y_next = (double *) lv_calloc(dim, sizeof(double));
    if (!y_curr || !y_next) {
        lv_free((void **) &y_curr);
        lv_free((void **) &y_next);
        lv_free((void **) &sol->t_values);
        lv_free((void **) &sol->y_values);
        lv_free((void **) &sol);
        return NULL;
    }

    /* Pre-allocate arrays for adaptive step size control (RK4 only) */
    double *y_full = (double *) lv_malloc(dim * sizeof(double));
    double *y_half = (double *) lv_malloc(dim * sizeof(double));
    double *y_tmp_adapt = (double *) lv_malloc(dim * sizeof(double));
    if (!y_full || !y_half || !y_tmp_adapt) {
        lv_free((void **) &y_curr);
        lv_free((void **) &y_next);
        lv_free((void **) &y_full);
        lv_free((void **) &y_half);
        lv_free((void **) &y_tmp_adapt);
        lv_free((void **) &sol->t_values);
        lv_free((void **) &sol->y_values);
        lv_free((void **) &sol);
        return NULL;
    }

    /* Copy initial state */
    memcpy(y_curr, problem->y0, dim * sizeof(double));

    /* Adams-Bashforth history buffer (4 most recent derivatives, circular) */
    double *f_history[4] = {NULL, NULL, NULL, NULL};
    size_t ab_history_idx = 0;

    const double t_end = problem->t_span[1];
    const int use_adaptive = (config->method == ODE_RK4 && config->rtol > 0.0 && config->atol > 0.0);
    size_t i = 1; /* 已写入条目数（含初始条件），即下一个写入索引 */
    const size_t capacity = n_steps + 1; /* 预分配容量（含初始条件） */

    while (t < t_end && i < capacity) {
        /* 负步长/非法步长防护：杜绝时间倒流 */
        if (dt <= 0.0) {
            break;
        }

        /* 越界钳制：最后一步截断到 t_end */
        if (t + dt > t_end) {
            dt = t_end - t;
        }
        if (dt <= 0.0) {
            break;
        }

        /* 机器精度防护：步长过小（t+dt==t）时直接收尾，避免死循环 */
        if (t + dt <= t) {
            t = t_end;
            sol->t_values[i] = t;
            for (size_t j = 0; j < dim; j++) {
                sol->y_values[i * dim + j] = y_curr[j];
            }
            i++;
            break;
        }

        double step_used = dt; /* 本步实际使用的步长（自适应路径可能不等于 dt） */

        switch (config->method) {
            case ODE_RK4:
                if (use_adaptive) {
                    /* 自适应步长控制（基于步长加倍误差估计） */
                    double current_dt = dt;
                    int retries = 0;
                    const int max_retries = 100;

                    for (;;) {
                        /* 用 current_dt 做一步完整步进 */
                        rk4_step(problem->rhs_fn, t, y_curr, current_dt, dim, problem->params, y_full);

                        /* 用两个半步进 */
                        rk4_step(problem->rhs_fn, t, y_curr, current_dt * 0.5, dim, problem->params, y_tmp_adapt);
                        rk4_step(problem->rhs_fn, t + current_dt * 0.5, y_tmp_adapt, current_dt * 0.5, dim,
                                 problem->params, y_half);

                        /* 估计相对误差 */
                        double max_err = 0.0;
                        for (size_t j = 0; j < dim; j++) {
                            double scale = config->atol + config->rtol * fmax(fabs(y_curr[j]), fabs(y_full[j]));
                            double err = fabs(y_full[j] - y_half[j]) / scale;
                            if (err > max_err) max_err = err;
                        }

                        const double safety = 0.9;

                        if (max_err > 1.0 && retries < max_retries) {
                            /* 步进被拒绝：缩小 dt 重试 */
                            current_dt *= fmax(0.2, safety / pow(max_err, 1.0 / 4.0));
                            retries++;
                            continue;
                        }

                        /* 步进被接受：根据误差调整后续步长 */
                        if (max_err < 0.5 && max_err > 1e-15) {
                            double factor = fmin(5.0, safety / pow(max_err, 1.0 / 4.0));
                            /* 上限比值始终取正，防御 current_dt 非正导致的符号翻转 */
                            double ratio_cap = 10.0 * fabs(config->dt) / fabs(current_dt);
                            dt = current_dt * fmin(factor, ratio_cap);
                        } else {
                            dt = current_dt;
                        }

                        /* 时间推进必须用本步实际步长，而非下一步建议步长 dt */
                        step_used = current_dt;
                        memcpy(y_next, y_full, dim * sizeof(double));
                        break;
                    }
                } else {
                    /* 固定步长 RK4 */
                    rk4_step(problem->rhs_fn, t, y_curr, dt, dim, problem->params, y_next);
                }
                break;
            case ODE_ADAMS: {
                /* AB4 startup: first 3 steps use RK4 to build history */
                if (i <= 3) {
                    rk4_step(problem->rhs_fn, t, y_curr, dt, dim, problem->params, y_next);
                    /* Compute and store derivative for history */
                    double *f_k = (double *) lv_calloc(dim, sizeof(double));
                    if (f_k) {
                        problem->rhs_fn(t, y_curr, problem->params, f_k);
                        f_history[ab_history_idx] = f_k;
                        ab_history_idx = (ab_history_idx + 1) & 3;
                    }
                } else {
                    /* Full AB4 step */
                    ab4_step(problem->rhs_fn, t, y_curr, dt, dim, problem->params, y_next, f_history, ab_history_idx);
                }
                break;
            }
            case ODE_EULER:
            default:
                euler_step(problem->rhs_fn, t, y_curr, dt, dim, problem->params, y_next);
                break;
        }

        /* 用实际步长推进时间 */
        t += step_used;
        sol->t_values[i] = t;

        /* Swap buffers */
        double *tmp = y_curr;
        y_curr = y_next;
        y_next = tmp;

        /* Store state */
        for (size_t j = 0; j < dim; j++) {
            sol->y_values[i * dim + j] = y_curr[j];
        }
        i++;
    }

    /* 实际存储条目数（含初始条件）：自适应路径步数可变 */
    sol->n_steps = i;

    lv_free((void **) &y_curr);
    lv_free((void **) &y_next);
    lv_free((void **) &y_full);
    lv_free((void **) &y_half);
    lv_free((void **) &y_tmp_adapt);
    /* Free AB history buffers */
    for (int k = 0; k < 4; k++) {
        lv_free((void **) &f_history[k]);
    }
    return sol;
}

/* ============================================================
 * API: Destroy
 * ============================================================ */

void ode_solution_destroy(lvODESolution *sol) {
    if (!sol)
        return;
    lv_free((void **) &sol->t_values);
    lv_free((void **) &sol->y_values);
    lv_free((void **) &sol);
}
