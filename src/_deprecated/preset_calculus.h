/**
 * @file preset_calculus.h
 * @brief 微积分预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的微积分运算预设函数块，包括：
 *   - 极限运算：数列极限、函数极限、左/右极限、无穷极限、不定式极限
 *   - 微分运算：导数定义、幂函数导数、链式法则、乘积法则、商法则、
 *               隐函数求导、参数方程求导、偏导数
 *   - 积分运算：不定积分、定积分、换元积分法、分部积分法、
 *               部分分式积分、三角积分、反常积分、曲线积分
 *   - 级数展开：Taylor级数、Maclaurin级数、Fourier级数、幂级数
 *   - 多元微积分：梯度、散度、旋度、Laplace算子
 *
 * @module Calculus
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#ifndef PRESET_CALCULUS_H
#define PRESET_CALCULUS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 极限运算 -------------------- */

/** 数列极限：lim_{n->inf} a_n */
#define PRESET_LIMIT_SEQUENCE          "limit_sequence"

/** 函数极限：lim_{x->a} f(x) */
#define PRESET_LIMIT_FUNCTION          "limit_function"

/** 左极限：lim_{x->a^-} f(x) */
#define PRESET_LIMIT_LEFT              "limit_left"

/** 右极限：lim_{x->a^+} f(x) */
#define PRESET_LIMIT_RIGHT             "limit_right"

/** 无穷极限：lim_{x->inf} f(x) */
#define PRESET_LIMIT_INFINITY          "limit_infinity"

/** 不定式极限（L'Hopital法则） */
#define PRESET_LIMIT_INDETERMINATE     "limit_indeterminate"

/* -------------------- 微分运算 -------------------- */

/** 导数定义：f'(x) = lim_{h->0} [f(x+h)-f(x)]/h */
#define PRESET_DERIVATIVE_DEFINITION   "derivative_definition"

/** 幂函数导数：d/dx[x^n] = nx^{n-1} */
#define PRESET_DERIVATIVE_POWER        "derivative_power"

/** 链式法则：d/dx[f(g(x))] = f'(g(x))g'(x) */
#define PRESET_DERIVATIVE_CHAIN        "derivative_chain"

/** 乘积法则：(fg)' = f'g + fg' */
#define PRESET_DERIVATIVE_PRODUCT      "derivative_product"

/** 商法则：(f/g)' = (f'g - fg')/g^2 */
#define PRESET_DERIVATIVE_QUOTIENT     "derivative_quotient"

/** 隐函数求导 */
#define PRESET_DERIVATIVE_IMPLICIT     "derivative_implicit"

/** 参数方程求导 */
#define PRESET_DERIVATIVE_PARAMETRIC   "derivative_parametric"

/** 偏导数：df/dx */
#define PRESET_DERIVATIVE_PARTIAL      "derivative_partial"

/* -------------------- 积分运算 -------------------- */

/** 不定积分：int f(x)dx */
#define PRESET_INTEGRAL_INDEFINITE     "integral_indefinite"

/** 定积分：int_a^b f(x)dx */
#define PRESET_INTEGRAL_DEFINITE       "integral_definite"

/** 换元积分法 */
#define PRESET_INTEGRAL_SUBSTITUTION   "integral_substitution"

/** 分部积分法：int u dv = uv - int v du */
#define PRESET_INTEGRAL_BY_PARTS       "integral_by_parts"

/** 部分分式积分 */
#define PRESET_INTEGRAL_PARTIAL_FRACTION "integral_partial_fraction"

/** 三角积分 */
#define PRESET_INTEGRAL_TRIGONOMETRIC  "integral_trigonometric"

/** 反常积分 */
#define PRESET_INTEGRAL_IMPROPER       "integral_improper"

/** 曲线积分 */
#define PRESET_INTEGRAL_LINE           "integral_line"

/* -------------------- 级数展开 -------------------- */

/** Taylor级数展开 */
#define PRESET_SERIES_TAYLOR           "series_taylor"

/** Maclaurin级数展开 */
#define PRESET_SERIES_MACLAURIN        "series_maclaurin"

/** Fourier级数展开 */
#define PRESET_SERIES_FOURIER          "series_fourier"

/** 幂级数展开 */
#define PRESET_SERIES_POWER            "series_power"

/* -------------------- 多元微积分 -------------------- */

/** 梯度：nabla f */
#define PRESET_MULTIVARIABLE_GRADIENT  "multivariable_gradient"

/** 散度：nabla . F */
#define PRESET_MULTIVARIABLE_DIVERGENCE "multivariable_divergence"

/** 旋度：nabla x F */
#define PRESET_MULTIVARIABLE_CURL      "multivariable_curl"

/** Laplace算子：nabla^2 f */
#define PRESET_MULTIVARIABLE_LAPLACIAN "multivariable_laplacian"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有微积分预设函数块
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_calculus_register(void);

/**
 * @brief 获取微积分预设函数块数量
 *
 * @return int 微积分模块预设函数块总数
 */
int preset_calculus_count(void);

/**
 * @brief 获取微积分预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_calculus_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取微积分预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_calculus_category(void);

#ifdef __cplusplus
}
#endif

#endif /* PRESET_CALCULUS_H */
