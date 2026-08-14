/**
 * @file preset_trigonometry.c
 * @brief 三角函数预设函数块 - 实现
 *
 * @details 实现三角函数模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共20个预设，涵盖基本三角函数、反三角函数、双曲函数、
 *          三角恒等式、三角方程求解和三角级数展开。
 *
 * @module Trigonometry
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 1.0.0
 */

#include "lv/preset_trigonometry.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 三角函数模块预设函数块总数 */

/* ============================================================
 * 模块信息接口
 * ============================================================ */

int preset_trigonometry_count(void) {
    return TRIGONOMETRY_PRESET_COUNT;
}

PresetCategory preset_trigonometry_category(void) {
    return PRESET_CATEGORY_ALGEBRAIC;
}

bool preset_trigonometry_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 基本三角函数 */
        PRESET_TRIG_SIN,
        PRESET_TRIG_COS,
        PRESET_TRIG_TAN,
        PRESET_TRIG_COT,
        PRESET_TRIG_SEC,
        PRESET_TRIG_CSC,
        /* 反三角函数 */
        PRESET_TRIG_ARCSIN,
        PRESET_TRIG_ARCCOS,
        PRESET_TRIG_ARCTAN,
        PRESET_TRIG_ARCCOT,
        /* 双曲函数 */
        PRESET_TRIG_SINH,
        PRESET_TRIG_COSH,
        PRESET_TRIG_TANH,
        /* 三角恒等式 */
        PRESET_TRIG_SUM_TO_PRODUCT,
        PRESET_TRIG_PRODUCT_TO_SUM,
        PRESET_TRIG_DOUBLE_ANGLE,
        PRESET_TRIG_HALF_ANGLE,
        /* 三角方程求解 */
        PRESET_TRIG_EQUATION_SOLVE,
        /* 三角级数展开 */
        PRESET_TRIG_SERIES_EXPAND,
        PRESET_TRIG_FOURIER_SERIES,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
