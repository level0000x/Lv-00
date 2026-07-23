/**
 * @file preset_statistics.c
 * @brief 统计学预设函数块 - 实现
 *
 * @details 实现统计学模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共22个预设，涵盖描述统计、假设检验、非参数检验、
 *          回归分析、贝叶斯统计和Bootstrap方法。
 *
 * @module Statistics
 * @category PRESET_CATEGORY_PROBABILITY
 * @version 1.0.0
 */

#include "preset_statistics.h"
#include "preset_blocks.h"
#include "lv_internal.h"
#include "lv_utils.h"

#include <string.h>

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 统计学模块预设函数块总数 */

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 注册单个统计学预设
 */
static bool register_stat_preset(
    const char *name,
    const char *description,
    const PresetType *input_types,
    int input_count,
    PresetType output_type,
    const char *math_def,
    const char *complexity,
    bool is_constructive,
    bool is_reversible)
{
    return preset_blocks_register_simple(
        name, description,
        PRESET_CATEGORY_PROBABILITY,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ============================================================
 * 模块注册实现
 * ============================================================ */

bool preset_statistics_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：描述统计 (4个)
     * ============================================================ */

    /* 算术平均值 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        if (register_stat_preset(
                PRESET_STAT_MEAN,
                "算术平均值：mu = (1/n) Σ x_i",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\mu = \\bar{x} = \\frac{1}{n}\\sum_{i=1}^{n} x_i",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 中位数 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        if (register_stat_preset(
                PRESET_STAT_MEDIAN,
                "中位数：排序后位于中间位置的值",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\tilde{x} = \\begin{cases} x_{(n+1)/2} & n\\text{奇数} "
                "\\\\ \\frac{x_{n/2}+x_{n/2+1}}{2} & n\\text{偶数} \\end{cases}",
                "O(n\\log n)", true, false)) {
            success_count++;
        }
    }

    /* 众数 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        if (register_stat_preset(
                PRESET_STAT_MODE,
                "众数：出现频率最高的值 M_o = argmax f(x)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "M_o = \\arg\\max_x f(x)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 分位数 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_SCALAR};
        if (register_stat_preset(
                PRESET_STAT_QUANTILE,
                "分位数：计算第 p 百分位数 P_p",
                inputs, 2, PRESET_TYPE_SCALAR,
                "P_p = x_{\\lceil p\\cdot n/100 \\rceil},\\quad p \\in [0,100]",
                "O(n\\log n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：分布基础 (3个)
     * ============================================================ */

    /* 正态分布 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_stat_preset(
                PRESET_STAT_NORMAL_DIST,
                "正态分布：计算 N(mu, sigma^2) 的概率密度函数值",
                inputs, 3, PRESET_TYPE_PROBABILITY,
                "f(x) = \\frac{1}{\\sigma\\sqrt{2\\pi}}"
                "\\exp\\left(-\\frac{(x-\\mu)^2}{2\\sigma^2}\\right)",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* t分布 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_stat_preset(
                PRESET_STAT_T_DIST,
                "t分布：计算 t(nu) 的概率密度函数值",
                inputs, 2, PRESET_TYPE_PROBABILITY,
                "f(t) = \\frac{\\Gamma((\\nu+1)/2)}{\\sqrt{\\nu\\pi}\\Gamma(\\nu/2)}"
                "\\left(1+\\frac{t^2}{\\nu}\\right)^{-(\\nu+1)/2}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* F分布 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_stat_preset(
                PRESET_STAT_F_DIST,
                "F分布：计算 F(d1, d2) 的概率密度函数值",
                inputs, 3, PRESET_TYPE_PROBABILITY,
                "F \\sim F(d_1, d_2)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：假设检验 (4个)
     * ============================================================ */

    /* Z检验 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_stat_preset(
                PRESET_STAT_Z_TEST,
                "Z检验：大样本均值检验，Z = (x_bar - mu0) / (sigma / sqrt(n))",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "Z = \\frac{\\bar{X} - \\mu_0}{\\sigma/\\sqrt{n}},\\quad H_0: \\mu = \\mu_0",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* t检验 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_SCALAR};
        if (register_stat_preset(
                PRESET_STAT_T_TEST,
                "t检验：小样本均值检验（sigma未知），t = (x_bar - mu0) / (S / sqrt(n))",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "t = \\frac{\\bar{X} - \\mu_0}{S/\\sqrt{n}},\\quad H_0: \\mu = \\mu_0",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* F检验 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST};
        if (register_stat_preset(
                PRESET_STAT_F_TEST,
                "F检验：两总体方差齐性检验，F = S1^2 / S2^2",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "F = \\frac{S_1^2}{S_2^2},\\quad H_0: \\sigma_1^2 = \\sigma_2^2",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ANOVA */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        if (register_stat_preset(
                PRESET_STAT_ANOVA,
                "ANOVA（单因素方差分析）：检验 k 组均值是否相等",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "F = \\frac{SS_B/(k-1)}{SS_W/(n-k)} \\sim F(k-1, n-k),"
                "\\quad H_0: \\mu_1 = \\mu_2 = \\cdots = \\mu_k",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：非参数检验 (2个)
     * ============================================================ */

    /* Wilcoxon秩和检验 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST};
        if (register_stat_preset(
                PRESET_STAT_WILCOXON,
                "Wilcoxon秩和检验：非参数两独立样本位置检验",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "U = n_1 n_2 + \\frac{n_1(n_1+1)}{2} - R_1,"
                "\\quad H_0: \\text{两总体分布相同}",
                "O(n\\log n)", true, false)) {
            success_count++;
        }
    }

    /* Kruskal-Wallis检验 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        if (register_stat_preset(
                PRESET_STAT_KRUSKAL_WALLIS,
                "Kruskal-Wallis检验：非参数多独立样本位置检验",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "H = \\frac{12}{n(n+1)}\\sum\\frac{R_i^2}{n_i} - 3(n+1),"
                "\\quad H_0: \\text{各组分布相同}",
                "O(n\\log n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：回归分析 (3个)
     * ============================================================ */

    /* 多元线性回归 */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_LIST};
        if (register_stat_preset(
                PRESET_STAT_MULTIPLE_LINEAR_REGRESSION,
                "多元线性回归：y = X·beta + eps，beta_hat = (X^T X)^{-1} X^T y",
                inputs, 2, PRESET_TYPE_LIST,
                "\\hat{\\beta} = (X^T X)^{-1} X^T y",
                "O(p^3 + np^2)", true, false)) {
            success_count++;
        }
    }

    /* 逻辑回归 */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_LIST};
        if (register_stat_preset(
                PRESET_STAT_LOGISTIC_REGRESSION,
                "逻辑回归：P(Y=1|X) = 1/(1+e^{-Xβ})，极大似然估计",
                inputs, 2, PRESET_TYPE_LIST,
                "P(Y=1|X) = \\frac{1}{1+e^{-(\\beta_0+\\beta_1 x_1+\\cdots+\\beta_p x_p)}}",
                "O(n k)", true, false)) {
            success_count++;
        }
    }

    /* 决定系数 R^2 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST};
        if (register_stat_preset(
                PRESET_STAT_R_SQUARED,
                "决定系数 R^2：衡量回归模型的拟合优度",
                inputs, 2, PRESET_TYPE_SCALAR,
                "R^2 = 1 - \\frac{SS_{res}}{SS_{tot}} = "
                "1 - \\frac{\\sum(y_i-\\hat{y}_i)^2}{\\sum(y_i-\\bar{y})^2}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：贝叶斯统计 (3个)
     * ============================================================ */

    /* 先验分布 */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_LIST};
        if (register_stat_preset(
                PRESET_STAT_PRIOR_DISTRIBUTION,
                "先验分布：设定参数的先验分布 pi(theta)",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "\\pi(\\theta): \\text{关于参数 } \\theta \\text{ 的先验信念}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 后验分布 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST};
        if (register_stat_preset(
                PRESET_STAT_POSTERIOR_DISTRIBUTION,
                "后验分布：由贝叶斯定理 pi(theta|X) ∝ L(X|theta)·pi(theta)",
                inputs, 3, PRESET_TYPE_FUNCTION,
                "\\pi(\\theta|X) = "
                "\\frac{L(X|\\theta)\\pi(\\theta)}{\\int L(X|\\theta)\\pi(\\theta)\\,d\\theta}",
                "O(\\infty)", true, false)) {
            success_count++;
        }
    }

    /* 贝叶斯因子 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_stat_preset(
                PRESET_STAT_BAYES_FACTOR,
                "贝叶斯因子：B01 = P(X|H0)/P(X|H1)，衡量两个假设的证据强度",
                inputs, 3, PRESET_TYPE_SCALAR,
                "B_{01} = \\frac{P(X|H_0)}{P(X|H_1)}",
                "O(\\infty)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第七部分：Bootstrap (1个)
     * ============================================================ */

    /* Bootstrap重抽样 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_INTEGER, PRESET_TYPE_FUNCTION};
        if (register_stat_preset(
                PRESET_STAT_BOOTSTRAP,
                "Bootstrap重抽样：B次有放回重抽样，估计统计量的抽样分布",
                inputs, 3, PRESET_TYPE_LIST,
                "\\hat{\\theta}_b^* = T(X_b^*),\\quad b = 1,\\ldots,B",
                "O(B n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第八部分：其他 (2个)
     * ============================================================ */

    /* 方差 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        if (register_stat_preset(
                PRESET_STAT_VARIANCE,
                "方差：sigma^2 = (1/n) Σ (x_i - x_bar)^2",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\sigma^2 = \\frac{1}{n}\\sum_{i=1}^{n}(x_i - \\bar{x})^2",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 置信区间 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_SCALAR};
        if (register_stat_preset(
                PRESET_STAT_CONFIDENCE_INTERVAL,
                "置信区间：构建参数在置信水平 1-alpha 下的置信区间",
                inputs, 2, PRESET_TYPE_TUPLE,
                "P(L \\leq \\theta \\leq U) = 1 - \\alpha",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == STATISTICS_PRESET_COUNT;
}

/* ============================================================
 * 模块信息接口
 * ============================================================ */

int preset_statistics_count(void)
{
    return STATISTICS_PRESET_COUNT;
}

PresetCategory preset_statistics_category(void)
{
    return PRESET_CATEGORY_PROBABILITY;
}

bool preset_statistics_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    char **names = (char**)lv_malloc(STATISTICS_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

    const char *preset_names[] = {
        /* 描述统计 */
        PRESET_STAT_MEAN,
        PRESET_STAT_MEDIAN,
        PRESET_STAT_MODE,
        PRESET_STAT_QUANTILE,
        /* 分布基础 */
        PRESET_STAT_NORMAL_DIST,
        PRESET_STAT_T_DIST,
        PRESET_STAT_F_DIST,
        /* 假设检验 */
        PRESET_STAT_Z_TEST,
        PRESET_STAT_T_TEST,
        PRESET_STAT_F_TEST,
        PRESET_STAT_ANOVA,
        /* 非参数检验 */
        PRESET_STAT_WILCOXON,
        PRESET_STAT_KRUSKAL_WALLIS,
        /* 回归分析 */
        PRESET_STAT_MULTIPLE_LINEAR_REGRESSION,
        PRESET_STAT_LOGISTIC_REGRESSION,
        PRESET_STAT_R_SQUARED,
        /* 贝叶斯统计 */
        PRESET_STAT_PRIOR_DISTRIBUTION,
        PRESET_STAT_POSTERIOR_DISTRIBUTION,
        PRESET_STAT_BAYES_FACTOR,
        /* Bootstrap */
        PRESET_STAT_BOOTSTRAP,
        /* 其他 */
        PRESET_STAT_VARIANCE,
        PRESET_STAT_CONFIDENCE_INTERVAL,
    };

    int count = (int)(sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) {
                { void *tmp = names[j]; lv_free(&tmp); }
            }
            { void *tmp = names; lv_free(&tmp); }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
