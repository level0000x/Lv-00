/**
 * @file preset_stochastic_processes.c
 * @brief 随机过程预设函数块模块 - 实现（v2统一宏模式）
 *
 * 实现理论数学研究中常用的随机过程运算预设函数块。
 * 涵盖马尔可夫链、泊松过程、布朗运动、鞅论、随机游走。
 * 共25个预设函数块，均遵循模块化、确定性原则。
 *
 * @module StochasticProcesses
 * @category PRESET_CATEGORY_PROBABILITY
 * @version 5.0.0
 */

#include "preset_stochastic_processes.h"

#include <stdlib.h>
#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 随机过程模块预设函数块总数 */
#define STOCHASTIC_PROCESSES_PRESET_COUNT 25

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个随机过程预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有随机过程预设都属于 PRESET_CATEGORY_PROBABILITY 类别。
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义（LaTeX格式）
 * @param complexity 时间复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_stochastic_processes_preset(const char *name, const char *description,
                                                 const PresetType *input_types, int input_count, PresetType output_type,
                                                 const char *math_def, const char *complexity, bool is_constructive,
                                                 bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_PROBABILITY, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== v2统一注册宏 ==================== */

/**
 * @brief 随机过程预设统一注册宏
 *
 * 使用do-while(0)包装，确保宏展开后在语法上等价于单条语句。
 * 注册成功时递增success_count，失败时输出错误日志。
 *
 * @param name       预设名称
 * @param desc       中文描述
 * @param inputs     输入类型数组
 * @param in_count   输入数量
 * @param output     输出类型
 * @param math       数学定义（LaTeX格式字符串）
 * @param comp       时间复杂度
 * @param cons       是否构造性
 * @param rev        是否可逆
 */
#define REGISTER_SP(name, desc, inputs, in_count, output, math, comp, cons, rev)                                 \
    do {                                                                                                         \
        if (register_stochastic_processes_preset((name), (desc), (inputs), (in_count), (output), (math), (comp), \
                                                 (cons), (rev))) {                                               \
            success_count++;                                                                                     \
        } else {                                                                                                 \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */                                                  \
        }                                                                                                        \
    } while (0)

/* ==================== 模块注册实现 ==================== */

bool preset_stochastic_processes_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：马尔可夫链（8个）
     * ============================================================ */

    /**
     * @brief sp_markov_chain_construct - 马尔可夫链构造
     *
     * 由状态空间S和转移概率矩阵P构造离散时间马尔可夫链 (X_n)。
     * 转移概率矩阵P满足：P(i,j) >= 0 且对每个i，sum_j P(i,j) = 1。
     * 马尔可夫性质：P(X_{n+1}=j | X_n=i, X_{n-1}, ...) = P(i,j)。
     *
     * @param S 状态空间（PRESET_TYPE_SET）
     * @param P 转移概率矩阵（PRESET_TYPE_MATRIX）
     * @return 马尔可夫链（PRESET_TYPE_FUNCTION）
     * @math (X_n)_{n \\geq 0}, \\quad P(X_{n+1}=j \\mid X_n=i) = P_{ij}, \\quad \\sum_j P_{ij} = 1
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_MATRIX};
        REGISTER_SP("sp_markov_chain_construct",
                    "马尔可夫链构造：由状态空间S和转移概率矩阵P构造离散时间马尔可夫链 (X_n)", inputs, 2,
                    PRESET_TYPE_FUNCTION,
                    "(X_n)_{n \\geq 0}, \\quad P(X_{n+1}=j \\mid X_n=i) = P_{ij}, \\quad \\sum_j P_{ij} = 1", "O(n^2)",
                    true, false);
    }

    /**
     * @brief sp_markov_chain_transition - 转移概率计算
     *
     * 计算马尔可夫链的n步转移概率 P^n(i,j)。
     * n步转移概率矩阵等于转移概率矩阵的n次幂：P^n = P * P * ... * P。
     * Chapman-Kolmogorov方程：P^{m+n}(i,j) = sum_k P^m(i,k) * P^n(k,j)。
     *
     * @param P 转移概率矩阵（PRESET_TYPE_MATRIX）
     * @param n 步数（PRESET_TYPE_INTEGER）
     * @return n步转移概率矩阵 P^n（PRESET_TYPE_MATRIX）
     * @math P^n(i,j) = (P^n)_{ij}, \\quad P^{m+n} = P^m \\cdot P^n
     * @complexity O(n^3 \\log k)（矩阵快速幂）
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_INTEGER};
        REGISTER_SP("sp_markov_chain_transition", "转移概率计算：计算马尔可夫链的n步转移概率 P^n(i,j)", inputs, 2,
                    PRESET_TYPE_MATRIX, "P^n(i,j) = (P^n)_{ij}, \\quad P^{m+n} = P^m \\cdot P^n", "O(n^3 \\log k)",
                    true, false);
    }

    /**
     * @brief sp_markov_chain_stationary - 平稳分布计算
     *
     * 求马尔可夫链的平稳分布 π。
     * 平稳分布满足 πP = π 且 sum_i π_i = 1。
     * 对于不可约非周期有限马尔可夫链，平稳分布存在且唯一，
     * 且 π_i = lim_{n->inf} P^n(j,i) 与初始状态j无关。
     *
     * @param P 转移概率矩阵（PRESET_TYPE_MATRIX）
     * @return 平稳分布 π（PRESET_TYPE_DISTRIBUTION）
     * @math \\pi P = \\pi, \\quad \\sum_i \\pi_i = 1, \\quad \\pi_i \\geq 0
     * @complexity O(n^3)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        REGISTER_SP("sp_markov_chain_stationary", "平稳分布计算：求马尔可夫链的平稳分布 π，满足 πP = π", inputs, 1,
                    PRESET_TYPE_DISTRIBUTION, "\\pi P = \\pi, \\quad \\sum_i \\pi_i = 1, \\quad \\pi_i \\geq 0",
                    "O(n^3)", true, false);
    }

    /**
     * @brief sp_markov_chain_irreducible - 不可约性判定
     *
     * 判定马尔可夫链是否不可约。
     * 不可约性条件：从任意状态i出发，可以到达任意状态j，
     * 即对所有i, j，存在n >= 0 使得 P^n(i,j) > 0。
     * 等价于状态转移图是强连通的。
     *
     * @param P 转移概率矩阵（PRESET_TYPE_MATRIX）
     * @return 是否不可约（PRESET_TYPE_BOOLEAN）
     * @math \\text{不可约} \\Leftrightarrow \\forall i, j, \\exists n \\geq 0: P^n(i,j) > 0
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        REGISTER_SP("sp_markov_chain_irreducible", "不可约性判定：判定马尔可夫链是否不可约（任意两状态可达）", inputs,
                    1, PRESET_TYPE_BOOLEAN,
                    "\\text{不可约} \\Leftrightarrow \\forall i, j, \\exists n \\geq 0: P^n(i,j) > 0", "O(n^2)", false,
                    false);
    }

    /**
     * @brief sp_markov_chain_aperiodic - 非周期性判定
     *
     * 判定马尔可夫链是否非周期。
     * 状态i的周期定义为 d(i) = gcd{n >= 1 : P^n(i,i) > 0}。
     * 若d(i) = 1，则状态i是非周期的。
     * 对于不可约马尔可夫链，所有状态具有相同的周期。
     *
     * @param P 转移概率矩阵（PRESET_TYPE_MATRIX）
     * @return 是否非周期（PRESET_TYPE_BOOLEAN）
     * @math d(i) = \\gcd\\{n \\geq 1 : P^n(i,i) > 0\\}, \\quad \\text{非周期} \\Leftrightarrow d(i) = 1
     * @complexity O(n^3)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        REGISTER_SP("sp_markov_chain_aperiodic", "非周期性判定：判定马尔可夫链是否非周期（状态周期 d(i) = 1）", inputs,
                    1, PRESET_TYPE_BOOLEAN,
                    "d(i) = \\gcd\\{n \\geq 1 : P^n(i,i) > 0\\}, \\quad \\text{非周期} \\Leftrightarrow d(i) = 1",
                    "O(n^3)", false, false);
    }

    /**
     * @brief sp_markov_chain_recurrent - 常返性判定
     *
     * 判定马尔可夫链的状态i是否常返。
     * 状态i是常返的当且仅当从i出发，几乎必然回到i。
     * 等价条件：sum_{n=1}^{inf} P^n(i,i) = inf（常返）或 < inf（非常返）。
     * 常返状态分为正常返（期望返回时间有限）和零常返（期望返回时间无限）。
     *
     * @param P 转移概率矩阵（PRESET_TYPE_MATRIX）
     * @param i 状态索引（PRESET_TYPE_INTEGER）
     * @return 是否常返（PRESET_TYPE_BOOLEAN）
     * @math f_{ii} = P(\\tau_i < \\infty \\mid X_0 = i) = 1 \\text{（常返）}
     * @complexity O(n^3)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_INTEGER};
        REGISTER_SP("sp_markov_chain_recurrent", "常返性判定：判定马尔可夫链的状态i是否常返（从i出发几乎必然回到i）",
                    inputs, 2, PRESET_TYPE_BOOLEAN, "f_{ii} = P(\\tau_i < \\infty \\mid X_0 = i) = 1 \\text{（常返）}",
                    "O(n^3)", false, false);
    }

    /**
     * @brief sp_markov_chain_absorbing - 吸收性判定
     *
     * 判定马尔可夫链是否有吸收状态。
     * 状态i是吸收状态当且仅当 P(i,i) = 1。
     * 对于有吸收状态的马尔可夫链，最终几乎必然被某个吸收状态吸收
     * （当从任意非吸收状态出发可以到达某个吸收状态时）。
     *
     * @param P 转移概率矩阵（PRESET_TYPE_MATRIX）
     * @return 是否有吸收状态（PRESET_TYPE_BOOLEAN）
     * @math \\text{吸收状态} \\Leftrightarrow P(i,i) = 1
     * @complexity O(n)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        REGISTER_SP("sp_markov_chain_absorbing", "吸收性判定：判定马尔可夫链是否有吸收状态（P(i,i) = 1）", inputs, 1,
                    PRESET_TYPE_BOOLEAN, "\\text{吸收状态} \\Leftrightarrow P(i,i) = 1", "O(n)", false, false);
    }

    /**
     * @brief sp_markov_chain_expected_time - 期望到达时间
     *
     * 计算从状态i首次到达状态j的期望步数 E_i[τ_j]。
     * 其中 τ_j = min{n >= 1 : X_n = j} 是首次到达j的时间。
     * 期望到达时间满足方程组：m_i = 1 + sum_{k != j} P(i,k) * m_k，m_j = 0。
     * 若i和j属于同一常返类，则 E_i[τ_j] < inf。
     *
     * @param P 转移概率矩阵（PRESET_TYPE_MATRIX）
     * @param i 起始状态（PRESET_TYPE_INTEGER）
     * @param j 目标状态（PRESET_TYPE_INTEGER）
     * @return 期望到达步数（PRESET_TYPE_SCALAR）
     * @math m_i = E_i[\\tau_j] = 1 + \\sum_{k \\neq j} P(i,k) \\cdot m_k, \\quad m_j = 0
     * @complexity O(n^3)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        REGISTER_SP("sp_markov_chain_expected_time", "期望到达时间：计算从状态i首次到达状态j的期望步数 E_i[τ_j]",
                    inputs, 3, PRESET_TYPE_SCALAR,
                    "m_i = E_i[\\tau_j] = 1 + \\sum_{k \\neq j} P(i,k) \\cdot m_k, \\quad m_j = 0", "O(n^3)", true,
                    false);
    }

    /* ============================================================
     * 第二部分：泊松过程（5个）
     * ============================================================ */

    /**
     * @brief sp_poisson_process_construct - 泊松过程构造
     *
     * 由强度参数λ构造齐次泊松过程 {N(t), t >= 0}。
     * 泊松过程满足：N(0) = 0，独立增量，且 N(t+s) - N(s) ~ Poisson(λt)。
     * 等价定义：事件间隔 T_k = S_k - S_{k-1} 独立同分布，T_k ~ Exp(λ)。
     *
     * @param lambda 强度参数（PRESET_TYPE_SCALAR）
     * @return 泊松过程（PRESET_TYPE_FUNCTION）
     * @math \\{N(t), t \\geq 0\\}, \\quad N(0) = 0, \\quad N(t+s) - N(s) \\sim \\text{Poisson}(\\lambda t)
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        REGISTER_SP("sp_poisson_process_construct", "泊松过程构造：由强度λ构造齐次泊松过程 {N(t), t >= 0}", inputs, 1,
                    PRESET_TYPE_FUNCTION,
                    "\\{N(t), t \\geq 0\\}, \\quad N(0) = 0, \\quad N(t+s) - N(s) \\sim \\text{Poisson}(\\lambda t)",
                    "O(1)", true, false);
    }

    /**
     * @brief sp_poisson_process_counting - 计数分布
     *
     * 计算泊松过程在时间t内恰好发生k个事件的概率。
     * P(N(t) = k) = (λt)^k * e^(-λt) / k!
     * 其中k = 0, 1, 2, ...，λ > 0为强度参数，t > 0为时间。
     * 均值和方差均为 E[N(t)] = Var[N(t)] = λt。
     *
     * @param lambda 强度参数（PRESET_TYPE_SCALAR）
     * @param t 时间（PRESET_TYPE_SCALAR）
     * @param k 事件数（PRESET_TYPE_INTEGER）
     * @return 概率 P(N(t)=k)（PRESET_TYPE_PROBABILITY）
     * @math P(N(t) = k) = \\frac{(\\lambda t)^k e^{-\\lambda t}}{k!}, \\quad k = 0, 1, 2, \\ldots
     * @complexity O(k)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        REGISTER_SP("sp_poisson_process_counting", "计数分布：计算泊松过程在时间t内恰好发生k个事件的概率 P(N(t)=k)",
                    inputs, 3, PRESET_TYPE_PROBABILITY,
                    "P(N(t) = k) = \\frac{(\\lambda t)^k e^{-\\lambda t}}{k!}, \\quad k = 0, 1, 2, \\ldots", "O(k)",
                    true, false);
    }

    /**
     * @brief sp_poisson_process_waiting - 等待时间分布
     *
     * 计算第k个事件的等待时间 S_k = T_1 + T_2 + ... + T_k 的分布。
     * 等待时间服从Gamma分布：S_k ~ Gamma(k, λ)。
     * 概率密度函数：f_{S_k}(t) = λ^k * t^{k-1} * e^{-λt} / (k-1)!，t > 0。
     * 特别地，S_1 ~ Exp(λ) 为指数分布。
     *
     * @param k 事件序号（PRESET_TYPE_INTEGER）
     * @param lambda 强度参数（PRESET_TYPE_SCALAR）
     * @param t 时间（PRESET_TYPE_SCALAR）
     * @return 等待时间概率密度 f_{S_k}(t)（PRESET_TYPE_PROBABILITY）
     * @math S_k \\sim \\text{Gamma}(k, \\lambda), \\quad f_{S_k}(t) = \\frac{\\lambda^k t^{k-1} e^{-\\lambda t}}{(k-1)!}
     * @complexity O(k)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_SP("sp_poisson_process_waiting", "等待时间分布：第k个事件的等待时间 S_k ~ Gamma(k, λ)", inputs, 3,
                    PRESET_TYPE_PROBABILITY,
                    "S_k \\sim \\text{Gamma}(k, \\lambda), \\quad f_{S_k}(t) = \\frac{\\lambda^k t^{k-1} e^{-\\lambda "
                    "t}}{(k-1)!}",
                    "O(k)", true, false);
    }

    /**
     * @brief sp_poisson_process_thinning - 泊松过程稀疏化
     *
     * 以概率p独立删除泊松过程中的每个事件，得到新的泊松过程。
     * 保留的事件构成强度为 λp 的泊松过程。
     * 删除的事件构成强度为 λ(1-p) 的泊松过程。
     * 两个新过程相互独立。这是泊松过程的重要性质。
     *
     * @param N 原泊松过程（PRESET_TYPE_FUNCTION）
     * @param p 保留概率（PRESET_TYPE_PROBABILITY）
     * @return 稀疏化后的泊松过程（PRESET_TYPE_FUNCTION）
     * @math N_1(t) \\sim \\text{PP}(\\lambda p), \\quad N_2(t) \\sim \\text{PP}(\\lambda(1-p)), \\quad N_1 \\perp N_2
     * @complexity O(n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_PROBABILITY};
        REGISTER_SP(
            "sp_poisson_process_thinning", "泊松过程稀疏化：以概率p独立删除事件，保留的事件构成强度为λp的泊松过程",
            inputs, 2, PRESET_TYPE_FUNCTION,
            "N_1(t) \\sim \\text{PP}(\\lambda p), \\quad N_2(t) \\sim \\text{PP}(\\lambda(1-p)), \\quad N_1 \\perp N_2",
            "O(n)", true, false);
    }

    /**
     * @brief sp_poisson_process_superposition - 泊松过程叠加
     *
     * 将多个独立的泊松过程叠加，得到新的泊松过程。
     * 若 N_1 ~ PP(λ_1), N_2 ~ PP(λ_2), ..., N_k ~ PP(λ_k) 相互独立，
     * 则 N_1 + N_2 + ... + N_k ~ PP(λ_1 + λ_2 + ... + λ_k)。
     * 这是泊松过程的叠加性质（超级泊松过程）。
     *
     * @param processes 泊松过程列表（PRESET_TYPE_LIST）
     * @return 叠加后的泊松过程（PRESET_TYPE_FUNCTION）
     * @math \\sum_{i=1}^{k} N_i(t) \\sim \\text{PP}\\left(\\sum_{i=1}^{k} \\lambda_i\\right)
     * @complexity O(k)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        REGISTER_SP("sp_poisson_process_superposition", "泊松过程叠加：独立泊松过程的和仍为泊松过程，强度为各强度之和",
                    inputs, 1, PRESET_TYPE_FUNCTION,
                    "\\sum_{i=1}^{k} N_i(t) \\sim \\text{PP}\\left(\\sum_{i=1}^{k} \\lambda_i\\right)", "O(k)", true,
                    false);
    }

    /* ============================================================
     * 第三部分：布朗运动（5个）
     * ============================================================ */

    /**
     * @brief sp_brownian_motion_construct - 布朗运动构造
     *
     * 构造标准布朗运动（维纳过程）{W(t), t >= 0}。
     * 标准布朗运动的性质：W(0) = 0，几乎必然连续的轨道，
     * 独立增量，且 W(t) - W(s) ~ N(0, t-s)。
     * 布朗运动是连续时间连续状态的随机过程，是随机分析的基础。
     *
     * @param T 时间范围（PRESET_TYPE_SCALAR）
     * @return 布朗运动（PRESET_TYPE_FUNCTION）
     * @math W(0) = 0, \\quad W(t) - W(s) \\sim N(0, t-s), \\quad t > s \\geq 0
     * @complexity O(n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        REGISTER_SP("sp_brownian_motion_construct", "布朗运动构造：构造标准布朗运动（维纳过程）{W(t), t >= 0}", inputs,
                    1, PRESET_TYPE_FUNCTION, "W(0) = 0, \\quad W(t) - W(s) \\sim N(0, t-s), \\quad t > s \\geq 0",
                    "O(n)", true, false);
    }

    /**
     * @brief sp_brownian_motion_increment - 增量分布
     *
     * 计算布朗运动在区间[s, t]上的增量分布。
     * W(t) - W(s) ~ N(0, t-s)，即均值为0、方差为 t-s 的正态分布。
     * 布朗运动的增量具有独立性和平稳性。
     *
     * @param s 起始时间（PRESET_TYPE_SCALAR）
     * @param t 终止时间（PRESET_TYPE_SCALAR）
     * @return 增量分布 N(0, t-s)（PRESET_TYPE_DISTRIBUTION）
     * @math W(t) - W(s) \\sim N(0, t-s), \\quad E[W(t)-W(s)] = 0, \\quad \\text{Var}[W(t)-W(s)] = t-s
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_SP("sp_brownian_motion_increment", "增量分布：计算布朗运动增量 W(t)-W(s) ~ N(0, t-s) 的分布", inputs,
                    2, PRESET_TYPE_DISTRIBUTION,
                    "W(t) - W(s) \\sim N(0, t-s), \\quad E[W(t)-W(s)] = 0, \\quad \\text{Var}[W(t)-W(s)] = t-s", "O(1)",
                    true, false);
    }

    /**
     * @brief sp_brownian_motion_reflection - 反射原理
     *
     * 布朗运动的反射原理：对于 a > 0，
     * P(max_{0<=s<=t} W(s) >= a) = 2 * P(W(t) >= a) = 2 * (1 - Φ(a/sqrt(t)))。
     * 反射原理是布朗运动最重要的性质之一，用于推导首达时间分布和
     * 各种极值概率。
     *
     * @param a 阈值（PRESET_TYPE_SCALAR）
     * @param t 时间（PRESET_TYPE_SCALAR）
     * @return 概率 P(max W(s) >= a)（PRESET_TYPE_PROBABILITY）
     * @math P\\left(\\max_{0 \\leq s \\leq t} W(s) \\geq a\\right) = 2 \\cdot P(W(t) \\geq a) = 2\\left(1 - \\Phi\\left(\\frac{a}{\\sqrt{t}}\\right)\\right)
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_SP("sp_brownian_motion_reflection", "反射原理：计算 P(max_{0<=s<=t} W(s) >= a) = 2P(W(t) >= a)",
                    inputs, 2, PRESET_TYPE_PROBABILITY,
                    "P\\left(\\max_{0 \\leq s \\leq t} W(s) \\geq a\\right) = 2 \\cdot P(W(t) \\geq a) = 2\\left(1 - "
                    "\\Phi\\left(\\frac{a}{\\sqrt{t}}\\right)\\right)",
                    "O(1)", true, false);
    }

    /**
     * @brief sp_brownian_motion_hitting - 首达时间
     *
     * 计算布朗运动首次到达水平线 a 的首达时间 τ_a = inf{t > 0 : W(t) = a}。
     * 对于 a > 0，τ_a 服从逆高斯（Levy）分布：
     * P(τ_a <= t) = 2 * (1 - Φ(a/sqrt(t)))。
     * τ_a 几乎必然有限，但 E[τ_a] = inf（期望为无穷）。
     *
     * @param a 水平线（PRESET_TYPE_SCALAR）
     * @param t 时间（PRESET_TYPE_SCALAR）
     * @return 概率 P(τ_a <= t)（PRESET_TYPE_PROBABILITY）
     * @math \\tau_a = \\inf\\{t > 0 : W(t) = a\\}, \\quad P(\\tau_a \\leq t) = 2\\left(1 - \\Phi\\left(\\frac{a}{\\sqrt{t}}\\right)\\right)
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_SP("sp_brownian_motion_hitting", "首达时间：计算布朗运动首次到达水平线a的时间 τ_a 的分布", inputs, 2,
                    PRESET_TYPE_PROBABILITY,
                    "\\tau_a = \\inf\\{t > 0 : W(t) = a\\}, \\quad P(\\tau_a \\leq t) = 2\\left(1 - "
                    "\\Phi\\left(\\frac{a}{\\sqrt{t}}\\right)\\right)",
                    "O(1)", true, false);
    }

    /**
     * @brief sp_brownian_motion_bridge - 布朗桥
     *
     * 构造布朗桥 W_0(t) = W(t) - t*W(1)，t in [0, 1]。
     * 布朗桥是在条件 W(1) = 0 下的布朗运动。
     * 布朗桥的性质：W_0(0) = W_0(1) = 0，是高斯过程，
     * 均值函数为0，协方差函数为 Cov(W_0(s), W_0(t)) = min(s,t) - st。
     *
     * @param W 布朗运动（PRESET_TYPE_FUNCTION）
     * @return 布朗桥（PRESET_TYPE_FUNCTION）
     * @math W_0(t) = W(t) - t \\cdot W(1), \\quad t \\in [0, 1], \\quad \\text{Cov}(W_0(s), W_0(t)) = \\min(s,t) - st
     * @complexity O(n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        REGISTER_SP(
            "sp_brownian_motion_bridge", "布朗桥：构造布朗桥 W_0(t) = W(t) - tW(1)，满足 W_0(0) = W_0(1) = 0", inputs,
            1, PRESET_TYPE_FUNCTION,
            "W_0(t) = W(t) - t \\cdot W(1), \\quad t \\in [0, 1], \\quad \\text{Cov}(W_0(s), W_0(t)) = \\min(s,t) - st",
            "O(n)", true, false);
    }

    /* ============================================================
     * 第四部分：鞅论（4个）
     * ============================================================ */

    /**
     * @brief sp_martingale_check - 鞅判定
     *
     * 判定随机过程 {X_n} 是否为鞅（关于 filtration F_n）。
     * 鞅的条件：E[|X_n|] < inf 且 E[X_{n+1} | F_n] = X_n。
     * 直观含义：在已知过去信息的条件下，下一期的期望值等于当前值。
     * 若 E[X_{n+1} | F_n] >= X_n 则为下鞅，若 <= X_n 则为上鞅。
     *
     * @param X 随机过程（PRESET_TYPE_FUNCTION）
     * @param F 滤波（信息流）（PRESET_TYPE_FUNCTION）
     * @return 是否为鞅（PRESET_TYPE_BOOLEAN）
     * @math E[X_{n+1} \\mid \\mathcal{F}_n] = X_n \\quad \\text{a.s.}, \\quad E[|X_n|] < \\infty
     * @complexity O(n)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        REGISTER_SP("sp_martingale_check", "鞅判定：判定随机过程 {X_n} 是否为鞅（E[X_{n+1}|F_n] = X_n）", inputs, 2,
                    PRESET_TYPE_BOOLEAN,
                    "E[X_{n+1} \\mid \\mathcal{F}_n] = X_n \\quad \\text{a.s.}, \\quad E[|X_n|] < \\infty", "O(n)",
                    false, false);
    }

    /**
     * @brief sp_martingale_stopping - 停时定理（可选停时定理）
     *
     * 验证可选停时定理的条件并给出结论。
     * 若 {X_n} 是鞅，τ 是停时，且满足以下条件之一：
     * (1) τ 几乎必然有界；(2) X 是一致可积鞅；
     * 则 E[X_τ] = E[X_0]。
     * 停时τ满足 {τ <= n} in F_n 对所有n。
     *
     * @param X 鞅（PRESET_TYPE_FUNCTION）
     * @param tau 停时（PRESET_TYPE_FUNCTION）
     * @return E[X_τ] = E[X_0] 是否成立（PRESET_TYPE_BOOLEAN）
     * @math E[X_\\tau] = E[X_0], \\quad \\text{其中 } \\tau \\text{ 是停时，满足有界或一致可积条件}
     * @complexity O(n)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        REGISTER_SP("sp_martingale_stopping", "停时定理（可选停时定理）：验证 E[X_τ] = E[X_0] 的条件并给出结论", inputs,
                    2, PRESET_TYPE_BOOLEAN,
                    "E[X_\\tau] = E[X_0], \\quad \\text{其中 } \\tau \\text{ 是停时，满足有界或一致可积条件}", "O(n)",
                    false, false);
    }

    /**
     * @brief sp_martingale_convergence - 鞅收敛定理
     *
     * 鞅收敛定理：若 {X_n} 是 L^2 有界鞅（即 sup_n E[X_n^2] < inf），
     * 则 X_n 几乎必然且 L^2 收敛到某个随机变量 X_inf。
     * 更一般地，下鞅若一致可积也几乎必然收敛。
     * 这是概率论中最深刻的定理之一。
     *
     * @param X 鞅（PRESET_TYPE_FUNCTION）
     * @return 是否满足收敛条件（PRESET_TYPE_BOOLEAN）
     * @math \\sup_n E[X_n^2] < \\infty \\Rightarrow X_n \\xrightarrow{a.s.} X_\\infty, \\quad E[X_n^2] \\to E[X_\\infty^2]
     * @complexity O(n)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        REGISTER_SP("sp_martingale_convergence", "鞅收敛定理：验证L^2有界鞅几乎必然且L^2收敛的条件", inputs, 1,
                    PRESET_TYPE_BOOLEAN,
                    "\\sup_n E[X_n^2] < \\infty \\Rightarrow X_n \\xrightarrow{a.s.} X_\\infty, \\quad E[X_n^2] \\to "
                    "E[X_\\infty^2]",
                    "O(n)", false, false);
    }

    /**
     * @brief sp_martingale_decomposition - Doob分解
     *
     * 将下鞅 {X_n} 分解为鞅部分和可料部分：X_n = M_n + A_n。
     * 其中 {M_n} 是鞅（M_0 = 0），{A_n} 是可料增过程（A_0 = 0，A_n <= A_{n+1}）。
     * Doob分解是唯一的：A_n = sum_{k=1}^{n} E[X_k - X_{k-1} | F_{k-1}]。
     *
     * @param X 下鞅（PRESET_TYPE_FUNCTION）
     * @return Doob分解 {M_n, A_n}（PRESET_TYPE_TUPLE）
     * @math X_n = M_n + A_n, \\quad M_0 = 0, \\quad A_0 = 0, \\quad A_n \\text{ 可料且递增}
     * @complexity O(n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        REGISTER_SP("sp_martingale_decomposition", "Doob分解：将下鞅 X_n 分解为鞅部分 M_n 和可料增部分 A_n", inputs, 1,
                    PRESET_TYPE_TUPLE,
                    "X_n = M_n + A_n, \\quad M_0 = 0, \\quad A_0 = 0, \\quad A_n \\text{ 可料且递增}", "O(n)", true,
                    false);
    }

    /* ============================================================
     * 第五部分：随机游走（3个）
     * ============================================================ */

    /**
     * @brief sp_random_walk_construct - 随机游走构造
     *
     * 由步长分布构造随机游走 S_n = X_1 + X_2 + ... + X_n。
     * 其中 {X_i} 是独立同分布的随机变量，X_i 的分布由用户指定。
     * 常见特例：对称随机游走（P(X_i=1) = P(X_i=-1) = 1/2），
     * 一般随机游走（P(X_i=1) = p, P(X_i=-1) = q = 1-p）。
     *
     * @param dist 步长分布（PRESET_TYPE_DISTRIBUTION）
     * @param n 步数（PRESET_TYPE_INTEGER）
     * @return 随机游走（PRESET_TYPE_FUNCTION）
     * @math S_n = \\sum_{i=1}^{n} X_i, \\quad X_i \\text{ i.i.d.}, \\quad S_0 = 0
     * @complexity O(n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_INTEGER};
        REGISTER_SP("sp_random_walk_construct", "随机游走构造：由步长分布构造随机游走 S_n = X_1 + X_2 + ... + X_n",
                    inputs, 2, PRESET_TYPE_FUNCTION,
                    "S_n = \\sum_{i=1}^{n} X_i, \\quad X_i \\text{ i.i.d.}, \\quad S_0 = 0", "O(n)", true, false);
    }

    /**
     * @brief sp_random_walk_return - 回归概率
     *
     * 计算对称随机游走（一维）回归原点的概率。
     * 对称随机游走在 Z 上是常返的：回归概率 f = 1。
     * 具体地，P(存在 n >= 1 使得 S_n = 0 | S_0 = 0) = 1。
     * 但期望回归时间 E[τ_0] = inf（无穷）。
     * 在 d >= 3 维中，随机游走是非常返的（Polya定理）。
     *
     * @param d 维度（PRESET_TYPE_INTEGER）
     * @return 回归概率（PRESET_TYPE_PROBABILITY）
     * @math f_d = P(\\exists n \\geq 1: S_n = 0 \\mid S_0 = 0) = \\begin{cases} 1 & d = 1, 2 \\\\ < 1 & d \\geq 3 \\end{cases}
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        REGISTER_SP("sp_random_walk_return", "回归概率：计算d维对称随机游走回归原点的概率（Polya定理）", inputs, 1,
                    PRESET_TYPE_PROBABILITY,
                    "f_d = P(\\exists n \\geq 1: S_n = 0 \\mid S_0 = 0) = \\begin{cases} 1 & d = 1, 2 \\\\ < 1 & d "
                    "\\geq 3 \\end{cases}",
                    "O(1)", true, false);
    }

    /**
     * @brief sp_random_walk_gambler_ruin - 赌徒破产问题
     *
     * 计算赌徒破产概率。
     * 赌徒初始资本为a，目标资本为N，每步以概率p赢1、概率q=1-p输1。
     * 破产概率：若 p != q，P(破产) = ( (q/p)^a - (q/p)^N ) / ( 1 - (q/p)^N )；
     * 若 p = q = 1/2，P(破产) = 1 - a/N。
     *
     * @param a 初始资本（PRESET_TYPE_INTEGER）
     * @param N 目标资本（PRESET_TYPE_INTEGER）
     * @param p 赢的概率（PRESET_TYPE_PROBABILITY）
     * @return 破产概率（PRESET_TYPE_PROBABILITY）
     * @math P(\\text{破产}) = \\begin{cases} \\frac{(q/p)^a - (q/p)^N}{1 - (q/p)^N} & p \\neq q \\\\ 1 - \\frac{a}{N} & p = q = \\frac{1}{2} \\end{cases}
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_PROBABILITY};
        REGISTER_SP("sp_random_walk_gambler_ruin", "赌徒破产问题：计算初始资本a、目标N、赢概率p下的破产概率", inputs, 3,
                    PRESET_TYPE_PROBABILITY,
                    "P(\\text{破产}) = \\begin{cases} \\frac{(q/p)^a - (q/p)^N}{1 - (q/p)^N} & p \\neq q \\\\ 1 - "
                    "\\frac{a}{N} & p = q = \\frac{1}{2} \\end{cases}",
                    "O(1)", true, false);
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == STOCHASTIC_PROCESSES_PRESET_COUNT;
}

/**
 * @brief 获取随机过程预设函数块数量
 *
 * @return int 随机过程模块预设函数块总数
 */
int preset_stochastic_processes_count(void) {
    return STOCHASTIC_PROCESSES_PRESET_COUNT;
}

/**
 * @brief 获取随机过程预设函数块名称列表
 *
 * @param out_names 输出名称数组指针（调用者负责释放）
 * @param out_count 输出预设数量
 * @return true 获取成功
 * @return false 获取失败
 */
bool preset_stochastic_processes_get_names(char ***out_names, int *out_count) {
    if (out_names == NULL || out_count == NULL) {
        return false;
    }

    *out_count = STOCHASTIC_PROCESSES_PRESET_COUNT;

    /* 分配名称数组（使用项目统一的内存管理函数） */
    char **names = (char **) lv00_malloc(STOCHASTIC_PROCESSES_PRESET_COUNT * sizeof(char *));
    if (names == NULL) {
        return false;
    }

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 马尔可夫链 */
        "sp_markov_chain_construct", "sp_markov_chain_transition", "sp_markov_chain_stationary",
        "sp_markov_chain_irreducible", "sp_markov_chain_aperiodic", "sp_markov_chain_recurrent",
        "sp_markov_chain_absorbing", "sp_markov_chain_expected_time",
        /* 泊松过程 */
        "sp_poisson_process_construct", "sp_poisson_process_counting", "sp_poisson_process_waiting",
        "sp_poisson_process_thinning", "sp_poisson_process_superposition",
        /* 布朗运动 */
        "sp_brownian_motion_construct", "sp_brownian_motion_increment", "sp_brownian_motion_reflection",
        "sp_brownian_motion_hitting", "sp_brownian_motion_bridge",
        /* 鞅论 */
        "sp_martingale_check", "sp_martingale_stopping", "sp_martingale_convergence", "sp_martingale_decomposition",
        /* 随机游走 */
        "sp_random_walk_construct", "sp_random_walk_return", "sp_random_walk_gambler_ruin"};

    for (int i = 0; i < STOCHASTIC_PROCESSES_PRESET_COUNT; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 分配失败时释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv00_free(&tmp);
            }
            lv00_free((void **) &names);
            return false;
        }
    }

    *out_names = names;
    return true;
}

/**
 * @brief 获取随机过程模块类别名称
 *
 * @return 类别名称字符串
 */
const char *preset_stochastic_processes_category(void) {
    return "随机过程";
}
