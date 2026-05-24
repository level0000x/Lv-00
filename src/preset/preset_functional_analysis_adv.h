/**
 * @file preset_functional_analysis_adv.h
 * @brief 泛函分析进阶预设函数块 - 头文件
 *
 * 提供理论数学研究项目Lv-00中泛函分析进阶领域的预设函数块，包括：
 *   - 空间构造：Banach空间、Hilbert空间
 *   - 算子理论：有界线性算子、谱定理
 *   - 三大基本定理：Hahn-Banach定理、开映射定理、闭图像定理
 *   - 弱拓扑：弱收敛
 *
 * @module FunctionalAnalysisAdv
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 * @author Lv-00 开发团队
 */

#ifndef PRESET_FUNCTIONAL_ANALYSIS_ADV_H
#define PRESET_FUNCTIONAL_ANALYSIS_ADV_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 空间构造 -------------------- */

/** Banach空间构造：验证完备赋范线性空间 */
#define PRESET_FA_BANACH_SPACE              "banach_space"

/** Hilbert空间构造：验证完备内积空间 */
#define PRESET_FA_HILBERT_SPACE             "hilbert_space"

/* -------------------- 算子理论 -------------------- */

/** 有界线性算子：构造或验证有界线性算子及其范数 */
#define PRESET_FA_BOUNDED_OPERATOR          "bounded_operator"

/** 谱定理：自伴紧算子的谱分解 */
#define PRESET_FA_SPECTRAL_THEOREM          "spectral_theorem"

/* -------------------- 三大基本定理 -------------------- */

/** Hahn-Banach定理：有界线性泛函的延拓 */
#define PRESET_FA_HAHN_BANACH_THEOREM       "hahn_banach_theorem"

/** 开映射定理：满射有界线性算子将开集映射为开集 */
#define PRESET_FA_OPEN_MAPPING_THEOREM      "open_mapping_theorem"

/** 闭图像定理：线性算子有界当且仅当其图像闭 */
#define PRESET_FA_CLOSED_GRAPH_THEOREM      "closed_graph_theorem"

/* -------------------- 弱拓扑 -------------------- */

/** 弱收敛：序列在弱拓扑下的收敛性判定 */
#define PRESET_FA_WEAK_CONVERGENCE          "weak_convergence"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有泛函分析进阶预设函数块
 *
 * 将泛函分析进阶模块的全部8个预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_functional_analysis_adv_register(void);

/**
 * @brief 获取泛函分析进阶预设函数块数量
 *
 * @return int 泛函分析进阶模块预设函数块总数（8）
 */
int preset_functional_analysis_adv_count(void);

#ifdef __cplusplus
}
#endif

#endif /* PRESET_FUNCTIONAL_ANALYSIS_ADV_H */
