/**
 * @file preset_functional_analysis.c
 * @brief 泛函分析预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/functional_analysis.lvz 数据驱动（convert_presets.py 生成）。
 *
 * @details 实现泛函分析模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共22个预设，涵盖赋范空间、内积空间、线性算子理论、
 *          三大基本定理、一致有界原理、弱收敛、对偶与伴随、
 *          投影与正交化以及不动点理论。
 *
 * @module FunctionalAnalysis
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 */

#include "lv/preset_functional_analysis.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 泛函分析模块预设函数块总数：21（与头文件中 FUNCTIONAL_ANALYSIS_PRESET_COUNT 一致） */

/* ============================================================
 * 模块信息接口
 * ============================================================ */

int preset_functional_analysis_count(void) {
    return FUNCTIONAL_ANALYSIS_PRESET_COUNT;
}

PresetCategory preset_functional_analysis_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}

bool preset_functional_analysis_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 赋范空间 */
        PRESET_FA_NORM_CHECK,
        PRESET_FA_BANACH_SPACE_CHECK,
        /* 内积空间 */
        PRESET_FA_INNER_PRODUCT_CHECK,
        PRESET_FA_HILBERT_SPACE_CHECK,
        /* 线性算子 */
        PRESET_FA_BOUNDED_OPERATOR,
        PRESET_FA_COMPACT_OPERATOR,
        PRESET_FA_SPECTRAL_ANALYSIS,
        /* 三大基本定理 */
        PRESET_FA_HAHN_BANACH,
        PRESET_FA_OPEN_MAPPING,
        PRESET_FA_CLOSED_GRAPH,
        /* 一致有界原理 */
        PRESET_FA_UNIFORM_BOUNDEDNESS,
        /* 弱收敛 */
        PRESET_FA_WEAK_CONVERGENCE,
        PRESET_FA_WEAK_STAR_CONVERGENCE,
        /* 对偶与伴随 */
        PRESET_FA_DUAL_SPACE,
        PRESET_FA_ADJOINT_OPERATOR,
        PRESET_FA_SELF_ADJOINT,
        /* 投影与正交化 */
        PRESET_FA_ORTHOGONAL_PROJECTION,
        PRESET_FA_GRAM_SCHMIDT,
        PRESET_FA_RIESZ_REPRESENTATION,
        /* 不动点理论 */
        PRESET_FA_FIXED_POINT,
        PRESET_FA_CONTRACTION_MAPPING,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
