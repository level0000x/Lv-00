/**
 * @file preset_measure_theory.c
 * @brief 测度论预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/measure_theory.lvz 数据驱动（convert_presets.py 生成）。
 *
 * @details 实现测度论模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共20个预设，涵盖σ代数、测度构造、可测函数与积分、
 *          收敛定理、乘积测度与Fubini定理、Radon-Nikodym导数。
 *
 * @module MeasureTheory
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 */

#include "preset_measure_theory.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 测度论模块预设函数块总数：20（与头文件中 MEASURE_THEORY_PRESET_COUNT 一致） */

/* ============================================================
 * 模块信息接口
 * ============================================================ */

int preset_measure_theory_count(void) {
    return MEASURE_THEORY_PRESET_COUNT;
}

PresetCategory preset_measure_theory_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}

bool preset_measure_theory_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* σ代数与测度基础 */
        PRESET_MT_SIGMA_ALGEBRA,
        PRESET_MT_BOREL_ALGEBRA,
        PRESET_MT_MEASURE_SPACE,
        PRESET_MT_NULL_SET,
        /* 测度构造 */
        PRESET_MT_LEBESGUE_MEASURE,
        PRESET_MT_COUNTING_MEASURE,
        PRESET_MT_DIRAC_MEASURE,
        PRESET_MT_OUTER_MEASURE,
        /* 可测函数与积分 */
        PRESET_MT_MEASURABLE_FUNCTION,
        PRESET_MT_SIMPLE_FUNCTION,
        PRESET_MT_LEBESGUE_INTEGRAL,
        PRESET_MT_LP_NORM,
        /* 收敛定理 */
        PRESET_MT_MONOTONE_CONVERGENCE,
        PRESET_MT_FATOU_LEMMA,
        PRESET_MT_DOMINATED_CONVERGENCE,
        PRESET_MT_ALMOST_EVERYWHERE,
        /* 乘积测度与Fubini */
        PRESET_MT_PRODUCT_MEASURE,
        PRESET_MT_FUBINI_THEOREM,
        /* Radon-Nikodym */
        PRESET_MT_ABSOLUTE_CONTINUITY,
        PRESET_MT_RADON_NIKODYM,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
