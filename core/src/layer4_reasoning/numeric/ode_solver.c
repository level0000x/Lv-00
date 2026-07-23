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

#include <math.h>
#include <stdlib.h>
#include <string.h>

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
 */
static void euler_step(lvODERhsFn rhs, double t, const double *y, double dt, size_t dim, void *params, double *y_next) {
    double *dydt = (double *) lv_calloc(dim, sizeof(double));
    if (!dydt) {
        /* 分配失败：清零输出并返回 */
        if (y_next)
            memset(y_next, 0, dim * sizeof(double));
        return;
    }

    rhs(t, y, params, dydt);

    for (size_t i = 0; i < dim; i++) {
        y_next[i] = y[i] + dt * dydt[i];
    }

    lv_free((void **) &dydt);
}

/**
 * @brief Perform one step of the classical RK4 method.
 */
static void rk4_step(lvODERhsFn rhs, double t, const double *y, double dt, size_t dim, void *params, double *y_next) {
    double *k1 = (double *) lv_calloc(dim, sizeof(double));
    double *k2 = (double *) lv_calloc(dim, sizeof(double));
    double *k3 = (double *) lv_calloc(dim, sizeof(double));
    double *k4 = (double *) lv_calloc(dim, sizeof(double));
    double *ytmp = (double *) lv_calloc(dim, sizeof(double));

    if (!k1 || !k2 || !k3 || !k4 || !ytmp) {
        /* Allocation failure: zero out y_next and clean up */
        if (y_next)
            memset(y_next, 0, dim * sizeof(double));
        lv_free((void **) &k1);
        lv_free((void **) &k2);
        lv_free((void **) &k3);
        lv_free((void **) &k4);
        lv_free((void **) &ytmp);
        return;
    }

    /* k1 = f(t, y) */
    rhs(t, y, params, k1);

    /* k2 = f(t + dt/2, y + dt/2 * k1) */
    for (size_t i = 0; i < dim; i++) {
        ytmp[i] = y[i] + 0.5 * dt * k1[i];
    }
    rhs(t + 0.5 * dt, ytmp, params, k2);

    /* k3 = f(t + dt/2, y + dt/2 * k2) */
    for (size_t i = 0; i < dim; i++) {
        ytmp[i] = y[i] + 0.5 * dt * k2[i];
    }
    rhs(t + 0.5 * dt, ytmp, params, k3);

    /* k4 = f(t + dt, y + dt * k3) */
    for (size_t i = 0; i < dim; i++) {
        ytmp[i] = y[i] + dt * k3[i];
    }
    rhs(t + dt, ytmp, params, k4);

    /* y_next = y + dt/6 * (k1 + 2*k2 + 2*k3 + k4) */
    for (size_t i = 0; i < dim; i++) {
        y_next[i] = y[i] + (dt / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }

    lv_free((void **) &k1);
    lv_free((void **) &k2);
    lv_free((void **) &k3);
    lv_free((void **) &k4);
    lv_free((void **) &ytmp);
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
static void ab4_step(lvODERhsFn rhs, double t, const double *y, double dt,
                     size_t dim, void *params, double *y_next,
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
    static const double beta[4] = {55.0 / 24.0, -59.0 / 24.0, 37.0 / 24.0, -9.0 / 24.0};
    size_t hist_idx[4];
    hist_idx[0] = history_idx;
    hist_idx[1] = (history_idx == 0) ? 3 : history_idx - 1;
    hist_idx[2] = (hist_idx[1] == 0) ? 3 : hist_idx[1] - 1;
    hist_idx[3] = (hist_idx[2] == 0) ? 3 : hist_idx[2] - 1;

    for (size_t j = 0; j < dim; j++) {
        double sum = beta[0] * f_n[j]
                   + beta[1] * f_history[hist_idx[1]][j]
                   + beta[2] * f_history[hist_idx[2]][j]
                   + beta[3] * f_history[hist_idx[3]][j];
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

    /* Copy initial state */
    memcpy(y_curr, problem->y0, dim * sizeof(double));

    /* Adams-Bashforth history buffer (4 most recent derivatives, circular) */
    double *f_history[4] = {NULL, NULL, NULL, NULL};
    size_t ab_history_idx = 0;

    for (size_t i = 1; i <= n_steps; i++) {
        /* Check if we would overshoot t_end */
        if (t + dt > problem->t_span[1]) {
            dt = problem->t_span[1] - t;
        }

        switch (config->method) {
            case ODE_RK4:
                rk4_step(problem->rhs_fn, t, y_curr, dt, dim, problem->params, y_next);
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
                    ab4_step(problem->rhs_fn, t, y_curr, dt, dim, problem->params, y_next,
                             f_history, ab_history_idx);
                }
                break;
            }
            case ODE_EULER:
            default:
                euler_step(problem->rhs_fn, t, y_curr, dt, dim, problem->params, y_next);
                break;
        }

        t += dt;
        sol->t_values[i] = t;

        /* Swap buffers */
        double *tmp = y_curr;
        y_curr = y_next;
        y_next = tmp;

        /* Store state */
        for (size_t j = 0; j < dim; j++) {
            sol->y_values[i * dim + j] = y_curr[j];
        }
    }

    lv_free((void **) &y_curr);
    lv_free((void **) &y_next);
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
