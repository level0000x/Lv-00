/**
 * @file preset_numerical_analysis.h
 * @brief 数值分析预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的数值分析预设函数块，包括：
 *   - 数值积分：梯形法则、辛普森法则、高斯求积、龙贝格积分、蒙特卡洛积分
 *   - 方程求解：二分法、牛顿法、割线法、不动点迭代、非线性方程组
 *   - 插值方法：拉格朗日插值、牛顿均差插值、线性样条、三次样条、切比雪夫插值
 *   - 数值微分：前向差分、后向差分、中心差分、理查森外推
 *   - 常微分方程：欧拉方法、四阶龙格-库塔、自适应步长、ODE方程组
 *   - 矩阵运算：LU分解、特征值计算
 *
 * @module NumericalAnalysis
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 3.2.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_NUMERICAL_ANALYSIS_H
#define LV00_PRESET_NUMERICAL_ANALYSIS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 数值积分 -------------------- */

/** 梯形法则 */
#define PRESET_NUMERICAL_INTEGRAL_TRAPEZOID  "numerical_integral_trapezoid"

/** 辛普森法则 */
#define PRESET_NUMERICAL_INTEGRAL_SIMPSON    "numerical_integral_simpson"

/** 高斯求积 */
#define PRESET_NUMERICAL_INTEGRAL_GAUSS      "numerical_integral_gauss"

/** 龙贝格积分 */
#define PRESET_NUMERICAL_INTEGRAL_ROMBERG    "numerical_integral_romberg"

/** 蒙特卡洛积分 */
#define PRESET_NUMERICAL_INTEGRAL_MONTE_CARLO "numerical_integral_monte_carlo"

/* -------------------- 方程求解 -------------------- */

/** 二分法求根 */
#define PRESET_ROOT_BISECTION                "root_bisection"

/** 牛顿法求根 */
#define PRESET_ROOT_NEWTON                   "root_newton"

/** 割线法求根 */
#define PRESET_ROOT_SECANT                   "root_secant"

/** 不动点迭代 */
#define PRESET_ROOT_FIXED_POINT              "root_fixed_point"

/** 非线性方程组牛顿法 */
#define PRESET_SYSTEM_NEWTON                 "system_newton"

/* -------------------- 插值方法 -------------------- */

/** 拉格朗日插值 */
#define PRESET_INTERPOLATION_LAGRANGE        "interpolation_lagrange"

/** 牛顿均差插值 */
#define PRESET_INTERPOLATION_NEWTON          "interpolation_newton"

/** 线性样条插值 */
#define PRESET_INTERPOLATION_SPLINE_LINEAR   "interpolation_spline_linear"

/** 三次样条插值 */
#define PRESET_INTERPOLATION_SPLINE_CUBIC    "interpolation_spline_cubic"

/** 切比雪夫插值 */
#define PRESET_INTERPOLATION_CHEBYSHEV       "interpolation_chebyshev"

/* -------------------- 数值微分 -------------------- */

/** 前向差分 */
#define PRESET_DIFFERENTIATION_FORWARD       "differentiation_forward"

/** 后向差分 */
#define PRESET_DIFFERENTIATION_BACKWARD      "differentiation_backward"

/** 中心差分 */
#define PRESET_DIFFERENTIATION_CENTRAL       "differentiation_central"

/** 理查森外推 */
#define PRESET_DIFFERENTIATION_RICHARDSON    "differentiation_richardson"

/* -------------------- 常微分方程 -------------------- */

/** 欧拉方法 */
#define PRESET_ODE_EULER                     "ode_euler"

/** 四阶龙格-库塔方法 */
#define PRESET_ODE_RK4                       "ode_rk4"

/** 自适应步长方法 */
#define PRESET_ODE_ADAPTIVE                  "ode_adaptive"

/** ODE方程组求解 */
#define PRESET_ODE_SYSTEM                    "ode_system"

/* -------------------- 矩阵运算 -------------------- */

/** LU分解 */
#define PRESET_MATRIX_LU_DECOMPOSE           "matrix_lu_decompose"

/** 特征值计算 */
#define PRESET_MATRIX_EIGENVALUES            "matrix_eigenvalues"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有数值分析预设函数块
 *
 * 将数值分析模块的所有预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_numerical_analysis_register(void);

/**
 * @brief 获取数值分析预设函数块数量
 *
 * @return int 数值分析模块预设函数块总数
 */
int preset_numerical_analysis_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_NUMERICAL_ANALYSIS_H */
