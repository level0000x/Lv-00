/**
 * @file preset_math_logic.c
 * @brief 数理逻辑预设函数块 - 实现
 *
 * 实现理论数学研究中常用的数理逻辑运算预设函数块。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module MathLogic
 * @category PRESET_CATEGORY_MATH_LOGIC
 * @version 4.0.0
 */

#include "preset_math_logic.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* 内部别名：与 preset_math_logic.h 中 MATH_LOGIC_PRESET_COUNT 一致 */
#define ADVANCED_MATH_LOGIC_PRESET_COUNT MATH_LOGIC_PRESET_COUNT

/* ==================== 预设函数块数量 ==================== */

/** 数理逻辑模块预设函数块总数 */

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个数理逻辑预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有数理逻辑预设都属于 PRESET_CATEGORY_MATH_LOGIC 类别。
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
static bool register_logic_preset(const char *name, const char *description, const PresetType *input_types,
                                  int input_count, PresetType output_type, const char *math_def, const char *complexity,
                                  bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_MATH_LOGIC, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_math_logic_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：命题逻辑
     * ============================================================ */

    /* -------------------- 合取 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        if (register_logic_preset(PRESET_LOGIC_CONJUNCTION, "命题合取：P ∧ Q，当且仅当 P 和 Q 均为真时结果为真", inputs,
                                  2, PRESET_TYPE_BOOLEAN, "P \\land Q", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 析取 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        if (register_logic_preset(PRESET_LOGIC_DISJUNCTION, "命题析取：P ∨ Q，当且仅当 P 或 Q 至少一个为真时结果为真",
                                  inputs, 2, PRESET_TYPE_BOOLEAN, "P \\lor Q", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 否定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN};
        if (register_logic_preset(PRESET_LOGIC_NEGATION, "命题否定：¬P，当 P 为真时结果为假，反之亦然", inputs, 1,
                                  PRESET_TYPE_BOOLEAN, "\\lnot P", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 蕴含 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        if (register_logic_preset(PRESET_LOGIC_IMPLICATION, "命题蕴含：P → Q，等价于 ¬P ∨ Q", inputs, 2,
                                  PRESET_TYPE_BOOLEAN, "P \\to Q \\equiv \\lnot P \\lor Q", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 等价 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        if (register_logic_preset(PRESET_LOGIC_BICONDITIONAL, "命题等价：P ↔ Q，当 P 和 Q 真值相同时结果为真", inputs,
                                  2, PRESET_TYPE_BOOLEAN, "P \\leftrightarrow Q \\equiv (P \\to Q) \\land (Q \\to P)",
                                  "O(1)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 永真式判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA};
        if (register_logic_preset(PRESET_LOGIC_TAUTOLOGY_TEST, "判定命题公式是否为永真式（对所有真值指派均为真）",
                                  inputs, 1, PRESET_TYPE_BOOLEAN,
                                  "\\varphi \\text{ 是永真式} \\Leftrightarrow \\forall v: v(\\varphi) = \\text{T}",
                                  "O(2^n)，n 为命题变元数", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 可满足性判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA};
        if (register_logic_preset(PRESET_LOGIC_SATISFIABILITY_TEST, "判定命题公式是否可满足（存在使其为真的真值指派）",
                                  inputs, 1, PRESET_TYPE_BOOLEAN,
                                  "\\varphi \\text{ 可满足} \\Leftrightarrow \\exists v: v(\\varphi) = \\text{T}",
                                  "O(2^n)，n 为命题变元数", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：一阶逻辑
     * ============================================================ */

    /* -------------------- 全称实例化 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_ANY};
        if (register_logic_preset(PRESET_LOGIC_UNIVERSAL_INSTANTIATION,
                                  "全称实例化：从 ∀x P(x) 推出 P(c)，c 为论域中任意元素", inputs, 2,
                                  PRESET_TYPE_FORMULA, "\\forall x \\, P(x) \\vdash P(c)", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 存在泛化 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_ANY};
        if (register_logic_preset(PRESET_LOGIC_EXISTENTIAL_GENERALIZATION,
                                  "存在泛化：从 P(c) 推出 ∃x P(x)，c 为论域中某个元素", inputs, 2, PRESET_TYPE_FORMULA,
                                  "P(c) \\vdash \\exists x \\, P(x)", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 全称泛化 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_ANY};
        if (register_logic_preset(PRESET_LOGIC_UNIVERSAL_GENERALIZATION,
                                  "全称泛化：从 P(c) 推出 ∀x P(x)，要求 c 为任意常量且未被特殊假定", inputs, 2,
                                  PRESET_TYPE_FORMULA, "P(c) \\vdash \\forall x \\, P(x), \\quad c \\text{ 为任意常量}",
                                  "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 存在实例化 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_ANY};
        if (register_logic_preset(PRESET_LOGIC_EXISTENTIAL_INSTANTIATION,
                                  "存在实例化：从 ∃x P(x) 引入 P(c)，c 为此前未出现的新常量", inputs, 2,
                                  PRESET_TYPE_FORMULA, "\\exists x \\, P(x) \\vdash P(c), \\quad c \\text{ 为新常量}",
                                  "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：证明系统
     * ============================================================ */

    /* -------------------- 自然演绎 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_SEQUENCE};
        if (register_logic_preset(
                PRESET_LOGIC_NATURAL_DEDUCTION,
                "自然演绎系统：基于引入规则（∧I, ∨I, →I, ∀I, ∃I）和消去规则（∧E, ∨E, →E, ∀E, ∃E）的证明", inputs, 2,
                PRESET_TYPE_BOOLEAN, "\\Gamma \\vdash_{\\text{ND}} \\varphi", "O(2^{|\\Gamma|})", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 归结原理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_logic_preset(
                PRESET_LOGIC_RESOLUTION, "归结原理：将公式集转化为子句集，通过归结消解推导空子句以证明不可满足性",
                inputs, 1, PRESET_TYPE_BOOLEAN, "S \\vdash_{\\text{Res}} \\Box \\Leftrightarrow S \\text{ 不可满足}",
                "O(n^k)，n 为子句数", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 表方法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA};
        if (register_logic_preset(
                PRESET_LOGIC_TABLEAU_METHOD, "语义表方法：通过系统性地分解公式构建反证树，判定公式的可满足性", inputs,
                1, PRESET_TYPE_BOOLEAN, "\\lnot \\varphi \\text{ 的表封闭} \\Leftrightarrow \\varphi \\text{ 有效}",
                "O(2^n)，n 为子公式数", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：模型论
     * ============================================================ */

    /* -------------------- 模型判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_SEQUENCE};
        if (register_logic_preset(
                PRESET_LOGIC_MODEL_CHECK, "模型判定：判定结构 M 是否为公式集 Γ 的模型（M ⊨ Γ）", inputs, 2,
                PRESET_TYPE_BOOLEAN,
                "M \\models \\Gamma \\Leftrightarrow \\forall \\varphi \\in \\Gamma, M \\models \\varphi",
                "O(|\\Gamma| \\cdot |M|)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 有效式判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA};
        if (register_logic_preset(PRESET_LOGIC_VALIDITY_TEST, "有效式判定：判定一阶公式是否在所有结构中都为真（⊨ φ）",
                                  inputs, 1, PRESET_TYPE_BOOLEAN,
                                  "\\models \\varphi \\Leftrightarrow \\forall M, M \\models \\varphi",
                                  "不可判定（半可判定）", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 模型论可满足性 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_logic_preset(PRESET_LOGIC_MODEL_SATISFIABILITY,
                                  "模型论可满足性：判定一阶公式集 Γ 是否存在一致的模型（Γ 有模型）", inputs, 1,
                                  PRESET_TYPE_BOOLEAN,
                                  "\\Gamma \\text{ 可满足} \\Leftrightarrow \\exists M: M \\models \\Gamma",
                                  "不可判定（半可判定）", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：递归论
     * ============================================================ */

    /* -------------------- 图灵机判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRING};
        if (register_logic_preset(
                PRESET_LOGIC_TURING_MACHINE_CHECK,
                "判定给定描述是否定义了一台合法的图灵机（状态集、字母表、转移函数完备性检验）", inputs, 1,
                PRESET_TYPE_BOOLEAN,
                "T = (Q, \\Sigma, \\Gamma, \\delta, q_0, q_{\\text{acc}}, q_{\\text{rej}}) \\text{ 合法性检验}",
                "O(|Q| \\cdot |\\Gamma|)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 递归函数判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_logic_preset(PRESET_LOGIC_RECURSIVE_CHECK, "判定给定函数是否为递归函数（可通过图灵机计算）",
                                  inputs, 1, PRESET_TYPE_BOOLEAN,
                                  "f \\text{ 是递归函数} \\Leftrightarrow \\exists T_M: T_M(x) = f(x)", "不可判定",
                                  false, false)) {
            success_count++;
        }
    }

    /* -------------------- 停机问题 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRING, PRESET_TYPE_STRING};
        if (register_logic_preset(
                PRESET_LOGIC_HALTING_PROBLEM, "停机问题：判定图灵机 M 在输入 w 上是否最终停机（经典不可判定问题）",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "H(M, w) = \\begin{cases} 1 & M \\text{ 在 } w \\text{ 上停机} \\\\ 0 & \\text{否则} \\end{cases}",
                "不可判定", false, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    /* lv_log_info("数理逻辑预设注册完成，共 %d 个预设", success_count) */
    return success_count == ADVANCED_MATH_LOGIC_PRESET_COUNT;
}

