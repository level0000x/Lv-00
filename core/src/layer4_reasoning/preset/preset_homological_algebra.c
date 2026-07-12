/**
 * @file preset_homological_algebra.c
 * @brief 同调代数预设函数块 - 实现
 *
 * 实现同调代数领域的预设函数块注册。
 * 涵盖链复形、同调群、导出函子、Ext与Tor函子等。
 *
 * @module HomologicalAlgebra
 * @category PRESET_EXT_ALGEBRA_ADVANCED
 */

#include "preset_homological_algebra.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 同调代数模块预设函数块总数 */
/* 已在头文件中定义 HOMOLOGICAL_ALGEBRA_PRESET_COUNT = 10 */

/* ==================== 模块注册实现 ==================== */

bool preset_homological_algebra_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：链复形
     * ============================================================ */

    /* 链复形构造 */
    if (preset_blocks_register_by_category(
            "chain_complex_construct",
            "构造链复形 ... → C_{n+1} → C_n → C_{n-1} → ...",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1)) {
        success_count++;
    }

    /* 上链复形构造 */
    if (preset_blocks_register_by_category(
            "cochain_complex_construct",
            "构造上链复形 ... ← C^{n+1} ← C^n ← C^{n-1} ← ...",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：同调与上同调
     * ============================================================ */

    /* 同调群计算 */
    if (preset_blocks_register_by_category(
            "homology_group_compute",
            "计算链复形的第n个同调群 H_n = Ker d_n / Im d_{n+1}",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1)) {
        success_count++;
    }

    /* 上同调群计算 */
    if (preset_blocks_register_by_category(
            "cohomology_group_compute",
            "计算上链复形的第n个上同调群 H^n = Ker d^n / Im d^{n-1}",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1)) {
        success_count++;
    }

    /* 正合序列判定 */
    if (preset_blocks_register_by_category(
            "exact_sequence_test",
            "判定序列 0 → A → B → C → 0 是否正合",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：导出函子
     * ============================================================ */

    /* 左导出函子 */
    if (preset_blocks_register_by_category(
            "left_derived_functor",
            "计算右正合函子F的左导出函子 L_n F",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1)) {
        success_count++;
    }

    /* 右导出函子 */
    if (preset_blocks_register_by_category(
            "right_derived_functor",
            "计算左正合函子F的右导出函子 R^n F",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：Ext与Tor
     * ============================================================ */

    /* Ext函子 */
    if (preset_blocks_register_by_category(
            "ext_functor",
            "计算Ext^n_R(A, B)（Hom的右导出函子）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1)) {
        success_count++;
    }

    /* Tor函子 */
    if (preset_blocks_register_by_category(
            "tor_functor",
            "计算Tor_n^R(A, B)（张量积的左导出函子）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1)) {
        success_count++;
    }

    /* 投射分解 */
    if (preset_blocks_register_by_category(
            "projective_resolution",
            "构造模M的投射分解 ... → P_1 → P_0 → M → 0",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1)) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == HOMOLOGICAL_ALGEBRA_PRESET_COUNT;
}

int preset_homological_algebra_count(void)
{
    return HOMOLOGICAL_ALGEBRA_PRESET_COUNT;
}
