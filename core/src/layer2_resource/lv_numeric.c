/**
 * @file lv_numeric.c
 * @brief Lv-00 数值工具模块实现 - 基础数值计算工具
 *
 * 实现功能：
 * - 浮点比较工具（带 epsilon 的安全比较）
 * - 数值范围检查与限制
 * - 角度与弧度转换
 * - 符号函数
 * - 多项式求值（Horner 方法）
 *
 * @version 1.1.0
 */

#include "lv/lv_numeric.h"

#include <math.h>
#include <string.h>

#include "lv/lv_utils.h"

/* ============================================================
 * 浮点比较工具
 * ============================================================ */

/**
 * @brief 判断浮点数是否接近零
 * @param x 要判断的值
 * @param epsilon 精度阈值
 * @return true 如果 |x| < epsilon
 */
bool lv_is_zero(double x, double epsilon) {
    return fabs(x) < epsilon;
}

/**
 * @brief 判断两个浮点数是否近似相等
 * @param a 第一个值
 * @param b 第二个值
 * @param epsilon 精度阈值
 * @return true 如果 |a - b| < epsilon
 */
bool lv_is_equal(double a, double b, double epsilon) {
    return fabs(a - b) < epsilon;
}

/**
 * @brief 判断浮点数是否为正数（大于 epsilon）
 * @param x 要判断的值
 * @param epsilon 精度阈值
 * @return true 如果 x > epsilon
 */
bool lv_is_positive(double x, double epsilon) {
    return x > epsilon;
}

/**
 * @brief 判断浮点数是否为负数（小于 -epsilon）
 * @param x 要判断的值
 * @param epsilon 精度阈值
 * @return true 如果 x < -epsilon
 */
bool lv_is_negative(double x, double epsilon) {
    return x < -epsilon;
}

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
bool lv_is_in_range(double x, double lo, double hi) {
    return x >= lo && x <= hi;
}

/**
 * @brief 将值限制在指定范围内
 * @param x 输入值
 * @param lo 下界
 * @param hi 上界
 * @return 限制后的值：lo <= result <= hi
 */
double lv_clamp(double x, double lo, double hi) {
    if (x < lo)
        return lo;
    if (x > hi)
        return hi;
    return x;
}

/* ============================================================
 * 角度转换
 * ============================================================ */

/**
 * @brief 角度转弧度
 * @param deg 角度值（度数）
 * @return 对应的弧度值
 *
 * 运算顺序 deg * lv_PI / 180.0（先乘后除）与全库既有
 * `deg * M_PI / 180.0` 魔法表达式保持一致（lv_PI 与 M_PI
 * 在 double 精度下取值相同），替换后数值结果逐位一致。
 */
double lv_deg_to_rad(double deg) {
    return deg * lv_PI / 180.0;
}

/**
 * @brief 弧度转角度
 * @param rad 弧度值
 * @return 对应的角度值（度数）
 *
 * 运算顺序 rad * 180.0 / lv_PI（先乘后除）与全库既有
 * `rad * 180.0 / M_PI` 魔法表达式保持一致。
 */
double lv_rad_to_deg(double rad) {
    return rad * 180.0 / lv_PI;
}

/* ============================================================
 * 符号函数
 * ============================================================ */

/**
 * @brief 浮点数符号函数
 * @param x 输入值
 * @return x > 0 返回 1.0，x < 0 返回 -1.0，x == 0 返回 0.0
 */
double lv_sign(double x) {
    if (x > 0.0)
        return 1.0;
    if (x < 0.0)
        return -1.0;
    return 0.0;
}

/**
 * @brief 整数符号函数
 * @param x 输入值
 * @return x > 0 返回 1，x < 0 返回 -1，x == 0 返回 0
 */
int lv_sign_int(int x) {
    if (x > 0)
        return 1;
    if (x < 0)
        return -1;
    return 0;
}

/* ============================================================
 * 多项式求值（Horner 方法）
 * ============================================================ */

/**
 * @brief 计算二次多项式 a*x^2 + b*x + c 的值（Horner 方法）
 *
 * 使用 Horner 格式 ((a * x) + b) * x + c。
 *
 * @param a 二次项系数
 * @param b 一次项系数
 * @param c 常数项
 * @param x 自变量的值
 * @return 多项式在 x 处的值
 */
double lv_evaluate_quadratic(double a, double b, double c, double x) {
    return (a * x + b) * x + c;
}

/**
 * @brief 计算三次多项式 a*x^3 + b*x^2 + c*x + d 的值（Horner 方法）
 *
 * 使用 Horner 格式 (((a * x) + b) * x + c) * x + d。
 *
 * @param a 三次项系数
 * @param b 二次项系数
 * @param c 一次项系数
 * @param d 常数项
 * @param x 自变量的值
 * @return 多项式在 x 处的值
 */
