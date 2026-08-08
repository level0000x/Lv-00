/**
 * @file lv_numeric.h
 * @brief Lv-00 数值工具模块 - 提供基础数值计算工具与数学常量
 *
 * 本模块提供：
 * - 浮点比较工具（带 epsilon 的安全比较）
 * - 数值范围检查与限制
 * - 数学常量定义
 * - 角度与弧度转换
 * - 线性插值
 * - 符号函数
 * - 多项式求值（Horner 方法）
 *
 * @version 1.1.0
 */

#ifndef lv_NUMERIC_H
#define lv_NUMERIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <stdbool.h>
#include <gmp.h>

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ============================================================
 * 数学常量
 * ============================================================ */

/** @brief 圆周率 π（精度与 lv_config.h 的 lv_PI 保持一致，22 位） */
#ifndef lv_PI
#define lv_PI 3.1415926535897932384626
#endif

/** @brief 自然常数 e */
#define lv_E 2.71828182845904523536

/**
 * @brief 默认浮点比较精度阈值
 *
 * 语义别名 = lv_EPSILON_DOUBLE（lv_utils.h，1e-12）。本文件未包含
 * lv_utils.h，此宏在核心代码中无使用点（仅 lv-formal Lean 侧引用），
 * 展开时需保证 lv_utils.h 已可见。
 */
#define lv_EPSILON lv_EPSILON_DOUBLE

/** @brief 正无穷大 */
#define lv_INFINITY INFINITY

/* ============================================================
 * 浮点比较工具
 * ============================================================ */

/**
 * @brief 判断浮点数是否接近零
 * @param x 要判断的值
 * @param epsilon 精度阈值
 * @return true 如果 |x| < epsilon
 */
lv_PUBLIC_API bool lv_is_zero(double x, double epsilon);

/**
 * @brief 判断两个浮点数是否近似相等
 * @param a 第一个值
 * @param b 第二个值
 * @param epsilon 精度阈值
 * @return true 如果 |a - b| < epsilon
 */
lv_PUBLIC_API bool lv_is_equal(double a, double b, double epsilon);

/**
 * @brief 判断浮点数是否为正数（大于 epsilon）
 * @param x 要判断的值
 * @param epsilon 精度阈值
 * @return true 如果 x > epsilon
 */
lv_PUBLIC_API bool lv_is_positive(double x, double epsilon);

/**
 * @brief 判断浮点数是否为负数（小于 -epsilon）
 * @param x 要判断的值
 * @param epsilon 精度阈值
 * @return true 如果 x < -epsilon
 */
lv_PUBLIC_API bool lv_is_negative(double x, double epsilon);

/* ============================================================
 * 数值范围检查
 * ============================================================ */

/**
 * @brief 判断值是否在指定范围内（闭区间）
 * @param x 要检查的值
 * @param lo 下界
 * @param hi 上界
 * @return true 如果 lo <= x <= hi
 */
lv_PUBLIC_API bool lv_is_in_range(double x, double lo, double hi);

/**
 * @brief 将值限制在指定范围内
 * @param x 输入值
 * @param lo 下界
 * @param hi 上界
 * @return 限制后的值：lo <= result <= hi
 */
lv_PUBLIC_API double lv_clamp(double x, double lo, double hi);

/* ============================================================
 * 角度转换
 * ============================================================ */

/**
 * @brief 角度转弧度
 * @param deg 角度值（度数）
 * @return 对应的弧度值
 */
lv_PUBLIC_API double lv_deg_to_rad(double deg);

/**
 * @brief 弧度转角度
 * @param rad 弧度值
 * @return 对应的角度值（度数）
 */
lv_PUBLIC_API double lv_rad_to_deg(double rad);

/* ============================================================
 * 插值工具
 * ============================================================ */

/**
 * @brief 线性插值
 *
 * 计算 a + t * (b - a)，当 t = 0 时返回 a，t = 1 时返回 b。
 *
 * @param a 起始值
 * @param b 终止值
 * @param t 插值参数（通常在 [0, 1] 区间内）
 * @return 插值结果
 */
