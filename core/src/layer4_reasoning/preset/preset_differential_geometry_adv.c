/**
 * @file preset_differential_geometry_adv.c
 * @brief 微分几何进阶预设函数块 - 实现
 *
 * 实现微分几何进阶领域的预设函数块注册。
 * 涵盖联络、曲率张量、测地线、Riemann几何及纤维丛。
 *
 * @module DifferentialGeometryAdv
 * @category PRESET_EXT_DIFFERENTIAL_GEOMETRY
 */

#include "lv/preset_differential_geometry_adv.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 微分几何进阶模块预设函数块总数 */
/* 已在头文件中定义 DIFFERENTIAL_GEOMETRY_ADV_PRESET_COUNT = 12 */


int preset_differential_geometry_adv_count(void) {
    return DIFFERENTIAL_GEOMETRY_ADV_PRESET_COUNT;
}
