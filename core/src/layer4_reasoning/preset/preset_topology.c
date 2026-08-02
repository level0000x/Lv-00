/**
 * @file preset_topology.c
 * @brief 拓扑学预设函数块 - 实现
 *
 * 实现理论数学研究中常用的拓扑学运算预设函数块。
 * 涵盖拓扑空间基础、连续映射、分离公理、紧致性、连通性及基本群。
 *
 * @module Topology
 * @category PRESET_CATEGORY_TOPOLOGY
 * @version 5.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_topology.h -> preset_blocks.h -> func_block_registry.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_simple() 声明
 *   -> 提供 PresetCategory 枚举（PRESET_CATEGORY_TOPOLOGY 等）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 *   -> 提供 PRESET_SAFE_MALLOC 等安全内存操作宏
 * lv_internal.h / lv_utils.h
 *   -> 提供 lv_malloc、lv_free、lv_strdup、lv_log_* 等
 * ============================================================
 */
#include "preset_topology.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h" /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等） */

/* ==================== 预设函数块数量 ==================== */

/** 拓扑学模块预设函数块总数 */

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个拓扑学预设
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义
 * @param complexity 时间复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
LV_DECLARE_PRESET_REGISTER(PRESET_CATEGORY_TOPOLOGY)

/* ==================== 模块注册实现 ====================*/

