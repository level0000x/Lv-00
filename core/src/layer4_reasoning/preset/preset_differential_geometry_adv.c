/**
 * @file preset_differential_geometry_adv.c
 * @brief 微分几何进阶预设函数块 - 实现
 *
 * 实现微分几何进阶领域的预设函数块注册。
 * 涵盖联络、曲率张量、测地线、Riemann几何及纤维丛。
 *
 * @module DifferentialGeometryAdv
 * @category PRESET_EXT_DIFFERENTIAL_GEOMETRY
 */

#include "preset_differential_geometry_adv.h"
#include "preset_blocks.h"
#include "lv_internal.h"
#include "lv_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 微分几何进阶模块预设函数块总数 */
/* 已在头文件中定义 DIFFERENTIAL_GEOMETRY_ADV_PRESET_COUNT = 12 */

/* ==================== 模块注册实现 ==================== */

bool preset_differential_geometry_adv_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：联络与协变导数
     * ============================================================ */

    /* Levi-Civita联络 */
    if (preset_blocks_register_by_category(
            "levi_civita_connection",
            "计算Riemann流形上的Levi-Civita联络",
            PRESET_EXT_DIFFERENTIAL_GEOMETRY,
            2, 1)) {
        success_count++;
    }

    /* 协变导数 */
    if (preset_blocks_register_by_category(
            "covariant_derivative",
            "计算向量场沿另一向量场的协变导数 ∇_X Y",
            PRESET_EXT_DIFFERENTIAL_GEOMETRY,
            3, 1)) {
        success_count++;
    }

    /* Christoffel符号 */
    if (preset_blocks_register_by_category(
            "christoffel_symbols",
            "由度量张量计算Christoffel符号 Γ^k_{ij}",
            PRESET_EXT_DIFFERENTIAL_GEOMETRY,
            1, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：曲率
     * ============================================================ */

    /* Riemann曲率张量 */
    if (preset_blocks_register_by_category(
            "riemann_curvature_tensor",
            "计算Riemann曲率张量 R^l_{ijk}",
            PRESET_EXT_DIFFERENTIAL_GEOMETRY,
            1, 1)) {
        success_count++;
    }

    /* Ricci曲率张量 */
    if (preset_blocks_register_by_category(
            "ricci_curvature",
            "计算Ricci曲率张量 R_{ij} = R^k_{ikj}",
            PRESET_EXT_DIFFERENTIAL_GEOMETRY,
            1, 1)) {
        success_count++;
    }

    /* 标量曲率 */
    if (preset_blocks_register_by_category(
            "scalar_curvature",
            "计算标量曲率 S = g^{ij} R_{ij}",
            PRESET_EXT_DIFFERENTIAL_GEOMETRY,
            1, 1)) {
        success_count++;
    }

    /* 截面曲率 */
    if (preset_blocks_register_by_category(
            "sectional_curvature",
            "计算二维截面的截面曲率 K(σ)",
            PRESET_EXT_DIFFERENTIAL_GEOMETRY,
            3, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：测地线
     * ============================================================ */

    /* 测地线方程 */
    if (preset_blocks_register_by_category(
            "geodesic_equation",
            "建立测地线方程 d²x^k/dt² + Γ^k_{ij} dx^i/dt dx^j/dt = 0",
            PRESET_EXT_DIFFERENTIAL_GEOMETRY,
            2, 1)) {
        success_count++;
    }

    /* 指数映射 */
    if (preset_blocks_register_by_category(
            "exponential_map",
            "计算Riemann流形上的指数映射 exp_p(v)",
            PRESET_EXT_DIFFERENTIAL_GEOMETRY,
            2, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：张量与微分形式
     * ============================================================ */

    /* 张量缩并 */
    if (preset_blocks_register_by_category(
            "tensor_contraction",
            "计算张量的缩并运算",
            PRESET_EXT_DIFFERENTIAL_GEOMETRY,
            3, 1)) {
        success_count++;
    }

    /* 李导数 */
    if (preset_blocks_register_by_category(
            "lie_derivative",
            "计算张量场沿向量场的李导数 L_X T",
            PRESET_EXT_DIFFERENTIAL_GEOMETRY,
            3, 1)) {
        success_count++;
    }

    /* 测地线偏离方程 */
    if (preset_blocks_register_by_category(
            "geodesic_deviation",
            "计算测地线偏离方程（Jacobi方程）",
            PRESET_EXT_DIFFERENTIAL_GEOMETRY,
            2, 1)) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == DIFFERENTIAL_GEOMETRY_ADV_PRESET_COUNT;
}

int preset_differential_geometry_adv_count(void)
{
    return DIFFERENTIAL_GEOMETRY_ADV_PRESET_COUNT;
}
