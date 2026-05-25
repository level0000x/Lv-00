/**
 * @file preset_game_theory.h
 * @brief 博弈论预设函数块 - 常量定义
 *
 * 提供理论数学研究中常用的博弈论预设函数块。
 * 涵盖策略型博弈、合作博弈、展开型博弈及特殊博弈模型。
 *
 * 包含的预设函数块：
 * - 策略型博弈 (6个)：博弈构造、占优策略、劣策略剔除、Nash均衡、混合策略均衡、最优响应
 * - 合作博弈 (5个)：合作博弈构造、Shapley值、核心判定、Banzhaf权力指数、分配集
 * - 展开型博弈 (4个)：展开型博弈构造、逆向归纳法、子博弈精炼均衡、信息集分析
 * - 特殊博弈模型 (5个)：囚徒困境、极小化极大策略、Pareto最优、进化稳定策略、Nash讨价还价解
 *
 * @module GameTheory
 * @category PRESET_CATEGORY_OPTIMIZATION
 * @version 5.0.0
 */

#ifndef LV00_PRESET_GAME_THEORY_H
#define LV00_PRESET_GAME_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 预设名称宏常量
 * ================================================================ */

/* ==================== 策略型博弈 ==================== */

/**
 * @brief 预设：策略型博弈构造
 * @details 数学定义: $\Gamma = (N, \{S_i\}_{i \in N}, \{u_i\}_{i \in N})$，由玩家集合、策略空间和支付矩阵构造正规型博弈
 * @note 输入: PRESET_TYPE_LIST, PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_STRUCTURE | 复杂度: O(|N| \cdot |S|)
 */
#define PRESET_GT_NORMAL_FORM_CONSTRUCT "gt_normal_form_construct"

/**
 * @brief 预设：占优策略判定
 * @details 数学定义: $s_i^* \succ s_i$ 当且仅当 $u_i(s_i^*, s_{-i}) > u_i(s_i, s_{-i}),\ \forall s_{-i}$，判定是否存在严格占优策略
 * @note 输入: PRESET_TYPE_STRUCTURE | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|N| \cdot |S|^2)
 */
#define PRESET_GT_DOMINANT_STRATEGY "gt_dominant_strategy"

/**
 * @brief 预设：劣策略剔除
 * @details 数学定义: 迭代剔除所有严格劣策略 $s_i'$ 使得 $\exists s_i: u_i(s_i, s_{-i}) > u_i(s_i', s_{-i}),\ \forall s_{-i}$，直至无劣策略可剔除
 * @note 输入: PRESET_TYPE_STRUCTURE | 输出: PRESET_TYPE_STRUCTURE | 复杂度: O(|N| \cdot |S|^3)
 */
#define PRESET_GT_DOMINATED_ELIMINATION "gt_dominated_elimination"

/**
 * @brief 预设：Nash均衡计算
 * @details 数学定义: $u_i(s_i^*, s_{-i}^*) \ge u_i(s_i, s_{-i}^*),\ \forall s_i \in S_i,\ \forall i \in N$，计算所有纯策略Nash均衡
 * @note 输入: PRESET_TYPE_STRUCTURE | 输出: PRESET_TYPE_LIST | 复杂度: O(|S|^{|N|})
 */
#define PRESET_GT_NASH_EQUILIBRIUM "gt_nash_equilibrium"

/**
 * @brief 预设：混合策略Nash均衡
 * @details 数学定义: $\sigma_i^* \in \Delta(S_i)$ 使得 $U_i(\sigma_i^*, \sigma_{-i}^*) \ge U_i(\sigma_i, \sigma_{-i}^*),\ \forall \sigma_i \in \Delta(S_i)$，计算混合策略Nash均衡
 * @note 输入: PRESET_TYPE_STRUCTURE | 输出: PRESET_TYPE_LIST | 复杂度: O(|S|^{|N|}) 枚举支撑集
 */
#define PRESET_GT_NASH_MIXED "gt_nash_mixed"

