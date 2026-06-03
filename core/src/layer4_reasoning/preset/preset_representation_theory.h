/**
 * @file preset_representation_theory.h
 * @brief 表示论预设函数块 - 接口定义
 *
 * 提供理论数学研究中常用的表示论运算预设函数块，包括：
 *   - 群表示：线性表示、置换表示、正则表示、酉表示、
 *             表示维数、表示的直和与张量积
 *   - 特征标理论：特征标计算、特征标表、内积、正交关系、
 *                 不可约特征标判定、类函数展开
 *   - 不可约表示：不可约表示判定、Maschke定理（完全可约性）、
 *                 不可约表示分解、Schur引理
 *   - 诱导表示：Frobenius互反律、诱导特征标、Mackey公式
 *   - 李代数表示：伴随表示、权空间分解、最高权表示、根系
 *
 * 共18个预设函数块，均遵循模块化、确定性原则，
 * 使用统一的 preset_blocks_register_simple 注册接口。
 *
 * @module RepresentationTheory
 * @category PRESET_CATEGORY_GROUP_THEORY
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#ifndef PRESET_REPRESENTATION_THEORY_H
#define PRESET_REPRESENTATION_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 群表示（6个） -------------------- */

/** 线性表示：构造群 G 在向量空间 V 上的同态表示 */
#define PRESET_RT_LINEAR_REPRESENTATION "rt_linear_representation"

/** 置换表示：群在集合上的作用诱导的置换表示 */
#define PRESET_RT_PERMUTATION_REP "rt_permutation_representation"

/** 正则表示：群在自身上的左平移作用诱导的正则表示 */
#define PRESET_RT_REGULAR_REPRESENTATION "rt_regular_representation"

/** 酉表示：在 Hilbert 空间上保持内积的酉表示 */
#define PRESET_RT_UNITARY_REPRESENTATION "rt_unitary_representation"

/** 表示维数：计算表示的维数 dim(rho) */
#define PRESET_RT_REPRESENTATION_DIMENSION "rt_representation_dimension"

/** 表示的直和与张量积：构造新表示的代数运算 */
#define PRESET_RT_REP_DIRECT_SUM_TENSOR "rt_rep_direct_sum_tensor"

/* -------------------- 特征标理论（4个） -------------------- */

/** 特征标计算：计算表示的特征标 chi(g) = Tr(rho(g)) */
#define PRESET_RT_CHARACTER_CALCULATION "rt_character_calculation"

/** 特征标表：构造有限群的特征标表 */
#define PRESET_RT_CHARACTER_TABLE "rt_character_table"

/** 特征标内积：计算两个类函数的内积 (chi, psi) */
#define PRESET_RT_CHARACTER_INNER_PRODUCT "rt_character_inner_product"

/** 不可约特征标正交性：验证不可约特征标的行正交关系 */
#define PRESET_RT_CHARACTER_ORTHOGONALITY "rt_character_orthogonality"

/* -------------------- 不可约表示（4个） -------------------- */

/** 不可约表示判定：验证表示是否不可约（无真不变子空间） */
#define PRESET_RT_IRREDUCIBILITY_TEST "rt_irreducibility_test"

/** Maschke定理：有限群在特征不整除|G|的域上的表示完全可约 */
#define PRESET_RT_MASCHKE_THEOREM "rt_maschke_theorem"

/** 不可约表示分解：将给定表示分解为不可约表示的直和 */
#define PRESET_RT_IRREDUCIBLE_DECOMPOSITION "rt_irreducible_decomposition"

/** Schur引理：不可约表示之间的 Intertwiner 空间是零或一维 */
#define PRESET_RT_SCHURS_LEMMA "rt_schurs_lemma"

/* -------------------- 诱导表示（2个） -------------------- */

/** Frobenius互反律：(Ind_H^G chi, psi)_G = (chi, Res_H^G psi)_H */
#define PRESET_RT_FROBENIUS_RECIPROCITY "rt_frobenius_reciprocity"

/** 诱导特征标公式：给出诱导表示的特征标显式公式 */
#define PRESET_RT_INDUCED_CHARACTER "rt_induced_character"

/* -------------------- 李代数表示（2个） -------------------- */

/** 伴随表示：李代数的伴随表示 ad_X(Y) = [X, Y] */
#define PRESET_RT_ADJOINT_REPRESENTATION "rt_adjoint_representation"

/** 最高权表示：从最高权构造有限维不可约表示（Cartan-Weyl理论） */
#define PRESET_RT_HIGHEST_WEIGHT_REP "rt_highest_weight_representation"

/* ============================================================
 * 预设数量
 * ============================================================ */

/** 表示论模块预设函数块总数 */
#define REPRESENTATION_THEORY_PRESET_COUNT 18

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有表示论预设函数块
 *
 * 将表示论模块的全部18个预设函数块注册到全局预设库中。
 * 涵盖群表示(6)、特征标理论(4)、不可约表示(4)、诱导表示(2)、李代数表示(2)。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_representation_theory_register(void);

/**
 * @brief 获取表示论预设函数块数量
 *
 * @return int 表示论模块预设函数块总数（18）
 */
int preset_representation_theory_count(void);

/**
 * @brief 获取表示论预设的类别
 *
 * @return PresetCategory 预设类别（PRESET_CATEGORY_GROUP_THEORY）
 */
PresetCategory preset_representation_theory_category(void);

/**
 * @brief 获取表示论预设名称列表
 *
 * 返回堆分配的预设名称数组，调用者需释放每个元素和数组本身。
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败（out_names 或 out_count 为 NULL，或内存不足）
 */
bool preset_representation_theory_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* PRESET_REPRESENTATION_THEORY_H */
