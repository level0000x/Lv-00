/**
 * @file preset_probability_statistics.c
 * @brief 概率统计预设函数块 - 实现
 *
 * 实现概率统计领域的预设函数块注册。
 * 涵盖概率分布、假设检验、贝叶斯推断、回归分析及置信区间。
 *
 * @module ProbabilityStatistics
 * @category PRESET_EXT_ANALYSIS
 */

#include "lv/preset_probability_statistics.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 概率统计模块预设函数块总数 */
/* 已在头文件中定义 PROBABILITY_STATISTICS_PRESET_COUNT = 14 */


int preset_probability_statistics_count(void) {
    return PROBABILITY_STATISTICS_PRESET_COUNT;
}
