/**
 * @file preset_linear_algebra.h
 * @brief 线性代数预设函数块
 *
 * 提供理论数学研究中常用的线性代数运算预设函数块，包括：
 * - 矩阵基础运算：加法、减法、乘法、数乘、转置、迹、共轭转置
 * - 行列式与逆：行列式、逆矩阵、伴随矩阵、秩、零化度
 * - 线性方程组：求解、相容性判定、克莱默法则
 * - 向量空间：基判定、维数计算、生成空间、线性无关/相关判定、正交性、Gram-Schmidt 正交化
 * - 特征值与特征向量：特征值计算、特征向量计算、特征多项式、可对角化判定、对角化
 * - 矩阵分解：LU分解、QR分解、奇异值分解、Cholesky分解、Jordan标准形
 * - 内积空间：内积计算、范数计算、正交投影、距离计算
 *
 * @module LinearAlgebra
 * @category PRESET_EXT_LINEAR_ALGEBRA
 * @version 1.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_LINEAR_ALGEBRA_H
#define LV00_PRESET_LINEAR_ALGEBRA_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 矩阵基础运算 -------------------- */

/**
 * @brief 矩阵加法
 *
 * 数学定义：给定同型矩阵 $A, B \in \mathbb{F}^{m \times n}$，构造 $C = A + B$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 *   - B: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - C: 矩阵 (PRESET_TYPE_MATRIX) - 和矩阵
 *
 * 复杂度：O(mn)
 */
#define PRESET_MATRIX_ADD "matrix_add"

/**
 * @brief 矩阵减法
 *
 * 数学定义：给定同型矩阵 $A, B \in \mathbb{F}^{m \times n}$，构造 $C = A - B$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 *   - B: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - C: 矩阵 (PRESET_TYPE_MATRIX) - 差矩阵
 *
 * 复杂度：O(mn)
 */
#define PRESET_MATRIX_SUBTRACT "matrix_subtract"

/**
 * @brief 矩阵乘法
 *
 * 数学定义：给定矩阵 $A \in \mathbb{F}^{m \times p}$ 和 $B \in \mathbb{F}^{p \times n}$，
 * 构造 $C = A \cdot B \in \mathbb{F}^{m \times n}$，其中 $c_{ij} = \sum_{k=1}^{p} a_{ik} b_{kj}$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 *   - B: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - C: 矩阵 (PRESET_TYPE_MATRIX) - 乘积矩阵
 *
 * 复杂度：O(mnp)
 */
#define PRESET_MATRIX_MULTIPLY "matrix_multiply"

/**
 * @brief 矩阵数乘
 *
 * 数学定义：给定标量 $k \in \mathbb{F}$ 和矩阵 $A \in \mathbb{F}^{m \times n}$，
 * 构造 $B = kA$，其中 $b_{ij} = k \cdot a_{ij}$
 *
 * 输入：
 *   - k: 标量 (PRESET_TYPE_SCALAR)
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - B: 矩阵 (PRESET_TYPE_MATRIX) - 数乘结果
 *
 * 复杂度：O(mn)
 */
#define PRESET_MATRIX_SCALE "matrix_scale"

/**
 * @brief 矩阵转置
 *
 * 数学定义：给定矩阵 $A \in \mathbb{F}^{m \times n}$，构造 $A^T \in \mathbb{F}^{n \times m}$，
 * 其中 $(A^T)_{ij} = a_{ji}$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - A^T: 矩阵 (PRESET_TYPE_MATRIX) - 转置矩阵
 *
 * 复杂度：O(mn)
 */
#define PRESET_MATRIX_TRANSPOSE "matrix_transpose"

/**
 * @brief 矩阵迹
 *
 * 数学定义：给定方阵 $A \in \mathbb{F}^{n \times n}$，计算 $\text{tr}(A) = \sum_{i=1}^{n} a_{ii}$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - tr(A): 标量 (PRESET_TYPE_SCALAR) - 迹
 *
 * 复杂度：O(n)
 */
#define PRESET_MATRIX_TRACE "matrix_trace"

/**
 * @brief 共轭转置
 *
 * 数学定义：给定矩阵 $A \in \mathbb{C}^{m \times n}$，构造 $A^{\dagger} = \overline{A}^T$，
 * 其中 $(A^{\dagger})_{ij} = \overline{a_{ji}}$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - A^†: 矩阵 (PRESET_TYPE_MATRIX) - 共轭转置矩阵
 *
 * 复杂度：O(mn)
 */
