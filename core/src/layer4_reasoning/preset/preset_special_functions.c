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

#include "lv/preset_special_functions.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 特殊函数模块预设函数块总数 */
/* 已在头文件中定义 SPECIAL_FUNCTIONS_PRESET_COUNT = 16 */


int preset_special_functions_count(void) {
    return SPECIAL_FUNCTIONS_PRESET_COUNT;
}
