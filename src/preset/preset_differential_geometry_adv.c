/**
 * @file preset_differential_geometry_adv.c
 * @brief 微分几何进阶预设函数块模块 - 实现
 *
 * 实现理论数学研究中常用的微分几何进阶预设函数块。
 * 涵盖切空间、Riemann度量、测地线、Levi-Civita连接、
 * 指数映射、平行移动、曲率张量、Gauss-Bonnet定理，
 * 共 8 个预设。
 *
 * @module DifferentialGeometryAdv
 * @category PRESET_EXT_DIFFERENTIAL_GEOMETRY
 * @version 1.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_differential_geometry_adv.h -> preset_blocks.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_by_category() 声明
 *   -> 提供 PresetExtendedCategory 枚举（PRESET_EXT_DIFFERENTIAL_GEOMETRY）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 * lv00_internal.h / lv00_utils.h
 *   -> 提供 lv00_malloc、lv00_free、lv00_strdup、lv00_log_* 等
 * ============================================================
 */
#include "preset_differential_geometry_adv.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 微分几何进阶模块预设函数块总数 */
#define DIFFERENTIAL_GEOMETRY_ADV_PRESET_COUNT 8

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个微分几何进阶预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有微分几何进阶预设都属于 PRESET_EXT_DIFFERENTIAL_GEOMETRY 类别。
 * 使用 preset_blocks_register_by_category() 直接注册到扩展类别。
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_count 输入端口数量
 * @param output_count 输出端口数量
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_dg_adv_preset(
    const char *name,
    const char *description,
    int input_count,
    int output_count)
{
    return preset_blocks_register_by_category(
        name, description,
        PRESET_EXT_DIFFERENTIAL_GEOMETRY,
        input_count, output_count);
}

/* ==================== 模块注册实现 ==================== */

