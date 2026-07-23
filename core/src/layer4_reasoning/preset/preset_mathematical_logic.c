/**
 * @file preset_mathematical_logic.c
 * @brief 数理逻辑预设函数块模块 - 实现
 *
 * 实现理论数学研究项目Lv-00中数理逻辑领域的预设函数块。
 * 采用v2统一宏模式，使用 REGISTER_LOGIC 宏简化注册流程。
 *
 * 模块包含40个预设，分为五大类别：
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

#include "preset_mathematical_logic.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* 内部别名：与 preset_mathematical_logic.h 中 MATHEMATICAL_LOGIC_PRESET_COUNT 一致 */
#define MATH_LOGIC_PRESET_COUNT MATHEMATICAL_LOGIC_PRESET_COUNT

/* ==================== 预设函数块数量 ==================== */

/** 数理逻辑模块预设函数块总数 */

/* ==================== 内部辅助函数与宏 ==================== */

/**
 * @brief 注册单个数理逻辑预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有数理逻辑预设都属于 PRESET_CATEGORY_LOGIC 类别。
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
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_LOGIC, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/**
 * @brief 简化预设注册的宏
 *
 * 减少重复代码，提高可维护性。
 * 注册成功时递增 success_count，失败时输出错误日志。
 */
#define REGISTER_LOGIC(name, desc, inputs, in_count, output, math, comp, cons, rev)                                 \
    do {                                                                                                            \
        if (register_logic_preset((name), (desc), (inputs), (in_count), (output), (math), (comp), (cons), (rev))) { \
            success_count++;                                                                                        \
        } else {                                                                                                    \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */                                                     \
        }                                                                                                           \
    } while (0)

/* ==================== 模块注册实现 ==================== */

