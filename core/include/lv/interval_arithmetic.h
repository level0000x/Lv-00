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

/* ========================================================================
 * 与 interval_arith.h/c（lv_interval_*）的关系 —— 并存而非重复
 *
 * 项目曾存在多套区间算术实现，现已收敛为两套互补库，共用同一
 * lvInterval 类型（lo/hi/is_exact），但语义基准不同，不可互相转发：
 *
 * 1) 本文件 interval_*（IEEE 1788 空区间语义）：
 *    - 定义域外（除零、sqrt/log 负下界）返回 interval_empty()（lo=1,hi=-1），
 *      空区间可被 interval_is_empty/intersect/union/from_symbolic 等显式处理。
 *    - round_down(0.0) = -DBL_TRUE_MIN（nextafter 严格方向），与 float_error
 *      的 round_down(0.0) = -0.0 逐位不同。
 *    - sin/cos 通过枚举极值点（ceil/floor 范围内逐个检查 k*pi、pi/2+k*pi）。
 *    - 独有功能：interval_from_symbolic 表达式解析、interval_verify_solution/
 *      adaptive 自适应验证、intersect/union 集合运算 —— interval_arith 无。
 *    选择依据：符号求值与自适应验证依赖"空区间"信号做分支与二分判定，
 *    且"空集是任何集合的子集"等集合代数（interval_is_subset/equals）需要
 *    空区间语义，故本文件保留独立实现。
 *
 * 2) interval_arith.h/c 的 lv_interval_*（float_error 语义基准）：
 *    - 定义域外返回保守全实数区间 [-HUGE_VAL, HUGE_VAL]，避免 FPTaylor 的
 *      half_width 因空区间变负而误判 TrustColor。
 *    - round_down(0.0) = -0.0。
 *    - sin/cos 采用 float_error 的 ceil/floor + fmod(k,2) 极值点算法。
 *    - 独有扩展：tan/atan/pow/asin/acos/floor/ceil。
 *    选择依据：数值验证（fptaylor 误差界）与 gappa_propagate 的区间传播。
 *
 * 结论：两者 API 集合、定义域外语义、舍入细节、极值点算法均不同。
 *
 * 【v1.1.0 收口】interval_arithmetic.c 已删除：interval_* 实现并入
 * interval_arith.c 末尾的「废弃兼容层」，geo_predicate.c 已迁移到
 * lv_interval_*。interval_* 仅为测试与兼容用途保留（deprecated），
 * 新代码请使用 lv_interval_*（interval_arith.h）。
 * ======================================================================== */

#ifdef __cplusplus
}
#endif
#endif
