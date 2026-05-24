/**
 * @file preset_polynomial.h
 * @brief 多项式理论预设函数块 - 头文件
 *
 * 定义多项式理论模块的所有预设函数块名称常量、
 * 模块注册接口和元数据查询接口。
 *
 * 涵盖以下分类：
 * - 多项式运算（加减乘除、GCD、LCM）
 * - 多项式分析（次数、求值、导数、积分、复合）
 * - 多项式根（二次、三次、四次方程求根、因式分解）
 * - 特殊多项式（结式、判别式、拉格朗日插值）
 *
 * @module Polynomial
 * @category PRESET_CATEGORY_ALGEBRA
 * @version 3.3.0
 */

#ifndef LV00_PRESET_POLYNOMIAL_H
#define LV00_PRESET_POLYNOMIAL_H

#include "preset_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 预设函数块名称常量 ==================== */

/** 多项式加法：对两个多项式进行加法运算 */
#define PRESET_POLY_ADD "polynomial_add"
/** 多项式减法：对两个多项式进行减法运算 */
#define PRESET_POLY_SUBTRACT "polynomial_subtract"
/** 多项式乘法：对两个多项式进行乘法运算（卷积） */
#define PRESET_POLY_MULTIPLY "polynomial_multiply"
/** 多项式除法：对两个多项式进行带余除法 */
#define PRESET_POLY_DIVIDE "polynomial_divide"
/** 多项式最大公因式：计算两个多项式的 GCD */
#define PRESET_POLY_GCD "polynomial_gcd"
/** 多项式最小公倍式：计算两个多项式的 LCM */
#define PRESET_POLY_LCM "polynomial_lcm"

/** 多项式次数：获取多项式的最高次数 */
#define PRESET_POLY_DEGREE "polynomial_degree"
/** 多项式求值：在给定点 x 处计算多项式值 P(x) */
#define PRESET_POLY_EVALUATE "polynomial_evaluate"
/** 多项式求导：计算多项式的一阶导数 */
#define PRESET_POLY_DERIVATIVE "polynomial_derivative"
/** 多项式积分：计算多项式的不定积分 */
#define PRESET_POLY_INTEGRAL "polynomial_integral"
/** 多项式复合：计算两个多项式的复合 P(Q(x)) */
#define PRESET_POLY_COMPOSE "polynomial_compose"

/** 二次方程求根：求解 ax^2 + bx + c = 0（判别式法） */
#define PRESET_POLY_ROOTS_QUADRATIC "polynomial_roots_quadratic"
/** 三次方程求根：求解 ax^3 + bx^2 + cx + d = 0（卡尔丹公式） */
#define PRESET_POLY_ROOTS_CUBIC "polynomial_roots_cubic"
/** 四次方程求根：求解四次多项式方程（费拉里法） */
#define PRESET_POLY_ROOTS_QUARTIC "polynomial_roots_quartic"
/** 多项式因式分解：将多项式分解为不可约因式的乘积 */
#define PRESET_POLY_FACTOR "polynomial_factor"

/** 多项式结式：计算两个多项式的结式（Sylvester 结式） */
#define PRESET_POLY_RESULTANT "polynomial_resultant"
/** 多项式判别式：计算多项式的判别式 */
#define PRESET_POLY_DISCRIMINANT "polynomial_discriminant"
/** 拉格朗日插值：通过给定点集构造插值多项式 */
#define PRESET_POLY_INTERPOLATION "polynomial_interpolation"

/* ==================== 模块注册接口 ==================== */

/**
 * @brief 注册多项式理论模块的所有预设函数块
 *
 * 将 18 个多项式理论预设函数块注册到全局注册表中。
 * 注册顺序按照多项式运算 -> 多项式分析 -> 多项式根 -> 特殊多项式的逻辑排列。
 *
 * @return true  所有预设注册成功
 * @return false 部分或全部预设注册失败（非致命，可继续使用已注册部分）
 */
bool preset_polynomial_register(void);

/**
 * @brief 获取多项式理论模块的预设函数块数量
 *
 * @return int 预设函数块总数（当前为 18）
 */
int preset_polynomial_count(void);

/**
 * @brief 获取多项式理论模块的预设类别
 *
 * @return PresetCategory 返回 PRESET_CATEGORY_ALGEBRA
 */
PresetCategory preset_polynomial_category(void);

/**
 * @brief 获取多项式理论模块所有预设的名称列表
 *
 * 调用者负责释放返回的名称数组和每个名称字符串。
 *
 * @param out_names 输出：名称数组指针（调用者通过 lv00_free 逐项释放）
 * @param out_count 输出：名称数量
 * @return true  成功获取
 * @return false 参数为空或内存分配失败
 */
bool preset_polynomial_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_POLYNOMIAL_H */
