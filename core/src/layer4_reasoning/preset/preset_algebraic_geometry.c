/**
 * @file preset_algebraic_geometry.c
 * @brief 代数几何预设函数块 - 实现
 *
 * 实现代数几何领域的预设函数块注册。
 * 涵盖代数簇、理想与Gr"obner基、射影几何等核心概念。
 *
 * @module AlgebraicGeometry
 * @category PRESET_EXT_ALGEBRA_ADVANCED
 */

#include "preset_algebraic_geometry.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 代数几何模块预设函数块总数 */
/* 已在头文件中定义 ALGEBRAIC_GEOMETRY_PRESET_COUNT = 14 */

/* ==================== 模块注册实现 ==================== */

bool preset_algebraic_geometry_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：仿射代数簇
     * ============================================================ */

    /* 仿射代数簇构造 */
    if (preset_blocks_register_by_category(
            "affine_variety",
            "由多项式零点集定义仿射代数簇 V(I) = {p : f(p)=0, f in I}",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 仿射簇判定 */
    if (preset_blocks_register_by_category(
            "affine_variety_test",
            "判定点是否在仿射代数簇上",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* 簇的维数 */
    if (preset_blocks_register_by_category(
            "variety_dimension",
            "计算代数簇的维数（Krull维数）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1) == 0) {
        success_count++;
    }

    /* 不可约簇判定 */
    if (preset_blocks_register_by_category(
            "irreducible_variety_test",
            "判定代数簇是否不可约",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：理想与Gröbner基
     * ============================================================ */

    /* 理想构造 */
    if (preset_blocks_register_by_category(
            "ideal_construct",
            "由多项式集合生成的理想 I = (f1, f2, ..., fn)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* Gröbner基计算 */
    if (preset_blocks_register_by_category(
            "groebner_basis",
            "计算多项式理想的Gröbner基（Buchberger算法）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1) == 0) {
        success_count++;
    }

    /* 理想的交 */
    if (preset_blocks_register_by_category(
            "ideal_intersection",
            "计算两个理想的交集 I ∩ J",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 理想的商 */
    if (preset_blocks_register_by_category(
            "ideal_quotient",
            "计算理想商 I : J = {f : fJ ⊆ I}",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：射影几何
     * ============================================================ */

    /* 射影空间构造 */
    if (preset_blocks_register_by_category(
            "projective_space",
            "构造n维射影空间 P^n(k)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 射影代数簇 */
    if (preset_blocks_register_by_category(
            "projective_variety",
            "由齐次多项式定义射影代数簇",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：态射与映射
     * ============================================================ */

    /* 有理映射 */
    if (preset_blocks_register_by_category(
            "rational_map",
            "构造代数簇之间的有理映射",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* 态射判定 */
    if (preset_blocks_register_by_category(
            "morphism_test",
            "判定映射是否为代数簇间的态射",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* 双有理等价 */
    if (preset_blocks_register_by_category(
            "birational_equivalence",
            "判定两个代数簇是否双有理等价",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 奇异点检测 */
    if (preset_blocks_register_by_category(
            "singularity_detection",
            "检测代数簇上的奇异点",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == ALGEBRAIC_GEOMETRY_PRESET_COUNT;
}

int preset_algebraic_geometry_count(void)
{
    return ALGEBRAIC_GEOMETRY_PRESET_COUNT;
}
