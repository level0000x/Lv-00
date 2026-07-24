/**
 * @file test_ode_solver.c
 * @brief Tests for the ODE solver module.
 *
 * @details Tests cover:
 *          - Euler method: dy/dt = -y (exponential decay)
 *          - RK4 method: dy/dt = -y (exponential decay)
 *          - Euler method: dy/dt = -2y with different initial conditions
 *          - RK4 vs Euler accuracy comparison
 *          - NULL safety
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

/* [QA] Uses double for test assertions against GMP mpq_t via comparison helpers. Acceptable in test code. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "ode_solver.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/** Tolerance for floating-point comparisons */
#define ODE_TOLERANCE 1e-6

/**
 * @brief Assert that two doubles are approximately equal.
 */
#define TEST_ASSERT_NEAR(actual, expected, tol, msg)                                                              \
    do {                                                                                                          \
        double _ode_actual = (double) (actual);                                                                   \
        double _ode_expected = (double) (expected);                                                               \
        double _ode_diff = _ode_actual - _ode_expected;                                                           \
        if (_ode_diff < 0.0)                                                                                      \
            _ode_diff = -_ode_diff;                                                                               \
        if (_ode_diff > (tol)) {                                                                                  \
            fprintf(stderr, "  FAIL [%s:%d] %s (actual=%.12f, expected=%.12f, diff=%.12e)\n", __FILE__, __LINE__, \
                    (msg), _ode_actual, _ode_expected, _ode_diff);                                                \
            g_fail_count++;                                                                                       \
            return;                                                                                               \
        }                                                                                                         \
        g_pass_count++;                                                                                           \
    } while (0)

/* ============================================================
 * RHS functions for test problems
 * ============================================================ */

/** dy/dt = -y (exponential decay, exact solution: y = y0 * exp(-t)) */
static void rhs_decay(double t, const double *y, void *params, double *dydt) {
    (void) t;
    (void) params;
    dydt[0] = -y[0];
}

/** dy/dt = -2y (faster decay, exact solution: y = y0 * exp(-2t)) */
static void rhs_fast_decay(double t, const double *y, void *params, double *dydt) {
    (void) t;
    (void) params;
    dydt[0] = -2.0 * y[0];
}

/* ============================================================
 * Test: Euler method on dy/dt = -y
 * ============================================================ */

static void test_euler_exponential_decay(void) {
    lvODEProblem problem;
    problem.rhs_fn = rhs_decay;
    problem.dim = 1;
    problem.y0 = (double[]) {1.0};
    problem.t_span[0] = 0.0;
    problem.t_span[1] = 1.0;
    problem.params = NULL;

    lvODEConfig config;
    config.method = ODE_EULER;
    config.dt = 0.01;
    config.max_steps = 10000;
    config.rtol = 1e-6;
    config.atol = 1e-9;

    lvODESolution *sol = ode_solve(&problem, &config);
    TEST_ASSERT_NOT_NULL(sol);
    TEST_ASSERT(sol->n_steps > 0, "solution should have steps");

    /* At t=1.0, exact solution is exp(-1) ~ 0.367879 */
    double y_final = sol->y_values[(sol->n_steps - 1) * sol->dim];
    double exact = exp(-1.0);
    /* Euler is only 1st order, so allow larger tolerance */
    TEST_ASSERT_NEAR(y_final, exact, 0.01, "Euler: y(1) should be ~exp(-1)");

    /* At t=0.0, y should be 1.0 */
    TEST_ASSERT_NEAR(sol->y_values[0], 1.0, 1e-12, "Euler: y(0) should be 1.0");

    ode_solution_destroy(sol);
}

/* ============================================================
 * Test: RK4 method on dy/dt = -y
 * ============================================================ */

