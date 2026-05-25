/**
 * @file preset_numerical.h
 * @brief 数值分析预设函数块 - 常量定义
 *
 * 提供理论数学研究中常用的数值分析预设函数块。
 * 涵盖方程求根、数值积分、数值微分、插值、拟合、
 * 常微分方程求解、线性方程组及矩阵特征值计算。
 *
 * 包含的预设函数块：
 * - 方程求根 (4个)：二分法、牛顿法、割线法、不动点迭代
 * - 数值积分 (4个)：梯形公式、Simpson公式、高斯求积、Romberg积分
 * - 数值微分 (3个)：前向差分、中心差分、Richardson外推
 * - 插值 (3个)：Lagrange插值、Newton插值、样条插值
 * - 拟合 (2个)：最小二乘法、多项式拟合
 * - 常微分方程 (3个)：Euler方法、四阶Runge-Kutta方法、Adams多步方法
 * - 线性方程组 (3个)：高斯消元法、LU分解、迭代求解法
 * - 矩阵特征值 (2个)：特征值计算、奇异值分解
 *
 * @module Numerical
 * @category PRESET_CATEGORY_CUSTOM
 * @version 5.0.0
 */

#ifndef LV00_PRESET_NUMERICAL_H
#define LV00_PRESET_NUMERICAL_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 预设名称宏常量
 * ================================================================ */

/* ==================== 方程求根 ==================== */

/**
 * @brief 预设：二分法求根
 * @details 数学定义: $x_{n+1} = \frac{a_n + b_n}{2},\ f(a_n) \cdot f(b_n) < 0$，要求 $f$ 在区间 $[a,b]$ 上连续且变号
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n \log \frac{b-a}{\varepsilon})
 */
#define PRESET_NUMERICAL_BISECTION "numerical_bisection"

/**
 * @brief 预设：牛顿法（切线法）求根
 * @details 数学定义: $x_{n+1} = x_n - \frac{f(x_n)}{f'(x_n)}$，二阶收敛，需要函数及其导数
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n \log \frac{1}{\varepsilon})
 */
#define PRESET_NUMERICAL_NEWTON "numerical_newton"

/**
 * @brief 预设：割线法求根
 * @details 数学定义: $x_{n+1} = x_n - f(x_n) \cdot \frac{x_n - x_{n-1}}{f(x_n) - f(x_{n-1})}$，用差商代替导数，超线性收敛
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n \log \frac{1}{\varepsilon})
 */
#define PRESET_NUMERICAL_SECANT "numerical_secant"

/**
 * @brief 预设：不动点迭代法
 * @details 数学定义: $x_{n+1} = g(x_n),\ x^* = g(x^*)$，将 $f(x)=0$ 转化为 $x=g(x)$ 进行迭代
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n \log \frac{1}{\varepsilon})
 */
#define PRESET_NUMERICAL_FIXED_POINT "numerical_fixed_point"

/* ==================== 数值积分 ==================== */

/**
 * @brief 预设：复化梯形公式
 * @details 数学定义: $T_n = \frac{h}{2}\left[f(a) + 2\sum_{i=1}^{n-1} f(x_i) + f(b)\right],\ h = \frac{b-a}{n}$，代数精度为 1
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_NUMERICAL_TRAPEZOID "numerical_trapezoid"

/**
 * @brief 预设：复化Simpson公式
 * @details 数学定义: $S_n = \frac{h}{3}\left[f(a) + 4\sum_{\text{odd}} f(x_i) + 2\sum_{\text{even}} f(x_i) + f(b)\right]$，代数精度为 3
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_NUMERICAL_SIMPSON "numerical_simpson"

/**
 * @brief 预设：高斯求积公式
 * @details 数学定义: $\int_a^b f(x)\,dx \approx \sum_{i=1}^{n} A_i \, f(x_i)$，$x_i$ 为 Legendre 多项式零点，$2n-1$ 次代数精度
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_NUMERICAL_GAUSS_QUADRATURE "numerical_gauss_quadrature"

/**
 * @brief 预设：Romberg积分
 * @details 数学定义: $R_{k,m} = \frac{4^m R_{k,m-1} - R_{k-1,m-1}}{4^m - 1}$，基于 Richardson 外推的逐次加速积分方法
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n^2)
 */
#define PRESET_NUMERICAL_ROMBERG "numerical_romberg"

/* ==================== 数值微分 ==================== */

/**
 * @brief 预设：前向差分
 * @details 数学定义: $f'(x) \approx \frac{f(x+h) - f(x)}{h}$，误差 $O(h)$，一阶精度
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(1)
 */
#define PRESET_NUMERICAL_FORWARD_DIFF "numerical_forward_diff"

/**
 * @brief 预设：中心差分
 * @details 数学定义: $f'(x) \approx \frac{f(x+h) - f(x-h)}{2h}$，误差 $O(h^2)$，二阶精度
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(1)
 */
#define PRESET_NUMERICAL_CENTRAL_DIFF "numerical_central_diff"

/**
 * @brief 预设：Richardson外推法
 * @details 数学定义: $D_{k,m} = \frac{4^m D_{k,m-1} - D_{k-1,m-1}}{4^m - 1}$，通过逐步减半步长并外推消除低阶误差项
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n^2)
 */
#define PRESET_NUMERICAL_RICHARDSON "numerical_richardson"

/* ==================== 插值 ==================== */

/**
 * @brief 预设：Lagrange插值
 * @details 数学定义: $L_n(x) = \sum_{i=0}^{n} y_i \prod_{j=0, j \neq i}^{n} \frac{x - x_j}{x_i - x_j}$，基函数满足 $L_i(x_j) = \delta_{ij}$
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n^2)
 */
