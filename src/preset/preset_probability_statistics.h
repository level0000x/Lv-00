/**
 * @file preset_probability_statistics.h
 * @brief 概率论与数理统计预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的概率论与数理统计运算预设函数块，包括：
 *   - 概率基础：概率空间、事件概率、对立事件、加法公式、条件概率、Bayes定理、全概率公式
 *   - 随机变量：期望、方差、标准差、矩、协方差、相关系数、矩母函数、特征函数
 *   - 概率分布：正态分布、均匀分布、指数分布、泊松分布、二项分布、几何分布、
 *               Gamma分布、Beta分布、卡方分布、t分布
 *   - 统计推断：最大似然估计、贝叶斯估计、置信区间、Z检验、t检验、卡方检验、
 *               KS检验、线性回归、方差分析
 *
 * @module ProbabilityStatistics
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 * @author Lv-00 开发团队
 */

#ifndef PRESET_PROBABILITY_STATISTICS_H
#define PRESET_PROBABILITY_STATISTICS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 概率基础 -------------------- */

/**
 * @brief 概率空间 (Omega, F, P) 构造
 *
 * @details 数学定义：样本空间 Omega、事件域 F（sigma-代数）、概率测度 P
 *          构成的 Kolmogorov 概率空间三元组。
 *
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_FUNCTION
 *       输出: PRESET_TYPE_TUPLE
 *       复杂度: O(1)
 */
#define PRESET_PROBABILITY_SPACE_CREATE     "probability_space_create"

/**
 * @brief 事件概率 P(A)
 *
 * @details 数学定义：P(A) = |A| / |Omega|（古典概型），
 *          或由概率测度 P 直接给出的事件 A 的概率值。
 *
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SCALAR | 复杂度: O(1)
 */
#define PRESET_PROBABILITY_EVENT            "probability_event"

/**
 * @brief 对立事件概率 P(A^c) = 1 - P(A)
 *
 * @details 数学定义：事件 A 的对立事件 A^c 的概率，满足 P(A) + P(A^c) = 1。
 *
 * @note 输入: PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(1) | 可逆: 是
 */
#define PRESET_PROBABILITY_COMPLEMENT       "probability_complement"

/**
 * @brief 概率加法公式 P(A union B)
 *
 * @details 数学定义：P(A ∪ B) = P(A) + P(B) - P(A ∩ B)，
 *          用于计算两个事件至少一个发生的概率。
 *
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR
 *       输出: PRESET_TYPE_SCALAR | 复杂度: O(1)
 */
#define PRESET_PROBABILITY_UNION            "probability_union"

/**
 * @brief 交集概率 P(A ∩ B)
 *
 * @details 数学定义：事件 A 和 B 同时发生的概率。
 *
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(1)
 */
#define PRESET_PROBABILITY_INTERSECTION     "probability_intersection"

/**
 * @brief 条件概率 P(A|B) = P(A∩B) / P(B)
 *
 * @details 数学定义：在事件 B 发生的条件下，事件 A 发生的概率。
 *          当 P(B) > 0 时定义。
 *
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR
 *       输出: PRESET_TYPE_SCALAR | 复杂度: O(1)
 */
#define PRESET_CONDITIONAL_PROBABILITY      "conditional_probability"

/**
 * @brief Bayes 定理
 *
 * @details 数学定义：P(A_i|B) = P(B|A_i)P(A_i) / sum_j P(B|A_j)P(A_j)，
 *          用于根据结果反推原因概率的后验推断。
 *
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR
 *       输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_BAYES_THEOREM                "bayes_theorem"

/**
 * @brief 全概率公式
 *
 * @details 数学定义：P(B) = sum_i P(B|A_i)P(A_i)，
 *          将样本空间划分为互不相容的事件 A_i，计算 B 的总概率。
 *
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_LIST | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_TOTAL_PROBABILITY            "total_probability"

/* -------------------- 随机变量 -------------------- */

