/**
 * @file preset_algebraic.c
 * @brief 代数模块预设函数块 - 实现
 *
 * 实现代数运算模块的预设函数块。
 * 支持多项式运算、方程求解与数学变换等。
 *
 * @module Algebraic
 * @category PRESET_CATEGORY_ALGEBRAIC
 */

#include "lv/preset_algebraic.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/preset_blocks.h"

/* ==================== 预设函数块定义 ==================== */

/** 代数模块预设函数块总数 */


int preset_algebraic_count(void) {
    return ALGEBRAIC_PRESET_COUNT;
}
