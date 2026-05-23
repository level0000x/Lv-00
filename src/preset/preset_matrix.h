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

#ifndef PRESET_MATRIX_H
#define PRESET_MATRIX_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 基础矩阵运算 -------------------- */

/**
 * @brief 矩阵加法 C = A + B
 *
 * @details 数学定义：对同型矩阵 A, B in F^{m * n}，逐元素相加 C_{ij} = A_{ij} + B_{ij}。
 *
 * @note 输入: PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX
 *       输出: PRESET_TYPE_MATRIX | 复杂度: O(mn) | 可逆: 是
 */
#define PRESET_MATRIX_ADD                 "matrix_add"

/**
 * @brief 矩阵减法 C = A - B
 *
 * @details 数学定义：对同型矩阵 A, B in F^{m * n}，逐元素相减 C_{ij} = A_{ij} - B_{ij}。
 *
 * @note 输入: PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX
 *       输出: PRESET_TYPE_MATRIX | 复杂度: O(mn) | 可逆: 是
 */
#define PRESET_MATRIX_SUBTRACT            "matrix_subtract"

/**
 * @brief 标量乘法 B = k * A
 *
 * @details 数学定义：矩阵每个元素乘以标量 k，B_{ij} = k * A_{ij}。
 *
 * @note 输入: PRESET_TYPE_MATRIX, PRESET_TYPE_SCALAR
 *       输出: PRESET_TYPE_MATRIX | 复杂度: O(mn)
 */
#define PRESET_MATRIX_SCALAR_MULTIPLY     "matrix_scalar_multiply"

/**
 * @brief 矩阵乘法 C = A * B
 *
 * @details 数学定义：C_{ij} = sum_k A_{ik} * B_{kj}，
 *          要求 A in F^{m * p}，B in F^{p * n}，结果 C in F^{m * n}。
 *
 * @note 输入: PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX
 *       输出: PRESET_TYPE_MATRIX | 复杂度: O(mnp)（朴素），O(n^{log_2 7})（Strassen）
 */
#define PRESET_MATRIX_MULTIPLY            "matrix_multiply"

/**
 * @brief 矩阵转置 B = A^T，B_{ij} = A_{ji}
 *
 * @details 数学定义：将矩阵的行列互换，(A^T)_{ij} = A_{ji}。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_MATRIX | 复杂度: O(mn) | 可逆: 是
 */
#define PRESET_MATRIX_TRANSPOSE           "matrix_transpose"

/**
 * @brief 矩阵迹 tr(A) = sum_i A_{ii}
 *
 * @details 数学定义：方阵主对角线元素之和，tr(AB) = tr(BA)。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_MATRIX_TRACE               "matrix_trace"

/**
 * @brief 矩阵行列式 det(A) = |A|
 *
 * @details 数学定义：方阵 A 的行列式，det(A) = 0 ⇔ A 奇异（不可逆）。
 *          det(AB) = det(A) * det(B)。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n^3)（LU分解）
 */
#define PRESET_MATRIX_DETERMINANT         "matrix_determinant"

/**
 * @brief 矩阵逆 B = A^{-1}，满足 A * A^{-1} = I
 *
 * @details 数学定义：方阵 A 的逆矩阵，要求 det(A) ≠ 0。
 *          (A^{-1})^{-1} = A，(AB)^{-1} = B^{-1}A^{-1}。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_MATRIX | 复杂度: O(n^3) | 可逆: 是
 */
#define PRESET_MATRIX_INVERSE             "matrix_inverse"

/* -------------------- 线性代数 -------------------- */

/**
 * @brief 矩阵秩 rank(A) = dim(col(A)) = dim(row(A))
 *
 * @details 数学定义：矩阵 A 线性无关列（或行）的最大数量，
 *          rank(A) = rank(A^T)，满秩 ⇒ 可逆。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_INTEGER | 复杂度: O(mn^2)
 */
