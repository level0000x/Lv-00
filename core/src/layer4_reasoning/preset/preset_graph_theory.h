/**
 * @file preset_graph_theory.h
 * @brief 图论预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的图论运算预设函数块，包括：
 *   - 图基础：图构造、邻接矩阵、度序列、子图判定、补图
 *   - 连通性：连通分量、连通性判定、强连通分量、割点检测、桥检测
 *   - 路径与环：最短路径、最小生成树、欧拉路径判定、哈密顿路径判定、环检测
 *   - 图着色：色数、顶点着色、边着色、平面图判定、四色定理验证
 *   - 匹配与覆盖：最大匹配、完美匹配判定、独立集、顶点覆盖、支配集
 *   - 特殊图：完全图、二部图判定、树判定、平面图对偶
 *   - 图同构：图同构判定、自同构群
 *
 * @module GraphTheory
 * @category PRESET_CATEGORY_GRAPH_THEORY
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_GRAPH_THEORY_H
#define LV00_PRESET_GRAPH_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 图基础 -------------------- */

/** @brief 图构造：由顶点集和边集构造图结构 */
#define PRESET_GRAPH_CONSTRUCT "graph_construct"

/** @brief 邻接矩阵：计算图的邻接矩阵表示，用于矩阵代数分析 */
#define PRESET_GRAPH_ADJACENCY_MATRIX "graph_adjacency_matrix"

/** @brief 图的度序列：计算各顶点的度及整体度序列 */
#define PRESET_GRAPH_DEGREE_SEQUENCE "graph_degree_sequence"

/** @brief 子图判定：判定图 H 是否是图 G 的子图 */
#define PRESET_GRAPH_SUBGRAPH_TEST "graph_subgraph_test"

/** @brief 补图：计算图的补图，将边与非边互换 */
#define PRESET_GRAPH_COMPLEMENT "graph_complement"

/* -------------------- 连通性 -------------------- */

/** @brief 连通分量：计算图的所有连通分量分解 */
#define PRESET_GRAPH_CONNECTED_COMPONENTS "graph_connected_components"

/** @brief 连通性判定：判定图是否连通 */
#define PRESET_GRAPH_CONNECTIVITY_TEST "graph_connectivity_test"

/** @brief 强连通分量：计算有向图的强连通分量（Kosaraju算法） */
#define PRESET_GRAPH_SCC "graph_strongly_connected_components"

/** @brief 割点检测：检测图中的所有割点（关节点），移除后增加连通分量数 */
#define PRESET_GRAPH_ARTICULATION_POINTS "graph_articulation_points"

/** @brief 桥检测：检测图中的所有桥（割边），移除后断开图的连通性 */
#define PRESET_GRAPH_BRIDGES "graph_bridges"

/* -------------------- 路径与环 -------------------- */

/** @brief 最短路径：Dijkstra单源最短路径算法，计算源点到各顶点的最短距离 */
#define PRESET_GRAPH_SHORTEST_PATH "graph_shortest_path"

/** @brief 最小生成树：Kruskal最小生成树算法，求连通图的最小权生成树 */
#define PRESET_GRAPH_MST "graph_minimum_spanning_tree"

/** @brief 欧拉路径判定：判定图是否存在欧拉路径或欧拉回路 */
#define PRESET_GRAPH_EULER_PATH_TEST "graph_euler_path_test"

/** @brief 哈密顿路径判定：判定图是否存在哈密顿路径（NP完全问题） */
#define PRESET_GRAPH_HAMILTONIAN_TEST "graph_hamiltonian_path_test"

/** @brief 环检测：检测图中是否存在环（Cycle） */
#define PRESET_GRAPH_CYCLE_DETECT "graph_cycle_detection"

/* -------------------- 图着色 -------------------- */

/** @brief 色数：计算图的色数 chi(G)，即正常顶点着色所需的最少颜色数 */
#define PRESET_GRAPH_CHROMATIC_NUMBER "graph_chromatic_number"

/** @brief 顶点着色：贪心顶点着色算法，为每个顶点分配颜色使相邻顶点异色 */
#define PRESET_GRAPH_VERTEX_COLORING "graph_vertex_coloring"

/** @brief 边着色：贪心边着色算法，为每条边分配颜色使相邻边异色 */
#define PRESET_GRAPH_EDGE_COLORING "graph_edge_coloring"

/** @brief 平面图判定：判定图是否是平面图（可在平面上无交叉嵌入） */
#define PRESET_GRAPH_PLANARITY_TEST "graph_planarity_test"

/** @brief 四色定理验证：验证四色定理对给定平面图成立（最多四色足够着色） */
#define PRESET_GRAPH_FOUR_COLOR_VERIFY "graph_four_color_verify"

/* -------------------- 匹配与覆盖 -------------------- */

/** @brief 最大匹配：计算图的最大匹配，即最大基数的不共边边集 */
#define PRESET_GRAPH_MAXIMUM_MATCHING "graph_maximum_matching"

/** @brief 完美匹配判定：判定图是否存在完美匹配（覆盖所有顶点） */
#define PRESET_GRAPH_PERFECT_MATCHING_TEST "graph_perfect_matching_test"

/** @brief 独立集：计算图的最大独立集，即两两不相邻的最大顶点集 */
#define PRESET_GRAPH_INDEPENDENT_SET "graph_maximum_independent_set"

/** @brief 顶点覆盖：计算图的最小顶点覆盖，覆盖所有边的最小顶点集 */
#define PRESET_GRAPH_VERTEX_COVER "graph_minimum_vertex_cover"

/** @brief 支配集：计算图的最小支配集，使每个顶点都在支配集中或与之相邻 */
#define PRESET_GRAPH_DOMINATING_SET "graph_minimum_dominating_set"

/* -------------------- 特殊图 -------------------- */

/** @brief 完全图：构造 n 阶完全图 K_n，所有顶点两两相连 */
#define PRESET_GRAPH_COMPLETE "graph_complete_graph"

/** @brief 二部图判定：判定图是否是二部图（可划分为两个独立集） */
#define PRESET_GRAPH_BIPARTITE_TEST "graph_bipartite_test"

/** @brief 树判定：判定图是否是树（连通且无环） */
#define PRESET_GRAPH_TREE_TEST "graph_tree_test"

/** @brief 平面图对偶：计算平面图的对偶图 */
#define PRESET_GRAPH_DUAL "graph_planar_dual"

/* -------------------- 图同构 -------------------- */

/** @brief 图同构判定：判定两个图是否同构（存在保持邻接关系的双射） */
#define PRESET_GRAPH_ISOMORPHISM_TEST "graph_isomorphism_test"

/** @brief 自同构群：计算图的自同构群 Aut(G)，即图到自身的同构映射集合 */
#define PRESET_GRAPH_AUTOMORPHISM_GROUP "graph_automorphism_group"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有图论预设函数块
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_graph_theory_register(void);

/**
 * @brief 获取图论预设函数块数量
 *
 * @return int 图论模块预设函数块总数
 */
int preset_graph_theory_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_GRAPH_THEORY_H */
