/**
 * @file preset_group_theory.c
 * @brief 群论预设函数块 - 实现
 *
 * 实现理论数学研究中常用的群论运算预设函数块。
 * 涵盖群基础运算、子群理论、同态同构、特殊群及群结构分析。
 *
 * @module GroupTheory
 * @category PRESET_CATEGORY_GROUP_THEORY
 * @version 5.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_group_theory.h -> preset_blocks.h -> func_block_registry.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_simple() 声明
 *   -> 提供 PresetCategory 枚举（PRESET_CATEGORY_GROUP_THEORY 等）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 *   -> 提供 PRESET_SAFE_MALLOC 等安全内存操作宏
 * lv_internal.h / lv_utils.h
 *   -> 提供 lv_malloc、lv_free、lv_strdup、lv_log_* 等
 * ============================================================
 */
#include "preset_group_theory.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h" /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等） */

/* ==================== 内部辅助函数 ==================== */

LV_DECLARE_PRESET_REGISTER(PRESET_CATEGORY_GROUP_THEORY)

/* ==================== 模块注册实现 ==================== */

bool preset_group_theory_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：群基础运算
     * ============================================================ */

    /* -------------------- 群运算 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_GROUP_OPERATION, "群运算：计算群中两个元素的乘积 a * b", 3, PRESET_TYPE_GROUP_ELEMENT, "a \\cdot b \\in G", "O(1)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT, PRESET_TYPE_GROUP_ELEMENT);

    /* -------------------- 逆元 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_GROUP_INVERSE, "计算群元素的逆元 a^(-1)", 2, PRESET_TYPE_GROUP_ELEMENT, "a \\cdot a^{-1} = e", "O(1)", true, true, PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT);

    /* -------------------- 幂运算 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_GROUP_POWER, "群元素的幂运算 a^n（快速幂算法）", 3, PRESET_TYPE_GROUP_ELEMENT, "a^n = \\underbrace{a \\cdot a \\cdots a}_{n \\text{ 次}}", "O(log n)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT, PRESET_TYPE_INTEGER);

    /* -------------------- 单位元检测 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_GROUP_IDENTITY_TEST, "判定元素是否是群的单位元", 2, PRESET_TYPE_BOOLEAN, "e \\cdot a = a \\cdot e = a, \\forall a \\in G", "O(1)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT);

    /* -------------------- 元素阶 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_ELEMENT_ORDER, "计算群元素的阶 ord(a)", 2, PRESET_TYPE_INTEGER, "\\text{ord}(a) = \\min\\{n > 0 : a^n = e\\}", "O(ord(a))", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT);

    /* ============================================================
     * 第二部分：子群相关
     * ============================================================ */

    /* -------------------- 子群判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_SUBGROUP_TEST, "判定子集是否构成子群（封闭性、单位元、逆元）", 2, PRESET_TYPE_BOOLEAN, "H \\le G \\Leftrightarrow \\forall a,b \\in H: ab^{-1} \\in H", "O(|H|²)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP);

    /* -------------------- 生成子群 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_GENERATED_SUBGROUP, "由元素集合生成的子群 <S>", 2, PRESET_TYPE_SUBGROUP, "\\langle S \\rangle = \\{s_1^{e_1} \\cdots s_k^{e_k} : s_i \\in S\\}", "O(|G|)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_SEQUENCE);

    /* -------------------- 左陪集 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_LEFT_COSET, "计算元素关于子群的左陪集 aH", 3, PRESET_TYPE_SET, "aH = \\{ah : h \\in H\\}", "O(|H|)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP, PRESET_TYPE_GROUP_ELEMENT);

    /* -------------------- 右陪集 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_RIGHT_COSET, "计算元素关于子群的右陪集 Ha", 3, PRESET_TYPE_SET, "Ha = \\{ha : h \\in H\\}", "O(|H|)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP, PRESET_TYPE_GROUP_ELEMENT);

    /* -------------------- 陪集分解 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_COSET_DECOMPOSITION, "群关于子群的陪集分解", 2, PRESET_TYPE_SET, "G = \\bigcup_{i=1}^{[G:H]} g_i H", "O(|G|)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP);

    /* -------------------- 正规子群判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NORMAL_SUBGROUP_TEST, "判定子群是否是正规子群", 2, PRESET_TYPE_BOOLEAN, "H \\triangleleft G \\Leftrightarrow gHg^{-1} = H, \\forall g \\in G", "O(|G|·|H|)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP);

    /* -------------------- 商群构造 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_QUOTIENT_GROUP, "构造商群 G/H（要求 H 是正规子群）", 2, PRESET_TYPE_GROUP, "G/H = \\{gH : g \\in G\\}", "O(|G|)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP);

    /* ============================================================
     * 第三部分：同态与同构
     * ============================================================ */

    /* -------------------- 群同态检测 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_GROUP_HOMOMORPHISM_TEST, "判定映射是否是群同态", 3, PRESET_TYPE_BOOLEAN, "f(ab) = f(a)f(b), \\forall a,b \\in G", "O(|G|²)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_GROUP, PRESET_TYPE_HOMOMORPHISM);

    /* -------------------- 同态核 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_HOMOMORPHISM_KERNEL, "计算群同态的核 ker(f)", 3, PRESET_TYPE_SUBGROUP, "\\ker(f) = \\{g \\in G : f(g) = e_H\\}", "O(|G|)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_GROUP, PRESET_TYPE_HOMOMORPHISM);

    /* -------------------- 同态像 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_HOMOMORPHISM_IMAGE, "计算群同态的像 im(f)", 3, PRESET_TYPE_SUBGROUP, "\\text{im}(f) = \\{f(g) : g \\in G\\}", "O(|G|)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_GROUP, PRESET_TYPE_HOMOMORPHISM);

    /* -------------------- 群同构判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_GROUP_ISOMORPHISM_TEST, "判定两个群是否同构", 2, PRESET_TYPE_BOOLEAN, "G \\cong H \\Leftrightarrow \\exists \\varphi: G \\to H \\text{ 双射同态}", "O(|G|!)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_GROUP);

    /* -------------------- 自同构群 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_AUTOMORPHISM_GROUP, "计算群的自同构群 Aut(G)", 1, PRESET_TYPE_GROUP, "\\text{Aut}(G) = \\{\\varphi : G \\to G \\text{ 同构}\\}", "O(|G|!)", true, false, PRESET_TYPE_GROUP);

    /* -------------------- 内自同构群 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_INNER_AUTOMORPHISM, "计算群的内自同构群 Inn(G)", 1, PRESET_TYPE_GROUP, "\\text{Inn}(G) = \\{\\varphi_g : x \\mapsto gxg^{-1}\\}", "O(|G|)", true, false, PRESET_TYPE_GROUP);

    /* ============================================================
     * 第四部分：特殊群
     * ============================================================ */

    /* -------------------- 循环群判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_CYCLIC_GROUP_TEST, "判定群是否是循环群", 1, PRESET_TYPE_BOOLEAN, "G \\text{ 循环} \\Leftrightarrow \\exists g: G = \\langle g \\rangle", "O(|G|)", true, false, PRESET_TYPE_GROUP);

    /* -------------------- 循环群生成元 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_CYCLIC_GENERATORS, "计算循环群的所有生成元", 1, PRESET_TYPE_SET, "\\{g : \\langle g \\rangle = G\\}", "O(|G| log |G|)", true, false, PRESET_TYPE_GROUP);

    /* -------------------- 阿贝尔群判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_ABELIAN_GROUP_TEST, "判定群是否是阿贝尔群（交换群）", 1, PRESET_TYPE_BOOLEAN, "G \\text{ 阿贝尔} \\Leftrightarrow ab = ba, \\forall a,b \\in G", "O(|G|²)", true, false, PRESET_TYPE_GROUP);

    /* -------------------- 置换群乘法 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PERMUTATION_MULTIPLY, "两个置换的乘积（复合）", 2, PRESET_TYPE_SEQUENCE, "(\\sigma \\circ \\tau)(i) = \\sigma(\\tau(i))", "O(n)", true, false, PRESET_TYPE_SEQUENCE, PRESET_TYPE_SEQUENCE);

    /* -------------------- 置换乘积分解 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_PERMUTATION_DECOMPOSE, "将置换分解为不相交轮换的乘积", 1, PRESET_TYPE_TUPLE, "\\sigma = \\tau_1 \\circ \\tau_2 \\circ \\cdots \\circ \\tau_k", "O(n)", true, false, PRESET_TYPE_SEQUENCE);

    /* -------------------- 对称群 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_SYMMETRIC_GROUP, "构造 n 次对称群 S_n", 1, PRESET_TYPE_GROUP, "S_n = \\{\\text{所有 } n \\text{ 元置换}\\}, |S_n| = n!", "O(n!)", true, false, PRESET_TYPE_INTEGER);

    /* -------------------- 交错群 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_ALTERNATING_GROUP, "构造 n 次交错群 A_n（偶置换群）", 1, PRESET_TYPE_GROUP, "A_n = \\{\\sigma \\in S_n : \\text{sgn}(\\sigma) = 1\\}", "O(n!)", true, false, PRESET_TYPE_INTEGER);

    /* -------------------- 二面体群 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_DIHEDRAL_GROUP, "构造 n 阶二面体群 D_n（正 n 边形的对称群）", 1, PRESET_TYPE_GROUP, "D_n = \\langle r, s : r^n = s^2 = e, srs = r^{-1} \\rangle", "O(n)", true, false, PRESET_TYPE_INTEGER);

    /* -------------------- 四元数群 -------------------- */
    {
        PresetType _qv[1] = {(PresetType)0};
        if (lv_preset_register_helper(PRESET_QUATERNION_GROUP, "构造四元数群 Q_8", _qv, 0, PRESET_TYPE_GROUP,
                                      "Q_8 = \\{\\pm 1, \\pm i, \\pm j, \\pm k\\}", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：群结构
     * ============================================================ */

    /* -------------------- 群的阶 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_GROUP_ORDER, "计算群的阶 |G|", 1, PRESET_TYPE_INTEGER, "|G| = \\text{群中元素个数}", "O(1)", true, false, PRESET_TYPE_GROUP);

    /* -------------------- 共轭类 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_CONJUGACY_CLASS, "计算元素的共轭类 [a]", 2, PRESET_TYPE_SET, "[a] = \\{gag^{-1} : g \\in G\\}", "O(|G|)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT);

    /* -------------------- 类方程 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_CLASS_EQUATION, "计算群的类方程", 1, PRESET_TYPE_EQUATION, "|G| = |Z(G)| + \\sum_{i} [G : C_G(g_i)]", "O(|G|²)", true, false, PRESET_TYPE_GROUP);

    /* -------------------- 群中心 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_GROUP_CENTER, "计算群的中心 Z(G)", 1, PRESET_TYPE_SUBGROUP, "Z(G) = \\{z \\in G : zg = gz, \\forall g \\in G\\}", "O(|G|²)", true, false, PRESET_TYPE_GROUP);

    /* -------------------- 换位子群 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_COMMUTATOR_SUBGROUP, "计算换位子群 [G,G]", 1, PRESET_TYPE_SUBGROUP, "[G,G] = \\langle [a,b] : a,b \\in G \\rangle", "O(|G|²)", true, false, PRESET_TYPE_GROUP);

    /* -------------------- 导列 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_DERIVED_SERIES, "计算群的导列", 1, PRESET_TYPE_SEQUENCE, "G^{(0)} = G, G^{(i+1)} = [G^{(i)}, G^{(i)}]", "O(|G|² k)", true, false, PRESET_TYPE_GROUP);

    /* -------------------- 可解群判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_SOLVABLE_GROUP_TEST, "判定群是否是可解群", 1, PRESET_TYPE_BOOLEAN, "G \\text{ 可解} \\Leftrightarrow \\exists k: G^{(k)} = \\{e\\}", "O(|G|² k)", true, false, PRESET_TYPE_GROUP);

    /* -------------------- 幂零群判定 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NILPOTENT_GROUP_TEST, "判定群是否是幂零群", 1, PRESET_TYPE_BOOLEAN, "G \\text{ 幂零} \\Leftrightarrow \\exists k: G_k = \\{e\\}", "O(|G|² k)", true, false, PRESET_TYPE_GROUP);

    /* -------------------- 西罗 p-子群 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_SYLOW_P_SUBGROUP, "计算西罗 p-子群", 2, PRESET_TYPE_SUBGROUP, "P \\le G, |P| = p^k, p^k || |G|", "O(|G|)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_PRIME);

    /* -------------------- 西罗子群数量 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_SYLOW_SUBGROUP_COUNT, "计算西罗 p-子群的数量 n_p", 2, PRESET_TYPE_INTEGER, "n_p \\equiv 1 \\pmod{p}, n_p | m", "O(|G|)", true, false, PRESET_TYPE_GROUP, PRESET_TYPE_PRIME);

    /* ============================================================
     * 第七部分：中心列
     * ============================================================ */

    /* -------------------- 下中心列 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_LOWER_CENTRAL_SERIES, "计算群的下中心列", 1, PRESET_TYPE_SEQUENCE, "\\gamma_1(G) = G, \\gamma_{i+1}(G) = [\\gamma_i(G), G]", "O(|G|^2 k)", true, false, PRESET_TYPE_GROUP);

    /* -------------------- 上中心列 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_UPPER_CENTRAL_SERIES, "计算群的上中心列", 1, PRESET_TYPE_SEQUENCE, "Z_0(G) = \\{e\\}, Z_{i+1}(G)/Z_i(G) = Z(G/Z_i(G))", "O(|G|^2 k)", true, false, PRESET_TYPE_GROUP);

    /* 返回是否所有预设都注册成功 */
    /* lv_log_info("群论预设注册完成，共 %d 个预设", success_count) */
    return success_count == GROUP_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取群论预设函数块数量
 *
 * @return int 群论模块预设函数块总数
 */
int preset_group_theory_count(void) {
    return GROUP_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取群论预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_group_theory_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv_malloc(GROUP_THEORY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        PRESET_GROUP_OPERATION,
        PRESET_GROUP_INVERSE,
        PRESET_GROUP_POWER,
        PRESET_GROUP_IDENTITY_TEST,
        PRESET_ELEMENT_ORDER,
        PRESET_SUBGROUP_TEST,
        PRESET_GENERATED_SUBGROUP,
        PRESET_LEFT_COSET,
        PRESET_RIGHT_COSET,
        PRESET_COSET_DECOMPOSITION,
        PRESET_NORMAL_SUBGROUP_TEST,
        PRESET_QUOTIENT_GROUP,
        PRESET_GROUP_HOMOMORPHISM_TEST,
        PRESET_HOMOMORPHISM_KERNEL,
        PRESET_HOMOMORPHISM_IMAGE,
        PRESET_GROUP_ISOMORPHISM_TEST,
        PRESET_AUTOMORPHISM_GROUP,
        PRESET_INNER_AUTOMORPHISM,
        PRESET_CYCLIC_GROUP_TEST,
        PRESET_CYCLIC_GENERATORS,
        PRESET_ABELIAN_GROUP_TEST,
        PRESET_PERMUTATION_MULTIPLY,
        PRESET_PERMUTATION_DECOMPOSE,
        PRESET_SYMMETRIC_GROUP,
        PRESET_ALTERNATING_GROUP,
        PRESET_DIHEDRAL_GROUP,
        PRESET_QUATERNION_GROUP,
        PRESET_GROUP_ORDER,
        PRESET_CONJUGACY_CLASS,
        PRESET_CLASS_EQUATION,
        PRESET_GROUP_CENTER,
        PRESET_COMMUTATOR_SUBGROUP,
        PRESET_DERIVED_SERIES,
        PRESET_SOLVABLE_GROUP_TEST,
        PRESET_NILPOTENT_GROUP_TEST,
        PRESET_SYLOW_P_SUBGROUP,
        PRESET_SYLOW_SUBGROUP_COUNT,
        PRESET_LOWER_CENTRAL_SERIES,
        PRESET_UPPER_CENTRAL_SERIES,
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

/**
 * @brief 获取群论预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_group_theory_category(void) {
    return PRESET_CATEGORY_GROUP_THEORY;
}

/**
 * @brief 获取群论模块所有预设名称列表
 * @return 以 NULL 结尾的名称数组，调用者需使用 lv_free 释放
 */
static __attribute__((unused)) char **get_group_theory_names(void) {
    static const char *names[] = {
        PRESET_GROUP_OPERATION,
        PRESET_GROUP_INVERSE,
        PRESET_GROUP_POWER,
        PRESET_GROUP_IDENTITY_TEST,
        PRESET_ELEMENT_ORDER,
        PRESET_SUBGROUP_TEST,
        PRESET_GENERATED_SUBGROUP,
        PRESET_LEFT_COSET,
        PRESET_RIGHT_COSET,
        PRESET_COSET_DECOMPOSITION,
        PRESET_NORMAL_SUBGROUP_TEST,
        PRESET_QUOTIENT_GROUP,
        PRESET_GROUP_HOMOMORPHISM_TEST,
        PRESET_HOMOMORPHISM_KERNEL,
        PRESET_HOMOMORPHISM_IMAGE,
        PRESET_GROUP_ISOMORPHISM_TEST,
        PRESET_AUTOMORPHISM_GROUP,
        PRESET_INNER_AUTOMORPHISM,
        PRESET_CYCLIC_GROUP_TEST,
        PRESET_CYCLIC_GENERATORS,
        PRESET_ABELIAN_GROUP_TEST,
        PRESET_PERMUTATION_MULTIPLY,
        PRESET_PERMUTATION_DECOMPOSE,
        PRESET_SYMMETRIC_GROUP,
        PRESET_ALTERNATING_GROUP,
        PRESET_DIHEDRAL_GROUP,
        PRESET_QUATERNION_GROUP,
        PRESET_GROUP_ORDER,
        PRESET_CONJUGACY_CLASS,
        PRESET_CLASS_EQUATION,
        PRESET_GROUP_CENTER,
        PRESET_COMMUTATOR_SUBGROUP,
        PRESET_DERIVED_SERIES,
        PRESET_SOLVABLE_GROUP_TEST,
        PRESET_NILPOTENT_GROUP_TEST,
        PRESET_SYLOW_P_SUBGROUP,
        PRESET_SYLOW_SUBGROUP_COUNT,
        PRESET_LOWER_CENTRAL_SERIES,
        PRESET_UPPER_CENTRAL_SERIES,
    };
    const int count = sizeof(names) / sizeof(names[0]);
    char **result = (char **) lv_malloc((count + 1) * sizeof(char *));
    if (!result)
        return NULL;
    for (int i = 0; i < count; i++) {
        result[i] = lv_strdup(names[i]);
        if (!result[i]) {
            for (int j = 0; j < i; j++) {
                void *tmp = result[j];
                lv_free(&tmp);
            }
            {
                void *tmp = result;
                lv_free(&tmp);
            }
            return NULL;
        }
    }
    result[count] = NULL;
    return result;
}
