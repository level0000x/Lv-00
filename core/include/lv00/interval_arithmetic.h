#ifndef LV00_INTERVAL_ARITHMETIC_H
#define LV00_INTERVAL_ARITHMETIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

/* ── Interval type ── */
typedef struct {
    double lo;
    double hi;
    int    is_exact;
} Lv00Interval;

/* ── Interval config ── */
typedef struct {
    int    precision;
    double rounding_eps;
} Lv00IntervalConfig;

/* ── Constructor / factory ── */
Lv00Interval  interval_create(double lo, double hi, int is_exact);
Lv00Interval  interval_point(double val);
Lv00Interval  interval_empty(void);
Lv00Interval  interval_entire(void);
Lv00IntervalConfig interval_config_default(void);

/* ── Operations ── */
Lv00Interval  interval_add(Lv00Interval a, Lv00Interval b);
Lv00Interval  interval_sub(Lv00Interval a, Lv00Interval b);
Lv00Interval  interval_mul(Lv00Interval a, Lv00Interval b);
Lv00Interval  interval_div(Lv00Interval a, Lv00Interval b);
Lv00Interval  interval_sqrt(Lv00Interval a);
Lv00Interval  interval_sin(Lv00Interval a);
Lv00Interval  interval_cos(Lv00Interval a);
Lv00Interval  interval_exp(Lv00Interval a);
Lv00Interval  interval_log(Lv00Interval a);
Lv00Interval  interval_abs(Lv00Interval a);
Lv00Interval  interval_neg(Lv00Interval a);
Lv00Interval  interval_intersect(Lv00Interval a, Lv00Interval b);
Lv00Interval  interval_union(Lv00Interval a, Lv00Interval b);

/* ── Queries ── */
double        interval_diam(Lv00Interval a);
double        interval_mid(Lv00Interval a);
int           interval_is_empty(Lv00Interval a);
int           interval_contains(Lv00Interval a, double val);
int           interval_is_subset(Lv00Interval a, Lv00Interval b);
int           interval_equals(Lv00Interval a, Lv00Interval b);

/* ── Symbolic / string ── */
Lv00Interval  interval_from_symbolic(const char *expr_str,
                                      const char **var_names,
                                      const Lv00Interval *var_intervals,
                                      int var_count);
int           interval_to_symbolic(Lv00Interval a, char *buf, size_t buf_size);

/* ── Verification ── */
int           interval_verify_solution(Lv00Interval f_interval, double tolerance);
int           interval_verify_adaptive(const char *expr_str,
                                        const char **var_names,
                                        Lv00Interval *var_intervals,
                                        int var_count,
                                        int max_depth,
                                        double tolerance);

/* ── Legacy stubs ── */
Lv00Interval lv00_interval_add(Lv00Interval a, Lv00Interval b);
Lv00Interval lv00_interval_sub(Lv00Interval a, Lv00Interval b);
Lv00Interval lv00_interval_mul(Lv00Interval a, Lv00Interval b);
Lv00Interval lv00_interval_div(Lv00Interval a, Lv00Interval b);

#ifdef __cplusplus
}
#endif
#endif
