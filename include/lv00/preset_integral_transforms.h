/**
 * @file preset_integral_transforms.h
 * @brief 积分变换预设函数块 - 接口定义
 *
 * 提供理论数学研究中常用的积分变换运算预设函数块，包括：
 *   - 傅里叶分析：傅里叶级数、傅里叶变换、离散傅里叶变换（DFT）、
 *                快速傅里叶变换（FFT）、逆傅里叶变换、傅里叶余弦/正弦变换
 *   - 拉普拉斯变换：拉普拉斯变换、逆拉普拉斯变换、卷积定理、
 *                   传递函数、初值定理/终值定理
 *   - Z变换：Z变换、逆Z变换、Z域卷积
 *   - 其他变换：梅林变换、希尔伯特变换、小波变换
 *   - 卷积运算：连续卷积、离散卷积、循环卷积
 *
 * 共19个预设函数块，均遵循模块化、确定性原则，
 * 使用统一的 preset_blocks_register_simple 注册接口。
 *
 * @module IntegralTransforms
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_INTEGRAL_TRANSFORMS_H
#define LV00_PRESET_INTEGRAL_TRANSFORMS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 傅里叶分析（6个） -------------------- */

/** 傅里叶级数：将周期函数展开为正弦/余弦级数 */
#define PRESET_IT_FOURIER_SERIES          "it_fourier_series"

/** 傅里叶变换：将时域信号变换到频域 */
#define PRESET_IT_FOURIER_TRANSFORM       "it_fourier_transform"

/** 逆傅里叶变换：将频域信号恢复到时域 */
#define PRESET_IT_INVERSE_FOURIER         "it_inverse_fourier"

/** 离散傅里叶变换（DFT）：有限序列的离散频谱分析 */
#define PRESET_IT_DFT                     "it_dft"

/** 快速傅里叶变换（FFT）：O(N log N) 计算 DFT */
#define PRESET_IT_FFT                     "it_fft"

/** 傅里叶余弦/正弦变换：对称扩展后的傅里叶变换特例 */
#define PRESET_IT_FOURIER_SINE_COSINE     "it_fourier_sine_cosine"

/* -------------------- 拉普拉斯变换（5个） -------------------- */

/** 拉普拉斯变换：将时域函数变换到复频域 */
#define PRESET_IT_LAPLACE_TRANSFORM       "it_laplace_transform"

/** 逆拉普拉斯变换：复频域到时域的复原 */
#define PRESET_IT_INVERSE_LAPLACE         "it_inverse_laplace"

/** 卷积定理：时域卷积对应频域乘积 L{f*g} = F(s)G(s) */
#define PRESET_IT_CONVOLUTION_THEOREM     "it_convolution_theorem"

/** 传递函数：系统冲激响应的拉普拉斯变换 H(s) */
#define PRESET_IT_TRANSFER_FUNCTION       "it_transfer_function"

/** 初值定理/终值定理：由复频域获取初值和稳态值 */
#define PRESET_IT_INITIAL_FINAL_THEOREM   "it_initial_final_theorem"

/* -------------------- Z变换（3个） -------------------- */

/** Z变换：离散时间信号的复频域表示 */
#define PRESET_IT_Z_TRANSFORM             "it_z_transform"

/** 逆Z变换：由Z域恢复离散序列 */
#define PRESET_IT_INVERSE_Z               "it_inverse_z"

/** Z域卷积：离散序列卷积的Z域乘积表示 */
#define PRESET_IT_Z_CONVOLUTION           "it_z_convolution"

/* -------------------- 其他变换（3个） -------------------- */

/** 梅林变换：M{f}(s) = \int_0^\infty x^{s-1} f(x) dx，乘性傅里叶变换 */
#define PRESET_IT_MELLIN_TRANSFORM        "it_mellin_transform"

/** 希尔伯特变换：90度相移积分变换，构造解析信号 */
#define PRESET_IT_HILBERT_TRANSFORM       "it_hilbert_transform"

/** 小波变换：多分辨率时频分析，CWT 连续小波变换 */
#define PRESET_IT_WAVELET_TRANSFORM       "it_wavelet_transform"

/* -------------------- 卷积运算（2个） -------------------- */

/** 连续卷积：(f*g)(t) = \int f(\tau)g(t-\tau) d\tau */
#define PRESET_IT_CONTINUOUS_CONVOLUTION  "it_continuous_convolution"

/** 离散/循环卷积：有限长度序列的卷积或循环卷积 */
#define PRESET_IT_DISCRETE_CONVOLUTION    "it_discrete_convolution"

/* ============================================================
 * 预设数量
 * ============================================================ */

/** 积分变换模块预设函数块总数 */
#define INTEGRAL_TRANSFORMS_PRESET_COUNT 19

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有积分变换预设函数块
 *
 * 将积分变换模块的全部19个预设函数块注册到全局预设库中。
 * 涵盖傅里叶分析(6)、拉普拉斯变换(5)、Z变换(3)、其他变换(3)、卷积运算(2)。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_integral_transforms_register(void);

/**
 * @brief 获取积分变换预设函数块数量
 *
 * @return int 积分变换模块预设函数块总数（19）
 */
int preset_integral_transforms_count(void);

/**
 * @brief 获取积分变换预设的类别
 *
 * @return PresetCategory 预设类别（PRESET_CATEGORY_ANALYSIS）
 */
PresetCategory preset_integral_transforms_category(void);

/**
 * @brief 获取积分变换预设名称列表
 *
 * 返回堆分配的预设名称数组，调用者需释放每个元素和数组本身。
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败（out_names 或 out_count 为 NULL，或内存不足）
 */
bool preset_integral_transforms_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_INTEGRAL_TRANSFORMS_H */
