/**
 * @file preset_dynamical_systems.c
 * @brief 动力系统预设函数块 - 实现
 *
 * 实现动力系统领域的预设函数块注册。
 * 涵盖不动点分析、分岔理论、Lyapunov指数、混沌与吸引子。
 *
 * @module DynamicalSystems
 * @category PRESET_EXT_ANALYSIS
 */

#include "lv/preset_dynamical_systems.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 动力系统模块预设函数块总数 */
/* 已在头文件中定义 DYNAMICAL_SYSTEMS_PRESET_COUNT = 14 */


int preset_dynamical_systems_count(void) {
    return DYNAMICAL_SYSTEMS_PRESET_COUNT;
}
