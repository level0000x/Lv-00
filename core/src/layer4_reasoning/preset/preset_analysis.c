/**
 * @file preset_analysis.c
 * @brief 分析学预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/analysis.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的分析学运算预设函数块。
 * 涵盖极限、微分、积分、级数、函数空间及度量空间。
 *
 * @module Analysis
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_analysis.h -> preset_blocks.h -> func_block_registry.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_simple() 声明
 *   -> 提供 PresetCategory 枚举（PRESET_CATEGORY_ANALYSIS 等）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 *   -> 提供 PRESET_SAFE_MALLOC 等安全内存操作宏
 * lv_internal.h / lv_utils.h
 *   -> 提供 lv_malloc、lv_free、lv_strdup、lv_log_* 等
 * ============================================================
 */
#include "lv/preset_analysis.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h" /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等） */

/* ==================== 预设函数块数量 ==================== */

/** 分析学模块预设函数块总数：49（与头文件中 ANALYSIS_PRESET_COUNT 一致） */


/**
 * @brief 获取分析学预设函数块数量
 *
 * @return int 分析学模块预设函数块总数
 */
int preset_analysis_count(void) {
    return ANALYSIS_PRESET_COUNT;
}

PresetCategory preset_analysis_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}

bool preset_analysis_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 极限运算 */
        PRESET_SEQUENCE_LIMIT,
        PRESET_FUNCTION_LIMIT,
        PRESET_LEFT_LIMIT,
        PRESET_RIGHT_LIMIT,
        PRESET_INFINITE_LIMIT,
        PRESET_LIMIT_SUPERIOR,
        PRESET_LIMIT_INFERIOR,
        PRESET_LIMIT_EXISTS_TEST,
        /* 连续性 */
        PRESET_CONTINUITY_TEST,
        PRESET_UNIFORM_CONTINUITY_TEST,
        PRESET_DISCONTINUITY_CLASSIFY,
        PRESET_LIPSCHITZ_TEST,
        /* 微分运算 */
        PRESET_DERIVATIVE,
        PRESET_HIGHER_DERIVATIVE,
        PRESET_PARTIAL_DERIVATIVE,
        PRESET_DIRECTIONAL_DERIVATIVE,
        PRESET_GRADIENT,
        PRESET_DIVERGENCE,
        PRESET_CURL,
        PRESET_LAPLACIAN,
        PRESET_DIFFERENTIABILITY_TEST,
        PRESET_TAYLOR_EXPANSION,
        /* 积分运算 */
        PRESET_INDEFINITE_INTEGRAL,
        PRESET_DEFINITE_INTEGRAL,
        PRESET_IMPROPER_INTEGRAL,
        PRESET_MULTIPLE_INTEGRAL,
        PRESET_LINE_INTEGRAL,
        PRESET_SURFACE_INTEGRAL,
        PRESET_INTEGRABILITY_TEST,
        PRESET_MEAN_VALUE_THEOREM,
        /* 级数运算 */
        PRESET_SERIES_CONVERGENCE_TEST,
        PRESET_ABSOLUTE_CONVERGENCE,
        PRESET_CONDITIONAL_CONVERGENCE,
        PRESET_POWER_SERIES_RADIUS,
        PRESET_SERIES_SUM,
        PRESET_FOURIER_SERIES,
        /* 函数空间 */
        PRESET_LP_NORM,
        PRESET_SUP_NORM,
        PRESET_COMPLETION,
        PRESET_COMPACTNESS_TEST,
        /* 度量空间 */
        PRESET_METRIC_SPACE_TEST,
        PRESET_CAUCHY_SEQUENCE_TEST,
        PRESET_COMPLETE_SPACE_TEST,
        PRESET_CONTRACTION_MAPPING,
        PRESET_FIXED_POINT_THEOREM,
        /* 特殊函数 */
        PRESET_GAMMA_FUNCTION,
        PRESET_BETA_FUNCTION,
        PRESET_ZETA_FUNCTION,
        PRESET_ERROR_FUNCTION,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