static inline double lv_lerp(double a, double b, double t) {
    return a + t * (b - a);
}

/* ============================================================
 * 符号函数
 * ============================================================ */

/**
 * @brief 浮点数符号函数
 * @param x 输入值
 * @return x > 0 返回 1.0，x < 0 返回 -1.0，x == 0 返回 0.0
 */
lv_PUBLIC_API double lv_sign(double x);

/**
 * @brief 整数符号函数
 * @param x 输入值
 * @return x > 0 返回 1，x < 0 返回 -1，x == 0 返回 0
 */
lv_PUBLIC_API int lv_sign_int(int x);

/* ============================================================
 * 多项式求值（Horner 方法）
 * ============================================================ */

/**
 * @brief 计算二次多项式 a*x^2 + b*x + c 的值（Horner 方法）
 *
 * 使用 Horner 格式 ((a * x) + b) * x + c，减少乘法次数并提高数值稳定性。
 *
 * @param a 二次项系数
 * @param b 一次项系数
 * @param c 常数项
 * @param x 自变量的值
 * @return 多项式在 x 处的值
 */
lv_PUBLIC_API double lv_evaluate_quadratic(double a, double b, double c, double x);

/**
 * @brief 计算三次多项式 a*x^3 + b*x^2 + c*x + d 的值（Horner 方法）
 *
 * 使用 Horner 格式 (((a * x) + b) * x + c) * x + d，减少乘法次数并提高数值稳定性。
 *
 * @param a 三次项系数
 * @param b 二次项系数
 * @param c 一次项系数
 * @param d 常数项
 * @param x 自变量的值
 * @return 多项式在 x 处的值
 */
lv_PUBLIC_API double lv_evaluate_cubic(double a, double b, double c, double d, double x);

/* ============================================================
 * double → mpq 转换
 * ============================================================ */

/**
 * @brief 从 double 构造 mpq_t 有理数（初始化 + 赋值）
 *
 * 封装 mpq_init + mpq_set_d 样板，替代各处的「mpq_t q; mpq_init(q);
 * mpq_set_d(q, v);」重复代码。注意：
 * - q 在调用前必须是未初始化状态（与 mpq_init 语义一致），使用完毕后
 *   由调用方 mpq_clear / mpq_clears 释放；
 * - 不做 isfinite 防御（与现有调用点行为保持一致：lv_impl_native.c
 *   无守卫，double_to_mpz_scaled 在调用前自行 isfinite 检查）；
 * - 语义与裸 mpq_init + mpq_set_d 完全一致，mpq_set_d 保留 double 的
 *   最佳精度二进制分数表示。
 *
 * @param q 输出 mpq_t（未初始化）
 * @param v 输入 double 值
 */
lv_PUBLIC_API void lv_mpq_set_d_checked(mpq_t q, double v);

/* ============================================================
 * 有限差分工具（数值微分，统一 4 处手写差分实现）
 * ============================================================ */

/**
 * @brief 统一数值差分步长基准常量。
 *
 * 收敛来源（原三处独立常量，量级一致 ~1e-8）：
 * - geo_constraint_solver_newton.c 的 NUMERICAL_DIFF_EPSILON（1e-8）
 * - geom_evol.c BDF 雅可比构建的 fd_eps（1e-8）
 * - float_error.c 中心差分的 sqrt(DBL_EPSILON)（≈1.49e-8）
 *
 * 统一取 1e-8 作为默认相对步长基准。各调用方如需保持历史数值行为，
 * 可显式传入自定义步长 h 或自有 eps（如 float_error.c 保留
 * sqrt(DBL_EPSILON) 作为中心差分 O(h^2) 截断/舍入平衡的语义别名）。
 */
#define lv_NUMERICAL_DIFF_EPSILON 1e-8