bool preset_topology_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：拓扑空间基础
     * ============================================================ */

    /* -------------------- 拓扑判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_TOPOLOGY_TEST, "判定集合族是否构成集合 X 上的拓扑", 2, PRESET_TYPE_BOOLEAN, "\\mathcal{T} \\text{ 是拓扑} \\Leftrightarrow \\emptyset, X \\in \\mathcal{T}, "
                       "\\mathcal{T} \\text{ 对任意并、有限交封闭}", "O(|\\mathcal{T}|²)", true, false, PRESET_TYPE_SET, PRESET_TYPE_SET);

    /* -------------------- 开集判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_OPEN_SET_TEST, "判定集合是否是拓扑空间中的开集", 2, PRESET_TYPE_BOOLEAN, "U \\in \\mathcal{T}", "O(|\\mathcal{T}|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_OPEN_SET);

    /* -------------------- 闭集判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_CLOSED_SET_TEST, "判定集合是否是拓扑空间中的闭集", 2, PRESET_TYPE_BOOLEAN, "X \\setminus A \\in \\mathcal{T}", "O(|\\mathcal{T}|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_CLOSED_SET);

    /* -------------------- 闭包计算 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_CLOSURE, "计算集合的闭包（包含该集合的最小闭集）", 2, PRESET_TYPE_CLOSED_SET, "\\bar{A} = \\bigcap \\{F : A \\subseteq F, F \\text{ 闭}\\}", "O(|\\mathcal{T}|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SET);

    /* -------------------- 内部计算 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_INTERIOR, "计算集合的内部（包含在该集合中的最大开集）", 2, PRESET_TYPE_OPEN_SET, "A^\\circ = \\bigcup \\{U : U \\subseteq A, U \\in \\mathcal{T}\\}", "O(|\\mathcal{T}|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SET);

    /* -------------------- 边界计算 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_BOUNDARY, "计算集合的边界", 2, PRESET_TYPE_SET, "\\partial A = \\bar{A} \\setminus A^\\circ", "O(|\\mathcal{T}|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SET);

    /* -------------------- 邻域判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NEIGHBORHOOD_TEST, "判定集合是否是某点的邻域", 3, PRESET_TYPE_BOOLEAN, "N \\text{ 是 } x \\text{ 的邻域} \\Leftrightarrow \\exists U \\in \\mathcal{T}: x \\in U \\subseteq N", "O(|\\mathcal{T}|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SET, PRESET_TYPE_SET);

    /* -------------------- 邻域系计算 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NEIGHBORHOOD_SYSTEM, "计算某点的邻域系", 2, PRESET_TYPE_SET, "\\mathcal{N}(x) = \\{N : x \\in N^\\circ\\}", "O(|\\mathcal{T}|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SET);

    /* -------------------- 基判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_BASE_TEST, "判定集合族是否是拓扑的基", 2, PRESET_TYPE_BOOLEAN, "\\mathcal{B} \\text{ 是基} \\Leftrightarrow \\forall x, \\forall U \\ni x, "
                       "\\exists B \\in \\mathcal{B}: x \\in B \\subseteq U", "O(|\\mathcal{B}|²)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SET);

    /* -------------------- 由基生成拓扑 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_TOPOLOGY_FROM_BASE, "由基生成拓扑", 2, PRESET_TYPE_SPACE, "\\mathcal{T} = \\{\\bigcup \\mathcal{B}' : \\mathcal{B}' \\subseteq \\mathcal{B}\\}", "O(2^{|\\mathcal{B}|})", true, false, PRESET_TYPE_SET, PRESET_TYPE_SET);

    /* ============================================================
     * 第二部分：连续映射
     * ============================================================ */

    /* -------------------- 连续映射判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_CONTINUOUS_MAP_TEST, "判定映射是否连续", 3, PRESET_TYPE_BOOLEAN, "f \\text{ 连续} \\Leftrightarrow f^{-1}(U) \\in \\mathcal{T}_X, \\forall U \\in \\mathcal{T}_Y", "O(|\\mathcal{T}_Y|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SPACE, PRESET_TYPE_FUNCTION);

    /* -------------------- 同胚判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_HOMEOMORPHISM_TEST, "判定两个拓扑空间是否同胚", 2, PRESET_TYPE_BOOLEAN, "X \\cong Y \\Leftrightarrow \\exists f: X \\to Y \\text{ 连续双射且 } f^{-1} \\text{ 连续}", "O(|X|!)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SPACE);

    /* -------------------- 商拓扑构造 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_QUOTIENT_TOPOLOGY, "构造商空间 X/~ 的商拓扑", 2, PRESET_TYPE_SPACE, "\\mathcal{T}_{X/~} = \\{U : \\pi^{-1}(U) \\in \\mathcal{T}_X\\}", "O(|\\mathcal{T}_X|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_FUNCTION);

    /* -------------------- 积拓扑构造 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PRODUCT_TOPOLOGY, "构造积空间 X × Y 的积拓扑", 2, PRESET_TYPE_SPACE, "\\mathcal{T}_{X×Y} = \\text{由 } \\{U × V : U \\in \\mathcal{T}_X, V \\in \\mathcal{T}_Y\\} \\text{ 生成}", "O(|\\mathcal{T}_X| × |\\mathcal{T}_Y|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SPACE);

    /* -------------------- 子空间拓扑 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_SUBSPACE_TOPOLOGY, "构造子空间 A ⊆ X 的诱导拓扑", 2, PRESET_TYPE_SPACE, "\\mathcal{T}_A = \\{U ∩ A : U \\in \\mathcal{T}_X\\}", "O(|\\mathcal{T}_X|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SET);

    /* ============================================================
     * 第三部分：分离公理
     * ============================================================ */

    /* -------------------- T0 空间判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_T0_SPACE_TEST, "判定是否是 T0 (Kolmogorov) 空间", 1, PRESET_TYPE_BOOLEAN, "X \\text{ 是 } T_0 \\Leftrightarrow \\forall x \\neq y, "
                       "\\exists U \\in \\mathcal{T}: (x \\in U, y \\notin U) \\lor (y \\in U, x \\notin U)", "O(|X|² |\\mathcal{T}|)", true, false, PRESET_TYPE_SPACE);

    /* -------------------- T1 空间判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_T1_SPACE_TEST, "判定是否是 T1 (Fréchet) 空间", 1, PRESET_TYPE_BOOLEAN, "X \\text{ 是 } T_1 \\Leftrightarrow \\forall x \\neq y, \\exists U \\in \\mathcal{T}: x \\in U, y \\notin U", "O(|X|² |\\mathcal{T}|)", true, false, PRESET_TYPE_SPACE);

    /* -------------------- T2 空间判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_T2_SPACE_TEST, "判定是否是 T2 (Hausdorff) 空间", 1, PRESET_TYPE_BOOLEAN, "X \\text{ 是 } T_2 \\Leftrightarrow \\forall x \\neq y, \\exists U, V \\in \\mathcal{T}: x \\in U, y \\in V, U ∩ V = \\emptyset", "O(|X|² |\\mathcal{T}|²)", true, false, PRESET_TYPE_SPACE);

    /* -------------------- T3 空间判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_T3_SPACE_TEST, "判定是否是 T3 (正则) 空间", 1, PRESET_TYPE_BOOLEAN, "X \\text{ 正则} \\Leftrightarrow T_1 + \\forall x, \\forall \\text{闭集} F \\not\\ni x, \\exists U, V \\in \\mathcal{T}: x \\in U, F \\subseteq V, U ∩ V = \\emptyset", "O(|X|² |\\mathcal{T}|²)", true, false, PRESET_TYPE_SPACE);

    /* -------------------- T4 空间判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_T4_SPACE_TEST, "判定是否是 T4 (正规) 空间", 1, PRESET_TYPE_BOOLEAN, "X \\text{ 正规} \\Leftrightarrow T_1 + \\forall \\text{闭集} E, F \\text{ 不相交}, \\exists U, V \\in \\mathcal{T}: E \\subseteq U, F \\subseteq V, U ∩ V = \\emptyset", "O(|X|² |\\mathcal{T}|²)", true, false, PRESET_TYPE_SPACE);

    /* ============================================================
     * 第四部分：紧致性
     * ============================================================ */

    /* -------------------- 紧致空间判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_COMPACT_SPACE_TEST, "判定拓扑空间是否紧致", 1, PRESET_TYPE_BOOLEAN, "X \\text{ 紧致} \\Leftrightarrow \\text{每个开覆盖有有限子覆盖}", "O(2^{|\\mathcal{T}|})", true, false, PRESET_TYPE_SPACE);

    /* -------------------- 列紧空间判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_SEQUENTIALLY_COMPACT, "判定拓扑空间是否列紧", 1, PRESET_TYPE_BOOLEAN, "X \\text{ 列紧} \\Leftrightarrow \\text{每个序列有收敛子序列}", "O(|X|^{|X|})", true, false, PRESET_TYPE_SPACE);

    /* -------------------- 局部紧致判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_LOCALLY_COMPACT_TEST, "判定拓扑空间是否局部紧致", 1, PRESET_TYPE_BOOLEAN, "X \\text{ 局部紧致} \\Leftrightarrow \\forall x, \\exists \\text{紧致邻域 } K \\ni x", "O(|X| |\\mathcal{T}|)", true, false, PRESET_TYPE_SPACE);

    /* -------------------- 单点紧致化 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_ONE_POINT_COMPACTIFICATION, "构造拓扑空间的单点紧致化 X* = X ∪ {∞}", 1, PRESET_TYPE_SPACE, "X^* = X \\cup \\{\\infty\\}, \\mathcal{T}^* = \\mathcal{T} \\cup \\{X^* \\setminus K : K \\text{ 紧致}\\}", "O(|\\mathcal{T}|)", true, false, PRESET_TYPE_SPACE);

    /* ============================================================
     * 第五部分：连通性
     * ============================================================ */

    /* -------------------- 连通空间判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_CONNECTED_SPACE_TEST, "判定拓扑空间是否连通", 1, PRESET_TYPE_BOOLEAN, "X \\text{ 连通} \\Leftrightarrow \\nexists U, V \\in \\mathcal{T} \\setminus \\{\\emptyset\\}: U ∪ V = X, U ∩ V = \\emptyset", "O(|\\mathcal{T}|²)", true, false, PRESET_TYPE_SPACE);

    /* -------------------- 道路连通判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PATH_CONNECTED_TEST, "判定拓扑空间是否道路连通", 1, PRESET_TYPE_BOOLEAN, "X \\text{ 道路连通} \\Leftrightarrow \\forall x, y, \\exists \\gamma: [0,1] \\to X \\text{ 连续}: \\gamma(0) = x, \\gamma(1) = y", "O(|X|²)", true, false, PRESET_TYPE_SPACE);

    /* -------------------- 连通分支 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_CONNECTED_COMPONENT, "计算包含某点的连通分支", 2, PRESET_TYPE_SET, "C(x) = \\bigcup \\{C : x \\in C, C \\text{ 连通}\\}", "O(|X| |\\mathcal{T}|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SET);

    /* -------------------- 道路连通分支 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PATH_COMPONENT, "计算包含某点的道路连通分支", 2, PRESET_TYPE_SET, "P(x) = \\{y : \\exists \\text{道路 } \\gamma: x \\to y\\}", "O(|X|²)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SET);

    /* -------------------- 局部连通判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_LOCALLY_CONNECTED_TEST, "判定拓扑空间是否局部连通", 1, PRESET_TYPE_BOOLEAN, "X \\text{ 局部连通} \\Leftrightarrow \\forall x, \\forall U \\ni x, \\exists \\text{连通开集 } V: x \\in V \\subseteq U", "O(|X| |\\mathcal{T}|²)", true, false, PRESET_TYPE_SPACE);

    /* -------------------- 完全不连通判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_TOTALLY_DISCONNECTED, "判定拓扑空间是否完全不连通", 1, PRESET_TYPE_BOOLEAN, "X \\text{ 完全不连通} \\Leftrightarrow \\text{所有连通分支都是单点集}", "O(|X|)", true, false, PRESET_TYPE_SPACE);

    /* ============================================================
     * 第六部分：基本群
     * ============================================================ */

    /* -------------------- 同伦判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_HOMOTOPY_TEST, "判定两条道路是否同伦", 3, PRESET_TYPE_BOOLEAN, "f \\simeq g \\Leftrightarrow \\exists H: X × [0,1] \\to Y \\text{ 连续}: H(x,0) = f(x), H(x,1) = g(x)", "O(|X|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_PATH, PRESET_TYPE_PATH);

    /* -------------------- 道路同伦判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PATH_HOMOTOPY_TEST, "判定两条道路是否道路同伦（保持端点）", 3, PRESET_TYPE_BOOLEAN, "f \\simeq_p g \\Leftrightarrow f \\simeq g \\land f(0)=g(0) \\land f(1)=g(1)", "O(|X|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_PATH, PRESET_TYPE_PATH);

    /* -------------------- 基本群计算 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_FUNDAMENTAL_GROUP, "计算拓扑空间在基点处的基本群 π₁(X, x₀)", 2, PRESET_TYPE_GROUP, "\\pi_1(X, x_0) = \\{[\\gamma] : \\gamma \\text{ 是基于 } x_0 \\text{ 的闭路}\\}", "O(|X|²)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SET);

    /* -------------------- 道路类乘法 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PATH_CLASS_MULTIPLY, "道路类的乘法（首尾相接）", 3, PRESET_TYPE_PATH, "[f] \\cdot [g] = [f * g], \\text{其中 } (f * g)(t) = \\begin{cases} f(2t) & t \\le 1/2 \\\\ g(2t-1) & t \\ge 1/2 \\end{cases}", "O(1)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_PATH, PRESET_TYPE_PATH);

    /* -------------------- 单连通判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_SIMPLY_CONNECTED_TEST, "判定拓扑空间是否单连通", 1, PRESET_TYPE_BOOLEAN, "X \\text{ 单连通} \\Leftrightarrow X \\text{ 道路连通} \\land \\pi_1(X) = \\{e\\}", "O(|X|²)", true, false, PRESET_TYPE_SPACE);

    /* -------------------- 覆盖空间构造 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_COVERING_SPACE, "构造覆盖空间", 1, PRESET_TYPE_SPACE, "p: \\tilde{X} \\to X \\text{ 是覆盖映射} \\Leftrightarrow \\forall x, \\exists U: p^{-1}(U) = \\bigsqcup V_\\alpha", "O(|X|)", true, false, PRESET_TYPE_SPACE);

    /* ============================================================
     * 第七部分：特殊拓扑空间
     * ============================================================ */

    /* -------------------- 离散拓扑构造 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_DISCRETE_TOPOLOGY, "构造集合上的离散拓扑（所有子集都是开集）", 1, PRESET_TYPE_SPACE, "\\mathcal{T}_{\\text{离散}} = \\mathcal{P}(X)", "O(2^{|X|})", true, false, PRESET_TYPE_SET);

    /* -------------------- 平凡拓扑构造 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_TRIVIAL_TOPOLOGY, "构造集合上的平凡拓扑（只有空集和全集是开集）", 1, PRESET_TYPE_SPACE, "\\mathcal{T}_{\\text{平凡}} = \\{\\emptyset, X\\}", "O(1)", true, false, PRESET_TYPE_SET);

    /* -------------------- 度量拓扑构造 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_METRIC_TOPOLOGY, "由度量函数生成拓扑", 2, PRESET_TYPE_SPACE, "\\mathcal{T}_d = \\text{由 } \\{B(x,r) : x \\in X, r > 0\\} \\text{ 生成}", "O(|X|²)", true, false, PRESET_TYPE_SET, PRESET_TYPE_FUNCTION);

    /* -------------------- 序拓扑构造 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_ORDER_TOPOLOGY, "由全序关系生成拓扑", 2, PRESET_TYPE_SPACE, "\\mathcal{T}_< = \\text{由 } \\{(a,b), (-\\infty, b), (a, +\\infty)\\} \\text{ 生成}", "O(|X|)", true, false, PRESET_TYPE_SET, PRESET_TYPE_FUNCTION);

    /* -------------------- 开映射判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_OPEN_MAP_TEST, "判定映射是否是开映射", 3, PRESET_TYPE_BOOLEAN, "f \\text{ 开映射} \\Leftrightarrow f(U) \\in \\mathcal{T}_Y, \\forall U \\in \\mathcal{T}_X", "O(|\\mathcal{T}_X|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SPACE, PRESET_TYPE_FUNCTION);

    /* -------------------- 闭映射判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_CLOSED_MAP_TEST, "判定映射是否是闭映射", 3, PRESET_TYPE_BOOLEAN, "f \\text{ 闭映射} \\Leftrightarrow f(F) \\text{ 闭}, \\forall F \\text{ 闭}", "O(|\\mathcal{T}_X|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SPACE, PRESET_TYPE_FUNCTION);

    /* -------------------- 嵌入判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_EMBEDDING_TEST, "判定映射是否是嵌入", 3, PRESET_TYPE_BOOLEAN, "f \\text{ 是嵌入} \\Leftrightarrow f \\text{ 单射且 } f: X \\to f(X) \\text{ 同胚}", "O(|\\mathcal{T}_X| + |\\mathcal{T}_Y|)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SPACE, PRESET_TYPE_FUNCTION);

    /* -------------------- 子基判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_SUBBASE_TEST, "判定集合族是否是拓扑的子基", 2, PRESET_TYPE_BOOLEAN, "\\mathcal{S} \\text{ 是子基} \\Leftrightarrow \\mathcal{T} = \\text{由有限交生成的拓扑}", "O(|\\mathcal{S}|²)", true, false, PRESET_TYPE_SPACE, PRESET_TYPE_SET);

    /* -------------------- 开覆盖计算 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_OPEN_COVER, "计算拓扑空间的一个开覆盖", 1, PRESET_TYPE_SET, "\\bigcup_{U \\in \\mathcal{C}} U = X", "O(|\\mathcal{T}|)", true, false, PRESET_TYPE_SPACE);

    /* -------------------- 分离公理完整判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_SEPARATION_AXIOMS, "判定拓扑空间满足的最高分离公理", 1, PRESET_TYPE_SCALAR, "依次检验 T0→T1→T2→T3→T4，返回满足的最高级别", "O(n²)", false, false, PRESET_TYPE_SET);

    /* -------------------- 紧致化 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_COMPACTIFICATION, "构造拓扑空间的紧致化", 2, PRESET_TYPE_SET, "将非紧致空间嵌入紧致空间中", "O(∞)", true, false, PRESET_TYPE_SET, PRESET_TYPE_FUNCTION);

    /* -------------------- 有限子覆盖 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_FINITE_SUBCOVER, "从开覆盖中提取有限子覆盖", 1, PRESET_TYPE_LIST, "若空间紧致，则任一开覆盖存在有限子覆盖", "O(n²)", true, false, PRESET_TYPE_LIST);

    /* -------------------- 提升存在性 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_LIFTING_EXISTENCE, "判定连续映射的提升是否存在", 3, PRESET_TYPE_BOOLEAN, "给定 f:X→B 和覆盖空间 p:E→B，判定是否存在 g:X→E 使得 p∘g=f", "O(∞)", false, false, PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SET);

    /* 返回是否所有预设都注册成功 */
    /* lv_log_info("拓扑学预设注册完成，共 %d 个预设", success_count) */
    return success_count == TOPOLOGY_PRESET_COUNT;
}

