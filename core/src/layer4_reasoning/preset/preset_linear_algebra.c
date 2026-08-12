/**
 * @file preset_linear_algebra.c
 * @brief 线性代数预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/linear_algebra.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的线性代数运算预设函数块。
 * 涵盖矩阵基础运算、行列式与逆矩阵、矩阵分解、向量空间、
 * 线性映射及内积空间。
 *
 * @module LinearAlgebra
 * @category PRESET_CATEGORY_LINEAR_ALGEBRA
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "preset_linear_algebra.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 线性代数模块预设函数块总数：32（与头文件中 LINEAR_ALGEBRA_PRESET_COUNT 一致） */


/**
 * @brief 获取线性代数预设函数块数量
 *
 * @return int 线性代数模块预设函数块总数
 */
int preset_linear_algebra_count(void) {
    return LINEAR_ALGEBRA_PRESET_COUNT;
}

/**
 * @brief 获取线性代数预设的类别
 *
 * @return PresetCategory 线性代数模块所属类别
 */
PresetCategory preset_linear_algebra_category(void) {
    return PRESET_CATEGORY_LINEAR_ALGEBRA;
}

bool preset_linear_algebra_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 矩阵基础运算 */
        PRESET_LINALG_MATRIX_CREATE,
        PRESET_LINALG_MATRIX_ADD,
        PRESET_LINALG_MATRIX_SUBTRACT,
        PRESET_LINALG_MATRIX_MULTIPLY,
        PRESET_LINALG_MATRIX_SCALE,
        PRESET_LINALG_MATRIX_TRANSPOSE,
        PRESET_LINALG_MATRIX_TRACE,
        PRESET_LINALG_MATRIX_NEGATE,
        /* 行列式与逆矩阵 */
        PRESET_LINALG_DETERMINANT_2X2,
        PRESET_LINALG_DETERMINANT_3X3,
        PRESET_LINALG_DETERMINANT_N,
        PRESET_LINALG_INVERSE_2X2,
        PRESET_LINALG_INVERSE_3X3,
        PRESET_LINALG_ADJUGATE,
        /* 矩阵分解 */
        PRESET_LINALG_LU_DECOMPOSITION,
        PRESET_LINALG_QR_DECOMPOSITION,
        PRESET_LINALG_CHOLESKY,
        PRESET_LINALG_EIGENVALUES_2X2,
        PRESET_LINALG_EIGENVECTORS_2X2,
        /* 向量空间 */
        PRESET_LINALG_VECTOR_SPACE_TEST,
        PRESET_LINALG_LINEAR_INDEPENDENCE,
        PRESET_LINALG_SPAN,
        PRESET_LINALG_BASIS,
        PRESET_LINALG_DIMENSION,
        /* 线性映射 */
        PRESET_LINALG_LINEAR_MAP_TEST,
        PRESET_LINALG_KERNEL,
        PRESET_LINALG_IMAGE,
        PRESET_LINALG_RANK_NULLITY,
        /* 内积空间 */
        PRESET_LINALG_INNER_PRODUCT,
        PRESET_LINALG_GRAM_SCHMIDT,
        PRESET_LINALG_ORTHOGONAL_COMPLEMENT,
        PRESET_LINALG_PROJECTION,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