double lv_evaluate_cubic(double a, double b, double c, double d, double x) {
    return ((a * x + b) * x + c) * x + d;
}

/* ============================================================
 * double → mpq 转换
 * ============================================================ */

/**
 * @brief 从 double 构造 mpq_t 有理数（初始化 + 赋值）
 *
 * 等价于 mpq_init(q) + mpq_set_d(q, v)，见 lv_numeric.h 的语义说明。
 */
void lv_mpq_set_d_checked(mpq_t q, double v) {
    mpq_init(q);
    mpq_set_d(q, v);
}

/* ============================================================
 * 有限差分工具（数值微分）
 * ============================================================ */

/**
 * @brief 标量函数 f 在 x 处的一阶导数（有限差分近似）
 *
 * 统一实现，收敛 float_error.c / fptaylor_eval.c / geom_evol.c /
 * geo_constraint_solver_newton.c 四处手写差分：
 * - lv_FD_CENTRAL：(f(x+h) - f(x-h)) / (2h)
 * - lv_FD_FORWARD：(f(x+h) - f(x)) / h
 * h > 0 时用调用方自定义步长（绝对量），h <= 0 时用默认自适应策略
 * h = lv_NUMERICAL_DIFF_EPSILON * max(1, |x|)。
 */
double lv_finite_difference(lvFdFunc f, double x, double h, lvDiffScheme scheme, void *userdata) {
    double f_hi, f_lo;

    if (!f)
        return NAN;
    if (!(h > 0.0)) {
        h = lv_fd_step_adaptive(x, lv_NUMERICAL_DIFF_EPSILON);
    }

    /* 前向差分：基准点为 x（不额外扰动），扰动点为 x+h */
    if (scheme == lv_FD_FORWARD) {
        f_hi = f(x + h, userdata);
        f_lo = f(x, userdata);
        if (isnan(f_hi) || isnan(f_lo))
            return NAN;
        return (f_hi - f_lo) / h;
    }

    /* 中心差分：扰动点为 x+h 与 x-h */
    f_hi = f(x + h, userdata);
    f_lo = f(x - h, userdata);
    if (isnan(f_hi) || isnan(f_lo))
        return NAN;
    return (f_hi - f_lo) / (2.0 * h);
}

/**
 * @brief 向量函数 F(x)=(F_0(x),...,F_{n-1}(x)) 的一阶导数（逐分量有限差分）
 *
 * 一次扰动求值得到整向量后逐分量差分，避免逐分量重复求值
 * （原 geom_evol.c BDF / geo_constraint_solver_newton.c 雅可比构建形态）。
 * FORWARD 且 f_base 非 NULL 时复用调用方已算好的基准值 F(x)，
 * 求值次数与调用方原实现一致。
 */
int lv_finite_difference_vec(lvFdVecFunc fn, void *userdata, double x, double h, lvDiffScheme scheme,
                             const double *f_base, double *df, int n) {
    double *tmp_hi, *tmp_lo;
    const double *base;
    int i, ret;

    if (!fn || !df || n <= 0)
        return -1;
    if (!(h > 0.0)) {
        h = lv_fd_step_adaptive(x, lv_NUMERICAL_DIFF_EPSILON);
    }

    tmp_hi = (double *) lv_malloc((size_t) n * sizeof(double));
    if (!tmp_hi)
        return -1;
    tmp_lo = (double *) lv_malloc((size_t) n * sizeof(double));
    if (!tmp_lo) {
        lv_free((void **) &tmp_hi);
        return -1;
    }

    ret = fn(x + h, userdata, tmp_hi, n);
    if (ret != 0) {
        lv_free((void **) &tmp_hi);
        lv_free((void **) &tmp_lo);
        return -1;
    }

    if (scheme == lv_FD_FORWARD) {
        /* 基准点：优先复用调用方提供的 f_base，否则内部求值 */
        base = f_base;
        if (!base) {
            ret = fn(x, userdata, tmp_lo, n);
            if (ret != 0) {
                lv_free((void **) &tmp_hi);
                lv_free((void **) &tmp_lo);
                return -1;
            }
            base = tmp_lo;
        }
        for (i = 0; i < n; i++) {
            df[i] = (tmp_hi[i] - base[i]) / h;
        }
    } else {
        /* 中心差分 */
        ret = fn(x - h, userdata, tmp_lo, n);
        if (ret != 0) {
            lv_free((void **) &tmp_hi);
            lv_free((void **) &tmp_lo);
            return -1;
        }
        for (i = 0; i < n; i++) {
            df[i] = (tmp_hi[i] - tmp_lo[i]) / (2.0 * h);
        }
    }

    lv_free((void **) &tmp_hi);
    lv_free((void **) &tmp_lo);
    return 0;
}