#define PRESET_MATRIX_CONJUGATE_TRANSPOSE "matrix_conjugate_transpose"

/* -------------------- 行列式与逆 -------------------- */

/**
 * @brief 行列式
 *
 * 数学定义：给定方阵 $A \in \mathbb{F}^{n \times n}$，计算
 * $\det(A) = \sum_{\sigma \in S_n} \text{sgn}(\sigma) \prod_{i=1}^{n} a_{i,\sigma(i)}$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - det(A): 标量 (PRESET_TYPE_SCALAR) - 行列式值
 *
 * 复杂度：O(n^3)
 */
#define PRESET_MATRIX_DETERMINANT "matrix_determinant"

/**
 * @brief 矩阵逆
 *
 * 数学定义：给定可逆方阵 $A \in \mathbb{F}^{n \times n}$，构造 $A^{-1}$，
 * 满足 $A \cdot A^{-1} = A^{-1} \cdot A = I_n$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - A^{-1}: 矩阵 (PRESET_TYPE_MATRIX) - 逆矩阵
 *
 * 复杂度：O(n^3)
 * 前置条件：det(A) != 0
 */
#define PRESET_MATRIX_INVERSE "matrix_inverse"

/**
 * @brief 伴随矩阵
 *
 * 数学定义：给定方阵 $A \in \mathbb{F}^{n \times n}$，构造伴随矩阵 $\text{adj}(A)$，
 * 其中 $(\text{adj}(A))_{ij} = (-1)^{i+j} M_{ji}$，$M_{ji}$ 为余子式
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - adj(A): 矩阵 (PRESET_TYPE_MATRIX) - 伴随矩阵
 *
 * 复杂度：O(n^3)
 */
#define PRESET_MATRIX_ADJOINT "matrix_adjoint"

/**
 * @brief 矩阵秩
 *
 * 数学定义：给定矩阵 $A \in \mathbb{F}^{m \times n}$，计算
 * $\text{rank}(A) = \text{dim}(\text{col}(A)) = \text{dim}(\text{row}(A))$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - rank(A): 整数 (PRESET_TYPE_INTEGER) - 矩阵的秩
 *
 * 复杂度：O(mn^2)
 */
#define PRESET_MATRIX_RANK "matrix_rank"

/**
 * @brief 零化度
 *
 * 数学定义：给定矩阵 $A \in \mathbb{F}^{m \times n}$，计算
 * $\text{nullity}(A) = \text{dim}(\text{ker}(A)) = n - \text{rank}(A)$
 * （秩-零化度定理）
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - nullity(A): 整数 (PRESET_TYPE_INTEGER) - 零化度
 *
 * 复杂度：O(mn^2)
 */
#define PRESET_MATRIX_NULLITY "matrix_nullity"

/* -------------------- 线性方程组 -------------------- */

/**
 * @brief 线性方程组求解
 *
 * 数学定义：给定系数矩阵 $A \in \mathbb{F}^{m \times n}$ 和常数向量 $b \in \mathbb{F}^m$，
 * 求解 $Ax = b$ 的解集
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX) - 系数矩阵
 *   - b: 向量 (PRESET_TYPE_VECTOR) - 常数向量
 * 输出：
 *   - x: 向量 (PRESET_TYPE_VECTOR) - 解向量（特解）
 *
 * 复杂度：O(mn^2)
 */
#define PRESET_LINEAR_SYSTEM_SOLVE "linear_system_solve"

/**
 * @brief 相容性判定
 *
 * 数学定义：判定线性方程组 $Ax = b$ 是否有解，
 * 即 $\text{rank}(A) = \text{rank}(A|b)$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX) - 系数矩阵
 *   - b: 向量 (PRESET_TYPE_VECTOR) - 常数向量
 * 输出：
 *   - consistent: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否相容
 *
 * 复杂度：O(mn^2)
 */
#define PRESET_LINEAR_SYSTEM_CONSISTENCY "linear_system_consistency"

/**
 * @brief 克莱默法则
 *
 * 数学定义：当 $A \in \mathbb{F}^{n \times n}$ 可逆时，
 * $x_i = \frac{\det(A_i)}{\det(A)}$，其中 $A_i$ 是将 $A$ 的第 $i$ 列替换为 $b$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX) - 系数矩阵
 *   - b: 向量 (PRESET_TYPE_VECTOR) - 常数向量
 * 输出：
 *   - x: 向量 (PRESET_TYPE_VECTOR) - 解向量
 *
 * 复杂度：O(n^4)
 * 前置条件：det(A) != 0
 */
