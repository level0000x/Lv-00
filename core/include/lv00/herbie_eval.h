/**
 * @file herbie_eval.h
 * @brief Floating-point precision evaluation inspired by Herbie
 *
 * @details Provides tools for evaluating and improving the floating-point
 *          accuracy of mathematical expressions. Inspired by the Herbie tool
 *          (herbie.uwplse.org) which automatically improves floating-point
 *          accuracy by searching for alternative expression forms.
 *
 *          Features:
 *          - Sampling-based accuracy evaluation over input domains
 *          - Bit-error computation (number of incorrect bits in the result)
 *          - AMBER scoring (Accuracy Measure Based on Error Range)
 *          - Input domain partitioning for regime-aware evaluation
 *          - Path selection for choosing the most accurate expression variant
 *
 *          Design references:
 *          - Herbie (herbie.uwplse.org) -- automatic floating-point improvement
 *          - FPBench (fpbench.org) -- floating-point benchmarking
 *          - Rosa (github.com/soarlab/Rosa) -- range analysis for FP
 *
 * @version 3.4.0
 * @date 2026-05-25
 */

#ifndef LV00_HERBIE_EVAL_H
#define LV00_HERBIE_EVAL_H

#include <stdbool.h>
#include <stddef.h>
#include "lv00.h"
#include "interval_arithmetic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Result structures
 * ======================================================================== */

/** Maximum number of samples */
#define LV00_HERBIE_MAX_SAMPLES 10000

/** Maximum number of regimes */
#define LV00_HERBIE_MAX_REGIMES 16

/** 最大变量数（与 fptaylor_eval.h 保持一致） */
#ifndef LV00_TAYLOR_MAX_VARS
#define LV00_TAYLOR_MAX_VARS 32
#endif

/** Maximum number of expression paths */
#define LV00_HERBIE_MAX_PATHS 32

/**
 * @brief Result of evaluating a single expression variant.
 */
typedef struct {
    char expression[256];          /**< The expression string */
    double max_bit_error;          /**< Maximum bit error across all samples */
    double avg_bit_error;          /**< Average bit error across all samples */
    double max_relative_error;     /**< Maximum relative error */
    double avg_relative_error;     /**< Average relative error */
    double amber_score;            /**< AMBER accuracy score (0.0 = worst, 1.0 = best) */
    int sample_count;              /**< Number of samples evaluated */
    int valid_samples;             /**< Number of valid (non-NaN, non-Inf) samples */
} Lv00HerbieResult;

/**
 * @brief An input regime (sub-domain) for partitioned evaluation.
 */
typedef struct {
    Lv00Interval bounds[LV00_TAYLOR_MAX_VARS]; /**< Variable bounds for this regime */
    int var_count;                              /**< Number of variables */
    double weight;                              /**< Relative weight of this regime */
    char description[128];                      /**< Human-readable description */
} Lv00HerbieRegime;

/**
 * @brief Result of regime partitioning.
 */
typedef struct {
    Lv00HerbieRegime regimes[LV00_HERBIE_MAX_REGIMES]; /**< Array of regimes */
    int regime_count;                                   /**< Number of regimes */
    double total_weight;                                /**< Sum of all weights */
} Lv00HerbiePartitionResult;

/* ========================================================================
 * Configuration
 * ======================================================================== */

/**
 * @brief Configuration for Herbie-style evaluation.
 */
typedef struct {
    int sample_count;           /**< Number of random samples per evaluation */
    unsigned int random_seed;   /**< Random seed for reproducibility (0 = use time) */
    double amber_alpha;         /**< AMBER alpha parameter (default 0.5) */
    double amber_beta;          /**< AMBER beta parameter (default 2.0) */
    int enable_regime_detection; /**< Enable automatic regime detection */
    double regime_threshold;    /**< Threshold for regime boundary detection */
} Lv00HerbieConfig;

/* ========================================================================
 * Main API
 * ======================================================================== */

