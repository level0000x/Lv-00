/**
 * @file preset_optimization.h
 * @brief 优化理论预设函数块 - 常量定义
 *
 * 提供理论数学研究中常用的优化理论预设函数块。
 * 涵盖无约束优化、约束优化、线性规划、凸优化、变分法、对偶理论及全局优化。
 *
 * 包含的预设函数块：
 * - 无约束优化 (4个)：梯度下降法、牛顿法、共轭梯度法、拟牛顿法
 * - 约束优化 (4个)：拉格朗日乘子法、KKT条件、罚函数法、障碍函数法
 * - 线性规划 (3个)：单纯形法、内点法、对偶单纯形法
 * - 凸优化 (4个)：凸性检验、凸梯度法、近端梯度法、ADMM
 * - 变分法 (2个)：欧拉-拉格朗日方程、变分法
 * - 对偶理论 (3个)：对偶问题、强对偶性检验、弱对偶性检验
 * - 全局优化 (2个)：模拟退火、遗传算法
 *
 * @module Optimization
 * @category PRESET_CATEGORY_CUSTOM
 * @version 5.0.0
 */

#ifndef LV00_PRESET_OPTIMIZATION_H
#define LV00_PRESET_OPTIMIZATION_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 预设名称宏常量
 * ================================================================ */

/* ==================== 无约束优化 ==================== */

/**
 * @brief 预设：梯度下降法
 * @details 数学定义: $x_{k+1} = x_k - \alpha_k \nabla f(x_k)$，沿目标函数的负梯度方向迭代搜索极小值点
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n \cdot k)
 */
#define PRESET_OPT_GRADIENT_DESCENT "opt_gradient_descent"

/**
 * @brief 预设：牛顿法
 * @details 数学定义: $x_{k+1} = x_k - [\nabla^2 f(x_k)]^{-1} \nabla f(x_k)$，利用 Hessian 矩阵的二阶收敛方法
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n^3 \cdot k)
 */
#define PRESET_OPT_NEWTON_METHOD "opt_newton_method"

/**
 * @brief 预设：共轭梯度法
 * @details 数学定义: $d_{k+1} = -\nabla f(x_{k+1}) + \beta_k d_k,\ \beta_k = \frac{\|\nabla f(x_{k+1})\|^2}{\|\nabla f(x_k)\|^2}$（Fletcher-Reeves 公式），适用于大规模问题
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n \cdot k)
 */
#define PRESET_OPT_CONJUGATE_GRADIENT "opt_conjugate_gradient"

/**
 * @brief 预设：拟牛顿法（BFGS/DFP）
 * @details 数学定义: $B_{k+1} = B_k + \frac{y_k y_k^T}{y_k^T s_k} - \frac{B_k s_k s_k^T B_k}{s_k^T B_k s_k}$（BFGS 更新），通过梯度差分近似 Hessian 逆
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n^2 \cdot k)
 */
#define PRESET_OPT_QUASI_NEWTON "opt_quasi_newton"

/* ==================== 约束优化 ==================== */

/**
 * @brief 预设：拉格朗日乘子法
 * @details 数学定义: $\mathcal{L}(x, \lambda) = f(x) + \sum_{i=1}^{m} \lambda_i h_i(x)$，引入乘子将等式约束转化为无约束
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n^3)
 */
#define PRESET_OPT_LAGRANGE_MULTIPLIER "opt_lagrange_multiplier"

/**
 * @brief 预设：KKT条件
 * @details 数学定义: $\nabla f(x^*) + \sum_i \lambda_i \nabla g_i(x^*) + \sum_j \mu_j \nabla h_j(x^*) = 0,\ \lambda_i \ge 0,\ \lambda_i g_i(x^*) = 0$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_LIST | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(n^3)
 */
#define PRESET_OPT_KKT_CONDITIONS "opt_kkt_conditions"

/**
 * @brief 预设：罚函数法
 * @details 数学定义: $P(x, \rho) = f(x) + \frac{\rho}{2} \sum [\max(0, g_i(x))]^2 + \frac{\rho}{2} \sum h_j(x)^2$，$\rho \to \infty$ 时逼近原问题
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(n \cdot k)
 */
#define PRESET_OPT_PENALTY_METHOD "opt_penalty_method"

/**
 * @brief 预设：障碍函数法
 * @details 数学定义: $B(x, \mu) = f(x) - \mu \sum \ln(-g_i(x))$，对数障碍函数，$\mu \to 0$ 时逼近最优解
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(n \cdot k)
 */
#define PRESET_OPT_BARRIER_METHOD "opt_barrier_method"

/* ==================== 线性规划 ==================== */

/**
 * @brief 预设：单纯形法
 * @details 数学定义: $\min c^T x$ s.t. $Ax = b,\ x \ge 0$，在可行域顶点间移动提优，Dantzig 的经典算法
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_MATRIX, PRESET_TYPE_TUPLE | 输出: PRESET_TYPE_TUPLE | 复杂度: O(2^n) 最坏情况
 */
#define PRESET_OPT_SIMPLEX "opt_simplex"

/**
 * @brief 预设：内点法
 * @details 数学定义: $\min c^T x - \mu \sum \ln x_i$ s.t. $Ax = b,\ x > 0$，多项式时间复杂度 $O(n^{3.5} L)$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_MATRIX, PRESET_TYPE_TUPLE | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n^{3.5} L)
 */
