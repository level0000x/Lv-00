/**
 * @file preset_game_theory.c
 * @brief 博弈论预设函数块 - 实现
 *
 * 实现博弈论领域的预设函数块注册。
 * 涵盖Nash均衡、混合策略、合作博弈及纳什讨价还价等。
 *
 * @module GameTheory
 * @category PRESET_EXT_OPTIMIZATION_THEORY
 */

#include "preset_game_theory.h"
#include "preset_blocks.h"
#include "lv_internal.h"
#include "lv_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 博弈论模块预设函数块总数 */
/* 已在头文件中定义 GAME_THEORY_PRESET_COUNT = 10 */

/* ==================== 模块注册实现 ==================== */

bool preset_game_theory_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：博弈的基本表示
     * ============================================================ */

    /* 标准型博弈构造 */
    if (preset_blocks_register_by_category(
            "normal_form_game",
            "构造标准型（策略型）博弈 G = (N, S, u)",
            PRESET_EXT_OPTIMIZATION_THEORY,
            3, 1)) {
        success_count++;
    }

    /* 扩展型博弈构造 */
    if (preset_blocks_register_by_category(
            "extensive_form_game",
            "构造扩展型博弈（博弈树）",
            PRESET_EXT_OPTIMIZATION_THEORY,
            3, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：Nash均衡
     * ============================================================ */

    /* 纯策略Nash均衡 */
    if (preset_blocks_register_by_category(
            "pure_nash_equilibrium",
            "求解纯策略Nash均衡",
            PRESET_EXT_OPTIMIZATION_THEORY,
            1, 1)) {
        success_count++;
    }

    /* 混合策略Nash均衡 */
    if (preset_blocks_register_by_category(
            "mixed_nash_equilibrium",
            "求解双人博弈的混合策略Nash均衡",
            PRESET_EXT_OPTIMIZATION_THEORY,
            1, 1)) {
        success_count++;
    }

    /* Nash均衡存在性 */
    if (preset_blocks_register_by_category(
            "nash_equilibrium_existence",
            "判定Nash均衡的存在性",
            PRESET_EXT_OPTIMIZATION_THEORY,
            1, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：策略分析
     * ============================================================ */

    /* 占优策略分析 */
    if (preset_blocks_register_by_category(
            "dominant_strategy_analysis",
            "分析占优策略并进行策略删除（迭代删除被占优策略）",
            PRESET_EXT_OPTIMIZATION_THEORY,
            1, 1)) {
        success_count++;
    }

    /* 最大最小策略 */
    if (preset_blocks_register_by_category(
            "maxmin_strategy",
            "计算玩家的最大最小策略（最坏情况下的最优策略）",
            PRESET_EXT_OPTIMIZATION_THEORY,
            2, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：合作博弈
     * ============================================================ */

    /* Shapley值 */
    if (preset_blocks_register_by_category(
            "shapley_value",
            "计算合作博弈的Shapley值（公平分配）",
            PRESET_EXT_OPTIMIZATION_THEORY,
            1, 1)) {
        success_count++;
    }

    /* 核心 */
    if (preset_blocks_register_by_category(
            "coalition_core",
            "计算合作博弈的核心（稳定分配集）",
            PRESET_EXT_OPTIMIZATION_THEORY,
            1, 1)) {
        success_count++;
    }

    /* 纳什讨价还价解 */
    if (preset_blocks_register_by_category(
            "nash_bargaining_solution",
            "计算纳什讨价还价问题的解",
            PRESET_EXT_OPTIMIZATION_THEORY,
            3, 1)) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == GAME_THEORY_PRESET_COUNT;
}

int preset_game_theory_count(void)
{
    return GAME_THEORY_PRESET_COUNT;
}
