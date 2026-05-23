/**
 * @file preset_mathematical_logic.h
 * @brief 数理逻辑预设函数块模块 - 头文件
 *
 * 提供理论数学研究项目Lv-00中数理逻辑领域的预设函数块，包括：
 *   - 命题逻辑（12个）：合取、析取、否定、蕴涵、等价、异或、
 *     与非、或非、重言式判定、矛盾式判定、可满足性判定、析取范式转换
 *   - 一阶逻辑（10个）：全称量化、存在量化、量词否定、项代入、
 *     自由变量检查、约束变量检查、全称实例化、存在泛化、
 *     前束范式、Skolem范式
 *   - 证明论（8个）：假言推理、否定后件、合取引入、析取消除、
 *     归谬法、条件证明、反证法、自然演绎系统
 *   - 模型论（5个）：模型满足关系、理论一致性判定、初等等价、
 *     紧致性定理、Lowenheim-Skolem定理
 *   - 递归论（5个）：可计算函数判定、图灵机模拟、停机问题、
 *     递归可枚举判定、可判定性检查
 *
 * @module MathematicalLogic
 * @category PRESET_CATEGORY_LOGIC
 * @version 2.0.0
 * @author Lv-00 开发团队
 */

#ifndef PRESET_MATHEMATICAL_LOGIC_H
#define PRESET_MATHEMATICAL_LOGIC_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 命题逻辑（12个） -------------------- */

/** 合取：P ∧ Q */
#define PRESET_PROP_AND                   "prop_and"

/** 析取：P ∨ Q */
#define PRESET_PROP_OR                    "prop_or"

/** 否定：¬P */
#define PRESET_PROP_NOT                   "prop_not"

/** 蕴涵：P → Q */
#define PRESET_PROP_IMPLIES               "prop_implies"

/** 等价：P ↔ Q */
#define PRESET_PROP_IFF                   "prop_iff"

/** 异或：P ⊕ Q */
#define PRESET_PROP_XOR                   "prop_xor"

/** 与非：P ↑ Q（Sheffer竖线） */
#define PRESET_PROP_NAND                  "prop_nand"

/** 或非：P ↓ Q（Peirce箭头） */
#define PRESET_PROP_NOR                   "prop_nor"

/** 重言式判定 */
#define PRESET_PROP_TAUTOLOGY_CHECK       "prop_tautology_check"

/** 矛盾式判定 */
#define PRESET_PROP_CONTRADICTION_CHECK   "prop_contradiction_check"

/** 可满足性判定（SAT） */
#define PRESET_PROP_SATISFIABLE_CHECK     "prop_satisfiable_check"

/** 析取范式转换（DNF） */
#define PRESET_PROP_DNF                   "prop_dnf"

/* -------------------- 一阶逻辑（10个） -------------------- */

/** 全称量化：∀x P(x) */
#define PRESET_FOL_FORALL                 "fol_forall"

/** 存在量化：∃x P(x) */
#define PRESET_FOL_EXISTS                 "fol_exists"

/** 量词否定 */
#define PRESET_FOL_NEGATE_QUANTIFIER      "fol_negate_quantifier"

/** 项代入 */
#define PRESET_FOL_SUBSTITUTION           "fol_substitution"

/** 自由变量检查 */
#define PRESET_FOL_FREE_VARIABLE_CHECK    "fol_free_variable_check"

/** 约束变量检查 */
#define PRESET_FOL_BOUND_VARIABLE_CHECK   "fol_bound_variable_check"

/** 全称实例化 */
#define PRESET_FOL_UNIVERSAL_INSTANTIATION "fol_universal_instantiation"

/** 存在泛化 */
#define PRESET_FOL_EXISTENTIAL_GENERALIZATION "fol_existential_generalization"

/** 前束范式（PNF） */
#define PRESET_FOL_PRENEX_NORMAL_FORM     "fol_prenex_normal_form"

/** Skolem范式 */
#define PRESET_FOL_SKOLEM_NORMAL_FORM     "fol_skolem_normal_form"

/* -------------------- 证明论（8个） -------------------- */

/** 假言推理（Modus Ponens） */
#define PRESET_PROOF_MODUS_PONENS         "proof_modus_ponens"

/** 否定后件（Modus Tollens） */
#define PRESET_PROOF_MODUS_TOLLENS        "proof_modus_tollens"

/** 合取引入（∧I） */
#define PRESET_PROOF_CONJUNCTION_INTRO    "proof_conjunction_intro"

/** 析取消除（∨E） */
#define PRESET_PROOF_DISJUNCTION_ELIM     "proof_disjunction_elim"

/** 归谬法 */
#define PRESET_PROOF_REDUCTIO_AD_ABSURDUM "proof_reductio_ad_absurdum"

/** 条件证明 */
#define PRESET_PROOF_CONDITIONAL_PROOF    "proof_conditional_proof"

/** 反证法 */
#define PRESET_PROOF_BY_CONTRADICTION     "proof_by_contradiction"

/** 自然演绎系统 */
#define PRESET_PROOF_NATURAL_DEDUCTION    "proof_natural_deduction"

/* -------------------- 模型论（5个） -------------------- */

/** 模型满足关系：M ⊨ φ */
#define PRESET_MODEL_SATISFIES            "model_satisfies"

/** 理论一致性判定 */
#define PRESET_MODEL_THEORY_CHECK         "model_theory_check"

/** 初等等价 */
#define PRESET_MODEL_ELEMENTARY_EQUIVALENCE "model_elementary_equivalence"

/** 紧致性定理 */
#define PRESET_MODEL_COMPACTNESS          "model_compactness"

/** Lowenheim-Skolem定理 */
#define PRESET_MODEL_LOWENHEIM_SKOLEM     "model_lowenheim_skolem"

/* -------------------- 递归论（5个） -------------------- */

/** 可计算函数判定 */
#define PRESET_COMPUTABLE_FUNCTION_CHECK  "computable_function_check"

/** 图灵机模拟 */
#define PRESET_TURING_MACHINE_SIMULATE    "turing_machine_simulate"

/** 停机问题 */
#define PRESET_HALTING_PROBLEM            "halting_problem"

/** 递归可枚举判定 */
#define PRESET_RECURSIVE_ENUMERABLE_CHECK "recursive_enumerable_check"

/** 可判定性检查 */
#define PRESET_DECIDABILITY_CHECK         "decidability_check"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有数理逻辑预设函数块
 *
 * 将数理逻辑模块的全部40个预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_mathematical_logic_register(void);

/**
 * @brief 获取数理逻辑预设函数块数量
 *
 * @return int 数理逻辑模块预设函数块总数（40）
 */
int preset_mathematical_logic_count(void);

#ifdef __cplusplus
}
#endif

#endif /* PRESET_MATHEMATICAL_LOGIC_H */
