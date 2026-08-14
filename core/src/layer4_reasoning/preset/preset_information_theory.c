/**
 * @file preset_information_theory.c
 * @brief 信息论预设函数块 - 实现
 *
 * 实现信息论领域的预设函数块注册。
 * 涵盖信息熵、互信息、信道容量、率失真理论及数据压缩。
 *
 * @module InformationTheory
 * @category PRESET_EXT_ALGEBRA_ADVANCED
 */

#include "lv/preset_information_theory.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 信息论模块预设函数块总数 */
/* 已在头文件中定义 INFORMATION_THEORY_PRESET_COUNT = 10 */


int preset_information_theory_count(void) {
    return INFORMATION_THEORY_PRESET_COUNT;
}
