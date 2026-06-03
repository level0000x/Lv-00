/**
 * @file interval_arithmetic.h
 * @brief Unified interval arithmetic interface for Lv-00
 *
 * @details Provides a standalone interval arithmetic module that merges design
 *          concepts from MPFI and FLINT/Arb. Uses double-based intervals
 *          without external library dependencies for basic operations.
 *
 *          All interval operations guarantee containment: the true result
 *          is always within the computed interval (conservativeness).
 *
 *          Design references:
 *          - MPFI (mpfi.org) -- multi-precision floating-point interval arithmetic
 *          - FLINT/Arb (fredrikj.net/arb) -- ball/interval arithmetic for rigorous numerics
 *          - IEEE 1788 -- standard for interval arithmetic
 *
 * @version 3.4.0
 * @date 2026-05-25
 */

#ifndef LV00_INTERVAL_ARITHMETIC_H
#define LV00_INTERVAL_ARITHMETIC_H

#include <stdbool.h>
#include <stddef.h>
#include "lv00.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Interval structure
 * ======================================================================== */

/**
 * @brief Represents a closed interval [lo, hi] on the real line.
 *
 * When lo > hi the interval is empty. When lo == hi and is_exact is true,
 * the interval represents a single exact value with no rounding error.
 */
typedef struct {
    double lo;       /**< Lower bound */
    double hi;       /**< Upper bound */
    int is_exact;    /**< Nonzero if the interval represents an exact value */
} Lv00Interval;

/* ========================================================================
 * Configuration
 * ======================================================================== */

/**
 * @brief Configuration for interval arithmetic operations.
 */
typedef struct {
    int precision;          /**< Working precision in bits (unused in double mode) */
    double rounding_eps;    /**< Epsilon for rounding outward to ensure containment */
} Lv00IntervalConfig;

/* ========================================================================
 * Factory functions
 * ======================================================================== */

/**
 * @brief Create an interval [lo, hi] with exactness flag.
 */
LV00_PUBLIC_API Lv00Interval interval_create(double lo, double hi, int is_exact);

/**
 * @brief Create an interval representing an exact point value.
 */
LV00_PUBLIC_API Lv00Interval interval_point(double val);

/**
 * @brief Create the empty interval (lo > hi).
 */
LV00_PUBLIC_API Lv00Interval interval_empty(void);

/**
 * @brief Create the entire real line (-inf, +inf).
 */
LV00_PUBLIC_API Lv00Interval interval_entire(void);

/**
 * @brief Create default configuration.
 */
LV00_PUBLIC_API Lv00IntervalConfig interval_config_default(void);

/* ========================================================================
 * Arithmetic operations
 * ======================================================================== */

/**
 * @brief Interval addition: [a,b] + [c,d] = [a+c, b+d]
 */
LV00_PUBLIC_API Lv00Interval interval_add(Lv00Interval a, Lv00Interval b);

/**
 * @brief Interval subtraction: [a,b] - [c,d] = [a-d, b-c]
 */
LV00_PUBLIC_API Lv00Interval interval_sub(Lv00Interval a, Lv00Interval b);

/**
 * @brief Interval multiplication: [a,b] * [c,d] = [min(S), max(S)]
 *        where S = {ac, ad, bc, bd}
 */
LV00_PUBLIC_API Lv00Interval interval_mul(Lv00Interval a, Lv00Interval b);

/**
 * @brief Interval division: [a,b] / [c,d]
 *
 * Returns empty interval if 0 is in [c,d].
 */
LV00_PUBLIC_API Lv00Interval interval_div(Lv00Interval a, Lv00Interval b);

/**
 * @brief Interval square root: sqrt([lo, hi])
 *
 * Requires lo >= 0. Returns empty interval if lo < 0.
 */
LV00_PUBLIC_API Lv00Interval interval_sqrt(Lv00Interval a);

/**
 * @brief Interval sine: sin([lo, hi])
 *
 * Handles non-monotonicity by checking for critical points.
 */
LV00_PUBLIC_API Lv00Interval interval_sin(Lv00Interval a);

/**
 * @brief Interval cosine: cos([lo, hi])
 *
 * Handles non-monotonicity by checking for critical points.
 */
LV00_PUBLIC_API Lv00Interval interval_cos(Lv00Interval a);

/**
 * @brief Interval exponential: exp([lo, hi])
 *
 * Monotone increasing: exp([lo, hi]) = [exp(lo), exp(hi)]
 */
LV00_PUBLIC_API Lv00Interval interval_exp(Lv00Interval a);

