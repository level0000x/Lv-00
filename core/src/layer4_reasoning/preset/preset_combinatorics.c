/**
 * @file preset_combinatorics.c
 * @brief 组合数学预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/combinatorics.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的组合数学预设函数块。
 * 涵盖排列组合、生成函数、图论基础及计数方法。
 *
 * @module Combinatorics
 * @category PRESET_CATEGORY_COMBINATORICS
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "preset_combinatorics.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 组合数学模块预设函数块总数：20（与头文件中 COMBINATORICS_PRESET_COUNT 一致） */

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个组合数学预设
 */
LV_DECLARE_PRESET_REGISTER(PRESET_CATEGORY_COMBINATORICS)

/* ==================== 模块注册实现 ==================== */

bool preset_combinatorics_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：排列组合（6个）
     * ============================================================ */

    /* -------------------- 排列数 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_PERMUTATION, "排列数：P(n, k) = n! / (n-k)!，从n个元素中取k个的有序排列数", 2, PRESET_TYPE_INTEGER, "P(n, k) = \\frac{n!}{(n-k)!}", "O(k)", true, false, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER);

    /* -------------------- 组合数 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_COMBINATION, "组合数：C(n, k) = n! / (k!(n-k)!)，从n个元素中取k个的无序组合数", 2, PRESET_TYPE_INTEGER, "C(n, k) = \\binom{n}{k} = \\frac{n!}{k!(n-k)!}", "O(k)", true, false, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER);

    /* -------------------- 多重集排列数 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_MULTISET_PERMUTATION, "多重集排列数：n个元素中有重复时的排列数 n! / (n1!·n2!·...·nk!)", 1, PRESET_TYPE_INTEGER, "\\frac{n!}{n_1! \\cdot n_2! \\cdots n_k!}", "O(n)", true, false, PRESET_TYPE_LIST);

    /* -------------------- 多重集组合数 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_MULTISET_COMBINATION, "多重集组合数（重复组合）：C(n+k-1, k)，从n类中可重复取k个", 2, PRESET_TYPE_INTEGER, "\\left(\\!\\!\\binom{n}{k}\\!\\!\\right) = \\binom{n+k-1}{k}", "O(k)", true, false, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER);

    /* -------------------- 第一类Stirling数 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_STIRLING_FIRST, "第一类Stirling数：s(n, k)，将n个元素排成k个轮换的方式数", 2, PRESET_TYPE_INTEGER, "s(n, k) = s(n-1, k-1) - (n-1) \\cdot s(n-1, k)", "O(nk)", true, false, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER);

    /* -------------------- 第二类Stirling数 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_STIRLING_SECOND, "第二类Stirling数：S(n, k)，将n个元素划分为k个非空子集的方式数", 2, PRESET_TYPE_INTEGER, "S(n, k) = S(n-1, k-1) + k \\cdot S(n-1, k)", "O(nk)", true, false, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER);

    /* ============================================================
     * 第二部分：生成函数（4个）
     * ============================================================ */

    /* -------------------- 普通生成函数 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_OGF, "普通生成函数：G(x) = sum_{n>=0} a_n x^n", 1, PRESET_TYPE_FUNCTION, "G(x) = \\sum_{n=0}^{\\infty} a_n x^n", "O(n)", true, false, PRESET_TYPE_LIST);

    /* -------------------- 指数生成函数 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_EGF, "指数生成函数：E(x) = sum_{n>=0} a_n x^n / n!", 1, PRESET_TYPE_FUNCTION, "E(x) = \\sum_{n=0}^{\\infty} a_n \\frac{x^n}{n!}", "O(n)", true, false, PRESET_TYPE_LIST);

    /* -------------------- 系数提取 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_COEFFICIENT_EXTRACT, "系数提取：提取生成函数 F(x) 中 x^n 的系数 [x^n]F(x)", 2, PRESET_TYPE_SCALAR, "[x^n] F(x) = a_n", "O(n)", true, false, PRESET_TYPE_FUNCTION, PRESET_TYPE_INTEGER);

    /* -------------------- 生成函数复合 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_COMPOSITION, "生成函数复合：计算 F(G(x))，两个生成函数的复合运算", 2, PRESET_TYPE_FUNCTION, "F(G(x)) = \\sum_{n=0}^{\\infty} a_n [G(x)]^n", "O(n^2)", true, false, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION);

    /* ============================================================
     * 第三部分：图论基础（5个）
     * ============================================================ */

    /* -------------------- 图创建 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_GRAPH_CREATE, "图创建：创建含 n 个顶点的空图 G(V, E)", 1, PRESET_TYPE_GRAPH, "G = (V, E), \\quad |V| = n, \\quad E = \\emptyset", "O(n)", true, false, PRESET_TYPE_INTEGER);

    /* -------------------- 添加边 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_GRAPH_ADD_EDGE, "添加边：向图中添加一条边 (u, v)", 3, PRESET_TYPE_GRAPH, "G' = (V, E \\cup \\{(u, v)\\})", "O(1)", true, false, PRESET_TYPE_GRAPH, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER);

    /* -------------------- 顶点度数 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_GRAPH_DEGREE, "顶点度数：计算顶点 v 的度数 deg(v) = |{u : (u,v) in E}|", 2, PRESET_TYPE_INTEGER, "\\deg(v) = |\\{u : (u, v) \\in E\\}|", "O(n)", true, false, PRESET_TYPE_GRAPH, PRESET_TYPE_INTEGER);

    /* -------------------- 连通性判定 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_GRAPH_IS_CONNECTED, "连通性判定：判定无向图 G 是否连通（任意两顶点间存在路径）", 1, PRESET_TYPE_BOOLEAN, "G \\text{ 连通} \\Leftrightarrow \\forall u, v \\in V, "
                                 "\\exists \\text{ 路径 } u \\leadsto v", "O(V + E)", false, false, PRESET_TYPE_GRAPH);

    /* -------------------- 树判定 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_GRAPH_IS_TREE, "树判定：判定图 G 是否为树（连通且 |E| = |V| - 1）", 1, PRESET_TYPE_BOOLEAN, "G \\text{ 是树} \\Leftrightarrow G \\text{ 连通且 } |E| = |V| - 1", "O(V + E)", false, false, PRESET_TYPE_GRAPH);

    /* ============================================================
     * 第四部分：计数方法（5个）
     * ============================================================ */

    /* -------------------- 容斥原理 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_INCLUSION_EXCLUSION, "容斥原理：|A1 U ... U An| = Σ|Ai| - Σ|Ai∩Aj| + ... + (-1)^(n+1)|A1∩...∩An|", 2, PRESET_TYPE_INTEGER, "\\left|\\bigcup_{i=1}^{n} A_i\\right| = "
                                 "\\sum_{k=1}^{n} (-1)^{k+1} \\sum_{1 \\le i_1 < \\cdots < i_k \\le n} "
                                 "\\left|A_{i_1} \\cap \\cdots \\cap A_{i_k}\\right|", "O(2^n)", true, false, PRESET_TYPE_SET, PRESET_TYPE_LIST);

    /* -------------------- 鸽巢原理 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_PIGEONHOLE, "鸽巢原理：将 n+1 个物体放入 n 个盒子，至少一个盒子有 >= 2 个物体", 2, PRESET_TYPE_BOOLEAN, "\\text{若 } |A| > n \\text{ 且 } f: A \\to \\{1, \\ldots, n\\}, "
                                 "\\text{则 } \\exists i, j: f(i) = f(j)", "O(1)", false, false, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER);

    /* -------------------- Ramsey数 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_RAMSEY_NUMBER, "Ramsey数：R(s, t)，保证在完全图中存在s团或t独立集的最小顶点数", 2, PRESET_TYPE_INTEGER, "R(s, t) \\le \\binom{s+t-2}{s-1}", "O(2^n)", true, false, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER);

    /* -------------------- Catalan数 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_CATALAN_NUMBER, "Catalan数：C_n = (1/(n+1))·C(2n, n)，计数括号匹配、二叉树等", 1, PRESET_TYPE_INTEGER, "C_n = \\frac{1}{n+1}\\binom{2n}{n} = "
                                 "\\frac{(2n)!}{(n+1)! \\cdot n!}", "O(n)", true, false, PRESET_TYPE_INTEGER);

    /* -------------------- 整数分拆数 -------------------- */
        LV_PRESET_REGISTER(success_count, PRESET_COMB_PARTITION_NUMBER, "整数分拆数：p(n)，将正整数 n 表示为正整数之和的方式数", 1, PRESET_TYPE_INTEGER, "p(n) = p(n-1) + p(n-2) - p(n-5) - p(n-7) + \\cdots "
                                 "\\text{（五边形数定理）}", "O(n\\sqrt{n})", true, false, PRESET_TYPE_INTEGER);

    /* 检查是否所有预设都注册成功 */
    return success_count == COMBINATORICS_PRESET_COUNT;
}

