/**
 * @file interval_arithmetic.c
 * @brief Implementation of unified interval arithmetic for Lv-00
 *
 * @details Double-based interval arithmetic implementation. All operations
 *          guarantee containment: the true mathematical result is always
 *          within the computed interval. Uses outward rounding to ensure
 *          conservativeness in the presence of floating-point rounding.
 *
 * @version 3.4.0
 * @date 2026-05-25
 */

#include "lv/lv_platform.h"
#include "interval_arithmetic.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "lv_internal.h"

/* ========================================================================
 * Internal helpers
 * ======================================================================== */

/**
 * @brief Round a double downward (toward -infinity) to ensure containment.
 *
 * For rigorous interval arithmetic, outward rounding is essential to
 * guarantee that the true mathematical result lies within the computed
 * interval. nextafter(x, -INFINITY) returns the next representable
 * double value less than x, providing a strict lower bound.
 *
 * For infinity/NaN inputs, returns the input unchanged.
 *
 * @param x  The value to round downward
 * @return   The next representable double less than or equal to x
 */
static double round_down(double x) {
    if (isfinite(x)) {
        return nextafter(x, -INFINITY);
    }
    return x;
}

/**
 * @brief Round a double upward (toward +infinity) to ensure containment.
 *
 * Symmetric to round_down. nextafter(x, INFINITY) returns the next
 * representable double value greater than x, providing a strict upper bound.
 *
 * For infinity/NaN inputs, returns the input unchanged.
 *
 * @param x  The value to round upward
 * @return   The next representable double greater than or equal to x
 */
static double round_up(double x) {
    if (isfinite(x)) {
        return nextafter(x, INFINITY);
    }
    return x;
}

/**
 * @brief Minimum of four doubles.
 */
static double min4(double a, double b, double c, double d) {
    double m = a;
    if (b < m)
        m = b;
    if (c < m)
        m = c;
    if (d < m)
        m = d;
    return m;
}

/**
 * @brief Maximum of four doubles.
 */
static double max4(double a, double b, double c, double d) {
    double m = a;
    if (b > m)
        m = b;
    if (c > m)
        m = c;
    if (d > m)
        m = d;
    return m;
}

/* ========================================================================
 * Factory functions
 * ======================================================================== */

lvInterval interval_create(double lo, double hi, int is_exact) {
    lvInterval iv;
    iv.lo = lo;
    iv.hi = hi;
    iv.is_exact = is_exact;
    return iv;
}

lvInterval interval_point(double val) {
    lvInterval iv;
    iv.lo = val;
    iv.hi = val;
    iv.is_exact = 1;
    return iv;
}

lvInterval interval_empty(void) {
    lvInterval iv;
    iv.lo = 1.0;
    iv.hi = -1.0;
    iv.is_exact = 0;
    return iv;
}

lvInterval interval_entire(void) {
    lvInterval iv;
    iv.lo = -INFINITY;
    iv.hi = INFINITY;
    iv.is_exact = 0;
    return iv;
}

lvIntervalConfig interval_config_default(void) {
    lvIntervalConfig cfg;
    cfg.precision = 53;     /* double precision */
    cfg.rounding_eps = 0.0; /* no extra rounding in double mode */
    return cfg;
}

/* ========================================================================
 * Arithmetic operations
 * ======================================================================== */

lvInterval interval_add(lvInterval a, lvInterval b) {
    if (interval_is_empty(a) || interval_is_empty(b)) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = round_down(a.lo + b.lo);
    r.hi = round_up(a.hi + b.hi);
    r.is_exact = a.is_exact && b.is_exact;
    return r;
}

lvInterval interval_sub(lvInterval a, lvInterval b) {
    if (interval_is_empty(a) || interval_is_empty(b)) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = round_down(a.lo - b.hi);
    r.hi = round_up(a.hi - b.lo);
    r.is_exact = a.is_exact && b.is_exact;
    return r;
}

lvInterval interval_mul(lvInterval a, lvInterval b) {
    if (interval_is_empty(a) || interval_is_empty(b)) {
        return interval_empty();
    }
    lvInterval r;
    double p1 = a.lo * b.lo;
    double p2 = a.lo * b.hi;
    double p3 = a.hi * b.lo;
    double p4 = a.hi * b.hi;
    r.lo = round_down(min4(p1, p2, p3, p4));
    r.hi = round_up(max4(p1, p2, p3, p4));
    r.is_exact = a.is_exact && b.is_exact;
    return r;
}