#define PRESET_OPT_INTERIOR_POINT "opt_interior_point"

/**
 * @brief 预设：对偶单纯形法
 * @details 数学定义: $\max b^T y$ s.t. $A^T y \le c$，在保持对偶可行下恢复原始可行性
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_MATRIX, PRESET_TYPE_TUPLE | 输出: PRESET_TYPE_TUPLE | 复杂度: O(2^n) 最坏情况
 */
#define PRESET_OPT_DUAL_SIMPLEX "opt_dual_simplex"

/* ==================== 凸优化 ==================== */

/**
 * @brief 预设：凸性检验
 * @details 数学定义: $f(\theta x + (1-\theta)y) \le \theta f(x) + (1-\theta)f(y),\ \forall \theta \in [0,1]$，凸函数 Hessian 矩阵半正定
 * @note 输入: PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(n^3)
 */
#define PRESET_OPT_CONVEXITY_TEST "opt_convexity_test"

/**
 * @brief 预设：凸梯度法
 * @details 数学定义: $x_{k+1} = x_k - \alpha \nabla f(x_k)$，凸函数保证收敛到全局最优，$f(x^*) \le f(x_k) - \frac{\|\nabla f(x_k)\|^2}{2L}$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n \cdot k)
 */
#define PRESET_OPT_CVX_GRADIENT "opt_cvx_gradient"

/**
 * @brief 预设：近端梯度法
 * @details 数学定义: $x_{k+1} = \text{prox}_{\alpha g}(x_k - \alpha \nabla f(x_k))$，其中 $\text{prox}_{g}(v) = \arg\min_x \frac{1}{2}\|x-v\|^2 + g(x)$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n \cdot k)
 */
#define PRESET_OPT_PROXIMAL_GRADIENT "opt_proximal_gradient"

/**
 * @brief 预设：ADMM（交替方向乘子法）
 * @details 数学定义: $\min f(x) + g(z)$ s.t. $Ax + Bz = c$，将大问题分解为局部子问题迭代求解，适用于分布式优化
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n \cdot k)
 */
#define PRESET_OPT_ADMM "opt_admm"

/* ==================== 变分法 ==================== */

/**
 * @brief 预设：欧拉-拉格朗日方程
 * @details 数学定义: $\frac{\partial L}{\partial y} - \frac{d}{dx}\frac{\partial L}{\partial y'} = 0$，泛函极值问题的必要条件，为变分法核心方程
 * @note 输入: PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_EQUATION | 复杂度: O(n)
 */
#define PRESET_OPT_EULER_LAGRANGE "opt_euler_lagrange"

/**
 * @brief 预设：变分法
 * @details 数学定义: $J[y] = \int_a^b L(x, y(x), y'(x)) \, dx \to \min$，泛函极值问题的数学框架
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(n)
 */
#define PRESET_OPT_CALCULUS_OF_VARIATIONS "opt_calculus_of_variations"

/* ==================== 对偶理论 ==================== */

/**
 * @brief 预设：对偶问题构造
 * @details 数学定义: $g(\lambda, \mu) = \inf_x \mathcal{L}(x, \lambda, \mu) = \inf_x [f(x) + \sum \lambda_i g_i(x) + \sum \mu_j h_j(x)]$，Lagrange 对偶函数
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_LIST | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(n^3)
 */
#define PRESET_OPT_DUAL_PROBLEM "opt_dual_problem"

/**
 * @brief 预设：强对偶性检验
 * @details 数学定义: $p^* = d^*$，其中 $p^* = \inf_x f_0(x),\ d^* = \sup_{\lambda,\mu} g(\lambda, \mu)$（对偶间隙为零），Slater 条件是充分条件
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_LIST | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(n^3)
 */
#define PRESET_OPT_STRONG_DUALITY_TEST "opt_strong_duality_test"

/**
 * @brief 预设：弱对偶性检验
 * @details 数学定义: $d^* \le p^*$（恒成立），对偶间隙 $p^* - d^* \ge 0$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_LIST | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(n^3)
 */
#define PRESET_OPT_WEAK_DUALITY_TEST "opt_weak_duality_test"

/* ==================== 全局优化 ==================== */

/**
 * @brief 预设：模拟退火
 * @details 数学定义: $P(\text{接受}) = \begin{cases} 1 & \Delta E < 0 \\ \exp(-\Delta E / T_k) & \Delta E \ge 0 \end{cases}$，受物理退火启发，通过概率接受劣解跳出局部最优
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n \cdot k)
 */
#define PRESET_OPT_SIMULATED_ANNEALING "opt_simulated_annealing"

/**
 * @brief 预设：遗传算法
 * @details 数学定义: $x_{\text{child}} = \text{Crossover}(x_{p1}, x_{p2}),\ x' = \text{Mutate}(x, p_m)$，基于自然选择和遗传机制的元启发式算法
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_TUPLE | 复杂度: O(N \cdot G \cdot n)
 */
#define PRESET_OPT_GENETIC_ALGORITHM "opt_genetic_algorithm"

/* ================================================================
 * 模块注册函数
 * ================================================================ */

/**
 * @brief 注册所有优化理论预设函数块
 *
 * 将优化理论模块的所有预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_optimization_register(void);

/**
 * @brief 获取优化理论预设函数块数量
 *
 * @return int 优化理论模块预设函数块总数（22）
 */
int preset_optimization_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_OPTIMIZATION_H */