/**
 * @brief 预设：最优响应
 * @details 数学定义: $BR_i(s_{-i}) = \arg\max_{s_i \in S_i} u_i(s_i, s_{-i})$，计算给定对手策略组合时的最优响应策略
 * @note 输入: PRESET_TYPE_STRUCTURE, PRESET_TYPE_TUPLE | 输出: PRESET_TYPE_LIST | 复杂度: O(|N| \cdot |S|)
 */
#define PRESET_GT_BEST_RESPONSE "gt_best_response"

/* ==================== 合作博弈 ==================== */

/**
 * @brief 预设：合作博弈构造
 * @details 数学定义: $(N, v)$，其中 $v: 2^N \to \mathbb{R}$ 为特征函数，$v(\emptyset) = 0$，由联盟特征函数构造合作博弈
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_STRUCTURE | 复杂度: O(2^{|N|})
 */
#define PRESET_GT_COALITIONAL_GAME_CONSTRUCT "gt_coalitional_game_construct"

/**
 * @brief 预设：Shapley值计算
 * @details 数学定义: $\phi_i(v) = \sum_{S \subseteq N \setminus \{i\}} \frac{|S|!(|N|-|S|-1)!}{|N|!}[v(S \cup \{i\}) - v(S)]$，计算每个玩家的Shapley值
 * @note 输入: PRESET_TYPE_STRUCTURE | 输出: PRESET_TYPE_TUPLE | 复杂度: O(|N| \cdot 2^{|N|})
 */
#define PRESET_GT_SHAPLEY_VALUE "gt_shapley_value"

/**
 * @brief 预设：核心判定
 * @details 数学定义: $\text{Core}(v) = \{x \in \mathbb{R}^N : \sum_{i \in N} x_i = v(N),\ \sum_{i \in S} x_i \ge v(S),\ \forall S \subseteq N\}$，判定分配方案是否在核心中
 * @note 输入: PRESET_TYPE_STRUCTURE, PRESET_TYPE_TUPLE | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(2^{|N|})
 */
#define PRESET_GT_CORE_CHECK "gt_core_check"

/**
 * @brief 预设：Banzhaf权力指数
 * @details 数学定义: $\beta_i = \sum_{S \subseteq N \setminus \{i\}} [v(S \cup \{i\}) - v(S)]$，归一化后 $\beta_i' = \beta_i / \sum_j \beta_j$，计算投票博弈中的Banzhaf权力指数
 * @note 输入: PRESET_TYPE_STRUCTURE | 输出: PRESET_TYPE_TUPLE | 复杂度: O(|N| \cdot 2^{|N|})
 */
#define PRESET_GT_BANZHAF_POWER "gt_banzhaf_power"

/**
 * @brief 预设：分配集计算
 * @details 数学定义: $I(v) = \{x \in \mathbb{R}^N : x_i \ge v(\{i\}),\ \sum_{i \in N} x_i = v(N)\}$，计算满足个体理性和效率性的分配集
 * @note 输入: PRESET_TYPE_STRUCTURE | 输出: PRESET_TYPE_SET | 复杂度: O(|N|)
 */
#define PRESET_GT_IMPUTATION_SET "gt_imputation_set"

/* ==================== 展开型博弈 ==================== */

/**
 * @brief 预设：展开型博弈构造
 * @details 数学定义: $\Gamma_E = (N, \mathcal{X}, \mathcal{A}, \alpha, \mathcal{H}, \rho, \sigma, u)$，由博弈树构造展开型博弈
 * @note 输入: PRESET_TYPE_TREE, PRESET_TYPE_LIST, PRESET_TYPE_LIST | 输出: PRESET_TYPE_STRUCTURE | 复杂度: O(|\mathcal{X}|)
 */
#define PRESET_GT_EXTENSIVE_FORM_CONSTRUCT "gt_extensive_form_construct"

/**
 * @brief 预设：逆向归纳法
 * @details 数学定义: 从终端节点倒推，在每个决策节点选择使当前玩家支付最大化的行动，求解子博弈精炼均衡
 * @note 输入: PRESET_TYPE_STRUCTURE | 输出: PRESET_TYPE_LIST | 复杂度: O(|\mathcal{X}| \cdot |\mathcal{A}|)
 */
