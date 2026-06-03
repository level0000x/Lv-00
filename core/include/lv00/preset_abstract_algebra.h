/**
 * @file preset_abstract_algebra.h
 * @brief 抽象代数预设函数块 - 头文件
 *
 * @details 为理论数学研究提供抽象代数领域的预设函数块，
 *          包括群论、环论、域论等代数结构的运算和性质判定。
 *
 * 本模块涵盖：
 * - 群论：循环群生成元、同构判定、子群检验
 * - 环论：理想构造、商环、环同态
 * - 域论：域扩张、伽罗瓦群、最小多项式
 * - 模论：模同态、正合序列、Hom函子
 * - 表示论基础：群表示、特征标
 *
 * @module AbstractAlgebra
 * @category PRESET_CATEGORY_ALGEBRA
 * @version 13.0.0
 */

#ifndef LV00_PRESET_ABSTRACT_ALGEBRA_H
#define LV00_PRESET_ABSTRACT_ALGEBRA_H

#include "preset_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设函数块名称常量定义
 * ============================================================ */

/**
 * @defgroup group_presets_abstract_algebra 抽象代数预设名称
 * @{
 */

/* -------------------- 群论运算 -------------------- */
/** 循环群生成元计算 */
#define PRESET_GROUP_CYCLIC_GENERATOR "group_cyclic_generator"
/** 群的阶计算 */
#define PRESET_GROUP_ORDER "group_order"
/** 元素阶计算 */
#define PRESET_GROUP_ELEMENT_ORDER "group_element_order"
/** 陪集构造 */
#define PRESET_GROUP_COSET "group_coset"
/** 正规子群检验 */
#define PRESET_GROUP_NORMAL_SUBGROUP "group_normal_subgroup"
/** 商群构造 */
#define PRESET_GROUP_QUOTIENT "group_quotient"
/** 同构判定 */
#define PRESET_GROUP_ISOMORPHISM "group_isomorphism"
/** 自同构群计算 */
#define PRESET_GROUP_AUTOMORPHISM "group_automorphism"
/** 共轭类计算 */
#define PRESET_GROUP_CONJUGACY_CLASS "group_conjugacy_class"
/** 中心化子计算 */
#define PRESET_GROUP_CENTRALIZER "group_centralizer"
/** 换位子计算 */
#define PRESET_GROUP_COMMUTATOR "group_commutator"
/** 导群计算 */
#define PRESET_GROUP_DERIVED_SUBGROUP "group_derived_subgroup"
/** Sylow子群检验 */
#define PRESET_GROUP_SYLOW_SUBGROUP "group_sylow_subgroup"

/* -------------------- 环论运算 -------------------- */
/** 理想构造 */
#define PRESET_RING_IDEAL "ring_ideal"
/** 主理想环生成 */
#define PRESET_RING_PRINCIPAL_IDEAL "ring_principal_ideal"
/** 商环构造 */
#define PRESET_RING_QUOTIENT "ring_quotient"
/** 环同态核 */
#define PRESET_RING_HOMOMORPHISM_KERNEL "ring_homomorphism_kernel"
/** 环同态像 */
#define PRESET_RING_HOMOMORPHISM_IMAGE "ring_homomorphism_image"
/** 素理想检验 */
#define PRESET_RING_PRIME_IDEAL "ring_prime_ideal"
/** 极大理想检验 */
#define PRESET_RING_MAXIMAL_IDEAL "ring_maximal_ideal"
/** Jacobson根 */
#define PRESET_RING_JACOBSON_RADICAL "ring_jacobson_radical"
/** Nilradical */
#define PRESET_RING_NILRADICAL "ring_nilradical"
/** 分式域构造 */
#define PRESET_RING_FRACTION_FIELD "ring_fraction_field"

/* -------------------- 域论运算 -------------------- */
/** 域扩张次数 */
#define PRESET_FIELD_EXTENSION_DEGREE "field_extension_degree"
/** 最小多项式 */
#define PRESET_FIELD_MINIMAL_POLYNOMIAL "field_minimal_polynomial"
/** 代数元共轭 */
#define PRESET_FIELD_CONJUGATE "field_conjugate"
/** 分裂域构造 */
#define PRESET_FIELD_SPLITTING_FIELD "field_splitting_field"
/** 伽罗瓦群计算 */
#define PRESET_FIELD_GALOIS_GROUP "field_galois_group"
/** 伽罗瓦对应 */
#define PRESET_FIELD_GALOIS_CORRESPONDENCE "field_galois_correspondence"
/** 可分扩张检验 */
#define PRESET_FIELD_SEPARABLE_EXTENSION "field_separable_extension"
/** 正规基计算 */
#define PRESET_FIELD_NORMAL_BASIS "field_normal_basis"

/* -------------------- 模论运算 -------------------- */
/** 自由模秩计算 */
#define PRESET_MODULE_FREE_RANK "module_free_rank"
/** 子模构造 */
#define PRESET_MODULE_SUBMODULE "module_submodule"
/** 商模构造 */
#define PRESET_MODULE_QUOTIENT "module_quotient"
/** 模同态核 */
#define PRESET_MODULE_HOMOMORPHISM_KERNEL "module_homomorphism_kernel"
/** 模同态像 */
#define PRESET_MODULE_HOMOMORPHISM_IMAGE "module_homomorphism_image"
/** Hom函子 */
#define PRESET_MODULE_HOM "module_hom"
/** 张量积 */
#define PRESET_MODULE_TENSOR_PRODUCT "module_tensor_product"
/** 正合序列检验 */
#define PRESET_MODULE_EXACT_SEQUENCE "module_exact_sequence"

/* -------------------- 表示论基础 -------------------- */
/** 群表示构造 */
#define PRESET_REPRESENTATION_GROUP "representation_group"
/** 表示等价判定 */
#define PRESET_REPRESENTATION_EQUIVALENCE "representation_equivalence"
/** 特征标计算 */
#define PRESET_REPRESENTATION_CHARACTER "representation_character"
/** 表示分解 */
#define PRESET_REPRESENTATION_DECOMPOSITION "representation_decomposition"

/** @} */

/* ============================================================
 * 预设数量常量
 * ============================================================ */

/** 抽象代数模块预设函数块总数 */
#define ABSTRACT_ALGEBRA_PRESET_COUNT 40

/* ============================================================
 * 模块接口函数声明
 * ============================================================ */

/**
 * @brief 注册抽象代数模块的所有预设函数块
 *
 * @return true 所有预设注册成功，false 部分失败
 */
bool preset_abstract_algebra_register(void);

/**
 * @brief 获取抽象代数预设函数块数量
 *
 * @return int 抽象代数模块预设函数块总数
 */
int preset_abstract_algebra_count(void);

/**
 * @brief 获取抽象代数模块的预设类别
 *
 * @return PresetCategory 抽象代数模块所属类别
 */
PresetCategory preset_abstract_algebra_category(void);

/**
 * @brief 获取抽象代数预设函数块名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 获取成功
 * @return false 参数无效或内存不足
 */
bool preset_abstract_algebra_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_ABSTRACT_ALGEBRA_H */
