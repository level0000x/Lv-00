/**
 * @file preset_mathematical_logic.h
 * @brief 数理逻辑预设函数块 - 兼容性别名
 *
 * 本文件为 preset_math_logic.h 的兼容性别名。
 * 新代码请直接使用 preset_math_logic.h。
 *
 * 历史说明：
 * - preset_mathematical_logic.h（v2.0.0）定义了 40 个预设块，使用 prop_/fol_/proof_/model_ 前缀
 * - preset_math_logic.h（v4.0.0）重新设计了 20 个预设块，使用 logic_ 前缀，注释更完善
 * - 两者功能重叠但命名不同，为避免混淆，本文件重定向至 preset_math_logic.h
 *
 * @module MathematicalLogic (兼容性)
 * @category PRESET_CATEGORY_LOGIC
 * @version 3.3.0
 * @author Lv-00 开发团队
 * @deprecated 请使用 preset_math_logic.h 替代
 */

#ifndef LV00_PRESET_MATHEMATICAL_LOGIC_H
#define LV00_PRESET_MATHEMATICAL_LOGIC_H

#include "preset_math_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 兼容性别名宏
 * 将旧版 preset_mathematical_logic.h 的宏名映射到新版
 * ============================================================ */

/* -------------------- 命题逻辑兼容别名 -------------------- */

/** @deprecated 使用 PRESET_LOGIC_CONJUNCTION 替代 */
#define PRESET_PROP_AND PRESET_LOGIC_CONJUNCTION

/** @deprecated 使用 PRESET_LOGIC_DISJUNCTION 替代 */
#define PRESET_PROP_OR PRESET_LOGIC_DISJUNCTION

/** @deprecated 使用 PRESET_LOGIC_NEGATION 替代 */
#define PRESET_PROP_NOT PRESET_LOGIC_NEGATION

/** @deprecated 使用 PRESET_LOGIC_IMPLICATION 替代 */
#define PRESET_PROP_IMPLIES PRESET_LOGIC_IMPLICATION

/** @deprecated 使用 PRESET_LOGIC_BICONDITIONAL 替代 */
#define PRESET_PROP_IFF PRESET_LOGIC_BICONDITIONAL

/** @deprecated 使用 PRESET_LOGIC_TAUTOLOGY_TEST 替代 */
#define PRESET_PROP_TAUTOLOGY_CHECK PRESET_LOGIC_TAUTOLOGY_TEST

/** @deprecated 使用 PRESET_LOGIC_SATISFIABILITY_TEST 替代 */
#define PRESET_PROP_SATISFIABLE_CHECK PRESET_LOGIC_SATISFIABILITY_TEST

/* -------------------- 一阶逻辑兼容别名 -------------------- */

/** @deprecated 使用 PRESET_LOGIC_UNIVERSAL_INSTANTIATION 替代 */
#define PRESET_FOL_FORALL PRESET_LOGIC_UNIVERSAL_GENERALIZATION

/** @deprecated 使用 PRESET_LOGIC_EXISTENTIAL_GENERALIZATION 替代 */
#define PRESET_FOL_EXISTS PRESET_LOGIC_EXISTENTIAL_GENERALIZATION

/** @deprecated 使用 PRESET_LOGIC_UNIVERSAL_INSTANTIATION 替代 */
#define PRESET_FOL_UNIVERSAL_INSTANTIATION PRESET_LOGIC_UNIVERSAL_INSTANTIATION

/** @deprecated 使用 PRESET_LOGIC_EXISTENTIAL_GENERALIZATION 替代 */
#define PRESET_FOL_EXISTENTIAL_GENERALIZATION PRESET_LOGIC_EXISTENTIAL_GENERALIZATION

/* -------------------- 证明论兼容别名 -------------------- */

/** @deprecated 使用 PRESET_LOGIC_NATURAL_DEDUCTION 替代 */
#define PRESET_PROOF_MODUS_PONENS "proof_modus_ponens"

/** @deprecated 使用 PRESET_LOGIC_RESOLUTION 替代 */
#define PRESET_PROOF_MODUS_TOLLENS "proof_modus_tollens"

/** @deprecated 使用 PRESET_LOGIC_NATURAL_DEDUCTION 替代 */
#define PRESET_PROOF_NATURAL_DEDUCTION PRESET_LOGIC_NATURAL_DEDUCTION

/* -------------------- 模型论兼容别名 -------------------- */

/** @deprecated 使用 PRESET_LOGIC_MODEL_CHECK 替代 */
#define PRESET_MODEL_SATISFIES PRESET_LOGIC_MODEL_CHECK

/** @deprecated 使用 PRESET_LOGIC_VALIDITY_TEST 替代 */
#define PRESET_MODEL_THEORY_CHECK PRESET_LOGIC_VALIDITY_TEST

/* -------------------- 递归论兼容别名 -------------------- */

/** @deprecated 使用 PRESET_LOGIC_TURING_MACHINE_CHECK 替代 */
#define PRESET_TURING_MACHINE_SIMULATE PRESET_LOGIC_TURING_MACHINE_CHECK

/** @deprecated 使用 PRESET_LOGIC_HALTING_PROBLEM 替代 */
#define PRESET_HALTING_PROBLEM PRESET_LOGIC_HALTING_PROBLEM

/** @deprecated 使用 PRESET_LOGIC_HALTING_PROBLEM 替代 */
#define PRESET_COMPUTABLE_FUNCTION_CHECK "computable_function_check"

/** @deprecated 使用 PRESET_LOGIC_RECURSIVE_CHECK 替代 */
#define PRESET_RECURSIVE_ENUMERABLE_CHECK "recursive_enumerable_check"

/** @deprecated 使用 PRESET_LOGIC_RECURSIVE_CHECK 替代 */
#define PRESET_DECIDABILITY_CHECK "decidability_check"

/* ============================================================
 * 兼容性注册函数
 * ============================================================ */

/**
 * @brief 注册所有数理逻辑预设函数块（兼容性接口）
 *
 * @deprecated 请直接调用 preset_math_logic_register() 替代
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
static inline bool preset_mathematical_logic_register(void) {
    return preset_math_logic_register();
}

/**
 * @brief 获取数理逻辑预设函数块数量（兼容性接口）
 *
 * @deprecated 请直接调用 preset_math_logic_count() 替代
 * @return int 数理逻辑模块预设函数块总数
 */
static inline int preset_mathematical_logic_count(void) {
    return preset_math_logic_count();
}

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_MATHEMATICAL_LOGIC_H */
