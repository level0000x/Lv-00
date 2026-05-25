/**
 * @file preset_combinatorics.h
 * @brief 组合数学预设函数块
 *
 * 提供理论数学研究中常用的组合数学预设函数块，包括：
 * - 排列组合基础：排列数、组合数、可重排列、可重组合、多项式系数、阶乘、双阶乘
 * - 容斥原理：容斥原理、错排数、满射数量
 * - 递推关系：第一类Stirling数、第二类Stirling数、Bell数、Catalan数、
 *   整数分拆数、Fibonacci数、Lucas数
 * - 图论基础：度数、连通性、树判定、平面图判定、欧拉图判定、
 *   二部图判定、色数、最短路径、最小生成树
 * - 组合恒等式：Vandermonde恒等式、Pascal恒等式、生成函数构造
 *
 * @module Combinatorics
 * @category PRESET_CATEGORY_COMBINATORICS
 */

#ifndef LV00_PRESET_COMBINATORICS_H
#define LV00_PRESET_COMBINATORICS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 排列组合基础 ==================== */

/**
 * @brief 排列数
 *
 * 数学定义：$P(n,k) = \frac{n!}{(n-k)!}$，从 $n$ 个不同元素中取 $k$ 个的有序排列数
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 元素总数
 *   - k: 整数 (PRESET_TYPE_INTEGER) - 选取数量
 * 输出：
 *   - P(n,k): 整数 (PRESET_TYPE_INTEGER) - 排列数
 *
 * 复杂度：O(k)
 */
#define PRESET_PERMUTATION_COUNT "permutation_count"

/**
 * @brief 组合数
 *
 * 数学定义：$C(n,k) = \binom{n}{k} = \frac{n!}{k!(n-k)!}$，从 $n$ 个不同元素中取 $k$ 个的无序组合数
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 元素总数
 *   - k: 整数 (PRESET_TYPE_INTEGER) - 选取数量
 * 输出：
 *   - C(n,k): 整数 (PRESET_TYPE_INTEGER) - 组合数
 *
 * 复杂度：O(min(k, n-k))
 */
#define PRESET_COMBINATION_COUNT "combination_count"

/**
 * @brief 可重排列
 *
 * 数学定义：$n^k$，从 $n$ 个元素中允许重复地取 $k$ 个的有序排列数
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 元素种类数
 *   - k: 整数 (PRESET_TYPE_INTEGER) - 选取数量
 * 输出：
 *   - n^k: 整数 (PRESET_TYPE_INTEGER) - 可重排列数
 *
 * 复杂度：O(log k)
 */
#define PRESET_PERMUTATION_WITH_REPETITION "permutation_with_repetition"

/**
 * @brief 可重组合
 *
 * 数学定义：$\binom{n+k-1}{k} = \frac{(n+k-1)!}{k!(n-1)!}$，从 $n$ 种元素中允许重复地取 $k$ 个的无序组合数
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 元素种类数
 *   - k: 整数 (PRESET_TYPE_INTEGER) - 选取数量
 * 输出：
 *   - C(n+k-1,k): 整数 (PRESET_TYPE_INTEGER) - 可重组合数
 *
 * 复杂度：O(min(k, n-1))
 */
#define PRESET_COMBINATION_WITH_REPETITION "combination_with_repetition"

/**
 * @brief 多项式系数
 *
 * 数学定义：$\binom{n}{k_1, k_2, \ldots, k_r} = \frac{n!}{k_1! \, k_2! \, \cdots \, k_r!}$，
 * 其中 $k_1 + k_2 + \cdots + k_r = n$
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 元素总数
 *   - k_i: 序列 (PRESET_TYPE_SEQUENCE) - 各组元素数量
 * 输出：
 *   - multinomial: 整数 (PRESET_TYPE_INTEGER) - 多项式系数
 *
 * 复杂度：O(n)
 */
#define PRESET_MULTINOMIAL_COEFFICIENT "multinomial_coefficient"

/**
 * @brief 阶乘
 *
 * 数学定义：$n! = \prod_{i=1}^{n} i$，$0! = 1$
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 非负整数
 * 输出：
 *   - n!: 整数 (PRESET_TYPE_INTEGER) - 阶乘值
 *
 * 复杂度：O(n)
 */
