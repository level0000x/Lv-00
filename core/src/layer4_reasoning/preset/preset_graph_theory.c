/**
 * @file preset_graph_theory.c
 * @brief 图论预设函数块 - 实现
 *
 * 实现理论数学研究中常用的图论运算预设函数块。
 * 涵盖图基础、连通性、路径与环、图着色、匹配与覆盖、特殊图及图同构。
 *
 * @module GraphTheory
 * @category PRESET_CATEGORY_GRAPH_THEORY
 * @version 4.0.0
 */

#include "preset_graph_theory.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 图论模块预设函数块总数 */

/* ==================== 内部辅助函数 ==================== */

LV_DECLARE_PRESET_REGISTER(PRESET_CATEGORY_GRAPH_THEORY)

/* ==================== 模块注册实现 ==================== */

bool preset_graph_theory_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：图基础
     * ============================================================ */

    /* -------------------- 图构造 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_CONSTRUCT, "由顶点集 V 和边集 E 构造图 G = (V, E)",
                       PRESET_TYPE_SET, PRESET_TYPE_SET, 2,
                       PRESET_TYPE_GRAPH,
                       "G = (V, E), V = \\{v_1, v_2, \\ldots, v_n\\}, E \\subseteq V \\times V",
                       "O(|V| + |E|)", true, false);

    /* -------------------- 邻接矩阵 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_ADJACENCY_MATRIX, "计算图的邻接矩阵 A(G)，其中 A_{ij} 表示顶点 i 与 j 之间的边数",
                       PRESET_TYPE_GRAPH, 1, PRESET_TYPE_MATRIX,
                       "A(G)_{ij} = \\begin{cases} 1 & \\text{若 } (v_i, v_j) \\in E \\\\ 0 & \\text{否则} \\end{cases}",
                       "O(|V|^2)", true, false);

    /* -------------------- 度序列 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_DEGREE_SEQUENCE, "计算图的度序列，按非递增顺序排列",
                       PRESET_TYPE_GRAPH, 1, PRESET_TYPE_SEQUENCE,
                       "d(G) = (d_1, d_2, \\ldots, d_n), d_1 \\ge d_2 \\ge \\cdots \\ge d_n", "O(|V| + |E|)", true, false);

    /* -------------------- 子图判定 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_SUBGRAPH_TEST, "判定图 H 是否是图 G 的子图（V(H) ⊆ V(G), E(H) ⊆ E(G)）",
                       PRESET_TYPE_GRAPH, PRESET_TYPE_GRAPH, 2,
                       PRESET_TYPE_BOOLEAN, "H \\le G \\Leftrightarrow V(H) \\subseteq V(G) \\wedge E(H) \\subseteq E(G)",
                       "O(|V_H| + |E_H|)", true, false);

    /* -------------------- 补图 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_COMPLEMENT, "计算图的补图 \\bar{G}，其中 \bar{G} 包含 G 中不存在的所有边",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_GRAPH, "\\bar{G} = (V, \\bar{E}), \\bar{E} = \\{uv : u,v \\in V, uv \\notin E, u \\ne v\\}",
                       "O(|V|^2)", true, false);

    /* ============================================================
     * 第二部分：连通性
     * ============================================================ */

    /* -------------------- 连通分量 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_CONNECTED_COMPONENTS, "计算图的所有连通分量（基于BFS/DFS遍历）",
                       PRESET_TYPE_GRAPH, 1, PRESET_TYPE_LIST,
                       "G = C_1 \\cup C_2 \\cup \\cdots \\cup C_k, C_i \\text{ 连通}", "O(|V| + |E|)",
                       true, false);

    /* -------------------- 连通性判定 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_CONNECTIVITY_TEST, "判定无向图是否连通（任意两顶点间存在路径）",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_BOOLEAN,
                       "G \\text{ 连通} \\Leftrightarrow \\forall u,v \\in V, \\exists \\text{ 路径 } u \\to v",
                       "O(|V| + |E|)", true, false);

    /* -------------------- 强连通分量 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_SCC, "计算有向图的所有强连通分量（Kosaraju算法）",
                       PRESET_TYPE_GRAPH, 1, PRESET_TYPE_LIST,
                       "\\text{SCC}(G) = \\{C_1, C_2, \\ldots, C_k\\}, C_i \\text{ 强连通}", "O(|V| + |E|)", true, false);

    /* -------------------- 割点检测 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_ARTICULATION_POINTS, "检测图中的所有割点（关节点），删除后增加连通分量数",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_SET, "v \\text{ 是割点} \\Leftrightarrow c(G - v) > c(G)", "O(|V| + |E|)", true, false);

    /* -------------------- 桥检测 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_BRIDGES, "检测图中的所有桥（割边），删除后增加连通分量数",
                       PRESET_TYPE_GRAPH, 1, PRESET_TYPE_SET, "e \\text{ 是桥} \\Leftrightarrow c(G - e) > c(G)",
                       "O(|V| + |E|)", true, false);

    /* ============================================================
     * 第三部分：路径与环
     * ============================================================ */

    /* -------------------- 最短路径 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_SHORTEST_PATH, "Dijkstra单源最短路径算法，计算从源点到目标点的最短路径",
                       PRESET_TYPE_GRAPH, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, 3,
                       PRESET_TYPE_SEQUENCE,
                       "d(s, t) = \\min\\{\\sum_{e \\in P} w(e) : P \\text{ 是 } s \\to t \\text{ 的路径}\\}",
                       "O((|V| + |E|) \\log |V|)", true, false);

    /* -------------------- 最小生成树 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_MST, "Kruskal最小生成树算法，计算连通加权图的最小生成树",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_GRAPH,
                       "T = \\arg\\min_{T'' \\subseteq E} \\sum_{e \\in T''} w(e), |T''| = |V| - 1",
                       "O(|E| \\log |E|)", true, false);

    /* -------------------- 欧拉路径判定 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_EULER_PATH_TEST, "判定图是否存在欧拉路径或欧拉回路（经过每条边恰好一次）",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_BOOLEAN, "G \\text{ 有欧拉回路} \\Leftrightarrow G \\text{ 连通且所有顶点度为偶数}",
                       "O(|V| + |E|)", true, false);

    /* -------------------- 哈密顿路径判定 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_HAMILTONIAN_TEST, "判定图是否存在哈密顿路径（经过每个顶点恰好一次）",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_BOOLEAN,
                       "G \\text{ 有哈密顿路径} \\Leftrightarrow \\exists P = v_1 v_2 \\cdots v_n, V(P) = V(G)",
                       "O(n! \\cdot n)", true, false);

    /* -------------------- 环检测 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_CYCLE_DETECT, "检测图中是否存在环（基于DFS回溯边检测）",
                       PRESET_TYPE_GRAPH, 1, PRESET_TYPE_BOOLEAN,
                       "G \\text{ 有环} \\Leftrightarrow \\exists v_1, v_2, \\ldots, v_k: v_1 = v_k, |E(P)| = k",
                       "O(|V| + |E|)", true, false);

    /* ============================================================
     * 第四部分：图着色
     * ============================================================ */

    /* -------------------- 色数 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_CHROMATIC_NUMBER, "计算图的色数 chi(G)，即顶点着色所需的最少颜色数",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_INTEGER, "\\chi(G) = \\min\\{k : \\exists \\text{ 合法的 } k \\text{-着色}\\}",
                       "O(2.44^{\\chi(G)} \\cdot |V|)", true, false);

    /* -------------------- 顶点着色 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_VERTEX_COLORING, "贪心顶点着色算法，用 k 种颜色对图的顶点着色",
                       PRESET_TYPE_GRAPH, PRESET_TYPE_INTEGER, 2,
                       PRESET_TYPE_FUNCTION,
                       "f: V \\to \\{1, 2, \\ldots, k\\}, f(u) \\ne f(v) \\text{ 若 } uv \\in E",
                       "O(|V| + |E|)", true, false);

    /* -------------------- 边着色 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_EDGE_COLORING, "贪心边着色算法，用 k 种颜色对图的边着色",
                       PRESET_TYPE_GRAPH, PRESET_TYPE_INTEGER, 2, PRESET_TYPE_FUNCTION,
                       "f: E \\to \\{1, 2, \\ldots, k\\}, f(e_1) \\ne f(e_2) \\text{ 若 } e_1, e_2 \\text{ 相邻}",
                       "O(|V| \\cdot |E|)", true, false);

    /* -------------------- 平面图判定 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_PLANARITY_TEST, "判定图是否是平面图（可在平面上绘制且边不相交）",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_BOOLEAN,
                       "G \\text{ 平面} \\Leftrightarrow |E| \\le 3|V| - 6 \\text{（|V| \\ge 3 时必要条件）}", "O(|V|)", true,
                       false);

    /* -------------------- 四色定理验证 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_FOUR_COLOR_VERIFY, "验证四色定理对给定平面图成立（构造4-着色方案）",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_BOOLEAN, "\\chi(G) \\le 4, \\forall \\text{ 平面图 } G", "O(|V|^2)", true, false);

    /* ============================================================
     * 第五部分：匹配与覆盖
     * ============================================================ */

    /* -------------------- 最大匹配 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_MAXIMUM_MATCHING, "计算图的最大匹配（基于增广路算法）",
                       PRESET_TYPE_GRAPH, 1, PRESET_TYPE_SET,
                       "M^* = \\arg\\max_{M \\subseteq E} |M|, M \\text{ 中边两两不邻}", "O(|V| \\cdot |E|)", true, false);

    /* -------------------- 完美匹配判定 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_PERFECT_MATCHING_TEST, "判定图是否存在完美匹配（每个顶点恰好被一条匹配边覆盖）",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_BOOLEAN, "G \\text{ 有完美匹配} \\Leftrightarrow |M^*| = |V|/2",
                       "O(|V| \\cdot |E|)", true, false);

    /* -------------------- 独立集 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_INDEPENDENT_SET, "计算图的最大独立集（集合中任意两顶点不相邻）",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_SET, "\\alpha(G) = \\max\\{|S| : S \\subseteq V, \\forall u,v \\in S, uv \\notin E\\}",
                       "O(1.1996^{|V|})", true, false);

    /* -------------------- 顶点覆盖 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_VERTEX_COVER, "计算图的最小顶点覆盖（每条边至少有一个端点在覆盖中）",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_SET,
                       "\\tau(G) = \\min\\{|C| : C \\subseteq V, \\forall uv \\in E, u \\in C \\lor v \\in C\\}",
                       "O(1.2738^{\\tau(G)} \\cdot |V|)", true, false);

    /* -------------------- 支配集 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_DOMINATING_SET, "计算图的最小支配集（每个顶点要么在集合中，要么与集合中顶点相邻）",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_SET,
                       "\\gamma(G) = \\min\\{|D| : \\forall v \\in V, v \\in D \\lor \\exists u \\in D, uv \\in E\\}",
                       "O(1.4969^{|V|})", true, false);

    /* ============================================================
     * 第六部分：特殊图
     * ============================================================ */

    /* -------------------- 完全图 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_COMPLETE, "构造 n 阶完全图 K_n（每对顶点之间都有边相连）",
                       PRESET_TYPE_INTEGER, 1,
                       PRESET_TYPE_GRAPH, "K_n = (V, E), |V| = n, |E| = \\binom{n}{2}", "O(n^2)",
                       true, false);

    /* -------------------- 二部图判定 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_BIPARTITE_TEST, "判定图是否是二部图（顶点集可划分为两个独立集）",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_BOOLEAN,
                       "G \\text{ 二部} \\Leftrightarrow \\chi(G) \\le 2 \\Leftrightarrow G \\text{ 无奇数环}", "O(|V| + |E|)",
                       true, false);

    /* -------------------- 树判定 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_TREE_TEST, "判定图是否是树（连通且无环的无向图）",
                       PRESET_TYPE_GRAPH, 1, PRESET_TYPE_BOOLEAN,
                       "G \\text{ 是树} \\Leftrightarrow G \\text{ 连通且 } |E| = |V| - 1", "O(|V| + |E|)", true, false);

    /* -------------------- 平面图对偶 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_DUAL, "计算平面图的对偶图 G*（面与顶点互换）",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_GRAPH, "G^* = (F, E^*), F \\text{ 是 } G \\text{ 的面集}",
                       "O(|V| + |E|)", true, false);

    /* ============================================================
     * 第七部分：图同构
     * ============================================================ */

    /* -------------------- 图同构判定 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_ISOMORPHISM_TEST, "判定两个图是否同构（存在保持邻接关系的双射）",
                       PRESET_TYPE_GRAPH, PRESET_TYPE_GRAPH, 2, PRESET_TYPE_BOOLEAN,
                       "G \\cong H \\Leftrightarrow \\exists \\varphi: V(G) \\to V(H) \\text{ 双射}, "
                       "uv \\in E(G) \\Leftrightarrow \\varphi(u)\\varphi(v) \\in E(H)",
                       "O(|V|! \\cdot |V|^2)", true, false);

    /* -------------------- 自同构群 -------------------- */
    LV_PRESET_REGISTER(success, PRESET_GRAPH_AUTOMORPHISM_GROUP, "计算图的自同构群 Aut(G)，即保持图结构的所有顶点置换",
                       PRESET_TYPE_GRAPH, 1,
                       PRESET_TYPE_GROUP,
                       "\\text{Aut}(G) = \\{\\sigma \\in S_{|V|} : uv \\in E \\Leftrightarrow \\sigma(u)\\sigma(v) \\in E\\}",
                       "O(|V|! \\cdot |V|^2)", true, false);

    /* 返回是否所有预设都注册成功 */
    /* lv_log_info("图论预设注册完成，共 %d 个预设", success_count) */
    return success_count == GRAPH_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取图论预设函数块数量
 *
 * @return int 图论模块预设函数块总数
 */
