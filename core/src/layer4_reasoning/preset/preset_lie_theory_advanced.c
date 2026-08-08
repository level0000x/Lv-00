/**
 * @file preset_lie_theory_advanced.c
 * @brief 李理论进阶预设函数块 - 实现
 *
 * 实现李理论进阶领域的预设函数块注册。
 * 涵盖李代数、根系、Weyl群、表示论及权空间分解。
 *
 * @module LieTheoryAdvanced
 * @category PRESET_EXT_ALGEBRA_ADVANCED
 */

#include "preset_lie_theory_advanced.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 李理论进阶模块预设函数块总数 */
/* 已在头文件中定义 LIE_THEORY_ADVANCED_PRESET_COUNT = 8 */


int preset_lie_theory_advanced_count(void) {
    return LIE_THEORY_ADVANCED_PRESET_COUNT;
}
