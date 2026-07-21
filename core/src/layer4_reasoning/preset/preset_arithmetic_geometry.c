/**
 * @file preset_arithmetic_geometry.c
 * @brief 算术几何预设函数块 - 实现
 *
 * 实现算术几何领域的预设函数块注册。
 * 涵盖椭圆曲线、模形式、Abel簇及有理点理论。
 *
 * @module ArithmeticGeometry
 * @category PRESET_EXT_NUMBER_THEORY
 */

#include "preset_arithmetic_geometry.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 算术几何模块预设函数块总数 */
/* 已在头文件中定义 ARITHMETIC_GEOMETRY_PRESET_COUNT = 12 */

/* ==================== 模块注册实现 ==================== */

bool preset_arithmetic_geometry_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：椭圆曲线
     * ============================================================ */

    /* 椭圆曲线构造 */
    if (preset_blocks_register_by_category(
            "elliptic_curve_construct",
            "构造椭圆曲线 E: y^2 = x^3 + ax + b（Weierstrass形式）",
            PRESET_EXT_NUMBER_THEORY,
            3, 1) == 0) {
        success_count++;
    }

    /* 椭圆曲线加法 */
    if (preset_blocks_register_by_category(
            "elliptic_curve_add",
            "椭圆曲线上两点的群加法 P + Q",
            PRESET_EXT_NUMBER_THEORY,
            3, 1) == 0) {
        success_count++;
    }

    /* 椭圆曲线倍点 */
    if (preset_blocks_register_by_category(
            "elliptic_curve_double",
            "椭圆曲线上点的倍点运算 [n]P",
            PRESET_EXT_NUMBER_THEORY,
            3, 1) == 0) {
        success_count++;
    }

    /* 椭圆曲线阶 */
    if (preset_blocks_register_by_category(
            "elliptic_curve_order",
            "计算椭圆曲线在有限域上的有理点数 #E(F_p)",
            PRESET_EXT_NUMBER_THEORY,
            2, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：模形式
     * ============================================================ */

    /* 模形式构造 */
    if (preset_blocks_register_by_category(
            "modular_form_construct",
            "构造权为k、级为N的模形式",
            PRESET_EXT_NUMBER_THEORY,
            3, 1) == 0) {
        success_count++;
    }

    /* 模形式q展开 */
    if (preset_blocks_register_by_category(
            "modular_form_q_expansion",
            "计算模形式的q展开系数",
            PRESET_EXT_NUMBER_THEORY,
            2, 1) == 0) {
        success_count++;
    }

    /* Hecke算子 */
    if (preset_blocks_register_by_category(
            "hecke_operator",
            "计算模形式上的Hecke算子 T_n 作用",
            PRESET_EXT_NUMBER_THEORY,
            3, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：有理点理论
     * ============================================================ */

    /* Mordell-Weil群 */
    if (preset_blocks_register_by_category(
            "mordell_weil_group",
            "计算椭圆曲线的Mordell-Weil群 E(Q)/torsion",
            PRESET_EXT_NUMBER_THEORY,
            1, 1) == 0) {
        success_count++;
    }

    /* 有理点搜索 */
    if (preset_blocks_register_by_category(
            "rational_point_search",
            "在代数簇上搜索有理点",
            PRESET_EXT_NUMBER_THEORY,
            2, 1) == 0) {
        success_count++;
    }

    /* Torsion子群 */
    if (preset_blocks_register_by_category(
            "torsion_subgroup",
            "计算椭圆曲线的Torsion子群 E(Q)_tors",
            PRESET_EXT_NUMBER_THEORY,
            1, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：L函数与BSD猜想
     * ============================================================ */

    /* Hasse-Weil L函数 */
    if (preset_blocks_register_by_category(
            "hasse_weil_l_function",
            "计算椭圆曲线的Hasse-Weil L函数 L(E, s)",
            PRESET_EXT_NUMBER_THEORY,
            2, 1) == 0) {
        success_count++;
    }

    /* Birch-Swinnerton-Dyer不变量 */
    if (preset_blocks_register_by_category(
            "bsd_invariants",
            "计算BSD猜想相关的不变量（秩、Sha群、调节子）",
            PRESET_EXT_NUMBER_THEORY,
            1, 1) == 0) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == ARITHMETIC_GEOMETRY_PRESET_COUNT;
}

int preset_arithmetic_geometry_count(void)
{
    return ARITHMETIC_GEOMETRY_PRESET_COUNT;
}