/**
 * @brief 期望 E[X] = sum x_i * p_i（离散）或 integral x f(x) dx（连续）
 *
 * @details 数学定义：随机变量 X 的概率加权平均值，度量分布的中心位置。
 *
 * @note 输入: PRESET_TYPE_RANDOM_VARIABLE | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_RANDOM_VARIABLE_EXPECTATION  "random_variable_expectation"

/**
 * @brief 方差 Var(X) = E[(X - mu)^2] = E[X^2] - (E[X])^2
 *
 * @details 数学定义：随机变量 X 与其期望的平方偏差的期望，度量分布的离散程度。
 *
 * @note 输入: PRESET_TYPE_RANDOM_VARIABLE | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_RANDOM_VARIABLE_VARIANCE     "random_variable_variance"

/**
 * @brief 标准差 sigma(X) = sqrt(Var(X))
 *
 * @details 数学定义：方差的平方根，与原始数据具有相同量纲的离散度度量。
 *
 * @note 输入: PRESET_TYPE_RANDOM_VARIABLE | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_RANDOM_VARIABLE_STD          "random_variable_std"

/**
 * @brief k 阶矩 E[X^k]
 *
 * @details 数学定义：随机变量 X 的 k 次幂的期望值，
 *          一阶矩为期望，二阶中心矩为方差。
 *
 * @note 输入: PRESET_TYPE_RANDOM_VARIABLE, PRESET_TYPE_INTEGER
 *       输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_RANDOM_VARIABLE_MOMENT       "random_variable_moment"

/**
 * @brief 协方差 Cov(X,Y) = E[(X - E[X])(Y - E[Y])]
 *
 * @details 数学定义：两随机变量 X, Y 线性相关方向和强度的度量。
 *          Cov(X,Y) > 0 为正相关，< 0 为负相关，= 0 为不相关。
 *
 * @note 输入: PRESET_TYPE_RANDOM_VARIABLE, PRESET_TYPE_RANDOM_VARIABLE
 *       输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_RANDOM_VARIABLE_COVARIANCE   "random_variable_covariance"

/**
 * @brief 相关系数 rho(X,Y) = Cov(X,Y) / (sigma_X * sigma_Y)
 *
 * @details 数学定义：标准化的协方差，取值 [-1, 1]，
 *          rho = 1 为完全正相关，rho = -1 为完全负相关，rho = 0 为不相关。
 *
 * @note 输入: PRESET_TYPE_RANDOM_VARIABLE, PRESET_TYPE_RANDOM_VARIABLE
 *       输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_RANDOM_VARIABLE_CORRELATION  "random_variable_correlation"

/**
 * @brief 矩母函数 M_X(t) = E[e^{tX}]
 *
 * @details 数学定义：随机变量 X 的指数变换的期望值，
 *          通过对 t 求导可获得各阶矩：M_X^{(k)}(0) = E[X^k]。
 *
 * @note 输入: PRESET_TYPE_RANDOM_VARIABLE | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(n)
 */
#define PRESET_MOMENT_GENERATING_FUNCTION   "moment_generating_function"

/**
 * @brief 特征函数 phi_X(t) = E[e^{itX}]
 *
 * @details 数学定义：随机变量 X 的 Fourier 变换，
 *          特征函数唯一决定分布，且总是存在（相比矩母函数）。
 *
 * @note 输入: PRESET_TYPE_RANDOM_VARIABLE | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(n)
 */
#define PRESET_CHARACTERISTIC_FUNCTION      "characteristic_function"

/* -------------------- 概率分布 -------------------- */

/**
 * @brief 正态分布 N(mu, sigma^2)，PDF: f(x) = (1/(sigma*sqrt(2pi))) * exp(-(x-mu)^2/(2*sigma^2))
 *
 * @details 数学定义：参数为均值 mu 和方差 sigma^2 的高斯分布，
 *          中心极限定理的极限分布，自然界最广泛出现的分布。
 *
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_DISTRIBUTION | 复杂度: O(1)
 */
#define PRESET_DISTRIBUTION_NORMAL          "distribution_normal"

/**
 * @brief 均匀分布 U(a,b)，PDF: f(x) = 1/(b-a)，x in [a,b]
 *
 * @details 数学定义：在区间 [a, b] 上等概率取值的连续分布，
 *          是所有连续分布中熵最大的有界分布。
 *
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_DISTRIBUTION | 复杂度: O(1)
 */
#define PRESET_DISTRIBUTION_UNIFORM         "distribution_uniform"