#define PRESET_NUMERICAL_LAGRANGE_INTERP "numerical_lagrange_interp"

/**
 * @brief 预设：Newton差商插值
 * @details 数学定义: $N_n(x) = f[x_0] + \sum_{i=1}^{n} f[x_0, \ldots, x_i] \prod_{j=0}^{i-1} (x - x_j)$，利用差商表递推
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n^2)
 */
#define PRESET_NUMERICAL_NEWTON_INTERP "numerical_newton_interp"

/**
 * @brief 预设：三次样条插值
 * @details 数学定义: $S_i(x) = a_i + b_i(x-x_i) + c_i(x-x_i)^2 + d_i(x-x_i)^3, x \in [x_i, x_{i+1}]$，节点处二阶连续
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_NUMERICAL_SPLINE_INTERP "numerical_spline_interp"

/* ==================== 拟合 ==================== */

/**
 * @brief 预设：最小二乘拟合
 * @details 数学定义: $\min_{\{c_i\}} \sum_{j=1}^{m} \left| y_j - \sum_{i=1}^{n} c_i \, \varphi_i(x_j) \right|^2$，法方程 $\Phi^T\Phi c = \Phi^T y$
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_LIST | 复杂度: O(m n^2)
 */
#define PRESET_NUMERICAL_LEAST_SQUARES "numerical_least_squares"

/**
 * @brief 预设：多项式拟合
 * @details 数学定义: $\min_{\{a_k\}} \sum_{j=1}^{m} \left| y_j - \sum_{k=0}^{n} a_k x_j^k \right|^2$，最小二乘框架下 $\varphi_k(x) = x^k$
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_LIST | 复杂度: O(m n^2)
 */
#define PRESET_NUMERICAL_POLYNOMIAL_FIT "numerical_polynomial_fit"

/* ==================== 常微分方程 ==================== */

/**
 * @brief 预设：显式Euler方法
 * @details 数学定义: $y_{n+1} = y_n + h \, f(x_n, y_n),\ h = \frac{x_{\text{end}} - x_0}{N}$，一阶精度，局部截断误差 $O(h^2)$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_LIST | 复杂度: O(N)
 */
#define PRESET_NUMERICAL_EULER "numerical_euler"

/**
 * @brief 预设：经典四阶Runge-Kutta方法
 * @details 数学定义: $y_{n+1} = y_n + \frac{h}{6}(k_1 + 2k_2 + 2k_3 + k_4)$，四阶精度，局部截断误差 $O(h^5)$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_LIST | 复杂度: O(N)
 */
#define PRESET_NUMERICAL_RK4 "numerical_rk4"

/**
 * @brief 预设：Adams-Bashforth四步显式方法
 * @details 数学定义: $y_{n+1} = y_n + \frac{h}{24}(55f_n - 59f_{n-1} + 37f_{n-2} - 9f_{n-3})$，四阶多步线性方法
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_LIST | 复杂度: O(N)
 */
#define PRESET_NUMERICAL_ADAMS "numerical_adams"

/* ==================== 线性方程组 ==================== */

/**
 * @brief 预设：高斯消元法（列主元）
 * @details 数学定义: $Ax = b \Rightarrow Ux = c \Rightarrow x = U^{-1}c$，选列主元提高数值稳定性
 * @note 输入: PRESET_TYPE_MATRIX, PRESET_TYPE_LIST | 输出: PRESET_TYPE_LIST | 复杂度: O(n^3)
 */
#define PRESET_NUMERICAL_GAUSS_ELIMINATION "numerical_gauss_elimination"

/**
 * @brief 预设：LU分解
 * @details 数学定义: $A = LU$，$L$ 为单位下三角矩阵，$U$ 为上三角矩阵，$PA = LU$（部分选主元）
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n^3)
 */
#define PRESET_NUMERICAL_LU_DECOMPOSITION "numerical_lu_decomposition"

/**
 * @brief 预设：迭代求解法（Jacobi/Gauss-Seidel）
 * @details 数学定义: $x^{(k+1)} = D^{-1}(b - (L+U)x^{(k)})$（Jacobi）或 $x^{(k+1)} = (D+L)^{-1}(b - Ux^{(k)})$（Gauss-Seidel）
 * @note 输入: PRESET_TYPE_MATRIX, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_LIST | 复杂度: O(n^2 \cdot k)
 */
#define PRESET_NUMERICAL_ITERATIVE_SOLVE "numerical_iterative_solve"

/* ==================== 矩阵特征值 ==================== */

/**
 * @brief 预设：QR算法计算特征值
 * @details 数学定义: $Ax = \lambda x$，$A = QR \Rightarrow A' = RQ$，迭代收敛至上三角矩阵，对角元即特征值
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_LIST | 复杂度: O(n^3 \cdot k)
 */
#define PRESET_NUMERICAL_EIGENVALUES "numerical_eigenvalues"

/**
 * @brief 预设：奇异值分解 (SVD)
 * @details 数学定义: $A = U \Sigma V^T$，$\sigma_i = \sqrt{\lambda_i(A^T A)}$，奇异值 $\sigma_i \ge 0$ 按降序排列
 * @note 输入: PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_TUPLE | 复杂度: O(m n^2)
 */
#define PRESET_NUMERICAL_SVD "numerical_svd"

/* ================================================================
 * 模块注册函数
 * ================================================================ */

/**
 * @brief 注册所有数值分析预设函数块
 *
 * 将数值分析模块的所有预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_numerical_register(void);

/**
 * @brief 获取数值分析预设函数块数量
 *
 * @return int 数值分析模块预设函数块总数（24）
 */
int preset_numerical_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_NUMERICAL_H */
