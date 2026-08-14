/**
 * @file preset_homological_algebra.c
 * @brief 同调代数预设函数块 - 实现
 *
 * 实现同调代数领域的预设函数块注册。
 * 涵盖链复形、同调群、导出函子、Ext与Tor函子等。
 *
 * @module HomologicalAlgebra
 * @category PRESET_EXT_ALGEBRA_ADVANCED
 */

#include "lv/preset_homological_algebra.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 同调代数模块预设函数块总数 */
/* 已在头文件中定义 HOMOLOGICAL_ALGEBRA_PRESET_COUNT = 10 */


int preset_homological_algebra_count(void) {
    return HOMOLOGICAL_ALGEBRA_PRESET_COUNT;
}