/**
 * @brief 指数分布 Exp(lambda)，PDF: f(x) = lambda * e^{-lambda x}, x >= 0
 *
 * @details 数学定义：描述独立事件发生时间间隔的连续分布，
 *          具有无记忆性：P(X > s+t | X > s) = P(X > t)。
 *
 * @note 输入: PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_DISTRIBUTION | 复杂度: O(1)
 */
#define PRESET_DISTRIBUTION_EXPONENTIAL     "distribution_exponential"

/**
 * @brief 泊松分布 Poisson(lambda)，PMF: P(X=k) = (lambda^k * e^{-lambda}) / k!
 *
 * @details 数学定义：单位时间内随机事件发生次数的离散分布，
 *          lambda 既是期望也是方差。
 *
 * @note 输入: PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_DISTRIBUTION | 复杂度: O(1)
 */
#define PRESET_DISTRIBUTION_POISSON         "distribution_poisson"

/**
 * @brief 二项分布 B(n,p)，PMF: P(X=k) = C(n,k) * p^k * (1-p)^{n-k}
 *
 * @details 数学定义：n 次独立伯努利试验中成功次数的离散分布。
 *          期望 np，方差 np(1-p)。
 *
 * @note 输入: PRESET_TYPE_INTEGER, PRESET_TYPE_SCALAR
 *       输出: PRESET_TYPE_DISTRIBUTION | 复杂度: O(1)
 */
#define PRESET_DISTRIBUTION_BINOMIAL        "distribution_binomial"

/**
 * @brief 几何分布 Geo(p)，PMF: P(X=k) = (1-p)^{k-1} * p, k >= 1
 *
 * @details 数学定义：伯努利试验中首次成功所需次数的离散分布，
 *          具有离散无记忆性。
 *
 * @note 输入: PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_DISTRIBUTION | 复杂度: O(1)
 */
#define PRESET_DISTRIBUTION_GEOMETRIC       "distribution_geometric"

/**
 * @brief Gamma 分布 Gamma(alpha, beta)，PDF: f(x) = (beta^alpha / Gamma(alpha)) * x^{alpha-1} * e^{-beta x}
 *
 * @details 数学定义：参数 alpha（形状）、beta（速率）的连续分布，
 *          指数分布是 alpha=1 的特例，卡方分布是 alpha=k/2, beta=1/2 的特例。
 *
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR
 *       输出: PRESET_TYPE_DISTRIBUTION | 复杂度: O(1)
 */
#define PRESET_DISTRIBUTION_GAMMA           "distribution_gamma"

/**
 * @brief Beta 分布 Beta(alpha, beta)，PDF: f(x) = (x^{alpha-1} * (1-x)^{beta-1}) / B(alpha, beta), x in [0,1]
 *
 * @details 数学定义：定义在 [0,1] 区间上的连续分布，
 *          Beta 函数 B(alpha, beta) 是归一化常数。均匀分布是 alpha=beta=1 的特例。
 *
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR
 *       输出: PRESET_TYPE_DISTRIBUTION | 复杂度: O(1)
 */
#define PRESET_DISTRIBUTION_BETA            "distribution_beta"

/**
 * @brief 卡方分布 chi^2(k)，PDF: f(x) = (x^{k/2-1} * e^{-x/2}) / (2^{k/2} * Gamma(k/2)), x >= 0
 *
 * @details 数学定义：k 个独立标准正态随机变量的平方和所服从的分布，
 *          广泛用于假设检验和置信区间估计。
 *
 * @note 输入: PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_DISTRIBUTION | 复杂度: O(1)
 */
#define PRESET_DISTRIBUTION_CHI_SQUARED     "distribution_chi_squared"

/**
 * @brief Student's t 分布 t(k)，PDF: f(x) ~ (1 + x^2/k)^{-(k+1)/2}
 *
 * @details 数学定义：自由度 k 的 t 分布，
 *          用于小样本均值的假设检验，当 k → infinity 时趋近于标准正态分布。
 *
 * @note 输入: PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_DISTRIBUTION | 复杂度: O(1)
 */
#define PRESET_DISTRIBUTION_STUDENT_T       "distribution_student_t"

/* -------------------- 统计推断 -------------------- */

