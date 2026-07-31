/**
 * @file preset_probability.c
 * @brief 概率论与统计预设函数块 - 实现
 *
 * 实现理论数学研究中常用的概率论与统计预设函数块。
 * 涵盖概率基础、条件概率、离散分布、连续分布、统计量及假设检验。
 *
 * @module Probability
 * @category PRESET_CATEGORY_PROBABILITY
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "preset_probability.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 概率论与统计模块预设函数块总数 */

/* ==================== 内部辅助函数 ==================== */

LV_DECLARE_PRESET_REGISTER(PRESET_CATEGORY_PROBABILITY)

/* ==================== 模块注册实现 ==================== */

bool preset_probability_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：概率基础（5个）
     * ============================================================ */

    /* -------------------- 样本空间 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_SAMPLE_SPACE, "样本空间：定义随机试验的所有可能结果的集合 Omega", 1, PRESET_TYPE_SET, "\\Omega = \\{\\omega_1, \\omega_2, \\ldots, \\omega_n\\}", "O(1)", true, false, PRESET_TYPE_SET);

    /* -------------------- 事件概率 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_EVENT_PROBABILITY, "事件概率：计算事件 A 的概率 P(A) = |A| / |Omega|", 2, PRESET_TYPE_PROBABILITY, "P(A) = \\frac{|A|}{|\\Omega|}", "O(1)", true, false, PRESET_TYPE_SET, PRESET_TYPE_SET);

    /* -------------------- 补事件 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_COMPLEMENT_EVENT, "补事件概率：P(A^c) = 1 - P(A)", 1, PRESET_TYPE_PROBABILITY, "P(A^c) = 1 - P(A)", "O(1)", true, true, PRESET_TYPE_PROBABILITY);

    /* -------------------- 并事件 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_UNION_EVENT, "并事件概率：P(A U B) = P(A) + P(B) - P(A ∩ B)", 3, PRESET_TYPE_PROBABILITY, "P(A \\cup B) = P(A) + P(B) - P(A \\cap B)", "O(1)", true, false, PRESET_TYPE_PROBABILITY, PRESET_TYPE_PROBABILITY, PRESET_TYPE_PROBABILITY);

    /* -------------------- 交事件 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_INTERSECTION_EVENT, "交事件概率：计算 P(A ∩ B)", 2, PRESET_TYPE_PROBABILITY, "P(A \\cap B)", "O(1)", true, false, PRESET_TYPE_PROBABILITY, PRESET_TYPE_PROBABILITY);

    /* ============================================================
     * 第二部分：条件概率（4个）
     * ============================================================ */

    /* -------------------- 条件概率 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_CONDITIONAL, "条件概率：P(A|B) = P(A∩B) / P(B)，已知事件B已发生时A的概率", 2, PRESET_TYPE_PROBABILITY, "P(A|B) = \\frac{P(A \\cap B)}{P(B)}", "O(1)", true, false, PRESET_TYPE_PROBABILITY, PRESET_TYPE_PROBABILITY);

    /* -------------------- 贝叶斯定理 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_BAYES, "贝叶斯定理：P(A|B) = P(B|A)·P(A) / P(B)，后验概率计算", 3, PRESET_TYPE_PROBABILITY, "P(A|B) = \\frac{P(B|A) \\cdot P(A)}{P(B)}", "O(1)", true, false, PRESET_TYPE_PROBABILITY, PRESET_TYPE_PROBABILITY, PRESET_TYPE_PROBABILITY);

    /* -------------------- 独立性检验 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_INDEPENDENCE_TEST, "独立性检验：验证 P(A∩B) = P(A)·P(B) 是否成立", 3, PRESET_TYPE_BOOLEAN, "A \\perp\\!\\!\\!\\perp B \\Leftrightarrow P(A \\cap B) = P(A) \\cdot P(B)", "O(1)", true, false, PRESET_TYPE_PROBABILITY, PRESET_TYPE_PROBABILITY, PRESET_TYPE_PROBABILITY);

    /* -------------------- 全概率公式 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_TOTAL_PROBABILITY, "全概率公式：P(A) = Σ P(A|Bi)·P(Bi)，{Bi} 为完备事件组", 2, PRESET_TYPE_PROBABILITY, "P(A) = \\sum_{i=1}^{n} P(A|B_i) \\cdot P(B_i)", "O(n)", true, false, PRESET_TYPE_LIST, PRESET_TYPE_LIST);

    /* ============================================================
     * 第三部分：离散分布（5个）
     * ============================================================ */

    /* -------------------- 二项分布PMF -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_BINOMIAL_PMF, "二项分布PMF：P(X=k) = C(n,k)·p^k·(1-p)^(n-k)", 3, PRESET_TYPE_PROBABILITY, "P(X=k) = \\binom{n}{k} p^k (1-p)^{n-k}", "O(n)", true, false, PRESET_TYPE_INTEGER, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER);

    /* -------------------- 泊松分布PMF -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_POISSON_PMF, "泊松分布PMF：P(X=k) = λ^k·e^(-λ) / k!", 2, PRESET_TYPE_PROBABILITY, "P(X=k) = \\frac{\\lambda^k e^{-\\lambda}}{k!}", "O(k)", true, false, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER);

    /* -------------------- 几何分布PMF -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_GEOMETRIC_PMF, "几何分布PMF：P(X=k) = (1-p)^(k-1)·p，首次成功所需的试验次数", 2, PRESET_TYPE_PROBABILITY, "P(X=k) = (1-p)^{k-1} p, \\quad k = 1, 2, \\ldots", "O(k)", true, false, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER);

    /* -------------------- 超几何分布PMF -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_HYPERGEOMETRIC_PMF, "超几何分布PMF：从N个中（含K个成功）取n个，恰好k个成功的概率", 4, PRESET_TYPE_PROBABILITY, "P(X=k) = \\frac{\\binom{K}{k}\\binom{N-K}{n-k}}{\\binom{N}{n}}", "O(k)", true, false, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER);

    /* -------------------- 离散均匀分布PMF -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_DISCRETE_UNIFORM_PMF, "离散均匀分布PMF：P(X=k) = 1/n，k ∈ {a, a+1, ..., b}", 2, PRESET_TYPE_PROBABILITY, "P(X=k) = \\frac{1}{n}, \\quad k \\in \\{a, a+1, \\ldots, b\\}", "O(1)", true, false, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER);

    /* ============================================================
     * 第四部分：连续分布（4个）
     * ============================================================ */

    /* -------------------- 正态分布PDF -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_NORMAL_PDF, "正态分布概率密度函数：f(x) = (1/σ√2π)·exp(-(x-μ)^2/2σ^2)", 3, PRESET_TYPE_PROBABILITY, "f(x) = \\frac{1}{\\sigma\\sqrt{2\\pi}} "
                       "\\exp\\left(-\\frac{(x-\\mu)^2}{2\\sigma^2}\\right)", "O(1)", true, false, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR);

    /* -------------------- 正态分布CDF -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_NORMAL_CDF, "正态分布累积分布函数：Φ(x) = ∫_{-∞}^x f(t)dt", 3, PRESET_TYPE_PROBABILITY, "\\Phi(x) = \\int_{-\\infty}^{x} "
                       "\\frac{1}{\\sigma\\sqrt{2\\pi}} e^{-\\frac{(t-\\mu)^2}{2\\sigma^2}} dt", "O(n)", true, false, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR);

    /* -------------------- 指数分布PDF -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_EXPONENTIAL_PDF, "指数分布概率密度函数：f(x) = λ·exp(-λx)，x >= 0", 2, PRESET_TYPE_PROBABILITY, "f(x) = \\lambda e^{-\\lambda x}, \\quad x \\ge 0", "O(1)", true, false, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR);

    /* -------------------- 均匀分布PDF -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_UNIFORM_PDF, "均匀分布概率密度函数：f(x) = 1/(b-a)，a <= x <= b", 3, PRESET_TYPE_PROBABILITY, "f(x) = \\frac{1}{b-a}, \\quad a \\le x \\le b", "O(1)", true, false, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR);

    /* ============================================================
     * 第五部分：统计量（4个）
     * ============================================================ */

    /* -------------------- 样本均值 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_SAMPLE_MEAN, "样本均值：x̄ = (1/n)·Σ xi", 1, PRESET_TYPE_SCALAR, "\\bar{x} = \\frac{1}{n} \\sum_{i=1}^{n} x_i", "O(n)", true, false, PRESET_TYPE_LIST);

    /* -------------------- 样本方差 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_SAMPLE_VARIANCE, "样本方差（无偏估计）：s^2 = (1/(n-1))·Σ(xi - x̄)^2", 1, PRESET_TYPE_SCALAR, "s^2 = \\frac{1}{n-1} \\sum_{i=1}^{n} (x_i - \\bar{x})^2", "O(n)", true, false, PRESET_TYPE_LIST);

    /* -------------------- 样本标准差 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_SAMPLE_STD, "样本标准差：s = √(样本方差)", 1, PRESET_TYPE_SCALAR, "s = \\sqrt{\\frac{1}{n-1} \\sum_{i=1}^{n} (x_i - \\bar{x})^2}", "O(n)", true, false, PRESET_TYPE_LIST);

    /* -------------------- 样本中位数 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_SAMPLE_MEDIAN, "样本中位数：将数据排序后取中间值", 1, PRESET_TYPE_SCALAR, "\\tilde{x} = \\begin{cases} x_{(n+1)/2} & n \\text{ 为奇数} \\\\ "
                       "\\frac{x_{n/2} + x_{n/2+1}}{2} & n \\text{ 为偶数} \\end{cases}", "O(n log n)", true, false, PRESET_TYPE_LIST);

    /* ============================================================
     * 第六部分：假设检验（3个）
     * ============================================================ */

    /* -------------------- Z检验 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_Z_TEST, "Z检验：z = (x̄ - μ₀) / (σ/√n)，大样本均值假设检验", 4, PRESET_TYPE_TUPLE, "z = \\frac{\\bar{x} - \\mu_0}{\\sigma / \\sqrt{n}}", "O(n)", true, false, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR);

    /* -------------------- t检验 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_T_TEST, "t检验：t = (x̄ - μ₀) / (s/√n)，小样本均值假设检验", 2, PRESET_TYPE_TUPLE, "t = \\frac{\\bar{x} - \\mu_0}{s / \\sqrt{n}}", "O(n)", true, false, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR);

    /* -------------------- 卡方检验 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PROB_CHI_SQUARED_TEST, "卡方检验：χ² = Σ (Oi - Ei)^2 / Ei，拟合优度或独立性检验", 2, PRESET_TYPE_TUPLE, "\\chi^2 = \\sum_{i} \\frac{(O_i - E_i)^2}{E_i}", "O(n)", true, false, PRESET_TYPE_LIST, PRESET_TYPE_LIST);

    /* 检查是否所有预设都注册成功 */
    return success_count == PROBABILITY_PRESET_COUNT;
}