#define PRESET_CRAMERS_RULE "cramers_rule"

/* -------------------- 向量空间 -------------------- */

/**
 * @brief 基判定
 *
 * 数学定义：给定向量组 $\{v_1, v_2, \ldots, v_k\}$，
 * 判定其是否构成向量空间 $V$ 的一组基（线性无关且张成 $V$）
 *
 * 输入：
 *   - vectors: 向量列表 (PRESET_TYPE_VECTOR, 可变)
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 空间维数
 * 输出：
 *   - is_basis: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(k^2 n)
 */
#define PRESET_VECTOR_SPACE_BASIS_TEST "vector_space_basis_test"

/**
 * @brief 维数计算
 *
 * 数学定义：给定向量空间的生成集 $S = \{v_1, \ldots, v_k\}$，
 * 计算 $\text{dim}(\text{span}(S))$
 *
 * 输入：
 *   - vectors: 向量列表 (PRESET_TYPE_VECTOR, 可变)
 * 输出：
 *   - dim: 整数 (PRESET_TYPE_INTEGER) - 维数
 *
 * 复杂度：O(k^2 n)
 */
#define PRESET_VECTOR_SPACE_DIMENSION "vector_space_dimension"

/**
 * @brief 生成空间
 *
 * 数学定义：给定向量组 $\{v_1, v_2, \ldots, v_k\}$，
 * 构造生成空间 $\text{span}\{v_1, \ldots, v_k\} = \{\sum_{i=1}^{k} c_i v_i : c_i \in \mathbb{F}\}$
 *
 * 输入：
 *   - vectors: 向量列表 (PRESET_TYPE_VECTOR, 可变)
 * 输出：
 *   - basis: 向量列表 (PRESET_TYPE_VECTOR) - 生成空间的一组基
 *
 * 复杂度：O(k^2 n)
 */
#define PRESET_VECTOR_SPACE_SPAN "vector_space_span"

/**
 * @brief 线性无关判定
 *
 * 数学定义：给定向量组 $\{v_1, v_2, \ldots, v_k\}$，
 * 判定 $\sum_{i=1}^{k} c_i v_i = 0 \Rightarrow c_i = 0, \forall i$
 *
 * 输入：
 *   - vectors: 向量列表 (PRESET_TYPE_VECTOR, 可变)
 * 输出：
 *   - is_independent: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(k^2 n)
 */
#define PRESET_LINEAR_INDEPENDENCE_TEST "linear_independence_test"

/**
 * @brief 线性相关判定
 *
 * 数学定义：给定向量组 $\{v_1, v_2, \ldots, v_k\}$，
 * 判定是否存在不全为零的标量 $c_1, \ldots, c_k$ 使得 $\sum_{i=1}^{k} c_i v_i = 0$
 *
 * 输入：
 *   - vectors: 向量列表 (PRESET_TYPE_VECTOR, 可变)
 * 输出：
 *   - is_dependent: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(k^2 n)
 */
#define PRESET_LINEAR_DEPENDENCE_TEST "linear_dependence_test"

/**
 * @brief 正交性判定
 *
 * 数学定义：给定两个向量 $u, v \in V$（配备内积 $\langle \cdot, \cdot \rangle$），
 * 判定 $\langle u, v \rangle = 0$
 *
 * 输入：
 *   - u: 向量 (PRESET_TYPE_VECTOR)
 *   - v: 向量 (PRESET_TYPE_VECTOR)
 * 输出：
 *   - is_orthogonal: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n)
 */
#define PRESET_ORTHOGONAL_TEST "orthogonal_test"

/**
 * @brief 正交化（Gram-Schmidt）
 *
 * 数学定义：给定线性无关向量组 $\{v_1, \ldots, v_k\}$，构造正交向量组
 * $u_i = v_i - \sum_{j=1}^{i-1} \frac{\langle v_i, u_j \rangle}{\langle u_j, u_j \rangle} u_j$
 *
 * 输入：
 *   - vectors: 向量列表 (PRESET_TYPE_VECTOR, 可变)
 * 输出：
 *   - orthogonal_basis: 向量列表 (PRESET_TYPE_VECTOR) - 正交基
 *
 * 复杂度：O(k^2 n)
 * 前置条件：输入向量组线性无关
 */