int preset_combinatorics_count(void) {
    return COMBINATORICS_PRESET_COUNT;
}

PresetCategory preset_combinatorics_category(void) {
    return PRESET_CATEGORY_COMBINATORICS;
}

bool preset_combinatorics_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv_malloc(COMBINATORICS_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        /* 排列组合 */
        PRESET_COMB_PERMUTATION,
        PRESET_COMB_COMBINATION,
        PRESET_COMB_MULTISET_PERMUTATION,
        PRESET_COMB_MULTISET_COMBINATION,
        PRESET_COMB_STIRLING_FIRST,
        PRESET_COMB_STIRLING_SECOND,
        /* 生成函数 */
        PRESET_COMB_OGF,
        PRESET_COMB_EGF,
        PRESET_COMB_COEFFICIENT_EXTRACT,
        PRESET_COMB_COMPOSITION,
        /* 图论基础 */
        PRESET_COMB_GRAPH_CREATE,
        PRESET_COMB_GRAPH_ADD_EDGE,
        PRESET_COMB_GRAPH_DEGREE,
        PRESET_COMB_GRAPH_IS_CONNECTED,
        PRESET_COMB_GRAPH_IS_TREE,
        /* 计数方法 */
        PRESET_COMB_INCLUSION_EXCLUSION,
        PRESET_COMB_PIGEONHOLE,
        PRESET_COMB_RAMSEY_NUMBER,
        PRESET_COMB_CATALAN_NUMBER,
        PRESET_COMB_PARTITION_NUMBER,
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
