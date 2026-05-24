/**
 * @file preset_dynamical_systems.h
 * @brief 动力系统预设函数块 - 头文件
 *
 * @details 提供动力系统相关的预设函数块，包括：
 *          - 稳定性分析（Lyapunov、渐近稳定性、指数稳定性）
 *          - 分岔分析（鞍点分岔、Hopf分岔、跨临界分岔）
 *          - 极限环与周期解
 *          - 混沌与奇怪吸引子
 *          - 不变流形与中心流形
 *          - 摄动理论与渐近方法
 *
 * @module DynamicalSystems
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 */

#ifndef LV00_PRESET_DYNAMICAL_SYSTEMS_H
#define LV00_PRESET_DYNAMICAL_SYSTEMS_H

#include "func_block_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义 - 稳定性分析
 * ============================================================ */

/** Lyapunov 直接法稳定性判定 */
#define PRESET_DS_LYAPUNOV_DIRECT "ds_lyapunov_direct"

/** 渐近稳定性判定（线性化） */
#define PRESET_DS_ASYMPTOTIC_LINEAR "ds_asymptotic_linear"

/** 指数稳定性判定 */
#define PRESET_DS_EXPONENTIAL_STABILITY "ds_exponential_stability"

/** 中心流形约化 */
#define PRESET_DS_CENTER_MANIFOLD "ds_center_manifold"

/* ============================================================
 * 预设名称常量定义 - 分岔分析
 * ============================================================ */

/** 鞍点分岔判定（Saddle-Node） */
#define PRESET_DS_SADDLE_NODE "ds_saddle_node"

/** 跨临界分岔判定（Transcritical） */
#define PRESET_DS_TRANSCRITICAL "ds_transcritical"

/** Pitchfork分岔判定 */
#define PRESET_DS_PITCHFORK "ds_pitchfork"

/** Hopf分岔判定 */
#define PRESET_DS_HOPF "ds_hopf"

/** 分岔图计算 */
#define PRESET_DS_BIFURCATION_DIAGRAM "ds_bifurcation_diagram"

/* ============================================================
 * 预设名称常量定义 - 极限环与周期解
 * ============================================================ */

/** Poincaré映射构造 */
#define PRESET_DS_POINCARE_MAP "ds_poincare_map"

/** 极限环稳定性分析 */
#define PRESET_DS_LIMIT_CYCLE_STABILITY "ds_limit_cycle_stability"

/** 周期解求解（谐波平衡） */
#define PRESET_DS_HARMONIC_BALANCE "ds_harmonic_balance"

/** Floquet乘子计算 */
#define PRESET_DS_FLOQUET_MULTIPLIERS "ds_floquet_multipliers"

/* ============================================================
 * 预设名称常量定义 - 混沌与吸引子
 * ============================================================ */

/** Lyapunov指数计算 */
#define PRESET_DS_LYAPUNOV_EXPONENTS "ds_lyapunov_exponents"

/** 混沌判定（Devaney定义） */
#define PRESET_DS_CHAOS_DEVANEY "ds_chaos_devaney"

/** Lorenz吸引子 */
#define PRESET_DS_LORENZ_ATTRACTOR "ds_lorenz_attractor"

/** Henon映射 */
#define PRESET_DS_HENON_MAP "ds_henon_map"

/* ============================================================
 * 预设名称常量定义 - 不变流形
 * ============================================================ */

/** 稳定流形 */
#define PRESET_DS_STABLE_MANIFOLD "ds_stable_manifold"

/** 不稳定流形 */
#define PRESET_DS_UNSTABLE_MANIFOLD "ds_unstable_manifold"

/** 惯性流形 */
#define PRESET_DS_INERTIAL_MANIFOLD "ds_inertial_manifold"

/* ============================================================
 * 预设名称常量定义 - 渐近方法
 * ============================================================ */

/** 方法平均法 */
#define PRESET_DS_METHOD_AVG "ds_method_avg"

/** 多尺度方法 */
#define PRESET_DS_MULTIPLE_SCALES "ds_multiple_scales"

/** 奇异摄动法 */
#define PRESET_DS_SINGULAR_PERTURBATION "ds_singular_perturbation"

/** WKBJ近似 */
#define PRESET_DS_WKBJ "ds_wkbj"

/* ============================================================
 * 模块接口
 * ============================================================ */

/**
 * @brief 注册所有动力系统预设函数块
 *
 * @return true  所有预设注册成功
 * @return false 部分或全部预设注册失败
 */
bool preset_dynamical_systems_register(void);

/**
 * @brief 获取动力系统预设函数块数量
 *
 * @return int 预设数量（固定为 25）
 */
int preset_dynamical_systems_count(void);

/**
 * @brief 获取动力系统预设的类别
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_ANALYSIS
 */
PresetCategory preset_dynamical_systems_category(void);

/**
 * @brief 获取动力系统预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组）
 * @param out_count 输出名称数量
 * @return true  成功获取名称列表
 * @return false 参数为 NULL 或内存分配失败
 */
bool preset_dynamical_systems_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_DYNAMICAL_SYSTEMS_H */
