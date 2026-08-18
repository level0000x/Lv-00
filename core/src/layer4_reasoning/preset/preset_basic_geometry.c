/**
 * @file preset_basic_geometry.c
 * @brief 基础几何构造预设函数块 - 实现
 *
 * 实现基础几何构造模块的预设函数块。
 * 包含点的构造、线段绘制、直线和射线、圆的构造等。
 *
 * @module BasicGeometry
 * @category PRESET_CATEGORY_CONSTRUCTION
 */

#include "lv/preset_basic_geometry.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"

/* ==================== 预设函数块定义 ==================== */

/** 基础几何模块预设函数块总数 */


int preset_basic_geometry_count(void) {
    return BASIC_GEOMETRY_PRESET_COUNT;
}
