/**
 * @file preset_differential_geometry.h
 * @brief 微分几何预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的微分几何运算预设函数块，包括：
 *   - 曲线理论：参数曲线、弧长、曲率、挠率、Frenet标架
 *   - 曲面理论：参数曲面、基本形式、曲率、法向量、面积
 *   - 测地线：测地线方程、测地距离、测地曲率、指数映射、平行移动
 *   - 张量运算：张量创建、缩并、张量积、协变导数、克里斯托费尔符号
 *   - 流形理论：坐标卡、转移映射、切空间、余切空间
 *
 * @module DifferentialGeometry
 * @category PRESET_CATEGORY_GEOMETRY
 * @version 4.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_DIFFERENTIAL_GEOMETRY_H
#define LV00_PRESET_DIFFERENTIAL_GEOMETRY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 曲线理论 -------------------- */

/** 参数曲线定义 γ(t) = (x(t), y(t), z(t)) */
#define PRESET_CURVE_PARAMETRIC        "curve_parametric"

/** 计算曲线弧长 s = ∫|γ'(t)|dt */
#define PRESET_CURVE_ARC_LENGTH        "curve_arc_length"

/** 计算曲线曲率 κ = |γ'×γ''|/|γ'|³ */
#define PRESET_CURVE_CURVATURE         "curve_curvature"

/** 计算曲线挠率 τ */
#define PRESET_CURVE_TORSION           "curve_torsion"

/** 计算Frenet标架 (T, N, B) */
#define PRESET_CURVE_FRENET_FRAME      "curve_frenet_frame"

/** 计算曲线法向量 */
#define PRESET_CURVE_NORMAL_VECTOR     "curve_normal_vector"

/** 计算曲线副法向量 */
#define PRESET_CURVE_BINORMAL_VECTOR   "curve_binormal_vector"

/** 计算密切圆 */
#define PRESET_CURVE_OSCULATING_CIRCLE "curve_osculating_circle"

/* -------------------- 曲面理论 -------------------- */

/** 参数曲面定义 r(u,v) */
#define PRESET_SURFACE_PARAMETRIC              "surface_parametric"

/** 计算第一基本形式 (E, F, G) */
#define PRESET_SURFACE_FIRST_FUNDAMENTAL       "surface_first_fundamental"

/** 计算第二基本形式 (L, M, N) */
#define PRESET_SURFACE_SECOND_FUNDAMENTAL      "surface_second_fundamental"

/** 计算高斯曲率 K */
#define PRESET_SURFACE_GAUSS_CURVATURE         "surface_gauss_curvature"

/** 计算平均曲率 H */
#define PRESET_SURFACE_MEAN_CURVATURE          "surface_mean_curvature"

/** 计算主曲率 κ₁, κ₂ */
#define PRESET_SURFACE_PRINCIPAL_CURVATURES    "surface_principal_curvatures"

/** 计算曲面法向量 */
#define PRESET_SURFACE_NORMAL                  "surface_normal"

/** 计算曲面面积 */
#define PRESET_SURFACE_AREA                    "surface_area"

/* -------------------- 测地线 -------------------- */

/** 测地线微分方程 */
#define PRESET_GEODESIC_EQUATION       "geodesic_equation"

/** 计算测地线距离 */
#define PRESET_GEODESIC_DISTANCE       "geodesic_distance"

/** 计算测地曲率 */
#define PRESET_GEODESIC_CURVATURE      "geodesic_curvature"

/** 指数映射 */
#define PRESET_EXPONENTIAL_MAP         "exponential_map"

/** 平行移动 */
#define PRESET_PARALLEL_TRANSPORT      "parallel_transport"

/* -------------------- 张量运算 -------------------- */

/** 创建张量 */
#define PRESET_TENSOR_CREATE           "tensor_create"

/** 张量缩并 */
#define PRESET_TENSOR_CONTRACT         "tensor_contract"

/** 张量积 */
#define PRESET_TENSOR_PRODUCT          "tensor_product"

/** 协变导数 */
#define PRESET_COVARIANT_DERIVATIVE    "covariant_derivative"

/** 计算克里斯托费尔符号 Γᵏᵢⱼ */
#define PRESET_CHRISTOFFEL_SYMBOLS     "christoffel_symbols"

/* -------------------- 流形理论 -------------------- */

/** 定义流形坐标卡 */
#define PRESET_MANIFOLD_CHART          "manifold_chart"

/** 计算转移映射 */
#define PRESET_MANIFOLD_TRANSITION_MAP "manifold_transition_map"

/** 计算切空间 */
#define PRESET_MANIFOLD_TANGENT_SPACE  "manifold_tangent_space"

/** 计算余切空间 */
#define PRESET_MANIFOLD_COTANGENT_SPACE "manifold_cotangent_space"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有微分几何预设函数块
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_differential_geometry_register(void);

/**
 * @brief 获取微分几何预设函数块数量
 *
 * @return int 微分几何模块预设函数块总数
 */
int preset_differential_geometry_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_DIFFERENTIAL_GEOMETRY_H */
