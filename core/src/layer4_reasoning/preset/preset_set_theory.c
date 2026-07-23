/**
 * @file preset_set_theory.c
 * @brief 集合论预设函数块模块 - 实现
 *
 * 实现理论数学研究项目Lv-00中集合论领域的预设函数块。
 * 采用v2统一宏模式，使用 REGISTER_SET 宏简化注册流程。
 *
 * 模块包含35个预设，分为五大类别：
 *   - 集合基本运算（10个）：并集、交集、差集、补集、对称差、
 *     笛卡尔积、幂集、子集判定、集合相等判定、空集判定
 *   - 关系与函数（8个）：关系复合、逆关系、自反性判定、
 *     对称性判定、传递性判定、等价关系判定、等价类、商集
 *   - 映射理论（8个）：函数复合、逆函数判定、单射判定、
 *     满射判定、双射判定、像、原像、不动点
 *   - 序理论（5个）：偏序关系判定、最小元、最大元、上确界、下确界
 *   - 公理集合论（4个）：ZFC配对公理、ZFC并集公理、
 *     ZFC幂集公理、ZFC替换公理
 *
 * @module SetTheory
 * @category PRESET_CATEGORY_SET_THEORY
 * @version 2.0.0
 * @author Lv-00 开发团队
 */

#include "preset_set_theory.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 集合论模块预设函数块总数 */

/* ==================== 内部辅助函数与宏 ==================== */

/**
 * @brief 注册单个集合论预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有集合论预设都属于 PRESET_CATEGORY_SET_THEORY 类别。
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
static bool register_set_preset(
    const char *name, const char *description,
    const PresetType *input_types, int input_count, PresetType output_type,
    const char *math_def, const char *complexity,
    bool is_constructive, bool is_reversible)
{
    return preset_blocks_register_simple(
        name, description, PRESET_CATEGORY_SET_THEORY,
        input_types, input_count, output_type,
        math_def, complexity, is_constructive, is_reversible);
}

/**
 * @brief 简化预设注册的宏
 *
 * 减少重复代码，提高可维护性。
 * 注册成功时递增 success_count，失败时输出错误日志。
 */
#define REGISTER_SET(name, desc, inputs, in_count, output, math, comp, cons, rev) \
    do { \
        if (register_set_preset( \
                (name), (desc), (inputs), (in_count), (output), \
                (math), (comp), (cons), (rev))) { \
            success_count++; \
        } else { \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */ \
        } \
    } while (0)

/* ==================== 模块注册实现 ==================== */