static void test_rk4_exponential_decay(void) {
    lvODEProblem problem;
    problem.rhs_fn = rhs_decay;
    problem.dim = 1;
    problem.y0 = (double[]) {1.0};
    problem.t_span[0] = 0.0;
    problem.t_span[1] = 1.0;
    problem.params = NULL;

    lvODEConfig config;
    config.method = ODE_RK4;
    config.dt = 0.01;
    config.max_steps = 10000;
    config.rtol = 1e-6;
    config.atol = 1e-9;

    lvODESolution *sol = ode_solve(&problem, &config);
    TEST_ASSERT_NOT_NULL(sol);
    TEST_ASSERT(sol->n_steps > 0, "solution should have steps");

    /* At t=1.0, exact solution is exp(-1) ~ 0.367879 */
    double y_final = sol->y_values[(sol->n_steps - 1) * sol->dim];
    double exact = exp(-1.0);
    /* RK4 is 4th order, should be very accurate */
    TEST_ASSERT_NEAR(y_final, exact, 1e-8, "RK4: y(1) should be ~exp(-1)");

    /* At t=0.0, y should be 1.0 */
    TEST_ASSERT_NEAR(sol->y_values[0], 1.0, 1e-12, "RK4: y(0) should be 1.0");

    ode_solution_destroy(sol);
}

/* ============================================================
 * Test: Euler method on dy/dt = -2y
 * ============================================================ */

static void test_euler_fast_decay(void) {
    lvODEProblem problem;
    problem.rhs_fn = rhs_fast_decay;
    problem.dim = 1;
    problem.y0 = (double[]) {5.0};
    problem.t_span[0] = 0.0;
    problem.t_span[1] = 2.0;
    problem.params = NULL;

    lvODEConfig config;
    config.method = ODE_EULER;
    config.dt = 0.005;
    config.max_steps = 10000;
    config.rtol = 1e-6;
    config.atol = 1e-9;

    lvODESolution *sol = ode_solve(&problem, &config);
    TEST_ASSERT_NOT_NULL(sol);

    /* At t=2.0, exact solution is 5 * exp(-4) ~ 0.091578 */
    double y_final = sol->y_values[(sol->n_steps - 1) * sol->dim];
    double exact = 5.0 * exp(-4.0);
    TEST_ASSERT_NEAR(y_final, exact, 0.05, "Euler: y(2) for -2y should be ~5*exp(-4)");

    ode_solution_destroy(sol);
}

/* ============================================================
 * Test: RK4 method on dy/dt = -2y
 * ============================================================ */

static void test_rk4_fast_decay(void) {
    lvODEProblem problem;
    problem.rhs_fn = rhs_fast_decay;
    problem.dim = 1;
    problem.y0 = (double[]) {5.0};
    problem.t_span[0] = 0.0;
    problem.t_span[1] = 2.0;
    problem.params = NULL;

    lvODEConfig config;
    config.method = ODE_RK4;
    config.dt = 0.005;
    config.max_steps = 10000;
    config.rtol = 1e-6;
    config.atol = 1e-9;

    lvODESolution *sol = ode_solve(&problem, &config);
    TEST_ASSERT_NOT_NULL(sol);

    /* At t=2.0, exact solution is 5 * exp(-4) ~ 0.091578 */
    double y_final = sol->y_values[(sol->n_steps - 1) * sol->dim];
    double exact = 5.0 * exp(-4.0);
    TEST_ASSERT_NEAR(y_final, exact, 1e-6, "RK4: y(2) for -2y should be ~5*exp(-4)");

    ode_solution_destroy(sol);
}

/* ============================================================
 * Test: RK4 is more accurate than Euler
 * ============================================================ */

