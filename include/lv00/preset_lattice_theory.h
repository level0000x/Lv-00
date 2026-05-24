/**
 * @file preset_lattice_theory.h
 * @brief 格论预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的格论运算预设函数块，包括：
 *   - 格基础运算：上确界（并）、下确界（交）、最大元、最小元、补元
 *   - 格结构判定：偏序关系判定、格结构判定、有界格、分配格、模格判定
 *   - 特殊格：布尔代数、Heyting代数、完备格、格理想
 *   - 格同态与表示：格同态、格嵌入、格同构、子格、直积、对偶性、Stone表示
 *   - 格与序：Hasse图、链判定、反链判定、格的高度、格的宽度
 *
 * @module LatticeTheory
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_LATTICE_THEORY_H
#define LV00_PRESET_LATTICE_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 格基础运算 -------------------- */

/** 上确界（并）：a ∨ b */
#define PRESET_LATTICE_JOIN                "lattice_join"

/** 下确界（交）：a ∧ b */
#define PRESET_LATTICE_MEET                "lattice_meet"

/** 最大元（顶）：⊤ */
#define PRESET_LATTICE_TOP                 "lattice_top"

/** 最小元（底）：⊥ */
#define PRESET_LATTICE_BOTTOM              "lattice_bottom"

/** 补元：a' */
#define PRESET_LATTICE_COMPLEMENT          "lattice_complement"

/** 偏序关系判定 */
#define PRESET_LATTICE_PARTIAL_ORDER       "lattice_partial_order"

/** 格结构判定 */
#define PRESET_LATTICE_CHECK               "lattice_check"

/** 有界格判定 */
#define PRESET_LATTICE_BOUNDED_CHECK       "lattice_bounded_check"

/** 分配格判定 */
#define PRESET_LATTICE_DISTRIBUTIVE_CHECK  "lattice_distributive_check"

/** 模格判定 */
#define PRESET_LATTICE_MODULAR_CHECK       "lattice_modular_check"

/* -------------------- 特殊格 -------------------- */

/** 布尔代数判定 */
#define PRESET_BOOLEAN_ALGEBRA_CHECK       "boolean_algebra_check"

/** 布尔代数运算 */
#define PRESET_BOOLEAN_ALGEBRA_OPERATIONS  "boolean_algebra_operations"

/** Heyting代数判定 */
#define PRESET_HEYTING_ALGEBRA_CHECK       "heyting_algebra_check"

/** Heyting蕴涵 */
#define PRESET_HEYTING_IMPLICATION         "heyting_implication"

/** 完备格判定 */
#define PRESET_COMPLETE_LATTICE_CHECK      "complete_lattice_check"

/** 完备格上确界 */
#define PRESET_COMPLETE_LATTICE_SUP        "complete_lattice_sup"

/** 完备格下确界 */
#define PRESET_COMPLETE_LATTICE_INF        "complete_lattice_inf"

/** 格理想 */
#define PRESET_LATTICE_IDEAL               "lattice_ideal"

/* -------------------- 格同态与表示 -------------------- */

/** 格同态 */
#define PRESET_LATTICE_HOMOMORPHISM        "lattice_homomorphism"

/** 格嵌入 */
#define PRESET_LATTICE_EMBEDDING           "lattice_embedding"

/** 格同构判定 */
#define PRESET_LATTICE_ISOMORPHISM_CHECK   "lattice_isomorphism_check"

/** 子格判定 */
#define PRESET_LATTICE_SUBLATTICE_CHECK    "lattice_sublattice_check"

/** 格直积 */
#define PRESET_LATTICE_PRODUCT             "lattice_product"

/** 格对偶性 */
#define PRESET_LATTICE_DUALITY             "lattice_duality"

/** Stone表示定理 */
#define PRESET_STONE_REPRESENTATION        "stone_representation"

/* -------------------- 格与序 -------------------- */

/** Hasse图 */
#define PRESET_HASSE_DIAGRAM               "hasse_diagram"

/** 链判定（全序） */
#define PRESET_CHAIN_CHECK                 "chain_check"

/** 反链判定 */
#define PRESET_ANTICHAIN_CHECK             "antichain_check"

/** 格的高度 */
#define PRESET_LATTICE_HEIGHT              "lattice_height"

/** 格的宽度 */
#define PRESET_LATTICE_WIDTH               "lattice_width"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有格论预设函数块
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_lattice_theory_register(void);

/**
 * @brief 获取格论预设函数块数量
 *
 * @return int 格论模块预设函数块总数
 */
int preset_lattice_theory_count(void);

/**
 * @brief 获取格论预设函数块名称列表
 *
 * @param out_names 输出名称数组指针（调用者负责释放）
 * @param out_count 输出预设数量
 * @return true 获取成功
 * @return false 获取失败
 */
bool preset_lattice_theory_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取格论模块类别名称
 *
 * @return 类别名称字符串
 */
const char *preset_lattice_theory_category(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_LATTICE_THEORY_H */
