/**
 * @file preset_game_theory.c
 * @brief 博弈论预设函数块 - 实现
 *
 * 实现博弈论领域的预设函数块注册。
 * 涵盖Nash均衡、混合策略、合作博弈及纳什讨价还价等。
 *
 * @module GameTheory
 * @category PRESET_EXT_OPTIMIZATION_THEORY
 */

#include "preset_game_theory.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 博弈论模块预设函数块总数 */
/* 已在头文件中定义 GAME_THEORY_PRESET_COUNT = 10 */


int preset_game_theory_count(void) {
    return GAME_THEORY_PRESET_COUNT;
}
