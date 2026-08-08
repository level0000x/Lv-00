/**
 * @file preset_numerical_analysis.c
 * @brief 数值分析预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/numerical_analysis.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的数值分析预设函数块。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module NumericalAnalysis
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 4.0.0
 */

#include "preset_numerical_analysis.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 数值分析模块预设函数块总数：25（与头文件中 NUMERICAL_ANALYSIS_PRESET_COUNT 一致） */


int preset_numerical_analysis_count(void) {
    return NUMERICAL_ANALYSIS_PRESET_COUNT;
}