static void test_rk4_more_accurate_than_euler(void) {
    lvODEProblem problem;
    problem.rhs_fn = rhs_decay;
    problem.dim = 1;
    problem.y0 = (double[]) {1.0};
    problem.t_span[0] = 0.0;
    problem.t_span[1] = 1.0;
    problem.params = NULL;

    lvODEConfig config;
    config.dt = 0.1;
    config.max_steps = 10000;
    config.rtol = 1e-6;
    config.atol = 1e-9;

    /* Euler */
    config.method = ODE_EULER;
    lvODESolution *sol_euler = ode_solve(&problem, &config);
    TEST_ASSERT_NOT_NULL(sol_euler);

    /* RK4 */
    config.method = ODE_RK4;
    lvODESolution *sol_rk4 = ode_solve(&problem, &config);
    TEST_ASSERT_NOT_NULL(sol_rk4);

    double exact = exp(-1.0);
    double y_euler = sol_euler->y_values[(sol_euler->n_steps - 1) * sol_euler->dim];
    double y_rk4 = sol_rk4->y_values[(sol_rk4->n_steps - 1) * sol_rk4->dim];

    double err_euler = fabs(y_euler - exact);
    double err_rk4 = fabs(y_rk4 - exact);

    /* RK4 should have smaller error */
    TEST_ASSERT(err_rk4 < err_euler, "RK4 should be more accurate than Euler");

    ode_solution_destroy(sol_euler);
    ode_solution_destroy(sol_rk4);
}

/* ============================================================
 * Test: NULL safety
 * ============================================================ */

static void test_ode_null_safety(void) {
    lvODEProblem problem;
    problem.rhs_fn = rhs_decay;
    problem.dim = 1;
    problem.y0 = (double[]) {1.0};
    problem.t_span[0] = 0.0;
    problem.t_span[1] = 1.0;
    problem.params = NULL;

    lvODEConfig config;
    config.method = ODE_EULER;
    config.dt = 0.01;
    config.max_steps = 10000;
    config.rtol = 1e-6;
    config.atol = 1e-9;

    /* NULL problem */
    lvODESolution *sol = ode_solve(NULL, &config);
    TEST_ASSERT_NULL(sol);

    /* NULL config */
    sol = ode_solve(&problem, NULL);
    TEST_ASSERT_NULL(sol);

    /* NULL rhs_fn */
    lvODEProblem bad_problem = problem;
    bad_problem.rhs_fn = NULL;
    sol = ode_solve(&bad_problem, &config);
    TEST_ASSERT_NULL(sol);

    /* NULL y0 */
    bad_problem = problem;
    bad_problem.y0 = NULL;
    sol = ode_solve(&bad_problem, &config);
    TEST_ASSERT_NULL(sol);

    /* Zero dimension */
    bad_problem = problem;
    bad_problem.dim = 0;
    sol = ode_solve(&bad_problem, &config);
    TEST_ASSERT_NULL(sol);

    /* NULL-safe destroy */
    ode_solution_destroy(NULL);
}

/* ============================================================
 * Test: Solution dimensions are correct
 * ============================================================ */

static void test_solution_dimensions(void) {
    lvODEProblem problem;
    problem.rhs_fn = rhs_decay;
    problem.dim = 1;
    problem.y0 = (double[]) {1.0};
    problem.t_span[0] = 0.0;
    problem.t_span[1] = 1.0;
    problem.params = NULL;

    lvODEConfig config;
    config.method = ODE_EULER;
    config.dt = 0.1;
    config.max_steps = 10000;
    config.rtol = 1e-6;
    config.atol = 1e-9;

    lvODESolution *sol = ode_solve(&problem, &config);
    TEST_ASSERT_NOT_NULL(sol);
    TEST_ASSERT_EQ(sol->dim, 1);
    /* dt=0.1, interval=1.0 => 10 steps + initial = 11 entries */
    TEST_ASSERT_EQ(sol->n_steps, 11);
    TEST_ASSERT_NEAR(sol->t_values[0], 0.0, 1e-12, "first time should be 0");
    TEST_ASSERT_NEAR(sol->t_values[sol->n_steps - 1], 1.0, 1e-12, "last time should be 1");

    ode_solution_destroy(sol);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void) {
    TEST_SUITE_BEGIN("ODE Solver");

    TEST_RUN(test_euler_exponential_decay);
    TEST_RUN(test_rk4_exponential_decay);
    TEST_RUN(test_euler_fast_decay);
    TEST_RUN(test_rk4_fast_decay);
    TEST_RUN(test_rk4_more_accurate_than_euler);
    TEST_RUN(test_ode_null_safety);
    TEST_RUN(test_solution_dimensions);

    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}
