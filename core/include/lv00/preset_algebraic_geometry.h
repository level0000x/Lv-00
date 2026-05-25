/**
 * @file preset_algebraic_geometry.h
 * @brief 代数几何预设函数块 - 头文件
 *
 * @details 提供代数几何相关的预设函数块，包括：
 *          - 仿射代数集（零点集构造、理想-代数集对应、维数、分解、交）
 *          - 射影空间（射影空间构造、射影代数集、射影闭包、Bezout定理）
 *          - 曲线论（平面曲线、奇点分析、亏格、有理曲线、椭圆曲线）
 *          - 层与上同调（层构造、上同调群、结构层、切层、典范除子、Riemann-Roch定理）
 *
 * @module AlgebraicGeometry
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 2.0.0
 * @author Lv-00 Project
 */

#ifndef LV00_PRESET_ALGEBRAIC_GEOMETRY_H
#define LV00_PRESET_ALGEBRAIC_GEOMETRY_H

#include "func_block_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义 - 仿射代数集（5个）
 * ============================================================ */

/** 仿射代数集构造：由多项式理想构造仿射代数集 V(I) */
#define PRESET_AG_AFFINE_VARIETY_CONSTRUCT "ag_affine_variety_construct"

/** 理想-代数集对应：Hilbert零点定理 V(I(V(J))) = sqrt(J) */
#define PRESET_AG_IDEAL_VARIETY_CORRESPONDENCE "ag_ideal_variety_correspondence"

/** 代数集维数：计算仿射代数集的Krull维数 */
#define PRESET_AG_VARIETY_DIMENSION "ag_variety_dimension"

/** 代数集分解：将代数集分解为不可约分支 */
#define PRESET_AG_VARIETY_DECOMPOSITION "ag_variety_decomposition"

/** 代数集交：计算两个代数集的交集 */
#define PRESET_AG_VARIETY_INTERSECTION "ag_variety_intersection"

/* ============================================================
 * 预设名称常量定义 - 射影空间（4个）
 * ============================================================ */

/** 射影空间构造：构造n维射影空间 P^n */
#define PRESET_AG_PROJECTIVE_SPACE_CONSTRUCT "ag_projective_space_construct"

/** 射影代数集：由齐次多项式构造射影代数集 */
#define PRESET_AG_PROJECTIVE_VARIETY "ag_projective_variety"

/** 射影闭包：计算仿射代数集的射影闭包 */
#define PRESET_AG_PROJECTIVE_CLOSURE "ag_projective_closure"

/** Bezout定理：两个平面曲线的交点数 = 次数之积 */
#define PRESET_AG_BEZOUT_THEOREM "ag_bezout_theorem"

/* ============================================================
 * 预设名称常量定义 - 曲线论（5个）
 * ============================================================ */

/** 平面曲线构造：由方程 f(x,y)=0 构造平面曲线 */
#define PRESET_AG_PLANE_CURVE_CONSTRUCT "ag_plane_curve_construct"

/** 奇点分析：判定曲线的奇点并分类 */
#define PRESET_AG_CURVE_SINGULARITY "ag_curve_singularity"

/** 曲线亏格：计算平面曲线的亏格 g = (d-1)(d-2)/2 - sum(delta_P) */
#define PRESET_AG_CURVE_GENUS "ag_curve_genus"

/** 有理曲线判定：判定曲线是否为有理曲线 */
#define PRESET_AG_RATIONAL_CURVE "ag_rational_curve"

/** 椭圆曲线：由Weierstrass方程 y^2 = x^3 + ax + b 构造椭圆曲线 */
#define PRESET_AG_ELLIPTIC_CURVE "ag_elliptic_curve"

/* ============================================================
 * 预设名称常量定义 - 层与上同调（6个）
 * ============================================================ */

/** 层的构造：构造预层/层 */
#define PRESET_AG_SHEAF_CONSTRUCT "ag_sheaf_construct"

/** 层上同调：计算层的上同调群 H^i(X, F) */
#define PRESET_AG_SHEAF_COHOMOLOGY "ag_sheaf_cohomology"

/** 结构层：构造代数簇上的结构层 O_X */
#define PRESET_AG_STRUCTURE_SHEAF "ag_structure_sheaf"

/** 切层：构造代数簇上的切层 T_X */
#define PRESET_AG_TANGENT_SHEAF "ag_tangent_sheaf"

/** 典范除子：计算代数簇的典范除子 K_X */
#define PRESET_AG_CANONICAL_DIVISOR "ag_canonical_divisor"

/** Riemann-Roch定理：dim L(D) - dim L(K-D) = deg(D) + 1 - g */
#define PRESET_AG_RIEMANN_ROCH "ag_riemann_roch"

/* ============================================================
 * 模块接口
 * ============================================================ */

/**
 * @brief 注册所有代数几何预设函数块
 *
 * 此函数使用统一的 preset_blocks_register_simple() 接口
 * 注册代数几何模块的全部 20 个预设函数块。
 *
 * @return true  所有预设注册成功
 * @return false 部分或全部预设注册失败
 */
bool preset_algebraic_geometry_register(void);

/**
 * @brief 获取代数几何预设函数块数量
 *
 * @return int 预设数量（固定为 20）
 */
int preset_algebraic_geometry_count(void);

/**
 * @brief 获取代数几何预设的类别
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_ALGEBRAIC
 */
PresetCategory preset_algebraic_geometry_category(void);

/**
 * @brief 获取代数几何预设名称列表
 *
 * @param out_names 输出名称数组
 * @param out_count 输出名称数量
 * @return true 成功获取
 * @return false 失败
 */
bool preset_algebraic_geometry_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_ALGEBRAIC_GEOMETRY_H */
