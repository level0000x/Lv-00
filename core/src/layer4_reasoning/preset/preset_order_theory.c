/**
 * @file preset_order_theory.c
 * @brief 序理论预设函数块模块 - 实现
 *
 * 实现理论数学研究项目Lv-00中序理论领域的预设函数块。
 * 采用v2统一宏模式，使用 REGISTER_ORDER 宏简化注册流程。
 *
 * 模块包含8个预设，分为四大类别：
 *   - 偏序与格论（3个）：偏序关系构造、格的上确界（join）、格的下确界（meet）
 *   - 分解与选择公理（2个）：链分解（Dilworth定理）、Zorn引理应用
 *   - 不动点理论（1个）：Tarski/Knaster不动点定理
 *   - Galois连接与完备化（2个）：Galois连接、完备化
 *
 * @module OrderTheory
 * @category PRESET_CATEGORY_LOGIC
 * @version 1.0.0
 * @author Lv-00 开发团队
 */

#include "preset_order_theory.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 序理论模块预设函数块总数 */
#define ORDER_THEORY_PRESET_COUNT 8

/* ==================== 内部辅助函数与宏 ==================== */

/**
 * @brief 注册单个序理论预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有序理论预设都属于 PRESET_CATEGORY_LOGIC 类别。
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
static bool register_order_preset(const char *name, const char *description, const PresetType *input_types,
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
#define REGISTER_ORDER(name, desc, inputs, in_count, output, math, comp, cons, rev)                                 \
    do {                                                                                                            \
        if (register_order_preset((name), (desc), (inputs), (in_count), (output), (math), (comp), (cons), (rev))) { \
            success_count++;                                                                                        \
        } else {                                                                                                    \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */                                                     \
        }                                                                                                           \
    } while (0)

/* ==================== 模块注册实现 ==================== */