int preset_graph_theory_count(void) {
    return GRAPH_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取图论预设名称列表
 */
bool preset_graph_theory_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv_malloc(GRAPH_THEORY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        PRESET_GRAPH_CONSTRUCT,
        PRESET_GRAPH_ADJACENCY_MATRIX,
        PRESET_GRAPH_DEGREE_SEQUENCE,
        PRESET_GRAPH_SUBGRAPH_TEST,
        PRESET_GRAPH_COMPLEMENT,
        PRESET_GRAPH_CONNECTED_COMPONENTS,
        PRESET_GRAPH_CONNECTIVITY_TEST,
        PRESET_GRAPH_SCC,
        PRESET_GRAPH_ARTICULATION_POINTS,
        PRESET_GRAPH_BRIDGES,
        PRESET_GRAPH_SHORTEST_PATH,
        PRESET_GRAPH_MST,
        PRESET_GRAPH_EULER_PATH_TEST,
        PRESET_GRAPH_HAMILTONIAN_TEST,
        PRESET_GRAPH_CYCLE_DETECT,
        PRESET_GRAPH_CHROMATIC_NUMBER,
        PRESET_GRAPH_VERTEX_COLORING,
        PRESET_GRAPH_EDGE_COLORING,
        PRESET_GRAPH_PLANARITY_TEST,
        PRESET_GRAPH_FOUR_COLOR_VERIFY,
        PRESET_GRAPH_MAXIMUM_MATCHING,
        PRESET_GRAPH_PERFECT_MATCHING_TEST,
        PRESET_GRAPH_INDEPENDENT_SET,
        PRESET_GRAPH_VERTEX_COVER,
        PRESET_GRAPH_DOMINATING_SET,
        PRESET_GRAPH_COMPLETE,
        PRESET_GRAPH_BIPARTITE_TEST,
        PRESET_GRAPH_TREE_TEST,
        PRESET_GRAPH_DUAL,
        PRESET_GRAPH_ISOMORPHISM_TEST,
        PRESET_GRAPH_AUTOMORPHISM_GROUP,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv_free(&tmp);
            }
            {
                void *tmp = names;
                lv_free(&tmp);
            }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}

PresetCategory preset_graph_theory_category(void) {
    return PRESET_CATEGORY_GRAPH_THEORY;
}
