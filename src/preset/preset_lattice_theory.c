/**
 * @file preset_lattice_theory.c
 * @brief 格论预设函数块模块 - 实现（v2统一宏模式）
 *
 * 实现理论数学研究中常用的格论运算预设函数块。
 * 涵盖格基础运算、特殊格、格同态与表示、格与序。
 * 共30个预设函数块，均遵循模块化、确定性原则。
 *
 * @module LatticeTheory
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 5.0.0
 */

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_lattice_theory.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include <string.h>
#include <stdlib.h>

/* ==================== 预设函数块数量 ==================== */

/** 格论模块预设函数块总数 */
#define LATTICE_THEORY_PRESET_COUNT 30

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个格论预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有格论预设都属于 PRESET_CATEGORY_ALGEBRAIC 类别。
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义（LaTeX格式）
 * @param complexity 时间复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_lattice_theory_preset(
    const char *name, const char *description,
    const PresetType *input_types, int input_count, PresetType output_type,
    const char *math_def, const char *complexity,
    bool is_constructive, bool is_reversible)
{
    return preset_blocks_register_simple(
        name, description, PRESET_CATEGORY_ALGEBRAIC,
        input_types, input_count, output_type,
        math_def, complexity, is_constructive, is_reversible);
}

/* ==================== v2统一注册宏 ==================== */

/**
 * @brief 格论预设统一注册宏
 *
 * 使用do-while(0)包装，确保宏展开后在语法上等价于单条语句。
 * 注册成功时递增success_count，失败时输出错误日志。
 *
 * @param name       预设名称
 * @param desc       中文描述
 * @param inputs     输入类型数组
 * @param in_count   输入数量
 * @param output     输出类型
 * @param math       数学定义（LaTeX格式字符串）
 * @param comp       时间复杂度
 * @param cons       是否构造性
 * @param rev        是否可逆
 */
#define REGISTER_LATTICE(name, desc, inputs, in_count, output, math, comp, cons, rev) \
    do { \
        if (register_lattice_theory_preset( \
                (name), (desc), (inputs), (in_count), (output), \
                (math), (comp), (cons), (rev))) { \
            success_count++; \
        } else { \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */ \
        } \
    } while (0)

/* ==================== 模块注册实现 ==================== */

