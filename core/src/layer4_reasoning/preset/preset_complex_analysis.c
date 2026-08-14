/**
 * @file preset_complex_analysis.c
 * @brief 复分析预设函数块模块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/complex_analysis.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的复分析运算预设函数块。
 * 涵盖复变函数基础、复积分、级数展开、共形映射和特殊函数。
 *
 * 采用v2统一宏模式，使用 PRESET_CATEGORY_ANALYSIS 类别。
 *
 * @module ComplexAnalysis
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "lv/preset_complex_analysis.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 复分析模块预设函数块总数：35（与头文件中 COMPLEX_ANALYSIS_PRESET_COUNT 一致） */


/**
 * @brief 获取复分析预设函数块数量
 */
int preset_complex_analysis_count(void) {
    return COMPLEX_ANALYSIS_PRESET_COUNT;
}

/**
 * @brief 获取复分析预设名称列表
 */
bool preset_complex_analysis_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        "complex_function_eval",
        "complex_limit",
        "complex_continuity_check",
        "complex_differentiable_check",
        "complex_derivative",
        "cauchy_riemann_check",
        "harmonic_function_check",
        "harmonic_conjugate",
        "analytic_function_check",
        "entire_function_check",
        "complex_line_integral",
        "cauchy_integral_formula",
        "cauchy_integral_derivative",
        "cauchy_theorem",
        "morera_theorem",
        "winding_number",
        "residue_integral",
        "contour_integral_param",
        "taylor_series_complex",
        "laurent_series",
        "power_series_radius",
        "laurent_series_principal_part",
        "residue_compute",
        "pole_order",
        "essential_singular_check",
        "removable_singular_check",
        "conformal_map_check",
        "mobius_transform",
        "mobius_compose",
        "mobius_inverse",
        "riemann_mapping",
        "complex_exp",
        "complex_log",
        "complex_trig",
        "gamma_function",
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}

PresetCategory preset_complex_analysis_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}
