/**
 * @file preset_integral_transforms.c
 * @brief 积分变换预设函数块模块 - 实现
 *
 * 实现理论数学研究中常用的积分变换预设函数块。
 * 涵盖 Laplace 变换、Fourier 变换、Z 变换、小波变换和 Mellin 变换。
 *
 * @module IntegralTransforms
 * @category PRESET_EXT_ANALYSIS_INTEGRAL
 * @version 1.0.0
 * @author Lv-00 开发团队
 */

#include "preset_integral_transforms.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设元数据定义 ==================== */

/**
 * @brief 积分变换预设元数据条目
 *
 * 用于描述单个预设的基本信息，支持查询和文档生成。
 */
typedef struct {
    const char *name;                /* 预设名称 */
    const char *description;         /* 中文描述 */
    PresetExtendedCategory category; /* 扩展类别 */
    int input_count;                 /* 输入端口数 */
    int output_count;                /* 输出端口数 */
} IntegralTransformPresetMeta;

/** 积分变换模块预设元数据数组 */
static const IntegralTransformPresetMeta s_integral_transforms_meta[INTEGRAL_TRANSFORMS_PRESET_COUNT] = {
    /* Laplace 变换 (1-3) */
    {"laplace_transform", "Laplace 变换：F(s) = int_0^inf f(t) e^{-st} dt", PRESET_EXT_ANALYSIS_INTEGRAL, 2, 1},
    {"inverse_laplace_transform", "Laplace 逆变换：f(t) = (1/2*pi*i) int_{c-i*inf}^{c+i*inf} F(s) e^{st} ds",
     PRESET_EXT_ANALYSIS_INTEGRAL, 2, 1},
    {"bilateral_laplace_transform", "双边 Laplace 变换：F(s) = int_{-inf}^{inf} f(t) e^{-st} dt",
     PRESET_EXT_ANALYSIS_INTEGRAL, 2, 1},
    /* Fourier 变换 (4-6) */
    {"fourier_transform", "Fourier 变换：F(w) = int_{-inf}^{inf} f(t) e^{-iwt} dt", PRESET_EXT_ANALYSIS_INTEGRAL, 1, 1},
    {"inverse_fourier_transform", "Fourier 逆变换：f(t) = (1/2*pi) int_{-inf}^{inf} F(w) e^{iwt} dw",
     PRESET_EXT_ANALYSIS_INTEGRAL, 1, 1},
    {"discrete_fourier_transform", "离散 Fourier 变换：X[k] = sum_{n=0}^{N-1} x[n] e^{-i*2*pi*k*n/N}",
     PRESET_EXT_ANALYSIS_INTEGRAL, 2, 1},
    /* Z 变换 (7-8) */
    {"z_transform", "Z 变换：X(z) = sum_{n=0}^{inf} x[n] z^{-n}", PRESET_EXT_ANALYSIS_INTEGRAL, 2, 1},
    {"inverse_z_transform", "Z 逆变换：x[n] = (1/2*pi*i) oint_C X(z) z^{n-1} dz", PRESET_EXT_ANALYSIS_INTEGRAL, 2, 1},
    /* 小波变换 (9-12) */
    {"continuous_wavelet_transform", "连续小波变换：W(a,b) = (1/sqrt(a)) int f(t) psi*((t-b)/a) dt",
     PRESET_EXT_ANALYSIS_INTEGRAL, 3, 1},
    {"discrete_wavelet_transform", "离散小波变换：DWT 多分辨率分析，逐层分解信号", PRESET_EXT_ANALYSIS_INTEGRAL, 2, 1},
    {"wavelet_inverse_transform", "小波逆变换：从系数重构原始信号", PRESET_EXT_ANALYSIS_INTEGRAL, 2, 1},
    {"wavelet_coefficient_energy", "小波系数能量：E = sum |W(a,b)|^2，用于信号分析", PRESET_EXT_ANALYSIS_INTEGRAL, 1,
     1},
    /* Mellin 变换 (13-14) */
    {"mellin_transform", "Mellin 变换：M(s) = int_0^inf f(x) x^{s-1} dx", PRESET_EXT_ANALYSIS_INTEGRAL, 1, 1},
    {"inverse_mellin_transform", "Mellin 逆变换：f(x) = (1/2*pi*i) int_{c-i*inf}^{c+i*inf} M(s) x^{-s} ds",
     PRESET_EXT_ANALYSIS_INTEGRAL, 2, 1}};


/* ==================== 查询接口实现 ==================== */

/**
 * @brief 获取积分变换预设函数块数量
 */
int preset_integral_transforms_count(void) {
    return INTEGRAL_TRANSFORMS_PRESET_COUNT;
}

/**
 * @brief 获取积分变换预设元数据
 *
 * @param index 预设索引（0 到 INTEGRAL_TRANSFORMS_PRESET_COUNT - 1）
 * @return 元数据指针，索引越界返回 NULL
 */
const IntegralTransformPresetMeta *preset_integral_transforms_get_metadata(int index) {
    if (index < 0 || index >= INTEGRAL_TRANSFORMS_PRESET_COUNT) {
        return NULL;
    }
    return &s_integral_transforms_meta[index];
}
