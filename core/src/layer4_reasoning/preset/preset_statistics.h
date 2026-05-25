/**
 * @file preset_statistics.h
 * @brief 统计学预设函数块
 *
 * 提供理论数学研究中常用的统计分析预设函数块。
 * 涵盖描述统计、假设检验、非参数检验、回归分析、
 * 贝叶斯统计以及Bootstrap方法。
 *
 * 包含的预设函数块：
 * - 描述统计（4个）：均值、中位数、众数、分位数
 * - 分布基础（3个）：正态分布、t分布、F分布
 * - 假设检验（4个）：Z检验、t检验、F检验、ANOVA
 * - 非参数检验（2个）：Wilcoxon秩和检验、Kruskal-Wallis检验
 * - 回归分析（3个）：多元线性回归、逻辑回归、决定系数
 * - 贝叶斯统计（3个）：先验分布、后验分布、贝叶斯因子
 * - Bootstrap方法（1个）
 * - 其他（2个）：方差、置信区间
 *
 * @module Statistics
 * @category PRESET_CATEGORY_PROBABILITY
 */

#ifndef LV00_PRESET_STATISTICS_H
#define LV00_PRESET_STATISTICS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 描述统计 ==================== */

/**
 * @brief 算术平均值
 *
 * 数学定义：$\mu = \bar{x} = \frac{1}{n}\sum_{i=1}^{n} x_i$
 *
 * 输入：
 *   - data: 列表 (PRESET_TYPE_LIST) - 数据向量
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR)
 *
 * 复杂度：O(n)
 */
#define PRESET_STAT_MEAN "stat_mean"

/**
 * @brief 中位数
 *
 * 数学定义：排序后位于中间位置的值
 * $$\tilde{x} = \begin{cases} x_{(n+1)/2} & n\text{为奇数} \\ \frac{x_{n/2}+x_{n/2+1}}{2} & n\text{为偶数} \end{cases}$$
 *
 * 输入：
 *   - data: 列表 (PRESET_TYPE_LIST)
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR)
 *
 * 复杂度：O(n log n)
 */
#define PRESET_STAT_MEDIAN "stat_median"

/**
 * @brief 众数
 *
 * 数学定义：$M_o = \arg\max_x f(x)$，出现频率最高的值
 *
 * 输入：
 *   - data: 列表 (PRESET_TYPE_LIST)
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR)
 *
 * 复杂度：O(n)
 */
#define PRESET_STAT_MODE "stat_mode"

/**
 * @brief 分位数
 *
 * 数学定义：第 $p$ 百分位数 $P_p$ 是使得至少 $p\%$ 的数据不超过它的值
 *
 * 输入：
 *   - data: 列表 (PRESET_TYPE_LIST)
 *   - p: 标量 (PRESET_TYPE_SCALAR) - 分位点 $p \in [0, 100]$
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR)
 *
 * 复杂度：O(n log n)
 */
#define PRESET_STAT_QUANTILE "stat_quantile"

/* ==================== 分布基础 ==================== */

/**
 * @brief 正态分布
 *
 * 数学定义：$f(x) = \frac{1}{\sigma\sqrt{2\pi}}\exp\left(-\frac{(x-\mu)^2}{2\sigma^2}\right)$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR)
 *   - mu: 标量 (PRESET_TYPE_SCALAR) - 均值
 *   - sigma: 标量 (PRESET_TYPE_SCALAR) - 标准差
 * 输出：
 *   - 概率 (PRESET_TYPE_PROBABILITY) - 概率密度值
 *
 * 复杂度：O(1)
 */
#define PRESET_STAT_NORMAL_DIST "stat_normal_dist"

/**
 * @brief t分布
 *
 * 数学定义：$f(t) = \frac{\Gamma((\nu+1)/2)}{\sqrt{\nu\pi}\,\Gamma(\nu/2)}\left(1+\frac{t^2}{\nu}\right)^{-(\nu+1)/2}$
 *
 * 输入：
 *   - t: 标量 (PRESET_TYPE_SCALAR)
 *   - nu: 整数 (PRESET_TYPE_INTEGER) - 自由度
 * 输出：
 *   - 概率 (PRESET_TYPE_PROBABILITY) - 概率密度值
 *
 * 复杂度：O(n)
 */
#define PRESET_STAT_T_DIST "stat_t_dist"

/**
 * @brief F分布
 *
 * 数学定义：$F \sim F(d_1, d_2)$ 的概率密度函数
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR)
 *   - d1: 整数 (PRESET_TYPE_INTEGER) - 分子自由度
 *   - d2: 整数 (PRESET_TYPE_INTEGER) - 分母自由度
 * 输出：
 *   - 概率 (PRESET_TYPE_PROBABILITY)
 *
 * 复杂度：O(n)
 */
#define PRESET_STAT_F_DIST "stat_f_dist"

/* ==================== 假设检验 ==================== */