bool preset_mathematical_logic_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：命题逻辑（12个）
     * ============================================================ */

    /* -------------------- 1. 合取：P ∧ Q -------------------- */
    /**
     * @brief 命题合取
     *
     * 对两个命题进行合取运算。当且仅当 P 和 Q 均为真时，结果为真。
     * 合取是命题逻辑中最基本的二元联结词之一。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        REGISTER_LOGIC("prop_and", "命题合取：P ∧ Q，当且仅当 P 和 Q 均为真时结果为真", inputs, 2, PRESET_TYPE_BOOLEAN,
                       "P \\land Q \\equiv \\begin{cases} \\text{T} & P = \\text{T} \\land Q = \\text{T} \\\\ "
                       "\\text{F} & \\text{otherwise} \\end{cases}",
                       "O(1)", true, false);
    }

    /* -------------------- 2. 析取：P ∨ Q -------------------- */
    /**
     * @brief 命题析取
     *
     * 对两个命题进行析取运算。当 P 或 Q 至少一个为真时，结果为真。
     * 此处为包含性析取（inclusive disjunction）。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        REGISTER_LOGIC("prop_or", "命题析取：P ∨ Q，当 P 或 Q 至少一个为真时结果为真", inputs, 2, PRESET_TYPE_BOOLEAN,
                       "P \\lor Q \\equiv \\begin{cases} \\text{F} & P = \\text{F} \\land Q = \\text{F} \\\\ \\text{T} "
                       "& \\text{otherwise} \\end{cases}",
                       "O(1)", true, false);
    }

    /* -------------------- 3. 否定：¬P -------------------- */
    /**
     * @brief 命题否定
     *
     * 对命题进行否定运算。当 P 为真时结果为假，反之亦然。
     * 否定是命题逻辑中唯一的一元联结词，且是自逆运算。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN};
        REGISTER_LOGIC(
            "prop_not", "命题否定：¬P，当 P 为真时结果为假，反之亦然", inputs, 1, PRESET_TYPE_BOOLEAN,
            "\\lnot P \\equiv \\begin{cases} \\text{F} & P = \\text{T} \\\\ \\text{T} & P = \\text{F} \\end{cases}",
            "O(1)", true, true);
    }

    /* -------------------- 4. 蕴涵：P → Q -------------------- */
    /**
     * @brief 命题蕴涵（实质蕴涵）
     *
     * 定义 P → Q 等价于 ¬P ∨ Q。仅在 P 为真而 Q 为假时结果为假。
     * 蕴涵是逻辑推理和条件语句的基础。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        REGISTER_LOGIC("prop_implies", "命题蕴涵：P → Q，等价于 ¬P ∨ Q，仅在 P 真且 Q 假时为假", inputs, 2,
                       PRESET_TYPE_BOOLEAN,
                       "P \\to Q \\equiv \\lnot P \\lor Q \\equiv \\begin{cases} \\text{F} & P = \\text{T}, Q = "
                       "\\text{F} \\\\ \\text{T} & \\text{otherwise} \\end{cases}",
                       "O(1)", true, false);
    }

    /* -------------------- 5. 等价：P ↔ Q -------------------- */
    /**
     * @brief 命题等价（双条件）
     *
     * 定义 P ↔ Q 等价于 (P → Q) ∧ (Q → P)。当 P 和 Q 真值相同时为真。
     * 等价关系具有自反性、对称性和传递性。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        REGISTER_LOGIC("prop_iff", "命题等价：P ↔ Q，当 P 和 Q 真值相同时结果为真", inputs, 2, PRESET_TYPE_BOOLEAN,
                       "P \\leftrightarrow Q \\equiv (P \\to Q) \\land (Q \\to P)", "O(1)", true, true);
    }

    /* -------------------- 6. 异或：P ⊕ Q -------------------- */
    /**
     * @brief 命题异或（排他性析取）
     *
     * 定义 P ⊕ Q 等价于 (P ∨ Q) ∧ ¬(P ∧ Q)。当 P 和 Q 恰好一个为真时结果为真。
     * 异或在信息论、密码学和数字电路中有广泛应用。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        REGISTER_LOGIC("prop_xor", "命题异或：P ⊕ Q，当 P 和 Q 恰好一个为真时结果为真", inputs, 2, PRESET_TYPE_BOOLEAN,
                       "P \\oplus Q \\equiv (P \\lor Q) \\land \\lnot(P \\land Q)", "O(1)", true, true);
    }

    /* -------------------- 7. 与非：P ↑ Q -------------------- */
    /**
     * @brief 与非（Sheffer竖线）
     *
     * 定义 P ↑ Q 等价于 ¬(P ∧ Q)。与非运算是功能完备的，
     * 即仅用与非运算即可表达所有命题逻辑公式。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        REGISTER_LOGIC("prop_nand", "命题与非：P ↑ Q ≡ ¬(P ∧ Q)，Sheffer竖线，功能完备联结词", inputs, 2,
                       PRESET_TYPE_BOOLEAN, "P \\uparrow Q \\equiv \\lnot(P \\land Q)", "O(1)", true, false);
    }

    /* -------------------- 8. 或非：P ↓ Q -------------------- */
    /**
     * @brief 或非（Peirce箭头）
     *
     * 定义 P ↓ Q 等价于 ¬(P ∨ Q)。或非运算同样是功能完备的，
     * 即仅用或非运算即可表达所有命题逻辑公式。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_BOOLEAN, PRESET_TYPE_BOOLEAN};
        REGISTER_LOGIC("prop_nor", "命题或非：P ↓ Q ≡ ¬(P ∨ Q)，Peirce箭头，功能完备联结词", inputs, 2,
                       PRESET_TYPE_BOOLEAN, "P \\downarrow Q \\equiv \\lnot(P \\lor Q)", "O(1)", true, false);
    }

    /* -------------------- 9. 重言式判定 -------------------- */
    /**
     * @brief 重言式判定
     *
     * 判定命题公式是否为重言式（永真式），即对所有可能的真值指派，
     * 公式的真值均为真。通过穷举所有真值指派进行判定。
     * 该问题是 co-NP 完全的。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC(
            "prop_tautology_check", "重言式判定：判定命题公式是否对所有真值指派均为真", inputs, 1, PRESET_TYPE_BOOLEAN,
            "\\varphi \\text{ 是重言式} \\Leftrightarrow \\forall v \\in \\{0,1\\}^n: v(\\varphi) = \\text{T}",
            "O(2^n)，n 为命题变元数", true, false);
    }

    /* -------------------- 10. 矛盾式判定 -------------------- */
    /**
     * @brief 矛盾式判定
     *
     * 判定命题公式是否为矛盾式（永假式），即对所有可能的真值指派，
     * 公式的真值均为假。矛盾式是重言式的否定。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC(
            "prop_contradiction_check", "矛盾式判定：判定命题公式是否对所有真值指派均为假", inputs, 1,
            PRESET_TYPE_BOOLEAN,
            "\\varphi \\text{ 是矛盾式} \\Leftrightarrow \\forall v \\in \\{0,1\\}^n: v(\\varphi) = \\text{F}",
            "O(2^n)，n 为命题变元数", true, false);
    }

    /* -------------------- 11. 可满足性判定 -------------------- */
    /**
     * @brief 可满足性判定（SAT）
     *
     * 判定命题公式是否可满足，即是否存在至少一组真值指派使公式为真。
     * 这是计算复杂性理论中的核心问题，是 NP 完全的（Cook-Levin 定理）。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC("prop_satisfiable_check", "可满足性判定（SAT）：判定命题公式是否存在使其为真的真值指派", inputs,
                       1, PRESET_TYPE_BOOLEAN,
                       "\\varphi \\text{ 可满足} \\Leftrightarrow \\exists v \\in \\{0,1\\}^n: v(\\varphi) = \\text{T}",
                       "NP 完全（Cook-Levin 定理）", true, false);
    }

    /* -------------------- 12. 析取范式转换 -------------------- */
    /**
     * @brief 析取范式（DNF）转换
     *
     * 将任意命题公式转换为等价的析取范式。
     * 析取范式是合取子句（基本合取式）的析取。
     * 每个命题公式都存在与之等价的析取范式。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC(
            "prop_dnf", "析取范式转换：将命题公式转换为等价的析取范式（DNF）", inputs, 1, PRESET_TYPE_EXPRESSION,
            "\\varphi \\equiv \\bigvee_{i=1}^{m} \\bigwedge_{j=1}^{k_i} l_{ij}, \\quad l_{ij} \\in \\{p, \\lnot p\\}",
            "O(2^n)，n 为命题变元数", true, false);
    }

    /* ============================================================
     * 第二部分：一阶逻辑（10个）
     * ============================================================ */

    /* -------------------- 13. 全称量化：∀x P(x) -------------------- */
    /**
     * @brief 全称量化
     *
     * 对谓词施加全称量化，表示论域中所有元素都满足谓词 P。
     * ∀x P(x) 为真当且仅当对论域 D 中每一个元素 d，P(d) 均为真。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION, PRESET_TYPE_SET};
        REGISTER_LOGIC("fol_forall", "全称量化：∀x P(x)，论域中所有元素均满足谓词 P", inputs, 2, PRESET_TYPE_EXPRESSION,
                       "\\forall x \\in D, \\; P(x) \\Leftrightarrow \\forall d \\in D: P(d) = \\text{T}",
                       "O(|D|)，|D| 为论域大小", false, false);
    }

    /* -------------------- 14. 存在量化：∃x P(x) -------------------- */
    /**
     * @brief 存在量化
     *
     * 对谓词施加存在量化，表示论域中至少存在一个元素满足谓词 P。
     * ∃x P(x) 为真当且仅当论域 D 中存在某个元素 d 使得 P(d) 为真。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION, PRESET_TYPE_SET};
        REGISTER_LOGIC("fol_exists", "存在量化：∃x P(x)，论域中至少存在一个元素满足谓词 P", inputs, 2,
                       PRESET_TYPE_EXPRESSION,
                       "\\exists x \\in D, \\; P(x) \\Leftrightarrow \\exists d \\in D: P(d) = \\text{T}",
                       "O(|D|)，|D| 为论域大小", true, false);
    }

    /* -------------------- 15. 量词否定：¬∀x P(x) ≡ ∃x ¬P(x) -------------------- */
    /**
     * @brief 量词否定
     *
     * 实现量词的否定等价变换：
     *   - ¬∀x P(x) ≡ ∃x ¬P(x)
     *   - ¬∃x P(x) ≡ ∀x ¬P(x)
     * 这是量词的对偶性原理。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC("fol_negate_quantifier", "量词否定：¬∀x P(x) ≡ ∃x ¬P(x)，¬∃x P(x) ≡ ∀x ¬P(x)", inputs, 1,
                       PRESET_TYPE_EXPRESSION,
                       "\\lnot\\forall x \\, P(x) \\equiv \\exists x \\, \\lnot P(x), \\quad \\lnot\\exists x \\, P(x) "
                       "\\equiv \\forall x \\, \\lnot P(x)",
                       "O(n)，n 为公式长度", true, true);
    }

    /* -------------------- 16. 项代入 -------------------- */
    /**
     * @brief 项代入
     *
     * 将一阶逻辑公式中的自由变量用项 t 进行代入，得到新公式 P[x/t]。
     * 需要检查代入是否合法（避免变元捕获）。
     * 项 t 对变量 x 在公式 P 中是可代入的当且仅当代入后不发生变元捕获。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION, PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC("fol_substitution", "项代入：将公式中的自由变量 x 用项 t 代入，得到 P[x/t]", inputs, 2,
                       PRESET_TYPE_EXPRESSION,
                       "P[x/t] \\text{，要求 } t \\text{ 对 } x \\text{ 在 } P \\text{ 中可代入（无变元捕获）}",
                       "O(n)，n 为公式长度", true, false);
    }

    /* -------------------- 17. 自由变量检查 -------------------- */
    /**
     * @brief 自由变量检查
     *
     * 提取一阶逻辑公式中的所有自由变量集合。
     * 自由变量是不受任何量词约束的变量。
     * 自由变量集合记为 FV(P)。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC(
            "fol_free_variable_check", "自由变量检查：提取一阶逻辑公式中的所有自由变量集合 FV(P)", inputs, 1,
            PRESET_TYPE_SET,
            "\\text{FV}(P): \\text{递归定义} \\quad \\text{FV}(\\forall x \\, Q) = \\text{FV}(Q) \\setminus \\{x\\}",
            "O(n)，n 为公式长度", true, false);
    }

    /* -------------------- 18. 约束变量检查 -------------------- */
    /**
     * @brief 约束变量检查
     *
     * 提取一阶逻辑公式中的所有约束变量（绑定变量）集合。
     * 约束变量是被量词 ∀ 或 ∃ 约束的变量。
     * 约束变量集合记为 BV(P)。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC(
            "fol_bound_variable_check", "约束变量检查：提取一阶逻辑公式中的所有约束变量集合 BV(P)", inputs, 1,
            PRESET_TYPE_SET,
            "\\text{BV}(P): \\text{递归定义} \\quad \\text{BV}(\\forall x \\, Q) = \\text{BV}(Q) \\cup \\{x\\}",
            "O(n)，n 为公式长度", true, false);
    }

    /* -------------------- 19. 全称实例化 -------------------- */
    /**
     * @brief 全称实例化（全称消去）
     *
     * 推理规则：从 ∀x P(x) 推出 P(t)，其中 t 是论域中任意项。
     * 这是全称量词消去规则（∀E），是自然演绎系统中的基本规则之一。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION, PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC("fol_universal_instantiation", "全称实例化：从 ∀x P(x) 推出 P(t)，t 为论域中任意项", inputs, 2,
                       PRESET_TYPE_EXPRESSION, "\\forall x \\, P(x) \\vdash P(t), \\quad t \\text{ 为任意项}", "O(1)",
                       true, false);
    }

    /* -------------------- 20. 存在泛化 -------------------- */
    /**
     * @brief 存在泛化（存在引入）
     *
     * 推理规则：从 P(t) 推出 ∃x P(x)，其中 t 是论域中某个具体项。
     * 这是存在量词引入规则（∃I），是自然演绎系统中的基本规则之一。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION, PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC("fol_existential_generalization", "存在泛化：从 P(t) 推出 ∃x P(x)，t 为论域中某个项", inputs, 2,
                       PRESET_TYPE_EXPRESSION, "P(t) \\vdash \\exists x \\, P(x), \\quad t \\text{ 为某个项}", "O(1)",
                       true, false);
    }

    /* -------------------- 21. 前束范式 -------------------- */
    /**
     * @brief 前束范式（PNF）转换
     *
     * 将任意一阶逻辑公式转换为等价的前束范式。
     * 前束范式的形式为 Q1x1 Q2x2 ... Qnxn M，其中 Qi ∈ {∀, ∃}，
     * M 是无量词的母式（矩阵）。
     * 每个一阶公式都有与之逻辑等价的前束范式。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC("fol_prenex_normal_form", "前束范式转换：将一阶逻辑公式转换为等价的前束范式 Q1x1...Qnxn M",
                       inputs, 1, PRESET_TYPE_EXPRESSION,
                       "\\varphi \\equiv Q_1 x_1 \\, Q_2 x_2 \\, \\cdots \\, Q_n x_n \\, M, \\quad M \\text{ 无量词}",
                       "O(n^2)，n 为公式长度", true, false);
    }

    /* -------------------- 22. Skolem范式 -------------------- */
    /**
     * @brief Skolem范式转换
     *
     * 将前束范式转换为Skolem范式。通过引入Skolem函数消除存在量词，
     * 得到形如 ∀x1 ∀x2 ... ∀xm M' 的公式。
     * 原公式可满足当且仅当其Skolem范式可满足。
     * 注意：Skolem范式与原公式不一定逻辑等价，但保持可满足性。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC("fol_skolem_normal_form", "Skolem范式转换：通过Skolem化消除存在量词，保持可满足性", inputs, 1,
                       PRESET_TYPE_EXPRESSION,
                       "\\forall x_1 \\cdots \\forall x_m \\, M', \\quad \\text{引入Skolem函数消除 } \\exists",
                       "O(n^2)，n 为公式长度", true, false);
    }

    /* ============================================================
     * 第三部分：证明论（8个）
     * ============================================================ */

    /* -------------------- 23. 假言推理 -------------------- */
    /**
     * @brief 假言推理（Modus Ponens）
     *
     * 经典推理规则：从 P 和 P → Q 推出 Q。
     * 这是演绎推理中最基本、最常用的规则之一，
     * 也是自然演绎系统中蕴含消去规则（→E）的特例。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION, PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC("proof_modus_ponens", "假言推理（Modus Ponens）：从 P 和 P → Q 推出 Q", inputs, 2,
                       PRESET_TYPE_EXPRESSION, "P, \\; P \\to Q \\vdash Q", "O(1)", true, false);
    }

    /* -------------------- 24. 否定后件 -------------------- */
    /**
     * @brief 否定后件（Modus Tollens）
     *
     * 经典推理规则：从 P → Q 和 ¬Q 推出 ¬P。
     * 是假言推理的对偶形式，通过逆否律 P → Q ≡ ¬Q → ¬P
     * 与假言推理结合可得。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION, PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC("proof_modus_tollens", "否定后件（Modus Tollens）：从 P → Q 和 ¬Q 推出 ¬P", inputs, 2,
                       PRESET_TYPE_EXPRESSION, "P \\to Q, \\; \\lnot Q \\vdash \\lnot P", "O(1)", true, false);
    }

    /* -------------------- 25. 合取引入 -------------------- */
    /**
     * @brief 合取引入（∧I）
     *
     * 自然演绎规则：从 P 和 Q 推出 P ∧ Q。
     * 若两个命题分别成立，则它们的合取也成立。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION, PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC("proof_conjunction_intro", "合取引入（∧I）：从 P 和 Q 推出 P ∧ Q", inputs, 2,
                       PRESET_TYPE_EXPRESSION, "P, \\; Q \\vdash P \\land Q", "O(1)", true, false);
    }

    /* -------------------- 26. 析取消除 -------------------- */
    /**
     * @brief 析取消除（∨E）
     *
     * 自然演绎规则：从 P ∨ Q，以及假设 P 推出 R、假设 Q 推出 R，
     * 可以推出 R。这是分情况证明的基础。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION, PRESET_TYPE_EXPRESSION, PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC("proof_disjunction_elim", "析取消除（∨E）：从 P ∨ Q，以及 P ⊢ R 和 Q ⊢ R，推出 R", inputs, 3,
                       PRESET_TYPE_EXPRESSION, "P \\lor Q, \\; P \\vdash R, \\; Q \\vdash R \\vdash R", "O(1)", true,
                       false);
    }

    /* -------------------- 27. 归谬法 -------------------- */
    /**
     * @brief 归谬法（Reductio ad Absurdum）
     *
     * 证明方法：假设 P 为真，推导出矛盾 ⊥，从而得出 ¬P。
     * 这是间接证明的经典形式，在经典逻辑和直觉主义逻辑中均有效。
     * 直觉主义逻辑中的版本：从 P 推导出矛盾，得出 ¬P。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION, PRESET_TYPE_SEQUENCE};
        REGISTER_LOGIC("proof_reductio_ad_absurdum", "归谬法：假设 P 推导出矛盾 ⊥，从而得出 ¬P", inputs, 2,
                       PRESET_TYPE_EXPRESSION, "P \\vdash \\bot \\Rightarrow \\vdash \\lnot P", "取决于推导过程", true,
                       false);
    }

    /* -------------------- 28. 条件证明 -------------------- */
    /**
     * @brief 条件证明（Conditional Proof）
     *
     * 证明方法：假设 P 为真，在此假设下推导出 Q，从而得出 P → Q。
     * 条件证明是自然演绎系统中蕴含引入规则（→I）的具体应用。
     * 该方法在直觉主义逻辑中同样有效。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION, PRESET_TYPE_SEQUENCE};
        REGISTER_LOGIC("proof_conditional_proof", "条件证明：假设 P 推导出 Q，从而得出 P → Q", inputs, 2,
                       PRESET_TYPE_EXPRESSION, "[P] \\cdots Q \\Rightarrow \\vdash P \\to Q", "取决于推导过程", true,
                       false);
    }

    /* -------------------- 29. 反证法 -------------------- */
    /**
     * @brief 反证法（Proof by Contradiction）
     *
     * 证明方法：假设 ¬P 为真，推导出矛盾 ⊥，从而得出 P。
     * 反证法是经典逻辑特有的方法，在直觉主义逻辑中不成立。
     * 反证法与归谬法的区别：反证法得出 P（正命题），归谬法得出 ¬P（负命题）。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_EXPRESSION, PRESET_TYPE_SEQUENCE};
        REGISTER_LOGIC("proof_by_contradiction", "反证法：假设 ¬P 推导出矛盾 ⊥，从而得出 P（经典逻辑特有）", inputs, 2,
                       PRESET_TYPE_EXPRESSION,
                       "\\lnot P \\vdash \\bot \\Rightarrow \\vdash P \\quad \\text{（经典逻辑）}", "取决于推导过程",
                       false, false);
    }

    /* -------------------- 30. 自然演绎系统 -------------------- */
    /**
     * @brief 自然演绎系统
     *
     * 完整的自然演绎证明系统，包含引入规则和消去规则：
     *   - 引入规则：∧I, ∨I, →I, ∀I, ∃I, ⊥I, ¬I
     *   - 消去规则：∧E, ∨E, →E, ∀E, ∃E, ⊥E
     * 由 Gentzen 于1935年提出，是最接近人类自然推理方式的证明系统。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC("proof_natural_deduction", "自然演绎系统：基于引入规则和消去规则的完整证明系统（Gentzen, 1935）",
                       inputs, 2, PRESET_TYPE_BOOLEAN,
                       "\\Gamma \\vdash_{\\text{ND}} \\varphi \\quad \\text{（引入/消去规则集）}",
                       "O(2^{|\\Gamma|})，取决于策略", true, false);
    }

    /* ============================================================
     * 第四部分：模型论（5个）
     * ============================================================ */

    /* -------------------- 31. 模型满足关系：M ⊨ φ -------------------- */
    /**
     * @brief 模型满足关系
     *
     * 判定结构 M 是否满足公式 φ，记作 M ⊨ φ。
     * 给定一阶语言 L 的结构 M 和赋值 s，
     * 递归判定 M 在赋值 s 下是否满足公式 φ。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_EXPRESSION};
        REGISTER_LOGIC(
            "model_satisfies", "模型满足关系：判定结构 M 是否满足公式 φ（M ⊨ φ）", inputs, 2, PRESET_TYPE_BOOLEAN,
            "M \\models \\varphi[s] \\text{，递归判定结构 } M \\text{ 在赋值 } s \\text{ 下是否满足 } \\varphi",
            "O(|M| \\cdot |\\varphi|)", true, false);
    }

    /* -------------------- 32. 理论一致性判定 -------------------- */
    /**
     * @brief 理论一致性判定
     *
     * 判定一阶理论 T 是否一致（相容）。
     * 理论 T 是一致的当且仅当不存在公式 φ 使得 T ⊢ φ 且 T ⊢ ¬φ。
     * 根据完备性定理，T 一致当且仅当 T 有模型。
     * 一致性判定是不可判定的。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        REGISTER_LOGIC(
            "model_theory_check", "理论一致性判定：判定一阶理论 T 是否一致（无矛盾）", inputs, 1, PRESET_TYPE_BOOLEAN,
            "T \\text{ 一致} \\Leftrightarrow \\nexists \\varphi: T \\vdash \\varphi \\land T \\vdash \\lnot \\varphi",
            "不可判定", false, false);
    }

    /* -------------------- 33. 初等等价 -------------------- */
    /**
     * @brief 初等等价
     *
     * 判定两个结构 M 和 N 是否初等等价，记作 M ≡ N。
     * M ≡ N 当且仅当对一阶语言中的每个句子 φ，M ⊨ φ ⇔ N ⊨ φ。
     * 根据Cantor-Schroeder-Bernstein定理的推广，
     * 对有限结构而言初等等价与同构等价。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_LOGIC("model_elementary_equivalence", "初等等价：判定两个结构 M 和 N 是否满足 M ≡ N", inputs, 2,
                       PRESET_TYPE_BOOLEAN,
                       "M \\equiv N \\Leftrightarrow \\forall \\text{ 句子 } \\varphi: M \\models \\varphi "
                       "\\Leftrightarrow N \\models \\varphi",
                       "不可判定（一般情况）", false, false);
    }

    /* -------------------- 34. 紧致性定理 -------------------- */
    /**
     * @brief 紧致性定理
     *
     * 一阶逻辑的紧致性定理：一阶公式集 Γ 有模型当且仅当
     * Γ 的每个有限子集都有模型。
     * 紧致性定理是模型论的核心定理之一，有广泛的应用，
     * 如非标准分析的存在性证明。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        REGISTER_LOGIC("model_compactness", "紧致性定理应用：Γ 有模型当且仅当 Γ 的每个有限子集都有模型", inputs, 1,
                       PRESET_TYPE_BOOLEAN,
                       "\\Gamma \\text{ 有模型} \\Leftrightarrow \\forall \\Gamma_0 \\subseteq_{\\text{fin}} \\Gamma: "
                       "\\Gamma_0 \\text{ 有模型}",
                       "不可判定", false, false);
    }

    /* -------------------- 35. Lowenheim-Skolem定理 -------------------- */
    /**
     * @brief Lowenheim-Skolem定理
     *
     * Lowenheim-Skolem定理断言：
     *   - 向下：若可数语言中的公式集 Γ 有模型，则 Γ 有可数模型。
     *   - 向上：若 Γ 有无穷模型，则对任意无穷基数 κ ≥ |L|，Γ 有基数为 κ 的模型。
     * 该定理揭示了形式语言表达能力的内在局限性。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_INTEGER};
        REGISTER_LOGIC(
            "model_lowenheim_skolem", "Lowenheim-Skolem定理：可满足的可数理论有可数模型，有无穷模型则有任意大无穷模型",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "\\Gamma \\text{ 可满足} \\Rightarrow \\exists \\text{ 可数模型}; \\quad \\Gamma \\text{ 有无穷模型} "
            "\\Rightarrow \\forall \\kappa \\geq |L|, \\exists \\text{ 基数为 } \\kappa \\text{ 的模型}",
            "不可判定（一般情况）", false, false);
    }

    /* ============================================================
     * 第五部分：递归论（5个）
     * ============================================================ */

    /* -------------------- 36. 可计算函数判定 -------------------- */
    /**
     * @brief 可计算函数判定
     *
     * 判定给定函数是否为可计算函数（递归函数）。
     * 函数 f: N^k → N 是可计算的当且仅当存在图灵机 M
     * 使得对任意输入 x，M 在有限步内停机并输出 f(x)。
     * 该判定问题本身是不可判定的。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        REGISTER_LOGIC(
            "computable_function_check", "可计算函数判定：判定函数是否为可计算函数（存在图灵机计算）", inputs, 1,
            PRESET_TYPE_BOOLEAN,
            "f \\text{ 可计算} \\Leftrightarrow \\exists \\text{ 图灵机 } M: \\forall x, M(x) \\downarrow = f(x)",
            "不可判定", false, false);
    }

    /* -------------------- 37. 图灵机模拟 -------------------- */
    /**
     * @brief 图灵机模拟
     *
     * 给定图灵机的描述和输入，模拟其执行过程。
     * 图灵机 T = (Q, Σ, Γ, δ, q_0, q_acc, q_rej)，
     * 其中 Q 为状态集，Σ 为输入字母表，Γ 为带字母表，
     * δ 为转移函数，q_0 为初始状态。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_STRING, PRESET_TYPE_STRING};
        REGISTER_LOGIC("turing_machine_simulate", "图灵机模拟：给定图灵机描述和输入带，模拟执行过程", inputs, 2,
                       PRESET_TYPE_STRING,
                       "T = (Q, \\Sigma, \\Gamma, \\delta, q_0, q_{\\text{acc}}, q_{\\text{rej}}), \\quad M \\text{ "
                       "在输入 } w \\text{ 上的逐步模拟}",
                       "取决于运行步数", true, false);
    }

    /* -------------------- 38. 停机问题 -------------------- */
    /**
     * @brief 停机问题（Halting Problem）
     *
     * 经典不可判定问题：判定图灵机 M 在输入 w 上是否最终停机。
     * Turing (1936) 证明了不存在算法能对所有 (M, w) 对判定停机性。
     * 证明方法：对角线论证（反证法），假设存在判定器 H，构造导致矛盾的机器 D。
     * 停机问题是计算复杂性理论的基石之一。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_STRING, PRESET_TYPE_STRING};
        REGISTER_LOGIC("halting_problem", "停机问题：判定图灵机 M 在输入 w 上是否停机（Turing, 1936，不可判定）",
                       inputs, 2, PRESET_TYPE_BOOLEAN,
                       "H(M, w) = \\begin{cases} 1 & M \\text{ 在 } w \\text{ 上停机} \\\\ 0 & M \\text{ 在 } w "
                       "\\text{ 上不停机} \\end{cases} \\quad \\text{不可判定}",
                       "不可判定", false, false);
    }

    /* -------------------- 39. 递归可枚举判定 -------------------- */
    /**
     * @brief 递归可枚举判定
     *
     * 判定给定语言（集合）是否为递归可枚举集（r.e. 集）。
     * 语言 L 是递归可枚举的当且仅当存在图灵机 M 使得
     * L = {w : M 在输入 w 上停机并接受}。
     * 递归可枚举集也称为半可判定集。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        REGISTER_LOGIC("recursive_enumerable_check", "递归可枚举判定：判定语言是否为递归可枚举集（半可判定集）", inputs,
                       1, PRESET_TYPE_BOOLEAN,
                       "L \\text{ 是 r.e.} \\Leftrightarrow \\exists M: L = \\{w : M \\text{ 接受 } w\\}", "不可判定",
                       false, false);
    }

    /* -------------------- 40. 可判定性检查 -------------------- */
    /**
     * @brief 可判定性检查
     *
     * 判定给定语言是否为可判定集（递归集）。
     * 语言 L 是可判定的当且仅当存在图灵机 M 使得
     * 对任意输入 w，M 在有限步内停机，且 M 接受 w 当且仅当 w ∈ L。
     * 可判定集既是递归可枚举的，其补集也是递归可枚举的。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        REGISTER_LOGIC("decidability_check", "可判定性检查：判定语言是否为可判定集（递归集）", inputs, 1,
                       PRESET_TYPE_BOOLEAN,
                       "L \\text{ 可判定} \\Leftrightarrow \\exists M: \\forall w, M(w) \\downarrow \\land M \\text{ "
                       "接受 } w \\Leftrightarrow w \\in L",
                       "不可判定", false, false);
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == MATH_LOGIC_PRESET_COUNT;
}

/**
 * @brief 获取数理逻辑预设函数块数量
 *
 * @return int 数理逻辑模块预设函数块总数（40）
 */
int preset_mathematical_logic_count(void) {
    return MATH_LOGIC_PRESET_COUNT;
}
