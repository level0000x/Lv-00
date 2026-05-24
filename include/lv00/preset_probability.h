/**
 * @file preset_probability.h
 * @brief 概率统计预设函数块
 *
 * 提供理论数学研究中常用的概率统计预设函数块，包括：
 * - 概率基础：基本概率、条件概率、贝叶斯定理、全概率公式、独立性判定、互斥判定
 * - 随机变量：期望、方差、标准差、协方差、相关系数、矩、矩母函数
 * - 概率分布（离散）：二项分布、泊松分布、几何分布、超几何分布、负二项分布
 * - 概率分布（连续）：正态分布、指数分布、均匀分布、Gamma分布、
 *   卡方分布、t分布、F分布
 * - 统计推断：样本均值、样本方差、置信区间、假设检验、中心极限定理、大数定律
 *
 * @module Probability
 * @category PRESET_CATEGORY_ANALYSIS
 */

#ifndef LV00_PRESET_PROBABILITY_H
#define LV00_PRESET_PROBABILITY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 概率基础 ==================== */

/**
 * @brief 基本概率计算
 *
 * 数学定义：$P(A) = \frac{|A|}{|\Omega|}$（古典概型），
 * 或 $P(A) = \lim_{n \to \infty} \frac{n_A}{n}$（频率定义）
 *
 * 输入：
 *   - event: 集合 (PRESET_TYPE_SET) - 事件
 *   - sample_space: 集合 (PRESET_TYPE_SET) - 样本空间
 * 输出：
 *   - P(A): 概率值 (PRESET_TYPE_PROBABILITY) - 事件概率
 *
 * 复杂度：O(|A|)
 */
#define PRESET_PROBABILITY_BASIC "probability_basic"

/**
 * @brief 条件概率
 *
 * 数学定义：$P(A|B) = \frac{P(A \cap B)}{P(B)}$，其中 $P(B) > 0$
 *
 * 输入：
 *   - P_AB: 概率值 (PRESET_TYPE_PROBABILITY) - P(A∩B)
 *   - P_B: 概率值 (PRESET_TYPE_PROBABILITY) - P(B)
 * 输出：
 *   - P(A|B): 概率值 (PRESET_TYPE_PROBABILITY) - 条件概率
 *
 * 复杂度：O(1)
 * 前置条件：P(B) > 0
 */
#define PRESET_CONDITIONAL_PROBABILITY "conditional_probability"

/**
 * @brief 贝叶斯定理
 *
 * 数学定义：$P(A_i|B) = \frac{P(B|A_i) \cdot P(A_i)}{\sum_{j=1}^{n} P(B|A_j) \cdot P(A_j)}$
 *
 * 输入：
 *   - P_B_Ai: 序列 (PRESET_TYPE_SEQUENCE) - 各 P(B|A_i) 值
 *   - P_Ai: 序列 (PRESET_TYPE_SEQUENCE) - 各先验概率 P(A_i) 值
 *   - i: 整数 (PRESET_TYPE_INTEGER) - 目标假设索引
 * 输出：
 *   - P(A_i|B): 概率值 (PRESET_TYPE_PROBABILITY) - 后验概率
 *
 * 复杂度：O(n)
 */
#define PRESET_BAYES_THEOREM "bayes_theorem"

/**
 * @brief 全概率公式
 *
 * 数学定义：$P(B) = \sum_{i=1}^{n} P(B|A_i) \cdot P(A_i)$，
 * 其中 $\{A_1, A_2, \ldots, A_n\}$ 为样本空间的一个划分
 *
 * 输入：
 *   - P_B_Ai: 序列 (PRESET_TYPE_SEQUENCE) - 各 P(B|A_i) 值
 *   - P_Ai: 序列 (PRESET_TYPE_SEQUENCE) - 各 P(A_i) 值
 * 输出：
 *   - P(B): 概率值 (PRESET_TYPE_PROBABILITY) - 全概率
 *
 * 复杂度：O(n)
 */
#define PRESET_TOTAL_PROBABILITY "total_probability"

/**
 * @brief 独立性判定
 *
 * 数学定义：事件 $A$ 与 $B$ 独立当且仅当 $P(A \cap B) = P(A) \cdot P(B)$
 *
 * 输入：
 *   - P_A: 概率值 (PRESET_TYPE_PROBABILITY) - P(A)
 *   - P_B: 概率值 (PRESET_TYPE_PROBABILITY) - P(B)
 *   - P_AB: 概率值 (PRESET_TYPE_PROBABILITY) - P(A∩B)
 * 输出：
 *   - is_independent: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否独立
 *
 * 复杂度：O(1)
 */