/**
 * @brief 获取数理逻辑预设函数块的类别
 * @return 预设类别枚举值
 */
PresetCategory preset_math_logic_category(void) {
    return PRESET_CATEGORY_LOGIC;
}

/**
 * @brief 获取数理逻辑预设函数块的名称列表
 * @param out_names 输出：名称数组（调用者需先释放每个元素再释放数组本身）
 * @param out_count 输出：名称数量
 * @return true 成功，false 内存分配失败
 */
bool preset_math_logic_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;
    *out_count = ADVANCED_MATH_LOGIC_PRESET_COUNT;
    char **names = (char **) lv_malloc(ADVANCED_MATH_LOGIC_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        /* 命题逻辑 */
        PRESET_LOGIC_CONJUNCTION,
        PRESET_LOGIC_DISJUNCTION,
        PRESET_LOGIC_NEGATION,
        PRESET_LOGIC_IMPLICATION,
        PRESET_LOGIC_BICONDITIONAL,
        PRESET_LOGIC_TAUTOLOGY_TEST,
        PRESET_LOGIC_SATISFIABILITY_TEST,
        /* 一阶逻辑 */
        PRESET_LOGIC_UNIVERSAL_INSTANTIATION,
        PRESET_LOGIC_EXISTENTIAL_GENERALIZATION,
        PRESET_LOGIC_UNIVERSAL_GENERALIZATION,
        PRESET_LOGIC_EXISTENTIAL_INSTANTIATION,
        /* 证明系统 */
        PRESET_LOGIC_NATURAL_DEDUCTION,
        PRESET_LOGIC_RESOLUTION,
        PRESET_LOGIC_TABLEAU_METHOD,
        /* 模型论 */
        PRESET_LOGIC_MODEL_CHECK,
        PRESET_LOGIC_VALIDITY_TEST,
        PRESET_LOGIC_MODEL_SATISFIABILITY,
        /* 递归论 */
        PRESET_LOGIC_TURING_MACHINE_CHECK,
        PRESET_LOGIC_RECURSIVE_CHECK,
        PRESET_LOGIC_HALTING_PROBLEM,
    };

    for (int i = 0; i < ADVANCED_MATH_LOGIC_PRESET_COUNT; i++) {
        size_t len = strlen(preset_names[i]) + 1;
        names[i] = (char *) lv_malloc(len);
        if (!names[i]) {
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv_free(&tmp);
            }
            {
                void *tmp = names;
                lv_free(&tmp);
            }
            return false;
        }
        memcpy(names[i], preset_names[i], len);
    }
    *out_names = names;
    return true;
}

int preset_math_logic_count(void) {
    return ADVANCED_MATH_LOGIC_PRESET_COUNT;
}
