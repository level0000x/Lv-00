/**
 * @file gappa_dsl.h
 * @brief Floating-point proof DSL inspired by Gappa
 *
 * @details Provides a domain-specific language for specifying and proving
 *          properties of floating-point computations. Inspired by the Gappa
 *          tool (gappa.gitlabpages.inria.fr) for automated floating-point
 *          proof generation.
 *
 *          The DSL supports:
 *          - Specifying floating-point formats (binary32, binary64, etc.)
 *          - Declaring hypotheses (variable ranges)
 *          - Stating proof goals (predicates to prove)
 *          - Automated proof via interval propagation
 *
 *          Design references:
 *          - Gappa (gappa.gitlabpages.inria.fr) -- formal proof of floating-point programs
 *          - IEEE 754 -- floating-point arithmetic standard
 *
 * @version 3.4.0
 * @date 2026-05-25
 */

#ifndef LV00_GAPPA_DSL_H
#define LV00_GAPPA_DSL_H

#include <stdbool.h>
#include <stddef.h>
#include "lv00.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Rounding mode enumeration
 * ======================================================================== */

/**
 * @brief Rounding modes for floating-point operations.
 *
 * Corresponds to IEEE 754 rounding direction attributes.
 */
typedef enum {
    LV00_ROUND_NE = 0,  /**< Round to nearest, ties to even */
    LV00_ROUND_NA,      /**< Round to nearest, ties away from zero */
    LV00_ROUND_ZR,      /**< Round toward zero (truncation) */
    LV00_ROUND_DN,      /**< Round toward -infinity (floor) */
    LV00_ROUND_UP,      /**< Round toward +infinity (ceiling) */
    LV00_ROUND_COUNT    /**< Number of rounding modes */
} Lv00GappaRounding;

/* ========================================================================
 * Floating-point format
 * ======================================================================== */

/**
 * @brief Description of a floating-point format.
 */
typedef struct {
    int precision_bits;      /**< Total precision in bits (e.g., 24 for binary32) */
    int exponent_bits;       /**< Number of exponent bits (e.g., 8 for binary32) */
    Lv00GappaRounding rounding; /**< Default rounding mode */
    char name[64];           /**< Format name (e.g., "binary32") */
} Lv00GappaFormat;

/* ========================================================================
 * Predicate types
 * ======================================================================== */

/**
 * @brief Types of predicates that can be stated in the Gappa DSL.
 */
typedef enum {
    LV00_PRED_BND = 0,  /**< Bounded: x in [lo, hi] */
    LV00_PRED_ABS,      /**< Absolute value: |x| <= bound */
    LV00_PRED_REL,      /**< Relative error: |x - y| / |y| <= bound */
    LV00_PRED_LIN,      /**< Linear combination: a*x + b*y + ... in [lo, hi] */
    LV00_PRED_FIX,      /**< Fixed-point: x = exact value */
    LV00_PRED_FLT,      /**< Floating-point: x = round(exact) */
    LV00_PRED_NZR,      /**< Non-zero: |x| > bound */
    LV00_PRED_EQL       /**< Equality: x = y */
} Lv00GappaPredType;

/* ========================================================================
 * Predicate structure
 * ======================================================================== */

/**
 * @brief A predicate in the Gappa DSL.
 *
 * Represents a property such as "x in [0, 1]" or "|x - y| <= 0.5".
 */
typedef struct {
    Lv00GappaPredType type;   /**< Predicate type */
    char expr_lhs[256];       /**< Left-hand side expression */
    char expr_rhs[256];       /**< Right-hand side expression (for REL, EQL) */
    double bound_lo;          /**< Lower bound (for BND, LIN) */
    double bound_hi;          /**< Upper bound (for BND, LIN) */
    double bound_abs;         /**< Absolute bound (for ABS, NZR) */
    double bound_rel;         /**< Relative bound (for REL) */
    int is_hypothesis;        /**< Nonzero if this is a hypothesis, 0 if goal */
} Lv00GappaPredicate;

