/**
 * @file preset_arithmetic_geometry.h
 * @brief 算术几何预设函数块 - 头文件
 *
 * @details 提供算术几何相关的预设函数块，包括：
 *          - 椭圆曲线与模形式
 *          - Diophantine方程
 *          - 代数数论基础
 *          - 类域论基础
 *          - p-adic分析
 *          - 代数簇的有理点
 *
 * @module ArithmeticGeometry
 * @category PRESET_CATEGORY_NUMBER_THEORY
 * @version 1.0.0
 */

#ifndef LV00_PRESET_ARITHMETIC_GEOMETRY_H
#define LV00_PRESET_ARITHMETIC_GEOMETRY_H

#include "func_block_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义 - 椭圆曲线
 * ============================================================ */

/** Weierstrass 标准形式 */
#define PRESET_AG_WEIERSTRASS_FORM "ag_weierstrass_form"

/** 椭圆曲线群运算 */
#define PRESET_AG_ELLIPTIC_ADD "ag_elliptic_add"

/** 椭圆曲线点加倍 */
#define PRESET_AG_ELLIPTIC_DOUBLE "ag_elliptic_double"

/** 椭圆曲线标量乘法 */
#define PRESET_AG_ELLIPTIC_SCALAR "ag_elliptic_scalar"

/** 椭圆曲线判别式与j不变量 */
#define PRESET_AG_ELLIPTIC_INVARIANTS "ag_elliptic_invariants"

/** 椭圆曲线 torsion 点 */
#define PRESET_AG_ELLIPTIC_TORSION "ag_elliptic_torsion"

/* ============================================================
 * 预设名称常量定义 - 模形式
 * ============================================================ */

/** 模群作用 */
#define PRESET_AG_MODULAR_GROUP "ag_modular_group"

/** Eisenstein 级数 */
#define PRESET_AG_EISENSTEIN_SERIES "ag_eisenstein_series"

/** 模判别式 Δ */
#define PRESET_AG_MODULAR_DISCRIMINANT "ag_modular_discriminant"

/** j-不变量 */
#define PRESET_AG_J_INVARIANT "ag_j_invariant"

/* ============================================================
 * 预设名称常量定义 - Diophantine方程
 * ============================================================ */

/** Pell 方程求解 */
#define PRESET_AG_PELL_EQUATION "ag_pell_equation"

/** Thue 方程 */
#define PRESET_AG_THUE_EQUATION "ag_thue_equation"

/** Mordell 方程 */
#define PRESET_AG_MORDELL_EQUATION "ag_mordell_equation"

/** Fermat 方程判定 */
#define PRESET_AG_FERMAT_EQUATION "ag_fermat_equation"

/* ============================================================
 * 预设名称常量定义 - 代数数论
 * ============================================================ */

/** 代数整数环 */
#define PRESET_AG_INTEGER_RING "ag_integer_ring"

/** 理想类群 */
#define PRESET_AG_CLASS_GROUP "ag_class_group"

/** 单位群 */
#define PRESET_AG_UNIT_GROUP "ag_unit_group"

/** Dedekind zeta 函数 */
#define PRESET_AG_DEDEKIND_ZETA "ag_dedekind_zeta"

/* ============================================================
 * 预设名称常量定义 - p-adic分析
 * ============================================================ */

/** p-adic 赋值 */
#define PRESET_AG_PADIC_VALUATION "ag_padic_valuation"

/** p-adic 范数 */
#define PRESET_AG_PADIC_NORM "ag_padic_norm"

/** Hensel 引理 */
#define PRESET_AG_HENSEL_LEMMA "ag_hensel_lemma"

/** p-adic 数域扩张 */
#define PRESET_AG_PADIC_EXTENSION "ag_padic_extension"

/* ============================================================
 * 预设名称常量定义 - 有理点
 * ============================================================ */

/** 有理点高度 */
#define PRESET_AG_HEIGHT_FUNCTION "ag_height_function"

/** Mordell-Weil 定理 */
#define PRESET_AG_MORDELL_WEIL "ag_mordell_weil"

/** Faltings 定理（Mordell 猜想） */
#define PRESET_AG_FALTINGS "ag_faltings"

/** Chabauty 方法 */
#define PRESET_AG_CHABAUTY "ag_chabauty"

/* ============================================================
 * 模块接口
 * ============================================================ */

/**
 * @brief 注册所有算术几何预设函数块
 *
 * @return true  所有预设注册成功
 * @return false 部分或全部预设注册失败
 */
bool preset_arithmetic_geometry_register(void);

/**
 * @brief 获取算术几何预设函数块数量
 *
 * @return int 预设数量（固定为 25）
 */
int preset_arithmetic_geometry_count(void);

/**
 * @brief 获取算术几何预设的类别
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_NUMBER_THEORY
 */
PresetCategory preset_arithmetic_geometry_category(void);

/**
 * @brief 获取算术几何预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组）
 * @param out_count 输出名称数量
 * @return true  成功获取名称列表
 * @return false 参数为 NULL 或内存分配失败
 */
bool preset_arithmetic_geometry_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_ARITHMETIC_GEOMETRY_H */