/**
 * @brief 最大似然估计（MLE）
 *
 * @details 数学定义：theta_hat = argmax_theta L(theta|x) = argmax_theta product_i f(x_i|theta)，
 *          使得观察数据出现概率最大的参数估计值。
 *
 * @note 输入: PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_LIST
 *       输出: PRESET_TYPE_TUPLE | 复杂度: O(n * k)，k 为迭代次数
 */
#define PRESET_MLE_ESTIMATE                 "mle_estimate"

/**
 * @brief 贝叶斯估计
 *
 * @details 数学定义：后验分布 pi(theta|x) ∝ L(theta|x) * pi(theta)，
 *          结合先验分布和似然函数得到参数的后验分布估计。
 *
 * @note 输入: PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_LIST
 *       输出: PRESET_TYPE_TUPLE | 复杂度: O(n)
 */
#define PRESET_BAYESIAN_ESTIMATE            "bayesian_estimate"

/**
 * @brief 置信区间
 *
 * @details 数学定义：在置信水平 (1-alpha) 下，参数 theta 的区间估计 [L, U]，
 *          使得 P(L ≤ theta ≤ U) = 1 - alpha。
 *
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_SCALAR
 *       输出: PRESET_TYPE_TUPLE | 复杂度: O(n)
 */
#define PRESET_CONFIDENCE_INTERVAL          "confidence_interval"

/**
 * @brief Z 检验
 *
 * @details 数学定义：检验统计量 Z = (X_bar - mu_0) / (sigma / sqrt(n))，
 *          用于大样本或已知方差的总体均值检验。
 *
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR
 *       输出: PRESET_TYPE_TUPLE | 复杂度: O(n)
 */
#define PRESET_HYPOTHESIS_TEST_Z            "hypothesis_test_z"

/**
 * @brief Student's t 检验
 *
 * @details 数学定义：检验统计量 t = (X_bar - mu_0) / (s / sqrt(n))，
 *          用于小样本且方差未知的总体均值检验。
 *
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_SCALAR
 *       输出: PRESET_TYPE_TUPLE | 复杂度: O(n)
 */
#define PRESET_HYPOTHESIS_TEST_T            "hypothesis_test_t"

/**
 * @brief 卡方检验
 *
 * @details 数学定义：检验统计量 chi^2 = sum (O_i - E_i)^2 / E_i，
 *          用于分类数据的拟合优度检验和独立性检验。
 *
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_LIST
 *       输出: PRESET_TYPE_TUPLE | 复杂度: O(n)
 */
#define PRESET_HYPOTHESIS_TEST_CHI2         "hypothesis_test_chi2"

/**
 * @brief Kolmogorov-Smirnov 检验
 *
 * @details 数学定义：检验统计量 D_n = sup_x |F_n(x) - F(x)|，
 *          用于检验样本是否来自特定分布的非参数检验方法。
 *
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_DISTRIBUTION
 *       输出: PRESET_TYPE_TUPLE | 复杂度: O(n log n)
 */
#define PRESET_KS_TEST                      "ks_test"

/**
 * @brief 线性回归
 *
 * @details 数学定义：Y = beta_0 + beta_1 * X + epsilon，
 *          通过最小二乘法估计回归系数 beta_0, beta_1。
 *
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_LIST
 *       输出: PRESET_TYPE_TUPLE | 复杂度: O(n)
 */
#define PRESET_REGRESSION_LINEAR            "regression_linear"

/**
 * @brief 方差分析（ANOVA）
 *
 * @details 数学定义：检验统计量 F = MS_between / MS_within，
 *          用于比较多个总体的均值是否相等的假设检验方法。
 *
 * @note 输入: PRESET_TYPE_LIST | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n * k)
 */
#define PRESET_ANOVA                        "anova"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有概率论与数理统计预设函数块
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_probability_statistics_register(void);

/**
 * @brief 获取概率论与数理统计预设函数块数量
 *
 * @return int 概率论与数理统计模块预设函数块总数
 */
int preset_probability_statistics_count(void);

/**
 * @brief 获取概率论与数理统计预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_probability_statistics_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取概率论与数理统计预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_probability_statistics_category(void);

#ifdef __cplusplus
}
#endif

#endif /* PRESET_PROBABILITY_STATISTICS_H */
