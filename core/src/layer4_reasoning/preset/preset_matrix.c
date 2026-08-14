/**
 * @file preset_matrix.c
 * @brief 矩阵运算预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/matrix.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的矩阵运算预设函数块。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module Matrix
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 5.0.0
 */

#include "lv/preset_matrix.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 矩阵论模块预设函数块总数：28（与头文件中 MATRIX_PRESET_COUNT 一致） */


/**
 * @brief 获取矩阵运算预设函数块数量
 *
 * @return int 矩阵运算模块预设函数块总数
 */
int preset_matrix_count(void) {
    return MATRIX_PRESET_COUNT;
}

/**
 * @brief 获取矩阵运算模块的预设类别
 *
 * @return PresetCategory 矩阵运算模块所属类别
 */
PresetCategory preset_matrix_category(void) {
    return PRESET_CATEGORY_ALGEBRAIC;
}

/**
 * @brief 获取矩阵运算预设函数块名称列表
 *
 * @param out_names 输出名称数组（需调用者释放）
 * @param out_count 输出名称数量
 * @return true 获取成功
 * @return false 参数无效或内存不足
 */
bool preset_matrix_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 基础矩阵运算 */
        PRESET_MATRIX_ADD,
        PRESET_MATRIX_SUBTRACT,
        PRESET_MATRIX_SCALAR_MULTIPLY,
        PRESET_MATRIX_MULTIPLY,
        PRESET_MATRIX_TRANSPOSE,
        PRESET_MATRIX_TRACE,
        PRESET_MATRIX_DETERMINANT,
        PRESET_MATRIX_INVERSE,
        /* 线性代数 */
        PRESET_MATRIX_RANK,
        PRESET_MATRIX_NULLITY,
        PRESET_MATRIX_EIGENVALUES,
        PRESET_MATRIX_EIGENVECTORS,
        PRESET_MATRIX_CHARACTERISTIC_POLY,
        PRESET_MATRIX_MINIMAL_POLY,
        PRESET_MATRIX_KERNEL,
        PRESET_MATRIX_IMAGE,
        /* 矩阵分解 */
        PRESET_MATRIX_LU_DECOMPOSITION,
        PRESET_MATRIX_QR_DECOMPOSITION,
        PRESET_MATRIX_SVD,
        PRESET_MATRIX_CHOLESKY,
        PRESET_MATRIX_JORDAN_FORM,
        PRESET_MATRIX_SPECTRAL,
        /* 特殊矩阵 */
        PRESET_MATRIX_IDENTITY,
        PRESET_MATRIX_ZERO,
        PRESET_MATRIX_DIAGONAL,
        PRESET_MATRIX_ELEMENTARY_ROW,
        PRESET_MATRIX_VANDERMONDE,
        PRESET_MATRIX_HILBERT,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
