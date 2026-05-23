/**
 * @file preset_group_theory.h
 * @brief 群论预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的群论运算预设函数块，包括：
 *   - 群基础运算：群操作、逆元、幂运算
 *   - 子群检测：子群判定、生成子群、陪集计算
 *   - 同态与同构：群同态、核、像、同构判定
 *   - 特殊群：循环群、置换群、对称群
 *   - 群结构：拉格朗日定理、西罗定理相关
 *
 * @module GroupTheory
 * @category PRESET_CATEGORY_GROUP_THEORY
 * @version 4.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_GROUP_THEORY_H
#define LV00_PRESET_GROUP_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 群基础运算 -------------------- */

/** 群运算：a * b */
#define PRESET_GROUP_OPERATION         "group_operation"

/** 逆元：a^(-1) */
#define PRESET_GROUP_INVERSE           "group_inverse"

/** 幂运算：a^n */
#define PRESET_GROUP_POWER             "group_power"

/** 单位元检测 */
#define PRESET_GROUP_IDENTITY_TEST     "identity_test"

/** 元素阶：ord(a) */
#define PRESET_ELEMENT_ORDER           "element_order"

/* -------------------- 子群相关 -------------------- */

/** 子群判定 */
#define PRESET_SUBGROUP_TEST           "subgroup_test"

/** 生成子群：<S> */
#define PRESET_GENERATED_SUBGROUP      "generated_subgroup"

/** 左陪集：aH */
#define PRESET_LEFT_COSET              "left_coset"

/** 右陪集：Ha */
#define PRESET_RIGHT_COSET             "right_coset"

/** 陪集分解 */
#define PRESET_COSET_DECOMPOSITION     "coset_decomposition"

/** 正规子群判定 */
#define PRESET_NORMAL_SUBGROUP_TEST    "normal_subgroup_test"

/** 商群构造 */
#define PRESET_QUOTIENT_GROUP          "quotient_group"

/* -------------------- 同态与同构 -------------------- */

/** 群同态检测 */
#define PRESET_GROUP_HOMOMORPHISM_TEST "homomorphism_test"

/** 同态核：ker(f) */
#define PRESET_HOMOMORPHISM_KERNEL     "homomorphism_kernel"

/** 同态像：im(f) */
#define PRESET_HOMOMORPHISM_IMAGE      "homomorphism_image"

/** 群同构判定 */
#define PRESET_GROUP_ISOMORPHISM_TEST  "isomorphism_test"

/** 自同构群：Aut(G) */
#define PRESET_AUTOMORPHISM_GROUP      "automorphism_group"

/** 内自同构群：Inn(G) */
#define PRESET_INNER_AUTOMORPHISM      "inner_automorphism"

/* -------------------- 特殊群 -------------------- */

/** 循环群判定 */
#define PRESET_CYCLIC_GROUP_TEST       "cyclic_group_test"

/** 循环群生成元 */
#define PRESET_CYCLIC_GENERATORS       "cyclic_generators"

/** 阿贝尔群判定 */
#define PRESET_ABELIAN_GROUP_TEST      "abelian_group_test"

/** 置换群乘法 */
#define PRESET_PERMUTATION_MULTIPLY    "permutation_multiply"

/** 置换乘积分解 */
#define PRESET_PERMUTATION_DECOMPOSE   "permutation_decompose"

/** 对称群 S_n */
#define PRESET_SYMMETRIC_GROUP         "symmetric_group"

/** 交错群 A_n */
#define PRESET_ALTERNATING_GROUP       "alternating_group"

/** 二面体群 D_n */
#define PRESET_DIHEDRAL_GROUP          "dihedral_group"

/** 四元数群 Q_8 */
#define PRESET_QUATERNION_GROUP        "quaternion_group"

/* -------------------- 群结构 -------------------- */

/** 群的阶 */
#define PRESET_GROUP_ORDER             "group_order"

/** 元素共轭类 */
#define PRESET_CONJUGACY_CLASS         "conjugacy_class"

/** 类方程 */
#define PRESET_CLASS_EQUATION          "class_equation"

/** 中心：Z(G) */
#define PRESET_GROUP_CENTER            "group_center"

/** 换位子群：[G,G] */
#define PRESET_COMMUTATOR_SUBGROUP     "commutator_subgroup"

/** 导列 */
#define PRESET_DERIVED_SERIES          "derived_series"

/** 下中心列 */
#define PRESET_LOWER_CENTRAL_SERIES    "lower_central_series"

/** 上中心列 */
#define PRESET_UPPER_CENTRAL_SERIES    "upper_central_series"

/** 可解群判定 */
#define PRESET_SOLVABLE_GROUP_TEST     "solvable_group_test"

/** 幂零群判定 */
#define PRESET_NILPOTENT_GROUP_TEST    "nilpotent_group_test"

/* -------------------- 西罗定理 -------------------- */

/** 西罗 p-子群 */
#define PRESET_SYLOW_P_SUBGROUP        "sylow_p_subgroup"

/** 西罗 p-子群数量 */
#define PRESET_SYLOW_SUBGROUP_COUNT    "sylow_subgroup_count"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有群论预设函数块
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_group_theory_register(void);

/**
 * @brief 获取群论预设函数块数量
 *
 * @return int 群论模块预设函数块总数
 */
int preset_group_theory_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_GROUP_THEORY_H */
