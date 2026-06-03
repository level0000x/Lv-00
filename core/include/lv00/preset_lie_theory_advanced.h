/**
 * @file preset_lie_theory_advanced.h
 * @brief 李理论预设函数块 - 头文件
 *
 * @details 提供李理论相关的预设函数块，包括：
 *          - 李代数基础（括号运算、理想、同态）
 *          - 半单李代数（根系、Weyl群、Cartan矩阵）
 *          - 李代数表示（不可约表示、特征标）
 *          - 泛包络代数与PBW定理
 *          - 李群与李代数的对应
 *
 * @module LieTheoryAdvanced
 * @category PRESET_CATEGORY_ALGEBRA
 * @version 1.0.0
 * @author Lv-00 Project
 */

#ifndef LV00_PRESET_LIE_THEORY_ADVANCED_H
#define LV00_PRESET_LIE_THEORY_ADVANCED_H

#include "func_block_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义 - 李代数基础
 * ============================================================ */

/** 李括号：计算李代数中两个元素的括号 [X, Y] */
#define PRESET_LIE_BRACKET "lie_bracket"

/** 李代数理想：判定子空间是否为理想 */
#define PRESET_LIE_ALGEBRA_IDEAL "lie_algebra_ideal"

/** 李代数同态：判定映射是否为李代数同态 */
#define PRESET_LIE_ALGEBRA_HOMOMORPHISM "lie_algebra_homomorphism"

/** 导出代数：计算导出代数 [L, L] */
#define PRESET_DERIVED_ALGEBRA "derived_algebra"

/** 李代数的中心：计算李代数的中心 Z(L) */
#define PRESET_LIE_ALGEBRA_CENTER "lie_algebra_center"

/* ============================================================
 * 预设名称常量定义 - 半单李代数
 * ============================================================ */

/** 根系：计算半单李代数的根系 Φ */
#define PRESET_ROOT_SYSTEM "root_system"

/** 单根：计算根系的基础单根 Δ */
#define PRESET_SIMPLE_ROOTS "simple_roots"

/** Weyl群：计算根系的Weyl群 W */
#define PRESET_WEYL_GROUP "weyl_group"

/** Cartan矩阵：计算单根的Cartan矩阵 */
#define PRESET_CARTAN_MATRIX "cartan_matrix"

/** Dynkin图：构造根系的Dynkin图 */
#define PRESET_DYNKIN_DIAGRAM "dynkin_diagram"

/* ============================================================
 * 预设名称常量定义 - 李代数表示
 * ============================================================ */

/** 不可约表示：构造最高权表示 V(λ) */
#define PRESET_IRREDUCIBLE_REPRESENTATION "irreducible_representation"

/** 最高权：计算表示的最高权 */
#define PRESET_HIGHEST_WEIGHT "highest_weight"

/** 权空间：计算表示的权空间分解 */
#define PRESET_WEIGHT_SPACE_DECOMPOSITION "weight_space_decomposition"

/** 特征标公式：计算表示的特征标（Weyl特征标公式） */
#define PRESET_CHARACTER_FORMULA "character_formula"

/** 维数公式：计算不可约表示的维数（Weyl维数公式） */
#define PRESET_DIMENSION_FORMULA "dimension_formula"

/* ============================================================
 * 预设名称常量定义 - 泛包络代数
 * ============================================================ */

/** 泛包络代数：构造李代数的泛包络代数 U(L) */
#define PRESET_UNIVERSAL_ENVELOPING_ALGEBRA "universal_enveloping_algebra"

/** PBW基：构造PBW基 */
#define PRESET_PBW_BASIS "pbw_basis"

/** Casimir算子：构造Casimir算子 */
#define PRESET_CASIMIR_OPERATOR "casimir_operator"

/** 中心特征标：计算中心 Z(U(L)) 的特征标 */
#define PRESET_INFinitESIMAL_CHARACTER "infinitesimal_character"

/** Verma模：构造Verma模 M(λ) */
#define PRESET_VERMA_MODULE "verma_module"

/* ============================================================
 * 预设名称常量定义 - 李群与李代数
 * ============================================================ */

/** 李代数化：计算李群的李代数 Lie(G) */
#define PRESET_LIE_ALGEBRIZATION "lie_algebrization"

/** 指数映射：计算李代数到李群的指数映射 exp: L → G */
#define PRESET_EXPONENTIAL_MAP "exponential_map"

/** 伴随表示：计算李代数的伴随表示 ad: L → gl(L) */
#define PRESET_ADJOINT_REPRESENTATION "adjoint_representation"

/** Killing型：计算李代数的Killing型 κ(x, y) = tr(ad x ∘ ad y) */
#define PRESET_KILLING_FORM "killing_form"

/** 半单判定：判定李代数是否半单（Killing型非退化） */
#define PRESET_SEMISIMPLE_CHECK "semisimple_check"

/* ============================================================
 * 模块接口
 * ============================================================ */

/**
 * @brief 注册所有李理论预设函数块
 *
 * @return true  所有预设注册成功
 * @return false 部分或全部预设注册失败
 */
bool preset_lie_theory_advanced_register(void);

/**
 * @brief 获取李理论预设函数块数量
 *
 * @return int 预设数量（固定为 25）
 */
int preset_lie_theory_advanced_count(void);

/**
 * @brief 获取李理论预设的类别
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_ALGEBRA
 */
PresetCategory preset_lie_theory_advanced_category(void);

/**
 * @brief 获取李理论预设名称列表
 *
 * @param out_names 输出名称数组
 * @param out_count 输出名称数量
 * @return true 成功获取
 * @return false 失败
 */
bool preset_lie_theory_advanced_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_LIE_THEORY_ADVANCED_H */