/* ========================================================================
 * Proof goal
 * ======================================================================== */

/**
 * @brief A proof goal to be established.
 */
typedef struct {
    Lv00GappaPredicate predicate;  /**< The predicate to prove */
    char description[256];         /**< Human-readable description */
    int is_proven;                 /**< Nonzero if the goal has been proven */
    char proof_method[128];        /**< Method used to prove (e.g., "interval_propagation") */
} Lv00GappaProofGoal;

/* ========================================================================
 * Proof result
 * ======================================================================== */

/**
 * @brief Result of a Gappa proof attempt.
 */
typedef struct {
    int success;                   /**< Nonzero if all goals were proven */
    int goals_total;               /**< Total number of goals */
    int goals_proven;              /**< Number of goals successfully proven */
    int goals_failed;              /**< Number of goals that could not be proven */
    char error_message[512];       /**< Error message if proof failed */
    Lv00GappaProofGoal *goals;     /**< Array of goal results (caller must free) */
} Lv00GappaProofResult;

/* ========================================================================
 * Predefined formats
 * ======================================================================== */

/**
 * @brief Get a predefined floating-point format by name.
 *
 * Supported names: "binary16", "binary32", "binary64", "binary128"
 *
 * @param name  Format name
 * @param out   Output format structure
 * @return true if found, false if name is not recognized.
 */
LV00_PUBLIC_API bool gappa_format_predefined(const char *name, Lv00GappaFormat *out);

/* ========================================================================
 * DSL parsing
 * ======================================================================== */

/**
 * @brief Parse a Gappa DSL string into hypotheses and goals.
 *
 * Supported syntax:
 *   "x in [0, 1]"                          -- hypothesis
 *   "y in [-1, 1]"                         -- hypothesis
 *   "|x - 0.5| <= 0.5 -> |y| <= 1"         -- hypothesis implies goal
 *   "x in [0, 1] -> |x - 0.5| <= 0.5"     -- hypothesis implies goal
 *
 * Hypotheses are separated from goals by "->".
 * Multiple statements can be separated by semicolons.
 *
 * @param dsl_string   The DSL string to parse
 * @param hypotheses   Output array of hypotheses (caller must free)
 * @param hyp_count    Output: number of hypotheses
 * @param goals        Output array of goals (caller must free)
 * @param goal_count   Output: number of goals
 * @return true if parsing succeeded, false on syntax error.
 */
LV00_PUBLIC_API bool gappa_parse(
    const char *dsl_string,
    Lv00GappaPredicate **hypotheses,
    int *hyp_count,
    Lv00GappaProofGoal **goals,
    int *goal_count);

/* ========================================================================
 * Proof engine
 * ======================================================================== */

/**
 * @brief Attempt to prove goals from hypotheses using interval propagation.
 *
 * @param hypotheses   Array of hypothesis predicates
 * @param hyp_count    Number of hypotheses
 * @param goals        Array of proof goals (modified in place)
 * @param goal_count   Number of goals
 * @param fmt          Floating-point format to use (NULL for binary64)
 * @return Proof result structure (caller must free via gappa_result_free).
 */
LV00_PUBLIC_API Lv00GappaProofResult gappa_prove(
    const Lv00GappaPredicate *hypotheses,
    int hyp_count,
    Lv00GappaProofGoal *goals,
    int goal_count,
    const Lv00GappaFormat *fmt);

/**
 * @brief Free resources held by a proof result.
 */
LV00_PUBLIC_API void gappa_result_free(Lv00GappaProofResult *result);

/**
 * @brief Free an array of predicates.
 */
LV00_PUBLIC_API void gappa_predicates_free(Lv00GappaPredicate *preds, int count);

/**
 * @brief Free an array of proof goals.
 */
LV00_PUBLIC_API void gappa_goals_free(Lv00GappaProofGoal *goals, int count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GAPPA_DSL_H */