int preset_probability_count(void) {
    return PROBABILITY_PRESET_COUNT;
}

PresetCategory preset_probability_category(void) {
    return PRESET_CATEGORY_PROBABILITY;
}

bool preset_probability_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv_malloc(PROBABILITY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        /* 概率基础 */
        PRESET_PROB_SAMPLE_SPACE,
        PRESET_PROB_EVENT_PROBABILITY,
        PRESET_PROB_COMPLEMENT_EVENT,
        PRESET_PROB_UNION_EVENT,
        PRESET_PROB_INTERSECTION_EVENT,
        /* 条件概率 */
        PRESET_PROB_CONDITIONAL,
        PRESET_PROB_BAYES,
        PRESET_PROB_INDEPENDENCE_TEST,
        PRESET_PROB_TOTAL_PROBABILITY,
        /* 离散分布 */
        PRESET_PROB_BINOMIAL_PMF,
        PRESET_PROB_POISSON_PMF,
        PRESET_PROB_GEOMETRIC_PMF,
        PRESET_PROB_HYPERGEOMETRIC_PMF,
        PRESET_PROB_DISCRETE_UNIFORM_PMF,
        /* 连续分布 */
        PRESET_PROB_NORMAL_PDF,
        PRESET_PROB_NORMAL_CDF,
        PRESET_PROB_EXPONENTIAL_PDF,
        PRESET_PROB_UNIFORM_PDF,
        /* 统计量 */
        PRESET_PROB_SAMPLE_MEAN,
        PRESET_PROB_SAMPLE_VARIANCE,
        PRESET_PROB_SAMPLE_STD,
        PRESET_PROB_SAMPLE_MEDIAN,
        /* 假设检验 */
        PRESET_PROB_Z_TEST,
        PRESET_PROB_T_TEST,
        PRESET_PROB_CHI_SQUARED_TEST,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv_free(&tmp);
            }
            {
                void *tmp = names;
                lv_free(&tmp);
            }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
