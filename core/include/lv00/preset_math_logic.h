/**
 * @file preset_math_logic.h
 * @brief 数理逻辑预设函数块 - 常量定义
 *
 * 提供理论数学研究中常用的数理逻辑运算预设函数块。
 * 涵盖命题逻辑、一阶逻辑、证明系统、模型论及递归论。
 *
 * 包含的预设函数块：
 * - 命题逻辑 (7个)：合取、析取、否定、蕴含、等价、永真式判定、可满足性判定
 * - 一阶逻辑 (4个)：全称实例化、存在泛化、全称泛化、存在实例化
 * - 证明系统 (3个)：自然演绎、归结原理、表方法
 * - 模型论 (3个)：模型判定、有效式判定、模型论可满足性
 * - 递归论 (3个)：图灵机判定、递归函数判定、停机问题
 *
 * @module MathLogic
 * @category PRESET_CATEGORY_LOGIC
 * @version 3.3.0
 */

#ifndef LV00_PRESET_MATH_LOGIC_H
#define LV00_PRESET_MATH_LOGIC_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 预设名称宏常量
 * ================================================================ */

/* ==================== 命题逻辑 ==================== */

/**
 * @brief 预设：命题合取
 * @details 数学定义: $P \land Q$，当且仅当 $P$ 和 $Q$ 均为真时结果为真
 * @note 输入: PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(1)
 */
#define PRESET_LOGIC_CONJUNCTION "logic_conjunction"

/**
 * @brief 预设：命题析取
 * @details 数学定义: $P \lor Q$，当且仅当 $P$ 或 $Q$ 至少一个为真时结果为真
 * @note 输入: PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(1)
 */
#define PRESET_LOGIC_DISJUNCTION "logic_disjunction"

/**
 * @brief 预设：命题否定
 * @details 数学定义: $\lnot P$，当 $P$ 为真时结果为假，反之亦然
 * @note 输入: PRESET_TYPE_BOOLEAN | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(1) | 可逆: 是
 */
#define PRESET_LOGIC_NEGATION "logic_negation"

/**
 * @brief 预设：命题蕴含
 * @details 数学定义: $P \to Q \equiv \lnot P \lor Q$
 * @note 输入: PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(1)
 */
#define PRESET_LOGIC_IMPLICATION "logic_implication"

/**
 * @brief 预设：命题等价
 * @details 数学定义: $P \leftrightarrow Q \equiv (P \to Q) \land (Q \to P)$
 * @note 输入: PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(1) | 可逆: 是
 */
#define PRESET_LOGIC_BICONDITIONAL "logic_biconditional"

/**
 * @brief 预设：永真式判定
 * @details 数学定义: $\varphi$ 是永真式 $\Leftrightarrow \forall v: v(\varphi) = \text{T}$
 * @note 输入: PRESET_TYPE_FORMULA | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(2^n)，n 为命题变元数
 */
#define PRESET_LOGIC_TAUTOLOGY_TEST "logic_tautology_test"

/**
 * @brief 预设：可满足性判定
 * @details 数学定义: $\varphi$ 可满足 $\Leftrightarrow \exists v: v(\varphi) = \text{T}$，即 SAT 问题
 * @note 输入: PRESET_TYPE_FORMULA | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(2^n)，n 为命题变元数
 */
#define PRESET_LOGIC_SATISFIABILITY_TEST "logic_satisfiability_test"

/* ==================== 一阶逻辑 ==================== */

/**
 * @brief 预设：全称实例化
 * @details 数学定义: $\forall x\ P(x) \vdash P(c)$，$c$ 为论域中任意元素
 * @note 输入: PRESET_TYPE_FORMULA, PRESET_TYPE_ANY | 输出: PRESET_TYPE_FORMULA | 复杂度: O(1)
 */
#define PRESET_LOGIC_UNIVERSAL_INSTANTIATION "logic_universal_instantiation"

/**
 * @brief 预设：存在泛化
 * @details 数学定义: $P(c) \vdash \exists x\ P(x)$，$c$ 为论域中某个元素
 * @note 输入: PRESET_TYPE_FORMULA, PRESET_TYPE_ANY | 输出: PRESET_TYPE_FORMULA | 复杂度: O(1)
 */
#define PRESET_LOGIC_EXISTENTIAL_GENERALIZATION "logic_existential_generalization"

/**
 * @brief 预设：全称泛化
 * @details 数学定义: $P(c) \vdash \forall x\ P(x)$，要求 $c$ 为任意常量且未被特殊假定
 * @note 输入: PRESET_TYPE_FORMULA, PRESET_TYPE_ANY | 输出: PRESET_TYPE_FORMULA | 复杂度: O(1)
 */
#define PRESET_LOGIC_UNIVERSAL_GENERALIZATION "logic_universal_generalization"

/**
 * @brief 预设：存在实例化
 * @details 数学定义: $\exists x\ P(x) \vdash P(c)$，$c$ 为此前未出现的新常量（Skolem 化）
 * @note 输入: PRESET_TYPE_FORMULA, PRESET_TYPE_ANY | 输出: PRESET_TYPE_FORMULA | 复杂度: O(1)
 */
