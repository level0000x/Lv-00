/**
 * @file preset_matrix.h
 * @brief 矩阵运算预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的矩阵运算预设函数块，包括：
 *   - 基础矩阵运算：加法、减法、标量乘法、矩阵乘法、转置、迹、行列式、逆
 *   - 线性代数：秩、零化度、特征值、特征向量、特征多项式、最小多项式、核空间、像空间
 *   - 矩阵分解：LU分解、QR分解、奇异值分解、Cholesky分解、Jordan标准形、谱分解
 *   - 特殊矩阵：单位矩阵、零矩阵、对角矩阵、初等行变换矩阵、Vandermonde矩阵、Hilbert矩阵
 *
 * @module Matrix
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_MATRIX_H
#define LV00_PRESET_MATRIX_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 基础矩阵运算 -------------------- */

/** 矩阵加法：A + B */
#ifndef PRESET_MATRIX_ADD
#define PRESET_MATRIX_ADD "matrix_add"
#endif

/** 矩阵减法：A - B */
#ifndef PRESET_MATRIX_SUBTRACT
#define PRESET_MATRIX_SUBTRACT "matrix_subtract"
#endif

/** 标量乘法：kA */
#define PRESET_MATRIX_SCALAR_MULTIPLY "matrix_scalar_multiply"

/** 矩阵乘法：AB */
#ifndef PRESET_MATRIX_MULTIPLY
#define PRESET_MATRIX_MULTIPLY "matrix_multiply"
#endif

/** 矩阵转置：A^T */
#ifndef PRESET_MATRIX_TRANSPOSE
#define PRESET_MATRIX_TRANSPOSE "matrix_transpose"
#endif

/** 矩阵迹：tr(A) */
#ifndef PRESET_MATRIX_TRACE
#define PRESET_MATRIX_TRACE "matrix_trace"
#endif

/** 矩阵行列式：det(A) */
#ifndef PRESET_MATRIX_DETERMINANT
#define PRESET_MATRIX_DETERMINANT "matrix_determinant"
#endif

/** 矩阵逆：A^{-1} */
#ifndef PRESET_MATRIX_INVERSE
#define PRESET_MATRIX_INVERSE "matrix_inverse"
#endif

/* -------------------- 线性代数 -------------------- */

/** 矩阵秩：rank(A) */
#ifndef PRESET_MATRIX_RANK
#define PRESET_MATRIX_RANK "matrix_rank"
#endif

/** 零化度：nullity(A) */
#ifndef PRESET_MATRIX_NULLITY
#define PRESET_MATRIX_NULLITY "matrix_nullity"
#endif

/** 特征值：lambda_i */
#define PRESET_MATRIX_EIGENVALUES "matrix_eigenvalues"

/** 特征向量 */
#define PRESET_MATRIX_EIGENVECTORS "matrix_eigenvectors"

/** 特征多项式：p(lambda) = det(lambdaI - A) */
#define PRESET_MATRIX_CHARACTERISTIC_POLY "matrix_characteristic_poly"

/** 最小多项式 */
#define PRESET_MATRIX_MINIMAL_POLY "matrix_minimal_poly"

/** 核空间：ker(A) */
#define PRESET_MATRIX_KERNEL "matrix_kernel"

/** 像空间：Im(A) */
#define PRESET_MATRIX_IMAGE "matrix_image"

/* -------------------- 矩阵分解 -------------------- */

/** LU分解 */
#define PRESET_MATRIX_LU_DECOMPOSITION "matrix_lu_decomposition"

/** QR分解 */
#define PRESET_MATRIX_QR_DECOMPOSITION "matrix_qr_decomposition"

/** 奇异值分解：A = U Sigma V^T */
#define PRESET_MATRIX_SVD "matrix_svd"

/** Cholesky分解 */
#define PRESET_MATRIX_CHOLESKY "matrix_cholesky"

/** Jordan标准形 */
#define PRESET_MATRIX_JORDAN_FORM "matrix_jordan_form"

/** 谱分解 */
#define PRESET_MATRIX_SPECTRAL "matrix_spectral"

/* -------------------- 特殊矩阵 -------------------- */

/** 单位矩阵：I_n */
#define PRESET_MATRIX_IDENTITY "matrix_identity"

/** 零矩阵：O_{m x n} */
#define PRESET_MATRIX_ZERO "matrix_zero"

/** 对角矩阵 */
#define PRESET_MATRIX_DIAGONAL "matrix_diagonal"

/** 初等行变换矩阵 */
#define PRESET_MATRIX_ELEMENTARY_ROW "matrix_elementary_row"

/** Vandermonde矩阵 */
#define PRESET_MATRIX_VANDERMONDE "matrix_vandermonde"

/** Hilbert矩阵 */
#define PRESET_MATRIX_HILBERT "matrix_hilbert"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有矩阵运算预设函数块
 *
 * 将矩阵运算模块的所有预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_matrix_register(void);

/**
 * @brief 获取矩阵运算预设函数块数量
 *
 * @return int 矩阵运算模块预设函数块总数
 */
int preset_matrix_count(void);

/**
 * @brief 获取矩阵运算预设函数块名称列表
 *
 * 返回所有已注册的矩阵运算预设名称数组。
 * 调用者负责释放返回的名称数组和每个名称字符串。
 *
 * @param out_names 输出名称数组（需调用者释放）
 * @param out_count 输出名称数量
 * @return true 获取成功
 * @return false 参数无效或内存不足
 */
bool preset_matrix_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取矩阵运算模块的预设类别
 *
 * @return PresetCategory 矩阵运算模块所属类别
 */
PresetCategory preset_matrix_category(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_MATRIX_H */
