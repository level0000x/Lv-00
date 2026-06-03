/**
 * @file preset_special_functions.h
 * @brief 特殊函数预设函数块模块 - 头文件
 *
 * 提供理论数学研究中常用的特殊函数预设函数块，涵盖：
 *   - Gamma & Beta 函数族：Γ(z)、B(a,b)、ψ(z)、lnΓ(z)、不完全Beta函数
 *   - 误差与指数积分：erf(x)、erfc(x)、Ei(x)、li(x)、Si(x)/Ci(x)
 *   - Bessel 与相关函数：J_ν(x)、Y_ν(x)、I_ν(x)、K_ν(x)、球Bessel函数
 *   - 正交多项式与其他：Legendre、Hermite、Laguerre、Chebyshev、Riemann Zeta
 *
 * 所有预设均属于 PRESET_CATEGORY_ANALYSIS 类别，输出类型
 * 均为 PRESET_TYPE_SCALAR（sf_sin_cos_integral 除外，其输出为元组）。
 *
 * @module SpecialFunctions
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_SPECIAL_FUNCTIONS_H
#define LV00_PRESET_SPECIAL_FUNCTIONS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 第一组：Gamma & Beta 函数 -------------------- */

/** Gamma函数 Γ(z) — 1个标量输入，1个标量输出 */
#define PRESET_SF_GAMMA "sf_gamma"

/** 对数Gamma函数 ln(Γ(z)) — 1个标量输入，1个标量输出 */
#define PRESET_SF_LOG_GAMMA "sf_log_gamma"

/** Digamma函数 ψ(z) — 1个标量输入，1个标量输出 */
#define PRESET_SF_DIGAMMA "sf_digamma"

/** Beta函数 B(a,b) — 2个标量输入，1个标量输出 */
#define PRESET_SF_BETA "sf_beta"

/** 不完全Beta函数 B(x;a,b) — 3个标量输入，1个标量输出 */
#define PRESET_SF_INCOMPLETE_BETA "sf_incomplete_beta"

/* -------------------- 第二组：误差与指数积分 -------------------- */

/** 误差函数 erf(x) — 1个标量输入，1个标量输出 */
#define PRESET_SF_ERF "sf_erf"

/** 补误差函数 erfc(x) — 1个标量输入，1个标量输出 */
#define PRESET_SF_ERFC "sf_erfc"

/** 指数积分 Ei(x) — 1个标量输入，1个标量输出 */
#define PRESET_SF_EXP_INTEGRAL "sf_exp_integral"

/** 对数积分 li(x) — 1个标量输入，1个标量输出 */
#define PRESET_SF_LOG_INTEGRAL "sf_log_integral"

/** 正弦/余弦积分 Si(x)/Ci(x) — 1个标量输入，输出元组（2值） */
#define PRESET_SF_SIN_COS_INTEGRAL "sf_sin_cos_integral"

/* -------------------- 第三组：Bessel 与相关函数 -------------------- */

/** 第一类Bessel函数 J_ν(x) — 2个标量输入（阶数ν, 自变量x），1个标量输出 */
#define PRESET_SF_BESSEL_J "sf_bessel_j"

/** 第二类Bessel函数 Y_ν(x) — 2个标量输入，1个标量输出 */
#define PRESET_SF_BESSEL_Y "sf_bessel_y"

/** 修正Bessel函数 I_ν(x) — 2个标量输入，1个标量输出 */
#define PRESET_SF_MODIFIED_BESSEL_I "sf_modified_bessel_i"

/** 修正Bessel函数 K_ν(x) — 2个标量输入，1个标量输出 */
#define PRESET_SF_MODIFIED_BESSEL_K "sf_modified_bessel_k"

/** 球Bessel函数 j_n(x) — 2个标量输入，1个标量输出 */
#define PRESET_SF_SPHERICAL_BESSEL "sf_spherical_bessel"

/* -------------------- 第四组：正交多项式与其他 -------------------- */

/** Legendre多项式 P_n(x) — 2个标量输入（阶数n, 自变量x），1个标量输出 */
#define PRESET_SF_LEGENDRE_P "sf_legendre_p"

/** Hermite多项式 H_n(x) — 2个标量输入，1个标量输出 */
#define PRESET_SF_HERMITE_H "sf_hermite_h"

/** Laguerre多项式 L_n(x) — 2个标量输入，1个标量输出 */
#define PRESET_SF_LAGUERRE_L "sf_laguerre_l"

/** Chebyshev多项式 T_n(x) — 2个标量输入，1个标量输出 */
#define PRESET_SF_CHEBYSHEV_T "sf_chebyshev_t"

/** Riemann Zeta函数 ζ(s) — 1个标量输入，1个标量输出 */
#define PRESET_SF_ZETA "sf_zeta"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有特殊函数预设函数块
 *
 * 依次注册 4 组共 20 个特殊函数预设：
 *   1. Gamma & Beta 函数（5个）
 *   2. 误差与指数积分（5个）
 *   3. Bessel 与相关函数（5个）
 *   4. 正交多项式与其他（5个）
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_special_functions_register(void);

/**
 * @brief 获取特殊函数预设函数块数量
 *
 * @return int 特殊函数模块预设函数块总数（20）
 */
int preset_special_functions_count(void);

/**
 * @brief 获取特殊函数模块的预设类别
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_ANALYSIS
 */
PresetCategory preset_special_functions_category(void);

/**
 * @brief 获取特殊函数模块的所有预设名称列表
 *
 * 分配并返回模块内所有 20 个预设的名称数组。
 * 调用者负责通过 lv00_free 释放每个元素和数组本身。
 *
 * @param out_names 输出名称数组（调用者负责释放每个元素和数组本身）
 * @param out_count 输出名称数量
 * @return true 内存分配成功
 * @return false 内存分配失败
 */
bool preset_special_functions_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_SPECIAL_FUNCTIONS_H */
