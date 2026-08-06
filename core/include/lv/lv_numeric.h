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

/** @brief 默认浮点比较精度阈值 */
#define lv_EPSILON 1e-12

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

#ifdef __cplusplus
}
#endif

#endif /* lv_NUMERIC_H */
