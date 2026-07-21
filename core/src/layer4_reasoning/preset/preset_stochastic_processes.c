/**
 * @file preset_stochastic_processes.c
 * @brief 随机过程预设函数块 - 实现
 *
 * 实现随机过程领域的预设函数块注册。
 * 涵盖Markov链、Brown运动、鞅论、Poisson过程及随机微分方程。
 *
 * @module StochasticProcesses
 * @category PRESET_EXT_ANALYSIS
 */

#include "preset_stochastic_processes.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 随机过程模块预设函数块总数 */
/* 已在头文件中定义 STOCHASTIC_PROCESSES_PRESET_COUNT = 14 */

/* ==================== 模块注册实现 ==================== */

int preset_stochastic_processes_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：Markov链
     * ============================================================ */

    /* 离散Markov链构造 */
    if (preset_blocks_register_by_category(
            "markov_chain_construct",
            "由转移矩阵P构造离散时间Markov链",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* n步转移概率 */
    if (preset_blocks_register_by_category(
            "markov_n_step_transition",
            "计算n步转移概率矩阵 P^n",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* 平稳分布 */
    if (preset_blocks_register_by_category(
            "stationary_distribution",
            "计算Markov链的平稳分布 π = πP",
            PRESET_EXT_ANALYSIS,
            1, 1) == 0) {
        success_count++;
    }

    /* 遍历性判定 */
    if (preset_blocks_register_by_category(
            "ergodicity_test",
            "判定Markov链是否遍历（不可约且非周期）",
            PRESET_EXT_ANALYSIS,
            1, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：Brown运动
     * ============================================================ */

    /* Brown运动模拟 */
    if (preset_blocks_register_by_category(
            "brownian_motion_simulate",
            "模拟标准Brown运动路径 W(t)",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* Brown运动性质 */
    if (preset_blocks_register_by_category(
            "brownian_properties",
            "计算Brown运动的统计性质（均值、方差、协方差）",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* 反射Brown运动 */
    if (preset_blocks_register_by_category(
            "reflected_brownian",
            "构造反射Brown运动 |W(t)|",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：鞅论
     * ============================================================ */

    /* 鞅判定 */
    if (preset_blocks_register_by_category(
            "martingale_test",
            "判定随机过程是否是鞅 E[X_{n+1}|F_n] = X_n",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* 可选停时定理 */
    if (preset_blocks_register_by_category(
            "optional_stopping",
            "应用可选停时定理 E[X_T] = E[X_0]",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：Poisson过程
     * ============================================================ */

    /* Poisson过程构造 */
    if (preset_blocks_register_by_category(
            "poisson_process_construct",
            "构造强度为λ的Poisson过程 N(t)",
            PRESET_EXT_ANALYSIS,
            2, 1) == 0) {
        success_count++;
    }

    /* 到达时间分布 */
    if (preset_blocks_register_by_category(
            "arrival_time_distribution",
            "计算Poisson过程第k次到达时间（Erlang分布）",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第五部分：随机微分方程
     * ============================================================ */

    /* Itô积分 */
    if (preset_blocks_register_by_category(
            "ito_integral",
            "计算Itô随机积分 ∫_0^t f(s) dW(s)",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* 随机微分方程求解 */
    if (preset_blocks_register_by_category(
            "sde_solve",
            "求解Itô随机微分方程 dX = μ dt + σ dW",
            PRESET_EXT_ANALYSIS,
            3, 1) == 0) {
        success_count++;
    }

    /* 几何Brown运动 */
    if (preset_blocks_register_by_category(
            "geometric_brownian_motion",
            "构造几何Brown运动 S(t) = S(0) exp((μ-σ²/2)t + σW(t))",
            PRESET_EXT_ANALYSIS,
            4, 1) == 0) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == STOCHASTIC_PROCESSES_PRESET_COUNT;
}

int preset_stochastic_processes_count(void)
{
    return STOCHASTIC_PROCESSES_PRESET_COUNT;
}
