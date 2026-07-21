/**
 * @file preset_difference_equations.c
 * @brief 差分方程预设函数块 - 实现
 *
 * 实现差分方程领域的预设函数块注册。
 * 涵盖齐次/非齐次差分方程、Z变换、递推关系及稳定性分析。
 *
 * @module DifferenceEquations
 * @category PRESET_EXT_ALGEBRA_ADVANCED
 */

#include "preset_difference_equations.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 差分方程模块预设函数块总数 */
/* 已在头文件中定义 DIFFERENCE_EQUATIONS_PRESET_COUNT = 12 */

/* ==================== 模块注册实现 ==================== */

bool preset_difference_equations_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：齐次差分方程
     * ============================================================ */

    /* 一阶齐次差分方程 */
    if (preset_blocks_register_by_category(
            "homogeneous_first_order",
            "求解一阶齐次差分方程 a_n = r * a_{n-1}",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 二阶齐次差分方程 */
    if (preset_blocks_register_by_category(
            "homogeneous_second_order",
            "求解二阶齐次差分方程 a_n = p*a_{n-1} + q*a_{n-2}",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* k阶齐次差分方程 */
    if (preset_blocks_register_by_category(
            "homogeneous_kth_order",
            "求解k阶齐次线性差分方程（特征方程法）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：非齐次差分方程
     * ============================================================ */

    /* 非齐次差分方程求解 */
    if (preset_blocks_register_by_category(
            "nonhomogeneous_solve",
            "求解非齐次差分方程 a_n = p*a_{n-1} + f(n)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* 特解构造 */
    if (preset_blocks_register_by_category(
            "particular_solution",
            "待定系数法构造非齐次差分方程的特解",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：Z变换
     * ============================================================ */

    /* Z变换计算 */
    if (preset_blocks_register_by_category(
            "z_transform",
            "计算序列 {a_n} 的Z变换 A(z) = Σ a_n * z^{-n}",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 逆Z变换 */
    if (preset_blocks_register_by_category(
            "inverse_z_transform",
            "计算Z变换 A(z) 的逆变换得到序列 {a_n}",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* Z变换求解差分方程 */
    if (preset_blocks_register_by_category(
            "z_transform_solve",
            "利用Z变换求解线性常系数差分方程",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：稳定性与渐近分析
     * ============================================================ */

    /* 平衡点计算 */
    if (preset_blocks_register_by_category(
            "equilibrium_point",
            "计算差分方程的平衡点（不动点）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 稳定性判定 */
    if (preset_blocks_register_by_category(
            "stability_test",
            "判定差分方程平衡点的稳定性",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 渐近行为分析 */
    if (preset_blocks_register_by_category(
            "asymptotic_behavior",
            "分析差分方程解的渐近行为",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 生成函数方法 */
    if (preset_blocks_register_by_category(
            "generating_function_solve",
            "利用生成函数求解递推关系",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == DIFFERENCE_EQUATIONS_PRESET_COUNT;
}

int preset_difference_equations_count(void)
{
    return DIFFERENCE_EQUATIONS_PRESET_COUNT;
}