/**
 * @brief 获取拓扑学预设函数块数量
 */
int preset_topology_count(void) {
    return TOPOLOGY_PRESET_COUNT;
}

/**
 * @brief 获取拓扑学预设名称列表
 */
bool preset_topology_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv_malloc(TOPOLOGY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        PRESET_TOPOLOGY_TEST,
        PRESET_OPEN_SET_TEST,
        PRESET_CLOSED_SET_TEST,
        PRESET_CLOSURE,
        PRESET_INTERIOR,
        PRESET_BOUNDARY,
        PRESET_NEIGHBORHOOD_TEST,
        PRESET_NEIGHBORHOOD_SYSTEM,
        PRESET_BASE_TEST,
        PRESET_TOPOLOGY_FROM_BASE,
        PRESET_CONTINUOUS_MAP_TEST,
        PRESET_HOMEOMORPHISM_TEST,
        PRESET_QUOTIENT_TOPOLOGY,
        PRESET_PRODUCT_TOPOLOGY,
        PRESET_SUBSPACE_TOPOLOGY,
        PRESET_T0_SPACE_TEST,
        PRESET_T1_SPACE_TEST,
        PRESET_T2_SPACE_TEST,
        PRESET_T3_SPACE_TEST,
        PRESET_T4_SPACE_TEST,
        PRESET_COMPACT_SPACE_TEST,
        PRESET_SEQUENTIALLY_COMPACT,
        PRESET_LOCALLY_COMPACT_TEST,
        PRESET_ONE_POINT_COMPACTIFICATION,
        PRESET_CONNECTED_SPACE_TEST,
        PRESET_PATH_CONNECTED_TEST,
        PRESET_CONNECTED_COMPONENT,
        PRESET_PATH_COMPONENT,
        PRESET_LOCALLY_CONNECTED_TEST,
        PRESET_TOTALLY_DISCONNECTED,
        PRESET_HOMOTOPY_TEST,
        PRESET_PATH_HOMOTOPY_TEST,
        PRESET_FUNDAMENTAL_GROUP,
        PRESET_PATH_CLASS_MULTIPLY,
        PRESET_SIMPLY_CONNECTED_TEST,
        PRESET_COVERING_SPACE,
        PRESET_DISCRETE_TOPOLOGY,
        PRESET_TRIVIAL_TOPOLOGY,
        PRESET_METRIC_TOPOLOGY,
        PRESET_ORDER_TOPOLOGY,
        PRESET_OPEN_MAP_TEST,
        PRESET_CLOSED_MAP_TEST,
        PRESET_EMBEDDING_TEST,
        PRESET_SUBBASE_TEST,
        PRESET_OPEN_COVER,
        PRESET_SEPARATION_AXIOMS,
        PRESET_COMPACTIFICATION,
        PRESET_FINITE_SUBCOVER,
        PRESET_LIFTING_EXISTENCE,
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

PresetCategory preset_topology_category(void) {
    return PRESET_CATEGORY_TOPOLOGY;
}
