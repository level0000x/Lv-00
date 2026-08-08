/**
 * @file preset_differential_equations.c
 * @brief 微分方程预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/differential_equations.lvz 数据驱动（convert_presets.py 生成）。
 *
 * @details 实现微分方程模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共20个预设，涵盖ODE求解方法、特殊ODE、PDE基本方法、
 *          存在唯一性判定、稳定性分析和数值近似方法。
 *
 * @module DifferentialEquations
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 */

#include "preset_differential_equations.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 微分方程模块预设函数块总数：20（与头文件中 DIFFERENTIAL_EQUATIONS_PRESET_COUNT 一致） */

/* ============================================================
 * 模块信息接口
 * ============================================================ */

int preset_differential_equations_count(void) {
    return DIFFERENTIAL_EQUATIONS_PRESET_COUNT;
}

PresetCategory preset_differential_equations_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}

bool preset_differential_equations_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    /* 分配名称数组 */
    char **names = (char **) lv_malloc(DIFFERENTIAL_EQUATIONS_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* ODE求解方法 */
        PRESET_DE_SEPARABLE_METHOD,
        PRESET_DE_INTEGRATING_FACTOR,
        PRESET_DE_VARIATION_CONSTANTS,
        PRESET_DE_CHARACTERISTIC_EQ,
        /* 特殊ODE */
        PRESET_DE_BERNOULLI_EQ,
        PRESET_DE_RICCATI_EQ,
        PRESET_DE_EXACT_EQ,
        /* ODE补充 */
        PRESET_DE_LINEAR_FIRST_ORDER,
        PRESET_DE_HOMOGENEOUS_ODE,
        PRESET_DE_CAUCHY_EULER,
        /* PDE基本 */
        PRESET_DE_PDE_SEPARABLE,
        PRESET_DE_CHARACTERISTIC_LINE,
        /* 存在唯一性 */
        PRESET_DE_EXISTENCE_UNIQUENESS,
        PRESET_DE_LIPSCHITZ_CHECK,
        /* 稳定性分析 */
        PRESET_DE_LYAPUNOV_STABILITY,
        PRESET_DE_ASYMPTOTIC_STABILITY,
        PRESET_DE_PHASE_PLANE,
        /* 数值与近似 */
        PRESET_DE_EULER_METHOD,
        PRESET_DE_PICARD_ITERATION,
        PRESET_DE_SERIES_SOLUTION,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) {
                {
                    void *tmp = names[j];
                    lv_free(&tmp);
                }
            }
            {
                void *tmp = names;
                lv_free(&tmp);
            }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