#define PRESET_LOGIC_EXISTENTIAL_INSTANTIATION "logic_existential_instantiation"

/* ==================== 证明系统 ==================== */

/**
 * @brief 预设：自然演绎
 * @details 数学定义: $\Gamma \vdash_{\text{ND}} \varphi$，基于引入规则（$\land I, \lor I, \to I, \forall I, \exists I$）和消去规则
 * @note 输入: PRESET_TYPE_FORMULA, PRESET_TYPE_SEQUENCE | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(2^{|\Gamma|})
 */
#define PRESET_LOGIC_NATURAL_DEDUCTION "logic_natural_deduction"

/**
 * @brief 预设：归结原理
 * @details 数学定义: $S \vdash_{\text{Res}} \Box \Leftrightarrow S$ 不可满足，将公式集转化为子句集通过归结消解推导空子句
 * @note 输入: PRESET_TYPE_SEQUENCE | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(n^k)，n 为子句数
 */
#define PRESET_LOGIC_RESOLUTION "logic_resolution"

/**
 * @brief 预设：语义表方法
 * @details 数学定义: $\lnot \varphi$ 的表封闭 $\Leftrightarrow \varphi$ 有效，通过系统分解公式构建反证树判定有效性
 * @note 输入: PRESET_TYPE_FORMULA | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(2^n)，n 为子公式数
 */
#define PRESET_LOGIC_TABLEAU_METHOD "logic_tableau_method"

/* ==================== 模型论 ==================== */

/**
 * @brief 预设：模型判定
 * @details 数学定义: $M \models \Gamma \Leftrightarrow \forall \varphi \in \Gamma, M \models \varphi$
 * @note 输入: PRESET_TYPE_STRUCTURE, PRESET_TYPE_SEQUENCE | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|\Gamma| \cdot |M|)
 */
#define PRESET_LOGIC_MODEL_CHECK "logic_model_check"

/**
 * @brief 预设：有效式判定
 * @details 数学定义: $\models \varphi \Leftrightarrow \forall M, M \models \varphi$，一阶逻辑有效性不可判定（半可判定，Church-Turing 定理）
 * @note 输入: PRESET_TYPE_FORMULA | 输出: PRESET_TYPE_BOOLEAN | 复杂度: 不可判定（半可判定） | 构造性: 否
 */
#define PRESET_LOGIC_VALIDITY_TEST "logic_validity_test"

/**
 * @brief 预设：模型论可满足性
 * @details 数学定义: $\Gamma$ 可满足 $\Leftrightarrow \exists M: M \models \Gamma$（Godel 完备性定理: $\Gamma \vdash \varphi \Leftrightarrow \Gamma \models \varphi$）
 * @note 输入: PRESET_TYPE_SEQUENCE | 输出: PRESET_TYPE_BOOLEAN | 复杂度: 不可判定（半可判定） | 构造性: 否
 */
#define PRESET_LOGIC_MODEL_SATISFIABILITY "logic_model_satisfiability"

/* ==================== 递归论 ==================== */

/**
 * @brief 预设：图灵机合法性判定
 * @details 数学定义: $T = (Q, \Sigma, \Gamma, \delta, q_0, q_{\text{acc}}, q_{\text{rej}})$ 合法性检验（状态集、字母表、转移函数完备性）
 * @note 输入: PRESET_TYPE_STRING | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|Q| \cdot |\Gamma|)
 */
#define PRESET_LOGIC_TURING_MACHINE_CHECK "logic_turing_machine_check"

/**
 * @brief 预设：递归函数判定
 * @details 数学定义: $f$ 是递归函数 $\Leftrightarrow \exists T_M: T_M(x) = f(x)$，Church-Turing 论题: 直观可计算 = 图灵可计算
 * @note 输入: PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_BOOLEAN | 复杂度: 不可判定 | 构造性: 否
 */
#define PRESET_LOGIC_RECURSIVE_CHECK "logic_recursive_check"

/**
 * @brief 预设：停机问题
 * @details 数学定义: $H(M, w) = \begin{cases} 1 & M \text{ 在 } w \text{ 上停机} \\ 0 & \text{否则} \end{cases}$，经典不可判定问题（Turing, 1936）
 * @note 输入: PRESET_TYPE_STRING, PRESET_TYPE_STRING | 输出: PRESET_TYPE_BOOLEAN | 复杂度: 不可判定 | 构造性: 否
 */
#define PRESET_LOGIC_HALTING_PROBLEM "logic_halt_problem"

/* ================================================================
 * 模块注册函数
 * ================================================================ */

/**
 * @brief 注册所有数理逻辑预设函数块
 *
 * 将数理逻辑模块的所有预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_math_logic_register(void);

/**
 * @brief 获取数理逻辑预设函数块数量
 *
 * @return int 数理逻辑模块预设函数块总数（20）
 */
int preset_math_logic_count(void);

/**
 * @brief 获取数理逻辑预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_math_logic_category(void);

/**
 * @brief 获取数理逻辑预设的名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_math_logic_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_MATH_LOGIC_H */