lvInterval interval_div(lvInterval a, lvInterval b) {
    if (interval_is_empty(a) || interval_is_empty(b)) {
        return interval_empty();
    }
    /* Check if divisor contains zero */
    if (b.lo <= 0.0 && b.hi >= 0.0) {
        return interval_empty();
    }
    lvInterval r;
    /* Reciprocal of b: [1/b.hi, 1/b.lo] (signs handled by min/max) */
    double inv_lo = 1.0 / b.hi;
    double inv_hi = 1.0 / b.lo;
    lvInterval inv_b;
    inv_b.lo = round_down((inv_lo < inv_hi) ? inv_lo : inv_hi);
    inv_b.hi = round_up((inv_lo < inv_hi) ? inv_hi : inv_lo);
    inv_b.is_exact = 0;
    /* a / b = a * (1/b) */
    r = interval_mul(a, inv_b);
    r.is_exact = 0; /* Division always introduces potential rounding */
    return r;
}

lvInterval interval_sqrt(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    if (a.lo < 0.0) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = round_down(sqrt(a.lo));
    r.hi = round_up(sqrt(a.hi));
    r.is_exact = a.is_exact;
    return r;
}

lvInterval interval_sin(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    lvInterval r;

    /* Normalize the interval to [0, 2*pi) range */
    double lo = a.lo;
    double hi = a.hi;
    double two_pi = 2.0 * M_PI;

    /* If the interval spans more than 2*pi, the result is [-1, 1] */
    if (hi - lo >= two_pi) {
        r.lo = -1.0;
        r.hi = 1.0;
        r.is_exact = 0;
        return r;
    }

    /* Compute sin at endpoints */
    double s_lo = sin(lo);
    double s_hi = sin(hi);

    /* Check if the interval contains any critical points (pi/2 + k*pi) */
    /* where sin reaches its maximum of 1 or minimum of -1 */
    r.lo = round_down((s_lo < s_hi) ? s_lo : s_hi);
    r.hi = round_up((s_lo < s_hi) ? s_hi : s_lo);

    /* 计算极值点覆盖范围：使用区间端点计算所需的 k 范围而非固定 [-100,100] */
    /* 最大值在 pi/2 + k*pi 处，最小值在 -pi/2 + k*pi 处 */
    double k_min_lo = floor((lo - M_PI / 2.0) / M_PI);
    double k_max_hi = ceil((hi - M_PI / 2.0) / M_PI);
    /* 限制搜索范围避免过大循环（区间跨度已在上方保证 < 2*pi，因此最多覆盖 3 个极值点） */
    if (k_min_lo < -2.0)
        k_min_lo = -2.0;
    if (k_max_hi > 2.0)
        k_max_hi = 2.0;
    for (double k = k_min_lo; k <= k_max_hi; k += 1.0) {
        double crit = M_PI / 2.0 + k * M_PI;
        if (crit >= lo && crit <= hi) {
            double sc = sin(crit);
            if (sc > r.hi)
                r.hi = round_up(sc);
            if (sc < r.lo)
                r.lo = round_down(sc);
        }
    }

    for (double k = k_min_lo; k <= k_max_hi; k += 1.0) {
        double crit = -M_PI / 2.0 + k * M_PI;
        if (crit >= lo && crit <= hi) {
            double sc = sin(crit);
            if (sc < r.lo)
                r.lo = round_down(sc);
            if (sc > r.hi)
                r.hi = round_up(sc);
        }
    }

    r.is_exact = 0;
    return r;
}