#define PRESET_GT_BACKWARD_INDUCTION "gt_backward_induction"

/**
 * @brief 预设：子博弈精炼均衡判定
 * @details 数学定义: 策略组合 $s^*$ 是子博弈精炼Nash均衡当且仅当 $s^*$ 在每个子博弈中都诱导一个Nash均衡
 * @note 输入: PRESET_TYPE_STRUCTURE, PRESET_TYPE_LIST | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|\mathcal{X}|^2)
 */
#define PRESET_GT_SUBGAME_PERFECT "gt_subgame_perfect"

/**
 * @brief 预设：信息集分析
 * @details 数学定义: 分析信息集划分 $\mathcal{H}$ 的结构，判定完美信息/不完美信息、完美回忆等性质
 * @note 输入: PRESET_TYPE_STRUCTURE | 输出: PRESET_TYPE_TUPLE | 复杂度: O(|\mathcal{X}| \cdot |\mathcal{H}|)
 */
#define PRESET_GT_INFORMATION_SET "gt_information_set"

/* ==================== 特殊博弈模型 ==================== */

/**
 * @brief 预设：囚徒困境分析
 * @details 数学定义: 支付矩阵满足 $T > R > P > S$ 且 $2R > T + S$，分析囚徒困境博弈的结构、Nash均衡与社会最优
 * @note 输入: PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_TUPLE | 复杂度: O(|S|^2)
 */
#define PRESET_GT_PRISONERS_DILEMMA "gt_prisoners_dilemma"

/**
 * @brief 预设：极小化极大策略
 * @details 数学定义: $\max_{\sigma_i} \min_{\sigma_j} U_i(\sigma_i, \sigma_j) = \min_{\sigma_j} \max_{\sigma_i} U_i(\sigma_i, \sigma_j)$（minimax定理），计算零和博弈的极小化极大策略
 * @note 输入: PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX | 输出: PRESET_TYPE_TUPLE | 复杂度: O(|S_1|^3 + |S_2|^3) 线性规划
 */
#define PRESET_GT_MINIMAX "gt_minimax"

/**
 * @brief 预设：Pareto最优判定
 * @details 数学定义: 结果 $x$ 为Pareto最优当且仅当不存在 $y$ 使得 $u_i(y) \ge u_i(x),\ \forall i$ 且 $\exists j: u_j(y) > u_j(x)$
 * @note 输入: PRESET_TYPE_STRUCTURE, PRESET_TYPE_TUPLE | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|N| \cdot |S|^2)
 */
#define PRESET_GT_PARETO_OPTIMAL "gt_pareto_optimal"

/**
 * @brief 预设：进化稳定策略（ESS）
 * @details 数学定义: $s^*$ 是ESS当且仅当 (1) $u(s^*, s^*) \ge u(s, s^*),\ \forall s$；(2) 若 $u(s, s^*) = u(s^*, s^*)$ 则 $u(s^*, s) > u(s, s)$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|S|^2)
 */
#define PRESET_GT_EVOLUTIONARY_STABLE "gt_evolutionary_stable"

/**
 * @brief 预设：Nash讨价还价解
 * @details 数学定义: $(x_1^*, x_2^*) = \arg\max_{(x_1, x_2) \in F,\ x_i \ge d_i} (x_1 - d_1)(x_2 - d_2)$，计算Nash讨价还价问题的最优解
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_TUPLE | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n) 凸优化
 */
#define PRESET_GT_BARGAINING_NASH "gt_bargaining_nash"

/* ================================================================
 * 模块注册函数
 * ================================================================ */

/**
 * @brief 注册所有博弈论预设函数块
 *
 * 将博弈论模块的所有预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_game_theory_register(void);

/**
 * @brief 获取博弈论预设函数块数量
 *
 * @return int 博弈论模块预设函数块总数（20）
 */
int preset_game_theory_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_GAME_THEORY_H */
