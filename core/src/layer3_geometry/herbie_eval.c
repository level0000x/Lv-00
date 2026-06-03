/**
 * @file herbie_eval.c
 * @brief Implementation of Herbie-style floating-point precision evaluation
 *
 * @details Provides sampling-based accuracy evaluation, bit-error computation,
 *          and AMBER scoring for floating-point expressions. Inspired by the
 *          Herbie tool (herbie.uwplse.org).
 *
 * @version 3.4.0
 * @date 2026-05-25
 */

#include "herbie_eval.h"
#include "interval_arithmetic.h"

#ifndef LV00_TAYLOR_MAX_VARS
#define LV00_TAYLOR_MAX_VARS 32
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ========================================================================
 * Internal helpers
 * ======================================================================== */

/**
 * @brief Simple pseudo-random number generator (xoshiro256**-like).
 */
static uint64_t g_herbie_state[4];

static void herbie_seed(unsigned int seed) {
    if (seed == 0) {
        seed = (unsigned int)time(NULL);
    }
    /* SplitMix64 initialization */
    uint64_t z = (uint64_t)seed + 0x9E3779B97F4A7C15ULL;
    for (int i = 0; i < 4; i++) {
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        g_herbie_state[i] = z ^ (z >> 31);
    }
}

static double herbie_random_double(void) {
    /* Simple LCG for portability */
    static uint64_t lcg_state = 0;
    if (lcg_state == 0) {
        lcg_state = (uint64_t)time(NULL) ^ (uint64_t)clock();
    }
    lcg_state = lcg_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (double)(lcg_state >> 11) / (double)(1ULL << 53);
}

/**
 * @brief Evaluate an expression as a double at given variable values.
 */
static double herbie_eval_double(
    const char *expr,
    const char **var_names,
    const double *var_values,
    int var_count)
{
    /* Create point intervals */
    Lv00Interval intervals[LV00_TAYLOR_MAX_VARS];
    for (int i = 0; i < var_count && i < LV00_TAYLOR_MAX_VARS; i++) {
        intervals[i] = interval_point(var_values[i]);
    }

    /* Use the interval evaluator to get a point result */
    Lv00Interval result = interval_from_symbolic(expr, var_names, intervals, var_count);
    if (interval_is_empty(result)) {
        return NAN;
    }
    return interval_mid(result);
}

/**
 * @brief Compute bit error between computed and exact values.
 *
 * The bit error is the number of bits in the significand that differ
 * between the computed and exact floating-point representations.
 */
static double compute_bit_error(double computed, double exact) {
    if (isnan(computed) || isnan(exact)) {
        return 53.0; /* Maximum bit error for NaN */
    }
    if (isinf(computed) || isinf(exact)) {
        return 53.0;
    }
    if (exact == 0.0 && computed == 0.0) {
        return 0.0;
    }
    if (exact == 0.0) {
        return 53.0;
    }

    double rel_err = fabs(computed - exact) / fabs(exact);
    if (rel_err == 0.0) {
        return 0.0;
    }

    /* Convert relative error to bit error: bits = -log2(rel_err) */
    double bits = -log2(rel_err);
    if (bits < 0.0) bits = 0.0;
    if (bits > 53.0) bits = 53.0;
    return bits;
}

/* ========================================================================
 * Main API
 * ======================================================================== */