/**
 * @brief Z检验
 *
 * 数学定义：大样本均值检验，检验统计量 $Z = \frac{\bar{X} - \mu_0}{\sigma/\sqrt{n}}$
 * $H_0: \mu = \mu_0$，拒绝域由标准正态分布的分位数确定
 *
 * 输入：
 *   - data: 列表 (PRESET_TYPE_LIST) - 样本数据
 *   - mu0: 标量 (PRESET_TYPE_SCALAR) - 原假设均值
 *   - sigma: 标量 (PRESET_TYPE_SCALAR) - 已知总体标准差
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否拒绝 $H_0$（显著性水平 $\alpha = 0.05$）
 *
 * 复杂度：O(n)
 */
#define PRESET_STAT_Z_TEST "stat_z_test"

/**
 * @brief t检验
 *
 * 数学定义：小样本均值检验（$\sigma$ 未知），检验统计量 $t = \frac{\bar{X} - \mu_0}{S/\sqrt{n}}$
 * 自由度为 $n-1$ 的 t 分布
 *
 * 输入：
 *   - data: 列表 (PRESET_TYPE_LIST)
 *   - mu0: 标量 (PRESET_TYPE_SCALAR) - 原假设均值
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否拒绝 $H_0$
 *
 * 复杂度：O(n)
 */
#define PRESET_STAT_T_TEST "stat_t_test"

/**
 * @brief F检验
 *
 * 数学定义：两总体方差齐性检验，$F = S_1^2/S_2^2 \sim F(n_1-1, n_2-1)$
 * $H_0: \sigma_1^2 = \sigma_2^2$
 *
 * 输入：
 *   - data1: 列表 (PRESET_TYPE_LIST)
 *   - data2: 列表 (PRESET_TYPE_LIST)
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否拒绝 $H_0$
 *
 * 复杂度：O(n)
 */
#define PRESET_STAT_F_TEST "stat_f_test"

/**
 * @brief ANOVA（单因素方差分析）
 *
 * 数学定义：检验 $k$ 个总体均值是否相等
 * $$F = \frac{SS_B/(k-1)}{SS_W/(n-k)} \sim F(k-1, n-k)$$
 *
 * 输入：
 *   - groups: 列表 (PRESET_TYPE_LIST) - $k$ 个样本组的列表
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否拒绝 $H_0$（所有组均值相等）
 *
 * 复杂度：O(n)
 */
#define PRESET_STAT_ANOVA "stat_anova"

/* ==================== 非参数检验 ==================== */

/**
 * @brief Wilcoxon秩和检验
 *
 * 数学定义：非参数两独立样本位置检验（Mann-Whitney U检验的等价形式）
 * $H_0$：两总体分布相同
 *
 * 输入：
 *   - data1: 列表 (PRESET_TYPE_LIST)
 *   - data2: 列表 (PRESET_TYPE_LIST)
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否拒绝 $H_0$
 *
 * 复杂度：O(n log n)
 */
#define PRESET_STAT_WILCOXON "stat_wilcoxon"

/**
 * @brief Kruskal-Wallis检验
 *
 * 数学定义：非参数多独立样本位置检验，是Mann-Whitney检验的推广
 * 检验统计量 $H = \frac{12}{n(n+1)}\sum\frac{R_i^2}{n_i} - 3(n+1)$
 *
 * 输入：
 *   - groups: 列表 (PRESET_TYPE_LIST) - 多个样本组的列表
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否拒绝各组分布相同
 *
 * 复杂度：O(n log n)
 */
#define PRESET_STAT_KRUSKAL_WALLIS "stat_kruskal_wallis"

/* ==================== 回归分析 ==================== */

/**
 * @brief 多元线性回归
 *
 * 数学定义：$y = X\beta + \varepsilon$，最小二乘估计 $\hat{\beta} = (X^TX)^{-1}X^Ty$
 *
 * 输入：
 *   - X: 矩阵 (PRESET_TYPE_MATRIX) - 设计矩阵 $n \times p$
 *   - y: 列表 (PRESET_TYPE_LIST) - 响应向量
 * 输出：
 *   - 列表 (PRESET_TYPE_LIST) - 回归系数 $\hat{\beta}$
 *
 * 复杂度：O(p^3 + np^2)
 */
#define PRESET_STAT_MULTIPLE_LINEAR_REGRESSION "stat_multiple_linear_regression"

/**
 * @brief 逻辑回归
 *
 * 数学定义：$P(Y=1|X) = \frac{1}{1 + e^{-(\beta_0 + \beta_1 x_1 + \cdots + \beta_p x_p)}}$
 * 使用极大似然估计参数
 *
 * 输入：
 *   - X: 矩阵 (PRESET_TYPE_MATRIX) - 特征矩阵
 *   - y: 列表 (PRESET_TYPE_LIST) - 二元响应向量
 * 输出：
 *   - 列表 (PRESET_TYPE_LIST) - 回归系数
 *
 * 复杂度：O(n k)
 */
#define PRESET_STAT_LOGISTIC_REGRESSION "stat_logistic_regression"

