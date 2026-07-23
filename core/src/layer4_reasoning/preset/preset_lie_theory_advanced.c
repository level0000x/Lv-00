/**
 * @file preset_lie_theory_advanced.c
 * @brief 李理论进阶预设函数块 - 实现
 *
 * 实现李理论进阶领域的预设函数块注册。
 * 涵盖李代数、根系、Weyl群、表示论及权空间分解。
 *
 * @module LieTheoryAdvanced
 * @category PRESET_EXT_ALGEBRA_ADVANCED
 */

#include "preset_lie_theory_advanced.h"
#include "preset_blocks.h"
#include "lv_internal.h"
#include "lv_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 李理论进阶模块预设函数块总数 */
/* 已在头文件中定义 LIE_THEORY_ADVANCED_PRESET_COUNT = 8 */

/* ==================== 模块注册实现 ==================== */

bool preset_lie_theory_advanced_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：李代数
     * ============================================================ */

    /* 李代数构造 */
    if (preset_blocks_register_by_category(
            "lie_algebra_construct",
            "由生成元和李括号关系构造李代数 g",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1)) {
        success_count++;
    }

    /* 李括号运算 */
    if (preset_blocks_register_by_category(
            "lie_bracket",
            "计算李代数中两个元素的李括号 [X, Y]",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：根系与Weyl群
     * ============================================================ */

    /* 根系计算 */
    if (preset_blocks_register_by_category(
            "root_system_compute",
            "计算半单李代数的根系 Φ",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1)) {
        success_count++;
    }

    /* Weyl群 */
    if (preset_blocks_register_by_category(
            "weyl_group",
            "计算根系对应的Weyl群 W(Φ)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1)) {
        success_count++;
    }

    /* Cartan矩阵 */
    if (preset_blocks_register_by_category(
            "cartan_matrix",
            "由根系计算Cartan矩阵 A_{ij} = <α_i, α_j>",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：表示论
     * ============================================================ */

    /* 不可约表示构造 */
    if (preset_blocks_register_by_category(
            "irreducible_representations",
            "构造李代数的最高权不可约表示 V(λ)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1)) {
        success_count++;
    }

    /* 权空间分解 */
    if (preset_blocks_register_by_category(
            "weight_space_decomposition",
            "计算表示的权空间分解 V = ⊕ V_μ",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1)) {
        success_count++;
    }

    /* Weyl特征公式 */
    if (preset_blocks_register_by_category(
            "weyl_character_formula",
            "用Weyl特征公式计算不可约表示的特征 ch V(λ)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1)) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == LIE_THEORY_ADVANCED_PRESET_COUNT;
}

int preset_lie_theory_advanced_count(void)
{
    return LIE_THEORY_ADVANCED_PRESET_COUNT;
}
