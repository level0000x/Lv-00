/**
 * @file lv00_numeric.h
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

#ifndef LV00_NUMERIC_H
#define LV00_NUMERIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <stdbool.h>

#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/* ============================================================
 * 数学常量
 * ============================================================ */

/** @brief 圆周率 π */
#ifndef LV00_PI
#define LV00_PI 3.14159265358979323846
#endif

/** @brief 自然常数 e */
#define LV00_E 2.71828182845904523536

/** @brief 默认浮点比较精度阈值 */
#define LV00_EPSILON 1e-12

/** @brief 正无穷大 */
#define LV00_INFINITY INFINITY

/* ============================================================
 * 浮点比较工具
 * ============================================================ */

/**
 * @brief 判断浮点数是否接近零
 * @param x 要判断的值
 * @param epsilon 精度阈值
 * @return true 如果 |x| < epsilon
 */
LV00_PUBLIC_API bool lv00_is_zero(double x, double epsilon);

/**
 * @brief 判断两个浮点数是否近似相等
 * @param a 第一个值
 * @param b 第二个值
 * @param epsilon 精度阈值
 * @return true 如果 |a - b| < epsilon
 */
LV00_PUBLIC_API bool lv00_is_equal(double a, double b, double epsilon);

/**
 * @brief 判断浮点数是否为正数（大于 epsilon）
 * @param x 要判断的值
 * @param epsilon 精度阈值
 * @return true 如果 x > epsilon
 */
LV00_PUBLIC_API bool lv00_is_positive(double x, double epsilon);

/**
 * @brief 判断浮点数是否为负数（小于 -epsilon）
 * @param x 要判断的值
 * @param epsilon 精度阈值
 * @return true 如果 x < -epsilon
 */
LV00_PUBLIC_API bool lv00_is_negative(double x, double epsilon);

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
LV00_PUBLIC_API bool lv00_is_in_range(double x, double lo, double hi);

/**
 * @brief 将值限制在指定范围内
 * @param x 输入值
 * @param lo 下界
 * @param hi 上界
 * @return 限制后的值：lo <= result <= hi
 */
LV00_PUBLIC_API double lv00_clamp(double x, double lo, double hi);

/* ============================================================
 * 角度转换
 * ============================================================ */

/**
 * @brief 角度转弧度
 * @param deg 角度值（度数）
 * @return 对应的弧度值
 */
LV00_PUBLIC_API double lv00_deg_to_rad(double deg);

/**
 * @brief 弧度转角度
 * @param rad 弧度值
 * @return 对应的角度值（度数）
 */
LV00_PUBLIC_API double lv00_rad_to_deg(double rad);

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
static inline double lv00_lerp(double a, double b, double t)
{
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
LV00_PUBLIC_API double lv00_sign(double x);

/**
 * @brief 整数符号函数
 * @param x 输入值
 * @return x > 0 返回 1，x < 0 返回 -1，x == 0 返回 0
 */
LV00_PUBLIC_API int lv00_sign_int(int x);

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
LV00_PUBLIC_API double lv00_evaluate_quadratic(double a, double b, double c, double x);

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
LV00_PUBLIC_API double lv00_evaluate_cubic(double a, double b, double c, double d, double x);

#ifdef __cplusplus
}
#endif

#endif /* LV00_NUMERIC_H */