/**
 * @brief Interval natural logarithm: log([lo, hi])
 *
 * Requires lo > 0. Returns empty interval if lo <= 0.
 */
LV00_PUBLIC_API Lv00Interval interval_log(Lv00Interval a);

/**
 * @brief Interval absolute value: |[lo, hi]|
 */
LV00_PUBLIC_API Lv00Interval interval_abs(Lv00Interval a);

/**
 * @brief Interval negation: -[lo, hi] = [-hi, -lo]
 */
LV00_PUBLIC_API Lv00Interval interval_neg(Lv00Interval a);

/* ========================================================================
 * Properties
 * ======================================================================== */

/**
 * @brief Diameter (width) of the interval: hi - lo.
 *
 * Returns 0.0 for empty intervals.
 */
LV00_PUBLIC_API double interval_diam(Lv00Interval a);

/**
 * @brief Midpoint of the interval: (lo + hi) / 2.
 *
 * Returns NaN for empty intervals.
 */
LV00_PUBLIC_API double interval_mid(Lv00Interval a);

/**
 * @brief Check if the interval is empty (lo > hi).
 */
LV00_PUBLIC_API int interval_is_empty(Lv00Interval a);

/**
 * @brief Check if the interval contains a given value.
 */
LV00_PUBLIC_API int interval_contains(Lv00Interval a, double val);

/**
 * @brief Check if interval a is a subset of interval b.
 */
LV00_PUBLIC_API int interval_is_subset(Lv00Interval a, Lv00Interval b);

/**
 * @brief Check if two intervals are equal.
 */
LV00_PUBLIC_API int interval_equals(Lv00Interval a, Lv00Interval b);

/* ========================================================================
 * Set operations
 * ======================================================================== */

/**
 * @brief Intersection of two intervals.
 *
 * Returns empty interval if the intervals do not overlap.
 */
LV00_PUBLIC_API Lv00Interval interval_intersect(Lv00Interval a, Lv00Interval b);

/**
 * @brief Convex hull (union) of two intervals.
 */
LV00_PUBLIC_API Lv00Interval interval_union(Lv00Interval a, Lv00Interval b);

/* ========================================================================
 * Symbolic coordinate integration
 * ======================================================================== */

/**
 * @brief Create an interval from a symbolic coordinate expression.
 *
 * Evaluates the symbolic expression and returns its interval enclosure.
 *
 * @param expr_str  Symbolic expression string (e.g., "x + y")
 * @param var_names Array of variable name strings
 * @param var_intervals Array of variable intervals
 * @param var_count Number of variables
 * @return Interval enclosure of the expression, or empty interval on error.
 */
LV00_PUBLIC_API Lv00Interval interval_from_symbolic(
    const char *expr_str,
    const char **var_names,
    const Lv00Interval *var_intervals,
    int var_count);

/**
 * @brief Convert an interval to a symbolic coordinate string representation.
 *
 * @param a         The interval
 * @param buf       Output buffer
 * @param buf_size  Size of output buffer
 * @return Number of characters written (excluding null terminator), or -1 on error.
 */
LV00_PUBLIC_API int interval_to_symbolic(Lv00Interval a, char *buf, size_t buf_size);

/* ========================================================================
 * Verification functions
 * ======================================================================== */

/**
 * @brief Verify that a solution satisfies a constraint within an interval.
 *
 * Given a constraint f(x) = 0 and an interval enclosure for f(x),
 * checks whether 0 is contained in the enclosure.
 *
 * @param f_interval  Interval enclosure of f(x)
 * @param tolerance   Verification tolerance
 * @return 1 if verified (0 is in f_interval within tolerance),
 *         0 if not verified, -1 on error.
 */
LV00_PUBLIC_API int interval_verify_solution(Lv00Interval f_interval, double tolerance);

/**
 * @brief Adaptive verification with bisection refinement.
 *
 * Repeatedly bisects the input intervals and re-evaluates the constraint
 * until either the solution is verified or the maximum depth is reached.
 *
 * @param expr_str       Constraint expression string
 * @param var_names      Array of variable name strings
 * @param var_intervals  Array of variable intervals (modified in place)
 * @param var_count      Number of variables
 * @param max_depth      Maximum bisection depth
 * @param tolerance      Verification tolerance
 * @return 1 if verified, 0 if not verified, -1 on error.
 */
LV00_PUBLIC_API int interval_verify_adaptive(
    const char *expr_str,
    const char **var_names,
    Lv00Interval *var_intervals,
    int var_count,
    int max_depth,
    double tolerance);

#ifdef __cplusplus
}
#endif

#endif /* LV00_INTERVAL_ARITHMETIC_H */
