/**
 * @file preset_measurements.c
 * @brief 几何测量模块预设函数库 - 实现
 *
 * 实现几何测量模块的所有预设函数库。
 * 涵盖距离、角度、面积、长度等多种测量计算。
 *
 * @module Measurements
 * @category PRESET_CATEGORY_MEASUREMENT
 */

#include "preset_measurements.h"

#include <string.h>

#include "lv_internal.h"
#include "preset_blocks.h"

/* ==================== 预设函数注册表 ==================== */

/** 测量学模块预设函数块总数 */


int preset_measurements_count(void) {
    return MEASUREMENTS_PRESET_COUNT;
}