#define PRESET_INDEPENDENCE_TEST "independence_test"

/**
 * @brief 互斥判定
 *
 * 数学定义：事件 $A$ 与 $B$ 互斥当且仅当 $A \cap B = \emptyset$，即 $P(A \cap B) = 0$
 *
 * 输入：
 *   - P_AB: 概率值 (PRESET_TYPE_PROBABILITY) - P(A∩B)
 * 输出：
 *   - is_mutual_exclusive: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否互斥
 *
 * 复杂度：O(1)
 */
#define PRESET_MUTUAL_EXCLUSIVE_TEST "mutual_exclusive_test"

/* ==================== 随机变量 ==================== */

/**
 * @brief 期望
 *
 * 数学定义：$E[X] = \sum_{i} x_i \cdot p_i$（离散）或
 * $E[X] = \int_{-\infty}^{+\infty} x \cdot f(x) \, dx$（连续）
 *
 * 输入：
 *   - X: 分布 (PRESET_TYPE_DISTRIBUTION) - 随机变量的分布
 * 输出：
 *   - E[X]: 标量 (PRESET_TYPE_SCALAR) - 期望值
 *
 * 复杂度：O(n)
 */
#define PRESET_EXPECTED_VALUE "expected_value"

/**
 * @brief 方差
 *
 * 数学定义：$\text{Var}(X) = E[(X - \mu)^2] = E[X^2] - (E[X])^2$
 *
 * 输入：
 *   - X: 分布 (PRESET_TYPE_DISTRIBUTION) - 随机变量的分布
 * 输出：
 *   - Var(X): 标量 (PRESET_TYPE_SCALAR) - 方差
 *
 * 复杂度：O(n)
 */
#define PRESET_VARIANCE "variance"

/**
 * @brief 标准差
 *
 * 数学定义：$\sigma(X) = \sqrt{\text{Var}(X)}$
 *
 * 输入：
 *   - X: 分布 (PRESET_TYPE_DISTRIBUTION) - 随机变量的分布
 * 输出：
 *   - sigma(X): 标量 (PRESET_TYPE_SCALAR) - 标准差
 *
 * 复杂度：O(n)
 */
#define PRESET_STANDARD_DEVIATION "standard_deviation"

/**
 * @brief 协方差
 *
 * 数学定义：$\text{Cov}(X,Y) = E[(X - E[X])(Y - E[Y])] = E[XY] - E[X]E[Y]$
 *
 * 输入：
 *   - X: 分布 (PRESET_TYPE_DISTRIBUTION) - 随机变量 X 的分布
 *   - Y: 分布 (PRESET_TYPE_DISTRIBUTION) - 随机变量 Y 的分布
 * 输出：
 *   - Cov(X,Y): 标量 (PRESET_TYPE_SCALAR) - 协方差
 *
 * 复杂度：O(n)
 */
#define PRESET_COVARIANCE "covariance"

/**
 * @brief 相关系数
 *
 * 数学定义：$\rho(X,Y) = \frac{\text{Cov}(X,Y)}{\sigma_X \cdot \sigma_Y}$，
 * $\rho \in [-1, 1]$
 *
 * 输入：
 *   - X: 分布 (PRESET_TYPE_DISTRIBUTION) - 随机变量 X 的分布
 *   - Y: 分布 (PRESET_TYPE_DISTRIBUTION) - 随机变量 Y 的分布
 * 输出：
 *   - rho(X,Y): 标量 (PRESET_TYPE_SCALAR) - 相关系数
 *
 * 复杂度：O(n)
 */
#define PRESET_CORRELATION "correlation"

/**
 * @brief 矩计算
 *
 * 数学定义：$E[X^n] = \sum_{i} x_i^n \cdot p_i$（离散）或
 * $E[X^n] = \int_{-\infty}^{+\infty} x^n \cdot f(x) \, dx$（连续）
 *
 * 输入：
 *   - X: 分布 (PRESET_TYPE_DISTRIBUTION) - 随机变量的分布
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 矩的阶数
 * 输出：
 *   - E[X^n]: 标量 (PRESET_TYPE_SCALAR) - n 阶矩
 *
 * 复杂度：O(n * m)，m 为分布支撑集大小
 */
#define PRESET_MOMENT_COMPUTE "moment_compute"

