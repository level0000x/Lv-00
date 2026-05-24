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

#ifndef LV00_PRESET_DIFFERENTIAL_GEOMETRY_ADV_H
#define LV00_PRESET_DIFFERENTIAL_GEOMETRY_ADV_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 流形上的结构 -------------------- */

/** 切空间：计算光滑流形 M 在点 p 处的切空间 T_pM */
#define PRESET_DG_TANGENT_SPACE             "dg_adv_tangent_space"

/** Riemann度量：在光滑流形上定义Riemann度量张量 g */
#define PRESET_DG_RIEMANNIAN_METRIC         "dg_adv_riemannian_metric"

/* -------------------- 测地线与连接 -------------------- */

/** 测地线：计算Riemann流形上的测地线方程与解 */
#define PRESET_DG_GEODESIC                  "dg_adv_geodesic"

/** Levi-Civita连接：计算Riemann流形上唯一的无挠度量相容联络 */
#define PRESET_DG_CONNECTION_LEVI_CIVITA    "dg_adv_connection_levi_civita"

/** 指数映射：exp_p: T_pM → M，沿测地线定义的指数映射 */
#define PRESET_DG_EXPONENTIAL_MAP           "dg_adv_exponential_map"

/** 平行移动：沿曲线将切向量平行移动 */
#define PRESET_DG_PARALLEL_TRANSPORT        "dg_adv_parallel_transport"

/* -------------------- 曲率理论 -------------------- */

/** 曲率张量：计算Riemann曲率张量 R(X,Y)Z */
#define PRESET_DG_CURVATURE_TENSOR          "dg_adv_curvature_tensor"

/** Gauss-Bonnet定理：计算闭曲面上的Gauss-Bonnet积分 */
#define PRESET_DG_GAUSS_BONNET              "dg_adv_gauss_bonnet"

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

#endif /* LV00_PRESET_DIFFERENTIAL_GEOMETRY_ADV_H */
