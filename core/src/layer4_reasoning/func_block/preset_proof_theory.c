/**
 * @file preset_proof_theory.c
 * @brief 证明论预设函数块 - 实现
 *
 * @details 实现理论数学研究中证明论领域的预设函数块。
 *          所有预设函数块都遵循模块化、确定性原则，
 *          为自然推理、矢列演算、证明转换和类型论提供基础运算。
 *
 * 数学基础：
 * - 自然推理基于Gentzen的NK系统
 * - 矢列演算基于相继式演算
 * - 证明转换基于组合子逻辑
 * - 类型论基于Curry-Howard同构
 *
 * @module ProofTheory
 * @category PRESET_CATEGORY_MATH_LOGIC
 * @version 13.0.0
 */

#include "preset_proof_theory.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 证明论模块预设函数块总数 */
#define PROOF_THEORY_PRESET_COUNT 42

/* ==================== 模块注册实现 ==================== */

/**
 * @brief 证明论模块默认类别
 */
#define PROOF_THEORY_DEFAULT_CATEGORY PRESET_CATEGORY_MATH_LOGIC

/**
 * @brief 注册证明论模块的所有预设函数块
 *
 * 本函数实现证明论领域的42个核心预设：
 * - 自然推理规则（16个）：蕴含、合取、析取、否定、量词、等词
 * - 矢列演算（5个）：推导、切割消除、收敛判定
 * - 证明转换（6个）：SKI组合子、规范化、Curry-Howard
 * - 证明分析（6个）：证明搜索、复杂度、正规形式
 * - 类型论基础（9个）：lambda演算、类型推导、依赖类型
 *
 * @return true 所有预设注册成功
 */
