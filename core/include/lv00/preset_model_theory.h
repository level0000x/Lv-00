/**
 * @file preset_model_theory.h
 * @brief 模型论预设函数块 - 头文件
 *
 * @details 为理论数学研究提供模型论领域的预设函数块，
 *          包括结构构造、初等等价、紧致性、完备性等。
 *
 * 本模块涵盖：
 * - 结构理论：结构构造、子结构、同构
 * - 初等等价：初等等价、初等子结构、Tarski-Vaught测试
 * - 模型构造：常量扩展、类型实现、饱和模型
 * - 紧致性：紧致性定理、力量子化
 * - 稳定性理论：稳定、不稳定、omega-稳定
 * - 量词消去：量词消去算法、代数闭包
 *
 * @module ModelTheory
 * @category PRESET_CATEGORY_MATH_LOGIC
 * @version 13.0.0
 */

#ifndef LV00_PRESET_MODEL_THEORY_H
#define LV00_PRESET_MODEL_THEORY_H

#include "preset_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设函数块名称常量定义
 * ============================================================ */

/**
 * @defgroup group_presets_model_theory 模型论预设名称
 * @{
 */

/* -------------------- 结构理论 -------------------- */
/** 子结构判定 */
#define PRESET_STRUCTURE_SUBSTRUCTURE "structure_substructure"
/** 嵌入构造 */
#define PRESET_STRUCTURE_EMBEDDING "structure_embedding"
/** 同构判定 */
#define PRESET_STRUCTURE_ISOMORPHISM "structure_isomorphism"
/** 初等嵌入 */
#define PRESET_STRUCTURE_ELEMENTARY_EMBEDDING "structure_elementary_embedding"
/** 归约计算 */
#define PRESET_STRUCTURE_REDUCTION "structure_reduction"
/** 膨胀计算 */
#define PRESET_STRUCTURE_EXPANSION "structure_expansion"
/** 消去域计算 */
#define PRESET_STRUCTURE_ELIMINATION_DOMAIN "structure_elimination_domain"
/** 代数闭包 */
#define PRESET_STRUCTURE_ALGEBRAIC_CLOSURE "structure_algebraic_closure"
/** definable闭包 */
#define PRESET_STRUCTURE_DEFINABLE_CLOSURE "structure_definable_closure"

/* -------------------- 初等等价 -------------------- */
/** 初等等价判定 */
#define PRESET_ELEMENTARY_EQUIVALENCE "elementary_equivalence"
/** 初等子结构判定 */
#define PRESET_ELEMENTARY_SUBSTRUCTURE "elementary_substructure"
/** Tarski-Vaught测试 */
#define PRESET_TARSKI_VAUGHT_TEST "tarski_vaught_test"
/** Scott同构定理 */
#define PRESET_SCOTT_ISOMORPHISM "scott_isomorphism"
/** Back-and-forth系统 */
#define PRESET_BACK_FORTH_SYSTEM "back_forth_system"
/** Ehrenfeucht-Fraisse游戏 */
#define PRESET_EHRENFEUCHT_GAME "ehrenfeucht_game"

/* -------------------- 模型构造 -------------------- */
/** 常量扩展 */
#define PRESET_MODEL_CONSTANT_EXTENSION "model_constant_extension"
/** 类型实现 */
#define PRESET_MODEL_TYPE_REALIZATION "model_type_realization"
/** 素模型构造 */
#define PRESET_MODEL_PRIME_MODEL "model_prime_model"
/** 饱和模型构造 */
#define PRESET_MODEL_SATURATED_MODEL "model_saturated_model"
/** 均值模型构造 */
#define PRESET_MODEL_UNIVERSAL_MODEL "model_universal_model"
/** 原子模型构造 */
#define PRESET_MODEL_ATOMIC_MODEL "model_atomic_model"
/** 模型构造（Henkin构造） */
#define PRESET_MODEL_CONSTRUCTION "model_construction"

/* -------------------- 紧致性 -------------------- */
/** 紧致性检验 */
#define PRESET_COMPACTNESS "compactness"
/** 力量子化 */
#define PRESET_POWER_QUANTIFIER "power_quantifier"
/** Lindenbaum代数 */
#define PRESET_LINDENBAUM_ALGEBRA "lindenbaum_algebra"
/** 完备化构造 */
#define PRESET_COMPLETION_CONSTRUCTION "completion_construction"
/** Ultrafilter扩展 */
#define PRESET_ULTRAFILTER_EXTENSION "ultrafilter_extension"
/** ultraproduct */
#define PRESET_ULTRAPRODUCT "ultraproduct"
/** ultrapower */
#define PRESET_ULTRAPOWER "ultrapower"

/* -------------------- 稳定性理论 -------------------- */
/** 稳定性判定 */
#define PRESET_STABILITY "stability"
/** 不稳定性证明 */
#define PRESET_INSTABILITY "instability"
/** omega-稳定性判定 */
#define PRESET_OMEGA_STABILITY "omega_stability"
/** superstable判定 */
#define PRESET_SUPERSTABLE "superstable"
/** 稳定型计算 */
#define PRESET_STABLE_TYPE "stable_type"
/** 分叉点计算 */
#define PRESET_FORKING "forking"
/** 强极小集合 */
#define PRESET_STRONG_MINIMAL "strong_minimal"

/* -------------------- 量词消去 -------------------- */
/** 量词消去 */
#define PRESET_QUANTIFIER_ELIMINATION "quantifier_elimination"
/** 模型完备性检验 */
#define PRESET_MODEL_COMPLETENESS "model_completeness"
/** 互模拟判定 */
#define PRESET_BISIMULATION "bisimulation"
/** 可判定性检验 */
#define PRESET_DECIDABILITY "decidability"
/** 代数性判定 */
#define PRESET_ALGEBRAICITY "algebraicity"
/** 传递模型 */
#define PRESET_TRANSITIVE_MODEL "transitive_model"

 /** @} */

/* ============================================================
 * 预设数量常量
 * ============================================================ */

/** 模型论模块预设函数块总数 */
#define MODEL_THEORY_PRESET_COUNT 38

/* ============================================================
 * 模块接口函数声明
 * ============================================================ */

/**
 * @brief 注册模型论模块的所有预设函数块
 *
 * @return true 所有预设注册成功，false 部分失败
 */
bool preset_model_theory_register(void);

/**
 * @brief 获取模型论预设函数块数量
 *
 * @return int 模型论模块预设函数块总数
 */
int preset_model_theory_count(void);

/**
 * @brief 获取模型论模块的预设类别
 *
 * @return PresetCategory 模型论模块所属类别
 */
PresetCategory preset_model_theory_category(void);

/**
 * @brief 获取模型论预设函数块名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 获取成功
 * @return false 参数无效或内存不足
 */
bool preset_model_theory_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_MODEL_THEORY_H */
