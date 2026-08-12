/**
 * @file preset_differential_geometry.c
 * @brief 微分几何预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/differential_geometry.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的微分几何运算预设函数块。
 * 涵盖曲线论、曲面论、联络与曲率、测地线和张量分析五大领域。
 * 共25个预设函数块，均遵循模块化、确定性原则。
 *
 * 采用统一的 preset_blocks_register_simple 注册接口，
 * 使用 REGISTER_DG 宏模式简化注册代码。
 *
 * @module DifferentialGeometry
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "preset_differential_geometry.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 微分几何模块预设函数块总数：25（与头文件中 DIFFERENTIAL_GEOMETRY_PRESET_COUNT 一致） */
#define DG_PRESET_COUNT DIFFERENTIAL_GEOMETRY_PRESET_COUNT


/**
 * @brief 获取微分几何预设函数块数量
 *
 * @return int 微分几何模块预设函数块总数（25）
 */
int preset_differential_geometry_count(void) {
    return DG_PRESET_COUNT;
}

/**
 * @brief 获取微分几何预设的类别
 *
 * @return PresetCategory 预设类别（PRESET_CATEGORY_ANALYSIS）
 */
PresetCategory preset_differential_geometry_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}

/**
 * @brief 获取微分几何预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_differential_geometry_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 曲线论 */
        PRESET_DG_ARC_LENGTH_PARAM,
        PRESET_DG_FRENET_FRAME,
        PRESET_DG_CURVATURE,
        PRESET_DG_TORSION,
        PRESET_DG_BERTRAND_CURVE,
        /* 曲面论 */
        PRESET_DG_FIRST_FUNDAMENTAL_FORM,
        PRESET_DG_SECOND_FUNDAMENTAL_FORM,
        PRESET_DG_GAUSS_CURVATURE,
        PRESET_DG_MEAN_CURVATURE,
        PRESET_DG_PRINCIPAL_CURVATURES,
        PRESET_DG_WEINGARTEN_MAP,
        /* 联络与曲率 */
        PRESET_DG_LEVI_CIVITA_CONNECTION,
        PRESET_DG_RIEMANN_CURVATURE,
        PRESET_DG_RICCI_CURVATURE,
        PRESET_DG_SECTIONAL_CURVATURE,
        PRESET_DG_SCALAR_CURVATURE,
        /* 测地线 */
        PRESET_DG_GEODESIC_EQUATION,
        PRESET_DG_EXPONENTIAL_MAP,
        PRESET_DG_JACOBI_FIELD,
        PRESET_DG_CONJUGATE_POINTS,
        /* 张量分析 */
        PRESET_DG_TENSOR_PRODUCT,
        PRESET_DG_COVARIANT_DERIVATIVE,
        PRESET_DG_LIE_DERIVATIVE,
        PRESET_DG_EXTERIOR_DERIVATIVE,
        PRESET_DG_HODGE_STAR,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
