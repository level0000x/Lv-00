/**
 * @file preset_dynamical_systems.c
 * @brief 动力系统预设函数块 - 实现
 *
 * 实现动力系统领域的预设函数块注册。
 * 涵盖不动点分析、分岔理论、Lyapunov指数、混沌与吸引子。
 *
 * @module DynamicalSystems
 * @category PRESET_EXT_ANALYSIS
 */

#include "preset_dynamical_systems.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 动力系统模块预设函数块总数 */
/* 已在头文件中定义 DYNAMICAL_SYSTEMS_PRESET_COUNT = 14 */

/* ==================== 模块注册实现 ==================== */

bool preset_dynamical_systems_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：不动点分析
     * ============================================================ */

    /* 不动点计算 */
    if (preset_blocks_register_by_category(
            "fixed_point_compute",
            "计算离散动力系统 x_{n+1} = f(x_n) 的不动点",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* 不动点稳定性 */
    if (preset_blocks_register_by_category(
            "fixed_point_stability",
            "判定不动点的稳定性（|f'(x*)| 与1的关系）",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* 周期点计算 */
    if (preset_blocks_register_by_category(
            "periodic_point",
            "计算周期为n的周期点 f^n(x) = x",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：分岔理论
     * ============================================================ */

    /* 分岔点检测 */
    if (preset_blocks_register_by_category(
            "bifurcation_detection",
            "检测参数化系统 dx/dt = f(x,μ) 的分岔点",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* 分岔图构造 */
    if (preset_blocks_register_by_category(
            "bifurcation_diagram",
            "构造离散动力系统的分岔图（参数扫描）",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* Hopf分岔判定 */
    if (preset_blocks_register_by_category(
            "hopf_bifurcation_test",
            "判定连续动力系统是否发生Hopf分岔",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：Lyapunov指数与混沌
     * ============================================================ */

    /* Lyapunov指数计算 */
    if (preset_blocks_register_by_category(
            "lyapunov_exponent",
            "计算一维映射的Lyapunov指数 λ = lim (1/n) Σ ln|f'(x_i)|",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* 混沌判定 */
    if (preset_blocks_register_by_category(
            "chaos_detection",
            "基于Lyapunov指数判定系统是否混沌（λ > 0）",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：吸引子
     * ============================================================ */

    /* 吸引子计算 */
    if (preset_blocks_register_by_category(
            "attractor_compute",
            "计算动力系统的吸引子",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* 奇异吸引子检测 */
    if (preset_blocks_register_by_category(
            "strange_attractor_test",
            "检测系统是否具有奇异吸引子",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第五部分：连续动力系统
     * ============================================================ */

    /* 相平面分析 */
    if (preset_blocks_register_by_category(
            "phase_plane_analysis",
            "二维自治系统的相平面分析",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* 极限环检测 */
    if (preset_blocks_register_by_category(
            "limit_cycle_detection",
            "检测二维系统是否存在极限环",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* Poincaré映射 */
    if (preset_blocks_register_by_category(
            "poincare_map",
            "构造Poincaré截面映射",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* 轨道敏感性分析 */
    if (preset_blocks_register_by_category(
            "sensitivity_analysis",
            "分析初始条件的敏感依赖性",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == DYNAMICAL_SYSTEMS_PRESET_COUNT;
}

int preset_dynamical_systems_count(void)
{
    return DYNAMICAL_SYSTEMS_PRESET_COUNT;
}