/**
 * @brief 决定系数 $R^2$
 *
 * 数学定义：$R^2 = 1 - \frac{SS_{res}}{SS_{tot}} = 1 - \frac{\sum(y_i - \hat{y}_i)^2}{\sum(y_i - \bar{y})^2}$
 *
 * 输入：
 *   - y: 列表 (PRESET_TYPE_LIST) - 观测值
 *   - y_hat: 列表 (PRESET_TYPE_LIST) - 预测值
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 决定系数
 *
 * 复杂度：O(n)
 */
#define PRESET_STAT_R_SQUARED "stat_r_squared"

/* ==================== 贝叶斯统计 ==================== */

/**
 * @brief 先验分布
 *
 * 数学定义：设定参数的先验分布 $\pi(\theta)$，反映在观测数据前对参数的信念
 *
 * 输入：
 *   - dist_type: 整数 (PRESET_TYPE_INTEGER) - 分布类型
 *   - params: 列表 (PRESET_TYPE_LIST) - 分布参数
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 先验分布 $\pi(\theta)$
 *
 * 复杂度：O(1)
 */
#define PRESET_STAT_PRIOR_DISTRIBUTION "stat_prior_distribution"

/**
 * @brief 后验分布
 *
 * 数学定义：由贝叶斯定理计算后验分布
 * $$\pi(\theta|X) = \frac{L(X|\theta)\pi(\theta)}{\int L(X|\theta)\pi(\theta)\,d\theta}$$
 *
 * 输入：
 *   - prior: 函数 (PRESET_TYPE_FUNCTION) - 先验分布
 *   - likelihood: 函数 (PRESET_TYPE_FUNCTION) - 似然函数
 *   - X: 列表 (PRESET_TYPE_LIST) - 观测数据
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 后验分布
 *
 * 复杂度：O(∞)
 */
#define PRESET_STAT_POSTERIOR_DISTRIBUTION "stat_posterior_distribution"

/**
 * @brief 贝叶斯因子
 *
 * 数学定义：两个假设 $H_0$ 和 $H_1$ 的后验几率比与先验几率比的比值
 * $$B_{01} = \frac{P(X|H_0)}{P(X|H_1)}$$
 *
 * 输入：
 *   - X: 列表 (PRESET_TYPE_LIST) - 观测数据
 *   - H0: 函数 (PRESET_TYPE_FUNCTION) - $H_0$ 下的边际似然
 *   - H1: 函数 (PRESET_TYPE_FUNCTION) - $H_1$ 下的边际似然
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 贝叶斯因子 $B_{01}$
 *
 * 复杂度：O(∞)
 */
#define PRESET_STAT_BAYES_FACTOR "stat_bayes_factor"

/* ==================== Bootstrap ==================== */

/**
 * @brief Bootstrap重抽样
 *
 * 数学定义：对原始样本进行有放回的 $B$ 次重抽样，计算目标统计量
 * 的Bootstrap分布，用于构建置信区间或估计标准误差
 *
 * 输入：
 *   - data: 列表 (PRESET_TYPE_LIST) - 原始样本
 *   - B: 整数 (PRESET_TYPE_INTEGER) - Bootstrap次数
 *   - statistic: 函数 (PRESET_TYPE_FUNCTION) - 目标统计量函数
 * 输出：
 *   - 列表 (PRESET_TYPE_LIST) - Bootstrap统计量分布
 *
 * 复杂度：O(B n)
 */
#define PRESET_STAT_BOOTSTRAP "stat_bootstrap"

/* ==================== 其他 ==================== */

/**
 * @brief 方差
 *
 * 数学定义：$\sigma^2 = \frac{1}{n}\sum_{i=1}^{n}(x_i - \bar{x})^2$
 *
 * 输入：
 *   - data: 列表 (PRESET_TYPE_LIST)
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR)
 *
 * 复杂度：O(n)
 */
#define PRESET_STAT_VARIANCE "stat_variance"

/**
 * @brief 置信区间
 *
 * 数学定义：对总体参数 $\theta$，置信水平 $1-\alpha$ 的置信区间
 * $P(L \leq \theta \leq U) = 1 - \alpha$
 *
 * 输入：
 *   - data: 列表 (PRESET_TYPE_LIST)
 *   - alpha: 标量 (PRESET_TYPE_SCALAR) - 显著性水平
 * 输出：
 *   - 元组 (PRESET_TYPE_TUPLE) - 置信区间 $(L, U)$
 *
 * 复杂度：O(n)
 */
#define PRESET_STAT_CONFIDENCE_INTERVAL "stat_confidence_interval"

/* ==================== 模块初始化 ==================== */

/**
 * @brief 注册统计学预设函数块
 *
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_statistics_register(void);

/**
 * @brief 获取统计学模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_statistics_count(void);

/**
 * @brief 获取统计学模块的预设类别
 *
 * @return 预设类别
 */
PresetCategory preset_statistics_category(void);

/**
 * @brief 获取统计学模块的所有预设名称
 *
 * @param out_names 输出名称数组（调用者负责释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_statistics_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_STATISTICS_H */
