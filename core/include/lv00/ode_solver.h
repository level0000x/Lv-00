/**
 * @file ode_solver.h
 * @brief Ordinary Differential Equation (ODE) solver module.
 *
 * @details Provides numerical methods for solving initial value problems
 *          of the form dy/dt = f(t, y). Inspired by DifferentialEquations.jl,
 *          SUNDIALS (CVODE), and IPOPT.
 *
 *          Supported methods:
 *          - Euler (explicit forward Euler)
 *          - RK4 (classical 4th-order Runge-Kutta)
 *          - Adams (placeholder for Adams-Bashforth multistep)
 *
 *          The solver supports configurable tolerances (rtol, atol) and
 *          maximum step counts to prevent runaway integration.
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#ifndef LV00_ODE_SOLVER_H
#define LV00_ODE_SOLVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv00.h"

/* ============================================================
 * ODE method enumeration
 * ============================================================ */

/**
 * @brief Numerical integration methods for ODE solving.
 */
typedef enum Lv00ODEMethod {
    ODE_EULER = 0,   /**< Explicit forward Euler method (1st order) */
    ODE_RK4   = 1,   /**< Classical 4th-order Runge-Kutta */
    ODE_ADAMS = 2    /**< Adams-Bashforth multistep (placeholder) */
} Lv00ODEMethod;

/* ============================================================
 * ODE problem definition
 * ============================================================ */

/**
 * @brief Right-hand-side function signature: dy/dt = rhs(t, y, params).
 *
 * @param t      Current time
 * @param y      Current state vector (size = dim)
 * @param params User-provided parameter block (may be NULL)
 * @param[out] dydt  Output derivative vector (caller-allocated, size = dim)
 */
typedef void (*Lv00ODE rhs_fn)(double t, const double *y, void *params, double *dydt);

/**
 * @brief Describes an ODE initial value problem.
 *
 * @note y0 is owned by the caller and must remain valid for the lifetime
 *       of the problem. params may be NULL if not needed.
 */
typedef struct Lv00ODEProblem {
    Lv00ODE rhs_fn rhs_fn; /**< Right-hand-side function dy/dt = f(t,y) */
    double         *y0;    /**< Initial state vector (size = dim) */
    size_t          dim;   /**< Dimension of the state vector */
    double          t_span[2]; /**< Integration interval [t_start, t_end] */
    void           *params;    /**< User parameters passed to rhs_fn */
} Lv00ODEProblem;

/* ============================================================
 * ODE solver configuration
 * ============================================================ */

/**
 * @brief Configuration for the ODE solver.
 */
typedef struct Lv00ODEConfig {
    Lv00ODEMethod method;    /**< Integration method */
    double        dt;        /**< Fixed time step size */
    size_t        max_steps; /**< Maximum number of steps */
    double        rtol;      /**< Relative tolerance (for adaptive, reserved) */
    double        atol;      /**< Absolute tolerance (for adaptive, reserved) */
} Lv00ODEConfig;

/* ============================================================
 * ODE solution
 * ============================================================ */

/**
 * @brief Stores the solution trajectory of an ODE integration.
 *
 * t_values and y_values are flat arrays. y_values stores row-major
 * state vectors: y_values[i * dim + j] is the j-th component at step i.
 */
typedef struct Lv00ODESolution {
    double *t_values;  /**< Time points (size = n_steps) */
    double *y_values;  /**< State vectors (size = n_steps * dim) */
    size_t  n_steps;   /**< Number of time steps stored */
    size_t  dim;       /**< Dimension of the state vector */
} Lv00ODESolution;

/* ============================================================
 * API: Solve
 * ============================================================ */

/**
 * @brief Solve an ODE initial value problem.
 *
 * @param problem  The ODE problem definition
 * @param config   Solver configuration
 * @return Pointer to the solution, or NULL on failure
 */
LV00_PUBLIC_API Lv00ODESolution *ode_solve(const Lv00ODEProblem *problem,
                                           const Lv00ODEConfig  *config);

/* ============================================================
 * API: Destroy
 * ============================================================ */

/**
 * @brief Destroy an ODE solution and free all associated memory.
 *
 * @param sol  The solution to destroy (may be NULL)
 */
LV00_PUBLIC_API void ode_solution_destroy(Lv00ODESolution *sol);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ODE_SOLVER_H */
