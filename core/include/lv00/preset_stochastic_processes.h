/**
 * @file preset_stochastic_processes.h
 * @brief 随机过程预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的随机过程运算预设函数块，包括：
 *   - 马尔可夫链：构造、转移概率、平稳分布、不可约性、非周期性、常返性、吸收性、期望到达时间
 *   - 泊松过程：构造、计数分布、等待时间分布、稀疏化、叠加
 *   - 布朗运动：构造、增量分布、反射原理、首达时间、布朗桥
 *   - 鞅论：鞅判定、停时定理、鞅收敛定理、Doob分解
 *   - 随机游走：构造、回归概率、赌徒破产问题
 *
 * @module StochasticProcesses
 * @category PRESET_CATEGORY_PROBABILITY
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_STOCHASTIC_PROCESSES_H
#define LV00_PRESET_STOCHASTIC_PROCESSES_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 马尔可夫链（8个） -------------------- */

/** 马尔可夫链构造：由状态空间和转移概率矩阵构造离散时间马尔可夫链 */
#define PRESET_SP_MARKOV_CHAIN_CONSTRUCT "sp_markov_chain_construct"

/** 转移概率计算：计算n步转移概率 P^n(i,j) */
#define PRESET_SP_MARKOV_CHAIN_TRANSITION "sp_markov_chain_transition"

/** 平稳分布计算：求马尔可夫链的平稳分布 π */
#define PRESET_SP_MARKOV_CHAIN_STATIONARY "sp_markov_chain_stationary"

/** 不可约性判定：判定马尔可夫链是否不可约 */
#define PRESET_SP_MARKOV_CHAIN_IRREDUCIBLE "sp_markov_chain_irreducible"

/** 非周期性判定：判定马尔可夫链是否非周期 */
#define PRESET_SP_MARKOV_CHAIN_APERIODIC "sp_markov_chain_aperiodic"

/** 常返性判定：判定状态是否常返 */
#define PRESET_SP_MARKOV_CHAIN_RECURRENT "sp_markov_chain_recurrent"

/** 吸收性判定：判定马尔可夫链是否有吸收状态 */
#define PRESET_SP_MARKOV_CHAIN_ABSORBING "sp_markov_chain_absorbing"

/** 期望到达时间：计算首次到达某状态的期望步数 */
#define PRESET_SP_MARKOV_CHAIN_EXPECTED_TIME "sp_markov_chain_expected_time"

/* -------------------- 泊松过程（5个） -------------------- */

/** 泊松过程构造：由强度λ构造齐次泊松过程 */
#define PRESET_SP_POISSON_PROCESS_CONSTRUCT "sp_poisson_process_construct"

/** 计数分布：P(N(t)=k) = (λt)^k e^(-λt) / k! */
#define PRESET_SP_POISSON_PROCESS_COUNTING "sp_poisson_process_counting"

/** 等待时间分布：第k个事件的等待时间服从Gamma(k, λ) */
#define PRESET_SP_POISSON_PROCESS_WAITING "sp_poisson_process_waiting"

/** 泊松过程稀疏化：以概率p独立删除事件 */
#define PRESET_SP_POISSON_PROCESS_THINNING "sp_poisson_process_thinning"

/** 泊松过程叠加：独立泊松过程的和 */
#define PRESET_SP_POISSON_PROCESS_SUPERPOSITION "sp_poisson_process_superposition"

/* -------------------- 布朗运动（5个） -------------------- */

/** 布朗运动构造：构造标准布朗运动 W(t) */
#define PRESET_SP_BROWNIAN_MOTION_CONSTRUCT "sp_brownian_motion_construct"

/** 增量分布：W(t)-W(s) ~ N(0, t-s) */
#define PRESET_SP_BROWNIAN_MOTION_INCREMENT "sp_brownian_motion_increment"

/** 反射原理：P(max W(s) >= a) */
#define PRESET_SP_BROWNIAN_MOTION_REFLECTION "sp_brownian_motion_reflection"

/** 首达时间：τ_a = inf{t : W(t) = a} */
#define PRESET_SP_BROWNIAN_MOTION_HITTING "sp_brownian_motion_hitting"

/** 布朗桥：W_0(t) = W(t) - tW(1) */
#define PRESET_SP_BROWNIAN_MOTION_BRIDGE "sp_brownian_motion_bridge"

/* -------------------- 鞅论（4个） -------------------- */

/** 鞅判定：判定随机过程是否为鞅 */
#define PRESET_SP_MARTINGALE_CHECK "sp_martingale_check"

/** 停时定理（可选停时定理）：E[X_τ] = E[X_0] */
#define PRESET_SP_MARTINGALE_STOPPING "sp_martingale_stopping"

/** 鞅收敛定理：L^2有界鞅几乎必然收敛 */
#define PRESET_SP_MARTINGALE_CONVERGENCE "sp_martingale_convergence"

/** Doob分解：X_n = M_n + A_n（鞅部分+可料部分） */
#define PRESET_SP_MARTINGALE_DECOMPOSITION "sp_martingale_decomposition"

/* -------------------- 随机游走（3个） -------------------- */

/** 随机游走构造：由步长分布构造随机游走 */
#define PRESET_SP_RANDOM_WALK_CONSTRUCT "sp_random_walk_construct"

/** 回归概率：对称随机游走回归原点的概率 */
#define PRESET_SP_RANDOM_WALK_RETURN "sp_random_walk_return"

/** 赌徒破产问题：计算破产概率 */
#define PRESET_SP_RANDOM_WALK_GAMBLER_RUIN "sp_random_walk_gambler_ruin"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有随机过程预设函数块
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_stochastic_processes_register(void);

/**
 * @brief 获取随机过程预设函数块数量
 *
 * @return int 随机过程模块预设函数块总数
 */
int preset_stochastic_processes_count(void);

/**
 * @brief 获取随机过程预设函数块名称列表
 *
 * @param out_names 输出名称数组指针（调用者负责释放）
 * @param out_count 输出预设数量
 * @return true 获取成功
 * @return false 获取失败
 */
bool preset_stochastic_processes_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取随机过程模块类别名称
 *
 * @return 类别名称字符串
 */
const char *preset_stochastic_processes_category(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_STOCHASTIC_PROCESSES_H */
