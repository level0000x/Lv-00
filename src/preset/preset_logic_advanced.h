/**
 * @file preset_logic_advanced.h
 * @brief 高级逻辑预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的高级逻辑运算预设函数块，包括：
 *   - 经典推理规则：假言推理、逆否推理、假言三段论、析取三段论
 *   - 联结词规则：合取引入/消除、析取引入/消除、否定引入
 *   - 双重否定：双重否定消除
 *   - 量词规则：全称引入/消除、存在引入/消除
 *   - 证明方法：归谬法、反证法
 *   - 自动推理：消解原理、合一算法
 *   - 范式转换：斯柯伦化、合取范式、析取范式
 *
 * @module LogicAdvanced
 * @category PRESET_CATEGORY_LOGIC
 * @version 3.2.0
 * @author Lv-00 开发团队
 */

#ifndef PRESET_LOGIC_ADVANCED_H
#define PRESET_LOGIC_ADVANCED_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 经典推理规则 -------------------- */

/** 假言推理：(P -> Q, P) |- Q */
#define PRESET_LOGIC_MODUS_PONENS            "logic_modus_ponens"

/** 逆否推理：(P -> Q, ~Q) |- ~P */
#define PRESET_LOGIC_MODUS_TOLLENS           "logic_modus_tollens"

/** 假言三段论：(P -> Q, Q -> R) |- P -> R */
#define PRESET_LOGIC_HYPOTHETICAL_SYLLOGISM  "logic_hypothetical_syllogism"

/** 析取三段论：(P \/ Q, ~P) |- Q */
#define PRESET_LOGIC_DISJUNCTIVE_SYLLOGISM   "logic_disjunctive_syllogism"

/* -------------------- 联结词规则 -------------------- */

/** 合取引入：P, Q |- P /\ Q */
#define PRESET_LOGIC_CONJUNCTION_INTRO       "logic_conjunction_intro"

/** 合取消除：P /\ Q |- P（或 Q） */
#define PRESET_LOGIC_CONJUNCTION_ELIM        "logic_conjunction_elim"

/** 析取引入：P |- P \/ Q */
#define PRESET_LOGIC_DISJUNCTION_INTRO       "logic_disjunction_intro"

/** 析取消除：(P \/ Q, P -> R, Q -> R) |- R */
#define PRESET_LOGIC_DISJUNCTION_ELIM        "logic_disjunction_elim"

/** 否定引入（反证法）：若假设 P 推出矛盾，则推出 ~P */
#define PRESET_LOGIC_NEGATION_INTRO          "logic_negation_intro"

/** 双重否定消除：~~P |- P */
#define PRESET_LOGIC_DOUBLE_NEGATION_ELIM    "logic_double_negation_elim"

/* -------------------- 量词规则 -------------------- */

/** 全称引入：若能证明任意 c 满足 P(c)，则推出 forall x P(x) */
#define PRESET_LOGIC_UNIVERSAL_INTRO         "logic_universal_intro"

/** 全称消除：forall x P(x) |- P(t)（对任意项 t） */
#define PRESET_LOGIC_UNIVERSAL_ELIM          "logic_universal_elim"

/** 存在引入：P(t) |- exists x P(x) */
#define PRESET_LOGIC_EXISTENTIAL_INTRO       "logic_existential_intro"

/** 存在消除：exists x P(x), (forall x, P(x) -> C) |- C */
#define PRESET_LOGIC_EXISTENTIAL_ELIM        "logic_existential_elim"

/* -------------------- 证明方法 -------------------- */

/** 归谬法：若假设 ~P 推出矛盾，则推出 P */
#define PRESET_LOGIC_PROOF_BY_CONTRADICTION  "logic_proof_by_contradiction"

/* -------------------- 自动推理 -------------------- */

/** 消解原理：从子句集 {P \/ Q, ~P \/ R} 推出 Q \/ R */
#define PRESET_LOGIC_RESOLUTION              "logic_resolution"

/** 合一算法：求解两个原子公式的最一般合一（MGU） */
#define PRESET_LOGIC_UNIFICATION             "logic_unification"

/* -------------------- 范式转换 -------------------- */

/** 斯柯伦化：消除前束范式中的存在量词 */
#define PRESET_LOGIC_SKOLEMIZE               "logic_skolemize"

/** 合取范式转换：将公式转换为 CNF */
#define PRESET_LOGIC_CNF_CONVERT             "logic_cnf_convert"

/** 析取范式转换：将公式转换为 DNF */
#define PRESET_LOGIC_DNF_CONVERT             "logic_dnf_convert"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有高级逻辑预设函数块
 *
 * 将高级逻辑模块的所有预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_logic_advanced_register(void);

/**
 * @brief 获取高级逻辑预设函数块数量
 *
 * @return int 高级逻辑模块预设函数块总数
 */
int preset_logic_advanced_count(void);

#ifdef __cplusplus
}
#endif

#endif /* PRESET_LOGIC_ADVANCED_H */