#define PRESET_FACTORIAL "factorial"

/**
 * @brief 双阶乘
 *
 * 数学定义：$n!! = n \cdot (n-2) \cdot (n-4) \cdots$，
 * $n!! = \begin{cases} n \cdot (n-2) \cdots 1 & n \text{ 为奇数} \\ n \cdot (n-2) \cdots 2 & n \text{ 为偶数} \end{cases}$
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 非负整数
 * 输出：
 *   - n!!: 整数 (PRESET_TYPE_INTEGER) - 双阶乘值
 *
 * 复杂度：O(n)
 */
#define PRESET_DOUBLE_FACTORIAL "double_factorial"

/* ==================== 容斥原理 ==================== */

/**
 * @brief 容斥原理
 *
 * 数学定义：$\left|\bigcup_{i=1}^{n} A_i\right| = \sum_{k=1}^{n} (-1)^{k+1} \sum_{1 \le i_1 < \cdots < i_k \le n} \left|\bigcap_{j=1}^{k} A_{i_j}\right|$
 *
 * 输入：
 *   - sets: 序列 (PRESET_TYPE_SEQUENCE) - 各集合的元素个数序列
 *   - intersections: 序列 (PRESET_TYPE_SEQUENCE) - 各交集的元素个数序列
 * 输出：
 *   - count: 整数 (PRESET_TYPE_INTEGER) - 并集元素个数
 *
 * 复杂度：O(2^n)
 */
#define PRESET_INCLUSION_EXCLUSION "inclusion_exclusion"

/**
 * @brief 错排数
 *
 * 数学定义：$!n = n! \sum_{i=0}^{n} \frac{(-1)^i}{i!} = \left\lfloor \frac{n!}{e} + \frac{1}{2} \right\rfloor$，
 * 即 $n$ 个元素中没有任何元素在原来位置上的排列数
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 元素个数
 * 输出：
 *   - !n: 整数 (PRESET_TYPE_INTEGER) - 错排数
 *
 * 复杂度：O(n)
 */
#define PRESET_DERANGEMENT_COUNT "derangement_count"

/**
 * @brief 满射数量
 *
 * 数学定义：从 $m$ 个元素的集合到 $n$ 个元素的集合的满射数量为
 * $n! \cdot S(m,n)$，其中 $S(m,n)$ 为第二类 Stirling 数
 *
 * 输入：
 *   - m: 整数 (PRESET_TYPE_INTEGER) - 定义域大小
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 值域大小
 * 输出：
 *   - count: 整数 (PRESET_TYPE_INTEGER) - 满射数量
 *
 * 复杂度：O(mn)
 */
#define PRESET_SURJECTION_COUNT "surjection_count"

/* ==================== 递推关系 ==================== */

/**
 * @brief 第一类Stirling数
 *
 * 数学定义：$s(n,k)$ 表示将 $n$ 个元素划分为 $k$ 个不相交轮换的方式数
 * $s(n,k) = s(n-1,k-1) + (n-1) \cdot s(n-1,k)$，$s(0,0) = 1$
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 元素总数
 *   - k: 整数 (PRESET_TYPE_INTEGER) - 轮换数
 * 输出：
 *   - s(n,k): 整数 (PRESET_TYPE_INTEGER) - 第一类Stirling数
 *
 * 复杂度：O(nk)
 */
#define PRESET_STIRLING_NUMBER_FIRST "stirling_number_first"

/**
 * @brief 第二类Stirling数
 *
 * 数学定义：$S(n,k)$ 表示将 $n$ 个元素划分为 $k$ 个非空子集的方式数
 * $S(n,k) = S(n-1,k-1) + k \cdot S(n-1,k)$，$S(0,0) = 1$
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 元素总数
 *   - k: 整数 (PRESET_TYPE_INTEGER) - 子集数
 * 输出：
 *   - S(n,k): 整数 (PRESET_TYPE_INTEGER) - 第二类Stirling数
 *
 * 复杂度：O(nk)
 */
