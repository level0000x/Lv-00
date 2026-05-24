/**
 * @file preset_analysis.h
 * @brief 分析学预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的分析学运算预设函数块，包括：
 *   - 极限运算：序列极限、函数极限、左极限、右极限、无穷极限、上极限、下极限、极限存在性判定
 *   - 连续性：连续性判定、一致连续判定、间断点分类、Lipschitz连续判定
 *   - 微分运算：导数计算、高阶导数、偏导数、方向导数、梯度、散度、旋度、拉普拉斯算子、可微性判定、泰勒展开
 *   - 积分运算：不定积分、定积分、广义积分、重积分、曲线积分、曲面积分、可积性判定、积分中值定理应用
 *   - 级数运算：数项级数收敛判定、绝对收敛判定、条件收敛判定、幂级数收敛半径、级数求和、傅里叶级数
 *   - 函数空间：L^p空间范数、一致范数、完备化、紧致性判定（Arzelà-Ascoli）
 *   - 度量空间：度量空间判定、柯西序列判定、完备度量空间判定、压缩映射、不动点定理
 *   - 特殊函数：Γ函数、B函数、黎曼ζ函数、误差函数
 *
 * @module Analysis
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 3.3.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_ANALYSIS_H
#define LV00_PRESET_ANALYSIS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 极限运算 -------------------- */

/** 序列极限 */
#define PRESET_SEQUENCE_LIMIT "sequence_limit"

/** 函数极限 */
#define PRESET_FUNCTION_LIMIT "function_limit"

/** 左极限 */
#define PRESET_LEFT_LIMIT "left_limit"

/** 右极限 */
#define PRESET_RIGHT_LIMIT "right_limit"

/** 无穷极限 */
#define PRESET_INFINITE_LIMIT "infinite_limit"

/** 上极限 */
#define PRESET_LIMIT_SUPERIOR "limit_superior"

/** 下极限 */
#define PRESET_LIMIT_INFERIOR "limit_inferior"

/** 极限存在性判定 */
#define PRESET_LIMIT_EXISTS_TEST "limit_exists_test"

/* -------------------- 连续性 -------------------- */

/** 连续性判定 */
#define PRESET_CONTINUITY_TEST "continuity_test"

/** 一致连续判定 */
#define PRESET_UNIFORM_CONTINUITY_TEST "uniform_continuity_test"

/** 间断点分类 */
#define PRESET_DISCONTINUITY_CLASSIFY "discontinuity_classify"

/** Lipschitz 连续判定 */
#define PRESET_LIPSCHITZ_TEST "lipschitz_test"

/* -------------------- 微分运算 -------------------- */

/** 导数计算 */
#define PRESET_DERIVATIVE "derivative"

/** 高阶导数 */
#define PRESET_HIGHER_DERIVATIVE "higher_derivative"

/** 偏导数 */
#define PRESET_PARTIAL_DERIVATIVE "partial_derivative"

/** 方向导数 */
#define PRESET_DIRECTIONAL_DERIVATIVE "directional_derivative"

/** 梯度 */
#define PRESET_GRADIENT "gradient"

/** 散度 */
#define PRESET_DIVERGENCE "divergence"

/** 旋度 */
#define PRESET_CURL "curl"

/** 拉普拉斯算子 */
#define PRESET_LAPLACIAN "laplacian"

/** 可微性判定 */
#define PRESET_DIFFERENTIABILITY_TEST "differentiability_test"

/** 泰勒展开 */
#define PRESET_TAYLOR_EXPANSION "taylor_expansion"

/* -------------------- 积分运算 -------------------- */

/** 不定积分 */
#define PRESET_INDEFINITE_INTEGRAL "indefinite_integral"

/** 定积分 */
#define PRESET_DEFINITE_INTEGRAL "definite_integral"

/** 广义积分 */
#define PRESET_IMPROPER_INTEGRAL "improper_integral"

/** 重积分 */
#define PRESET_MULTIPLE_INTEGRAL "multiple_integral"

/** 曲线积分 */
#define PRESET_LINE_INTEGRAL "line_integral"

/** 曲面积分 */
#define PRESET_SURFACE_INTEGRAL "surface_integral"

/** 可积性判定 */
#define PRESET_INTEGRABILITY_TEST "integrability_test"

/** 积分中值定理应用 */
#define PRESET_MEAN_VALUE_THEOREM "mean_value_theorem"

/* -------------------- 级数运算 -------------------- */

/** 数项级数收敛判定 */
#define PRESET_SERIES_CONVERGENCE_TEST "series_convergence_test"

/** 绝对收敛判定 */
#define PRESET_ABSOLUTE_CONVERGENCE "absolute_convergence"

/** 条件收敛判定 */
#define PRESET_CONDITIONAL_CONVERGENCE "conditional_convergence"

/** 幂级数收敛半径 */
#define PRESET_POWER_SERIES_RADIUS "power_series_radius"

/** 级数求和 */
#define PRESET_SERIES_SUM "series_sum"

/** 傅里叶级数 */
#define PRESET_FOURIER_SERIES "fourier_series"

/* -------------------- 函数空间 -------------------- */

/** L^p 空间范数 */
#define PRESET_LP_NORM "lp_norm"

/** 一致范数 */
#define PRESET_SUP_NORM "sup_norm"

/** 完备化 */
#define PRESET_COMPLETION "completion"

/** 紧致性判定（Arzelà-Ascoli） */
#define PRESET_COMPACTNESS_TEST "compactness_test"

/* -------------------- 度量空间 -------------------- */

/** 度量空间判定 */
#define PRESET_METRIC_SPACE_TEST "metric_space_test"

/** 柯西序列判定 */
#define PRESET_CAUCHY_SEQUENCE_TEST "cauchy_sequence_test"

/** 完备度量空间判定 */
#define PRESET_COMPLETE_SPACE_TEST "complete_space_test"

/** 压缩映射 */
#define PRESET_CONTRACTION_MAPPING "contraction_mapping"

/** 不动点定理 */
#define PRESET_FIXED_POINT_THEOREM "fixed_point_theorem"

/* -------------------- 特殊函数 -------------------- */

/** Γ函数 */
#define PRESET_GAMMA_FUNCTION "gamma_function"

/** B函数 */
#define PRESET_BETA_FUNCTION "beta_function"

/** 黎曼ζ函数 */
#define PRESET_ZETA_FUNCTION "zeta_function"

/** 误差函数 */
#define PRESET_ERROR_FUNCTION "error_function"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有分析学预设函数块
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_analysis_register(void);

/**
 * @brief 获取分析学预设函数块数量
 *
 * @return int 分析学模块预设函数块总数
 */
int preset_analysis_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_ANALYSIS_H */
