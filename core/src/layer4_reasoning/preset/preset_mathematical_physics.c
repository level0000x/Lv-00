/**
 * @file preset_mathematical_physics.c
 * @brief 数学物理预设函数块 - 实现
 *
 * 实现数学物理领域的预设函数块注册。
 * 涵盖变分法、Hamilton力学、Lagrange力学及经典场论。
 *
 * @module MathematicalPhysics
 * @category PRESET_EXT_ANALYSIS
 */

#include "preset_mathematical_physics.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 数学物理模块预设函数块总数 */
/* 已在头文件中定义 MATHEMATICAL_PHYSICS_PRESET_COUNT = 12 */

/* ==================== 模块注册实现 ==================== */

bool preset_mathematical_physics_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：变分法
     * ============================================================ */

    /* 泛函极值问题 */
    if (preset_blocks_register_by_category("functional_extremum", "求解泛函 J[y] = ∫ L(x, y, y') dx 的极值",
                                           PRESET_EXT_ANALYSIS, 2, 1)) {
        success_count++;
    }

    /* Euler-Lagrange方程 */
    if (preset_blocks_register_by_category("euler_lagrange_equation",
                                           "建立并求解Euler-Lagrange方程 ∂L/∂y - d/dx(∂L/∂y') = 0", PRESET_EXT_ANALYSIS,
                                           2, 1)) {
        success_count++;
    }

    /* 等周问题 */
    if (preset_blocks_register_by_category("isoperimetric_problem", "求解带约束的变分问题（等周问题）",
                                           PRESET_EXT_ANALYSIS, 3, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：Lagrange力学
     * ============================================================ */

    /* Lagrange函数构造 */
    if (preset_blocks_register_by_category("lagrangian_construct", "构造Lagrange函数 L = T - V（动能减势能）",
                                           PRESET_EXT_ANALYSIS, 2, 1)) {
        success_count++;
    }

    /* Lagrange方程 */
    if (preset_blocks_register_by_category("lagrange_equation", "建立Lagrange运动方程 d/dt(∂L/∂q') - ∂L/∂q = Q",
                                           PRESET_EXT_ANALYSIS, 3, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：Hamilton力学
     * ============================================================ */

    /* Hamilton函数构造 */
    if (preset_blocks_register_by_category("hamiltonian_construct", "由Lagrange函数通过Legendre变换构造Hamilton函数 H",
                                           PRESET_EXT_ANALYSIS, 2, 1)) {
        success_count++;
    }

    /* Hamilton正则方程 */
    if (preset_blocks_register_by_category("hamilton_equation", "建立Hamilton正则方程 dq/dt = ∂H/∂p, dp/dt = -∂H/∂q",
                                           PRESET_EXT_ANALYSIS, 2, 1)) {
        success_count++;
    }

    /* Poisson括号 */
    if (preset_blocks_register_by_category("poisson_bracket", "计算Poisson括号 {f, g} = Σ(∂f/∂q·∂g/∂p - ∂f/∂p·∂g/∂q)",
                                           PRESET_EXT_ANALYSIS, 3, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：守恒律与对称性
     * ============================================================ */

    /* Noether定理 */
    if (preset_blocks_register_by_category("noether_theorem", "应用Noether定理：连续对称性对应守恒量",
                                           PRESET_EXT_ANALYSIS, 3, 1)) {
        success_count++;
    }

    /* 守恒量计算 */
    if (preset_blocks_register_by_category("conserved_quantity", "计算运动方程的守恒量（能量、动量、角动量等）",
                                           PRESET_EXT_ANALYSIS, 2, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第五部分：经典场论
     * ============================================================ */

    /* 场方程推导 */
    if (preset_blocks_register_by_category("field_equation", "由场的Lagrangian密度推导场的运动方程",
                                           PRESET_EXT_ANALYSIS, 2, 1)) {
        success_count++;
    }

    /* 作用量原理 */
    if (preset_blocks_register_by_category("action_principle", "计算作用量 S = ∫ L dt 并应用最小作用量原理",
                                           PRESET_EXT_ANALYSIS, 2, 1)) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == MATHEMATICAL_PHYSICS_PRESET_COUNT;
}

int preset_mathematical_physics_count(void) {
    return MATHEMATICAL_PHYSICS_PRESET_COUNT;
}
