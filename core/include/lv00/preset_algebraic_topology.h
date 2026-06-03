/**
 * @file preset_algebraic_topology.h
 * @brief 代数拓扑预设函数块 - 接口定义
 *
 * 提供理论数学研究中常用的代数拓扑运算预设函数块，包括：
 *   - 同调论：单纯同调群、奇异同调群、相对同调群、Mayer-Vietoris序列、切除定理、
 *             胞腔同调、Betti数、同调正合序列
 *   - 上同调论：奇异上同调群、上积结构、de Rham上同调、下积、上同调环
 *   - 基本群推广：高阶同伦群、相对同伦群、Hurewicz同态、同伦正合序列、
 *                 Whitehead定理
 *   - 单纯复形：单纯复形构造、三角剖分、Euler示性数、重心重分、单纯逼近
 *
 * 共23个预设函数块，均遵循模块化、确定性原则，
 * 使用统一的 preset_blocks_register_simple 注册接口。
 *
 * @module AlgebraicTopology
 * @category PRESET_CATEGORY_TOPOLOGY
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_ALGEBRAIC_TOPOLOGY_H
#define LV00_PRESET_ALGEBRAIC_TOPOLOGY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 同调论（8个） -------------------- */

/** 单纯同调群：基于单纯复形计算同调群 H_n(K) */
#define PRESET_AT_SIMPLICIAL_HOMOLOGY "at_simplicial_homology"

/** 奇异同调群：计算拓扑空间的奇异同调群 H_n(X) */
#define PRESET_AT_SINGULAR_HOMOLOGY "at_singular_homology"

/** 相对同调群：计算空间对的相对同调群 H_n(X, A) */
#define PRESET_AT_RELATIVE_HOMOLOGY "at_relative_homology"

/** Mayer-Vietoris序列：利用空间覆盖分解计算同调群 */
#define PRESET_AT_MAYER_VIETORIS "at_mayer_vietoris"

/** 切除定理：切除子空间不改变相对同调 */
#define PRESET_AT_EXCISION_THEOREM "at_excision_theorem"

/** 胞腔同调：基于CW复形计算胞腔同调群 */
#define PRESET_AT_CELLULAR_HOMOLOGY "at_cellular_homology"

/** Betti数：计算拓扑空间的Betti数 beta_n = rank H_n(X) */
#define PRESET_AT_BETTI_NUMBERS "at_betti_numbers"

/** 同调正合序列：构造空间对的长正合同调序列 */
#define PRESET_AT_HOMOLOGY_EXACT_SEQUENCE "at_homology_exact_sequence"

/* -------------------- 上同调论（5个） -------------------- */

/** 奇异上同调群：计算拓扑空间的奇异上同调群 H^n(X) */
#define PRESET_AT_SINGULAR_COHOMOLOGY "at_singular_cohomology"

/** 上积结构：计算上同调类的上积运算 */
#define PRESET_AT_CUP_PRODUCT "at_cup_product"

/** de Rham上同调：基于微分形式计算de Rham上同调群 */
#define PRESET_AT_DE_RHAM_COHOMOLOGY "at_de_rham_cohomology"

/** 下积：计算同调类与上同调类的下积运算 */
#define PRESET_AT_CAP_PRODUCT "at_cap_product"

/** 上同调环：构造上同调群的上积环结构 H^*(X; R) */
#define PRESET_AT_COHOMOLOGY_RING "at_cohomology_ring"

/* -------------------- 基本群推广（5个） -------------------- */

/** 高阶同伦群：计算拓扑空间的n阶同伦群 pi_n(X) */
#define PRESET_AT_HIGHER_HOMOTOPY_GROUPS "at_higher_homotopy_groups"

/** 相对同伦群：计算空间对的相对同伦群 pi_n(X, A) */
#define PRESET_AT_RELATIVE_HOMOTOPY "at_relative_homotopy"

/** Hurewicz同态：构造从同伦群到同调群的Hurewicz同态 */
#define PRESET_AT_HUREWICZ_HOMOMORPHISM "at_hurewicz_homomorphism"

/** 同伦正合序列：构造空间对的同伦正合序列 */
#define PRESET_AT_HOMOTOPY_EXACT_SEQUENCE "at_homotopy_exact_sequence"

/** Whitehead定理：判定映射在CW复形上诱导同伦等价 */
#define PRESET_AT_WHITEHEAD_THEOREM "at_whitehead_theorem"

/* -------------------- 单纯复形（5个） -------------------- */

/** 单纯复形构造：由顶点和单形列表构造单纯复形 */
#define PRESET_AT_SIMPLICIAL_COMPLEX "at_simplicial_complex"

/** 三角剖分：对拓扑空间或多面体进行三角剖分 */
#define PRESET_AT_TRIANGULATION "at_triangulation"

/** Euler示性数：计算单纯复形的Euler示性数 chi = sum(-1)^i n_i */
#define PRESET_AT_EULER_CHARACTERISTIC "at_euler_characteristic"

/** 重心重分：计算单纯复形的重心重分 */
#define PRESET_AT_BARYCENTRIC_SUBDIVISION "at_barycentric_subdivision"

/** 单纯逼近：构造连续映射的单纯逼近 */
#define PRESET_AT_SIMPLICIAL_APPROX "at_simplicial_approximation"

/* ============================================================
 * 预设数量
 * ============================================================ */

/** 代数拓扑模块预设函数块总数 */
#define ALGEBRAIC_TOPOLOGY_PRESET_COUNT 23

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有代数拓扑预设函数块
 *
 * 将代数拓扑模块的全部23个预设函数块注册到全局预设库中。
 * 涵盖同调论(8)、上同调论(5)、基本群推广(5)、单纯复形(5)。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_algebraic_topology_register(void);

/**
 * @brief 获取代数拓扑预设函数块数量
 *
 * @return int 代数拓扑模块预设函数块总数（23）
 */
int preset_algebraic_topology_count(void);

/**
 * @brief 获取代数拓扑预设的类别
 *
 * @return PresetCategory 预设类别（PRESET_CATEGORY_TOPOLOGY）
 */
PresetCategory preset_algebraic_topology_category(void);

/**
 * @brief 获取代数拓扑预设名称列表
 *
 * 返回堆分配的预设名称数组，调用者需释放每个元素和数组本身。
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败（out_names 或 out_count 为 NULL，或内存不足）
 */
bool preset_algebraic_topology_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_ALGEBRAIC_TOPOLOGY_H */
