/**
 * @file preset_difference_equations.h
 * @brief 差分方程预设函数块 - 头文件
 *
 * @details 提供差分方程相关的预设函数块，包括：
 *          - 线性差分方程（齐次、非齐次、特征方程法、广义Fibonacci、方程组、稳定性）
 *          - 非线性差分方程（Riccati差分方程、Logistic映射、Mandelbrot迭代、Lyapunov指数）
 *          - Z变换（Z变换、逆Z变换、传递函数、Z域稳定性、频率响应）
 *          - 差分方程应用（有限差分法、递推关系求解、组合计数递推）
 *
 * 注意：宏名前缀使用 DE_DIFF_ 而非 DE_，以避免与微分方程模块冲突。
 *
 * @module DifferenceEquations
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 * @author Lv-00 Project
 */

#ifndef LV00_PRESET_DIFFERENCE_EQUATIONS_H
#define LV00_PRESET_DIFFERENCE_EQUATIONS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义 - 线性差分方程（6个）
 * ============================================================ */

/** 线性齐次差分方程：a_n y_{n+k} + ... + a_0 y_n = 0 */
#define PRESET_DE_DIFF_LINEAR_HOMOGENEOUS "de_diff_linear_homogeneous"

/** 线性非齐次差分方程：特解+通解 */
#define PRESET_DE_DIFF_LINEAR_NONHOMOGENEOUS "de_diff_linear_nonhomogeneous"

/** 特征方程法：求解常系数线性差分方程 */
#define PRESET_DE_DIFF_CHARACTERISTIC_EQUATION "de_diff_characteristic_equation"

/** 广义Fibonacci数列：F_n = aF_{n-1} + bF_{n-2} */
#define PRESET_DE_DIFF_FIBONACCI_GENERALIZED "de_diff_fibonacci_generalized"

/** 线性差分方程组：矩阵形式求解 */
#define PRESET_DE_DIFF_SYSTEM_LINEAR "de_diff_system_linear"

/** 离散稳定性分析：判定差分方程的稳定性 */
#define PRESET_DE_DIFF_STABILITY_DISCRETE "de_diff_stability_discrete"

/* ============================================================
 * 预设名称常量定义 - 非线性差分方程（4个）
 * ============================================================ */

/** Riccati差分方程：y_{n+1} = (a*y_n + b)/(c*y_n + d) */
#define PRESET_DE_DIFF_RICCATI_DIFFERENCE "de_diff_riccati_difference"

/** Logistic映射：x_{n+1} = rx_n(1-x_n)，混沌分析 */
#define PRESET_DE_DIFF_LOGISTIC_MAP "de_diff_logistic_map"

/** Mandelbrot迭代：z_{n+1} = z_n^2 + c */
#define PRESET_DE_DIFF_MANDELBROT_ITERATION "de_diff_mandelbrot_iteration"

/** 离散Lyapunov指数：判定混沌行为 */
#define PRESET_DE_DIFF_LYAPUNOV_EXPONENT_DISCRETE "de_diff_lyapunov_exponent_discrete"

/* ============================================================
 * 预设名称常量定义 - Z变换（5个）
 * ============================================================ */

/** Z变换：Z{f_n} = sum f_n z^(-n) */
#define PRESET_DE_DIFF_Z_TRANSFORM "de_diff_z_transform"

/** 逆Z变换：部分分式展开法 */
#define PRESET_DE_DIFF_INVERSE_Z_TRANSFORM "de_diff_inverse_z_transform"

/** Z传递函数：H(z) = Y(z)/X(z) */
#define PRESET_DE_DIFF_Z_TRANSFER_FUNCTION "de_diff_z_transfer_function"

/** Z域稳定性：极点在单位圆内 */
#define PRESET_DE_DIFF_Z_STABILITY "de_diff_z_stability"

/** Z域频率响应：H(e^(j*omega)) */
#define PRESET_DE_DIFF_Z_FREQUENCY_RESPONSE "de_diff_z_frequency_response"

/* ============================================================
 * 预设名称常量定义 - 差分方程应用（3个）
 * ============================================================ */

/** 有限差分法：用差分近似微分 */
#define PRESET_DE_DIFF_FINITE_DIFFERENCE "de_diff_finite_difference"

/** 递推关系求解：生成函数法 */
#define PRESET_DE_DIFF_RECURRENCE_SOLVE "de_diff_recurrence_solve"

/** 组合计数递推：Catalan数、Stirling数等 */
#define PRESET_DE_DIFF_COMBINATORIAL_RECURRENCE "de_diff_combinatorial_recurrence"

/* ============================================================
 * 模块接口
 * ============================================================ */

/**
 * @brief 注册所有差分方程预设函数块
 *
 * 此函数使用统一的 preset_blocks_register_simple() 接口
 * 注册差分方程模块的全部 18 个预设函数块。
 *
 * @return true  所有预设注册成功
 * @return false 部分或全部预设注册失败
 */
bool preset_difference_equations_register(void);

/**
 * @brief 获取差分方程预设函数块数量
 *
 * @return int 预设数量（固定为 18）
 */
int preset_difference_equations_count(void);

/**
 * @brief 获取差分方程预设的类别
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_ANALYSIS
 */
PresetCategory preset_difference_equations_category(void);

/**
 * @brief 获取差分方程预设名称列表
 *
 * @param out_names 输出名称数组
 * @param out_count 输出名称数量
 * @return true 成功获取
 * @return false 失败
 */
bool preset_difference_equations_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_DIFFERENCE_EQUATIONS_H */