/**
 * @brief 矩母函数
 *
 * 数学定义：$M_X(t) = E[e^{tX}] = \sum_{i} e^{t x_i} \cdot p_i$（离散）或
 * $M_X(t) = \int_{-\infty}^{+\infty} e^{tx} \cdot f(x) \, dx$（连续）
 *
 * 输入：
 *   - X: 分布 (PRESET_TYPE_DISTRIBUTION) - 随机变量的分布
 *   - t: 标量 (PRESET_TYPE_SCALAR) - 参数值
 * 输出：
 *   - M_X(t): 标量 (PRESET_TYPE_SCALAR) - 矩母函数值
 *
 * 复杂度：O(m)，m 为分布支撑集大小
 */
#define PRESET_MOMENT_GENERATING_FUNCTION "moment_generating_function"

/* ==================== 概率分布 - 离散 ==================== */

/**
 * @brief 二项分布
 *
 * 数学定义：$X \sim B(n, p)$，$P(X = k) = \binom{n}{k} p^k (1-p)^{n-k}$，
 * $E[X] = np$，$\text{Var}(X) = np(1-p)$
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 试验次数
 *   - p: 标量 (PRESET_TYPE_SCALAR) - 成功概率
 *   - k: 整数 (PRESET_TYPE_INTEGER) - 成功次数
 * 输出：
 *   - P(X=k): 标量 (PRESET_TYPE_SCALAR) - 概率质量函数值
 *
 * 复杂度：O(min(k, n-k))
 */
#define PRESET_BINOMIAL_DISTRIBUTION "binomial_distribution"

/**
 * @brief 泊松分布
 *
 * 数学定义：$X \sim \text{Poisson}(\lambda)$，$P(X = k) = \frac{\lambda^k e^{-\lambda}}{k!}$，
 * $E[X] = \lambda$，$\text{Var}(X) = \lambda$
 *
 * 输入：
 *   - lambda: 标量 (PRESET_TYPE_SCALAR) - 均值参数
 *   - k: 整数 (PRESET_TYPE_INTEGER) - 事件次数
 * 输出：
 *   - P(X=k): 标量 (PRESET_TYPE_SCALAR) - 概率质量函数值
 *
 * 复杂度：O(k)
 */
#define PRESET_POISSON_DISTRIBUTION "poisson_distribution"

/**
 * @brief 几何分布
 *
 * 数学定义：$X \sim \text{Geo}(p)$，$P(X = k) = (1-p)^{k-1} p$，
 * $E[X] = \frac{1}{p}$，$\text{Var}(X) = \frac{1-p}{p^2}$
 *
 * 输入：
 *   - p: 标量 (PRESET_TYPE_SCALAR) - 成功概率
 *   - k: 整数 (PRESET_TYPE_INTEGER) - 试验次数
 * 输出：
 *   - P(X=k): 标量 (PRESET_TYPE_SCALAR) - 概率质量函数值
 *
 * 复杂度：O(k)
 */
#define PRESET_GEOMETRIC_DISTRIBUTION "geometric_distribution"

/**
 * @brief 超几何分布
 *
 * 数学定义：$X \sim H(N, K, n)$，$P(X = k) = \frac{\binom{K}{k}\binom{N-K}{n-k}}{\binom{N}{n}}$，
 * $E[X] = n\frac{K}{N}$
 *
 * 输入：
 *   - N: 整数 (PRESET_TYPE_INTEGER) - 总体大小
 *   - K: 整数 (PRESET_TYPE_INTEGER) - 总体中成功数
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 抽取次数
 *   - k: 整数 (PRESET_TYPE_INTEGER) - 成功次数
 * 输出：
 *   - P(X=k): 标量 (PRESET_TYPE_SCALAR) - 概率质量函数值
 *
 * 复杂度：O(min(k, K-k))
 */
#define PRESET_HYPERGEOMETRIC_DISTRIBUTION "hypergeometric_distribution"

/**
 * @brief 负二项分布
 *
 * 数学定义：$X \sim \text{NB}(r, p)$，$P(X = k) = \binom{k+r-1}{r-1} p^r (1-p)^k$，
 * $E[X] = \frac{r(1-p)}{p}$，$\text{Var}(X) = \frac{r(1-p)}{p^2}$
 *
 * 输入：
 *   - r: 整数 (PRESET_TYPE_INTEGER) - 成功次数
 *   - p: 标量 (PRESET_TYPE_SCALAR) - 成功概率
 *   - k: 整数 (PRESET_TYPE_INTEGER) - 失败次数
 * 输出：
 *   - P(X=k): 标量 (PRESET_TYPE_SCALAR) - 概率质量函数值
 *
 * 复杂度：O(min(k, r-1))
 */