bool preset_differential_geometry_adv_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一组：流形上的结构（2个预设）
     *
     * 涵盖光滑流形上的基本几何结构：
     *  - 切空间 T_pM：流形在一点处的线性化
     *  - Riemann度量 g：流形上的内积结构
     * ============================================================ */

    /**
     * @brief tangent_space - 切空间
     *
     * 计算光滑流形 M 在点 p 处的切空间 T_pM。
     * 切空间由所有经过 p 的光滑曲线在该点的速度向量组成，
     * 是流形在一点处的最佳线性近似。
     *
     * @param M 光滑流形（PRESET_TYPE_MANIFOLD）
     * @param p 流形上的点（PRESET_TYPE_POINT）
     * @return 切空间 T_pM（PRESET_TYPE_VECTOR）
     * @math T_pM = \\{\\dot{\\gamma}(0) : \\gamma \\in C^{\\infty}(\\mathbb{R}, M), \\; \\gamma(0) = p\\}
     * @complexity O(n)
     * @constructive true
     * @reversible false
     */
    {
        if (register_dg_adv_preset(
                PRESET_DG_TANGENT_SPACE,
                "切空间：计算光滑流形 M 在点 p 处的切空间 T_pM，"
                "由经过 p 的光滑曲线的速度向量构成",
                2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief riemannian_metric - Riemann度量
     *
     * 在光滑流形 M 上定义 Riemann 度量张量 g。
     * Riemann 度量是流形上逐点变化的正定对称双线性形式，
     * 赋予流形内积结构，从而可以定义长度、角度、体积等几何量。
     *
     * @param M 光滑流形（PRESET_TYPE_MANIFOLD）
     * @return Riemann度量张量 g（PRESET_TYPE_FUNCTION）
     * @math g_p : T_pM \\times T_pM \\to \\mathbb{R}, \\quad g_p(X, Y) = g_p(Y, X) > 0 \\; (X \\neq 0)
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        if (register_dg_adv_preset(
                PRESET_DG_RIEMANNIAN_METRIC,
                "Riemann度量：在光滑流形上定义正定对称双线性形式 g，"
                "赋予流形内积结构与距离函数",
                1, 1)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二组：测地线与连接（4个预设）
     *
     * 涵盖 Riemann 流形上的联络与测地线理论：
     *  - 测地线：流形上的"直线"
     *  - Levi-Civita连接：唯一的无挠度量相容联络
     *  - 指数映射：切空间到流形的局部微分同胚
     *  - 平行移动：沿曲线保持切向量平行
     * ============================================================ */

    /**
     * @brief geodesic - 测地线
     *
     * 计算 Riemann 流形 (M, g) 上的测地线方程及其解。
     * 测地线是切向量沿自身平行的曲线，是欧氏空间中直线的推广，
     * 满足测地线方程（二阶常微分方程）。
     *
     * @param M Riemann流形（PRESET_TYPE_MANIFOLD）
     * @param p 起始点（PRESET_TYPE_POINT）
     * @param v 初始切向量（PRESET_TYPE_VECTOR）
     * @return 测地线曲线 gamma(t)（PRESET_TYPE_PATH）
     * @math \\nabla_{\\dot{\\gamma}}\\dot{\\gamma} = 0, \\quad \\gamma(0) = p, \\; \\dot{\\gamma}(0) = v
     * @complexity O(n^2)（数值积分）
     * @constructive true
     * @reversible false
     */
    {
        if (register_dg_adv_preset(
                PRESET_DG_GEODESIC,
                "测地线：求解Riemann流形上的测地线方程 "
                "nabla_{dot_gamma} dot_gamma = 0，给定初始点和切向量",
                3, 1)) {
            success_count++;
        }
    }

    /**
     * @brief connection_levi_civita - Levi-Civita连接
     *
     * 计算 Riemann 流形 (M, g) 上唯一的 Levi-Civita 联络 nabla。
     * Levi-Civita 联络是满足无挠性和度量相容性的唯一仿射联络，
     * 由 Koszul 公式完全确定。
     *
     * @param M Riemann流形（PRESET_TYPE_MANIFOLD）
     * @param g Riemann度量（PRESET_TYPE_FUNCTION）
     * @return Levi-Civita联络 nabla（PRESET_TYPE_FUNCTION）
     * @math 2g(\\nabla_X Y, Z) = X \\cdot g(Y,Z) + Y \\cdot g(Z,X) - Z \\cdot g(X,Y) \\\\
     *       + g([X,Y],Z) - g([Y,Z],X) + g([Z,X],Y) \\quad \\text{(Koszull公式)}
     * @complexity O(n^3)
     * @constructive true
     * @reversible false
     */
    {
        if (register_dg_adv_preset(
                PRESET_DG_CONNECTION_LEVI_CIVITA,
                "Levi-Civita连接：计算Riemann流形上唯一的无挠度量相容联络，"
                "由Koszul公式确定Christoffel符号",
                2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief exponential_map - 指数映射
     *
     * 计算 Riemann 流形 (M, g) 在点 p 处的指数映射 exp_p。
     * 指数映射将切空间 T_pM 中的向量映射到流形上的点，
     * 沿以该向量为初始速度的测地线行走单位时间到达的位置。
     * 在 p 的充分小邻域内，指数映射是微分同胚。
     *
     * @param p 流形上的点（PRESET_TYPE_POINT）
     * @param v 切向量（PRESET_TYPE_VECTOR）
     * @return 流形上的点 exp_p(v)（PRESET_TYPE_POINT）
     * @math \\exp_p(v) = \\gamma_v(1), \\quad \\gamma_v(0) = p, \\; \\dot{\\gamma}_v(0) = v
     * @complexity O(n^2)（数值积分）
     * @constructive true
     * @reversible true（在法邻域内可逆，逆为对数映射 log_p）
     */
    {
        if (register_dg_adv_preset(
                PRESET_DG_EXPONENTIAL_MAP,
                "指数映射：exp_p: T_pM -> M，将切向量沿测地线映射到流形上的点，"
                "在法邻域内为微分同胚",
                2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief parallel_transport - 平行移动
     *
     * 沿 Riemann 流形 (M, g) 上的光滑曲线 gamma 将切向量平行移动。
     * 平行移动保持向量沿曲线与 Levi-Civita 联络的协变导数为零，
     * 是流形上"平移"概念的推广。平行移动一般依赖路径。
     *
     * @param gamma 光滑曲线（PRESET_TYPE_PATH）
     * @param v 初始切向量（PRESET_TYPE_VECTOR）
     * @return 平行移动后的切向量（PRESET_TYPE_VECTOR）
     * @math \\nabla_{\\dot{\\gamma}} V(t) = 0, \\quad V(0) = v
     * @complexity O(n^2)（沿曲线数值积分）
     * @constructive true
     * @reversible true（反向沿曲线平行移动可恢复原向量）
     */
    {
        if (register_dg_adv_preset(
                PRESET_DG_PARALLEL_TRANSPORT,
                "平行移动：沿光滑曲线将切向量按Levi-Civita联络平行移动，"
                "保持协变导数为零",
                2, 1)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三组：曲率理论（2个预设）
     *
     * 涵盖 Riemann 流形的曲率理论与全局定理：
     *  - 曲率张量：Riemann曲率张量的计算
     *  - Gauss-Bonnet定理：连接局部曲率与全局拓扑
     * ============================================================ */

    /**
     * @brief curvature_tensor - 曲率张量
     *
     * 计算 Riemann 流形 (M, g) 上的 Riemann 曲率张量 R。
     * 曲率张量度量平行移动的路径依赖性，反映了流形的内蕴弯曲程度。
     * 由 Levi-Civita 联络的 Christoffel 符号及其导数给出。
     *
     * @param M Riemann流形（PRESET_TYPE_MANIFOLD）
     * @param g Riemann度量（PRESET_TYPE_FUNCTION）
     * @return Riemann曲率张量 R(X,Y)Z（PRESET_TYPE_FUNCTION）
     * @math R(X,Y)Z = \\nabla_X \\nabla_Y Z - \\nabla_Y \\nabla_X Z - \\nabla_{[X,Y]} Z \\\\
     *       R^{i}_{\\;jkl} = \\partial_k \\Gamma^{i}_{jl} - \\partial_l \\Gamma^{i}_{jk}
     *       + \\Gamma^{i}_{km}\\Gamma^{m}_{jl} - \\Gamma^{i}_{lm}\\Gamma^{m}_{jk}
     * @complexity O(n^3)
     * @constructive true
     * @reversible false
     */
    {
        if (register_dg_adv_preset(
                PRESET_DG_CURVATURE_TENSOR,
                "曲率张量：计算Riemann曲率张量 R(X,Y)Z，"
                "度量平行移动的路径依赖性与流形的内蕴弯曲",
                2, 1)) {
            success_count++;
        }
    }

    /**
     * @brief gauss_bonnet - Gauss-Bonnet定理
     *
     * 计算 Gauss-Bonnet 定理中的积分，建立局部曲率与全局拓扑的联系。
     * 对于紧致定向二维 Riemann 流形（闭曲面），Gauss 曲率的积分
     * 等于 2pi 乘以 Euler 示性数，是微分几何中最深刻的定理之一。
     *
     * @param M 紧致定向二维Riemann流形（PRESET_TYPE_MANIFOLD）
     * @param g Riemann度量（PRESET_TYPE_FUNCTION）
     * @return Gauss-Bonnet积分值（PRESET_TYPE_SCALAR）
     * @math \\int_M K \\, dA = 2\\pi \\chi(M) \\\\
     *       \\text{其中 } K \\text{ 为Gauss曲率，} \\chi(M) \\text{ 为Euler示性数}
     * @complexity O(n^2)（数值积分）
     * @constructive true
     * @reversible false
     */
    {
        if (register_dg_adv_preset(
                PRESET_DG_GAUSS_BONNET,
                "Gauss-Bonnet定理：计算闭曲面上Gauss曲率的积分，"
                "等于 2pi 乘以 Euler 示性数，连接曲率与拓扑",
                2, 1)) {
            success_count++;
        }
    }

    /* ============================================================
     * 注册结果统计
     * ============================================================ */

    if (success_count < DIFFERENTIAL_GEOMETRY_ADV_PRESET_COUNT) {
        LV00_LOG_WARNING(
            "微分几何进阶模块：共 %d 个预设，成功注册 %d 个",
            DIFFERENTIAL_GEOMETRY_ADV_PRESET_COUNT, success_count);
    }

    return success_count == DIFFERENTIAL_GEOMETRY_ADV_PRESET_COUNT;
}

/* ==================== 模块信息函数 ==================== */

/**
 * @brief 获取微分几何进阶预设函数块数量
 *
 * @return int 预设函数块总数（8）
 */
int preset_differential_geometry_adv_count(void)
{
    return DIFFERENTIAL_GEOMETRY_ADV_PRESET_COUNT;
}