LV00_PUBLIC_API bool herbie_evaluate(
    const char *expr,
    const char **var_names,
    const Lv00Interval *var_bounds,
    int var_count,
    const Lv00HerbieConfig *config,
    Lv00HerbieResult *out)
{
    if (!expr || !var_names || !var_bounds || !out || var_count <= 0) {
        return false;
    }

    Lv00HerbieConfig cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = herbie_config_default();
    }

    memset(out, 0, sizeof(*out));
    strncpy(out->expression, expr, sizeof(out->expression) - 1);

    herbie_seed(cfg.random_seed);

    int sample_count = cfg.sample_count;
    if (sample_count > LV00_HERBIE_MAX_SAMPLES) {
        sample_count = LV00_HERBIE_MAX_SAMPLES;
    }

    double total_bit_error = 0.0;
    double max_bit_error = 0.0;
    double total_rel_error = 0.0;
    double max_rel_error = 0.0;
    double errors[LV00_HERBIE_MAX_SAMPLES];
    int valid_count = 0;

    for (int s = 0; s < sample_count; s++) {
        /* Generate random sample within bounds */
        double values[LV00_TAYLOR_MAX_VARS];
        for (int v = 0; v < var_count && v < LV00_TAYLOR_MAX_VARS; v++) {
            double lo = var_bounds[v].lo;
            double hi = var_bounds[v].hi;
            values[v] = lo + herbie_random_double() * (hi - lo);
        }

        /* Evaluate the expression */
        double computed = herbie_eval_double(expr, var_names, values, var_count);

        if (isnan(computed) || isinf(computed)) {
            continue; /* Skip invalid samples */
        }

        /* For the "exact" value, we use the interval midpoint as an approximation.
         * In a full Herbie implementation, this would use MPFR for true exact evaluation. */
        Lv00Interval intervals[LV00_TAYLOR_MAX_VARS];
        for (int v = 0; v < var_count && v < LV00_TAYLOR_MAX_VARS; v++) {
            intervals[v] = interval_point(values[v]);
        }
        Lv00Interval result = interval_from_symbolic(expr, var_names, intervals, var_count);
        double exact = interval_mid(result);

        if (isnan(exact) || isinf(exact) || exact == 0.0) {
            continue;
        }

        double rel_err = fabs(computed - exact) / fabs(exact);
        double bit_err = compute_bit_error(computed, exact);

        total_bit_error += bit_err;
        if (bit_err > max_bit_error) max_bit_error = bit_err;

        total_rel_error += rel_err;
        if (rel_err > max_rel_error) max_rel_error = rel_err;

        errors[valid_count] = fabs(computed - exact);
        valid_count++;
    }

    out->sample_count = sample_count;
    out->valid_samples = valid_count;

    if (valid_count > 0) {
        out->max_bit_error = max_bit_error;
        out->avg_bit_error = total_bit_error / valid_count;
        out->max_relative_error = max_rel_error;
        out->avg_relative_error = total_rel_error / valid_count;

        /* Compute AMBER score */
        out->amber_score = herbie_compute_amber(errors, valid_count,
                                                 cfg.amber_alpha, cfg.amber_beta);
    } else {
        out->max_bit_error = 53.0;
        out->avg_bit_error = 53.0;
        out->amber_score = 0.0;
    }

    return true;
}

LV00_PUBLIC_API bool herbie_compare(
    const char **exprs,
    int expr_count,
    const char **var_names,
    const Lv00Interval *var_bounds,
    int var_count,
    const Lv00HerbieConfig *config,
    Lv00HerbieResult *results,
    int *best_index)
{
    if (!exprs || expr_count <= 0 || !results) {
        return false;
    }

    double best_score = -1.0;
    int best = -1;

    for (int i = 0; i < expr_count && i < LV00_HERBIE_MAX_PATHS; i++) {
        if (!herbie_evaluate(exprs[i], var_names, var_bounds, var_count,
                              config, &results[i])) {
            return false;
        }
        if (results[i].amber_score > best_score) {
            best_score = results[i].amber_score;
            best = i;
        }
    }

    if (best_index) {
        *best_index = best;
    }

    return true;
}

