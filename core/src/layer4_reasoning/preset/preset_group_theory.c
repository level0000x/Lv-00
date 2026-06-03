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
 * preset_register_helper.h
 *   -> 提供 LV00_PRESET_REGISTER_EX 等统一注册辅助宏
 * lv00_internal.h / lv00_utils.h
 *   -> 提供 lv00_malloc、lv00_free、lv00_strdup、lv00_log_* 等
 * ============================================================
 */
#include "preset_group_theory.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h" /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等） */
#include "preset_register_helper.h" /* 统一注册辅助宏 */

/* ==================== 预设函数块数量 ==================== */

/** 群论模块预设函数块总数 */
#define GROUP_THEORY_PRESET_COUNT 39

/* ==================== 模块注册实现 ==================== */

bool preset_group_theory_register(void) {
    int success_count = 0;
    int total_count = 0;

    /* ============================================================
     * 第一部分：群基础运算
     * ============================================================ */

    /* -------------------- 群运算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT, PRESET_TYPE_GROUP_ELEMENT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_GROUP_OPERATION,
                                PRESET_TYPE_GROUP_ELEMENT, inputs, 3, "群运算：计算群中两个元素的乘积 a * b",
                                PRESET_CATEGORY_GROUP_THEORY, "O(1)", false);
    }

    /* -------------------- 逆元 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_GROUP_INVERSE,
                                PRESET_TYPE_GROUP_ELEMENT, inputs, 2, "计算群元素的逆元 a^(-1)",
                                PRESET_CATEGORY_GROUP_THEORY, "O(1)", true);
    }

    /* -------------------- 幂运算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT, PRESET_TYPE_INTEGER};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_GROUP_POWER,
                                PRESET_TYPE_GROUP_ELEMENT, inputs, 3, "群元素的幂运算 a^n（快速幂算法）",
                                PRESET_CATEGORY_GROUP_THEORY, "O(log n)", false);
    }

    /* -------------------- 单位元检测 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_GROUP_IDENTITY_TEST,
                                PRESET_TYPE_BOOLEAN, inputs, 2, "判定元素是否是群的单位元",
                                PRESET_CATEGORY_GROUP_THEORY, "O(1)", false);
    }

    /* -------------------- 元素阶 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_ELEMENT_ORDER,
                                PRESET_TYPE_INTEGER, inputs, 2, "计算群元素的阶 ord(a)",
                                PRESET_CATEGORY_GROUP_THEORY, "O(ord(a))", false);
    }

    /* ============================================================
     * 第二部分：子群相关
     * ============================================================ */

    /* -------------------- 子群判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_SUBGROUP_TEST,
                                PRESET_TYPE_BOOLEAN, inputs, 2, "判定子集是否构成子群（封闭性、单位元、逆元）",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|H|^2)", false);
    }

    /* -------------------- 生成子群 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_SEQUENCE};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_GENERATED_SUBGROUP,
                                PRESET_TYPE_SUBGROUP, inputs, 2, "由元素集合生成的子群 <S>",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|)", false);
    }

    /* -------------------- 左陪集 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP, PRESET_TYPE_GROUP_ELEMENT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LEFT_COSET,
                                PRESET_TYPE_SET, inputs, 3, "计算元素关于子群的左陪集 aH",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|H|)", false);
    }

    /* -------------------- 右陪集 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP, PRESET_TYPE_GROUP_ELEMENT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_RIGHT_COSET,
                                PRESET_TYPE_SET, inputs, 3, "计算元素关于子群的右陪集 Ha",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|H|)", false);
    }

    /* -------------------- 陪集分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_COSET_DECOMPOSITION,
                                PRESET_TYPE_SET, inputs, 2, "群关于子群的陪集分解",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|)", false);
    }

    /* -------------------- 正规子群判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_NORMAL_SUBGROUP_TEST,
                                PRESET_TYPE_BOOLEAN, inputs, 2, "判定子群是否是正规子群",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|*|H|)", false);
    }

    /* -------------------- 商群构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_QUOTIENT_GROUP,
                                PRESET_TYPE_GROUP, inputs, 2, "构造商群 G/H（要求 H 是正规子群）",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|)", false);
    }

    /* ============================================================
     * 第三部分：同态与同构
     * ============================================================ */

    /* -------------------- 群同态检测 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP, PRESET_TYPE_HOMOMORPHISM};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_GROUP_HOMOMORPHISM_TEST,
                                PRESET_TYPE_BOOLEAN, inputs, 3, "判定映射是否是群同态",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|^2)", false);
    }

    /* -------------------- 同态核 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP, PRESET_TYPE_HOMOMORPHISM};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_HOMOMORPHISM_KERNEL,
                                PRESET_TYPE_SUBGROUP, inputs, 3, "计算群同态的核 ker(f)",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|)", false);
    }

    /* -------------------- 同态像 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP, PRESET_TYPE_HOMOMORPHISM};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_HOMOMORPHISM_IMAGE,
                                PRESET_TYPE_SUBGROUP, inputs, 3, "计算群同态的像 im(f)",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|)", false);
    }

    /* -------------------- 群同构判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_GROUP_ISOMORPHISM_TEST,
                                PRESET_TYPE_BOOLEAN, inputs, 2, "判定两个群是否同构",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|!)", false);
    }

    /* -------------------- 自同构群 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_AUTOMORPHISM_GROUP,
                                PRESET_TYPE_GROUP, inputs, 1, "计算群的自同构群 Aut(G)",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|!)", false);
    }

    /* -------------------- 内自同构群 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_INNER_AUTOMORPHISM,
                                PRESET_TYPE_GROUP, inputs, 1, "计算群的内自同构群 Inn(G)",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|)", false);
    }

    /* ============================================================
     * 第四部分：特殊群
     * ============================================================ */

    /* -------------------- 循环群判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_CYCLIC_GROUP_TEST,
                                PRESET_TYPE_BOOLEAN, inputs, 1, "判定群是否是循环群",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|)", false);
    }

    /* -------------------- 循环群生成元 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_CYCLIC_GENERATORS,
                                PRESET_TYPE_SET, inputs, 1, "计算循环群的所有生成元",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G| log |G|)", false);
    }

    /* -------------------- 阿贝尔群判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_ABELIAN_GROUP_TEST,
                                PRESET_TYPE_BOOLEAN, inputs, 1, "判定群是否是阿贝尔群（交换群）",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|^2)", false);
    }

    /* -------------------- 置换群乘法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_SEQUENCE};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_PERMUTATION_MULTIPLY,
                                PRESET_TYPE_SEQUENCE, inputs, 2, "两个置换的乘积（复合）",
                                PRESET_CATEGORY_GROUP_THEORY, "O(n)", false);
    }

    /* -------------------- 置换乘积分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_PERMUTATION_DECOMPOSE,
                                PRESET_TYPE_TUPLE, inputs, 1, "将置换分解为不相交轮换的乘积",
                                PRESET_CATEGORY_GROUP_THEORY, "O(n)", false);
    }

    /* -------------------- 对称群 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_SYMMETRIC_GROUP,
                                PRESET_TYPE_GROUP, inputs, 1, "构造 n 次对称群 S_n",
                                PRESET_CATEGORY_GROUP_THEORY, "O(n!)", false);
    }

    /* -------------------- 交错群 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_ALTERNATING_GROUP,
                                PRESET_TYPE_GROUP, inputs, 1, "构造 n 次交错群 A_n（偶置换群）",
                                PRESET_CATEGORY_GROUP_THEORY, "O(n!)", false);
    }

    /* -------------------- 二面体群 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_DIHEDRAL_GROUP,
                                PRESET_TYPE_GROUP, inputs, 1, "构造 n 阶二面体群 D_n（正 n 边形的对称群）",
                                PRESET_CATEGORY_GROUP_THEORY, "O(n)", false);
    }

    /* -------------------- 四元数群 -------------------- */
    {
        PresetType inputs[1] = {0};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_QUATERNION_GROUP,
                                PRESET_TYPE_GROUP, inputs, 0, "构造四元数群 Q_8",
                                PRESET_CATEGORY_GROUP_THEORY, "O(1)", false);
    }

    /* ============================================================
     * 第五部分：群结构
     * ============================================================ */

    /* -------------------- 群的阶 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_GROUP_ORDER,
                                PRESET_TYPE_INTEGER, inputs, 1, "计算群的阶 |G|",
                                PRESET_CATEGORY_GROUP_THEORY, "O(1)", false);
    }

    /* -------------------- 共轭类 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_CONJUGACY_CLASS,
                                PRESET_TYPE_SET, inputs, 2, "计算元素的共轭类 [a]",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|)", false);
    }

    /* -------------------- 类方程 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_CLASS_EQUATION,
                                PRESET_TYPE_EQUATION, inputs, 1, "计算群的类方程",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|^2)", false);
    }

    /* -------------------- 群中心 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_GROUP_CENTER,
                                PRESET_TYPE_SUBGROUP, inputs, 1, "计算群的中心 Z(G)",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|^2)", false);
    }

    /* -------------------- 换位子群 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_COMMUTATOR_SUBGROUP,
                                PRESET_TYPE_SUBGROUP, inputs, 1, "计算换位子群 [G,G]",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|^2)", false);
    }

    /* -------------------- 导列 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_DERIVED_SERIES,
                                PRESET_TYPE_SEQUENCE, inputs, 1, "计算群的导列",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|^2 k)", false);
    }

    /* -------------------- 可解群判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_SOLVABLE_GROUP_TEST,
                                PRESET_TYPE_BOOLEAN, inputs, 1, "判定群是否是可解群",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|^2 k)", false);
    }

    /* -------------------- 幂零群判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_NILPOTENT_GROUP_TEST,
                                PRESET_TYPE_BOOLEAN, inputs, 1, "判定群是否是幂零群",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|^2 k)", false);
    }

    /* -------------------- 西罗 p-子群 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_PRIME};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_SYLOW_P_SUBGROUP,
                                PRESET_TYPE_SUBGROUP, inputs, 2, "计算西罗 p-子群",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|)", false);
    }

    /* -------------------- 西罗子群数量 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_PRIME};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_SYLOW_SUBGROUP_COUNT,
                                PRESET_TYPE_INTEGER, inputs, 2, "计算西罗 p-子群的数量 n_p",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|)", false);
    }

    /* ============================================================
     * 第六部分：中心列
     * ============================================================ */

    /* -------------------- 下中心列 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LOWER_CENTRAL_SERIES,
                                PRESET_TYPE_SEQUENCE, inputs, 1, "计算群的下中心列",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|^2 k)", false);
    }

    /* -------------------- 上中心列 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_UPPER_CENTRAL_SERIES,
                                PRESET_TYPE_SEQUENCE, inputs, 1, "计算群的上中心列",
                                PRESET_CATEGORY_GROUP_THEORY, "O(|G|^2 k)", false);
    }

    /* 返回是否所有预设都注册成功 */
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

    char **names = (char **) lv00_malloc(GROUP_THEORY_PRESET_COUNT * sizeof(char *));
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
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv00_free(&tmp);
            }
            {
                void *tmp = names;
                lv00_free(&tmp);
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
