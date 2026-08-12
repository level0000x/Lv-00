/**
 * @file preset_numerical.c
 * @brief 数值分析预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/numerical.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的数值分析预设函数块。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module Numerical
 * @category PRESET_CATEGORY_NUMERICAL
 * @version 4.0.0
 */

#include "preset_numerical.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 数值分析模块预设函数块总数：24（与头文件中 NUMERICAL_PRESET_COUNT 一致） */


/**
 * @brief 获取数值分析预设函数块的类别
 * @return 预设类别枚举值
 */
PresetCategory preset_numerical_category(void) {
    return PRESET_CATEGORY_CUSTOM;
}

/**
 * @brief 获取数值分析预设函数块的名称列表
 * @param out_names 输出：名称数组（调用者需先释放每个元素再释放数组本身）
 * @param out_count 输出：名称数量
 * @return true 成功，false 内存分配失败
 */
bool preset_numerical_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 方程求根 */
        PRESET_NUMERICAL_BISECTION,
        PRESET_NUMERICAL_NEWTON,
        PRESET_NUMERICAL_SECANT,
        PRESET_NUMERICAL_FIXED_POINT,
        /* 数值积分 */
        PRESET_NUMERICAL_TRAPEZOID,
        PRESET_NUMERICAL_SIMPSON,
        PRESET_NUMERICAL_GAUSS_QUADRATURE,
        PRESET_NUMERICAL_ROMBERG,
        /* 数值微分 */
        PRESET_NUMERICAL_FORWARD_DIFF,
        PRESET_NUMERICAL_CENTRAL_DIFF,
        PRESET_NUMERICAL_RICHARDSON,
        /* 插值 */
        PRESET_NUMERICAL_LAGRANGE_INTERP,
        PRESET_NUMERICAL_NEWTON_INTERP,
        PRESET_NUMERICAL_SPLINE_INTERP,
        /* 拟合 */
        PRESET_NUMERICAL_LEAST_SQUARES,
        PRESET_NUMERICAL_POLYNOMIAL_FIT,
        /* 常微分方程 */
        PRESET_NUMERICAL_EULER,
        PRESET_NUMERICAL_RK4,
        PRESET_NUMERICAL_ADAMS,
        /* 线性方程组 */
        PRESET_NUMERICAL_GAUSS_ELIMINATION,
        PRESET_NUMERICAL_LU_DECOMPOSITION,
        PRESET_NUMERICAL_ITERATIVE_SOLVE,
        /* 矩阵特征值 */
        PRESET_NUMERICAL_EIGENVALUES,
        PRESET_NUMERICAL_SVD,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
