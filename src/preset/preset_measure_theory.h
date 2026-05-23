/**
 * @file preset_measure_theory.h
 * @brief 测度论预设函数块
 *
 * 提供理论数学研究中常用的测度论基本运算与定理预设函数块。
 * 涵盖σ代数构造、测度构造、可测函数与积分、收敛定理、
 * 乘积测度、Fubini定理以及Radon-Nikodym导数。
 *
 * 包含的预设函数块：
 * - σ代数与测度基础（4个）：σ代数、Borel σ-代数、测度空间、零测集
 * - 测度构造（4个）：Lebesgue测度、计数测度、Dirac测度、外测度
 * - 可测函数与积分（4个）：可测函数判定、简单函数、Lebesgue积分、L^p空间
 * - 收敛定理（4个）：单调收敛、Fatou引理、控制收敛、几乎处处收敛
 * - 乘积测度与Fubini（2个）：乘积测度、Fubini定理
 * - Radon-Nikodym（2个）：绝对连续性、Radon-Nikodym导数
 *
 * @module MeasureTheory
 * @category PRESET_CATEGORY_ANALYSIS
 */

#ifndef LV00_PRESET_MEASURE_THEORY_H
#define LV00_PRESET_MEASURE_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== σ代数与测度基础 ==================== */

/**
 * @brief σ代数构造
 *
 * 数学定义：给定集合 $X$ 和子集族 $\mathcal{C}$，生成包含 $\mathcal{C}$ 的最小σ-代数 $\sigma(\mathcal{C})$
 * $$\sigma(\mathcal{C}) = \bigcap\{\Sigma : \mathcal{C} \subseteq \Sigma, \Sigma \text{ 为 } \sigma\text{-代数}\}$$
 *
 * 输入：
 *   - X: 集合 (PRESET_TYPE_SET) - 全集
 *   - C: 集合 (PRESET_TYPE_SET) - 子集族
 * 输出：
 *   - 集合 (PRESET_TYPE_SET) - σ-代数 $\sigma(\mathcal{C})$
 *
 * 复杂度：O(∞)（一般不可构造）
 */
#define PRESET_MT_SIGMA_ALGEBRA "mt_sigma_algebra"

/**
 * @brief Borel σ-代数
 *
 * 数学定义：给定拓扑空间 $X$，生成由所有开集生成的Borel σ-代数 $\mathcal{B}(X)$
 * $$\mathcal{B}(X) = \sigma(\{U \subseteq X : U \text{ 为开集}\})$$
 *
 * 输入：
 *   - X: 拓扑 (PRESET_TYPE_TOPOLOGY) - 拓扑空间
 * 输出：
 *   - 集合 (PRESET_TYPE_SET) - Borel σ-代数
 *
 * 复杂度：O(∞)
 */
#define PRESET_MT_BOREL_ALGEBRA "mt_borel_algebra"

/**
 * @brief 测度空间
 *
 * 数学定义：构造测度空间 $(X, \Sigma, \mu)$，其中 $\mu: \Sigma \to [0, +\infty]$ 满足
 * $\mu(\emptyset) = 0$ 且具有可数可加性
 *
 * 输入：
 *   - X: 集合 (PRESET_TYPE_SET)
 *   - sigma: 集合 (PRESET_TYPE_SET) - σ-代数
 *   - mu: 函数 (PRESET_TYPE_FUNCTION) - 测度函数
 * 输出：
 *   - 空间 (PRESET_TYPE_SPACE) - 测度空间
 *
 * 复杂度：O(1)
 */
#define PRESET_MT_MEASURE_SPACE "mt_measure_space"

/**
 * @brief 零测集判定
 *
 * 数学定义：判定集合 $N$ 是否为零测集，即 $\mu(N) = 0$
 *
 * 输入：
 *   - N: 集合 (PRESET_TYPE_SET)
 *   - mu: 函数 (PRESET_TYPE_FUNCTION) - 测度
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否为零测集
 *
 * 复杂度：O(1)
 */
#define PRESET_MT_NULL_SET "mt_null_set"

/* ==================== 测度构造 ==================== */

/**
 * @brief Lebesgue测度
 *
 * 数学定义：在 $\mathbb{R}^n$ 上构造Lebesgue测度 $m$，对矩体 $Q = \prod [a_i, b_i]$
 * $$m(Q) = \prod_{i=1}^{n} (b_i - a_i)$$
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 空间维数
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - Lebesgue测度
 *
 * 复杂度：O(1)
 */
#define PRESET_MT_LEBESGUE_MEASURE "mt_lebesgue_measure"

/**
 * @brief 计数测度
 *
 * 数学定义：对任意集合 $A$，计数测度 $\#(A)$ 为 $A$ 中元素个数（无穷集为 $+\infty$）
 *
 * 输入：
 *   - X: 集合 (PRESET_TYPE_SET)
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 计数测度
 *
 * 复杂度：O(1)
 */
