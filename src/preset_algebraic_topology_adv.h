/**
 * @file preset_algebraic_topology_adv.h
 * @brief 代数拓扑进阶预设函数块 - 头文件
 *
 * 提供理论数学研究项目Lv-00中代数拓扑进阶领域的预设函数块，包括：
 *   - 同伦论：基本群、覆盖空间、万有覆盖
 *   - 同调理论：同调群、上同调群、Mayer-Vietoris序列
 *   - 序列与结构：正合序列、Euler特征数
 *
 * @module AlgebraicTopologyAdv
 * @category PRESET_CATEGORY_TOPOLOGY
 * @version 1.0.0
 * @author Lv-00 开发团队
 */

#ifndef PRESET_ALGEBRAIC_TOPOLOGY_ADV_H
#define PRESET_ALGEBRAIC_TOPOLOGY_ADV_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 同伦论 -------------------- */

/** 基本群：计算拓扑空间 X 在基点 x0 处的基本群 π₁(X, x₀) */
#define PRESET_AT_FUNDAMENTAL_GROUP         "fundamental_group"

/** 覆盖空间：构造或验证覆盖映射 p: Ỹ → Y */
#define PRESET_AT_COVERING_SPACE            "covering_space"

/** 万有覆盖：构造拓扑空间的万有覆盖空间 */
#define PRESET_AT_UNIVERSAL_COVERING        "universal_covering"

/* -------------------- 同调理论 -------------------- */

/** 同调群：计算拓扑空间的奇异同调群 Hₙ(X) */
#define PRESET_AT_HOMOLOGY_GROUP            "homology_group"

/** 上同调群：计算拓扑空间的上同调群 Hⁿ(X) */
#define PRESET_AT_COHOMOLOGY_GROUP          "cohomology_group"

/** Mayer-Vietoris序列：利用空间分解计算同调群 */
#define PRESET_AT_MAYER_VIETORIS_SEQUENCE   "mayer_vietoris_sequence"

/* -------------------- 序列与结构 -------------------- */

/** 正合序列：验证或构造正合序列 */
#define PRESET_AT_EXACT_SEQUENCE             "exact_sequence"

/** Euler特征数：计算拓扑空间的Euler特征数 χ(X) */
#define PRESET_AT_EULER_CHARACTERISTIC      "euler_characteristic"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有代数拓扑进阶预设函数块
 *
 * 将代数拓扑进阶模块的全部8个预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_algebraic_topology_adv_register(void);

/**
 * @brief 获取代数拓扑进阶预设函数块数量
 *
 * @return int 代数拓扑进阶模块预设函数块总数（8）
 */
int preset_algebraic_topology_adv_count(void);

#ifdef __cplusplus
}
#endif

#endif /* PRESET_ALGEBRAIC_TOPOLOGY_ADV_H */