#define PRESET_MATRIX_RANK                "matrix_rank"

/**
 * @brief 零化度 nullity(A) = dim(ker(A)) = n - rank(A)
 *
 * @details 数学定义：矩阵 A 的零空间（核）的维数，
 *          由秩-零化度定理：rank(A) + nullity(A) = n。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_INTEGER | 复杂度: O(mn^2)
 */
#define PRESET_MATRIX_NULLITY             "matrix_nullity"

/**
 * @brief 矩阵特征值 det(A - lambda*I) = 0 的根
 *
 * @details 数学定义：满足 A*v = lambda*v 的标量 lambda，
 *          实对称矩阵的特征值全为实数。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n^3)
 */
#define PRESET_MATRIX_EIGENVALUES         "matrix_eigenvalues"

/**
 * @brief 特征向量 A*v = lambda*v，v ≠ 0
 *
 * @details 数学定义：满足 A*v = lambda*v 的非零向量 v，
 *          同一特征值的特征向量构成特征子空间。
 *
 * @note 输入: PRESET_TYPE_MATRIX, PRESET_TYPE_SCALAR
 *       输出: PRESET_TYPE_TUPLE | 复杂度: O(n^3)
 */
#define PRESET_MATRIX_EIGENVECTORS        "matrix_eigenvectors"

/**
 * @brief 特征多项式 p(lambda) = det(lambda*I - A)
 *
 * @details 数学定义：n 阶方阵 A 的特征多项式为 n 次多项式，
 *          Cayley-Hamilton 定理：p(A) = 0。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_POLYNOMIAL | 复杂度: O(n^3)
 */
#define PRESET_MATRIX_CHARACTERISTIC_POLY "matrix_characteristic_poly"

/**
 * @brief 最小多项式 m_A(x)，满足 m_A(A) = 0 的最低次首一多项式
 *
 * @details 数学定义：最小多项式整除特征多项式，且包含所有特征值的根。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_POLYNOMIAL | 复杂度: O(n^4)
 */
#define PRESET_MATRIX_MINIMAL_POLY        "matrix_minimal_poly"

/**
 * @brief 核空间 ker(A) = {x | A*x = 0}
 *
 * @details 数学定义：矩阵 A 的零空间（核），dim(ker(A)) = nullity(A)。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_SET | 复杂度: O(mn^2)
 */
#define PRESET_MATRIX_KERNEL              "matrix_kernel"

/**
 * @brief 像空间 Im(A) = {A*x | x in F^n}
 *
 * @details 数学定义：矩阵 A 的列空间（值域），dim(Im(A)) = rank(A)。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_SET | 复杂度: O(mn^2)
 */
#define PRESET_MATRIX_IMAGE               "matrix_image"

/* -------------------- 矩阵分解 -------------------- */

/**
 * @brief LU 分解 A = L * U
 *
 * @details 数学定义：将方阵 A 分解为下三角矩阵 L 和上三角矩阵 U，
 *          用于高效解线性方程组和计算行列式。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_TUPLE (L, U) | 复杂度: O(n^3)
 */
#define PRESET_MATRIX_LU_DECOMPOSITION    "matrix_lu_decomposition"

/**
 * @brief QR 分解 A = Q * R
 *
 * @details 数学定义：将矩阵 A 分解为正交矩阵 Q（Q^T = Q^{-1}）和上三角矩阵 R，
 *          通过 Gram-Schmidt 或 Householder 变换实现。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_TUPLE (Q, R) | 复杂度: O(mn^2)
 */
#define PRESET_MATRIX_QR_DECOMPOSITION    "matrix_qr_decomposition"

/**
 * @brief 奇异值分解 A = U * Sigma * V^T
 *
 * @details 数学定义：任意 m*n 矩阵的完全正交分解，
 *          U, V 为正交矩阵，Sigma 为对角奇异值矩阵。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_TUPLE (U, Sigma, V) | 复杂度: O(mn^2)
 */
