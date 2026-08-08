/**
 * @file preset_stochastic_processes.c
 * @brief 随机过程预设函数块 - 实现
 *
 * 实现随机过程领域的预设函数块注册。
 * 涵盖Markov链、Brown运动、鞅论、Poisson过程及随机微分方程。
 *
 * @module StochasticProcesses
 * @category PRESET_EXT_ANALYSIS
 */

#include "preset_stochastic_processes.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 随机过程模块预设函数块总数 */
/* 已在头文件中定义 STOCHASTIC_PROCESSES_PRESET_COUNT = 14 */


int preset_stochastic_processes_count(void) {
    return STOCHASTIC_PROCESSES_PRESET_COUNT;
}
