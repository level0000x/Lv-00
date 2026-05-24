/**
 * @file preset_topology.h
 * @brief 拓扑学预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的拓扑学运算预设函数块，包括：
 *   - 拓扑空间基础：拓扑判定、开集判定、闭集判定、闭包计算、内部计算、边界计算、邻域判定、邻域系计算、基判定、子基判定、由基生成拓扑
 *   - 连续映射：连续映射判定、开映射判定、闭映射判定、同胚判定、嵌入判定、商拓扑构造、积拓扑构造、子空间拓扑
 *   - 分离公理：T0(Kolmogorov)空间判定、T1(Fréchet)空间判定、T2(Hausdorff)空间判定、T3(正则)空间判定、T4(正规)空间判定、分离公理完整检查
 *   - 紧致性：紧致空间判定、列紧空间判定、局部紧致判定、紧致化、单点紧致化、开覆盖计算、有限子覆盖
 *   - 连通性：连通空间判定、道路连通判定、连通分支、道路连通分支、局部连通判定、完全不连通判定
 *   - 基本群：同伦判定、道路同伦判定、基本群计算、道路类乘法、单连通判定、覆盖空间构造、提升存在性判定
 *   - 特殊拓扑空间：离散拓扑构造、平凡拓扑构造、度量拓扑构造、序拓扑构造
 *
 * @module Topology
 * @category PRESET_CATEGORY_TOPOLOGY
 * @version 3.2.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_TOPOLOGY_H
#define LV00_PRESET_TOPOLOGY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 拓扑空间基础 -------------------- */

/** 拓扑判定：判定集合族是否构成拓扑 */
#define PRESET_TOPOLOGY_TEST "topology_test"

/** 开集判定 */
#define PRESET_OPEN_SET_TEST "open_set_test"

/** 闭集判定 */
#define PRESET_CLOSED_SET_TEST "closed_set_test"

/** 闭包计算 */
#define PRESET_CLOSURE "closure"

/** 内部计算 */
#define PRESET_INTERIOR "interior"

/** 边界计算 */
#define PRESET_BOUNDARY "boundary"

/** 邻域判定 */
#define PRESET_NEIGHBORHOOD_TEST "neighborhood_test"

/** 邻域系计算 */
#define PRESET_NEIGHBORHOOD_SYSTEM "neighborhood_system"

/** 基判定 */
#define PRESET_BASE_TEST "base_test"

/** 子基判定 */
#define PRESET_SUBBASE_TEST "subbase_test"

/** 由基生成拓扑 */
#define PRESET_TOPOLOGY_FROM_BASE "topology_from_base"

/* -------------------- 连续映射 -------------------- */

/** 连续映射判定 */
#define PRESET_CONTINUOUS_MAP_TEST "continuous_map_test"

/** 开映射判定 */
#define PRESET_OPEN_MAP_TEST "open_map_test"

/** 闭映射判定 */
#define PRESET_CLOSED_MAP_TEST "closed_map_test"

/** 同胚判定 */
#define PRESET_HOMEOMORPHISM_TEST "homeomorphism_test"

/** 嵌入判定 */
#define PRESET_EMBEDDING_TEST "embedding_test"

/** 商拓扑构造 */
#define PRESET_QUOTIENT_TOPOLOGY "quotient_topology"

/** 积拓扑构造 */
#define PRESET_PRODUCT_TOPOLOGY "product_topology"

/** 子空间拓扑 */
#define PRESET_SUBSPACE_TOPOLOGY "subspace_topology"

/* -------------------- 分离公理 -------------------- */

/** T0 (Kolmogorov) 空间判定 */
#define PRESET_T0_SPACE_TEST "t0_space_test"

/** T1 (Fréchet) 空间判定 */
#define PRESET_T1_SPACE_TEST "t1_space_test"

/** T2 (Hausdorff) 空间判定 */
#define PRESET_T2_SPACE_TEST "t2_space_test"

/** T3 (正则) 空间判定 */
#define PRESET_T3_SPACE_TEST "t3_space_test"

/** T4 (正规) 空间判定 */
#define PRESET_T4_SPACE_TEST "t4_space_test"

/** 分离公理完整检查 */
#define PRESET_SEPARATION_AXIOMS "separation_axioms"

/* -------------------- 紧致性 -------------------- */

/** 紧致空间判定 */
#define PRESET_COMPACT_SPACE_TEST "compact_space_test"

/** 列紧空间判定 */
#define PRESET_SEQUENTIALLY_COMPACT "sequentially_compact"

/** 局部紧致判定 */
#define PRESET_LOCALLY_COMPACT_TEST "locally_compact_test"

/** 紧致化 */
#define PRESET_COMPACTIFICATION "compactification"

/** 单点紧致化 */
#define PRESET_ONE_POINT_COMPACTIFICATION "one_point_compactification"

/** 开覆盖计算 */
#define PRESET_OPEN_COVER "open_cover"

/** 有限子覆盖 */
#define PRESET_FINITE_SUBCOVER "finite_subcover"

/* -------------------- 连通性 -------------------- */

/** 连通空间判定 */
#define PRESET_CONNECTED_SPACE_TEST "connected_space_test"

/** 道路连通判定 */
#define PRESET_PATH_CONNECTED_TEST "path_connected_test"

/** 连通分支 */
#define PRESET_CONNECTED_COMPONENT "connected_component"

/** 道路连通分支 */
#define PRESET_PATH_COMPONENT "path_component"

/** 局部连通判定 */
#define PRESET_LOCALLY_CONNECTED_TEST "locally_connected_test"

/** 完全不连通判定 */
#define PRESET_TOTALLY_DISCONNECTED "totally_disconnected"

/* -------------------- 基本群 -------------------- */

/** 同伦判定 */
#define PRESET_HOMOTOPY_TEST "homotopy_test"

/** 道路同伦判定 */
#define PRESET_PATH_HOMOTOPY_TEST "path_homotopy_test"

/** 基本群计算 */
#define PRESET_FUNDAMENTAL_GROUP "fundamental_group"

/** 道路类乘法 */
#define PRESET_PATH_CLASS_MULTIPLY "path_class_multiply"

/** 单连通判定 */
#define PRESET_SIMPLY_CONNECTED_TEST "simply_connected_test"

/** 覆盖空间构造 */
#define PRESET_COVERING_SPACE "covering_space"

/** 提升存在性判定 */
#define PRESET_LIFTING_EXISTENCE "lifting_existence"

/* -------------------- 特殊拓扑空间 -------------------- */

/** 离散拓扑构造 */
#define PRESET_DISCRETE_TOPOLOGY "discrete_topology"

/** 平凡拓扑构造 */
#define PRESET_TRIVIAL_TOPOLOGY "trivial_topology"

/** 度量拓扑构造 */
#define PRESET_METRIC_TOPOLOGY "metric_topology"

/** 序拓扑构造 */
#define PRESET_ORDER_TOPOLOGY "order_topology"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有拓扑学预设函数块
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_topology_register(void);

/**
 * @brief 获取拓扑学预设函数块数量
 *
 * @return int 拓扑学模块预设函数块总数
 */
int preset_topology_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_TOPOLOGY_H */
