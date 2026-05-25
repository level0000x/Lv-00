/**
 * @file preset_differential_geometry_adv.h
 * @brief 微分几何进阶预设函数块 - 头文件
 *
 * 提供理论数学研究项目Lv-00中微分几何进阶领域的预设函数块，包括：
 *   - 流形上的结构：切空间、Riemann度量
 *   - 测地线与连接：测地线、Levi-Civita连接、指数映射、平行移动
 *   - 曲率理论：曲率张量、Gauss-Bonnet定理
 *
 * @module DifferentialGeometryAdv
 * @category PRESET_CATEGORY_DIFFERENTIAL_GEOMETRY
 * @version 1.0.0
 * @author Lv-00 开发团队
 */

#ifndef PRESET_DIFFERENTIAL_GEOMETRY_ADV_H
#define PRESET_DIFFERENTIAL_GEOMETRY_ADV_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 流形上的结构 -------------------- */

/**
 * @brief 切空间 T_pM
 *
 * @details 计算光滑流形 M 在点 p 处的切空间 T_pM = {所有过 p 点的光滑曲线切向量}，
 *          dim(T_pM) = dim(M)，基为坐标偏导 {partial/partial x^i|_p}。
 *
 * @note 输入: PRESET_TYPE_MANIFOLD, PRESET_TYPE_POINT
 *       输出: PRESET_TYPE_SET | 复杂度: O(n)
 */
#define PRESET_DG_TANGENT_SPACE "tangent_space"

/**
 * @brief Riemann 度量 g
 *
 * @details 在光滑流形上定义 Riemann 度量张量 g_{ij} = <partial_i, partial_j>，
 *          正定对称双线性形式，赋予流形长度和角度概念。
 *
 * @note 输入: PRESET_TYPE_MANIFOLD | 输出: PRESET_TYPE_TENSOR | 复杂度: O(n^2)
 */
#define PRESET_DG_RIEMANNIAN_METRIC "riemannian_metric"

/* -------------------- 测地线与连接 -------------------- */

/**
 * @brief 测地线
 *
 * @details 计算 Riemann 流形上满足 nabla_{gamma'} gamma' = 0 的测地线方程与解，
 *          测地线是欧氏空间直线的流形推广，局部最短路径。
 *
 * @note 输入: PRESET_TYPE_MANIFOLD, PRESET_TYPE_POINT, PRESET_TYPE_VECTOR
 *       输出: PRESET_TYPE_CURVE | 复杂度: O(n)
 */
#define PRESET_DG_GEODESIC "geodesic"

/**
 * @brief Levi-Civita 连接
 *
 * @details 计算 Riemann 流形上唯一满足以下条件的线联络：
 *          (1) 无挠性 nabla_X Y - nabla_Y X = [X, Y]
 *          (2) 度量相容性 X<Y, Z> = <nabla_X Y, Z> + <Y, nabla_X Z>
 *
 * @note 输入: PRESET_TYPE_MANIFOLD | 输出: PRESET_TYPE_TENSOR (Christoffel 符号) | 复杂度: O(n^3)
 */
#define PRESET_DG_CONNECTION_LEVI_CIVITA "connection_levi_civita"

/**
 * @brief 指数映射 exp_p: T_pM → M
 *
 * @details 从切向量 v in T_pM 出发，沿以 v 为初速的测地线行进单位时间到达流形上点。
 *          exp_p 是从切空间到流形的局部微分同胚。
 *
 * @note 输入: PRESET_TYPE_MANIFOLD, PRESET_TYPE_POINT, PRESET_TYPE_VECTOR
 *       输出: PRESET_TYPE_POINT | 复杂度: O(n)
 */
#define PRESET_DG_EXPONENTIAL_MAP "exponential_map"

/**
 * @brief 平行移动
 *
 * @details 沿曲线 gamma 将切向量 v 平行移动，满足 nabla_{gamma'} v = 0。
 *          平行移动是曲面上"保持方向不变"的严格数学表达，
 *          在平坦空间中退化为普通向量平移。
 *
 * @note 输入: PRESET_TYPE_MANIFOLD, PRESET_TYPE_CURVE, PRESET_TYPE_VECTOR
 *       输出: PRESET_TYPE_VECTOR | 复杂度: O(n)
 */
#define PRESET_DG_PARALLEL_TRANSPORT "parallel_transport"

/* -------------------- 曲率理论 -------------------- */

/**
 * @brief Riemann 曲率张量 R(X,Y)Z
 *
 * @details 计算 (3,1) 型 Riemann 曲率张量：
 *          R(X,Y)Z = nabla_X nabla_Y Z - nabla_Y nabla_X Z - nabla_{[X,Y]} Z
 *          R = 0 ⇔ 流形局部等距于欧氏空间（平坦流形）。
 *
 * @note 输入: PRESET_TYPE_MANIFOLD | 输出: PRESET_TYPE_TENSOR | 复杂度: O(n^4)
 */
#define PRESET_DG_CURVATURE_TENSOR "curvature_tensor"

/**
 * @brief Gauss-Bonnet 定理
 *
 * @details 计算闭曲面上的 Gauss-Bonnet 积分：
 *          integral_M K dA = 2*pi*chi(M)，其中 chi(M) 为 Euler 示性数。
 *          将曲面的内蕴几何量（高斯曲率积分）与拓扑不变量（Euler 示性数）联系起来。
 *
 * @note 输入: PRESET_TYPE_SURFACE | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n^2)
 */
#define PRESET_DG_GAUSS_BONNET "gauss_bonnet"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有微分几何进阶预设函数块
 *
 * 将微分几何进阶模块的全部8个预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_differential_geometry_adv_register(void);

/**
 * @brief 获取微分几何进阶预设函数块数量
 *
 * @return int 微分几何进阶模块预设函数块总数（8）
 */
int preset_differential_geometry_adv_count(void);

#ifdef __cplusplus
}
#endif

#endif /* PRESET_DIFFERENTIAL_GEOMETRY_ADV_H */