bool preset_proof_theory_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：自然推理规则
     * ============================================================
     *
     * 自然推理是一种接近人类推理习惯的证明系统，
     * 通过引入规则和消除规则来构造证明。
     */

    /* -------------------- 蕴含引入 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_SEQUENCE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_IMPLIES_INTRO,
                                        "蕴含引入（条件证明）：从假设A推导出B，则得到 A→B",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{[A] \\vdash B}{\\vdash A \\to B}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 蕴含消除 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_IMPLIES_ELIM,
                                        "蕴含消除（分离规则MP）：从A和A→B推出B",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{\\vdash A \\quad \\vdash A \\to B}{\\vdash B}",
                                        "O(1)",
                                        true, false);
    }

    /* -------------------- 合取引入 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_CONJ_INTRO,
                                        "合取引入（∧I）：从A和B推出 A∧B",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{\\vdash A \\quad \\vdash B}{\\vdash A \\land B}",
                                        "O(1)",
                                        true, false);
    }

    /* -------------------- 合取消去 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_CONJ_ELIM,
                                        "合取消去（∧E）：从A∧B推出A或B",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{\\vdash A \\land B}{\\vdash A} \\quad \\frac{\\vdash A \\land B}{\\vdash B}",
                                        "O(1)",
                                        true, false);
    }

    /* -------------------- 析取引入 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_DISJ_INTRO,
                                        "析取引入（∨I）：从A推出A∨B或B∨A",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{\\vdash A}{\\vdash A \\lor B} \\quad \\frac{\\vdash B}{\\vdash A \\lor B}",
                                        "O(1)",
                                        true, false);
    }

    /* -------------------- 析取消去 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_SEQUENCE, PRESET_TYPE_SEQUENCE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_DISJ_ELIM,
                                        "析取消去（∨E）：从A∨B、A→C、B→C推出C",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 3,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{\\vdash A \\lor B \\quad [A] \\vdash C \\quad [B] \\vdash C}{\\vdash C}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 否定引入 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_NOT_INTRO,
                                        "否定引入（反证法）：从A假设推导出矛盾则得到¬A",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{[A] \\vdash \\bot}{\\vdash \\neg A}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 否定消除 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_NOT_ELIM,
                                        "否定消除（双反规则）：从¬A和A推出矛盾",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{\\vdash \\neg A \\quad \\vdash A}{\\vdash \\bot}",
                                        "O(1)",
                                        true, false);
    }

    /* -------------------- 双重否定消除 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_DNE,
                                        "双重否定消除：¬¬A推出A（直觉主义无效）",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{\\vdash \\neg\\neg A}{\\vdash A}",
                                        "O(1)",
                                        false, false);
    }

    /* -------------------- 排中律 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_LEM,
                                        "排中律：A∨¬A（直觉主义逻辑不接受）",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_FORMULA,
                                        "\\vdash A \\lor \\neg A",
                                        "O(1)",
                                        false, false);
    }

    /* -------------------- 全称量化引入 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_FORALL_INTRO,
                                        "全称量化引入：从对任意a推导φ(a)得到∀xφ(x)，a不在假设中自由出现",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{\\vdash \\phi(a)}{\\vdash \\forall x \\phi(x)} (a\\text{不在假设中自由出现})",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 全称量化消除 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_TERM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_FORALL_ELIM,
                                        "全称量化消除：从∀xφ(x)用项t替换得到φ(t)",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{\\vdash \\forall x \\phi(x)}{\\vdash \\phi(t)}",
                                        "O(1)",
                                        true, false);
    }

    /* -------------------- 存在量化引入 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_TERM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_EXISTS_INTRO,
                                        "存在量化引入：从φ(t)得到∃xφ(x)",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{\\vdash \\phi(t)}{\\vdash \\exists x \\phi(x)}",
                                        "O(1)",
                                        true, false);
    }

    /* -------------------- 存在量化消除 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_SEQUENCE, PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_EXISTS_ELIM,
                                        "存在量化消除：从∃xφ(x)和φ(a)→ψ得到ψ，a不在ψ中自由出现",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 3,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{\\vdash \\exists x \\phi(x) \\quad [\\phi(a)] \\vdash \\psi}{\\vdash \\psi} (a\\text{不在}\\psi\\text{中自由出现})",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 等词引入 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TERM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_EQ_INTRO,
                                        "等词引入：任意项t与自身相等 t=t",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_FORMULA,
                                        "\\vdash t = t",
                                        "O(1)",
                                        true, false);
    }

    /* -------------------- 等词消除 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_FORMULA, PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_EQ_ELIM,
                                        "等词消除（莱布尼茨规则）：从s=t和φ(s)得到φ(t)",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 3,
                                        PRESET_TYPE_FORMULA,
                                        "\\frac{\\vdash s = t \\quad \\vdash \\phi(s)}{\\vdash \\phi(t)}",
                                        "O(1)",
                                        true, false);
    }

    /* ============================================================
     * 第二部分：矢列演算
     * ============================================================
     *
     * 矢列演算是Gentzen提出的证明论工具，
     * 每个矢列Γ ⊢ Δ表示从Γ推出Δ中至少一个公式。
     */

    /* -------------------- 矢列推导 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_SEQUENT_DERIVE,
                                        "矢列推导：验证Γ ⊢ Δ的证明是否存在",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "\\Gamma \\vdash \\Delta \\text{ 可证}",
                                        "O(2^n)",
                                        false, false);
    }

    /* -------------------- 切割消除 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_PROOF};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_SEQUENT_CUT_ELIMINATION,
                                        "切割消除：移除证明中的切割规则，得到切割自由证明",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_PROOF,
                                        "\\text{给定证明} \\Rightarrow \\text{切割自由证明}",
                                        "O(n^2)",
                                        true, false);
    }

    /* -------------------- 主公式计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_SEQUENT_MAIN_FORMULA,
                                        "主公式：计算矢列中需要分解的主要公式",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_LIST,
                                        "\\text{主公式} = \\{ \\text{矢列中连接词的左/右公式} \\}",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 侧公式计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_SEQUENT_SIDE_FORMULA,
                                        "侧公式：计算矢列中作为上下文的公式",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_LIST,
                                        "\\Gamma \\backslash \\text{主公式}",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 序列收敛判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_SEQUENCE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_SEQUENT_CONVERGENCE,
                                        "序列收敛：检查证明树的收敛性",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "\\text{序列}\\Gamma_0 \\vdash \\Gamma_n\\text{收敛}",
                                        "O(n)",
                                        false, false);
    }

    /* ============================================================
     * 第三部分：证明转换
     * ============================================================
     *
     * 证明转换研究不同证明系统之间的等价性，
     * 基于组合子逻辑和Curry-Howard同构。
     */

    /* -------------------- SKI组合子归约 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TERM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_COMBINATOR_SKI,
                                        "SKI组合子归约：将任意lambda项转换为SKI组合子形式并归约",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_TERM,
                                        "Sxyz = xz(yz), \\quad Kxy = x, \\quad Ix = x",
                                        "O(n)",
                                        true, true);
    }

    /* -------------------- B/C/K/W组合子 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TERM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_COMBINATOR_BCKW,
                                        "BCKW组合子：将lambda项转换为BCKW基础组合子",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_TERM,
                                        "Bxyz = x(yz), \\quad Cxyz = xzy, \\quad Kxy = x, \\quad Wxy = xyy",
                                        "O(n)",
                                        true, true);
    }

    /* -------------------- 证明规范化 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_PROOF};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_NORMALIZATION,
                                        "证明规范化：将证明归约为正规形式",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_PROOF,
                                        "\\text{给定证明} \\Rightarrow \\beta\\text{-正规形式}",
                                        "O(n^2)",
                                        true, false);
    }

    /* -------------------- 证明等价比特 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_PROOF, PRESET_TYPE_PROOF};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_EQUIVALENCE,
                                        "证明等价：判断两个证明是否等价（规范化后相同）",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "\\pi_1 \\sim \\pi_2 \\Leftrightarrow N(\\pi_1) = N(\\pi_2)",
                                        "O(n^2)",
                                        false, false);
    }

    /* -------------------- proof_to_term 转换 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_PROOF};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_TO_TERM,
                                        "Proof到Term：使用Curry-Howard同构将证明转换为lambda项",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_TERM,
                                        "\\pi : A \\vdash B \\Rightarrow t : A \\to B",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- term_to_proof 转换 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TERM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_TERM_TO_PROOF,
                                        "Term到Proof：使用Curry-Howard同构将lambda项转换为证明",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_PROOF,
                                        "t : A \\to B \\Rightarrow \\pi : A \\vdash B",
                                        "O(n)",
                                        true, false);
    }

    /* ============================================================
     * 第四部分：证明分析
     * ============================================================
     *
     * 证明分析提供证明的结构信息和复杂度度量。
     */

    /* -------------------- 证明搜索（前向） -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_SEARCH_FORWARD,
                                        "前向证明搜索：从已知事实向目标推进",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_PROOF,
                                        "\\Gamma \\Rightarrow \\Delta \\text{ 前向搜索}",
                                        "O(b^d)",
                                        true, false);
    }

    /* -------------------- 证明搜索（后向） -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_SEARCH_BACKWARD,
                                        "后向证明搜索：从目标向已知事实回溯",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_PROOF,
                                        "\\Gamma \\Rightarrow \\Delta \\text{ 后向搜索}",
                                        "O(b^d)",
                                        true, false);
    }

    /* -------------------- 证明深度计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_PROOF};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_DEPTH,
                                        "证明深度：计算证明树的最大深度",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_INTEGER,
                                        "\\text{depth}(\\pi) = \\max\\{ \\text{路径长度} \\}",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 证明大小计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_PROOF};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_SIZE,
                                        "证明大小：计算证明中公式的总数",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_INTEGER,
                                        "|\\pi| = \\text{证明中的公式数}",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 证明正规形式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_PROOF};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_NORMAL_FORM,
                                        "证明正规形式：计算证明的正规形式表示",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_PROOF,
                                        "\\text{NF}(\\pi) = \\text{去除冗余规则后的证明}",
                                        "O(n^2)",
                                        true, false);
    }

    /* -------------------- 证明复杂度分析 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_PROOF};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_PROOF_COMPLEXITY,
                                        "证明复杂度：分析证明的计算复杂度",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_STRING,
                                        "\\text{复杂度} \\in \\{P, NP, PSPACE, \\ldots\\}",
                                        "O(n)",
                                        false, false);
    }

    /* ============================================================
     * 第五部分：类型论基础
     * ============================================================
     *
     * 类型论基于Curry-Howard同构，将证明与程序对应起来。
     */

    /* -------------------- lambda抽象 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TERM, PRESET_TYPE_TERM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_TYPE_LAMBDA_ABSTRACT,
                                        "lambda抽象：创建lambda项 λx.M",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_TERM,
                                        "\\lambda x . M",
                                        "O(1)",
                                        true, false);
    }

    /* -------------------- lambda应用 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TERM, PRESET_TYPE_TERM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_TYPE_LAMBDA_APPLY,
                                        "lambda应用：创建应用项 (M N)",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_TERM,
                                        "M \\ N \\equiv (M N)",
                                        "O(1)",
                                        true, true);
    }

    /* -------------------- 类型推导 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TERM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_TYPE_INFERENCE,
                                        "类型推导：使用自然推导为lambda项推导类型",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_TYPE,
                                        "\\Gamma \\vdash M : A",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 类型检查 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TERM, PRESET_TYPE_TYPE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_TYPE_CHECK,
                                        "类型检查：验证lambda项是否具有给定类型",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "\\Gamma \\vdash M : A ?",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 类型等价判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TYPE, PRESET_TYPE_TYPE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_TYPE_EQUIVALENCE,
                                        "类型等价：判断两个类型是否等价（可规范化相同）",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "A \\equiv B \\Leftrightarrow A \\Downarrow = B \\Downarrow",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- dependent_pair类型构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TERM, PRESET_TYPE_TERM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_TYPE_DEPENDENT_PAIR,
                                        "依赖对类型：构造 Σx:A.B 类型",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_TYPE,
                                        "\\Sigma x : A . B",
                                        "O(1)",
                                        true, false);
    }

    /* -------------------- Pi类型构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TERM, PRESET_TYPE_TERM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_TYPE_PI,
                                        "Pi类型：构造 Πx:A.B 类型（依赖函数类型）",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_TYPE,
                                        "\\Pi x : A . B",
                                        "O(1)",
                                        true, false);
    }

    /* -------------------- Sigma类型构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TERM, PRESET_TYPE_TERM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_TYPE_SIGMA,
                                        "Sigma类型：构造 Σx:A.B 类型（依赖和类型）",
                                        PROOF_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_TYPE,
                                        "\\Sigma x : A . B",
                                        "O(1)",
                                        true, false);
    }

    /* 证明论模块预设注册完成 */

    (void) PROOF_THEORY_PRESET_COUNT;

    return success_count == PROOF_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取证明论预设函数块数量
 */