#define PRESET_STIRLING_NUMBER_SECOND "stirling_number_second"

/**
 * @brief Bell数
 *
 * 数学定义：$B_n = \sum_{k=0}^{n} S(n,k)$，表示将 $n$ 个元素划分为非空子集的方式总数
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 元素个数
 * 输出：
 *   - B_n: 整数 (PRESET_TYPE_INTEGER) - Bell数
 *
 * 复杂度：O(n^2)
 */
#define PRESET_BELL_NUMBER "bell_number"

/**
 * @brief Catalan数
 *
 * 数学定义：$C_n = \frac{1}{n+1}\binom{2n}{n} = \frac{(2n)!}{(n+1)!\,n!}$，
 * 计数括号匹配、二叉搜索树等组合结构
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 索引
 * 输出：
 *   - C_n: 整数 (PRESET_TYPE_INTEGER) - Catalan数
 *
 * 复杂度：O(n)
 */
#define PRESET_CATALAN_NUMBER "catalan_number"

/**
 * @brief 整数分拆数
 *
 * 数学定义：$p(n)$ 表示将正整数 $n$ 写为正整数之和的方式数（不计顺序）
 * $p(n) = p(n-1) + p(n-2) - p(n-5) - p(n-7) + \cdots$（五边形数定理）
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 待分拆的正整数
 * 输出：
 *   - p(n): 整数 (PRESET_TYPE_INTEGER) - 分拆数
 *
 * 复杂度：O(n sqrt(n))
 */
#define PRESET_PARTITION_NUMBER "partition_number"

/**
 * @brief Fibonacci数
 *
 * 数学定义：$F_n = F_{n-1} + F_{n-2}$，$F_0 = 0$，$F_1 = 1$
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 索引
 * 输出：
 *   - F_n: 整数 (PRESET_TYPE_INTEGER) - 第 n 个 Fibonacci 数
 *
 * 复杂度：O(log n)
 */
#define PRESET_FIBONACCI_NUMBER "fibonacci_number"

/**
 * @brief Lucas数
 *
 * 数学定义：$L_n = L_{n-1} + L_{n-2}$，$L_0 = 2$，$L_1 = 1$
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 索引
 * 输出：
 *   - L_n: 整数 (PRESET_TYPE_INTEGER) - 第 n 个 Lucas 数
 *
 * 复杂度：O(log n)
 */
#define PRESET_LUCAS_NUMBER "lucas_number"

/* ==================== 图论基础 ==================== */

/**
 * @brief 图的度数
 *
 * 数学定义：顶点 $v$ 的度数 $\deg(v)$ 为与 $v$ 相邻的边数，
 * $\sum_{v \in V} \deg(v) = 2|E|$（握手定理）
 *
 * 输入：
 *   - G: 图 (PRESET_TYPE_GRAPH) - 输入图
 *   - v: 整数 (PRESET_TYPE_INTEGER) - 顶点编号
 * 输出：
 *   - deg(v): 整数 (PRESET_TYPE_INTEGER) - 顶点度数
 *
 * 复杂度：O(deg(v))
 */
#define PRESET_GRAPH_DEGREE "graph_degree"

/**
 * @brief 连通性判定
 *
 * 数学定义：图 $G$ 是连通的当且仅当对任意 $u, v \in V$，存在从 $u$ 到 $v$ 的路径
 *
 * 输入：
 *   - G: 图 (PRESET_TYPE_GRAPH) - 输入图
 * 输出：
 *   - is_connected: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否连通
 *
 * 复杂度：O(|V| + |E|)
 */
#define PRESET_GRAPH_CONNECTED_TEST "graph_connected_test"

/**
 * @brief 树判定
 *
 * 数学定义：图 $G$ 是树当且仅当 $G$ 连通且 $|E| = |V| - 1$
 *
 * 输入：
 *   - G: 图 (PRESET_TYPE_GRAPH) - 输入图
 * 输出：
 *   - is_tree: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否为树
 *
 * 复杂度：O(|V| + |E|)
 */
#define PRESET_GRAPH_TREE_TEST_CB "graph_tree_test_cb"

