/**
 * @file preset_special_functions.c
 * @brief 特殊函数预设函数块 - 实现
 *
 * 实现特殊函数领域的预设函数块注册。
 * 涵盖Gamma函数、Bessel函数、超几何函数、椭圆积分及正交多项式。
 *
 * @module SpecialFunctions
 * @category PRESET_EXT_ANALYSIS
 */

#include "preset_special_functions.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 特殊函数模块预设函数块总数 */
/* 已在头文件中定义 SPECIAL_FUNCTIONS_PRESET_COUNT = 16 */

/* ==================== 模块注册实现 ==================== */

bool preset_special_functions_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：Gamma函数族
     * ============================================================ */

    /* Gamma函数 */
    if (preset_blocks_register_by_category(
            "gamma_function",
            "计算Gamma函数 Γ(z) = ∫_0^∞ t^{z-1} e^{-t} dt",
            PRESET_EXT_ANALYSIS,
            1, 1)) {
        success_count++;
    }

    /* Beta函数 */
    if (preset_blocks_register_by_category(
            "beta_function",
            "计算Beta函数 B(a,b) = Γ(a)Γ(b)/Γ(a+b)",
            PRESET_EXT_ANALYSIS,
            2, 1)) {
        success_count++;
    }

    /* 不完全Gamma函数 */
    if (preset_blocks_register_by_category(
            "incomplete_gamma",
            "计算不完全Gamma函数 γ(s,x) 和 Γ(s,x)",
            PRESET_EXT_ANALYSIS,
            2, 1)) {
        success_count++;
    }

    /* 误差函数 */
    if (preset_blocks_register_by_category(
            "error_function",
            "计算误差函数 erf(x) = (2/√π) ∫_0^x e^{-t²} dt",
            PRESET_EXT_ANALYSIS,
            1, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：Bessel函数
     * ============================================================ */

    /* 第一类Bessel函数 */
    if (preset_blocks_register_by_category(
            "bessel_j",
            "计算第一类Bessel函数 J_ν(x)",
            PRESET_EXT_ANALYSIS,
            2, 1)) {
        success_count++;
    }

    /* 第二类Bessel函数 */
    if (preset_blocks_register_by_category(
            "bessel_y",
            "计算第二类Bessel函数 Y_ν(x)（Neumann函数）",
            PRESET_EXT_ANALYSIS,
            2, 1)) {
        success_count++;
    }

    /* 修正Bessel函数 */
    if (preset_blocks_register_by_category(
            "bessel_i",
            "计算第一类修正Bessel函数 I_ν(x)",
            PRESET_EXT_ANALYSIS,
            2, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：超几何函数
     * ============================================================ */

    /* 超几何函数 */
    if (preset_blocks_register_by_category(
            "hypergeometric_2f1",
            "计算Gauss超几何函数 _2F_1(a,b;c;z)",
            PRESET_EXT_ANALYSIS,
            4, 1)) {
        success_count++;
    }

    /* 合流超几何函数 */
    if (preset_blocks_register_by_category(
            "confluent_hypergeometric",
            "计算合流超几何函数 _1F_1(a;b;z)（Kummer函数）",
            PRESET_EXT_ANALYSIS,
            3, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：正交多项式
     * ============================================================ */

    /* Legendre多项式 */
    if (preset_blocks_register_by_category(
            "legendre_polynomial",
            "计算Legendre多项式 P_n(x)",
            PRESET_EXT_ANALYSIS,
            2, 1)) {
        success_count++;
    }

    /* Chebyshev多项式 */
    if (preset_blocks_register_by_category(
            "chebyshev_polynomial",
            "计算Chebyshev多项式 T_n(x) 和 U_n(x)",
            PRESET_EXT_ANALYSIS,
            2, 1)) {
        success_count++;
    }

    /* Hermite多项式 */
    if (preset_blocks_register_by_category(
            "hermite_polynomial",
            "计算Hermite多项式 H_n(x)",
            PRESET_EXT_ANALYSIS,
            2, 1)) {
        success_count++;
    }

    /* Laguerre多项式 */
    if (preset_blocks_register_by_category(
            "laguerre_polynomial",
            "计算Laguerre多项式 L_n(x)",
            PRESET_EXT_ANALYSIS,
            2, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第五部分：椭圆积分与Zeta函数
     * ============================================================ */

    /* 椭圆积分 */
    if (preset_blocks_register_by_category(
            "elliptic_integral",
            "计算第一类和第二类椭圆积分 F(φ,k), E(φ,k)",
            PRESET_EXT_ANALYSIS,
            3, 1)) {
        success_count++;
    }

    /* Riemann Zeta函数 */
    if (preset_blocks_register_by_category(
            "riemann_zeta",
            "计算Riemann Zeta函数 ζ(s) = Σ n^{-s}",
            PRESET_EXT_ANALYSIS,
            1, 1)) {
        success_count++;
    }

    /* Digamma函数 */
    if (preset_blocks_register_by_category(
            "digamma_function",
            "计算Digamma函数 ψ(z) = Γ'(z)/Γ(z)",
            PRESET_EXT_ANALYSIS,
            1, 1)) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == SPECIAL_FUNCTIONS_PRESET_COUNT;
}

int preset_special_functions_count(void)
{
    return SPECIAL_FUNCTIONS_PRESET_COUNT;
}
