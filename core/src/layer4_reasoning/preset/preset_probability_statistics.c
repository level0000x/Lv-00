/**
 * @file preset_probability_statistics.c
 * @brief 概率统计预设函数块 - 实现
 *
 * 实现概率统计领域的预设函数块注册。
 * 涵盖概率分布、假设检验、贝叶斯推断、回归分析及置信区间。
 *
 * @module ProbabilityStatistics
 * @category PRESET_EXT_ANALYSIS
 */

#include "preset_probability_statistics.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 概率统计模块预设函数块总数 */
/* 已在头文件中定义 PROBABILITY_STATISTICS_PRESET_COUNT = 14 */

/* ==================== 模块注册实现 ==================== */

int preset_probability_statistics_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：概率分布
     * ============================================================ */

    /* 正态分布 */
    if (preset_blocks_register_by_category(
            "normal_distribution",
            "构造正态分布 N(μ, σ²) 及其PDF/CDF计算",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* 二项分布 */
    if (preset_blocks_register_by_category(
            "binomial_distribution",
            "构造二项分布 B(n, p) 及概率计算",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* Poisson分布 */
    if (preset_blocks_register_by_category(
            "poisson_distribution",
            "构造Poisson分布 P(λ) 及概率计算",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* 期望与方差 */
    if (preset_blocks_register_by_category(
            "expectation_variance",
            "计算随机变量的期望 E[X] 和方差 Var(X)",
            PRESET_EXT_ANALYSIS,
            1, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：假设检验
     * ============================================================ */

    /* Z检验 */
    if (preset_blocks_register_by_category(
            "z_test",
            "单样本Z检验（已知总体方差的均值检验）",
            PRESET_EXT_ANALYSIS,
            4, 1) == 0) {
        success_count++;
    }

    /* t检验 */
    if (preset_blocks_register_by_category(
            "t_test",
            "单样本/双样本t检验（未知总体方差的均值检验）",
            PRESET_EXT_ANALYSIS,
            4, 1) == 0) {
        success_count++;
    }

    /* 卡方检验 */
    if (preset_blocks_register_by_category(
            "chi_square_test",
            "卡方拟合优度检验（观测频数与期望频数比较）",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：贝叶斯推断
     * ============================================================ */

    /* 贝叶斯定理 */
    if (preset_blocks_register_by_category(
            "bayes_theorem",
            "应用贝叶斯定理 P(A|B) = P(B|A)P(A)/P(B)",
            PRESET_EXT_ANALYSIS,
            4, 1) == 0) {
        success_count++;
    }

    /* 贝叶斯参数估计 */
    if (preset_blocks_register_by_category(
            "bayesian_estimation",
            "贝叶斯参数估计：后验 ∝ 似然 x 先验",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：回归与估计
     * ============================================================ */

    /* 最小二乘回归 */
    if (preset_blocks_register_by_category(
            "least_squares_regression",
            "最小二乘法线性回归 y = a + bx",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* 置信区间 */
    if (preset_blocks_register_by_category(
            "confidence_interval",
            "计算总体均值的置信区间",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* 极大似然估计 */
    if (preset_blocks_register_by_category(
            "maximum_likelihood_estimation",
            "极大似然估计（MLE）求参数估计值",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第五部分：统计量
     * ============================================================ */

    /* 相关系数 */
    if (preset_blocks_register_by_category(
            "correlation_coefficient",
            "计算Pearson相关系数 r = Cov(X,Y)/(σ_X σ_Y)",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* 中心极限定理 */
    if (preset_blocks_register_by_category(
            "central_limit_theorem",
            "中心极限定理应用：样本均值的近似分布",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == PROBABILITY_STATISTICS_PRESET_COUNT;
}

int preset_probability_statistics_count(void)
{
    return PROBABILITY_STATISTICS_PRESET_COUNT;
}
