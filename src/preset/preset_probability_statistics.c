/**
 * @file preset_probability_statistics.c
 * @brief 概率论与数理统计预设函数块模块 - 实现
 *
 * 实现理论数学研究中常用的概率论与数理统计预设函数块。
 * 涵盖概率基础、随机变量、概率分布、统计推断，
 * 共 35 个预设。
 *
 * @module ProbabilityStatistics
 * @category PRESET_EXT_ANALYSIS
 * @version 1.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_probability_statistics.h -> preset_blocks.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_by_category() 声明
 *   -> 提供 PresetExtendedCategory 枚举（PRESET_EXT_ANALYSIS）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 * lv00_internal.h / lv00_utils.h
 *   -> 提供 lv00_malloc、lv00_free、lv00_strdup、lv00_log_* 等
 * ============================================================
 */
#include "preset_probability_statistics.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 概率论与数理统计模块预设函数块总数 */
#define PROBABILITY_STATISTICS_PRESET_COUNT 35

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个概率论与数理统计预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有概率论与数理统计预设都属于 PRESET_EXT_ANALYSIS 类别。
 * 使用 preset_blocks_register_by_category() 直接注册到扩展类别。
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_count 输入端口数量
 * @param output_count 输出端口数量
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_ps_preset(const char *name, const char *description, int input_count, int output_count) {
    return preset_blocks_register_by_category(name, description, PRESET_EXT_ANALYSIS, input_count, output_count);
}

/* ==================== 模块注册实现 ==================== */

