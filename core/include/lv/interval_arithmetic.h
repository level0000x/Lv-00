#ifndef lv_INTERVAL_ARITHMETIC_H
#define lv_INTERVAL_ARITHMETIC_H

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
    int is_exact;
} lvInterval;

/* ── Interval config ── */
typedef struct {
    int precision;
    double rounding_eps;
} lvIntervalConfig;

/* ── Constructor / factory ── */
lvInterval interval_create(double lo, double hi, int is_exact);
lvInterval interval_point(double val);
lvInterval interval_empty(void);
lvInterval interval_entire(void);
lvIntervalConfig interval_config_default(void);

/* ── Operations ── */
lvInterval interval_add(lvInterval a, lvInterval b);
lvInterval interval_sub(lvInterval a, lvInterval b);
lvInterval interval_mul(lvInterval a, lvInterval b);
lvInterval interval_div(lvInterval a, lvInterval b);
lvInterval interval_sqrt(lvInterval a);
lvInterval interval_sin(lvInterval a);
lvInterval interval_cos(lvInterval a);
lvInterval interval_exp(lvInterval a);
lvInterval interval_log(lvInterval a);
lvInterval interval_abs(lvInterval a);
lvInterval interval_neg(lvInterval a);
lvInterval interval_intersect(lvInterval a, lvInterval b);
lvInterval interval_union(lvInterval a, lvInterval b);

/* ── Queries ── */
double interval_diam(lvInterval a);
double interval_mid(lvInterval a);
int interval_is_empty(lvInterval a);
int interval_contains(lvInterval a, double val);
int interval_is_subset(lvInterval a, lvInterval b);
int interval_equals(lvInterval a, lvInterval b);

/* ── Symbolic / string ── */
lvInterval interval_from_symbolic(const char *expr_str, const char **var_names, const lvInterval *var_intervals,
                                  int var_count);
int interval_to_symbolic(lvInterval a, char *buf, size_t buf_size);

/* ── Verification ── */
int interval_verify_solution(lvInterval f_interval, double tolerance);
int interval_verify_adaptive(const char *expr_str, const char **var_names, lvInterval *var_intervals, int var_count,
                             int max_depth, double tolerance);

/* 注：原 lv_ prefix legacy 包装（lv_interval_add/sub/mul/div）已迁移至
 * 公共区间算术库 interval_arith.h/c（以 float_error 语义为基准，定义域外
 * 返回全实数而非空区间）。本头文件仅保留 IEEE 1788 空区间语义的 interval_* API。 */

#ifdef __cplusplus
}
#endif
#endif
