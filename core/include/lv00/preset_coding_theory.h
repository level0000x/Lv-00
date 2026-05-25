/**
 * @file preset_coding_theory.h
 * @brief 编码理论预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的编码理论运算预设函数块，包括：
 *   - 线性码：线性码构造、Hamming码、奇偶校验矩阵、伴随式译码、码距、码维数
 *   - 循环码与BCH码：循环码构造、BCH码、Reed-Solomon码、卷积码
 *   - 码的界与性能：Hamming界、Singleton界、Gilbert-Varshamov界、Plotkin界
 *   - 编码应用：纠错能力、重量枚举器、MacWilliams恒等式、Turbo码
 *
 * @module CodingTheory
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_CODING_THEORY_H
#define LV00_PRESET_CODING_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 线性码（6个） -------------------- */

/** 线性码构造：由生成矩阵构造线性码 */
#define PRESET_CT_LINEAR_CODE_CONSTRUCT "ct_linear_code_construct"

/** Hamming码构造：构造Hamming码及其参数 */
#define PRESET_CT_HAMMING_CODE "ct_hamming_code"

/** 奇偶校验矩阵：由生成矩阵计算校验矩阵 */
#define PRESET_CT_PARITY_CHECK "ct_parity_check"

/** 伴随式译码：计算伴随式并译码 */
#define PRESET_CT_SYNDROME_DECODE "ct_syndrome_decode"

/** 码距计算：计算线性码的最小距离 */
#define PRESET_CT_CODE_DISTANCE "ct_code_distance"

/** 码的维数：计算线性码的维数和信息率 */
#define PRESET_CT_CODE_DIMENSION "ct_code_dimension"

/* -------------------- 循环码与BCH码（4个） -------------------- */

/** 循环码构造：由生成多项式构造循环码 */
#define PRESET_CT_CYCLIC_CODE_CONSTRUCT "ct_cyclic_code_construct"

/** BCH码构造：构造BCH码及其纠错能力 */
#define PRESET_CT_BCH_CODE_CONSTRUCT "ct_bch_code_construct"

/** Reed-Solomon码构造：构造RS码 */
#define PRESET_CT_REED_SOLOMON_CONSTRUCT "ct_reed_solomon_construct"

/** 卷积码构造：由生成器序列构造卷积码 */
#define PRESET_CT_CONVOLUTIONAL_CODE "ct_convolutional_code"

/* -------------------- 码的界与性能（4个） -------------------- */

/** Hamming界（球填充界）：Σ C(n,t) <= 2^(n-k) */
#define PRESET_CT_HAMMING_BOUND "ct_hamming_bound"

/** Singleton界：d <= n - k + 1（MDS码等号成立） */
#define PRESET_CT_SINGLETON_BOUND "ct_singleton_bound"

/** Gilbert-Varshamov界：存在码的下界 */
#define PRESET_CT_GILBERT_VARSHAMOV_BOUND "ct_gilbert_varshamov_bound"

/** Plotkin界：大距离码的上界 */
#define PRESET_CT_PLOTKIN_BOUND "ct_plotkin_bound"

/* -------------------- 编码应用（4个） -------------------- */

/** 纠错能力：t = floor((d-1)/2) */
#define PRESET_CT_ERROR_CORRECTION_CAPABILITY "ct_error_correction_capability"

/** 重量枚举器：计算码的重量分布 */
#define PRESET_CT_CODE_WEIGHT_ENUMERATOR "ct_code_weight_enumerator"

/** MacWilliams恒等式：对偶码的重量枚举器 */
#define PRESET_CT_MACWILLIAMS_IDENTITY "ct_macwilliams_identity"

/** Turbo码：迭代译码的性能分析 */
#define PRESET_CT_TURBO_CODE "ct_turbo_code"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有编码理论预设函数块
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_coding_theory_register(void);

/**
 * @brief 获取编码理论预设函数块数量
 *
 * @return int 编码理论模块预设函数块总数
 */
int preset_coding_theory_count(void);

/**
 * @brief 获取编码理论预设函数块名称列表
 *
 * @param out_names 输出名称数组指针（调用者负责释放）
 * @param out_count 输出预设数量
 * @return true 获取成功
 * @return false 获取失败
 */
bool preset_coding_theory_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取编码理论模块类别名称
 *
 * @return 类别名称字符串
 */
const char *preset_coding_theory_category(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_CODING_THEORY_H */