bool preset_lattice_theory_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：格基础运算（10个）
     * ============================================================ */

    /**
     * @brief lattice_join - 上确界（并）
     *
     * 计算格中两个元素的上确界（最小上界）a ∨ b。
     * 上确界满足：a ∨ b >= a, a ∨ b >= b，且若 c >= a 且 c >= b 则 c >= a ∨ b。
     * 上确界运算满足交换律、结合律、幂等律和吸收律。
     *
     * @param a 格元素（PRESET_TYPE_ALGEBRA）
     * @param b 格元素（PRESET_TYPE_ALGEBRA）
     * @return 上确界 a ∨ b（PRESET_TYPE_ALGEBRA）
     * @math a \\vee b = \\min\\{c \\in L : c \\geq a \\land c \\geq b\\}
     * @complexity O(1)
     * @constructive true
     * @reversible false（上确界运算一般不可逆）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_join",
            "上确界（并）：计算格中两个元素的最小上界 a ∨ b",
            inputs, 2, PRESET_TYPE_ALGEBRA,
            "a \\vee b = \\min\\{c \\in L : c \\geq a \\land c \\geq b\\}",
            "O(1)", true, false);
    }

    /**
     * @brief lattice_meet - 下确界（交）
     *
     * 计算格中两个元素的下确界（最大下界）a ∧ b。
     * 下确界满足：a ∧ b <= a, a ∧ b <= b，且若 c <= a 且 c <= b 则 c <= a ∧ b。
     * 下确界运算满足交换律、结合律、幂等律和吸收律。
     *
     * @param a 格元素（PRESET_TYPE_ALGEBRA）
     * @param b 格元素（PRESET_TYPE_ALGEBRA）
     * @return 下确界 a ∧ b（PRESET_TYPE_ALGEBRA）
     * @math a \\wedge b = \\max\\{c \\in L : c \\leq a \\land c \\leq b\\}
     * @complexity O(1)
     * @constructive true
     * @reversible false（下确界运算一般不可逆）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_meet",
            "下确界（交）：计算格中两个元素的最大下界 a ∧ b",
            inputs, 2, PRESET_TYPE_ALGEBRA,
            "a \\wedge b = \\max\\{c \\in L : c \\leq a \\land c \\leq b\\}",
            "O(1)", true, false);
    }

    /**
     * @brief lattice_top - 最大元（顶）
     *
     * 获取有界格L的最大元（顶元素）⊤。
     * 满足 a ≤ ⊤ 对所有 a in L。
     * 最大元若存在则唯一。
     *
     * @param L 格（PRESET_TYPE_ALGEBRA）
     * @return 最大元 ⊤（PRESET_TYPE_ALGEBRA）
     * @math \\top \\in L, \\quad \\forall a \\in L: a \\leq \\top
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_top",
            "最大元（顶）：获取有界格L的最大元 ⊤",
            inputs, 1, PRESET_TYPE_ALGEBRA,
            "\\top \\in L, \\quad \\forall a \\in L: a \\leq \\top",
            "O(1)", true, false);
    }

    /**
     * @brief lattice_bottom - 最小元（底）
     *
     * 获取有界格L的最小元（底元素）⊥。
     * 满足 ⊥ ≤ a 对所有 a in L。
     * 最小元若存在则唯一。
     *
     * @param L 格（PRESET_TYPE_ALGEBRA）
     * @return 最小元 ⊥（PRESET_TYPE_ALGEBRA）
     * @math \\bot \\in L, \\quad \\forall a \\in L: \\bot \\leq a
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_bottom",
            "最小元（底）：获取有界格L的最小元 ⊥",
            inputs, 1, PRESET_TYPE_ALGEBRA,
            "\\bot \\in L, \\quad \\forall a \\in L: \\bot \\leq a",
            "O(1)", true, false);
    }

    /**
     * @brief lattice_complement - 补元
     *
     * 计算有界格中元素a的补元a'。
     * 补元满足 a ∨ a' = ⊤ 且 a ∧ a' = ⊥。
     * 补元不一定存在，若存在也不一定唯一（除非在布尔代数中）。
     *
     * @param a 格元素（PRESET_TYPE_ALGEBRA）
     * @param L 格（PRESET_TYPE_ALGEBRA）
     * @return 补元 a'（PRESET_TYPE_ALGEBRA）
     * @math a' \\in L, \\quad a \\vee a' = \\top \\land a \\wedge a' = \\bot
     * @complexity O(n)
     * @constructive true
     * @reversible true（补元的补元是自身：(a')' = a）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_complement",
            "补元：计算有界格中元素a的补元a'，满足 a ∨ a' = ⊤ 且 a ∧ a' = ⊥",
            inputs, 2, PRESET_TYPE_ALGEBRA,
            "a' \\in L, \\quad a \\vee a' = \\top \\land a \\wedge a' = \\bot",
            "O(n)", true, true);
    }

    /**
     * @brief lattice_partial_order - 偏序关系判定
     *
     * 判定集合L上的关系≤是否构成偏序关系。
     * 偏序关系需满足：自反性(a ≤ a)、反对称性(a ≤ b 且 b ≤ a 则 a = b)、
     * 传递性(a ≤ b 且 b ≤ c 则 a ≤ c)。
     *
     * @param L 集合（PRESET_TYPE_SET）
     * @param R 关系（PRESET_TYPE_FUNCTION）
     * @return 是否为偏序关系（PRESET_TYPE_BOOLEAN）
     * @math (L, \\leq) \\text{ 是偏序集} \\Leftrightarrow \\leq \\text{ 满足自反性、反对称性、传递性}
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_FUNCTION};
        REGISTER_LATTICE("lattice_partial_order",
            "偏序关系判定：判定集合L上的关系≤是否构成偏序（自反、反对称、传递）",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "(L, \\leq) \\text{ 是偏序集} \\Leftrightarrow \\leq \\text{ 满足自反性、反对称性、传递性}",
            "O(n^2)", false, false);
    }

    /**
     * @brief lattice_check - 格结构判定
     *
     * 判定偏序集(L, ≤)是否构成格。
     * 格的条件：任意两个元素的上确界和下确界都存在。
     * 等价条件：任意两个元素都有最小上界和最大下界。
     *
     * @param L 偏序集（PRESET_TYPE_SET）
     * @param R 偏序关系（PRESET_TYPE_FUNCTION）
     * @return 是否为格（PRESET_TYPE_BOOLEAN）
     * @math (L, \\leq) \\text{ 是格} \\Leftrightarrow \\forall a, b \\in L: a \\vee b \\in L \\land a \\wedge b \\in L
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_FUNCTION};
        REGISTER_LATTICE("lattice_check",
            "格结构判定：判定偏序集(L, ≤)是否构成格（任意两元素的上确界和下确界存在）",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "(L, \\leq) \\text{ 是格} \\Leftrightarrow \\forall a, b \\in L: a \\vee b \\in L \\land a \\wedge b \\in L",
            "O(n^2)", false, false);
    }

    /**
     * @brief lattice_bounded_check - 有界格判定
     *
     * 判定格L是否为有界格。
     * 有界格的条件：存在最大元⊤和最小元⊥。
     * 等价于格中存在全局上界和全局下界。
     *
     * @param L 格（PRESET_TYPE_ALGEBRA）
     * @return 是否为有界格（PRESET_TYPE_BOOLEAN）
     * @math L \\text{ 是有界格} \\Leftrightarrow \\exists \\top, \\bot \\in L: \\bot \\leq a \\leq \\top \\; \\forall a \\in L
     * @complexity O(n)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_bounded_check",
            "有界格判定：判定格L是否为有界格（存在最大元⊤和最小元⊥）",
            inputs, 1, PRESET_TYPE_BOOLEAN,
            "L \\text{ 是有界格} \\Leftrightarrow \\exists \\top, \\bot \\in L: \\bot \\leq a \\leq \\top \\; \\forall a \\in L",
            "O(n)", false, false);
    }

    /**
     * @brief lattice_distributive_check - 分配格判定
     *
     * 判定格L是否为分配格。
     * 分配格的条件：交运算对并运算满足分配律（或等价地，并对交满足分配律）。
     * a ∧ (b ∨ c) = (a ∧ b) ∨ (a ∧ c) 对所有 a, b, c in L。
     *
     * @param L 格（PRESET_TYPE_ALGEBRA）
     * @return 是否为分配格（PRESET_TYPE_BOOLEAN）
     * @math L \\text{ 是分配格} \\Leftrightarrow \\forall a, b, c \\in L: a \\wedge (b \\vee c) = (a \\wedge b) \\vee (a \\wedge c)
     * @complexity O(n^3)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_distributive_check",
            "分配格判定：判定格L是否为分配格（交对并满足分配律）",
            inputs, 1, PRESET_TYPE_BOOLEAN,
            "L \\text{ 是分配格} \\Leftrightarrow \\forall a, b, c \\in L: a \\wedge (b \\vee c) = (a \\wedge b) \\vee (a \\wedge c)",
            "O(n^3)", false, false);
    }

    /**
     * @brief lattice_modular_check - 模格判定
     *
     * 判定格L是否为模格（Dedekind格）。
     * 模格的条件：若 a ≤ c，则 a ∨ (b ∧ c) = (a ∨ b) ∧ c。
     * 每个分配格都是模格，但反之不成立。
     *
     * @param L 格（PRESET_TYPE_ALGEBRA）
     * @return 是否为模格（PRESET_TYPE_BOOLEAN）
     * @math L \\text{ 是模格} \\Leftrightarrow \\forall a, b, c \\in L: a \\leq c \\Rightarrow a \\vee (b \\wedge c) = (a \\vee b) \\wedge c
     * @complexity O(n^3)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_modular_check",
            "模格判定：判定格L是否为模格（Dedekind格），若 a ≤ c 则 a ∨ (b ∧ c) = (a ∨ b) ∧ c",
            inputs, 1, PRESET_TYPE_BOOLEAN,
            "L \\text{ 是模格} \\Leftrightarrow \\forall a, b, c \\in L: a \\leq c \\Rightarrow a \\vee (b \\wedge c) = (a \\vee b) \\wedge c",
            "O(n^3)", false, false);
    }

    /* ============================================================
     * 第二部分：特殊格（8个）
     * ============================================================ */

    /**
     * @brief boolean_algebra_check - 布尔代数判定
     *
     * 判定有界分配格L是否为布尔代数。
     * 布尔代数的条件：有界分配格中每个元素都有补元。
     * 等价于：有界分配格中每个元素a都存在a'使得 a ∨ a' = ⊤ 且 a ∧ a' = ⊥。
     *
     * @param L 有界分配格（PRESET_TYPE_ALGEBRA）
     * @return 是否为布尔代数（PRESET_TYPE_BOOLEAN）
     * @math L \\text{ 是布尔代数} \\Leftrightarrow L \\text{ 是有界分配格} \\land \\forall a \\in L, \\exists a': a \\vee a' = \\top \\land a \\wedge a' = \\bot
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("boolean_algebra_check",
            "布尔代数判定：判定有界分配格L是否为布尔代数（每个元素都有补元）",
            inputs, 1, PRESET_TYPE_BOOLEAN,
            "L \\text{ 是布尔代数} \\Leftrightarrow L \\text{ 是有界分配格} \\land \\forall a \\in L, \\exists a': a \\vee a' = \\top \\land a \\wedge a' = \\bot",
            "O(n^2)", false, false);
    }

    /**
     * @brief boolean_algebra_operations - 布尔代数运算
     *
     * 在布尔代数上执行基本运算：并(∨)、交(∧)、补(')、蕴含(→)、
     * 异或(⊕)、等价(↔)。
     * 布尔代数运算满足De Morgan律、吸收律等经典恒等式。
     *
     * @param a 布尔代数元素（PRESET_TYPE_ALGEBRA）
     * @param b 布尔代数元素（PRESET_TYPE_ALGEBRA）
     * @param op 运算类型（PRESET_TYPE_INTEGER，0=∨, 1=∧, 2=', 3=→, 4=⊕, 5=↔）
     * @return 运算结果（PRESET_TYPE_ALGEBRA）
     * @math a \\vee b, \\; a \\wedge b, \\; a', \\; a \\to b, \\; a \\oplus b, \\; a \\leftrightarrow b
     * @complexity O(1)
     * @constructive true
     * @reversible true（补运算和等价运算是自逆的）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_ALGEBRA, PRESET_TYPE_INTEGER};
        REGISTER_LATTICE("boolean_algebra_operations",
            "布尔代数运算：执行并(∨)、交(∧)、补(')、蕴含(→)、异或(⊕)、等价(↔)运算",
            inputs, 3, PRESET_TYPE_ALGEBRA,
            "a \\vee b, \\; a \\wedge b, \\; a', \\; a \\to b, \\; a \\oplus b, \\; a \\leftrightarrow b",
            "O(1)", true, true);
    }

    /**
     * @brief heyting_algebra_check - Heyting代数判定
     *
     * 判定有界格L是否为Heyting代数。
     * Heyting代数的条件：有界格中任意两个元素a, b都存在相对伪补 a → b，
     * 满足 c ≤ a → b 当且仅当 c ∧ a ≤ b。
     * 每个布尔代数都是Heyting代数，但反之不成立。
     *
     * @param L 有界格（PRESET_TYPE_ALGEBRA）
     * @return 是否为Heyting代数（PRESET_TYPE_BOOLEAN）
     * @math L \\text{ 是Heyting代数} \\Leftrightarrow \\forall a, b \\in L, \\exists a \\to b: c \\leq a \\to b \\Leftrightarrow c \\wedge a \\leq b
     * @complexity O(n^3)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("heyting_algebra_check",
            "Heyting代数判定：判定有界格L是否为Heyting代数（任意两元素存在相对伪补）",
            inputs, 1, PRESET_TYPE_BOOLEAN,
            "L \\text{ 是Heyting代数} \\Leftrightarrow \\forall a, b \\in L, \\exists a \\to b: c \\leq a \\to b \\Leftrightarrow c \\wedge a \\leq b",
            "O(n^3)", false, false);
    }

    /**
     * @brief heyting_implication - Heyting蕴涵
     *
     * 计算Heyting代数中两个元素的相对伪补（Heyting蕴涵）a → b。
     * a → b 是满足 x ∧ a ≤ b 的最大元素 x。
     * 在布尔代数中，a → b = a' ∨ b。
     *
     * @param a 格元素（PRESET_TYPE_ALGEBRA）
     * @param b 格元素（PRESET_TYPE_ALGEBRA）
     * @return Heyting蕴涵 a → b（PRESET_TYPE_ALGEBRA）
     * @math a \\to b = \\max\\{x \\in L : x \\wedge a \\leq b\\}
     * @complexity O(n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("heyting_implication",
            "Heyting蕴涵：计算相对伪补 a → b = max{x : x ∧ a ≤ b}",
            inputs, 2, PRESET_TYPE_ALGEBRA,
            "a \\to b = \\max\\{x \\in L : x \\wedge a \\leq b\\}",
            "O(n)", true, false);
    }

    /**
     * @brief complete_lattice_check - 完备格判定
     *
     * 判定格L是否为完备格。
     * 完备格的条件：L的任意子集（包括空集和L本身）都有上确界和下确界。
     * 由完备格的定义，只需验证任意子集都有上确界（或下确界）即可。
     *
     * @param L 格（PRESET_TYPE_ALGEBRA）
     * @return 是否为完备格（PRESET_TYPE_BOOLEAN）
     * @math L \\text{ 是完备格} \\Leftrightarrow \\forall S \\subseteq L: \\bigvee S \\in L \\land \\bigwedge S \\in L
     * @complexity O(2^n)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("complete_lattice_check",
            "完备格判定：判定格L是否为完备格（任意子集都有上确界和下确界）",
            inputs, 1, PRESET_TYPE_BOOLEAN,
            "L \\text{ 是完备格} \\Leftrightarrow \\forall S \\subseteq L: \\bigvee S \\in L \\land \\bigwedge S \\in L",
            "O(2^n)", false, false);
    }

    /**
     * @brief complete_lattice_sup - 完备格上确界
     *
     * 计算完备格中子集S的上确界（最小上界）∨S。
     * ∨S 是满足 s ≤ ∨S 对所有 s in S 的最小元素。
     *
     * @param S 子集（PRESET_TYPE_SET）
     * @param L 完备格（PRESET_TYPE_ALGEBRA）
     * @return 上确界 ∨S（PRESET_TYPE_ALGEBRA）
     * @math \\bigvee S = \\min\\{c \\in L : \\forall s \\in S, s \\leq c\\}
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("complete_lattice_sup",
            "完备格上确界：计算完备格中子集S的上确界 ∨S",
            inputs, 2, PRESET_TYPE_ALGEBRA,
            "\\bigvee S = \\min\\{c \\in L : \\forall s \\in S, s \\leq c\\}",
            "O(n^2)", true, false);
    }

    /**
     * @brief complete_lattice_inf - 完备格下确界
     *
     * 计算完备格中子集S的下确界（最大下界）∧S。
     * ∧S 是满足 ∧S ≤ s 对所有 s in S 的最大元素。
     *
     * @param S 子集（PRESET_TYPE_SET）
     * @param L 完备格（PRESET_TYPE_ALGEBRA）
     * @return 下确界 ∧S（PRESET_TYPE_ALGEBRA）
     * @math \\bigwedge S = \\max\\{c \\in L : \\forall s \\in S, c \\leq s\\}
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("complete_lattice_inf",
            "完备格下确界：计算完备格中子集S的下确界 ∧S",
            inputs, 2, PRESET_TYPE_ALGEBRA,
            "\\bigwedge S = \\max\\{c \\in L : \\forall s \\in S, c \\leq s\\}",
            "O(n^2)", true, false);
    }

    /**
     * @brief lattice_ideal - 格理想
     *
     * 由格L的子集S生成格理想I。
     * 格理想的条件：I是L的非空子集，对下确界封闭（a, b in I 则 a ∧ b in I），
     * 且是下集（a in I, b ≤ a 则 b in I）。
     * 主理想↓a = {x in L : x ≤ a}。
     *
     * @param S 子集（PRESET_TYPE_SET）
     * @param L 格（PRESET_TYPE_ALGEBRA）
     * @return 格理想（PRESET_TYPE_IDEAL）
     * @math I \\trianglelefteq L \\Leftrightarrow I \\neq \\emptyset \\land (a, b \\in I \\Rightarrow a \\wedge b \\in I) \\land (a \\in I, b \\leq a \\Rightarrow b \\in I)
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_ideal",
            "格理想：由格L的子集S生成格理想（对∧封闭的下集）",
            inputs, 2, PRESET_TYPE_IDEAL,
            "I \\trianglelefteq L \\Leftrightarrow I \\neq \\emptyset \\land (a, b \\in I \\Rightarrow a \\wedge b \\in I) \\land (a \\in I, b \\leq a \\Rightarrow b \\in I)",
            "O(n^2)", true, false);
    }

    /* ============================================================
     * 第三部分：格同态与表示（7个）
     * ============================================================ */

    /**
     * @brief lattice_homomorphism - 格同态
     *
     * 构造或验证格同态 f: L1 -> L2。
     * 格同态满足：f(a ∨ b) = f(a) ∨ f(b) 且 f(a ∧ b) = f(a) ∧ f(b)。
     * 保持上确界和下确界运算。
     *
     * @param L1 源格（PRESET_TYPE_ALGEBRA）
     * @param L2 目标格（PRESET_TYPE_ALGEBRA）
     * @param f 映射（PRESET_TYPE_FUNCTION）
     * @return 是否为格同态（PRESET_TYPE_BOOLEAN）
     * @math f: L_1 \\to L_2 \\text{ 是格同态} \\Leftrightarrow f(a \\vee b) = f(a) \\vee f(b) \\land f(a \\wedge b) = f(a) \\wedge f(b)
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_ALGEBRA, PRESET_TYPE_FUNCTION};
        REGISTER_LATTICE("lattice_homomorphism",
            "格同态：验证映射 f: L1 -> L2 是否为格同态（保持∨和∧运算）",
            inputs, 3, PRESET_TYPE_BOOLEAN,
            "f: L_1 \\to L_2 \\text{ 是格同态} \\Leftrightarrow f(a \\vee b) = f(a) \\vee f(b) \\land f(a \\wedge b) = f(a) \\wedge f(b)",
            "O(n^2)", false, false);
    }

    /**
     * @brief lattice_embedding - 格嵌入
     *
     * 构造或验证格嵌入 f: L1 -> L2。
     * 格嵌入是单射的格同态，保持格的序结构。
     * 等价条件：f(a) ≤ f(b) 当且仅当 a ≤ b。
     *
     * @param L1 源格（PRESET_TYPE_ALGEBRA）
     * @param L2 目标格（PRESET_TYPE_ALGEBRA）
     * @param f 映射（PRESET_TYPE_FUNCTION）
     * @return 是否为格嵌入（PRESET_TYPE_BOOLEAN）
     * @math f: L_1 \\hookrightarrow L_2 \\text{ 是格嵌入} \\Leftrightarrow f \\text{ 是单射格同态}
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_ALGEBRA, PRESET_TYPE_FUNCTION};
        REGISTER_LATTICE("lattice_embedding",
            "格嵌入：验证映射 f: L1 -> L2 是否为格嵌入（单射格同态）",
            inputs, 3, PRESET_TYPE_BOOLEAN,
            "f: L_1 \\hookrightarrow L_2 \\text{ 是格嵌入} \\Leftrightarrow f \\text{ 是单射格同态}",
            "O(n^2)", false, false);
    }

    /**
     * @brief lattice_isomorphism_check - 格同构判定
     *
     * 判定两个格L1和L2是否同构。
     * L1 ≅ L2 当且仅当存在双射的格同态 f: L1 -> L2。
     * 同构的格具有相同的序结构。
     *
     * @param L1 格（PRESET_TYPE_ALGEBRA）
     * @param L2 格（PRESET_TYPE_ALGEBRA）
     * @return 是否同构（PRESET_TYPE_BOOLEAN）
     * @math L_1 \\cong L_2 \\Leftrightarrow \\exists f: L_1 \\to L_2 \\text{ 双射，且为格同态}
     * @complexity O(n!)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_isomorphism_check",
            "格同构判定：判定两个格L1和L2是否同构（存在双射格同态）",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "L_1 \\cong L_2 \\Leftrightarrow \\exists f: L_1 \\to L_2 \\text{ 双射，且为格同态}",
            "O(n!)", false, false);
    }

    /**
     * @brief lattice_sublattice_check - 子格判定
     *
     * 判定格L的子集S是否构成子格。
     * 子格的条件：S非空，且对S中任意两个元素，它们在L中的上确界和下确界都在S中。
     *
     * @param L 格（PRESET_TYPE_ALGEBRA）
     * @param S 子集（PRESET_TYPE_SET）
     * @return 是否为子格（PRESET_TYPE_BOOLEAN）
     * @math S \\le L \\Leftrightarrow S \\neq \\emptyset \\land (\\forall a, b \\in S: a \\vee_L b \\in S \\land a \\wedge_L b \\in S)
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_SET};
        REGISTER_LATTICE("lattice_sublattice_check",
            "子格判定：判定格L的子集S是否构成子格（对∨和∧封闭）",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "S \\le L \\Leftrightarrow S \\neq \\emptyset \\land (\\forall a, b \\in S: a \\vee_L b \\in S \\land a \\wedge_L b \\in S)",
            "O(n^2)", false, false);
    }

    /**
     * @brief lattice_product - 格直积
     *
     * 计算两个格L1和L2的直积 L1 × L2。
     * 直积格上的偏序定义为分量逐点比较：(a1, a2) ≤ (b1, b2) 当且仅当 a1 ≤ b1 且 a2 ≤ b2。
     * 直积格的上确界和下确界按分量计算。
     *
     * @param L1 格（PRESET_TYPE_ALGEBRA）
     * @param L2 格（PRESET_TYPE_ALGEBRA）
     * @return 直积格 L1 × L2（PRESET_TYPE_ALGEBRA）
     * @math L_1 \\times L_2 = \\{(a_1, a_2) : a_1 \\in L_1, a_2 \\in L_2\\}, \\quad (a_1, a_2) \\leq (b_1, b_2) \\Leftrightarrow a_1 \\leq b_1 \\land a_2 \\leq b_2
     * @complexity O(n_1 \\cdot n_2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_product",
            "格直积：计算两个格L1和L2的直积 L1 × L2（分量逐点偏序）",
            inputs, 2, PRESET_TYPE_ALGEBRA,
            "L_1 \\times L_2 = \\{(a_1, a_2) : a_1 \\in L_1, a_2 \\in L_2\\}, \\quad (a_1, a_2) \\leq (b_1, b_2) \\Leftrightarrow a_1 \\leq b_1 \\land a_2 \\leq b_2",
            "O(n_1 \\cdot n_2)", true, false);
    }

    /**
     * @brief lattice_duality - 格对偶性
     *
     * 应用格的对偶原理，将格L转化为其对偶格L^op。
     * 对偶格中偏序关系取反：a ≤_op b 当且仅当 b ≤ a。
     * 在对偶格中，原来的上确界变为下确界，原来的下确界变为上确界。
     *
     * @param L 格（PRESET_TYPE_ALGEBRA）
     * @return 对偶格 L^op（PRESET_TYPE_ALGEBRA）
     * @math L^{op}: a \\leq_{op} b \\Leftrightarrow b \\leq a, \\quad a \\vee_{op} b = a \\wedge b, \\quad a \\wedge_{op} b = a \\vee b
     * @complexity O(n)
     * @constructive true
     * @reversible true（对偶的对偶是自身：(L^op)^op = L）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_duality",
            "格对偶性：将格L转化为对偶格L^op（偏序取反，∨与∧互换）",
            inputs, 1, PRESET_TYPE_ALGEBRA,
            "L^{op}: a \\leq_{op} b \\Leftrightarrow b \\leq a, \\quad a \\vee_{op} b = a \\wedge b, \\quad a \\wedge_{op} b = a \\vee b",
            "O(n)", true, true);
    }

    /**
     * @brief stone_representation - Stone表示定理
     *
     * Stone表示定理：每个布尔代数都同构于某个集合的幂集代数。
     * 具体地，布尔代数B同构于其素理想集合上的幂集代数 P(Spec(B))。
     * 这是布尔代数表示理论的核心定理。
     *
     * @param B 布尔代数（PRESET_TYPE_ALGEBRA）
     * @return Stone表示（PRESET_TYPE_FUNCTION）
     * @math B \\cong \\mathcal{P}(\\text{Spec}(B)), \\quad \\text{其中 Spec}(B) \\text{ 是B的素理想集合}
     * @complexity O(2^n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("stone_representation",
            "Stone表示定理：构造布尔代数B到幂集代数 P(Spec(B)) 的同构表示",
            inputs, 1, PRESET_TYPE_FUNCTION,
            "B \\cong \\mathcal{P}(\\text{Spec}(B)), \\quad \\text{其中 Spec}(B) \\text{ 是B的素理想集合}",
            "O(2^n)", true, false);
    }

    /* ============================================================
     * 第四部分：格与序（5个）
     * ============================================================ */

    /**
     * @brief hasse_diagram - Hasse图
     *
     * 由偏序集(P, ≤)构造其Hasse图。
     * Hasse图是偏序集的图形表示：若 a < b 且不存在 c 使得 a < c < b（即 b 覆盖 a），
     * 则在图中画一条从a到b的线段。Hasse图中y坐标表示偏序关系（上方元素更大）。
     *
     * @param P 偏序集（PRESET_TYPE_SET）
     * @param R 偏序关系（PRESET_TYPE_FUNCTION）
     * @return Hasse图（PRESET_TYPE_GRAPH）
     * @math \\text{Hasse}(P, \\leq): a \\to b \\text{ 当且仅当 } b \\text{ 覆盖 } a \\text{（即 } a < b \\text{ 且 } \\nexists c: a < c < b\\text{）}
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_FUNCTION};
        REGISTER_LATTICE("hasse_diagram",
            "Hasse图：由偏序集(P, ≤)构造Hasse图（覆盖关系图）",
            inputs, 2, PRESET_TYPE_GRAPH,
            "\\text{Hasse}(P, \\leq): a \\to b \\text{ 当且仅当 } b \\text{ 覆盖 } a",
            "O(n^2)", true, false);
    }

    /**
     * @brief chain_check - 链判定（全序）
     *
     * 判定偏序集(P, ≤)中的子集S是否构成链。
     * 链的条件：S中任意两个元素都是可比较的（即对任意a, b in S，有 a ≤ b 或 b ≤ a）。
     * 链是全序集的子集。
     *
     * @param S 子集（PRESET_TYPE_SET）
     * @param P 偏序集（PRESET_TYPE_SET）
     * @param R 偏序关系（PRESET_TYPE_FUNCTION）
     * @return 是否为链（PRESET_TYPE_BOOLEAN）
     * @math S \\text{ 是链} \\Leftrightarrow \\forall a, b \\in S: a \\leq b \\lor b \\leq a
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_FUNCTION};
        REGISTER_LATTICE("chain_check",
            "链判定（全序）：判定偏序集的子集S是否构成链（任意两元素可比较）",
            inputs, 3, PRESET_TYPE_BOOLEAN,
            "S \\text{ 是链} \\Leftrightarrow \\forall a, b \\in S: a \\leq b \\lor b \\leq a",
            "O(n^2)", false, false);
    }

    /**
     * @brief antichain_check - 反链判定
     *
     * 判定偏序集(P, ≤)中的子集S是否构成反链。
     * 反链的条件：S中任意两个不同元素都不可比较（即对任意a ≠ b in S，有 a ≰ b 且 b ≰ a）。
     * 由Dilworth定理，偏序集可分解为宽度个链的不相交并。
     *
     * @param S 子集（PRESET_TYPE_SET）
     * @param P 偏序集（PRESET_TYPE_SET）
     * @param R 偏序关系（PRESET_TYPE_FUNCTION）
     * @return 是否为反链（PRESET_TYPE_BOOLEAN）
     * @math S \\text{ 是反链} \\Leftrightarrow \\forall a, b \\in S: a \\neq b \\Rightarrow a \\not\\leq b \\land b \\not\\leq a
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_FUNCTION};
        REGISTER_LATTICE("antichain_check",
            "反链判定：判定偏序集的子集S是否构成反链（任意两不同元素不可比较）",
            inputs, 3, PRESET_TYPE_BOOLEAN,
            "S \\text{ 是反链} \\Leftrightarrow \\forall a, b \\in S: a \\neq b \\Rightarrow a \\not\\leq b \\land b \\not\\leq a",
            "O(n^2)", false, false);
    }

    /**
     * @brief lattice_height - 格的高度
     *
     * 计算格L的高度（最长链的长度）。
     * 格的高度定义为从最小元到最大元的最长链中元素的个数减一。
     * 对于有限格，高度等于覆盖关系的最长路径长度。
     *
     * @param L 格（PRESET_TYPE_ALGEBRA）
     * @return 格的高度（PRESET_TYPE_INTEGER）
     * @math h(L) = \\max\\{|C| - 1 : C \\text{ 是 } L \\text{ 中的链}\\}
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_height",
            "格的高度：计算格L的高度（最长链的长度）",
            inputs, 1, PRESET_TYPE_INTEGER,
            "h(L) = \\max\\{|C| - 1 : C \\text{ 是 } L \\text{ 中的链}\\}",
            "O(n^2)", true, false);
    }

    /**
     * @brief lattice_width - 格的宽度
     *
     * 计算格L的宽度（最大反链的大小）。
     * 格的宽度定义为L中最大反链所含元素的个数。
     * 由Dilworth定理，宽度等于将L分解为不相交链所需的最少链数。
     *
     * @param L 格（PRESET_TYPE_ALGEBRA）
     * @return 格的宽度（PRESET_TYPE_INTEGER）
     * @math w(L) = \\max\\{|A| : A \\text{ 是 } L \\text{ 中的反链}\\}
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        REGISTER_LATTICE("lattice_width",
            "格的宽度：计算格L的宽度（最大反链的大小）",
            inputs, 1, PRESET_TYPE_INTEGER,
            "w(L) = \\max\\{|A| : A \\text{ 是 } L \\text{ 中的反链}\\}",
            "O(n^2)", true, false);
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == LATTICE_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取格论预设函数块数量
 *
 * @return int 格论模块预设函数块总数
 */