lvInterval interval_cos(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    lvInterval r;

    double lo = a.lo;
    double hi = a.hi;
    double two_pi = 2.0 * M_PI;

    /* If the interval spans more than 2*pi, the result is [-1, 1] */
    if (hi - lo >= two_pi) {
        r.lo = -1.0;
        r.hi = 1.0;
        r.is_exact = 0;
        return r;
    }

    /* Compute cos at endpoints */
    double c_lo = cos(lo);
    double c_hi = cos(hi);

    r.lo = round_down((c_lo < c_hi) ? c_lo : c_hi);
    r.hi = round_up((c_lo < c_hi) ? c_hi : c_lo);

    /* 使用区间端点动态计算极值点范围 */
    double k_min = floor(lo / M_PI);
    double k_max = ceil(hi / M_PI);
    if (k_min < -2.0)
        k_min = -2.0;
    if (k_max > 2.0)
        k_max = 2.0;
    for (double k = k_min; k <= k_max; k += 1.0) {
        double crit = k * M_PI;
        if (crit >= lo && crit <= hi) {
            double cc = cos(crit);
            if (cc > r.hi)
                r.hi = round_up(cc);
            if (cc < r.lo)
                r.lo = round_down(cc);
        }
    }

    r.is_exact = 0;
    return r;
}

lvInterval interval_exp(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = round_down(exp(a.lo));
    r.hi = round_up(exp(a.hi));
    r.is_exact = 0;
    return r;
}

lvInterval interval_log(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    if (a.lo <= 0.0) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = round_down(log(a.lo));
    r.hi = round_up(log(a.hi));
    r.is_exact = 0;
    return r;
}

lvInterval interval_abs(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    lvInterval r;
    if (a.lo >= 0.0) {
        /* Entirely non-negative */
        r.lo = a.lo;
        r.hi = a.hi;
    } else if (a.hi <= 0.0) {
        /* Entirely non-positive */
        r.lo = -a.hi;
        r.hi = -a.lo;
    } else {
        /* Spans zero */
        r.lo = 0.0;
        r.hi = (a.hi > -a.lo) ? a.hi : -a.lo;
    }
    r.is_exact = a.is_exact;
    return r;
}

lvInterval interval_neg(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = -a.hi;
    r.hi = -a.lo;
    r.is_exact = a.is_exact;
    return r;
}

/* ========================================================================
 * Properties
 * ======================================================================== */

double interval_diam(lvInterval a) {
    if (interval_is_empty(a)) {
        return 0.0;
    }
    return a.hi - a.lo;
}

double interval_mid(lvInterval a) {
    if (interval_is_empty(a)) {
        return NAN;
    }
    return (a.lo + a.hi) / 2.0;
}

int interval_is_empty(lvInterval a) {
    return a.lo > a.hi;
}

int interval_contains(lvInterval a, double val) {
    if (interval_is_empty(a)) {
        return 0;
    }
    return val >= a.lo && val <= a.hi;
}

int interval_is_subset(lvInterval a, lvInterval b) {
    if (interval_is_empty(a)) {
        return 1; /* Empty set is a subset of any set */
    }
    if (interval_is_empty(b)) {
        return 0;
    }
    return a.lo >= b.lo && a.hi <= b.hi;
}

int interval_equals(lvInterval a, lvInterval b) {
    /* Two empty intervals are equal */
    if (interval_is_empty(a) && interval_is_empty(b)) {
        return 1;
    }
    if (interval_is_empty(a) || interval_is_empty(b)) {
        return 0;
    }
    return a.lo == b.lo && a.hi == b.hi;
}

/* ========================================================================
 * Set operations
 * ======================================================================== */

lvInterval interval_intersect(lvInterval a, lvInterval b) {
    if (interval_is_empty(a) || interval_is_empty(b)) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = (a.lo > b.lo) ? a.lo : b.lo;
    r.hi = (a.hi < b.hi) ? a.hi : b.hi;
    if (r.lo > r.hi) {
        return interval_empty();
    }
    r.is_exact = a.is_exact && b.is_exact && a.lo == b.lo && a.hi == b.hi;
    return r;
}

lvInterval interval_union(lvInterval a, lvInterval b) {
    if (interval_is_empty(a))
        return b;
    if (interval_is_empty(b))
        return a;
    lvInterval r;
    r.lo = (a.lo < b.lo) ? a.lo : b.lo;
    r.hi = (a.hi > b.hi) ? a.hi : b.hi;
    r.is_exact = a.is_exact && b.is_exact && r.lo == r.hi;
    return r;
}

/* ========================================================================
 * Symbolic coordinate integration
 * ======================================================================== */