bool preset_set_theory_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：集合基本运算（10个）
     * ============================================================ */

    /* -------------------- 1. 并集：A ∪ B -------------------- */
    /**
     * @brief 并集运算
     *
     * 计算两个集合的并集。A ∪ B 包含所有属于 A 或属于 B 的元素。
     * 并集运算满足交换律、结合律，以空集为单位元。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "set_union",
            "并集：A ∪ B = {x : x ∈ A ∨ x ∈ B}",
            inputs, 2, PRESET_TYPE_SET,
            "A \\cup B = \\{x : x \\in A \\lor x \\in B\\}",
            "O(|A| + |B|)", true, false);
    }

    /* -------------------- 2. 交集：A ∩ B -------------------- */
    /**
     * @brief 交集运算
     *
     * 计算两个集合的交集。A ∩ B 包含所有同时属于 A 和 B 的元素。
     * 交集运算满足交换律、结合律，以全集为单位元。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "set_intersection",
            "交集：A ∩ B = {x : x ∈ A ∧ x ∈ B}",
            inputs, 2, PRESET_TYPE_SET,
            "A \\cap B = \\{x : x \\in A \\land x \\in B\\}",
            "O(min(|A|, |B|))", true, false);
    }

    /* -------------------- 3. 差集：A \ B -------------------- */
    /**
     * @brief 差集运算
     *
     * 计算两个集合的差集（相对补集）。A \ B 包含所有属于 A 但不属于 B 的元素。
     * 差集运算不满足交换律，A \ B ≠ B \ A（一般情况）。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "set_difference",
            "差集：A \\ B = {x : x ∈ A ∧ x ∉ B}",
            inputs, 2, PRESET_TYPE_SET,
            "A \\setminus B = \\{x : x \\in A \\land x \\notin B\\}",
            "O(|A|)", true, false);
    }

    /* -------------------- 4. 补集：A^c -------------------- */
    /**
     * @brief 补集运算
     *
     * 计算集合 A 相对于全集 U 的补集。A^c = U \ A，
     * 包含全集中不属于 A 的所有元素。
     * 补集运算是自逆的：(A^c)^c = A。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "set_complement",
            "补集：A^c = U \\ A = {x ∈ U : x ∉ A}，相对于全集 U",
            inputs, 2, PRESET_TYPE_SET,
            "A^c = U \\setminus A = \\{x \\in U : x \\notin A\\}",
            "O(|U|)", true, true);
    }

    /* -------------------- 5. 对称差：A △ B -------------------- */
    /**
     * @brief 对称差运算
     *
     * 计算两个集合的对称差。A △ B = (A \ B) ∪ (B \ A)，
     * 包含恰好属于 A 或 B 中一个集合的元素。
     * 对称差满足交换律和结合律，以空集为单位元，每个集合是自身的逆元。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "set_symmetric_difference",
            "对称差：A △ B = (A \\ B) ∪ (B \\ A)",
            inputs, 2, PRESET_TYPE_SET,
            "A \\triangle B = (A \\setminus B) \\cup (B \\setminus A)",
            "O(|A| + |B|)", true, true);
    }

    /* -------------------- 6. 笛卡尔积：A × B -------------------- */
    /**
     * @brief 笛卡尔积运算
     *
     * 计算两个集合的笛卡尔积。A × B = {(a, b) : a ∈ A, b ∈ B}，
     * 是所有有序对 (a, b) 的集合，其中 a 取自 A，b 取自 B。
     * 笛卡尔积不满足交换律：A × B ≠ B × A（一般情况）。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "set_cartesian_product",
            "笛卡尔积：A × B = {(a, b) : a ∈ A, b ∈ B}",
            inputs, 2, PRESET_TYPE_SET,
            "A \\times B = \\{(a, b) : a \\in A, b \\in B\\}",
            "O(|A| \\cdot |B|)", true, false);
    }

    /* -------------------- 7. 幂集：P(A) -------------------- */
    /**
     * @brief 幂集运算
     *
     * 计算集合 A 的幂集 P(A) = {S : S ⊆ A}，
     * 即 A 的所有子集构成的集合。
     * 若 |A| = n，则 |P(A)| = 2^n。
     * 幂集上的包含关系构成布尔代数。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        REGISTER_SET(
            "set_power_set",
            "幂集：P(A) = {S : S ⊆ A}，A 的所有子集构成的集合",
            inputs, 1, PRESET_TYPE_SET,
            "\\mathcal{P}(A) = \\{S : S \\subseteq A\\}, \\quad |\\mathcal{P}(A)| = 2^{|A|}",
            "O(2^{|A|})", true, false);
    }

    /* -------------------- 8. 子集判定：A ⊆ B -------------------- */
    /**
     * @brief 子集判定
     *
     * 判定集合 A 是否为集合 B 的子集（A ⊆ B）。
     * A ⊆ B 当且仅当 A 中的每个元素都属于 B。
     * 子集关系是自反的、反对称的和传递的，构成偏序关系。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "set_subset_check",
            "子集判定：A ⊆ B，判定 A 中每个元素是否都属于 B",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "A \\subseteq B \\Leftrightarrow \\forall x \\in A, x \\in B",
            "O(|A|)", true, false);
    }

    /* -------------------- 9. 集合相等判定 -------------------- */
    /**
     * @brief 集合相等判定
     *
     * 判定两个集合是否相等（A = B）。
     * A = B 当且仅当 A ⊆ B 且 B ⊆ A（外延公理）。
     * 集合相等由外延性公理定义：两个集合具有相同的元素当且仅当它们相等。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "set_equality_check",
            "集合相等判定：A = B 当且仅当 A ⊆ B 且 B ⊆ A（外延公理）",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "A = B \\Leftrightarrow A \\subseteq B \\land B \\subseteq A \\quad \\text{（外延公理）}",
            "O(|A| + |B|)", true, true);
    }

    /* -------------------- 10. 空集判定 -------------------- */
    /**
     * @brief 空集判定
     *
     * 判定集合是否为空集（A = ∅）。
     * 空集是唯一不含任何元素的集合，是任何集合的子集。
     * 空集在集合运算中充当并集的单位元和交集的零元。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        REGISTER_SET(
            "set_empty_check",
            "空集判定：A = ∅，判定集合是否不含任何元素",
            inputs, 1, PRESET_TYPE_BOOLEAN,
            "A = \\emptyset \\Leftrightarrow \\lnot \\exists x: x \\in A",
            "O(1)", true, false);
    }

    /* ============================================================
     * 第二部分：关系与函数（8个）
     * ============================================================ */

    /* -------------------- 11. 关系复合 -------------------- */
    /**
     * @brief 关系复合
     *
     * 计算两个二元关系的复合。R ∘ S = {(a, c) : ∃b, (a,b)∈S ∧ (b,c)∈R}。
     * 注意复合的顺序：先应用 S，再应用 R。
     * 关系复合满足结合律：(R ∘ S) ∘ T = R ∘ (S ∘ T)。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "relation_compose",
            "关系复合：R ∘ S = {(a, c) : ∃b, (a,b)∈S ∧ (b,c)∈R}",
            inputs, 2, PRESET_TYPE_SET,
            "R \\circ S = \\{(a, c) : \\exists b, (a,b) \\in S \\land (b,c) \\in R\\}",
            "O(|S| \\cdot |R|)", true, false);
    }

    /* -------------------- 12. 逆关系 -------------------- */
    /**
     * @brief 逆关系
     *
     * 计算关系 R 的逆关系 R^{-1} = {(b, a) : (a, b) ∈ R}。
     * 逆关系运算是对合的：(R^{-1})^{-1} = R。
     * 逆关系满足：(R ∘ S)^{-1} = S^{-1} ∘ R^{-1}。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        REGISTER_SET(
            "relation_inverse",
            "逆关系：R^{-1} = {(b, a) : (a, b) ∈ R}",
            inputs, 1, PRESET_TYPE_SET,
            "R^{-1} = \\{(b, a) : (a, b) \\in R\\}",
            "O(|R|)", true, true);
    }

    /* -------------------- 13. 自反性判定 -------------------- */
    /**
     * @brief 自反性判定
     *
     * 判定集合 A 上的二元关系 R 是否为自反关系。
     * R 是自反的当且仅当对所有 a ∈ A，(a, a) ∈ R。
     * 自反性要求 A 中每个元素都与自身相关。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "relation_reflexive_check",
            "自反性判定：R 是自反的当且仅当 ∀a ∈ A, (a, a) ∈ R",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "R \\text{ 自反} \\Leftrightarrow \\forall a \\in A, (a, a) \\in R",
            "O(|A|)", true, false);
    }

    /* -------------------- 14. 对称性判定 -------------------- */
    /**
     * @brief 对称性判定
     *
     * 判定集合 A 上的二元关系 R 是否为对称关系。
     * R 是对称的当且仅当对所有 a, b ∈ A，(a,b) ∈ R 蕴涵 (b,a) ∈ R。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "relation_symmetric_check",
            "对称性判定：R 是对称的当且仅当 (a,b) ∈ R ⇒ (b,a) ∈ R",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "R \\text{ 对称} \\Leftrightarrow \\forall a, b \\in A, (a,b) \\in R \\Rightarrow (b,a) \\in R",
            "O(|R|)", true, false);
    }

    /* -------------------- 15. 传递性判定 -------------------- */
    /**
     * @brief 传递性判定
     *
     * 判定集合 A 上的二元关系 R 是否为传递关系。
     * R 是传递的当且仅当对所有 a, b, c ∈ A，
     * (a,b) ∈ R 且 (b,c) ∈ R 蕴涵 (a,c) ∈ R。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "relation_transitive_check",
            "传递性判定：R 是传递的当且仅当 (a,b)∈R ∧ (b,c)∈R ⇒ (a,c)∈R",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "R \\text{ 传递} \\Leftrightarrow \\forall a, b, c: (a,b) \\in R \\land (b,c) \\in R \\Rightarrow (a,c) \\in R",
            "O(|R|^2)", true, false);
    }

    /* -------------------- 16. 等价关系判定 -------------------- */
    /**
     * @brief 等价关系判定
     *
     * 判定关系 R 是否为等价关系。
     * R 是等价关系当且仅当 R 同时满足自反性、对称性和传递性。
     * 等价关系将集合划分为不相交的等价类。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "relation_equivalence_check",
            "等价关系判定：R 是等价关系当且仅当自反 ∧ 对称 ∧ 传递",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "R \\text{ 是等价关系} \\Leftrightarrow \\text{自反}(R) \\land \\text{对称}(R) \\land \\text{传递}(R)",
            "O(|A|^2 \\cdot |R|)", true, false);
    }

    /* -------------------- 17. 等价类 -------------------- */
    /**
     * @brief 等价类
     *
     * 计算元素 a 关于等价关系 R 的等价类 [a]_R。
     * [a]_R = {x ∈ A : (a, x) ∈ R}。
     * 等价类具有性质：若 b ∈ [a]_R，则 [a]_R = [b]_R。
     * 不同等价类两两不交，其并集等于 A。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_ANY};
        REGISTER_SET(
            "equivalence_class",
            "等价类：[a]_R = {x ∈ A : (a, x) ∈ R}",
            inputs, 3, PRESET_TYPE_SET,
            "[a]_R = \\{x \\in A : (a, x) \\in R\\}",
            "O(|A|)", true, false);
    }

    /* -------------------- 18. 商集 -------------------- */
    /**
     * @brief 商集
     *
     * 计算集合 A 关于等价关系 R 的商集 A/R。
     * A/R = {[a]_R : a ∈ A}，即所有等价类构成的集合。
     * 商集是 A 的一个划分，每个等价类是划分的一个块。
     * 自然映射 π: A → A/R, a ↦ [a]_R 是满射。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "quotient_set",
            "商集：A/R = {[a]_R : a ∈ A}，所有等价类构成的集合",
            inputs, 2, PRESET_TYPE_SET,
            "A/R = \\{[a]_R : a \\in A\\}, \\quad \\pi: A \\to A/R, \\; a \\mapsto [a]_R",
            "O(|A|)", true, false);
    }

    /* ============================================================
     * 第三部分：映射理论（8个）
     * ============================================================ */

    /* -------------------- 19. 函数复合：g ∘ f -------------------- */
    /**
     * @brief 函数复合
     *
     * 计算两个函数的复合 g ∘ f。
     * (g ∘ f)(x) = g(f(x))，要求 f 的陪域与 g 的定义域一致。
     * 函数复合满足结合律：(h ∘ g) ∘ f = h ∘ (g ∘ f)，
     * 但一般不满足交换律。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        REGISTER_SET(
            "function_compose",
            "函数复合：g ∘ f，(g ∘ f)(x) = g(f(x))",
            inputs, 2, PRESET_TYPE_FUNCTION,
            "(g \\circ f)(x) = g(f(x)), \\quad \\text{要求 } \\text{cod}(f) = \\text{dom}(g)",
            "O(n)，n 为定义域大小", true, false);
    }

    /* -------------------- 20. 逆函数判定 -------------------- */
    /**
     * @brief 逆函数判定
     *
     * 判定函数 f: A → B 是否存在逆函数 f^{-1}: B → A。
     * f 存在逆函数当且仅当 f 是双射（既单又满）。
     * 逆函数满足 f ∘ f^{-1} = id_B 且 f^{-1} ∘ f = id_A。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "function_inverse_check",
            "逆函数判定：f 存在逆函数当且仅当 f 是双射",
            inputs, 3, PRESET_TYPE_BOOLEAN,
            "f^{-1} \\text{ 存在} \\Leftrightarrow f \\text{ 是双射}, \\quad f \\circ f^{-1} = \\text{id}_B",
            "O(|A| + |B|)", true, false);
    }

    /* -------------------- 21. 单射判定 -------------------- */
    /**
     * @brief 单射判定
     *
     * 判定函数 f: A → B 是否为单射（一对一映射，injective）。
     * f 是单射当且仅当对所有 a1, a2 ∈ A，
     * f(a1) = f(a2) 蕴涵 a1 = a2。
     * 等价条件：不同元素映射到不同像。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "function_injective_check",
            "单射判定：f 是单射当且仅当 f(a1) = f(a2) ⇒ a1 = a2",
            inputs, 3, PRESET_TYPE_BOOLEAN,
            "f \\text{ 单射} \\Leftrightarrow \\forall a_1, a_2 \\in A, f(a_1) = f(a_2) \\Rightarrow a_1 = a_2",
            "O(|A| \\log |A|)", true, false);
    }

    /* -------------------- 22. 满射判定 -------------------- */
    /**
     * @brief 满射判定
     *
     * 判定函数 f: A → B 是否为满射（到上映射，surjective）。
     * f 是满射当且仅当对所有 b ∈ B，存在 a ∈ A 使得 f(a) = b。
     * 等价条件：f 的像等于陪域 B。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "function_surjective_check",
            "满射判定：f 是满射当且仅当 ∀b ∈ B, ∃a ∈ A, f(a) = b",
            inputs, 3, PRESET_TYPE_BOOLEAN,
            "f \\text{ 满射} \\Leftrightarrow \\forall b \\in B, \\exists a \\in A, f(a) = b \\Leftrightarrow f(A) = B",
            "O(|A| + |B|)", true, false);
    }

    /* -------------------- 23. 双射判定 -------------------- */
    /**
     * @brief 双射判定
     *
     * 判定函数 f: A → B 是否为双射（一一对应，bijective）。
     * f 是双射当且仅当 f 既是单射又是满射。
     * 双射是集合之间建立一一对应关系的函数，
     * 是定义集合基数相等的基础。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "function_bijective_check",
            "双射判定：f 是双射当且仅当 f 既是单射又是满射",
            inputs, 3, PRESET_TYPE_BOOLEAN,
            "f \\text{ 双射} \\Leftrightarrow f \\text{ 单射} \\land f \\text{ 满射}",
            "O(|A| \\log |A| + |B|)", true, false);
    }

    /* -------------------- 24. 像：f(A) -------------------- */
    /**
     * @brief 函数的像
     *
     * 计算子集 S ⊆ A 在映射 f 下的像 f(S) = {f(x) : x ∈ S}。
     * 像满足单调性：S1 ⊆ S2 蕴涵 f(S1) ⊆ f(S2)。
     * 对并集保持：f(S1 ∪ S2) = f(S1) ∪ f(S2)。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "function_image",
            "像：f(S) = {f(x) : x ∈ S}，S 为定义域的子集",
            inputs, 3, PRESET_TYPE_SET,
            "f(S) = \\{f(x) : x \\in S \\subseteq A\\}",
            "O(|S|)", true, false);
    }

    /* -------------------- 25. 原像：f^(-1)(B) -------------------- */
    /**
     * @brief 函数的原像（逆像）
     *
     * 计算子集 T ⊆ B 在映射 f 下的原像 f^{-1}(T) = {x ∈ A : f(x) ∈ T}。
     * 原像保持所有集合运算：
     *   f^{-1}(T1 ∪ T2) = f^{-1}(T1) ∪ f^{-1}(T2)
     *   f^{-1}(T1 ∩ T2) = f^{-1}(T1) ∩ f^{-1}(T2)
     *   f^{-1}(T^c) = (f^{-1}(T))^c
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "function_preimage",
            "原像：f^{-1}(T) = {x ∈ A : f(x) ∈ T}，T 为陪域的子集",
            inputs, 3, PRESET_TYPE_SET,
            "f^{-1}(T) = \\{x \\in A : f(x) \\in T \\subseteq B\\}",
            "O(|A|)", true, false);
    }

    /* -------------------- 26. 不动点 -------------------- */
    /**
     * @brief 不动点
     *
     * 计算函数 f: A → A 的不动点集合。
     * 不动点满足 f(x) = x。
     * 根据Brouwer不动点定理，从闭球到自身的连续映射至少有一个不动点。
     * 根据Banach不动点定理，完备度量空间上的压缩映射有唯一不动点。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SET};
        REGISTER_SET(
            "function_fixpoint",
            "不动点：计算函数 f 的不动点集合 {x ∈ A : f(x) = x}",
            inputs, 2, PRESET_TYPE_SET,
            "\\text{Fix}(f) = \\{x \\in A : f(x) = x\\}",
            "O(|A|)", true, false);
    }

    /* ============================================================
     * 第四部分：序理论（5个）
     * ============================================================ */

    /* -------------------- 27. 偏序关系判定 -------------------- */
    /**
     * @brief 偏序关系判定
     *
     * 判定集合 A 上的二元关系 ≤ 是否构成偏序关系。
     * 偏序关系需满足三个性质：
     *   - 自反性：∀a ∈ A, a ≤ a
     *   - 反对称性：a ≤ b ∧ b ≤ a ⇒ a = b
     *   - 传递性：a ≤ b ∧ b ≤ c ⇒ a ≤ c
     * 常见偏序例子：集合包含关系 ⊆、整数上的 ≤、整除关系 |。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "order_check",
            "偏序关系判定：≤ 是偏序当且仅当自反 ∧ 反对称 ∧ 传递",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "\\le \\text{ 是偏序} \\Leftrightarrow \\text{自反} \\land \\text{反对称} \\land \\text{传递}",
            "O(|A|^2 \\cdot |R|)", true, false);
    }

    /* -------------------- 28. 最小元 -------------------- */
    /**
     * @brief 最小元
     *
     * 在偏序集 (A, ≤) 的子集 S 中查找最小元。
     * S 的最小元（如果存在）是唯一的元素 m ∈ S，
     * 使得对所有 x ∈ S，m ≤ x。
     * 最小元如果存在则唯一，但未必存在。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "order_min",
            "最小元：在偏序集子集 S 中查找最小元 m（∀x ∈ S, m ≤ x）",
            inputs, 3, PRESET_TYPE_ANY,
            "\\min S = m \\Leftrightarrow m \\in S \\land \\forall x \\in S, m \\le x",
            "O(|S|^2)", true, false);
    }

    /* -------------------- 29. 最大元 -------------------- */
    /**
     * @brief 最大元
     *
     * 在偏序集 (A, ≤) 的子集 S 中查找最大元。
     * S 的最大元（如果存在）是唯一的元素 M ∈ S，
     * 使得对所有 x ∈ S，x ≤ M。
     * 最大元如果存在则唯一，但未必存在。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "order_max",
            "最大元：在偏序集子集 S 中查找最大元 M（∀x ∈ S, x ≤ M）",
            inputs, 3, PRESET_TYPE_ANY,
            "\\max S = M \\Leftrightarrow M \\in S \\land \\forall x \\in S, x \\le M",
            "O(|S|^2)", true, false);
    }

    /* -------------------- 30. 上确界 -------------------- */
    /**
     * @brief 上确界（最小上界）
     *
     * 在偏序集 (A, ≤) 中计算子集 S 的上确界 sup(S)。
     * 上确界是 S 的最小上界：sup(S) 是上界，且不大于任何其他上界。
     * 上确界如果存在则唯一。
     * 在格中，任意两个元素的上确界记为 a ∨ b（join）。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "order_supremum",
            "上确界（最小上界）：sup(S)，S 的最小上界",
            inputs, 3, PRESET_TYPE_ANY,
            "\\sup S = \\min\\{u \\in A : \\forall x \\in S, x \\le u\\}",
            "O(|S|^2)", true, false);
    }

    /* -------------------- 31. 下确界 -------------------- */
    /**
     * @brief 下确界（最大下界）
     *
     * 在偏序集 (A, ≤) 中计算子集 S 的下确界 inf(S)。
     * 下确界是 S 的最大下界：inf(S) 是下界，且不小于任何其他下界。
     * 下确界如果存在则唯一。
     * 在格中，任意两个元素的下确界记为 a ∧ b（meet）。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "order_infimum",
            "下确界（最大下界）：inf(S)，S 的最大下界",
            inputs, 3, PRESET_TYPE_ANY,
            "\\inf S = \\max\\{l \\in A : \\forall x \\in S, l \\le x\\}",
            "O(|S|^2)", true, false);
    }

    /* ============================================================
     * 第五部分：公理集合论（4个）
     * ============================================================ */

    /* -------------------- 32. ZFC配对公理 -------------------- */
    /**
     * @brief ZFC配对公理
     *
     * ZFC集合论的配对公理：对任意集合 a 和 b，
     * 存在集合 {a, b} 恰好包含 a 和 b 作为元素。
     * 配对公理保证了无序对的存在性。
     * 由配对公理可推导单元素集 {a} = {a, a} 的存在性。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_SET(
            "zfc_pairing",
            "ZFC配对公理：对任意集合 a, b，存在集合 {a, b}",
            inputs, 2, PRESET_TYPE_SET,
            "\\forall a, \\forall b, \\exists c, \\forall x: x \\in c \\Leftrightarrow x = a \\lor x = b",
            "O(1)", true, false);
    }

    /* -------------------- 33. ZFC并集公理 -------------------- */
    /**
     * @brief ZFC并集公理
     *
     * ZFC集合论的并集公理：对任意集合 A（集合族），
     * 存在集合 ∪A = {x : ∃Y ∈ A, x ∈ Y}。
     * ∪A 是 A 中所有元素（它们本身是集合）的并集。
     * 注意与二元并集 A ∪ B 的区别：此处是对集合族取并。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        REGISTER_SET(
            "zfc_union",
            "ZFC并集公理：对任意集合族 A，存在集合 ∪A = {x : ∃Y ∈ A, x ∈ Y}",
            inputs, 1, PRESET_TYPE_SET,
            "\\forall A, \\exists B, \\forall x: x \\in B \\Leftrightarrow \\exists Y \\in A, x \\in Y",
            "O(|A| \\cdot \\max|Y|)", true, false);
    }

    /* -------------------- 34. ZFC幂集公理 -------------------- */
    /**
     * @brief ZFC幂集公理
     *
     * ZFC集合论的幂集公理：对任意集合 A，
     * 存在集合 P(A) = {S : S ⊆ A}。
     * 幂集公理保证了集合的所有子集构成一个集合。
     * 由Cantor定理，|P(A)| > |A|（不存在最大基数）。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        REGISTER_SET(
            "zfc_power_set",
            "ZFC幂集公理：对任意集合 A，存在集合 P(A) = {S : S ⊆ A}",
            inputs, 1, PRESET_TYPE_SET,
            "\\forall A, \\exists B, \\forall S: S \\in B \\Leftrightarrow S \\subseteq A",
            "O(2^{|A|})", true, false);
    }

    /* -------------------- 35. ZFC替换公理 -------------------- */
    /**
     * @brief ZFC替换公理模式
     *
     * ZFC集合论的替换公理模式（Fraenkel, 1922）：
     * 若 φ(x, y) 是一个"函数性质"的公式（对每个 x 恰好存在一个 y 使得 φ(x, y)），
     * 则对任意集合 A，存在集合 B = {y : ∃x ∈ A, φ(x, y)}。
     * 替换公理模式实际上是无穷多条公理的统称（每种公式 φ 对应一条）。
     * 替换公理蕴含分离公理模式，是ZFC中最强的公理之一。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_EXPRESSION};
        REGISTER_SET(
            "zfc_replacement",
            "ZFC替换公理模式：若 φ(x,y) 是函数性质，则 {y : ∃x ∈ A, φ(x,y)} 是集合",
            inputs, 2, PRESET_TYPE_SET,
            "(\\forall x \\exists! y \\, \\varphi(x,y)) \\Rightarrow \\forall A \\exists B \\forall y (y \\in B \\Leftrightarrow \\exists x \\in A \\, \\varphi(x,y))",
            "O(|A|)", false, false);
    }

    /* 返回是否所有预设都注册成功 */
    /* lv_log_info("集合论预设注册完成，共 %d 个预设", success_count) */
    return success_count == SET_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取集合论预设函数块的类别
 * @return 预设类别枚举值
 */