#define PRESET_NEGATIVE_BINOMIAL_DISTRIBUTION "negative_binomial_distribution"

/* ==================== 概率分布 - 连续 ==================== */

/**
 * @brief 正态分布
 *
 * 数学定义：$X \sim N(\mu, \sigma^2)$，
 * $f(x) = \frac{1}{\sigma\sqrt{2\pi}} e^{-\frac{(x-\mu)^2}{2\sigma^2}}$，
 * $E[X] = \mu$，$\text{Var}(X) = \sigma^2$
 *
 * 输入：
 *   - mu: 标量 (PRESET_TYPE_SCALAR) - 均值
 *   - sigma: 标量 (PRESET_TYPE_SCALAR) - 标准差
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 自变量值
 * 输出：
 *   - f(x): 标量 (PRESET_TYPE_SCALAR) - 概率密度函数值
 *
 * 复杂度：O(1)
 */
#define PRESET_NORMAL_DISTRIBUTION "normal_distribution"

/**
 * @brief 指数分布
 *
 * 数学定义：$X \sim \text{Exp}(\lambda)$，
 * $f(x) = \lambda e^{-\lambda x}$（$x \ge 0$），
 * $E[X] = \frac{1}{\lambda}$，$\text{Var}(X) = \frac{1}{\lambda^2}$
 *
 * 输入：
 *   - lambda: 标量 (PRESET_TYPE_SCALAR) - 速率参数
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 自变量值
 * 输出：
 *   - f(x): 标量 (PRESET_TYPE_SCALAR) - 概率密度函数值
 *
 * 复杂度：O(1)
 */
#define PRESET_EXPONENTIAL_DISTRIBUTION "exponential_distribution"

/**
 * @brief 均匀分布
 *
 * 数学定义：$X \sim U(a, b)$，
 * $f(x) = \frac{1}{b-a}$（$a \le x \le b$），
 * $E[X] = \frac{a+b}{2}$，$\text{Var}(X) = \frac{(b-a)^2}{12}$
 *
 * 输入：
 *   - a: 标量 (PRESET_TYPE_SCALAR) - 区间下界
 *   - b: 标量 (PRESET_TYPE_SCALAR) - 区间上界
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 自变量值
 * 输出：
 *   - f(x): 标量 (PRESET_TYPE_SCALAR) - 概率密度函数值
 *
 * 复杂度：O(1)
 */
#define PRESET_UNIFORM_DISTRIBUTION "uniform_distribution"

/**
 * @brief Gamma分布
 *
 * 数学定义：$X \sim \Gamma(\alpha, \beta)$，
 * $f(x) = \frac{\beta^\alpha}{\Gamma(\alpha)} x^{\alpha-1} e^{-\beta x}$（$x > 0$），
 * $E[X] = \frac{\alpha}{\beta}$，$\text{Var}(X) = \frac{\alpha}{\beta^2}$
 *
 * 输入：
 *   - alpha: 标量 (PRESET_TYPE_SCALAR) - 形状参数
 *   - beta: 标量 (PRESET_TYPE_SCALAR) - 速率参数
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 自变量值
 * 输出：
 *   - f(x): 标量 (PRESET_TYPE_SCALAR) - 概率密度函数值
 *
 * 复杂度：O(1)
 */
#define PRESET_GAMMA_DISTRIBUTION "gamma_distribution"

/**
 * @brief 卡方分布
 *
 * 数学定义：$X \sim \chi^2(k)$，即 $X = \sum_{i=1}^{k} Z_i^2$，
 * 其中 $Z_i \sim N(0,1)$ 独立同分布，
 * $E[X] = k$，$\text{Var}(X) = 2k$
 *
 * 输入：
 *   - k: 整数 (PRESET_TYPE_INTEGER) - 自由度
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 自变量值
 * 输出：
 *   - f(x): 标量 (PRESET_TYPE_SCALAR) - 概率密度函数值
 *
 * 复杂度：O(1)
 */
#define PRESET_CHI_SQUARED_DISTRIBUTION "chi_squared_distribution"