bool preset_order_theory_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：偏序与格论（3个）
     * ============================================================ */

    /* -------------------- 1. 偏序关系构造 -------------------- */
    /**
     * @brief 偏序关系构造
     *
     * 给定集合 P 和二元关系 R，验证 R 是否构成偏序关系，
     * 并构造偏序集 (P, ≤)。
     * 偏序关系需满足自反性、反对称性和传递性。
     * 若 R 不满足偏序条件，返回失败。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_ORDER("partial_order_relation",
                       "偏序关系构造：验证二元关系 R 是否构成偏序（自反 ∧ 反对称 ∧ 传递），构造偏序集 (P, ≤)", inputs,
                       2, PRESET_TYPE_SET,
                       "\\le \\text{ 是偏序} \\Leftrightarrow "
                       "\\text{自反}(\\forall a, a \\le a) \\land "
                       "\\text{反对称}(a \\le b \\land b \\le a \\Rightarrow a = b) \\land "
                       "\\text{传递}(a \\le b \\land b \\le c \\Rightarrow a \\le c)",
                       "O(|P|^2 \\cdot |R|)", true, false);
    }

    /* -------------------- 2. 格的上确界（join/并） -------------------- */
    /**
     * @brief 格的上确界
     *
     * 在格 (L, ≤) 中计算两个元素 a, b 的上确界（join/并）a ∨ b。
     * a ∨ b 是 a 和 b 的最小上界，满足：
     *   - a ≤ a ∨ b 且 b ≤ a ∨ b
     *   - 对任意 c，若 a ≤ c 且 b ≤ c，则 a ∨ b ≤ c
     * join 运算满足交换律、结合律、幂等律和吸收律。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_ANY, PRESET_TYPE_ANY};
        REGISTER_ORDER("lattice_join", "格的上确界（join/并）：a ∨ b，a 和 b 的最小上界", inputs, 3, PRESET_TYPE_ANY,
                       "a \\vee b = \\min\\{c \\in L : a \\le c \\land b \\le c\\}, "
                       "\\quad a \\vee a = a, \\; a \\vee b = b \\vee a",
                       "O(|L|^2)", true, false);
    }

    /* -------------------- 3. 格的下确界（meet/交） -------------------- */
    /**
     * @brief 格的下确界
     *
     * 在格 (L, ≤) 中计算两个元素 a, b 的下确界（meet/交）a ∧ b。
     * a ∧ b 是 a 和 b 的最大下界，满足：
     *   - a ∧ b ≤ a 且 a ∧ b ≤ b
     *   - 对任意 c，若 c ≤ a 且 c ≤ b，则 c ≤ a ∧ b
     * meet 运算满足交换律、结合律、幂等律和吸收律。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_ANY, PRESET_TYPE_ANY};
        REGISTER_ORDER("lattice_meet", "格的下确界（meet/交）：a ∧ b，a 和 b 的最大下界", inputs, 3, PRESET_TYPE_ANY,
                       "a \\wedge b = \\max\\{c \\in L : c \\le a \\land c \\le b\\}, "
                       "\\quad a \\wedge a = a, \\; a \\wedge b = b \\wedge a",
                       "O(|L|^2)", true, false);
    }

    /* ============================================================
     * 第二部分：分解与选择公理（2个）
     * ============================================================ */

    /* -------------------- 4. 链分解（Dilworth定理） -------------------- */
    /**
     * @brief 链分解
     *
     * 根据Dilworth定理，将有限偏序集 (P, ≤) 分解为不相交链的最小划分。
     * Dilworth定理：在有限偏序集中，最小链分解的链数等于最大反链的大小。
     * 即 width(P) = 最小链划分数。
     * 链是完全有序的子集，反链是两两不可比的子集。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_ORDER("chain_decomposition",
                       "链分解（Dilworth定理）：将有限偏序集分解为不相交链的最小划分，链数 = 最大反链大小", inputs, 2,
                       PRESET_TYPE_SET,
                       "\\text{Dilworth: } \\min\\{\\text{链划分数}\\} = "
                       "\\max\\{\\text{反链大小}\\} = \\text{width}(P)",
                       "O(|P|^3)", false, false);
    }

    /* -------------------- 5. Zorn引理应用 -------------------- */
    /**
     * @brief Zorn引理应用
     *
     * 在偏序集 (P, ≤) 中应用Zorn引理证明极大元的存在性。
     * Zorn引理：若 (P, ≤) 是非空偏序集，且其中每条链都有上界，
     * 则 P 中至少存在一个极大元。
     * Zorn引理与选择公理、良序定理三者等价。
     * 注意：Zorn引理的证明是非构造性的。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_ORDER("zorn_lemma_application",
                       "Zorn引理应用：若非空偏序集的每条链都有上界，则存在极大元（与选择公理等价）", inputs, 2,
                       PRESET_TYPE_BOOLEAN,
                       "(\\forall C \\subseteq P, C \\text{ 是链} \\Rightarrow "
                       "\\exists u \\in P, \\forall c \\in C, c \\le u) "
                       "\\Rightarrow \\exists m \\in P, \\forall x \\in P, m \\le x \\Rightarrow m = x",
                       "不可判定（依赖选择公理）", false, false);
    }

    /* ============================================================
     * 第三部分：不动点理论（1个）
     * ============================================================ */

    /* -------------------- 6. 不动点定理（Tarski/Knaster） -------------------- */
    /**
     * @brief Tarski/Knaster不动点定理
     *
     * 在完备格 (L, ≤) 上，保序映射 f: L → L 的不动点集 Fix(f)
     * 构成一个非空完备格。
     * 特别地，f 的最小不动点为 ∧{x ∈ L : f(x) ≤ x}，
     * 最大不动点为 ∨{x ∈ L : x ≤ f(x)}。
     * 此定理在程序语义（最小/最大不动点语义）中有重要应用。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_FUNCTION};
        REGISTER_ORDER("fixed_point_theorem", "不动点定理（Tarski/Knaster）：完备格上保序映射的不动点集构成非空完备格",
                       inputs, 2, PRESET_TYPE_SET,
                       "\\text{Fix}(f) = \\{x \\in L : f(x) = x\\} \\text{ 构成完备格}, \\quad "
                       "\\mu f = \\bigwedge\\{x : f(x) \\le x\\}, \\; "
                       "\\nu f = \\bigvee\\{x : x \\le f(x)\\}",
                       "O(|L|^2)", true, false);
    }

    /* ============================================================
     * 第四部分：Galois连接与完备化（2个）
     * ============================================================ */

    /* -------------------- 7. Galois连接 -------------------- */
    /**
     * @brief Galois连接
     *
     * 构造或验证两个偏序集 (A, ≤_A) 和 (B, ≤_B) 之间的Galois连接。
     * 一对映射 (f, g) 构成Galois连接（伴随对），当且仅当：
     *   f: A → B, g: B → A，且对所有 a ∈ A, b ∈ B：
     *   f(a) ≤_B b 当且仅当 a ≤_A g(b)
     * 其中 f 称为 g 的左伴随（下伴随），g 称为 f 的右伴随（上伴随）。
     * Galois连接保持上确界和下确界：f 保 ∧，g 保 ∨。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        REGISTER_ORDER("galois_connection",
                       "Galois连接：验证 (f, g) 是否构成偏序集间的Galois连接（伴随对），f(a) ≤ b ⟺ a ≤ g(b)", inputs, 4,
                       PRESET_TYPE_BOOLEAN,
                       "f \\dashv g \\Leftrightarrow \\forall a \\in A, \\forall b \\in B: "
                       "f(a) \\le_B b \\Leftrightarrow a \\le_A g(b)",
                       "O(|A| \\cdot |B|)", true, false);
    }

    /* -------------------- 8. 完备化 -------------------- */
    /**
     * @brief 完备化（Dedekind-MacNeille完备化）
     *
     * 将偏序集 (P, ≤) 嵌入到其Dedekind-MacNeille完备化中。
     * 完备化是包含 P 的最小完备格，通过将 P 的所有"闭包"
     * （即等于其下界的上确界的子集）作为元素来构造。
     * 嵌入映射 a ↦ ↓a 保持 P 中的所有上确界和下确界。
     * Dedekind-MacNeille完备化是最小的完备化，优于一般的理想完备化。
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        REGISTER_ORDER("complete_lattice_completion", "完备化（Dedekind-MacNeille）：将偏序集嵌入到其最小完备格中",
                       inputs, 2, PRESET_TYPE_SET,
                       "DM(P) = \\{S \\subseteq P : S^{ul} = S\\}, "
                       "\\quad \\iota: P \\hookrightarrow DM(P), \\; a \\mapsto \\downarrow a",
                       "O(2^{|P|})", true, false);
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == ORDER_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取序理论预设函数块数量
 *
 * @return int 序理论模块预设函数块总数（8）
 */
int preset_order_theory_count(void) {
    return ORDER_THEORY_PRESET_COUNT;
}