PresetCategory preset_set_theory_category(void)
{
    return PRESET_CATEGORY_LOGIC;
}

/**
 * @brief 获取集合论预设函数块的名称列表
 * @param out_names 输出：名称数组（调用者需先释放每个元素再释放数组本身）
 * @param out_count 输出：名称数量
 * @return true 成功，false 内存分配失败
 */
bool preset_set_theory_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;
    *out_count = SET_THEORY_PRESET_COUNT;
    char **names = (char **)lv_malloc(SET_THEORY_PRESET_COUNT * sizeof(char *));
    if (!names) return false;

    const char *preset_names[] = {
        /* 集合基本运算 */
        PRESET_SET_UNION,
        PRESET_SET_INTERSECTION,
        PRESET_SET_DIFFERENCE,
        PRESET_SET_COMPLEMENT,
        PRESET_SET_SYMMETRIC_DIFFERENCE,
        PRESET_SET_CARTESIAN_PRODUCT,
        PRESET_SET_POWER_SET,
        PRESET_SET_SUBSET_CHECK,
        PRESET_SET_EQUALITY_CHECK,
        PRESET_SET_EMPTY_CHECK,
        /* 关系与函数 */
        PRESET_RELATION_COMPOSE,
        PRESET_RELATION_INVERSE,
        PRESET_RELATION_REFLEXIVE_CHECK,
        PRESET_RELATION_SYMMETRIC_CHECK,
        PRESET_RELATION_TRANSITIVE_CHECK,
        PRESET_RELATION_EQUIVALENCE_CHECK,
        PRESET_EQUIVALENCE_CLASS,
        PRESET_QUOTIENT_SET,
        /* 映射理论 */
        PRESET_FUNCTION_COMPOSE,
        PRESET_FUNCTION_INVERSE_CHECK,
        PRESET_FUNCTION_INJECTIVE_CHECK,
        PRESET_FUNCTION_SURJECTIVE_CHECK,
        PRESET_FUNCTION_BIJECTIVE_CHECK,
        PRESET_FUNCTION_IMAGE,
        PRESET_FUNCTION_PREIMAGE,
        PRESET_FUNCTION_FIXPOINT,
        /* 序理论 */
        PRESET_ORDER_CHECK,
        PRESET_ORDER_MIN,
        PRESET_ORDER_MAX,
        PRESET_ORDER_SUPREMUM,
        PRESET_ORDER_INFIMUM,
        /* 公理集合论 */
        PRESET_ZFC_PAIRING,
        PRESET_ZFC_UNION,
        PRESET_ZFC_POWER_SET,
        PRESET_ZFC_REPLACEMENT,
    };

    for (int i = 0; i < SET_THEORY_PRESET_COUNT; i++) {
        size_t len = strlen(preset_names[i]) + 1;
        names[i] = (char *)lv_malloc(len);
        if (!names[i]) {
            for (int j = 0; j < i; j++) { void *tmp = names[j]; lv_free(&tmp); }
            { void *tmp = names; lv_free(&tmp); }
            return false;
        }
        memcpy(names[i], preset_names[i], len);
    }
    *out_names = names;
    return true;
}
