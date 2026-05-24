/**
 * @file preset_information_theory.h
 * @brief 信息论预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的信息论运算预设函数块，包括：
 *   - 信息度量：Shannon熵、联合熵、条件熵、互信息、相对熵、交叉熵
 *   - 信道理论：信道容量、BSC、BEC、信道编码定理、信源编码定理
 *   - 率失真理论：率失真函数、失真率函数、量化、数据压缩
 *   - 信息论应用：Kolmogorov复杂度、算法熵、Fano不等式、数据处理不等式、最大熵原理
 *
 * @module InformationTheory
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_INFORMATION_THEORY_H
#define LV00_PRESET_INFORMATION_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 信息度量（6个） -------------------- */

/** Shannon熵：H(X) = -Σ p(x) log₂ p(x) */
#define PRESET_IT_ENTROPY                "it_entropy"

/** 联合熵：H(X,Y) = -Σ p(x,y) log₂ p(x,y) */
#define PRESET_IT_JOINT_ENTROPY          "it_joint_entropy"

/** 条件熵：H(Y|X) = H(X,Y) - H(X) */
#define PRESET_IT_CONDITIONAL_ENTROPY    "it_conditional_entropy"

/** 互信息：I(X;Y) = H(X) - H(X|Y) */
#define PRESET_IT_MUTUAL_INFORMATION     "it_mutual_information"

/** 相对熵（KL散度）：D(P||Q) = Σ p(x) log₂(p(x)/q(x)) */
#define PRESET_IT_RELATIVE_ENTROPY       "it_relative_entropy"

/** 交叉熵：H(P,Q) = -Σ p(x) log₂ q(x) */
#define PRESET_IT_CROSS_ENTROPY          "it_cross_entropy"

/* -------------------- 信道理论（5个） -------------------- */

/** 信道容量：C = max I(X;Y) */
#define PRESET_IT_CHANNEL_CAPACITY       "it_channel_capacity"

/** 二元对称信道：BSC(p)的容量和错误概率 */
#define PRESET_IT_BINARY_SYMMETRIC_CHANNEL "it_binary_symmetric_channel"

/** 二元删除信道：BEC(ε)的容量 */
#define PRESET_IT_BINARY_ERASURE_CHANNEL "it_binary_erasure_channel"

/** 信道编码定理：可达速率判定 */
#define PRESET_IT_CHANNEL_CODING_THEOREM "it_channel_coding_theorem"

/** 信源编码定理：无损压缩极限 */
#define PRESET_IT_SOURCE_CODING_THEOREM  "it_source_coding_theorem"

/* -------------------- 率失真理论（4个） -------------------- */

/** 率失真函数：R(D) = min I(X;X̂) */
#define PRESET_IT_RATE_DISTORTION_FUNCTION "it_rate_distortion_function"

/** 失真率函数：D(R) */
#define PRESET_IT_DISTORTION_RATE_FUNCTION "it_distortion_rate_function"

/** 量化误差分析：标量/矢量量化的率失真性能 */
#define PRESET_IT_QUANTIZATION           "it_quantization"

/** 数据压缩：Huffman编码、算术编码的压缩率 */
#define PRESET_IT_DATA_COMPRESSION       "it_data_compression"

/* -------------------- 信息论应用（5个） -------------------- */

/** Kolmogorov复杂度：字符串的最短描述长度 */
#define PRESET_IT_KOLMOGOROV_COMPLEXITY  "it_kolmogorov_complexity"

/** 算法熵：与Kolmogorov复杂度相关 */
#define PRESET_IT_ALGORITHMIC_ENTROPY    "it_algorithmic_entropy"

/** Fano不等式：H(Pe) + Pe log(|X|-1) >= H(X|Y) */
#define PRESET_IT_FANO_INEQUALITY        "it_fano_inequality"

/** 数据处理不等式：X→Y→Z 则 I(X;Z) <= I(X;Y) */
#define PRESET_IT_DATA_PROCESSING_INEQUALITY "it_data_processing_inequality"

/** 最大熵原理：在约束下最大化熵 */
#define PRESET_IT_ENTROPY_MAXIMIZATION   "it_entropy_maximization"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有信息论预设函数块
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_information_theory_register(void);

/**
 * @brief 获取信息论预设函数块数量
 *
 * @return int 信息论模块预设函数块总数
 */
int preset_information_theory_count(void);

/**
 * @brief 获取信息论预设函数块名称列表
 *
 * @param out_names 输出名称数组指针（调用者负责释放）
 * @param out_count 输出预设数量
 * @return true 获取成功
 * @return false 获取失败
 */
bool preset_information_theory_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取信息论模块类别名称
 *
 * @return 类别名称字符串
 */
const char *preset_information_theory_category(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_INFORMATION_THEORY_H */
