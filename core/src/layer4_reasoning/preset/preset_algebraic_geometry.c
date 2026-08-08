/**
 * @file preset_algebraic_geometry.c
 * @brief 代数几何预设函数块 - 实现
 *
 * 实现代数几何领域的预设函数块注册。
 * 涵盖代数簇、理想与Gr"obner基、射影几何等核心概念。
 *
 * @module AlgebraicGeometry
 * @category PRESET_EXT_ALGEBRA_ADVANCED
 */

#include "preset_algebraic_geometry.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 代数几何模块预设函数块总数 */
/* 已在头文件中定义 ALGEBRAIC_GEOMETRY_PRESET_COUNT = 14 */


int preset_algebraic_geometry_count(void) {
    return ALGEBRAIC_GEOMETRY_PRESET_COUNT;
}