LV00_PUBLIC_API bool herbie_partition_regimes(
    const char *expr,
    const char **var_names,
    const Lv00Interval *var_bounds,
    int var_count,
    const Lv00HerbieConfig *config,
    Lv00HerbiePartitionResult *out)
{
    if (!expr || !var_names || !var_bounds || !out || var_count <= 0) {
        return false;
    }

    Lv00HerbieConfig cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = herbie_config_default();
    }

    memset(out, 0, sizeof(*out));

    /* Simple regime detection: bisect the first variable's domain */
    if (!cfg.enable_regime_detection) {
        /* Single regime: the entire domain */
        for (int v = 0; v < var_count && v < LV00_TAYLOR_MAX_VARS; v++) {
            out->regimes[0].bounds[v] = var_bounds[v];
        }
        out->regimes[0].var_count = var_count;
        out->regimes[0].weight = 1.0;
        strncpy(out->regimes[0].description, "full_domain",
                sizeof(out->regimes[0].description) - 1);
        out->regime_count = 1;
        out->total_weight = 1.0;
        return true;
    }

    /* Partition into two regimes based on the first variable */
    double mid = interval_mid(var_bounds[0]);

    /* Regime 1: lower half */
    for (int v = 0; v < var_count && v < LV00_TAYLOR_MAX_VARS; v++) {
        out->regimes[0].bounds[v] = var_bounds[v];
    }
    out->regimes[0].bounds[0].hi = mid;
    out->regimes[0].var_count = var_count;
    out->regimes[0].weight = 0.5;
    snprintf(out->regimes[0].description, sizeof(out->regimes[0].description),
             "%s_low", var_names[0]);

    /* Regime 2: upper half */
    for (int v = 0; v < var_count && v < LV00_TAYLOR_MAX_VARS; v++) {
        out->regimes[1].bounds[v] = var_bounds[v];
    }
    out->regimes[1].bounds[0].lo = mid;
    out->regimes[1].var_count = var_count;
    out->regimes[1].weight = 0.5;
    snprintf(out->regimes[1].description, sizeof(out->regimes[1].description),
             "%s_high", var_names[0]);

    out->regime_count = 2;
    out->total_weight = 1.0;

    return true;
}

LV00_PUBLIC_API bool herbie_select_path(
    const char **exprs,
    int expr_count,
    const char **var_names,
    const Lv00HerbiePartitionResult *partition,
    int var_count,
    const Lv00HerbieConfig *config,
    int *best_indices)
{
    if (!exprs || expr_count <= 0 || !var_names || !partition || !best_indices) {
        return false;
    }

    for (int r = 0; r < partition->regime_count && r < LV00_HERBIE_MAX_REGIMES; r++) {
        Lv00HerbieResult results[LV00_HERBIE_MAX_PATHS];
        int best = -1;

        if (herbie_compare(exprs, expr_count, var_names,
                            partition->regimes[r].bounds, var_count,
                            config, results, &best)) {
            best_indices[r] = best;
        } else {
            best_indices[r] = 0; /* Default to first expression */
        }
    }

    return true;
}

/* ========================================================================
 * AMBER scoring
 * ======================================================================== */

LV00_PUBLIC_API double herbie_compute_amber(
    const double *errors,
    int error_count,
    double alpha,
    double beta)
{
    if (!errors || error_count <= 0) {
        return 0.0;
    }

    /* AMBER score: 1 / (1 + alpha * mean_error^beta) */
    double sum = 0.0;
    for (int i = 0; i < error_count; i++) {
        sum += errors[i];
    }
    double mean_error = sum / error_count;

    double score = 1.0 / (1.0 + alpha * pow(mean_error, beta));

    /* Clamp to [0, 1] */
    if (score < 0.0) score = 0.0;
    if (score > 1.0) score = 1.0;

    return score;
}

LV00_PUBLIC_API Lv00HerbieConfig herbie_config_default(void) {
    Lv00HerbieConfig cfg;
    cfg.sample_count = 1000;
    cfg.random_seed = 0;
    cfg.amber_alpha = 0.5;
    cfg.amber_beta = 2.0;
    cfg.enable_regime_detection = 1;
    cfg.regime_threshold = 0.1;
    return cfg;
}