/**
 * @brief 平面图判定
 *
 * 数学定义：图 $G$ 是平面图当且仅当不包含 $K_5$ 或 $K_{3,3}$ 的细分作为子图（Kuratowski定理），
 * 或满足 $|E| \le 3|V| - 6$（当 $|V| \ge 3$ 时为必要条件）
 *
 * 输入：
 *   - G: 图 (PRESET_TYPE_GRAPH) - 输入图
 * 输出：
 *   - is_planar: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否为平面图
 *
 * 复杂度：O(|V|)
 */
#define PRESET_GRAPH_PLANAR_TEST "graph_planar_test"

/**
 * @brief 欧拉图判定
 *
 * 数学定义：连通图 $G$ 是欧拉图当且仅当每个顶点的度数都是偶数，
 * 即 $\forall v \in V: \deg(v) \equiv 0 \pmod{2}$
 *
 * 输入：
 *   - G: 图 (PRESET_TYPE_GRAPH) - 输入图
 * 输出：
 *   - is_eulerian: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否为欧拉图
 *
 * 复杂度：O(|V| + |E|)
 */
#define PRESET_GRAPH_EULERIAN_TEST "graph_eulerian_test"

/**
 * @brief 二部图判定
 *
 * 数学定义：图 $G$ 是二部图当且仅当 $G$ 不含奇数长度的环，
 * 等价于顶点集可划分为两个独立集 $V = X \cup Y$
 *
 * 输入：
 *   - G: 图 (PRESET_TYPE_GRAPH) - 输入图
 * 输出：
 *   - is_bipartite: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否为二部图
 *
 * 复杂度：O(|V| + |E|)
 */
#define PRESET_GRAPH_BIPARTITE_TEST_CB "graph_bipartite_test_cb"

/**
 * @brief 色数计算
 *
 * 数学定义：图 $G$ 的色数 $\chi(G)$ 是给 $G$ 的顶点着色使得相邻顶点颜色不同的最少颜色数
 *
 * 输入：
 *   - G: 图 (PRESET_TYPE_GRAPH) - 输入图
 * 输出：
 *   - chi(G): 整数 (PRESET_TYPE_INTEGER) - 色数
 *
 * 复杂度：O(2.44^|V|)（精确算法）
 */
#define PRESET_GRAPH_CHROMATIC_NUMBER_CB "graph_chromatic_number_cb"

/**
 * @brief 最短路径
 *
 * 数学定义：给定图 $G$ 和顶点对 $(s, t)$，求 $s$ 到 $t$ 的最短路径长度
 * $d(s,t) = \min\{\text{length}(p) : p \text{ 是 } s \text{ 到 } t \text{ 的路径}\}$
 *
 * 输入：
 *   - G: 图 (PRESET_TYPE_GRAPH) - 输入图（带权）
 *   - s: 整数 (PRESET_TYPE_INTEGER) - 起点编号
 *   - t: 整数 (PRESET_TYPE_INTEGER) - 终点编号
 * 输出：
 *   - distance: 整数 (PRESET_TYPE_INTEGER) - 最短路径长度
 *
 * 复杂度：O(|E| + |V| log |V|)（Dijkstra算法）
 */
#define PRESET_GRAPH_SHORTEST_PATH_CB "graph_shortest_path_cb"

/**
 * @brief 最小生成树
 *
 * 数学定义：给定连通加权图 $G = (V, E, w)$，求权值最小的生成树 $T \subseteq E$
 * $w(T) = \min\{w(T') : T' \text{ 是 } G \text{ 的生成树}\}$
 *
 * 输入：
 *   - G: 图 (PRESET_TYPE_GRAPH) - 输入连通加权图
 * 输出：
 *   - T: 图 (PRESET_TYPE_GRAPH) - 最小生成树
 *
 * 复杂度：O(|E| log |V|)（Kruskal算法）
 */
#define PRESET_GRAPH_MINIMUM_SPANNING_TREE_CB "graph_minimum_spanning_tree_cb"

/* ==================== 组合恒等式 ==================== */

