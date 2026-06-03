/**
 * @file fptaylor_eval.h
 * @brief FPTaylor-style floating-point error analysis for Lv-00
 *
 * @details Provides rigorous error bound analysis for floating-point expressions
 *          using Taylor expansion combined with interval arithmetic. Inspired by
 *          the FPTaylor tool (github.com/soarlab/FPTaylor) from the FPBench project.
 *
 *          The analysis decomposes a floating-point expression into:
 *          1. A real-valued expression (exact computation)
 *          2. Rounding errors at each operation
 *          3. Total error bound via Taylor expansion + interval evaluation
 *
 *          Design references:
 *          - FPTaylor (github.com/soarlab/FPTaylor) -- rigorous floating-point error analysis
 *          - FPBench (fpbench.org) -- standard benchmark suite for FP analysis
 *          - Gappa (gappa.gitlabpages.inria.fr) -- formal FP proof
 *
 * @version 3.4.0
 * @date 2026-05-25
 */

#ifndef LV00_FPTAYLOR_EVAL_H
#define LV00_FPTAYLOR_EVAL_H

#include <stdbool.h>
#include <stddef.h>
#include "lv00.h"
#include "interval_arithmetic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Taylor form structure
 * ======================================================================== */

/** Maximum number of variables in a Taylor form */
#define LV00_TAYLOR_MAX_VARS 32

/**
 * @brief First-order Taylor expansion of a function.
 *
 * Represents f(x) = center + sum_i deriv[i] * (x_i - x_i_center) + remainder
 * where the remainder is bounded by the interval [rem_lo, rem_hi].
 */
typedef struct {
    double center;                   /**< Center point value f(center) */
    double vars_center[LV00_TAYLOR_MAX_VARS]; /**< Center values for each variable */
    double derivs[LV00_TAYLOR_MAX_VARS];      /**< Partial derivatives df/dx_i */
    int var_count;                   /**< Number of variables */
    double rem_lo;                   /**< Remainder lower bound */
    double rem_hi;                   /**< Remainder upper bound */
    int order;                       /**< Taylor expansion order (1 or 2) */
} Lv00TaylorForm;

/* ========================================================================
 * Error bound structure
 * ======================================================================== */

/**
 * @brief Error bound result from FPTaylor analysis.
 */
typedef struct {
    double absolute_error;   /**< Absolute error upper bound */
    double relative_error;   /**< Relative error upper bound */
    double roundoff_error;   /**< Total roundoff error contribution */
    double truncation_error; /**< Truncation (Taylor remainder) error */
    int is_valid;            /**< Nonzero if the bound is valid */
    char proof_text[1024];   /**< Human-readable proof summary */
} Lv00ErrorBound;

/* ========================================================================
 * Configuration
 * ======================================================================== */

/**
 * @brief Configuration for FPTaylor error analysis.
 */
typedef struct {
    int taylor_order;            /**< Taylor expansion order (1 or 2) */
    double branch_threshold;     /**< Interval width threshold for bisection */
    int max_bisections;          /**< Maximum number of bisection steps */
    int enable_optimization;     /**< Enable affine relaxation optimization */
    double rounding_unit;        /**< Rounding unit (machine epsilon for target format) */
} Lv00FPTaylorConfig;

/* ========================================================================
 * Main API
 * ======================================================================== */

/**
 * @brief Evaluate the error bound of a floating-point expression.
 *
 * Parses the expression, computes its Taylor form, and evaluates
 * the error bound using interval arithmetic over the given variable ranges.
 *
 * Expression syntax:
 *   - Variables: x, y, z, x0, x1, etc.
 *   - Operators: +, -, *, /, ^
 *   - Functions: sqrt, sin, cos, exp, log, abs
 *   - Constants: pi, e
 *
 * @param expr         Expression string
 * @param var_names    Array of variable names
 * @param var_bounds   Array of variable intervals
 * @param var_count    Number of variables
 * @param config       Analysis configuration (NULL for defaults)
 * @param out          Output error bound
 * @return true if analysis succeeded, false on error.
 */
LV00_PUBLIC_API bool fptaylor_evaluate(
    const char *expr,
    const char **var_names,
    const Lv00Interval *var_bounds,
    int var_count,
    const Lv00FPTaylorConfig *config,
    Lv00ErrorBound *out);

/**
 * @brief Analyze a compound expression with multiple sub-expressions.
 *
 * Useful for analyzing multi-step computations where intermediate
 * results accumulate error.
 *
 * @param expr          Expression string (may contain multiple sub-expressions)
 * @param var_names     Array of variable names
 * @param var_bounds    Array of variable intervals
 * @param var_count     Number of variables
 * @param config        Analysis configuration (NULL for defaults)
 * @param out           Output error bound
 * @param taylor_out    Output Taylor form (NULL if not needed)
 * @return true if analysis succeeded, false on error.
 */
LV00_PUBLIC_API bool fptaylor_analyze_expression(
    const char *expr,
    const char **var_names,
    const Lv00Interval *var_bounds,
    int var_count,
    const Lv00FPTaylorConfig *config,
    Lv00ErrorBound *out,
    Lv00TaylorForm *taylor_out);

/**
 * @brief Get default FPTaylor configuration.
 */
LV00_PUBLIC_API Lv00FPTaylorConfig fptaylor_config_default(void);

/**
 * @brief Create a Taylor form for a simple arithmetic expression.
 *
 * @param expr         Expression string
 * @param var_names    Array of variable names
 * @param var_centers  Array of variable center values
 * @param var_count    Number of variables
 * @param order        Taylor expansion order
 * @param out          Output Taylor form
 * @return true if successful, false on error.
 */
LV00_PUBLIC_API bool fptaylor_taylor_form(
    const char *expr,
    const char **var_names,
    const double *var_centers,
    int var_count,
    int order,
    Lv00TaylorForm *out);

#ifdef __cplusplus
}
#endif

#endif /* LV00_FPTAYLOR_EVAL_H */