/** @brief 有限差分格式 */
typedef enum lvDiffScheme {
    lv_FD_CENTRAL = 0, /**< 中心差分 (f(x+h)-f(x-h))/(2h)，截断误差 O(h^2) */
    lv_FD_FORWARD = 1  /**< 前向差分 (f(x+h)-f(x))/h，截断误差 O(h) */
} lvDiffScheme;

/**
 * @brief 标量函数回调：给定扰动后的自变量值 x，返回函数值 f(x)。
 * @param x        扰动后的自变量值
 * @param userdata 函数上下文（表达式、变量数组等）
 * @return f(x)；求值失败返回 NaN
 */
typedef double (*lvFdFunc)(double x, void *userdata);

/**
 * @brief 向量函数回调：F: R -> R^n，给定扰动后的自变量值 x，
 *        将 n 个输出分量写入 out。
 * @param x        扰动后的自变量值
 * @param userdata 函数上下文
 * @param out      输出数组（长度 >= n）
 * @param n        输出分量数
 * @return 0 表示成功，非 0 表示求值失败
 */
typedef int (*lvFdVecFunc)(double x, void *userdata, double *out, int n);

/**
 * @brief 标量函数 f 在 x 处的一阶导数（有限差分近似）。
 *
 * - h > 0：使用调用方自定义步长（绝对量，扰动点为 x+h / x-h）；
 * - h <= 0：使用默认自适应策略 h = lv_NUMERICAL_DIFF_EPSILON * max(1, |x|)。
 *
 * @param f       标量函数回调（不可为 NULL）
 * @param x       求导中心点
 * @param h       步长（<= 0 时使用默认策略）
 * @param scheme  差分格式（中心/前向）
 * @param userdata 透传给 f 的上下文
 * @return 一阶导数近似值；f 在任一扰动点返回 NaN 时返回 NaN
 */
lv_PUBLIC_API double lv_finite_difference(lvFdFunc f, double x, double h, lvDiffScheme scheme, void *userdata);

/**
 * @brief 向量函数 F(x)=(F_0(x),...,F_{n-1}(x)) 的一阶导数（逐分量有限差分）。
 *
 * 一次扰动求值得到整向量，再对每个分量做差分，避免逐分量重复求值。
 * h 语义与 lv_finite_difference 相同（h <= 0 使用默认自适应策略）。
 *
 * @param fn       向量函数回调（不可为 NULL，返回 0 成功）
 * @param userdata 透传给 fn 的上下文
 * @param x        求导中心点
 * @param h        步长（<= 0 时使用默认策略）
 * @param scheme   差分格式（中心/前向）
 * @param f_base   可选：x 处的基准函数值 F(x)（长度 >= n），
 *                 NULL 时内部计算；仅 lv_FD_FORWARD 使用
 * @param df       输出导数向量（长度 >= n）
 * @param n        向量维度
 * @return 0 成功；回调失败或分配失败返回 -1（df 内容不变）
 */
lv_PUBLIC_API int lv_finite_difference_vec(lvFdVecFunc fn, void *userdata, double x, double h, lvDiffScheme scheme,
                                           const double *f_base, double *df, int n);

/**
 * @brief 自适应相对步长：eps * max(1, |x|)。
 *
 * 原语义：float_error.c finite_difference_partial 与
 * geo_constraint_solver_newton.c build_jacobian_and_residual 的自适应步长策略，
 * 保证 |x| 很大时扰动仍可区分、|x| 很小时不放大扰动。
 */
static inline double lv_fd_step_adaptive(double x, double eps) {
    return eps * fmax(1.0, fabs(x));
}

/**
 * @brief 相对步长（|x|+1 型）：eps * (|x| + 1)。
 *
 * 原语义：geom_evol.c BDF 雅可比构建的扰动步长，保证 |x|+1 > 1。
 */
static inline double lv_fd_step_relative(double x, double eps) {
    return eps * (fabs(x) + 1.0);
}

#ifdef __cplusplus
}
#endif

#endif /* lv_NUMERIC_H */
