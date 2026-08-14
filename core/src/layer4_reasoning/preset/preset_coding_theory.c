/**
 * @file preset_coding_theory.c
 * @brief 编码理论预设函数块 - 实现
 *
 * 实现编码理论领域的预设函数块注册。
 * 涵盖线性码、Hamming码、Reed-Solomon码、循环码等。
 *
 * @module CodingTheory
 * @category PRESET_EXT_ALGEBRA_ADVANCED
 */

#include "lv/preset_coding_theory.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 编码理论模块预设函数块总数 */
/* 已在头文件中定义 CODING_THEORY_PRESET_COUNT = 10 */


int preset_coding_theory_count(void) {
    return CODING_THEORY_PRESET_COUNT;
}
