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

#ifndef LV00_PRESET_PROBABILITY_STATISTICS_H
#define LV00_PRESET_PROBABILITY_STATISTICS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 概率基础 -------------------- */

/** 概率空间 (Ω, F, P) */
#define PRESET_PROBABILITY_SPACE_CREATE     "probability_space_create"

/** 事件概率 P(A) */
#define PRESET_PROBABILITY_EVENT            "probability_event"

/** 对立事件 P(A^c) = 1 - P(A) */
#define PRESET_PROBABILITY_COMPLEMENT       "probability_complement"

/** 概率加法公式 P(A∪B) */
#define PRESET_PROBABILITY_UNION            "probability_union"

/** 交集概率 P(A∩B) */
#define PRESET_PROBABILITY_INTERSECTION     "probability_intersection"

/** 条件概率 P(A|B) */
#ifndef PRESET_CONDITIONAL_PROBABILITY
#define PRESET_CONDITIONAL_PROBABILITY      "conditional_probability"
#endif

/** Bayes定理 */
#ifndef PRESET_BAYES_THEOREM
#define PRESET_BAYES_THEOREM                "bayes_theorem"
#endif

/** 全概率公式 */
#ifndef PRESET_TOTAL_PROBABILITY
#define PRESET_TOTAL_PROBABILITY            "total_probability"
#endif

/* -------------------- 随机变量 -------------------- */

/** 期望 E[X] */
#define PRESET_RANDOM_VARIABLE_EXPECTATION  "random_variable_expectation"

/** 方差 Var(X) */
#define PRESET_RANDOM_VARIABLE_VARIANCE     "random_variable_variance"

/** 标准差 σ(X) */
#define PRESET_RANDOM_VARIABLE_STD          "random_variable_std"

/** 矩 E[X^n] */
#define PRESET_RANDOM_VARIABLE_MOMENT       "random_variable_moment"

/** 协方差 Cov(X,Y) */
#define PRESET_RANDOM_VARIABLE_COVARIANCE   "random_variable_covariance"

/** 相关系数 ρ(X,Y) */
#define PRESET_RANDOM_VARIABLE_CORRELATION  "random_variable_correlation"

/** 矩母函数 M_X(t) */
#define PRESET_MOMENT_GENERATING_FUNCTION   "moment_generating_function"

/** 特征函数 φ_X(t) */
#define PRESET_CHARACTERISTIC_FUNCTION      "characteristic_function"

/* -------------------- 概率分布 -------------------- */

/** 正态分布 N(μ, σ²) */
#define PRESET_DISTRIBUTION_NORMAL          "distribution_normal"

/** 均匀分布 U(a,b) */
#define PRESET_DISTRIBUTION_UNIFORM         "distribution_uniform"

/** 指数分布 Exp(λ) */
#define PRESET_DISTRIBUTION_EXPONENTIAL     "distribution_exponential"

/** 泊松分布 Poisson(λ) */
#define PRESET_DISTRIBUTION_POISSON         "distribution_poisson"

/** 二项分布 B(n,p) */
#define PRESET_DISTRIBUTION_BINOMIAL        "distribution_binomial"

/** 几何分布 Geo(p) */
#define PRESET_DISTRIBUTION_GEOMETRIC       "distribution_geometric"

/** Gamma分布 Γ(α,β) */
#define PRESET_DISTRIBUTION_GAMMA           "distribution_gamma"

/** Beta分布 Beta(α,β) */
#define PRESET_DISTRIBUTION_BETA            "distribution_beta"

/** 卡方分布 χ²(k) */
#define PRESET_DISTRIBUTION_CHI_SQUARED     "distribution_chi_squared"

/** t分布 t(k) */
#define PRESET_DISTRIBUTION_STUDENT_T       "distribution_student_t"

/* -------------------- 统计推断 -------------------- */

/** 最大似然估计 */
#define PRESET_MLE_ESTIMATE                 "mle_estimate"

/** 贝叶斯估计 */
#define PRESET_BAYESIAN_ESTIMATE            "bayesian_estimate"

/** 置信区间 */
#define PRESET_CONFIDENCE_INTERVAL          "confidence_interval"

/** Z检验 */
#define PRESET_HYPOTHESIS_TEST_Z            "hypothesis_test_z"

/** t检验 */
#define PRESET_HYPOTHESIS_TEST_T            "hypothesis_test_t"

/** 卡方检验 */
#define PRESET_HYPOTHESIS_TEST_CHI2         "hypothesis_test_chi2"

/** Kolmogorov-Smirnov检验 */
#define PRESET_KS_TEST                      "ks_test"

/** 线性回归 */
#define PRESET_REGRESSION_LINEAR            "regression_linear"

/** 方差分析 */
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

#endif /* LV00_PRESET_PROBABILITY_STATISTICS_H */