#define PRESET_MT_COUNTING_MEASURE "mt_counting_measure"

/**
 * @brief Dirac测度
 *
 * 数学定义：在点 $x_0$ 处的Dirac测度 $\delta_{x_0}$：
 * $$\delta_{x_0}(A) = \begin{cases} 1 & x_0 \in A \\ 0 & x_0 \notin A \end{cases}$$
 *
 * 输入：
 *   - x0: 点 (PRESET_TYPE_POINT) - 支撑点
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - Dirac测度
 *
 * 复杂度：O(1)
 */
#define PRESET_MT_DIRAC_MEASURE "mt_dirac_measure"

/**
 * @brief 外测度
 *
 * 数学定义：计算集合 $A$ 的外测度 $\mu^*(A)$：
 * $$\mu^*(A) = \inf\left\{\sum_{i=1}^{\infty} \mu(E_i) : A \subseteq \bigcup_{i=1}^{\infty} E_i, E_i \in \Sigma\right\}$$
 *
 * 输入：
 *   - A: 集合 (PRESET_TYPE_SET)
 *   - mu: 函数 (PRESET_TYPE_FUNCTION) - 参考测度
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 外测度值
 *
 * 复杂度：O(∞)
 */
#define PRESET_MT_OUTER_MEASURE "mt_outer_measure"

/* ==================== 可测函数与积分 ==================== */

/**
 * @brief 可测函数判定
 *
 * 数学定义：判定函数 $f: X \to \mathbb{R}$ 是否为Σ-可测函数：
 * $$f \text{ 可测} \Leftrightarrow \forall B \in \mathcal{B}(\mathbb{R}), f^{-1}(B) \in \Sigma$$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION)
 *   - sigma: 集合 (PRESET_TYPE_SET) - σ-代数
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否可测
 *
 * 复杂度：O(∞)
 */
#define PRESET_MT_MEASURABLE_FUNCTION "mt_measurable_function"

/**
 * @brief 简单函数逼近
 *
 * 数学定义：用简单函数序列 $s_n$ 逼近非负可测函数 $f$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 非负可测函数
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 逼近层数
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 简单函数 $s_n$
 *
 * 复杂度：O(n·2^n)
 */
#define PRESET_MT_SIMPLE_FUNCTION "mt_simple_function"

/**
 * @brief Lebesgue积分
 *
 * 数学定义：计算可测函数 $f$ 在可测集 $A$ 上关于测度 $\mu$ 的Lebesgue积分
 * $$\int_A f\,d\mu = \int_A f^+\,d\mu - \int_A f^-\,d\mu$$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 可测函数
 *   - A: 集合 (PRESET_TYPE_SET) - 可测集
 *   - mu: 函数 (PRESET_TYPE_FUNCTION) - 测度
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 积分值
 *
 * 复杂度：O(∞)
 */
#define PRESET_MT_LEBESGUE_INTEGRAL "mt_lebesgue_integral"

/**
 * @brief L^p空间范数
 *
 * 数学定义：计算 $f$ 的 $L^p$ 范数：
 * $$\|f\|_p = \left(\int |f|^p\,d\mu\right)^{1/p}$$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 可测函数
 *   - p: 标量 (PRESET_TYPE_SCALAR) - 指数 $p \geq 1$
 *   - mu: 函数 (PRESET_TYPE_FUNCTION) - 测度
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - $L^p$ 范数
 *
 * 复杂度：O(∞)
 */
#define PRESET_MT_LP_NORM "mt_lp_norm"

/* ==================== 收敛定理 ==================== */

/**
 * @brief 单调收敛定理
 *
 * 数学定义：若 $0 \leq f_1 \leq f_2 \leq \cdots$ 且 $f_n \to f$ a.e.，则
 * $$\lim_{n\to\infty} \int f_n\,d\mu = \int f\,d\mu$$
 *
 * 输入：
 *   - f_n: 序列 (PRESET_TYPE_SEQUENCE) - 单调递增函数序列
 *   - mu: 函数 (PRESET_TYPE_FUNCTION) - 测度
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 极限积分值
 *
 * 复杂度：O(∞)
 */
#define PRESET_MT_MONOTONE_CONVERGENCE "mt_monotone_convergence"

/**
 * @brief Fatou引理
 *
 * 数学定义：对非负可测函数序列 $\{f_n\}$：
 * $$\int \liminf_{n\to\infty} f_n\,d\mu \leq \liminf_{n\to\infty} \int f_n\,d\mu$$
 *
 * 输入：
 *   - f_n: 序列 (PRESET_TYPE_SEQUENCE) - 非负可测函数序列
 *   - mu: 函数 (PRESET_TYPE_FUNCTION) - 测度
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 不等式是否成立
 *
 * 复杂度：O(∞)
 */
#define PRESET_MT_FATOU_LEMMA "mt_fatou_lemma"

