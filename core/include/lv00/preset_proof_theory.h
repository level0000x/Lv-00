/**
 * @file preset_proof_theory.h
 * @brief 证明论预设函数块 - 头文件
 *
 * @details 为理论数学研究提供证明论领域的预设函数块，
 *          包括自然推理、矢列演算、证明转换和证明分析等。
 *
 * 本模块涵盖：
 * - 自然推理系统：引入规则、消除规则、证明构造
 * - 矢列演算：矢列推导、证明搜索
 * - 证明转换：SKI组合子、证明规范化
 * - 证明分析：证明搜索、正规形式
 * - 类型论基础：Curry-Howard同构、lambda演算
 *
 * @module ProofTheory
 * @category PRESET_CATEGORY_MATH_LOGIC
 * @version 13.0.0
 */

#ifndef LV00_PRESET_PROOF_THEORY_H
#define LV00_PRESET_PROOF_THEORY_H

#include "preset_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设函数块名称常量定义
 * ============================================================ */

/**
 * @defgroup group_presets_proof_theory 证明论预设名称
 * @{
 */

/* -------------------- 自然推理规则 -------------------- */
/** 蕴含引入（条件证明） */
#define PRESET_PROOF_IMPLIES_INTRO "proof_implies_intro"
/** 蕴含消除（分离规则） */
#define PRESET_PROOF_IMPLIES_ELIM "proof_implies_elim"
/** 合取引入 */
#define PRESET_PROOF_CONJ_INTRO "proof_conj_intro"
/** 合取消去 */
#define PRESET_PROOF_CONJ_ELIM "proof_conj_elim"
/** 析取引入 */
#define PRESET_PROOF_DISJ_INTRO "proof_disj_intro"
/** 析取消去 */
#define PRESET_PROOF_DISJ_ELIM "proof_disj_elim"
/** 否定引入（反证法） */
#define PRESET_PROOF_NOT_INTRO "proof_not_intro"
/** 否定消除 */
#define PRESET_PROOF_NOT_ELIM "proof_not_elim"
/** 双重否定消除 */
#define PRESET_PROOF_DNE "proof_dne"
/** 排中律 */
#define PRESET_PROOF_LEM "proof_lem"
/** 全称量化引入 */
#define PRESET_PROOF_FORALL_INTRO "proof_forall_intro"
/** 全称量化消除 */
#define PRESET_PROOF_FORALL_ELIM "proof_forall_elim"
/** 存在量化引入 */
#define PRESET_PROOF_EXISTS_INTRO "proof_exists_intro"
/** 存在量化消除 */
#define PRESET_PROOF_EXISTS_ELIM "proof_exists_elim"
/** 等词引入 */
#define PRESET_PROOF_EQ_INTRO "proof_eq_intro"
/** 等词消除（替换规则） */
#define PRESET_PROOF_EQ_ELIM "proof_eq_elim"

/* -------------------- 矢列演算 -------------------- */
/** 矢列推导 */
#define PRESET_SEQUENT_DERIVE "sequent_derive"
/** 切割消除 */
#define PRESET_SEQUENT_CUT_ELIMINATION "sequent_cut_elimination"
/** 主公式计算 */
#define PRESET_SEQUENT_MAIN_FORMULA "sequent_main_formula"
/** 侧公式计算 */
#define PRESET_SEQUENT_SIDE_FORMULA "sequent_side_formula"
/** 序列收敛判定 */
#define PRESET_SEQUENT_CONVERGENCE "sequent_convergence"

/* -------------------- 证明转换 -------------------- */
/** SKI组合子归约 */
#define PRESET_COMBINATOR_SKI "combinator_ski"
/** B/C/K/W组合子 */
#define PRESET_COMBINATOR_BCKW "combinator_bckw"
/** 证明规范化 */
#define PRESET_PROOF_NORMALIZATION "proof_normalization"
/** 证明等价比特 */
#define PRESET_PROOF_EQUIVALENCE "proof_equivalence"
/** proof_to_term 转换（Curry-Howard） */
#define PRESET_PROOF_TO_TERM "proof_to_term"
/** term_to_proof 转换（Curry-Howard） */
#define PRESET_TERM_TO_PROOF "term_to_proof"

/* -------------------- 证明分析 -------------------- */
/** 证明搜索（前向） */
#define PRESET_PROOF_SEARCH_FORWARD "proof_search_forward"
/** 证明搜索（后向） */
#define PRESET_PROOF_SEARCH_BACKWARD "proof_search_backward"
/** 证明深度计算 */
#define PRESET_PROOF_DEPTH "proof_depth"
/** 证明大小计算 */
#define PRESET_PROOF_SIZE "proof_size"
/** 证明正规形式 */
#define PRESET_PROOF_NORMAL_FORM "proof_normal_form"
/** 证明复杂度分析 */
#define PRESET_PROOF_COMPLEXITY "proof_complexity"

/* -------------------- 类型论基础 -------------------- */
/** lambda抽象 */
#define PRESET_TYPE_LAMBDA_ABSTRACT "type_lambda_abstract"
/** lambda应用 */
#define PRESET_TYPE_LAMBDA_APPLY "type_lambda_apply"
/** 类型推导（自然推导） */
#define PRESET_TYPE_INFERENCE "type_inference"
/** 类型检查 */
#define PRESET_TYPE_CHECK "type_check"
/** 类型等价判定 */
#define PRESET_TYPE_EQUIVALENCE "type_equivalence"
/** dependent_pair类型构造 */
#define PRESET_TYPE_DEPENDENT_PAIR "type_dependent_pair"
/** Pi类型构造 */
#define PRESET_TYPE_PI "type_pi"
/** Sigma类型构造 */
#define PRESET_TYPE_SIGMA "type_sigma"

 /** @} */

/* ============================================================
 * 预设数量常量
 * ============================================================ */

/** 证明论模块预设函数块总数 */
#define PROOF_THEORY_PRESET_COUNT 42

/* ============================================================
 * 模块接口函数声明
 * ============================================================ */

/**
 * @brief 注册证明论模块的所有预设函数块
 *
 * @return true 所有预设注册成功，false 部分失败
 */
bool preset_proof_theory_register(void);

/**
 * @brief 获取证明论预设函数块数量
 *
 * @return int 证明论模块预设函数块总数
 */
int preset_proof_theory_count(void);

/**
 * @brief 获取证明论模块的预设类别
 *
 * @return PresetCategory 证明论模块所属类别
 */
PresetCategory preset_proof_theory_category(void);

/**
 * @brief 获取证明论预设函数块名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 获取成功
 * @return false 参数无效或内存不足
 */
bool preset_proof_theory_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_PROOF_THEORY_H */