/**
 * @brief t分布
 *
 * 数学定义：$T = \frac{Z}{\sqrt{V/k}}$，其中 $Z \sim N(0,1)$，
 * $V \sim \chi^2(k)$ 独立，
 * $E[T] = 0$（$k > 1$），$\text{Var}(T) = \frac{k}{k-2}$（$k > 2$）
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 自由度
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 自变量值
 * 输出：
 *   - f(x): 标量 (PRESET_TYPE_SCALAR) - 概率密度函数值
 *
 * 复杂度：O(1)
 */
#define PRESET_T_DISTRIBUTION "t_distribution"

/**
 * @brief F分布
 *
 * 数学定义：$F = \frac{U/m}{V/n}$，其中 $U \sim \chi^2(m)$，
 * $V \sim \chi^2(n)$ 独立，
 * $E[F] = \frac{n}{n-2}$（$n > 2$）
 *
 * 输入：
 *   - m: 整数 (PRESET_TYPE_INTEGER) - 分子自由度
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 分母自由度
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 自变量值
 * 输出：
 *   - f(x): 标量 (PRESET_TYPE_SCALAR) - 概率密度函数值
 *
 * 复杂度：O(1)
 */
#define PRESET_F_DISTRIBUTION "f_distribution"

/* ==================== 统计推断 ==================== */

/**
 * @brief 样本均值
 *
 * 数学定义：$\bar{X} = \frac{1}{n}\sum_{i=1}^{n} X_i$，
 * $E[\bar{X}] = \mu$，$\text{Var}(\bar{X}) = \frac{\sigma^2}{n}$
 *
 * 输入：
 *   - samples: 序列 (PRESET_TYPE_SEQUENCE) - 样本数据
 * 输出：
 *   - x_bar: 标量 (PRESET_TYPE_SCALAR) - 样本均值
 *
 * 复杂度：O(n)
 */
#define PRESET_SAMPLE_MEAN "sample_mean"

/**
 * @brief 样本方差
 *
 * 数学定义：$S^2 = \frac{1}{n-1}\sum_{i=1}^{n}(X_i - \bar{X})^2$（无偏估计），
 * $E[S^2] = \sigma^2$
 *
 * 输入：
 *   - samples: 序列 (PRESET_TYPE_SEQUENCE) - 样本数据
 * 输出：
 *   - s_squared: 标量 (PRESET_TYPE_SCALAR) - 样本方差
 *
 * 复杂度：O(n)
 */
#define PRESET_SAMPLE_VARIANCE "sample_variance"

/**
 * @brief 置信区间
 *
 * 数学定义：$\mu \in \left[\bar{X} - z_{\alpha/2} \frac{\sigma}{\sqrt{n}},\; \bar{X} + z_{\alpha/2} \frac{\sigma}{\sqrt{n}}\right]$
 * （正态总体均值的大样本置信区间）
 *
 * 输入：
 *   - samples: 序列 (PRESET_TYPE_SEQUENCE) - 样本数据
 *   - alpha: 标量 (PRESET_TYPE_SCALAR) - 显著性水平
 *   - sigma: 标量 (PRESET_TYPE_SCALAR) - 总体标准差（未知时传0）
 * 输出：
 *   - (lower, upper): 元组 (PRESET_TYPE_TUPLE) - 置信区间
 *
 * 复杂度：O(n)
 */
#define PRESET_CONFIDENCE_INTERVAL "confidence_interval"

/**
 * @brief 假设检验
 *
 * 数学定义：给定原假设 $H_0$ 和备择假设 $H_1$，计算检验统计量和 p 值，
 * 当 $p < \alpha$ 时拒绝 $H_0$
 *
 * 输入：
 *   - samples: 序列 (PRESET_TYPE_SEQUENCE) - 样本数据
 *   - mu_0: 标量 (PRESET_TYPE_SCALAR) - 原假设下的参数值
 *   - alpha: 标量 (PRESET_TYPE_SCALAR) - 显著性水平
 *   - test_type: 整数 (PRESET_TYPE_INTEGER) - 检验类型（0:双侧, 1:左单侧, 2:右单侧）
 * 输出：
 *   - (p_value, reject): 元组 (PRESET_TYPE_TUPLE) - p 值和是否拒绝 H_0
 *
 * 复杂度：O(n)
 */
#define PRESET_HYPOTHESIS_TEST "hypothesis_test"