/**
 * @brief 控制收敛定理
 *
 * 数学定义：若 $f_n \to f$ a.e.，且 $|f_n| \leq g \in L^1$，则
 * $$\lim_{n\to\infty} \int f_n\,d\mu = \int f\,d\mu$$
 *
 * 输入：
 *   - f_n: 序列 (PRESET_TYPE_SEQUENCE) - 函数序列
 *   - g: 函数 (PRESET_TYPE_FUNCTION) - 控制函数
 *   - mu: 函数 (PRESET_TYPE_FUNCTION) - 测度
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 极限积分值
 *
 * 复杂度：O(∞)
 */
#define PRESET_MT_DOMINATED_CONVERGENCE "mt_dominated_convergence"

/**
 * @brief 几乎处处收敛
 *
 * 数学定义：判定函数序列 $\{f_n\}$ 是否几乎处处收敛于 $f$：
 * $$\mu(\{x : \lim f_n(x) \neq f(x)\}) = 0$$
 *
 * 输入：
 *   - f_n: 序列 (PRESET_TYPE_SEQUENCE) - 函数序列
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 极限函数
 *   - mu: 函数 (PRESET_TYPE_FUNCTION) - 测度
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否几乎处处收敛
 *
 * 复杂度：O(∞)
 */
#define PRESET_MT_ALMOST_EVERYWHERE "mt_almost_everywhere"

/* ==================== 乘积测度与Fubini定理 ==================== */

/**
 * @brief 乘积测度
 *
 * 数学定义：给定两个σ-有限测度空间，构造乘积测度 $\mu \times \nu$：
 * $$(\mu \times \nu)(A \times B) = \mu(A) \cdot \nu(B)$$
 *
 * 输入：
 *   - space1: 空间 (PRESET_TYPE_SPACE) - 测度空间1
 *   - space2: 空间 (PRESET_TYPE_SPACE) - 测度空间2
 * 输出：
 *   - 空间 (PRESET_TYPE_SPACE) - 乘积测度空间
 *
 * 复杂度：O(1)
 */
#define PRESET_MT_PRODUCT_MEASURE "mt_product_measure"

/**
 * @brief Fubini定理
 *
 * 数学定义：若 $f \in L^1(\mu \times \nu)$，则可交换积分次序：
 * $$\int_{X\times Y} f\,d(\mu\times\nu) = \int_X\left(\int_Y f(x,y)\,d\nu(y)\right)d\mu(x)$$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 可测函数
 *   - mu: 函数 (PRESET_TYPE_FUNCTION) - 第一分量测度
 *   - nu: 函数 (PRESET_TYPE_FUNCTION) - 第二分量测度
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 重积分值
 *
 * 复杂度：O(∞)
 */
#define PRESET_MT_FUBINI_THEOREM "mt_fubini_theorem"

/* ==================== Radon-Nikodym导数 ==================== */

/**
 * @brief 绝对连续性
 *
 * 数学定义：判定测度 $\nu$ 是否关于 $\mu$ 绝对连续（$\nu \ll \mu$）：
 * $$\mu(A) = 0 \Rightarrow \nu(A) = 0$$
 *
 * 输入：
 *   - nu: 函数 (PRESET_TYPE_FUNCTION) - 被判定测度
 *   - mu: 函数 (PRESET_TYPE_FUNCTION) - 参考测度
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否绝对连续
 *
 * 复杂度：O(∞)
 */
#define PRESET_MT_ABSOLUTE_CONTINUITY "mt_absolute_continuity"

/**
 * @brief Radon-Nikodym导数
 *
 * 数学定义：当 $\nu \ll \mu$ 时，存在唯一的 $f = d\nu/d\mu$ 使得
 * $$\nu(A) = \int_A f\,d\mu, \quad \forall A \in \Sigma$$
 *
 * 输入：
 *   - nu: 函数 (PRESET_TYPE_FUNCTION) - 绝对连续测度
 *   - mu: 函数 (PRESET_TYPE_FUNCTION) - 参考测度
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - Radon-Nikodym导数 $d\nu/d\mu$
 *
 * 复杂度：O(∞)
 */
#define PRESET_MT_RADON_NIKODYM "mt_radon_nikodym"

/* ==================== 模块初始化 ==================== */

/**
 * @brief 注册测度论预设函数块
 *
 * 此函数由 preset_blocks_init() 自动调用，用户无需直接调用。
 *
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_measure_theory_register(void);

/**
 * @brief 获取测度论模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_measure_theory_count(void);

/**
 * @brief 获取测度论模块的预设类别
 *
 * @return 预设类别
 */
PresetCategory preset_measure_theory_category(void);

/**
 * @brief 获取测度论模块的所有预设名称
 *
 * @param out_names 输出名称数组（调用者负责释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_measure_theory_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_MEASURE_THEORY_H */