int preset_proof_theory_count(void) {
    return PROOF_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取证明论模块的预设类别
 */
PresetCategory preset_proof_theory_category(void) {
    return PRESET_CATEGORY_MATH_LOGIC;
}

/**
 * @brief 获取证明论预设函数块名称列表
 */
bool preset_proof_theory_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv00_malloc(PROOF_THEORY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        /* 自然推理规则 */
        PRESET_PROOF_IMPLIES_INTRO,
        PRESET_PROOF_IMPLIES_ELIM,
        PRESET_PROOF_CONJ_INTRO,
        PRESET_PROOF_CONJ_ELIM,
        PRESET_PROOF_DISJ_INTRO,
        PRESET_PROOF_DISJ_ELIM,
        PRESET_PROOF_NOT_INTRO,
        PRESET_PROOF_NOT_ELIM,
        PRESET_PROOF_DNE,
        PRESET_PROOF_LEM,
        PRESET_PROOF_FORALL_INTRO,
        PRESET_PROOF_FORALL_ELIM,
        PRESET_PROOF_EXISTS_INTRO,
        PRESET_PROOF_EXISTS_ELIM,
        PRESET_PROOF_EQ_INTRO,
        PRESET_PROOF_EQ_ELIM,
        /* 矢列演算 */
        PRESET_SEQUENT_DERIVE,
        PRESET_SEQUENT_CUT_ELIMINATION,
        PRESET_SEQUENT_MAIN_FORMULA,
        PRESET_SEQUENT_SIDE_FORMULA,
        PRESET_SEQUENT_CONVERGENCE,
        /* 证明转换 */
        PRESET_COMBINATOR_SKI,
        PRESET_COMBINATOR_BCKW,
        PRESET_PROOF_NORMALIZATION,
        PRESET_PROOF_EQUIVALENCE,
        PRESET_PROOF_TO_TERM,
        PRESET_TERM_TO_PROOF,
        /* 证明分析 */
        PRESET_PROOF_SEARCH_FORWARD,
        PRESET_PROOF_SEARCH_BACKWARD,
        PRESET_PROOF_DEPTH,
        PRESET_PROOF_SIZE,
        PRESET_PROOF_NORMAL_FORM,
        PRESET_PROOF_COMPLEXITY,
        /* 类型论基础 */
        PRESET_TYPE_LAMBDA_ABSTRACT,
        PRESET_TYPE_LAMBDA_APPLY,
        PRESET_TYPE_INFERENCE,
        PRESET_TYPE_CHECK,
        PRESET_TYPE_EQUIVALENCE,
        PRESET_TYPE_DEPENDENT_PAIR,
        PRESET_TYPE_PI,
        PRESET_TYPE_SIGMA,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) {
                lv00_free((void **) &names[j]);
            }
            lv00_free((void **) &names);
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
