/**
 * @file ode_solver.c
 * @brief Implementation of the ODE solver module.
 *
 * @details Implements explicit Euler and classical RK4 methods for
 *          solving initial value problems dy/dt = f(t, y).
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
static size_t compute_num_steps(const lvODEProblem *problem,
                                const lvODEConfig  *config) {
    double t_start = problem->t_span[0];
    double t_end   = problem->t_span[1];
    double dt      = config->dt;

    if (dt <= 0.0 || t_end <= t_start) {
        return 0;
    }

    size_t n = (size_t)ceil((t_end - t_start) / dt);
    /* Respect max_steps limit */
    if (config->max_steps > 0 && n > config->max_steps) {
        n = config->max_steps;
    }
    return n;
}

/**
 * @brief Perform one step of the explicit Euler method.
 */
static void euler_step(lvODERhsFn rhs, double t, const double *y,
                       double dt, size_t dim, void *params, double *y_next) {
    double *dydt = (double *)calloc(dim, sizeof(double));
    if (!dydt) return;

    rhs(t, y, params, dydt);

    for (size_t i = 0; i < dim; i++) {
        y_next[i] = y[i] + dt * dydt[i];
    }

    free(dydt);
}

/**
 * @brief Perform one step of the classical RK4 method.
 */
static void rk4_step(lvODERhsFn rhs, double t, const double *y,
                     double dt, size_t dim, void *params, double *y_next) {
    double *k1 = (double *)calloc(dim, sizeof(double));
    double *k2 = (double *)calloc(dim, sizeof(double));
    double *k3 = (double *)calloc(dim, sizeof(double));
    double *k4 = (double *)calloc(dim, sizeof(double));
    double *ytmp = (double *)calloc(dim, sizeof(double));

    if (!k1 || !k2 || !k3 || !k4 || !ytmp) {
        /* Allocation failure: zero out y_next and clean up */
        if (y_next) memset(y_next, 0, dim * sizeof(double));
        free(k1); free(k2); free(k3); free(k4); free(ytmp);
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

    free(k1);
    free(k2);
    free(k3);
    free(k4);
    free(ytmp);
}

/* ============================================================
 * API: Solve
 * ============================================================ */

lvODESolution *ode_solve(const lvODEProblem *problem,
                           const lvODEConfig  *config) {
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
    lvODESolution *sol = (lvODESolution *)calloc(1, sizeof(lvODESolution));
    if (!sol) return NULL;

    sol->n_steps = n_steps + 1; /* Include initial condition */
    sol->dim    = dim;

    sol->t_values = (double *)calloc(sol->n_steps, sizeof(double));
    sol->y_values = (double *)calloc(sol->n_steps * dim, sizeof(double));

    if (!sol->t_values || !sol->y_values) {
        free(sol->t_values);
        free(sol->y_values);
        free(sol);
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
    double *y_curr = (double *)calloc(dim, sizeof(double));
    double *y_next = (double *)calloc(dim, sizeof(double));
    if (!y_curr || !y_next) {
        free(y_curr);
        free(y_next);
        free(sol->t_values);
        free(sol->y_values);
        free(sol);
        return NULL;
    }

    /* Copy initial state */
    memcpy(y_curr, problem->y0, dim * sizeof(double));

    for (size_t i = 1; i <= n_steps; i++) {
        /* Check if we would overshoot t_end */
        if (t + dt > problem->t_span[1]) {
            dt = problem->t_span[1] - t;
        }

        switch (config->method) {
            case ODE_RK4:
                rk4_step(problem->rhs_fn, t, y_curr, dt, dim,
                         problem->params, y_next);
                break;
            case ODE_EULER:
            case ODE_ADAMS: /* Fall back to Euler for unsupported methods */
            default:
                euler_step(problem->rhs_fn, t, y_curr, dt, dim,
                           problem->params, y_next);
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

    free(y_curr);
    free(y_next);
    return sol;
}

/* ============================================================
 * API: Destroy
 * ============================================================ */

void ode_solution_destroy(lvODESolution *sol) {
    if (!sol) return;
    free(sol->t_values);
    free(sol->y_values);
    free(sol);
}