int preset_lattice_theory_count(void)
{
    return LATTICE_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取格论预设函数块名称列表
 *
 * @param out_names 输出名称数组指针（调用者负责释放）
 * @param out_count 输出预设数量
 * @return true 获取成功
 * @return false 获取失败
 */
bool preset_lattice_theory_get_names(char ***out_names, int *out_count)
{
    if (out_names == NULL || out_count == NULL) {
        return false;
    }

    *out_count = LATTICE_THEORY_PRESET_COUNT;

    /* 分配名称数组（使用项目统一的内存管理函数） */
    char **names = (char **)lv00_malloc(LATTICE_THEORY_PRESET_COUNT * sizeof(char *));
    if (names == NULL) {
        return false;
    }

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 格基础运算 */
        "lattice_join",
        "lattice_meet",
        "lattice_top",
        "lattice_bottom",
        "lattice_complement",
        "lattice_partial_order",
        "lattice_check",
        "lattice_bounded_check",
        "lattice_distributive_check",
        "lattice_modular_check",
        /* 特殊格 */
        "boolean_algebra_check",
        "boolean_algebra_operations",
        "heyting_algebra_check",
        "heyting_implication",
        "complete_lattice_check",
        "complete_lattice_sup",
        "complete_lattice_inf",
        "lattice_ideal",
        /* 格同态与表示 */
        "lattice_homomorphism",
        "lattice_embedding",
        "lattice_isomorphism_check",
        "lattice_sublattice_check",
        "lattice_product",
        "lattice_duality",
        "stone_representation",
        /* 格与序 */
        "hasse_diagram",
        "chain_check",
        "antichain_check",
        "lattice_height",
        "lattice_width"
    };

    for (int i = 0; i < LATTICE_THEORY_PRESET_COUNT; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 分配失败时释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv00_free(&tmp);
            }
            lv00_free((void **)&names);
            return false;
        }
    }

    *out_names = names;
    return true;
}

/**
 * @brief 获取格论模块类别名称
 *
 * @return 类别名称字符串
 */
const char *preset_lattice_theory_category(void)
{
    return "格论";
}