#define PRESET_ORTHONORMALIZE "orthonormalize"

/* -------------------- 特征值与特征向量 -------------------- */

/**
 * @brief 特征值计算
 *
 * 数学定义：给定方阵 $A \in \mathbb{F}^{n \times n}$，
 * 求 $\lambda$ 使得 $\det(A - \lambda I) = 0$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - eigenvalues: 标量列表 (PRESET_TYPE_TUPLE) - 特征值列表
 *
 * 复杂度：O(n^3)
 */
#define PRESET_EIGENVALUE_COMPUTE "eigenvalue_compute"

/**
 * @brief 特征向量计算
 *
 * 数学定义：给定方阵 $A$ 和特征值 $\lambda$，
 * 求非零向量 $v$ 使得 $Av = \lambda v$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 *   - lambda: 标量 (PRESET_TYPE_SCALAR) - 特征值
 * 输出：
 *   - eigenvector: 向量 (PRESET_TYPE_VECTOR) - 特征向量
 *
 * 复杂度：O(n^3)
 */
#define PRESET_EIGENVECTOR_COMPUTE "eigenvector_compute"

/**
 * @brief 特征多项式
 *
 * 数学定义：给定方阵 $A \in \mathbb{F}^{n \times n}$，计算
 * $p(\lambda) = \det(A - \lambda I) = \lambda^n + c_{n-1}\lambda^{n-1} + \cdots + c_0$
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - p(λ): 多项式 (PRESET_TYPE_POLYNOMIAL) - 特征多项式
 *
 * 复杂度：O(n^3)
 */
#define PRESET_CHARACTERISTIC_POLYNOMIAL "characteristic_polynomial"

/**
 * @brief 可对角化判定
 *
 * 数学定义：给定方阵 $A \in \mathbb{F}^{n \times n}$，
 * 判定 $A$ 是否可对角化（存在 $n$ 个线性无关的特征向量）
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - is_diagonalizable: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n^3)
 */
#define PRESET_MATRIX_DIAGONALIZABLE_TEST "matrix_diagonalizable_test"

/**
 * @brief 对角化
 *
 * 数学定义：给定可对角化方阵 $A$，构造 $P$ 和 $D$ 使得 $A = PDP^{-1}$，
 * 其中 $D$ 为对角矩阵
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - (P, D): 元组 (PRESET_TYPE_TUPLE) - 变换矩阵和对角矩阵
 *
 * 复杂度：O(n^3)
 * 前置条件：A 可对角化
 */
#define PRESET_MATRIX_DIAGONALIZE "matrix_diagonalize"

/* -------------------- 矩阵分解 -------------------- */

/**
 * @brief LU分解
 *
 * 数学定义：给定方阵 $A \in \mathbb{F}^{n \times n}$，分解为 $A = LU$，
 * 其中 $L$ 为下三角矩阵（对角线元素为1），$U$ 为上三角矩阵
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - (L, U): 元组 (PRESET_TYPE_TUPLE) - 下三角和上三角矩阵
 *
 * 复杂度：O(n^3)
 * 前置条件：A 的所有前导主子式非零
 */
#define PRESET_LU_DECOMPOSITION "lu_decomposition"

/**
 * @brief QR分解
 *
 * 数学定义：给定矩阵 $A \in \mathbb{F}^{m \times n}$，分解为 $A = QR$，
 * 其中 $Q$ 为正交矩阵（$Q^T Q = I$），$R$ 为上三角矩阵
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - (Q, R): 元组 (PRESET_TYPE_TUPLE) - 正交矩阵和上三角矩阵
 *
 * 复杂度：O(mn^2)
 */
#define PRESET_QR_DECOMPOSITION "qr_decomposition"

/**
 * @brief 奇异值分解
 *
 * 数学定义：给定矩阵 $A \in \mathbb{F}^{m \times n}$，分解为 $A = U \Sigma V^T$，
 * 其中 $U \in \mathbb{R}^{m \times m}$ 和 $V \in \mathbb{R}^{n \times n}$ 为正交矩阵，
 * $\Sigma \in \mathbb{R}^{m \times n}$ 为对角矩阵（对角线元素为奇异值 $\sigma_1 \ge \cdots \ge \sigma_r > 0$）
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - (U, Σ, V^T): 元组 (PRESET_TYPE_TUPLE) - 奇异值分解结果
 *
 * 复杂度：O(mn^2)
 */
