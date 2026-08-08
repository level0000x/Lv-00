/**
 * @file preset_mathematical_physics.c
 * @brief 数学物理预设函数块 - 实现
 *
 * 实现数学物理领域的预设函数块注册。
 * 涵盖变分法、Hamilton力学、Lagrange力学及经典场论。
 *
 * @module MathematicalPhysics
 * @category PRESET_EXT_ANALYSIS
 */

#include "preset_mathematical_physics.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 数学物理模块预设函数块总数 */
/* 已在头文件中定义 MATHEMATICAL_PHYSICS_PRESET_COUNT = 12 */


int preset_mathematical_physics_count(void) {
    return MATHEMATICAL_PHYSICS_PRESET_COUNT;
}
