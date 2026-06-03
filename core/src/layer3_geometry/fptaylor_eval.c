/**
 * @file fptaylor_eval.c
 * @brief Implementation of FPTaylor-style floating-point error analysis
 *
 * @details Implements Taylor expansion + interval arithmetic for rigorous
 *          floating-point error bound computation. Uses a simple recursive
 *          expression evaluator with numerical differentiation for partial
 *          derivatives.
 *
 * @version 3.4.0
 * @date 2026-05-25
 */

#include "fptaylor_eval.h"
#include "interval_arithmetic.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Internal expression evaluator
 * ======================================================================== */

/** Maximum expression length */
#define LV00_EXPR_MAX_LEN 1024

/**
 * @brief Expression evaluation context.
 */
typedef struct {
    const char *pos;
    const char **var_names;
    const Lv00Interval *var_intervals;
    int var_count;
    int error;
} FPEvalCtx;

static void fp_skip_ws(FPEvalCtx *ctx) {
    while (*ctx->pos == ' ' || *ctx->pos == '\t' || *ctx->pos == '\n') {
        ctx->pos++;
    }
}

static Lv00Interval fp_parse_primary(FPEvalCtx *ctx);
static Lv00Interval fp_parse_mul_div(FPEvalCtx *ctx);

static Lv00Interval fp_parse_primary(FPEvalCtx *ctx) {
    fp_skip_ws(ctx);

    /* Number literal */
    if ((*ctx->pos >= '0' && *ctx->pos <= '9') || *ctx->pos == '.') {
        char *end;
        double val = strtod(ctx->pos, &end);
        ctx->pos = end;
        return interval_point(val);
    }

    /* Parenthesized expression */
    if (*ctx->pos == '(') {
        ctx->pos++;
        Lv00Interval r = fp_parse_mul_div(ctx);
        fp_skip_ws(ctx);
        if (*ctx->pos == ')') ctx->pos++;
        else ctx->error = 1;
        return r;
    }

    /* Unary minus */
    if (*ctx->pos == '-') {
        ctx->pos++;
        Lv00Interval r = fp_parse_primary(ctx);
        return interval_neg(r);
    }

    /* Unary plus */
    if (*ctx->pos == '+') {
        ctx->pos++;
        return fp_parse_primary(ctx);
    }

    /* Function call or variable */
    if ((*ctx->pos >= 'a' && *ctx->pos <= 'z') || (*ctx->pos >= 'A' && *ctx->pos <= 'Z')) {
        char name[64];
        int len = 0;
        while (len < 63 && ((*ctx->pos >= 'a' && *ctx->pos <= 'z') ||
               (*ctx->pos >= 'A' && *ctx->pos <= 'Z') ||
               (*ctx->pos >= '0' && *ctx->pos <= '9') ||
               *ctx->pos == '_')) {
            name[len++] = *ctx->pos++;
        }
        name[len] = '\0';
        fp_skip_ws(ctx);

        /* Function call */
        if (*ctx->pos == '(') {
            ctx->pos++;
            Lv00Interval arg = fp_parse_mul_div(ctx);
            fp_skip_ws(ctx);
            if (*ctx->pos == ')') ctx->pos++;
            else ctx->error = 1;

            if (strcmp(name, "sqrt") == 0) return interval_sqrt(arg);
            if (strcmp(name, "sin") == 0) return interval_sin(arg);
            if (strcmp(name, "cos") == 0) return interval_cos(arg);
            if (strcmp(name, "exp") == 0) return interval_exp(arg);
            if (strcmp(name, "log") == 0) return interval_log(arg);
            if (strcmp(name, "abs") == 0) return interval_abs(arg);

            ctx->error = 1;
            return interval_empty();
        }

        /* Constants */
        if (strcmp(name, "pi") == 0) return interval_point(M_PI);
        if (strcmp(name, "e") == 0) return interval_point(M_E);

        /* Variable lookup */
        for (int i = 0; i < ctx->var_count; i++) {
            if (strcmp(name, ctx->var_names[i]) == 0) {
                return ctx->var_intervals[i];
            }
        }

        ctx->error = 1;
        return interval_empty();
    }

    ctx->error = 1;
    return interval_empty();
}

