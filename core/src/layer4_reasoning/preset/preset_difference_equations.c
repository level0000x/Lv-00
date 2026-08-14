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

#include "lv/preset_difference_equations.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 差分方程模块预设函数块总数 */
/* 已在头文件中定义 DIFFERENCE_EQUATIONS_PRESET_COUNT = 12 */


int preset_difference_equations_count(void) {
    return DIFFERENCE_EQUATIONS_PRESET_COUNT;
}