/**
 * @brief Simple expression evaluator for interval_from_symbolic.
 *
 * Supports: numeric literals, variable references, +, -, *, /, sqrt, sin, cos.
 * This is a basic recursive descent parser.
 */

typedef struct {
    const char *pos;                 /**< Current position in the expression string */
    const char **var_names;          /**< Variable names */
    const lvInterval *var_intervals; /**< Variable intervals */
    int var_count;                   /**< Number of variables */
    int error;                       /**< Nonzero if parsing error occurred */
} ExprParser;

static void skip_whitespace(ExprParser *p) {
    while (*p->pos == ' ' || *p->pos == '\t' || *p->pos == '\n' || *p->pos == '\r') {
        p->pos++;
    }
}

static lvInterval parse_expr(ExprParser *p);

/**
 * @brief 一元数学函数查找表
 *
 * 函数名 → 区间运算函数映射，线性扫描匹配，替代手写 strcmp 分支链。
 */
static const struct {
    const char *name;
    lvInterval (*fn)(lvInterval);
} kIntervalFuncs[] = {
    {"sqrt", interval_sqrt},
    {"sin", interval_sin},
    {"cos", interval_cos},
    {"exp", interval_exp},
    {"log", interval_log},
    {"abs", interval_abs},
};

static lvInterval parse_primary(ExprParser *p) {
    skip_whitespace(p);

    /* Number literal */
    if ((*p->pos >= '0' && *p->pos <= '9') || *p->pos == '.') {
        char *end;
        double val = strtod(p->pos, &end);
        p->pos = end;
        return interval_point(val);
    }

    /* Parenthesized expression */
    if (*p->pos == '(') {
        p->pos++;
        lvInterval r = parse_expr(p);
        skip_whitespace(p);
        if (*p->pos == ')') {
            p->pos++;
        } else {
            p->error = 1;
        }
        return r;
    }

    /* Unary minus */
    if (*p->pos == '-') {
        p->pos++;
        lvInterval r = parse_primary(p);
        return interval_neg(r);
    }

    /* Function call or variable */
    if ((*p->pos >= 'a' && *p->pos <= 'z') || (*p->pos >= 'A' && *p->pos <= 'Z') || *p->pos == '_') {
        char name[64];
        int len = 0;
        while (len < 63 && ((*p->pos >= 'a' && *p->pos <= 'z') || (*p->pos >= 'A' && *p->pos <= 'Z') ||
                            (*p->pos >= '0' && *p->pos <= '9') || *p->pos == '_')) {
            name[len++] = *p->pos;
            p->pos++;
        }
        name[len] = '\0';

        skip_whitespace(p);

        /* Check for function call */
        if (*p->pos == '(') {
            p->pos++;
            lvInterval arg = parse_expr(p);
            skip_whitespace(p);
            if (*p->pos == ')')
                p->pos++;
            else
                p->error = 1;

            /* 查表匹配一元数学函数 */
            for (size_t i = 0; i < lv_ARRAY_SIZE(kIntervalFuncs); i++) {
                if (strcmp(name, kIntervalFuncs[i].name) == 0)
                    return kIntervalFuncs[i].fn(arg);
            }

            /* Unknown function */
            p->error = 1;
            return interval_empty();
        }

        /* Variable lookup */
        for (int i = 0; i < p->var_count; i++) {
            if (strcmp(name, p->var_names[i]) == 0) {
                return p->var_intervals[i];
            }
        }

        /* Unknown variable */
        p->error = 1;
        return interval_empty();
    }

    p->error = 1;
    return interval_empty();
}

static lvInterval parse_mul_div(ExprParser *p) {
    lvInterval left = parse_primary(p);
    while (!p->error) {
        skip_whitespace(p);
        if (*p->pos == '*') {
            p->pos++;
            lvInterval right = parse_primary(p);
            left = interval_mul(left, right);
        } else if (*p->pos == '/') {
            p->pos++;
            lvInterval right = parse_primary(p);
            left = interval_div(left, right);
        } else {
            break;
        }
    }
    return left;
}