/**
 * @brief 中心极限定理应用
 *
 * 数学定义：$\frac{\bar{X}_n - \mu}{\sigma / \sqrt{n}} \xrightarrow{d} N(0,1)$（当 $n \to \infty$），
 * 即样本均值的标准化近似服从标准正态分布
 *
 * 输入：
 *   - samples: 序列 (PRESET_TYPE_SEQUENCE) - 样本数据
 *   - mu: 标量 (PRESET_TYPE_SCALAR) - 总体均值
 *   - sigma: 标量 (PRESET_TYPE_SCALAR) - 总体标准差
 * 输出：
 *   - z: 标量 (PRESET_TYPE_SCALAR) - 标准化检验统计量
 *
 * 复杂度：O(n)
 */
#define PRESET_CENTRAL_LIMIT_THEOREM "central_limit_theorem"

/**
 * @brief 大数定律应用
 *
 * 数学定义：弱大数定律 $\bar{X}_n \xrightarrow{P} \mu$（当 $n \to \infty$），
 * 强大数定律 $\bar{X}_n \xrightarrow{a.s.} \mu$（当 $n \to \infty$），
 * 即样本均值依概率（几乎必然）收敛于总体均值
 *
 * 输入：
 *   - samples: 序列 (PRESET_TYPE_SEQUENCE) - 样本数据
 *   - mu: 标量 (PRESET_TYPE_SCALAR) - 总体均值
 *   - epsilon: 标量 (PRESET_TYPE_SCALAR) - 偏差阈值
 * 输出：
 *   - (converged, deviation): 元组 (PRESET_TYPE_TUPLE) - 是否收敛及实际偏差
 *
 * 复杂度：O(n)
 */
#define PRESET_LAW_OF_LARGE_NUMBERS "law_of_large_numbers"

/* ==================== 概率统计预设（v5.0 统一宏，与 .c 对齐） ==================== */

/* 概率基础 */
#define PRESET_PROB_SAMPLE_SPACE "prob_sample_space"
#define PRESET_PROB_EVENT_PROBABILITY "prob_event_probability"
#define PRESET_PROB_COMPLEMENT_EVENT "prob_complement_event"
#define PRESET_PROB_UNION_EVENT "prob_union_event"
#define PRESET_PROB_INTERSECTION_EVENT "prob_intersection_event"

/* 条件概率 */
#define PRESET_PROB_CONDITIONAL "prob_conditional"
#define PRESET_PROB_BAYES "prob_bayes"
#define PRESET_PROB_INDEPENDENCE_TEST "prob_independence_test"
#define PRESET_PROB_TOTAL_PROBABILITY "prob_total_probability"

/* 离散分布 */
#define PRESET_PROB_BINOMIAL_PMF "prob_binomial_pmf"
#define PRESET_PROB_POISSON_PMF "prob_poisson_pmf"
#define PRESET_PROB_GEOMETRIC_PMF "prob_geometric_pmf"
#define PRESET_PROB_HYPERGEOMETRIC_PMF "prob_hypergeometric_pmf"
#define PRESET_PROB_DISCRETE_UNIFORM_PMF "prob_discrete_uniform_pmf"

/* 连续分布 */
#define PRESET_PROB_NORMAL_PDF "prob_normal_pdf"
#define PRESET_PROB_NORMAL_CDF "prob_normal_cdf"
#define PRESET_PROB_EXPONENTIAL_PDF "prob_exponential_pdf"
#define PRESET_PROB_UNIFORM_PDF "prob_uniform_pdf"

/* 统计量 */
#define PRESET_PROB_SAMPLE_MEAN "prob_sample_mean"
#define PRESET_PROB_SAMPLE_VARIANCE "prob_sample_variance"
#define PRESET_PROB_SAMPLE_STD "prob_sample_std"
#define PRESET_PROB_SAMPLE_MEDIAN "prob_sample_median"

/* 假设检验 */
#define PRESET_PROB_Z_TEST "prob_z_test"
#define PRESET_PROB_T_TEST "prob_t_test"
#define PRESET_PROB_CHI_SQUARED_TEST "prob_chi_squared_test"

/* ==================== 模块初始化 ==================== */

/**
 * @brief 注册概率统计预设函数块
 *
 * 此函数由 preset_blocks_init() 自动调用，用户无需直接调用。
 *
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_probability_register(void);

/**
 * @brief 获取概率统计模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_probability_count(void);

/**
 * @brief 获取概率统计预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_probability_category(void);

/**
 * @brief 获取概率统计模块的预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_probability_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_PROBABILITY_H */