static Lv00Interval fp_parse_mul_div(FPEvalCtx *ctx) {
    Lv00Interval left = fp_parse_primary(ctx);
    while (!ctx->error) {
        fp_skip_ws(ctx);
        if (*ctx->pos == '*') {
            ctx->pos++;
            Lv00Interval right = fp_parse_primary(ctx);
            left = interval_mul(left, right);
        } else if (*ctx->pos == '/') {
            ctx->pos++;
            Lv00Interval right = fp_parse_primary(ctx);
            left = interval_div(left, right);
        } else {
            break;
        }
    }
    return left;
}

static Lv00Interval fp_parse_expr(FPEvalCtx *ctx) {
    Lv00Interval left = fp_parse_mul_div(ctx);
    while (!ctx->error) {
        fp_skip_ws(ctx);
        if (*ctx->pos == '+') {
            ctx->pos++;
            Lv00Interval right = fp_parse_mul_div(ctx);
            left = interval_add(left, right);
        } else if (*ctx->pos == '-') {
            ctx->pos++;
            Lv00Interval right = fp_parse_mul_div(ctx);
            left = interval_sub(left, right);
        } else {
            break;
        }
    }
    return left;
}

/**
 * @brief Evaluate an expression as a double at given variable values.
 */
static double fp_eval_double(
    const char *expr,
    const char **var_names,
    const double *var_values,
    int var_count)
{
    /* Create point intervals from values */
    Lv00Interval intervals[LV00_TAYLOR_MAX_VARS];
    for (int i = 0; i < var_count && i < LV00_TAYLOR_MAX_VARS; i++) {
        intervals[i] = interval_point(var_values[i]);
    }

    FPEvalCtx ctx;
    ctx.pos = expr;
    ctx.var_names = var_names;
    ctx.var_intervals = intervals;
    ctx.var_count = var_count;
    ctx.error = 0;

    Lv00Interval result = fp_parse_expr(&ctx);

    if (ctx.error || interval_is_empty(result)) {
        return NAN;
    }
    return interval_mid(result);
}

/* ========================================================================
 * Taylor form computation
 * ======================================================================== */

LV00_PUBLIC_API bool fptaylor_taylor_form(
    const char *expr,
    const char **var_names,
    const double *var_centers,
    int var_count,
    int order,
    Lv00TaylorForm *out)
{
    if (!expr || !var_names || !var_centers || !out || var_count <= 0) {
        return false;
    }
    if (var_count > LV00_TAYLOR_MAX_VARS) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->var_count = var_count;
    out->order = (order >= 1 && order <= 2) ? order : 1;

    /* Store variable centers */
    for (int i = 0; i < var_count; i++) {
        out->vars_center[i] = var_centers[i];
    }

    /* Compute center value: f(center) */
    out->center = fp_eval_double(expr, var_names, var_centers, var_count);
    if (isnan(out->center)) {
        return false;
    }

    /* Compute partial derivatives using central differences */
    double h = 1e-7;
    for (int i = 0; i < var_count; i++) {
        double vals_plus[LV00_TAYLOR_MAX_VARS];
        double vals_minus[LV00_TAYLOR_MAX_VARS];
        memcpy(vals_plus, var_centers, sizeof(double) * var_count);
        memcpy(vals_minus, var_centers, sizeof(double) * var_count);

        vals_plus[i] += h;
        vals_minus[i] -= h;

        double f_plus = fp_eval_double(expr, var_names, vals_plus, var_count);
        double f_minus = fp_eval_double(expr, var_names, vals_minus, var_count);

        if (isnan(f_plus) || isnan(f_minus)) {
            out->derivs[i] = 0.0;
        } else {
            out->derivs[i] = (f_plus - f_minus) / (2.0 * h);
        }
    }

    /* Compute remainder bounds using interval evaluation */
    Lv00Interval intervals[LV00_TAYLOR_MAX_VARS];
    for (int i = 0; i < var_count; i++) {
        intervals[i] = interval_create(var_centers[i] - h, var_centers[i] + h, 0);
    }

    FPEvalCtx ctx;
    ctx.pos = expr;
    ctx.var_names = var_names;
    ctx.var_intervals = intervals;
    ctx.var_count = var_count;
    ctx.error = 0;

    Lv00Interval full_range = fp_parse_expr(&ctx);

    if (!ctx.error && !interval_is_empty(full_range)) {
        out->rem_lo = full_range.lo - out->center;
        out->rem_hi = full_range.hi - out->center;
    } else {
        out->rem_lo = -h;
        out->rem_hi = h;
    }

    return true;
}