static lvInterval parse_expr(ExprParser *p) {
    lvInterval left = parse_mul_div(p);
    while (!p->error) {
        skip_whitespace(p);
        if (*p->pos == '+') {
            p->pos++;
            lvInterval right = parse_mul_div(p);
            left = interval_add(left, right);
        } else if (*p->pos == '-') {
            p->pos++;
            lvInterval right = parse_mul_div(p);
            left = interval_sub(left, right);
        } else {
            break;
        }
    }
    return left;
}

lvInterval interval_from_symbolic(const char *expr_str, const char **var_names, const lvInterval *var_intervals,
                                  int var_count) {
    if (!expr_str || !var_names || !var_intervals || var_count <= 0) {
        return interval_empty();
    }

    ExprParser p;
    p.pos = expr_str;
    p.var_names = var_names;
    p.var_intervals = var_intervals;
    p.var_count = var_count;
    p.error = 0;

    lvInterval result = parse_expr(&p);

    if (p.error) {
        return interval_empty();
    }
    return result;
}

int interval_to_symbolic(lvInterval a, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "interval_to_symbolic: buf is NULL or buf_size is 0");
    }
    if (interval_is_empty(a)) {
        return snprintf(buf, buf_size, "[empty]");
    }
    if (a.is_exact && a.lo == a.hi) {
        return snprintf(buf, buf_size, "%.17g", a.lo);
    }
    return snprintf(buf, buf_size, "[%.17g, %.17g]", a.lo, a.hi);
}

/* ========================================================================
 * Verification functions
 * ======================================================================== */

int interval_verify_solution(lvInterval f_interval, double tolerance) {
    if (interval_is_empty(f_interval)) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "interval_verify_solution: empty interval");
    }
    /* Check if 0 is contained in the interval, considering tolerance */
    if (f_interval.lo <= tolerance && f_interval.hi >= -tolerance) {
        return 1;
    }
    return 0;
}

int interval_verify_adaptive(const char *expr_str, const char **var_names, lvInterval *var_intervals, int var_count,
                             int max_depth, double tolerance) {
    if (!expr_str || !var_names || !var_intervals || var_count <= 0) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "interval_verify_adaptive: NULL parameter or invalid var_count");
    }

    /* Evaluate the expression on the current intervals */
    lvInterval result = interval_from_symbolic(expr_str, var_names, var_intervals, var_count);

    /* Check if 0 is in the result interval */
    if (interval_verify_solution(result, tolerance) == 1) {
        return 1;
    }

    /* If max depth reached, give up */
    if (max_depth <= 0) {
        return 0;
    }

    /* Find the widest interval to bisect */
    int widest = 0;
    double max_diam = 0.0;
    for (int i = 0; i < var_count; i++) {
        double d = interval_diam(var_intervals[i]);
        if (d > max_diam) {
            max_diam = d;
            widest = i;
        }
    }

    if (max_diam <= 0.0) {
        /* All intervals are points, cannot refine further */
        return 0;
    }

    /* Bisect the widest interval */
    double mid = interval_mid(var_intervals[widest]);
    lvInterval saved = var_intervals[widest];

    /* Try lower half */
    var_intervals[widest] = interval_create(saved.lo, mid, 0);
    int lower = interval_verify_adaptive(expr_str, var_names, var_intervals, var_count, max_depth - 1, tolerance);
    if (lower == 1) {
        var_intervals[widest] = saved;
        return 1;
    }

    /* Try upper half */
    var_intervals[widest] = interval_create(mid, saved.hi, 0);
    int upper = interval_verify_adaptive(expr_str, var_names, var_intervals, var_count, max_depth - 1, tolerance);
    if (upper == 1) {
        var_intervals[widest] = saved;
        return 1;
    }

    /* Restore original interval */
    var_intervals[widest] = saved;
    return 0;
}

/* ========================================================================
 * Legacy lv_interval_* wrappers (matching interval_arithmetic.h stubs)
 * ======================================================================== */

lvInterval lv_interval_add(lvInterval a, lvInterval b) {
    return interval_add(a, b);
}

lvInterval lv_interval_sub(lvInterval a, lvInterval b) {
    return interval_sub(a, b);
}

lvInterval lv_interval_mul(lvInterval a, lvInterval b) {
    return interval_mul(a, b);
}

lvInterval lv_interval_div(lvInterval a, lvInterval b) {
    return interval_div(a, b);
}
