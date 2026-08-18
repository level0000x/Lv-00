/**
 * @file preset_transformations.c
 * @brief 几何变换预设函数块 - 实现
 *
 * 实现几何变换模块的预设函数块。
 * 包含平移、旋转、缩放、位似、反射变换等。
 *
 * @module Transformations
 * @category PRESET_CATEGORY_TRANSFORMATION
 */

#include "lv/preset_transformations.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/preset_blocks.h"

/* ==================== 预设函数块定义 ==================== */

/** 变换模块预设函数块总数 */


int preset_transformations_count(void) {
    return TRANSFORMATIONS_PRESET_COUNT;
}