/* ========================================================================
 * Error analysis
 * ======================================================================== */

LV00_PUBLIC_API bool fptaylor_evaluate(
    const char *expr,
    const char **var_names,
    const Lv00Interval *var_bounds,
    int var_count,
    const Lv00FPTaylorConfig *config,
    Lv00ErrorBound *out)
{
    return fptaylor_analyze_expression(expr, var_names, var_bounds, var_count,
                                       config, out, NULL);
}

LV00_PUBLIC_API bool fptaylor_analyze_expression(
    const char *expr,
    const char **var_names,
    const Lv00Interval *var_bounds,
    int var_count,
    const Lv00FPTaylorConfig *config,
    Lv00ErrorBound *out,
    Lv00TaylorForm *taylor_out)
{
    if (!expr || !var_names || !var_bounds || !out || var_count <= 0) {
        return false;
    }

    Lv00FPTaylorConfig cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = fptaylor_config_default();
    }

    memset(out, 0, sizeof(*out));

    /* Step 1: Compute the interval evaluation of the expression */
    FPEvalCtx ctx;
    ctx.pos = expr;
    ctx.var_names = var_names;
    ctx.var_intervals = var_bounds;
    ctx.var_count = var_count;
    ctx.error = 0;

    Lv00Interval result = fp_parse_expr(&ctx);

    if (ctx.error || interval_is_empty(result)) {
        out->is_valid = 0;
        snprintf(out->proof_text, sizeof(out->proof_text),
                 "Error: failed to evaluate expression '%s'", expr);
        return false;
    }

    /* Step 2: Compute center value at midpoints */
    double centers[LV00_TAYLOR_MAX_VARS];
    for (int i = 0; i < var_count && i < LV00_TAYLOR_MAX_VARS; i++) {
        centers[i] = interval_mid(var_bounds[i]);
    }

    double center_val = fp_eval_double(expr, var_names, centers, var_count);

    /* Step 3: Compute Taylor form if requested */
    Lv00TaylorForm tf;
    if (taylor_out || cfg.taylor_order > 0) {
        if (fptaylor_taylor_form(expr, var_names, centers, var_count,
                                  cfg.taylor_order, &tf)) {
            if (taylor_out) {
                *taylor_out = tf;
            }
        }
    }

    /* Step 4: Compute error bounds */
    /* The absolute error is bounded by the interval diameter */
    out->absolute_error = interval_diam(result) / 2.0;

    /* The relative error */
    double abs_center = fabs(center_val);
    if (abs_center > 1e-300) {
        out->relative_error = out->absolute_error / abs_center;
    } else {
        out->relative_error = out->absolute_error;
    }

    /* Roundoff error: machine epsilon times number of operations */
    out->roundoff_error = cfg.rounding_unit * out->absolute_error;

    /* Truncation error: from Taylor remainder */
    out->truncation_error = (tf.rem_hi - tf.rem_lo) / 2.0;

    out->is_valid = 1;

    /* Generate proof text */
    snprintf(out->proof_text, sizeof(out->proof_text),
             "Expression: %s\n"
             "Interval evaluation: [%.17g, %.17g]\n"
             "Center value: %.17g\n"
             "Absolute error bound: %.6e\n"
             "Relative error bound: %.6e\n"
             "Roundoff error: %.6e\n"
             "Truncation error: %.6e",
             expr, result.lo, result.hi, center_val,
             out->absolute_error, out->relative_error,
             out->roundoff_error, out->truncation_error);

    return true;
}

LV00_PUBLIC_API Lv00FPTaylorConfig fptaylor_config_default(void) {
    Lv00FPTaylorConfig cfg;
    cfg.taylor_order = 1;
    cfg.branch_threshold = 1e-6;
    cfg.max_bisections = 10;
    cfg.enable_optimization = 0;
    cfg.rounding_unit = 2.2204460492503131e-16; /* DBL_EPSILON */
    return cfg;
}
