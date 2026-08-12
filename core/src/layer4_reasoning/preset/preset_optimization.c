/**
 * @file preset_optimization.c
 * @brief 优化理论预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/optimization.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的优化理论预设函数块。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module Optimization
 * @category PRESET_CATEGORY_OPTIMIZATION
 * @version 4.0.0
 */

#include "preset_optimization.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 优化理论模块预设函数块总数：22（与头文件中 OPTIMIZATION_PRESET_COUNT 一致） */


/**
 * @brief 获取优化理论预设函数块的类别
 * @return 预设类别枚举值
 */
PresetCategory preset_optimization_category(void) {
    return PRESET_CATEGORY_CUSTOM;
}

/**
 * @brief 获取优化理论预设函数块的名称列表
 * @param out_names 输出：名称数组（调用者需先释放每个元素再释放数组本身）
 * @param out_count 输出：名称数量
 * @return true 成功，false 内存分配失败
 */
bool preset_optimization_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 无约束优化 */
        PRESET_OPT_GRADIENT_DESCENT,
        PRESET_OPT_NEWTON_METHOD,
        PRESET_OPT_CONJUGATE_GRADIENT,
        PRESET_OPT_QUASI_NEWTON,
        /* 约束优化 */
        PRESET_OPT_LAGRANGE_MULTIPLIER,
        PRESET_OPT_KKT_CONDITIONS,
        PRESET_OPT_PENALTY_METHOD,
        PRESET_OPT_BARRIER_METHOD,
        /* 线性规划 */
        PRESET_OPT_SIMPLEX,
        PRESET_OPT_INTERIOR_POINT,
        PRESET_OPT_DUAL_SIMPLEX,
        /* 凸优化 */
        PRESET_OPT_CONVEXITY_TEST,
        PRESET_OPT_CVX_GRADIENT,
        PRESET_OPT_PROXIMAL_GRADIENT,
        PRESET_OPT_ADMM,
        /* 变分法 */
        PRESET_OPT_EULER_LAGRANGE,
        PRESET_OPT_CALCULUS_OF_VARIATIONS,
        /* 对偶理论 */
        PRESET_OPT_DUAL_PROBLEM,
        PRESET_OPT_STRONG_DUALITY_TEST,
        PRESET_OPT_WEAK_DUALITY_TEST,
        /* 全局优化 */
        PRESET_OPT_SIMULATED_ANNEALING,
        PRESET_OPT_GENETIC_ALGORITHM,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
