/**
 * @file preset_probability.c
 * @brief 概率论与统计预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/probability.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的概率论与统计预设函数块。
 * 涵盖概率基础、条件概率、离散分布、连续分布、统计量及假设检验。
 *
 * @module Probability
 * @category PRESET_CATEGORY_PROBABILITY
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "lv/preset_probability.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 概率论与统计模块预设函数块总数：25（与头文件中 PROBABILITY_PRESET_COUNT 一致） */


int preset_probability_count(void) {
    return PROBABILITY_PRESET_COUNT;
}

PresetCategory preset_probability_category(void) {
    return PRESET_CATEGORY_PROBABILITY;
}

bool preset_probability_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 概率基础 */
        PRESET_PROB_SAMPLE_SPACE,
        PRESET_PROB_EVENT_PROBABILITY,
        PRESET_PROB_COMPLEMENT_EVENT,
        PRESET_PROB_UNION_EVENT,
        PRESET_PROB_INTERSECTION_EVENT,
        /* 条件概率 */
        PRESET_PROB_CONDITIONAL,
        PRESET_PROB_BAYES,
        PRESET_PROB_INDEPENDENCE_TEST,
        PRESET_PROB_TOTAL_PROBABILITY,
        /* 离散分布 */
        PRESET_PROB_BINOMIAL_PMF,
        PRESET_PROB_POISSON_PMF,
        PRESET_PROB_GEOMETRIC_PMF,
        PRESET_PROB_HYPERGEOMETRIC_PMF,
        PRESET_PROB_DISCRETE_UNIFORM_PMF,
        /* 连续分布 */
        PRESET_PROB_NORMAL_PDF,
        PRESET_PROB_NORMAL_CDF,
        PRESET_PROB_EXPONENTIAL_PDF,
        PRESET_PROB_UNIFORM_PDF,
        /* 统计量 */
        PRESET_PROB_SAMPLE_MEAN,
        PRESET_PROB_SAMPLE_VARIANCE,
        PRESET_PROB_SAMPLE_STD,
        PRESET_PROB_SAMPLE_MEDIAN,
        /* 假设检验 */
        PRESET_PROB_Z_TEST,
        PRESET_PROB_T_TEST,
        PRESET_PROB_CHI_SQUARED_TEST,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