/**
 * @brief Evaluate the floating-point accuracy of an expression.
 *
 * Samples the expression over the given input domain and computes
 * accuracy metrics including bit error and AMBER score.
 *
 * @param expr         Expression string (e.g., "x*x + y*y")
 * @param var_names    Array of variable names
 * @param var_bounds   Array of variable intervals (input domain)
 * @param var_count    Number of variables
 * @param config       Evaluation configuration (NULL for defaults)
 * @param out          Output result
 * @return true if evaluation succeeded, false on error.
 */
LV00_PUBLIC_API bool herbie_evaluate(
    const char *expr,
    const char **var_names,
    const Lv00Interval *var_bounds,
    int var_count,
    const Lv00HerbieConfig *config,
    Lv00HerbieResult *out);

/**
 * @brief Evaluate and compare multiple expression variants.
 *
 * Computes accuracy metrics for each expression and identifies
 * the most accurate variant.
 *
 * @param exprs        Array of expression strings
 * @param expr_count   Number of expressions
 * @param var_names    Array of variable names
 * @param var_bounds   Array of variable intervals
 * @param var_count    Number of variables
 * @param config       Evaluation configuration (NULL for defaults)
 * @param results      Output array of results (size >= expr_count)
 * @param best_index   Output: index of the best expression (NULL if not needed)
 * @return true if evaluation succeeded, false on error.
 */
LV00_PUBLIC_API bool herbie_compare(
    const char **exprs,
    int expr_count,
    const char **var_names,
    const Lv00Interval *var_bounds,
    int var_count,
    const Lv00HerbieConfig *config,
    Lv00HerbieResult *results,
    int *best_index);

/**
 * @brief Partition the input domain into regimes based on error behavior.
 *
 * Identifies sub-domains where different expression variants are more
 * accurate, enabling regime-aware expression selection.
 *
 * @param expr         Expression to analyze
 * @param var_names    Array of variable names
 * @param var_bounds   Array of variable intervals (full domain)
 * @param var_count    Number of variables
 * @param config       Evaluation configuration (NULL for defaults)
 * @param out          Output partition result
 * @return true if partitioning succeeded, false on error.
 */
LV00_PUBLIC_API bool herbie_partition_regimes(
    const char *expr,
    const char **var_names,
    const Lv00Interval *var_bounds,
    int var_count,
    const Lv00HerbieConfig *config,
    Lv00HerbiePartitionResult *out);

/**
 * @brief Select the best expression variant for each regime.
 *
 * Given a set of expression variants and a partitioned domain,
 * selects the best expression for each regime.
 *
 * @param exprs        Array of expression strings
 * @param expr_count   Number of expressions
 * @param var_names    Array of variable names
 * @param partition    Domain partition from herbie_partition_regimes
 * @param var_count    Number of variables
 * @param config       Evaluation configuration (NULL for defaults)
 * @param best_indices Output: best expression index for each regime (size >= partition.regime_count)
 * @return true if selection succeeded, false on error.
 */
LV00_PUBLIC_API bool herbie_select_path(
    const char **exprs,
    int expr_count,
    const char **var_names,
    const Lv00HerbiePartitionResult *partition,
    int var_count,
    const Lv00HerbieConfig *config,
    int *best_indices);

/**
 * @brief Get default Herbie evaluation configuration.
 */
LV00_PUBLIC_API Lv00HerbieConfig herbie_config_default(void);

/**
 * @brief Compute the AMBER score for a set of errors.
 *
 * AMBER (Accuracy Measure Based on Error Range) provides a normalized
 * accuracy score in [0, 1] where 1.0 means perfect accuracy.
 *
 * @param errors       Array of absolute errors
 * @param error_count  Number of errors
 * @param alpha        AMBER alpha parameter (controls sensitivity)
 * @param beta         AMBER beta parameter (controls scaling)
 * @return AMBER score in [0, 1].
 */
LV00_PUBLIC_API double herbie_compute_amber(
    const double *errors,
    int error_count,
    double alpha,
    double beta);

#ifdef __cplusplus
}
#endif

#endif /* LV00_HERBIE_EVAL_H */