#define PRESET_MATRIX_SVD                 "matrix_svd"

/**
 * @brief Cholesky 分解 A = L * L^T（对称正定矩阵）
 *
 * @details 数学定义：对称正定矩阵 A 的三角分解 A = L*L^T，
 *          L 为下三角矩阵，比 LU 分解效率高一倍。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_MATRIX | 复杂度: O(n^3)
 */
#define PRESET_MATRIX_CHOLESKY            "matrix_cholesky"

/**
 * @brief Jordan 标准形 A = P * J * P^{-1}
 *
 * @details 数学定义：矩阵在相似变换下的最简形式，
 *          J 为 Jordan 块组成的块对角矩阵。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_TUPLE (P, J) | 复杂度: O(n^4)
 */
#define PRESET_MATRIX_JORDAN_FORM         "matrix_jordan_form"

/**
 * @brief 谱分解 A = sum_i lambda_i * P_i（正规矩阵）
 *
 * @details 数学定义：正规矩阵（A*A^H = A^H*A）的谱分解，
 *          P_i 为特征空间上的正交投影矩阵。
 *
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n^3)
 */
#define PRESET_MATRIX_SPECTRAL            "matrix_spectral"

/* -------------------- 特殊矩阵 -------------------- */

/**
 * @brief 单位矩阵 I_n: (I_n)_{ij} = delta_{ij}
 *
 * @details 数学定义：主对角线全为 1，其余元素全为 0 的 n 阶方阵，
 *          满足 A * I = I * A = A。
 *
 * @note 输入: PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_MATRIX | 复杂度: O(n^2)
 */
#define PRESET_MATRIX_IDENTITY            "matrix_identity"

/**
 * @brief 零矩阵 O_{m*n}: 所有元素均为 0 的 m*n 矩阵
 *
 * @details 数学定义：满足 A + O = A 的加法单位元。
 *
 * @note 输入: PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER
 *       输出: PRESET_TYPE_MATRIX | 复杂度: O(mn)
 */
#define PRESET_MATRIX_ZERO                "matrix_zero"

/**
 * @brief 对角矩阵 diag(d_1, d_2, ..., d_n)
 *
 * @details 数学定义：仅主对角线元素非零的方阵，对角线上元素为 d_1,...,d_n。
 *
 * @note 输入: PRESET_TYPE_LIST | 输出: PRESET_TYPE_MATRIX | 复杂度: O(n)
 */
#define PRESET_MATRIX_DIAGONAL            "matrix_diagonal"

/**
 * @brief 初等行变换矩阵 E，左乘 E*A 实现行变换
 *
 * @details 数学定义：单位矩阵经一次初等行变换得到的矩阵：
 *          行交换、行倍乘、行倍加。
 *
 * @note 输入: PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER
 *       输出: PRESET_TYPE_MATRIX | 复杂度: O(1)
 */
#define PRESET_MATRIX_ELEMENTARY_ROW      "matrix_elementary_row"

/**
 * @brief Vandermonde 矩阵 V_{ij} = x_i^{j-1}
 *
 * @details 数学定义：由节点 x_1,...,x_n 生成的 Vandermonde 矩阵，
 *          在多项式插值和数值分析中有重要应用。
 *
 * @note 输入: PRESET_TYPE_LIST | 输出: PRESET_TYPE_MATRIX | 复杂度: O(n^2)
 */
#define PRESET_MATRIX_VANDERMONDE         "matrix_vandermonde"

/**
 * @brief Hilbert 矩阵 H_{ij} = 1/(i+j-1)
 *
 * @details 数学定义：i, j 从 1 开始的病态矩阵，
 *          条件数随 n 快速增大，用于测试数值稳定性。
 *
 * @note 输入: PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_MATRIX | 复杂度: O(n^2)
 */
#define PRESET_MATRIX_HILBERT             "matrix_hilbert"

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

#endif /* PRESET_MATRIX_H */
