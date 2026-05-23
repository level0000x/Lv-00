/**
 * @file preset_logic_advanced.c
 * @brief 高级逻辑预设函数块 - 实现
 *
 * 实现理论数学研究中常用的高级逻辑运算预设函数块。
 * 涵盖经典推理规则、联结词引入/消除规则、量词规则、
 * 证明方法、自动推理技术及范式转换。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module LogicAdvanced
 * @category PRESET_CATEGORY_LOGIC
 * @version 4.0.0
 */

#include "preset_logic_advanced.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 高级逻辑模块预设函数块总数 */
#define LOGIC_ADVANCED_PRESET_COUNT 20

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个高级逻辑预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有高级逻辑预设都属于 PRESET_CATEGORY_LOGIC 类别。
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
static bool register_logic_advanced_preset(
    const char *name,
    const char *description,
    const PresetType *input_types,
    int input_count,
    PresetType output_type,
    const char *math_def,
    const char *complexity,
    bool is_constructive,
    bool is_reversible)
{
    return preset_blocks_register_simple(
        name, description,
        PRESET_CATEGORY_LOGIC,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_logic_advanced_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：经典推理规则
     * ============================================================ */

    /* -------------------- 假言推理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_MODUS_PONENS,
                "假言推理：从蕴含式 P -> Q 和前件 P 推出后件 Q",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "(P \\to Q), P \\vdash Q",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 逆否推理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_MODUS_TOLLENS,
                "逆否推理：从蕴含式 P -> Q 和后件的否定 ~Q 推出前件的否定 ~P",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "(P \\to Q), \\neg Q \\vdash \\neg P",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 假言三段论 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_HYPOTHETICAL_SYLLOGISM,
                "假言三段论：从 P -> Q 和 Q -> R 推出 P -> R",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "(P \\to Q), (Q \\to R) \\vdash (P \\to R)",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 析取三段论 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_DISJUNCTIVE_SYLLOGISM,
                "析取三段论：从 P \\/ Q 和 ~P 推出 Q",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "(P \\lor Q), \\neg P \\vdash Q",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：联结词规则
     * ============================================================ */

    /* -------------------- 合取引入 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_CONJUNCTION_INTRO,
                "合取引入：从 P 和 Q 推出 P /\\ Q",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "P, Q \\vdash P \\land Q",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 合取消除 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_CONJUNCTION_ELIM,
                "合取消除：从 P /\\ Q 推出 P（左消除）或 Q（右消除）",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "P \\land Q \\vdash P \\quad \\text{（或 } Q \\text{）}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 析取引入 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_DISJUNCTION_INTRO,
                "析取引入：从 P 推出 P \\/ Q（左引入）或 Q \\/ P（右引入）",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "P \\vdash P \\lor Q \\quad \\text{（或 } Q \\lor P \\text{）}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 析取消除 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_DISJUNCTION_ELIM,
                "析取消除：从 P \\/ Q、P -> R、Q -> R 推出 R",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "(P \\lor Q), (P \\to R), (Q \\to R) \\vdash R",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 否定引入 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_NEGATION_INTRO,
                "否定引入（反证法）：若假设 P 推出矛盾 F，则推出 ~P",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\text{若 } (P \\vdash \\bot) \\text{ 则 } \\vdash \\neg P",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 双重否定消除 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_DOUBLE_NEGATION_ELIM,
                "双重否定消除：从 ~~P 推出 P",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\neg \\neg P \\vdash P",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：量词规则
     * ============================================================ */

    /* -------------------- 全称引入 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_UNIVERSAL_INTRO,
                "全称引入：若对任意常数 c 都能证明 P(c)，则推出 forall x P(x)",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\text{若 } c \\text{ 是任意的，且 } P(c) \\text{ 可证，则 } \\vdash \\forall x \\, P(x)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 全称消除 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_ANY};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_UNIVERSAL_ELIM,
                "全称消除：从 forall x P(x) 推出 P(t)（对任意项 t）",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "\\forall x \\, P(x) \\vdash P(t)",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 存在引入 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_ANY};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_EXISTENTIAL_INTRO,
                "存在引入：从 P(t) 推出 exists x P(x)",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "P(t) \\vdash \\exists x \\, P(x)",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 存在消除 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_FUNCTION};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_EXISTENTIAL_ELIM,
                "存在消除：从 exists x P(x) 和 P(c) -> C（c 不在 C 中自由出现）推出 C",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "\\exists x \\, P(x), \\; (\\forall x, P(x) \\to C) \\vdash C",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：证明方法
     * ============================================================ */

    /* -------------------- 归谬法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_PROOF_BY_CONTRADICTION,
                "归谬法：若假设 ~P 推出矛盾 F，则推出 P",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\text{若 } (\\neg P \\vdash \\bot) \\text{ 则 } \\vdash P",
                "O(n)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：自动推理
     * ============================================================ */

    /* -------------------- 消解原理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_RESOLUTION,
                "消解原理：从两个互补文字的子句推出消解式",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "(P \\lor Q), (\\neg P \\lor R) \\vdash (Q \\lor R)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 合一算法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_UNIFICATION,
                "合一算法：求解两个原子公式的最一般合一（MGU），如 unify(P(x, a), P(b, y)) = {x->b, y->a}",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "\\text{mgu}(A_1, A_2) = \\theta, \\quad A_1\\theta = A_2\\theta",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：范式转换
     * ============================================================ */

    /* -------------------- 斯柯伦化 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_SKOLEMIZE,
                "斯柯伦化：消除前束范式中的存在量词，引入斯柯伦函数",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\forall x_1 \\exists y_1 \\forall x_2 \\exists y_2 \\, \\phi "
                "\\Rightarrow \\forall x_1 \\forall x_2 \\, \\phi[y_1/f(x_1), y_2/g(x_1,x_2)]",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 合取范式转换 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_CNF_CONVERT,
                "合取范式转换：将一阶逻辑公式转换为合取范式 CNF",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\phi \\Rightarrow \\bigwedge_{i=1}^{m} \\bigvee_{j=1}^{n_i} L_{ij}, "
                "\\quad L_{ij} \\text{ 为文字}",
                "O(2^n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 析取范式转换 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN};
        if (register_logic_advanced_preset(
                PRESET_LOGIC_DNF_CONVERT,
                "析取范式转换：将一阶逻辑公式转换为析取范式 DNF",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\phi \\Rightarrow \\bigvee_{i=1}^{m} \\bigwedge_{j=1}^{n_i} L_{ij}, "
                "\\quad L_{ij} \\text{ 为文字}",
                "O(2^n)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == LOGIC_ADVANCED_PRESET_COUNT;
}

/**
 * @brief 获取高级逻辑预设函数块数量
 *
 * @return int 高级逻辑模块预设函数块总数
 */
int preset_logic_advanced_count(void)
{
    return LOGIC_ADVANCED_PRESET_COUNT;
}