bool preset_probability_statistics_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一组：概率基础（8个预设）
     *
     * 涵盖概率论的基本概念与运算：
     *  - 概率空间、事件概率、对立事件、加法公式、交集概率
     *  - 条件概率、Bayes定理、全概率公式
     * ============================================================ */

    /**
     * @brief probability_space_create - 概率空间
     *
     * 构造概率空间 (Omega, F, P)。
     * 概率空间是概率论的公理化基础，由样本空间、事件sigma-代数
     * 和满足Kolmogorov公理的概率测度组成。
     *
     * @math (\\Omega, \\mathcal{F}, P), \\quad P: \\mathcal{F} \\to [0,1], \\; P(\\Omega) = 1
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_PROBABILITY_SPACE_CREATE,
                               "概率空间：构造概率空间 (Omega, F, P)，"
                               "由样本空间、sigma-代数和概率测量组成",
                               1, 1)) {
            success_count++;
        }
    }

    /**
     * @brief probability_event - 事件概率
     *
     * 计算事件 A 的概率 P(A)。
     *
     * @math P(A) \\in [0, 1], \\quad P(\\emptyset) = 0, \\quad P(\\Omega) = 1
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_PROBABILITY_EVENT,
                               "事件概率：计算事件 A 的概率 P(A)，"
                               "满足 Kolmogorov 公理",
                               2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief probability_complement - 对立事件
     *
     * 计算对立事件 A^c 的概率。
     *
     * @math P(A^c) = 1 - P(A)
     * @constructive true
     * @reversible true
     */
    {
        if (register_ps_preset(PRESET_PROBABILITY_COMPLEMENT, "对立事件：计算 P(A^c) = 1 - P(A)", 1, 1)) {
            success_count++;
        }
    }

    /**
     * @brief probability_union - 概率加法公式
     *
     * 计算事件并集的概率 P(A U B)。
     *
     * @math P(A \\cup B) = P(A) + P(B) - P(A \\cap B)
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_PROBABILITY_UNION, "概率加法公式：计算 P(A∪B) = P(A) + P(B) - P(A∩B)", 2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief probability_intersection - 交集概率
     *
     * 计算事件交集的概率 P(A ∩ B)。
     *
     * @math P(A \\cap B) = P(A) \\cdot P(B|A)
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_PROBABILITY_INTERSECTION, "交集概率：计算 P(A∩B) = P(A) · P(B|A)", 2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief conditional_probability - 条件概率
     *
     * 计算条件概率 P(A|B)。
     * 条件概率是在事件 B 发生的前提下事件 A 发生的概率。
     *
     * @math P(A|B) = \\frac{P(A \\cap B)}{P(B)}, \\quad P(B) > 0
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_CONDITIONAL_PROBABILITY,
                               "条件概率：计算 P(A|B) = P(A∩B) / P(B)，"
                               "在事件B发生前提下事件A的概率",
                               2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief bayes_theorem - Bayes定理
     *
     * 应用 Bayes 定理计算后验概率。
     * Bayes 定理是贝叶斯推断的理论基础，将先验概率更新为后验概率。
     *
     * @math P(A_i|B) = \\frac{P(B|A_i) \\cdot P(A_i)}{\\sum_{j=1}^{n} P(B|A_j) \\cdot P(A_j)}
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_BAYES_THEOREM,
                               "Bayes定理：由先验概率和似然函数计算后验概率，"
                               "P(A|B) = P(B|A)P(A) / P(B)",
                               3, 1)) {
            success_count++;
        }
    }

    /**
     * @brief total_probability - 全概率公式
     *
     * 应用全概率公式计算事件概率。
     * 当样本空间可被互不相容的事件完备组分割时使用。
     *
     * @math P(B) = \\sum_{i=1}^{n} P(B|A_i) \\cdot P(A_i), \\quad \\{A_i\\} \\text{ 为完备事件组}
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_TOTAL_PROBABILITY, "全概率公式：利用完备事件组分解计算 P(B) = Σ P(B|A_i)P(A_i)",
                               2, 1)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二组：随机变量（9个预设）
     *
     * 涵盖随机变量的数字特征与生成函数：
     *  - 期望、方差、标准差、矩
     *  - 协方差、相关系数
     *  - 矩母函数、特征函数
     * ============================================================ */

    /**
     * @brief random_variable_expectation - 期望
     *
     * 计算随机变量 X 的数学期望 E[X]。
     *
     * @math E[X] = \\sum_i x_i p_i \\; (离散), \\quad E[X] = \\int_{-\\infty}^{\\infty} x f(x) \\, dx \\; (连续)
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_RANDOM_VARIABLE_EXPECTATION,
                               "期望：计算随机变量 X 的数学期望 E[X]，"
                               "离散情形为加权求和，连续情形为积分",
                               1, 1)) {
            success_count++;
        }
    }

    /**
     * @brief random_variable_variance - 方差
     *
     * 计算随机变量 X 的方差 Var(X)。
     * 方差度量随机变量取值偏离期望的程度。
     *
     * @math \\text{Var}(X) = E[(X - \\mu)^2] = E[X^2] - (E[X])^2
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_RANDOM_VARIABLE_VARIANCE,
                               "方差：计算 Var(X) = E[(X-μ)²] = E[X²] - (E[X])²，"
                               "度量随机变量偏离期望的程度",
                               1, 1)) {
            success_count++;
        }
    }

    /**
     * @brief random_variable_std - 标准差
     *
     * 计算随机变量 X 的标准差 sigma(X)。
     * 标准差是方差的算术平方根，与原变量具有相同量纲。
     *
     * @math \\sigma(X) = \\sqrt{\\text{Var}(X)}
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_RANDOM_VARIABLE_STD, "标准差：计算 σ(X) = √Var(X)，方差的算术平方根", 1, 1)) {
            success_count++;
        }
    }

    /**
     * @brief random_variable_moment - 矩
     *
     * 计算随机变量 X 的 n 阶矩 E[X^n]。
     *
     * @math E[X^n] = \\sum_i x_i^n p_i \\; (离散), \\quad E[X^n] = \\int x^n f(x) \\, dx \\; (连续)
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_RANDOM_VARIABLE_MOMENT, "矩：计算随机变量 X 的 n 阶矩 E[X^n]", 2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief random_variable_covariance - 协方差
     *
     * 计算两个随机变量 X, Y 的协方差 Cov(X,Y)。
     * 协方差度量两个随机变量线性相关性的方向。
     *
     * @math \\text{Cov}(X,Y) = E[(X - E[X])(Y - E[Y])] = E[XY] - E[X]E[Y]
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_RANDOM_VARIABLE_COVARIANCE,
                               "协方差：计算 Cov(X,Y) = E[(X-μ_X)(Y-μ_Y)]，"
                               "度量两个随机变量的线性相关性方向",
                               2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief random_variable_correlation - 相关系数
     *
     * 计算两个随机变量 X, Y 的 Pearson 相关系数 rho(X,Y)。
     * 相关系数是标准化后的协方差，取值范围为 [-1, 1]。
     *
     * @math \\rho(X,Y) = \\frac{\\text{Cov}(X,Y)}{\\sigma_X \\cdot \\sigma_Y}, \\quad \\rho \\in [-1, 1]
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_RANDOM_VARIABLE_CORRELATION,
                               "相关系数：计算 Pearson 相关系数 ρ(X,Y) = Cov(X,Y)/(σ_X·σ_Y)，"
                               "取值范围 [-1, 1]",
                               2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief moment_generating_function - 矩母函数
     *
     * 计算随机变量 X 的矩母函数 M_X(t)。
     * 矩母函数在存在区间内唯一确定分布，且可由其导数得到各阶矩。
     *
     * @math M_X(t) = E[e^{tX}], \\quad E[X^n] = M_X^{(n)}(0)
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_MOMENT_GENERATING_FUNCTION,
                               "矩母函数：计算 M_X(t) = E[e^{tX}]，"
                               "可由其导数得到各阶矩",
                               1, 1)) {
            success_count++;
        }
    }

    /**
     * @brief characteristic_function - 特征函数
     *
     * 计算随机变量 X 的特征函数 phi_X(t)。
     * 特征函数是矩母函数的 Fourier 变换形式，始终存在。
     *
     * @math \\varphi_X(t) = E[e^{itX}] = \\int_{-\\infty}^{\\infty} e^{itx} dF(x)
     * @constructive true
     * @reversible true（由逆 Fourier 变换可恢复分布）
     */
    {
        if (register_ps_preset(PRESET_CHARACTERISTIC_FUNCTION,
                               "特征函数：计算 φ_X(t) = E[e^{itX}]，"
                               "矩母函数的Fourier变换形式，始终存在",
                               1, 1)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三组：概率分布（10个预设）
     *
     * 涵盖常用连续与离散概率分布：
     *  - 连续分布：正态、均匀、指数、Gamma、Beta、卡方、t分布
     *  - 离散分布：泊松、二项、几何
     * ============================================================ */

    /**
     * @brief distribution_normal - 正态分布
     *
     * 正态分布 N(μ, σ²)，又称高斯分布。
     * 由中心极限定理，大量独立随机变量之和近似服从正态分布。
     *
     * @math f(x) = \\frac{1}{\\sigma\\sqrt{2\\pi}} \\exp\\left(-\\frac{(x-\\mu)^2}{2\\sigma^2}\\right)
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_DISTRIBUTION_NORMAL,
                               "正态分布 N(μ, σ²)：高斯分布，概率密度函数为钟形曲线，"
                               "由中心极限定理保证其普遍性",
                               2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief distribution_uniform - 均匀分布
     *
     * 均匀分布 U(a,b)，在区间 [a,b] 上等概率取值。
     *
     * @math f(x) = \\frac{1}{b-a}, \\quad x \\in [a, b]
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_DISTRIBUTION_UNIFORM, "均匀分布 U(a,b)：在区间 [a,b] 上等概率取值的连续分布", 2,
                               1)) {
            success_count++;
        }
    }

    /**
     * @brief distribution_exponential - 指数分布
     *
     * 指数分布 Exp(λ)，描述独立随机事件之间的等待时间。
     * 具有无记忆性：P(X > s+t | X > s) = P(X > t)。
     *
     * @math f(x) = \\lambda e^{-\\lambda x}, \\quad x \\geq 0, \\; \\lambda > 0
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_DISTRIBUTION_EXPONENTIAL, "指数分布 Exp(λ)：描述事件等待时间，具有无记忆性", 1,
                               1)) {
            success_count++;
        }
    }

    /**
     * @brief distribution_poisson - 泊松分布
     *
     * 泊松分布 Poisson(λ)，描述单位时间/空间内稀有事件发生的次数。
     *
     * @math P(X = k) = \\frac{\\lambda^k e^{-\\lambda}}{k!}, \\quad k = 0, 1, 2, \\ldots
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_DISTRIBUTION_POISSON, "泊松分布 Poisson(λ)：描述单位时间内稀有事件发生次数", 1,
                               1)) {
            success_count++;
        }
    }

    /**
     * @brief distribution_binomial - 二项分布
     *
     * 二项分布 B(n,p)，描述 n 次独立伯努利试验中成功的次数。
     *
     * @math P(X = k) = \\binom{n}{k} p^k (1-p)^{n-k}, \\quad k = 0, 1, \\ldots, n
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_DISTRIBUTION_BINOMIAL, "二项分布 B(n,p)：n 次独立伯努利试验中成功次数的分布", 2,
                               1)) {
            success_count++;
        }
    }

    /**
     * @brief distribution_geometric - 几何分布
     *
     * 几何分布 Geo(p)，描述首次成功所需的试验次数。
     * 具有无记忆性。
     *
     * @math P(X = k) = (1-p)^{k-1} p, \\quad k = 1, 2, 3, \\ldots
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_DISTRIBUTION_GEOMETRIC, "几何分布 Geo(p)：首次成功所需的试验次数，具有无记忆性",
                               1, 1)) {
            success_count++;
        }
    }

    /**
     * @brief distribution_gamma - Gamma分布
     *
     * Gamma 分布 Gamma(α, β)，指数分布和卡方分布的推广。
     *
     * @math f(x) = \\frac{\\beta^{\\alpha}}{\\Gamma(\\alpha)} x^{\\alpha-1} e^{-\\beta x}, \\quad x > 0
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_DISTRIBUTION_GAMMA, "Gamma分布 Γ(α,β)：指数分布与卡方分布的推广", 2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief distribution_beta - Beta分布
     *
     * Beta 分布 Beta(α, β)，定义在 [0,1] 上的灵活连续分布。
     * 常用于贝叶斯推断中的先验分布。
     *
     * @math f(x) = \\frac{x^{\\alpha-1}(1-x)^{\\beta-1}}{B(\\alpha,\\beta)}, \\quad x \\in [0,1]
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_DISTRIBUTION_BETA,
                               "Beta分布 Beta(α,β)：定义在 [0,1] 上的灵活分布，"
                               "常用于贝叶斯推断中的先验分布",
                               2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief distribution_chi_squared - 卡方分布
     *
     * 卡方分布 χ²(k)，k 个独立标准正态变量平方和的分布。
     * 广泛用于拟合优度检验和独立性检验。
     *
     * @math \\chi^2 = \\sum_{i=1}^{k} Z_i^2, \\quad Z_i \\sim N(0,1) \\text{ i.i.d.}
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_DISTRIBUTION_CHI_SQUARED,
                               "卡方分布 χ²(k)：k 个独立标准正态变量平方和的分布，"
                               "用于拟合优度检验",
                               1, 1)) {
            success_count++;
        }
    }

    /**
     * @brief distribution_student_t - t分布
     *
     * t 分布 t(k)，用于小样本下总体均值的推断。
     * 当自由度 k 趋于无穷时，t 分布趋于标准正态分布。
     *
     * @math T = \\frac{Z}{\\sqrt{V/k}}, \\quad Z \\sim N(0,1), \\; V \\sim \\chi^2(k)
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_DISTRIBUTION_STUDENT_T,
                               "t分布 t(k)：小样本下总体均值推断的分布，"
                               "自由度趋于无穷时趋于标准正态分布",
                               1, 1)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四组：统计推断（8个预设）
     *
     * 涵盖参数估计与假设检验：
     *  - 参数估计：最大似然估计、贝叶斯估计、置信区间
     *  - 假设检验：Z检验、t检验、卡方检验、KS检验
     *  - 建模分析：线性回归、方差分析
     * ============================================================ */

    /**
     * @brief mle_estimate - 最大似然估计
     *
     * 使用最大似然估计法估计分布参数。
     * MLE 选择使观测数据出现概率最大的参数值。
     *
     * @math \\hat{\\theta}_{\\text{MLE}} = \\arg\\max_{\\theta} L(\\theta) = \\arg\\max_{\\theta} \\prod_{i=1}^{n} f(x_i; \\theta)
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_MLE_ESTIMATE,
                               "最大似然估计：选择使观测数据似然函数最大的参数值 "
                               "θ_MLE = argmax L(θ)",
                               2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief bayesian_estimate - 贝叶斯估计
     *
     * 使用贝叶斯方法估计分布参数。
     * 将参数视为随机变量，由先验分布结合数据得到后验分布。
     *
     * @math \\pi(\\theta|x) \\propto L(x|\\theta) \\cdot \\pi(\\theta)
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_BAYESIAN_ESTIMATE,
                               "贝叶斯估计：由先验分布 π(θ) 结合似然函数 L(x|θ) "
                               "得到后验分布 π(θ|x)",
                               3, 1)) {
            success_count++;
        }
    }

    /**
     * @brief confidence_interval - 置信区间
     *
     * 计算参数的置信区间。
     * 置信区间以一定置信水平包含真实参数值。
     *
     * @math P(\\hat{\\theta} - z_{\\alpha/2} \\cdot \\text{SE} \\leq \\theta \\leq \\hat{\\theta} + z_{\\alpha/2} \\cdot \\text{SE}) = 1 - \\alpha
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_CONFIDENCE_INTERVAL,
                               "置信区间：计算参数的 (1-α) 置信区间，"
                               "以指定置信水平包含真实参数值",
                               3, 1)) {
            success_count++;
        }
    }

    /**
     * @brief hypothesis_test_z - Z检验
     *
     * 执行 Z 检验，用于大样本下总体均值的假设检验。
     * 要求总体方差已知或样本量足够大。
     *
     * @math Z = \\frac{\\bar{X} - \\mu_0}{\\sigma / \\sqrt{n}}, \\quad Z \\sim N(0,1) \\text{ under } H_0
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_HYPOTHESIS_TEST_Z,
                               "Z检验：大样本下总体均值的假设检验，"
                               "检验统计量 Z = (X̄ - μ₀) / (σ/√n)",
                               3, 1)) {
            success_count++;
        }
    }

    /**
     * @brief hypothesis_test_t - t检验
     *
     * 执行 t 检验，用于小样本下总体均值的假设检验。
     * 当总体方差未知时使用样本方差替代。
     *
     * @math t = \\frac{\\bar{X} - \\mu_0}{S / \\sqrt{n}}, \\quad t \\sim t(n-1) \\text{ under } H_0
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_HYPOTHESIS_TEST_T,
                               "t检验：小样本下总体均值的假设检验，"
                               "检验统计量 t = (X̄ - μ₀) / (S/√n)",
                               3, 1)) {
            success_count++;
        }
    }

    /**
     * @brief hypothesis_test_chi2 - 卡方检验
     *
     * 执行卡方检验，用于分类数据的拟合优度检验或独立性检验。
     *
     * @math \\chi^2 = \\sum_{i=1}^{k} \\frac{(O_i - E_i)^2}{E_i}, \\quad \\chi^2 \\sim \\chi^2(k-1) \\text{ under } H_0
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_HYPOTHESIS_TEST_CHI2,
                               "卡方检验：分类数据的拟合优度检验或独立性检验，"
                               "χ² = Σ(O_i - E_i)² / E_i",
                               2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief ks_test - Kolmogorov-Smirnov检验
     *
     * 执行 Kolmogorov-Smirnov 检验，比较样本分布与参考分布。
     * 基于经验分布函数与理论分布函数的最大偏差。
     *
     * @math D_n = \\sup_x |F_n(x) - F_0(x)|
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_KS_TEST,
                               "KS检验：基于经验分布函数与理论分布函数最大偏差的"
                               "非参数检验，D_n = sup|F_n(x) - F_0(x)|",
                               2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief regression_linear - 线性回归
     *
     * 执行线性回归分析，建立因变量与自变量之间的线性关系模型。
     * 使用最小二乘法估计回归系数。
     *
     * @math Y = \\beta_0 + \\beta_1 X + \\varepsilon, \\quad \\hat{\\beta} = (X^T X)^{-1} X^T Y
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_REGRESSION_LINEAR,
                               "线性回归：建立因变量与自变量的线性模型，"
                               "使用最小二乘法估计回归系数 β = (X^TX)^{-1}X^TY",
                               2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief anova - 方差分析
     *
     * 执行方差分析（ANOVA），比较多组均值是否存在显著差异。
     * 将总变异分解为组间变异和组内变异。
     *
     * @math F = \\frac{\\text{MSB}}{\\text{MSW}} = \\frac{SSB/(k-1)}{SSW/(n-k)}, \\quad F \\sim F(k-1, n-k)
     * @constructive true
     * @reversible false
     */
    {
        if (register_ps_preset(PRESET_ANOVA,
                               "方差分析（ANOVA）：比较多组均值差异，"
                               "F = MSB/MSW，将总变异分解为组间与组内变异",
                               2, 1)) {
            success_count++;
        }
    }

    /* ============================================================
     * 注册结果统计
     * ============================================================ */

    if (success_count < PROBABILITY_STATISTICS_PRESET_COUNT) {
        LV00_LOG_WARNING("概率论与数理统计模块：共 %d 个预设，成功注册 %d 个", PROBABILITY_STATISTICS_PRESET_COUNT,
                         success_count);
    }

    return success_count == PROBABILITY_STATISTICS_PRESET_COUNT;
}

/* ==================== 模块信息函数 ==================== */

/**
 * @brief 获取概率论与数理统计预设函数块数量
 *
 * @return int 预设函数块总数（35）
 */
int preset_probability_statistics_count(void) {
    return PROBABILITY_STATISTICS_PRESET_COUNT;
}

/**
 * @brief 获取概率论与数理统计预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_probability_statistics_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    *out_count = PROBABILITY_STATISTICS_PRESET_COUNT;
    *out_names = (char **) lv00_malloc((size_t) PROBABILITY_STATISTICS_PRESET_COUNT * sizeof(char *));
    if (!*out_names) {
        *out_count = 0;
        return false;
    }

    /* 按头文件中定义的顺序列出所有预设名称 */
    const char *names[PROBABILITY_STATISTICS_PRESET_COUNT] = {
        /* 概率基础 */
        PRESET_PROBABILITY_SPACE_CREATE, PRESET_PROBABILITY_EVENT, PRESET_PROBABILITY_COMPLEMENT,
        PRESET_PROBABILITY_UNION, PRESET_PROBABILITY_INTERSECTION, PRESET_CONDITIONAL_PROBABILITY, PRESET_BAYES_THEOREM,
        PRESET_TOTAL_PROBABILITY,
        /* 随机变量 */
        PRESET_RANDOM_VARIABLE_EXPECTATION, PRESET_RANDOM_VARIABLE_VARIANCE, PRESET_RANDOM_VARIABLE_STD,
        PRESET_RANDOM_VARIABLE_MOMENT, PRESET_RANDOM_VARIABLE_COVARIANCE, PRESET_RANDOM_VARIABLE_CORRELATION,
        PRESET_MOMENT_GENERATING_FUNCTION, PRESET_CHARACTERISTIC_FUNCTION,
        /* 概率分布 */
        PRESET_DISTRIBUTION_NORMAL, PRESET_DISTRIBUTION_UNIFORM, PRESET_DISTRIBUTION_EXPONENTIAL,
        PRESET_DISTRIBUTION_POISSON, PRESET_DISTRIBUTION_BINOMIAL, PRESET_DISTRIBUTION_GEOMETRIC,
        PRESET_DISTRIBUTION_GAMMA, PRESET_DISTRIBUTION_BETA, PRESET_DISTRIBUTION_CHI_SQUARED,
        PRESET_DISTRIBUTION_STUDENT_T,
        /* 统计推断 */
        PRESET_MLE_ESTIMATE, PRESET_BAYESIAN_ESTIMATE, PRESET_CONFIDENCE_INTERVAL, PRESET_HYPOTHESIS_TEST_Z,
        PRESET_HYPOTHESIS_TEST_T, PRESET_HYPOTHESIS_TEST_CHI2, PRESET_KS_TEST, PRESET_REGRESSION_LINEAR, PRESET_ANOVA};

    for (int i = 0; i < PROBABILITY_STATISTICS_PRESET_COUNT; i++) {
        (*out_names)[i] = lv00_strdup(names[i]);
        if (!(*out_names)[i]) {
            /* 内存分配失败，释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                lv00_free((void **) &(*out_names)[j]);
            }
            lv00_free((void **) out_names);
            *out_names = NULL;
            *out_count = 0;
            return false;
        }
    }

    return true;
}

/**
 * @brief 获取概率论与数理统计预设的类别
 *
 * @return 预设类别（PRESET_EXT_ANALYSIS）
 */
PresetCategory preset_probability_statistics_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}