/**
 * @brief Vandermonde恒等式
 *
 * 数学定义：$\sum_{k=0}^{r} \binom{m}{k}\binom{n}{r-k} = \binom{m+n}{r}$
 *
 * 输入：
 *   - m: 整数 (PRESET_TYPE_INTEGER)
 *   - n: 整数 (PRESET_TYPE_INTEGER)
 *   - r: 整数 (PRESET_TYPE_INTEGER)
 * 输出：
 *   - result: 整数 (PRESET_TYPE_INTEGER) - Vandermonde恒等式验证结果
 *
 * 复杂度：O(r)
 */
#define PRESET_BINOMIAL_IDENTITY_VANDERMONDE "binomial_identity_vandermonde"

/**
 * @brief Pascal恒等式
 *
 * 数学定义：$\binom{n}{k} = \binom{n-1}{k-1} + \binom{n-1}{k}$
 *
 * 输入：
 *   - n: 整数 (PRESET_TYPE_INTEGER)
 *   - k: 整数 (PRESET_TYPE_INTEGER)
 * 输出：
 *   - result: 整数 (PRESET_TYPE_INTEGER) - Pascal恒等式验证结果
 *
 * 复杂度：O(min(k, n-k))
 */
#define PRESET_BINOMIAL_IDENTITY_PASCAL "binomial_identity_pascal"

/**
 * @brief 生成函数构造
 *
 * 数学定义：给定数列 $\{a_n\}$，构造其普通生成函数 $G(x) = \sum_{n=0}^{\infty} a_n x^n$
 * 或指数生成函数 $G_e(x) = \sum_{n=0}^{\infty} a_n \frac{x^n}{n!}$
 *
 * 输入：
 *   - a_n: 序列 (PRESET_TYPE_SEQUENCE) - 输入数列
 *   - type: 整数 (PRESET_TYPE_INTEGER) - 类型（0:普通, 1:指数）
 * 输出：
 *   - G(x): 函数 (PRESET_TYPE_FUNCTION) - 生成函数
 *
 * 复杂度：O(n)
 */
#define PRESET_GENERATING_FUNCTION "generating_function"

/* ==================== COMB 模式函数块（v5.0 新增，含图论基础） ==================== */

#define PRESET_COMB_PERMUTATION "comb_permutation"
#define PRESET_COMB_COMBINATION "comb_combination"
#define PRESET_COMB_MULTISET_PERMUTATION "comb_multiset_permutation"
#define PRESET_COMB_MULTISET_COMBINATION "comb_multiset_combination"
#define PRESET_COMB_STIRLING_FIRST "comb_stirling_first"
#define PRESET_COMB_STIRLING_SECOND "comb_stirling_second"
#define PRESET_COMB_OGF "comb_ogf"
#define PRESET_COMB_EGF "comb_egf"
#define PRESET_COMB_COEFFICIENT_EXTRACT "comb_coefficient_extract"
#define PRESET_COMB_COMPOSITION "comb_composition"
#define PRESET_COMB_GRAPH_CREATE "comb_graph_create"
#define PRESET_COMB_GRAPH_ADD_EDGE "comb_graph_add_edge"
#define PRESET_COMB_GRAPH_DEGREE "comb_graph_degree"
#define PRESET_COMB_GRAPH_IS_CONNECTED "comb_graph_is_connected"
#define PRESET_COMB_GRAPH_IS_TREE "comb_graph_is_tree"
#define PRESET_COMB_INCLUSION_EXCLUSION "comb_inclusion_exclusion"
#define PRESET_COMB_PIGEONHOLE "comb_pigeonhole"
#define PRESET_COMB_RAMSEY_NUMBER "comb_ramsey_number"
#define PRESET_COMB_CATALAN_NUMBER "comb_catalan_number"
#define PRESET_COMB_PARTITION_NUMBER "comb_partition_number"

/* ==================== 模块初始化 ==================== */

/**
 * @brief 注册组合数学预设函数块
 *
 * 此函数由 preset_blocks_init() 自动调用，用户无需直接调用。
 *
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_combinatorics_register(void);

/**
 * @brief 获取组合数学模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_combinatorics_count(void);

/**
 * @brief 获取组合数学模块的预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_combinatorics_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取组合数学预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_combinatorics_category(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_COMBINATORICS_H */
