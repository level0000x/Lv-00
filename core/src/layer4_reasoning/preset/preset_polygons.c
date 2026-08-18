/**
 * @file preset_polygons.c
 * @brief 多边形构造预设函数块 - 实现
 *
 * 实现多边形构造模块的预设函数块。
 * 包含正多边形、任意多边形构造、四边形构造等。
 *
 * @module Polygons
 * @category PRESET_CATEGORY_CONSTRUCTION
 */

#include "lv/preset_polygons.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/preset_blocks.h"

/* ==================== 预设函数块定义 ==================== */

/** 多边形模块预设函数块总数 */


int preset_polygons_count(void) {
    return POLYGONS_PRESET_COUNT;
}