#define PRESET_SVD_DECOMPOSITION "svd_decomposition"

/**
 * @brief Cholesky分解
 *
 * 数学定义：给定正定对称矩阵 $A \in \mathbb{R}^{n \times n}$，分解为 $A = LL^T$，
 * 其中 $L$ 为下三角矩阵且对角线元素为正
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - L: 矩阵 (PRESET_TYPE_MATRIX) - 下三角矩阵
 *
 * 复杂度：O(n^3)
 * 前置条件：A 为正定对称矩阵
 */
#define PRESET_CHOLESKY_DECOMPOSITION "cholesky_decomposition"

/**
 * @brief Jordan标准形
 *
 * 数学定义：给定方阵 $A \in \mathbb{F}^{n \times n}$，构造可逆矩阵 $P$ 和 Jordan 矩阵 $J$，
 * 使得 $A = PJP^{-1}$，其中 $J$ 为由 Jordan 块组成的块对角矩阵
 *
 * 输入：
 *   - A: 矩阵 (PRESET_TYPE_MATRIX)
 * 输出：
 *   - (P, J): 元组 (PRESET_TYPE_TUPLE) - 变换矩阵和 Jordan 标准形
 *
 * 复杂度：O(n^3)
 */
#define PRESET_JORDAN_NORMAL_FORM "jordan_normal_form"

/* -------------------- 内积空间 -------------------- */

/**
 * @brief 内积计算
 *
 * 数学定义：给定内积空间中的两个向量 $u, v$，
 * 计算 $\langle u, v \rangle = \sum_{i=1}^{n} u_i \overline{v_i}$（标准内积）
 *
 * 输入：
 *   - u: 向量 (PRESET_TYPE_VECTOR)
 *   - v: 向量 (PRESET_TYPE_VECTOR)
 * 输出：
 *   - <u,v>: 标量 (PRESET_TYPE_SCALAR) - 内积值
 *
 * 复杂度：O(n)
 */
#define PRESET_INNER_PRODUCT "inner_product"

/**
 * @brief 范数计算
 *
 * 数学定义：给定内积空间中的向量 $v$，
 * 计算 $\|v\| = \sqrt{\langle v, v \rangle}$（由内积诱导的范数）
 *
 * 输入：
 *   - v: 向量 (PRESET_TYPE_VECTOR)
 * 输出：
 *   - ||v||: 标量 (PRESET_TYPE_SCALAR) - 范数值
 *
 * 复杂度：O(n)
 */
#define PRESET_NORM_COMPUTE "norm_compute"

/**
 * @brief 正交投影
 *
 * 数学定义：给定向量 $v$ 和子空间 $W$（由基 $\{w_1, \ldots, w_k\}$ 张成），
 * 计算 $v$ 在 $W$ 上的正交投影
 * $\text{proj}_W(v) = \sum_{i=1}^{k} \frac{\langle v, w_i \rangle}{\langle w_i, w_i \rangle} w_i$
 *
 * 输入：
 *   - v: 向量 (PRESET_TYPE_VECTOR) - 待投影向量
 *   - basis: 向量列表 (PRESET_TYPE_VECTOR, 可变) - 子空间基
 * 输出：
 *   - proj: 向量 (PRESET_TYPE_VECTOR) - 正交投影
 *
 * 复杂度：O(kn)
 */
#define PRESET_PROJECTION_ORTHOGONAL "projection_orthogonal"

/**
 * @brief 距离计算
 *
 * 数学定义：给定内积空间中的两个向量 $u, v$，
 * 计算 $d(u, v) = \|u - v\| = \sqrt{\langle u - v, u - v \rangle}$
 *
 * 输入：
 *   - u: 向量 (PRESET_TYPE_VECTOR)
 *   - v: 向量 (PRESET_TYPE_VECTOR)
 * 输出：
 *   - d(u,v): 标量 (PRESET_TYPE_SCALAR) - 距离
 *
 * 复杂度：O(n)
 */
#define PRESET_DISTANCE_COMPUTE "distance_compute"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有线性代数预设函数块
 *
 * 将线性代数模块的所有预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_linear_algebra_register(void);

/**
 * @brief 获取线性代数预设函数块数量
 *
 * @return int 线性代数模块预设函数块总数
 */
int preset_linear_algebra_count(void);

/**
 * @brief 获取线性代数模块的预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_linear_algebra_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取线性代数预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_linear_algebra_category(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_LINEAR_ALGEBRA_H */
