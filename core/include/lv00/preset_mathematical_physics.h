/**
 * @file preset_mathematical_physics.h
 * @brief 数学物理方程预设函数块 - 头文件
 *
 * @details 提供数学物理中核心方程的预设函数块，包括：
 *          - 波动方程（一维/二维/三维）
 *          - 热传导方程（扩散方程）
 *          - Laplace方程与Poisson方程
 *          - Schrödinger方程（定态与含时）
 *          - 波动光学方程
 *          - 电磁场方程（Maxwell方程组）
 *
 * @module MathematicalPhysics
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 */

#ifndef LV00_PRESET_MATHEMATICAL_PHYSICS_H
#define LV00_PRESET_MATHEMATICAL_PHYSICS_H

#include "func_block_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义 - 波动方程
 * ============================================================ */

/** 一维波动方程 d'Alembert解 */
#define PRESET_MP_WAVE_1D_DALEMBERT "mp_wave_1d_dalembert"

/** 二维波动方程 Kirchhoff公式 */
#define PRESET_MP_WAVE_2D_KIRCHHOFF "mp_wave_2d_kirchhoff"

/** 三维波动方程 Poisson公式 */
#define PRESET_MP_WAVE_3D_POISSON "mp_wave_3d_poisson"

/** 波动方程分离变量解 */
#define PRESET_MP_WAVE_SEPARATION "mp_wave_separation"

/* ============================================================
 * 预设名称常量定义 - 热传导方程
 * ============================================================ */

/** 一维热传导方程 Fourier解 */
#define PRESET_MP_HEAT_1D_FOURIER "mp_heat_1d_fourier"

/** 热传导方程基本解（热核） */
#define PRESET_MP_HEAT_FUNDAMENTAL "mp_heat_fundamental"

/** 热传导方程分离变量解 */
#define PRESET_MP_HEAT_SEPARATION "mp_heat_separation"

/** 热传导方程有限差分格式 */
#define PRESET_MP_HEAT_FINITE_DIFF "mp_heat_finite_diff"

/* ============================================================
 * 预设名称常量定义 - 位势方程
 * ============================================================ */

/** Laplace方程分离变量解 */
#define PRESET_MP_LAPLACE_SEPARATION "mp_laplace_separation"

/** Poisson方程 Green函数法 */
#define PRESET_MP_POISSON_GREEN "mp_poisson_green"

/** 调和函数均值性质 */
#define PRESET_MP_HARMONIC_MEAN "mp_harmonic_mean"

/** 极大值原理判定 */
#define PRESET_MP_MAX_PRINCIPLE "mp_max_principle"

/* ============================================================
 * 预设名称常量定义 - 量子力学方程
 * ============================================================ */

/** 定态Schrödinger方程求解 */
#define PRESET_MP_SCHRODINGER_STATIONARY "mp_schrodinger_stationary"

/** 含时Schrödinger方程演化 */
#define PRESET_MP_SCHRODINGER_TIME "mp_schrodinger_time"

/** 一维势阱本征值问题 */
#define PRESET_MP_POTENTIAL_WELL "mp_potential_well"

/** 谐振子本征态 */
#define PRESET_MP_HARMONIC_OSCILLATOR "mp_harmonic_oscillator"

/* ============================================================
 * 预设名称常量定义 - 电磁场方程
 * ============================================================ */

/** Maxwell方程组（时域） */
#define PRESET_MP_MAXWELL_TIME "mp_maxwell_time"

/** 静电场Poisson方程 */
#define PRESET_MP_ELECTROSTATIC "mp_electrostatic"

/** 静磁场Biot-Savart定律 */
#define PRESET_MP_MAGNETOSTATIC "mp_magnetostatic"

/** 电磁波传播方程 */
#define PRESET_MP_EM_WAVE "mp_em_wave"

/* ============================================================
 * 预设名称常量定义 - 流体力学方程
 * ============================================================ */

/** Navier-Stokes方程（不可压缩） */
#define PRESET_MP_NAVIER_STOKES "mp_navier_stokes"

/** Euler方程（理想流体） */
#define PRESET_MP_EULER_FLUID "mp_euler_fluid"

/** 边界层方程 */
#define PRESET_MP_BOUNDARY_LAYER "mp_boundary_layer"

/* ============================================================
 * 模块接口
 * ============================================================ */

/**
 * @brief 注册所有数学物理方程预设函数块
 *
 * 此函数使用统一的 preset_blocks_register_simple() 接口
 * 注册数学物理模块的全部 25 个预设函数块。
 *
 * @return true  所有预设注册成功
 * @return false 部分或全部预设注册失败
 */
bool preset_mathematical_physics_register(void);

/**
 * @brief 获取数学物理预设函数块数量
 *
 * @return int 预设数量（固定为 25）
 */
int preset_mathematical_physics_count(void);

/**
 * @brief 获取数学物理预设的类别
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_ANALYSIS
 */
PresetCategory preset_mathematical_physics_category(void);

/**
 * @brief 获取数学物理预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组）
 * @param out_count 输出名称数量
 * @return true  成功获取名称列表
 * @return false 参数为 NULL 或内存分配失败
 */
bool preset_mathematical_physics_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_MATHEMATICAL_PHYSICS_H */
