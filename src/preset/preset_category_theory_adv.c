/**
 * @file preset_category_theory_adv.c
 * @brief 范畴论进阶预设函数块模块 - 实现（v10.0 统一注册模式）
 *
 * 实现理论数学研究中范畴论进阶运算的预设函数块。
 * 涵盖Yoneda引理、Kan扩张、单子与余单子、预层与层、伴随函子进阶。
 * 共20个预设函数块，均遵循模块化、确定性原则，全部使用中文注释。
 *
 * @module CategoryTheoryAdvanced
 * @category PRESET_CATEGORY_CATEGORY_THEORY
 * @version 10.0.0
 */

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_category_theory_adv.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include <string.h>
#include <stdlib.h>

/* ==================== 预设函数块数量 ==================== */

/** 进阶范畴论模块预设函数块总数 */
#define CATEGORY_THEORY_ADV_PRESET_COUNT 20

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个进阶范畴论预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有进阶范畴论预设都属于 PRESET_CATEGORY_CATEGORY_THEORY 类别。
 *
 * @param name 预设名称（唯一标识符）
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入类型数量
 * @param output_type 输出类型
 * @param math_def 数学定义（LaTeX格式）
 * @param complexity 时间复杂度估计
 * @param is_constructive 是否构造性（能否实际构造出结果）
 * @param is_reversible 是否可逆（输入能否从输出唯一恢复）
 * @return true 注册成功，false 注册失败
 */
static bool register_cat_adv_preset(
    const char *name, const char *description,
    const PresetType *input_types, int input_count, PresetType output_type,
    const char *math_def, const char *complexity,
    bool is_constructive, bool is_reversible)
{
    return preset_blocks_register_simple(
        name, description, PRESET_CATEGORY_CATEGORY_THEORY,
        input_types, input_count, output_type,
        math_def, complexity, is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

/**
 * @brief 注册所有进阶范畴论预设函数块
 *
 * 按功能分四大类注册20个预设：
 * 1. 可表函子与Yoneda引理（4个）
 * 2. 极限与余极限进阶（5个）
 * 3. 单子与余单子（4个）
 * 4. 预层与Grothendieck拓扑（3个）
 * 5. 伴随函子与泛性质进阶（4个）
 *
 * @return true 全部（20个）注册成功，false 部分注册失败
 */
bool preset_category_theory_adv_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：可表函子与Yoneda引理（4个）
     * ============================================================ */

    /**
     * @brief cat_adv_hom_functor - Hom函子构造
     *
     * 对于范畴C中的任意两个对象X, Y，构造Hom函子
     * Hom_C(-, -): C^op × C → Set。
     * Hom函子是范畴论最核心的函子之一，它把态射集合提升为函子层面。
     *
     * @param X 范畴对象（PRESET_TYPE_SET）
     * @param Y 范畴对象（PRESET_TYPE_SET）
     * @return Hom集 Hom_C(X, Y)（PRESET_TYPE_SET）
     * @math \\mathrm{Hom}_\\mathcal{C}(X, Y) = \\{f: X \\to Y \\mid f \\in \\mathrm{Mor}(\\mathcal{C})\\}
     * @complexity O(|Mor(C)|)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_HOM_FUNCTOR,
                "Hom函子构造：对范畴C中的两个对象X、Y，构造它们之间的态射集Hom_C(X, Y)，"
                "具有对第一变元反变、对第二变元协变的函子性质",
                inputs, 2, PRESET_TYPE_SET,
                "\\mathrm{Hom}_\\mathcal{C}(-, -): \\mathcal{C}^{\\mathrm{op}} \\times \\mathcal{C} \\to \\mathbf{Set}, \\quad "
                "(X, Y) \\mapsto \\mathrm{Hom}_\\mathcal{C}(X, Y)",
                "O(|\\mathrm{Mor}(C)|)", true, false)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_representable_functor - 可表函子判定
     *
     * 判定一个从范畴C到Set的函子F: C → Set是否可表，
     * 即是否存在对象A ∈ C使得 F ≅ Hom_C(A, -)。
     * 可表函子是范畴论与代数几何的关键桥梁。
     *
     * @param F 函子（PRESET_TYPE_FUNCTION）
     * @return 是否可表（PRESET_TYPE_BOOLEAN）
     * @math \\exists A \\in \\mathcal{C}, \\; F \\cong \\mathrm{Hom}_\\mathcal{C}(A, -)
     * @complexity O(|C|·|F|)
     * @constructive true 若可表，可给出表示对象A
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_REPRESENTABLE_FUNCTOR,
                "可表函子判定：判断函子F: C → Set是否可由某个对象A表示（即F ≅ Hom_C(A, -)），"
                "这是范畴论核心问题之一",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "F \\text{ 可表} \\Leftrightarrow \\exists A, \\; F \\cong \\mathrm{Hom}_\\mathcal{C}(A, -)",
                "O(|\\mathcal{C}| \\cdot |F|)", true, false)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_yoneda_embedding - Yoneda嵌入
     *
     * 构造Yoneda嵌入函子 よ: C → [C^op, Set]，
     * 将对象A映射到可表预层 Hom_C(-, A)。
     * Yoneda嵌入是完全忠实函子（full and faithful），
     * 它将范畴C完全嵌入到预层范畴中。
     *
     * @param A 范畴对象（PRESET_TYPE_SET）
     * @return 对应的可表预层（PRESET_TYPE_FUNCTION）
     * @math \\text{よ}(A) = \\mathrm{Hom}_\\mathcal{C}(-, A), \\quad
     *       \\mathrm{Nat}(\\text{よ}(A), \\text{よ}(B)) \\cong \\mathrm{Hom}_\\mathcal{C}(A, B)
     * @complexity O(|C|)
     * @constructive true
     * @reversible true（由Yoneda引理，嵌入是完全忠实的）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_YONEDA_EMBEDDING,
                "Yoneda嵌入：构造完全忠实函子 よ: C → [C^op, Set]，将对象A映射到可表预层Hom_C(-, A)，"
                "实现范畴的忠实嵌入",
                inputs, 1, PRESET_TYPE_FUNCTION,
                "\\text{よ}: \\mathcal{C} \\to [\\mathcal{C}^{\\mathrm{op}}, \\mathbf{Set}], \\quad "
                "\\text{よ}(A) = \\mathrm{Hom}_\\mathcal{C}(-, A)",
                "O(|\\mathcal{C}|)", true, true)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_yoneda_lemma - Yoneda引理应用
     *
     * 应用Yoneda引理：对于任意预层F: C^op → Set和对象A，
     * 存在自然同构 Nat(Hom_C(-, A), F) ≅ F(A)。
     * 这是范畴论最深刻的引理之一，它将函子范畴中的自然变换
     * 还原为集合中的元素。
     *
     * @param preasheaf 预层F（PRESET_TYPE_FUNCTION）
     * @param object_A 对象A（PRESET_TYPE_SET）
     * @return 自然同构下F(A)中的对应元素（PRESET_TYPE_SET）
     * @math \\mathrm{Nat}(\\mathrm{Hom}_\\mathcal{C}(-, A), F) \\cong F(A), \\quad
     *       \\text{同构由 } \\Phi \\mapsto \\Phi_A(\\mathrm{id}_A) \\text{ 给出}
     * @complexity O(1)
     * @constructive true
     * @reversible true（同构可逆）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SET};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_YONEDA_LEMMA,
                "Yoneda引理应用：建立自然变换集Nat(Hom(-,A), F)与F(A)之间的双射，"
                "φ ↦ φ_A(id_A)是其核心对应",
                inputs, 2, PRESET_TYPE_SET,
                "\\mathrm{Nat}(\\mathrm{Hom}_\\mathcal{C}(-, A), F) \\cong F(A), \\quad "
                "\\Phi \\mapsto \\Phi_A(\\mathrm{id}_A)",
                "O(1)", true, true)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：极限与余极限进阶（5个）
     * ============================================================ */

    /**
     * @brief cat_adv_limit - 一般极限构造
     *
     * 对任意函子（图表）D: J → C，构造其极限lim D。
     * 极限是积、拉回、等化子等具体极限概念的统一概括。
     * 若极限存在，返回极限对象及其到图表的投射态射族。
     *
     * @param diagram 图表J（PRESET_TYPE_FUNCTION，视为函子）
     * @return 极限对象 lim D（PRESET_TYPE_SET）
     * @math \\lim D = \\{x \\in \\prod_{j} D(j) \\mid \\forall \\alpha: j \\to k, \\; D(\\alpha)(x_j) = x_k\\}
     * @complexity O(|J|·|C|)
     * @constructive true（若极限存在）
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_LIMIT,
                "一般极限构造：对任意图表函子D: J → C，构造其极限lim_D。极限统一了积、拉回、"
                "等化子等概念，满足泛性质：任意到D的锥锥可通过lim_D唯一分解",
                inputs, 1, PRESET_TYPE_SET,
                "\\lim_{j \\in J} D(j) = \\{x \\in \\prod_{j} D(j) \\mid \\forall \\alpha: j \\to k, "
                "D(\\alpha)(x_j) = x_k\\}",
                "O(|\\mathcal{J}| \\cdot |\\mathcal{C}|)", true, false)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_colimit - 一般余极限构造
     *
     * 对任意函子（图表）D: J → C，构造其余极限colim D。
     * 余极限是余积、推出、余等化子等概念的统一定义。
     * 若余极限存在，返回余极限对象及其从图表到余极限的内射态射族。
     *
     * @param diagram 图表J（PRESET_TYPE_FUNCTION）
     * @return 余极限对象 colim D（PRESET_TYPE_SET）
     * @math \\mathrm{colim}\\, D = \\coprod_{j} D(j) / {\\sim}, \\quad
     *       \\text{其中 } x \\in D(j) \\sim D(\\alpha)(x) \\in D(k)
     * @complexity O(|J|·|C|)
     * @constructive true（若余极限存在）
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_COLIMIT,
                "一般余极限构造：对任意图表函子D: J → C，构造其余极限colim_D。余极限统一了余积、"
                "推出、余等化子等概念，是极限的对偶概念",
                inputs, 1, PRESET_TYPE_SET,
                "\\mathrm{colim}_{j \\in J} D(j) = \\left(\\coprod_{j} D(j)\\right) / {\\sim}, \\quad "
                "x \\in D(j) \\sim D(\\alpha)(x) \\in D(k)",
                "O(|\\mathcal{J}| \\cdot |\\mathcal{C}|)", true, false)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_filtered_colimit - 滤过余极限
     *
     * 构造滤过范畴（filtered category）上的余极限。
     * 滤过余极限具有优良性质：在Set中与有限极限交换，
     * 且保持精确性（在Grothendieck范畴中）。
     *
     * @param diagram 滤过图表J（PRESET_TYPE_FUNCTION）
     * @return 滤过余极限对象（PRESET_TYPE_SET）
     * @math \\varinjlim_{j \\in J} D(j), \\quad J \\text{ 是滤过范畴} \\Leftrightarrow
     *       \\forall i,j \\; \\exists k \\text{ 及态射 } i \\to k, j \\to k
     * @complexity O(|J|·|C|)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_FILTERED_COLIMIT,
                "滤过余极限：在滤过范畴J上构造余极限colim_D。滤过余极限在Set中与有限极限交换，"
                "是层论和模型论中的重要工具",
                inputs, 1, PRESET_TYPE_SET,
                "\\varinjlim_{j \\in J} D(j), \\quad J \\text{ 滤过} \\Leftrightarrow "
                "\\forall i,j \\; \\exists k, i \\to k, j \\to k",
                "O(|\\mathcal{J}| \\cdot |\\mathcal{C}|)", true, false)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_kan_extension - Kan扩张
     *
     * 对于函子F: C → D和K: C → E，构造F沿K的左Kan扩张Lan_K F: E → D。
     * Kan扩张是泛性质在函子层面的体现，推广了极限和余极限的概念。
     * 在函子范畴[E, D]中，Lan_K F是使自然变换的泛性质成立的最佳逼近。
     *
     * @param F 函子F: C → D（PRESET_TYPE_FUNCTION）
     * @param K 函子K: C → E（PRESET_TYPE_FUNCTION）
     * @return 左Kan扩张 Lan_K F（PRESET_TYPE_FUNCTION）
     * @math (\\mathrm{Lan}_K F)(e) = \\mathrm{colim}((K \\downarrow e) \\to \\mathcal{C} \\xrightarrow{F} \\mathcal{D})
     * @complexity O(|E|·|C|·|D|)
     * @constructive true（若涉及的小余极限存在）
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_KAN_EXTENSION,
                "左Kan扩张：对函子F: C→D沿K: C→E做左Kan扩张Lan_K F: E→D。"
                "这是函子层面的最优延拓，推广了极限和余极限的概念",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "(\\mathrm{Lan}_K F)(e) = \\mathrm{colim}\\left((K \\downarrow e) \\to "
                "\\mathcal{C} \\xrightarrow{F} \\mathcal{D}\\right)",
                "O(|\\mathcal{E}| \\cdot |\\mathcal{C}| \\cdot |\\mathcal{D}|)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：单子与余单子（4个）
     * ============================================================ */

    /**
     * @brief cat_adv_monad - 单子（Monad）
     *
     * 构造范畴C上的单子 T = (T, η, μ)，包含：
     * - 自函子 T: C → C
     * - 单位 η: id_C → T
     * - 乘法 μ: T² → T
     * 满足结合律和单位律。单子是一般代数理论的范畴论表述，
     * 在Haskell等函数式编程语言中广泛使用。
     *
     * @param endofunctor 自函子T（PRESET_TYPE_FUNCTION）
     * @param unit 单位自然变换η（PRESET_TYPE_FUNCTION）
     * @param multiplication 乘法自然变换μ（PRESET_TYPE_FUNCTION）
     * @return 单子结构（PRESET_TYPE_STRUCTURE）
     * @math (T, \\eta: 1_\\mathcal{C} \\Rightarrow T, \\mu: T^2 \\Rightarrow T),
     *       \\quad \\mu \\circ T\\mu = \\mu \\circ \\mu T, \\quad \\mu \\circ T\\eta = \\mu \\circ \\eta T = 1_T
     * @complexity O(1)（仅验证结构）
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_MONAD,
                "单子（Monad）：构造范畴C上的单子(T,η,μ)，包含自函子T: C→C、单位η: 1_C→T、"
                "乘法μ: T²→T，满足结合律和单位律。单子是代数理论的范畴论表述",
                inputs, 3, PRESET_TYPE_STRUCTURE,
                "(T, \\eta: 1_\\mathcal{C} \\Rightarrow T, \\mu: T^2 \\Rightarrow T), \\quad "
                "\\mu \\circ T\\mu = \\mu \\circ \\mu T, \\; \\mu \\circ \\eta T = \\mu \\circ T\\eta = 1_T",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_comonad - 余单子（Comonad）
     *
     * 构造范畴C上的余单子，是单子的对偶概念。
     * 余单子包含余单位ε: W → id_C 和余乘法δ: W → W²。
     * 余单子在(∞,1)-范畴、导出范畴中有重要应用。
     *
     * @param endofunctor 自函子W（PRESET_TYPE_FUNCTION）
     * @param counit 余单位ε（PRESET_TYPE_FUNCTION）
     * @param comultiplication 余乘法δ（PRESET_TYPE_FUNCTION）
     * @return 余单子结构（PRESET_TYPE_STRUCTURE）
     * @math (W, \\varepsilon: W \\Rightarrow 1_\\mathcal{C}, \\delta: W \\Rightarrow W^2),
     *       \\quad \\delta W \\circ \\delta = W\\delta \\circ \\delta, \\; \\varepsilon W \\circ \\delta = W\\varepsilon \\circ \\delta = 1_W
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_COMONAD,
                "余单子（Comonad）：构造范畴C上的余单子(W,ε,δ)，是单子的对偶概念。"
                "包含余单位ε: W→1_C和余乘法δ: W→W²，满足余结合律和余单位律",
                inputs, 3, PRESET_TYPE_STRUCTURE,
                "(W, \\varepsilon: W \\Rightarrow 1_\\mathcal{C}, \\delta: W \\Rightarrow W^2), \\quad "
                "\\delta W \\circ \\delta = W\\delta \\circ \\delta",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_kleisli_category - Kleisli范畴
     *
     * 对给定单子(T, η, μ)构造其Kleisli范畴C_T。
     * Kleisli范畴的对象与C相同，态射C_T(A,B) = C(A, T(B))，
     * 复合由μ定义。Kleisli范畴是函数式编程中monadic计算的范畴论模型。
     *
     * @param monad 单子T（PRESET_TYPE_STRUCTURE）
     * @return Kleisli范畴C_T（PRESET_TYPE_SET）
     * @math \\mathrm{Ob}(\\mathcal{C}_T) = \\mathrm{Ob}(\\mathcal{C}), \\quad
     *       \\mathcal{C}_T(A, B) = \\mathcal{C}(A, T(B))
     * @complexity O(|C|²)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_KLEISLI_CATEGORY,
                "Kleisli范畴：对单子(T,η,μ)构造其Kleisli范畴C_T。对象同C，"
                "态射Hom_{C_T}(A,B) = Hom_C(A, T(B))，复合由μ定义。这是monadic计算的范畴论模型",
                inputs, 1, PRESET_TYPE_SET,
                "\\mathrm{Ob}(\\mathcal{C}_T) = \\mathrm{Ob}(\\mathcal{C}), \\quad "
                "\\mathcal{C}_T(A, B) = \\mathcal{C}(A, T(B)), \\quad g \\circ_T f = \\mu \\circ T(g) \\circ f",
                "O(|\\mathcal{C}|^2)", true, false)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_eilenberg_moore - Eilenberg-Moore代数
     *
     * 对给定单子(T, η, μ)构造其Eilenberg-Moore范畴（T-代数范畴）C^T。
     * 对象是T-代数(A, a: T(A) → A)，态射是T-代数同态。
     * Eilenberg-Moore范畴是单子的"终端"分解，
     * 与Kleisli范畴一起构成了单子分解的上下界。
     *
     * @param monad 单子T（PRESET_TYPE_STRUCTURE）
     * @return T-代数范畴C^T（PRESET_TYPE_SET）
     * @math (A, a: T(A) \\to A) \\text{ 满足 } a \\circ \\eta_A = 1_A, \\; a \\circ T(a) = a \\circ \\mu_A
     * @complexity O(|C|²)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_EILENBERG_MOORE,
                "Eilenberg-Moore代数：对单子T构造其T-代数范畴C^T（Eilenberg-Moore范畴）。"
                "对象是T-代数(A, a: TA→A)，满足结合律和单位律。是单子的终端分解",
                inputs, 1, PRESET_TYPE_SET,
                "(A, a: T(A) \\to A), \\quad a \\circ \\eta_A = 1_A, \\; a \\circ T(a) = a \\circ \\mu_A",
                "O(|\\mathcal{C}|^2)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：预层与Grothendieck拓扑（3个）
     * ============================================================ */

    /**
     * @brief cat_adv_presheaf - 预层构造
     *
     * 构造拓扑空间（或一般范畴）上的预层。
     * 预层是反变函子 F: Open(X)^op → Set（或一般范畴C上的反变函子C^op → Set）。
     * 预层范畴[Open(X)^op, Set]是一个Grothendieck topos，具丰富的结构。
     *
     * @param category 基范畴C（PRESET_TYPE_SET）
     * @return 预层范畴的结构（PRESET_TYPE_FUNCTION）
     * @math \\mathcal{F}: \\mathcal{C}^{\\mathrm{op}} \\to \\mathbf{Set}, \\quad
     *       \\text{预层是反变函子，对每个开集分配一个集合}
     * @complexity O(|C|)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_PRESHEAF,
                "预层构造：在基范畴C上构造预层（反变函子F: C^op → Set）。"
                "预层是层论的基础，其范畴[Open(X)^op, Set]是Grothendieck topos",
                inputs, 1, PRESET_TYPE_FUNCTION,
                "\\mathcal{F}: \\mathcal{C}^{\\mathrm{op}} \\to \\mathbf{Set}, \\quad "
                "\\text{预层为每个对象（开集）分配一个截面集合}",
                "O(|\\mathcal{C}|)", true, false)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_sheafification - 层化
     *
     * 将预层F通过层化函子(+)变成层F^+。
     * 层化是预层范畴到层范畴的左伴随，满足泛性质：
     * 对任意层G，Hom_{Sh}(F^+, G) ≅ Hom_{PSh}(F, G)。
     * 层化消除了预层中不满足粘合条件的数据。
     *
     * @param preasheaf 预层F（PRESET_TYPE_FUNCTION）
     * @return 对应的层F^+（PRESET_TYPE_FUNCTION）
     * @math \\mathcal{F}^+(U) = \\varinjlim_{\\mathcal{U} \\in \\mathrm{Cov}(U)} "
     *        \\ker\\left(\\prod_i \\mathcal{F}(U_i) \\rightrightarrows \\prod_{i,j} \\mathcal{F}(U_i \\cap U_j)\\right)
     * @complexity O(|X|²)
     * @constructive true
     * @reversible false（层化是幂等的：F^(++) ≅ F^+）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_SHEAFIFICATION,
                "层化：将预层F通过(+)-构造变成层F^+。层化是预层范畴到层范畴的左伴随函子，"
                "满足泛性质：Hom_{Sh}(F^+, G) ≅ Hom_{PSh}(F, G)",
                inputs, 1, PRESET_TYPE_FUNCTION,
                "\\mathcal{F}^+(U) = \\varinjlim_{\\mathcal{U}} \\ker\\left(\\prod_i \\mathcal{F}(U_i) "
                "\\rightrightarrows \\prod_{i,j} \\mathcal{F}(U_i \\cap U_j)\\right)",
                "O(|X|^2)", true, false)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_grothendieck_topology - Grothendieck拓扑
     *
     * 在范畴C上定义Grothendieck拓扑（由覆盖筛给出）。
     * Grothendieck拓扑将开覆盖的概念推广到任意范畴，
     * 使得任意范畴上都可以定义层。这是代数几何中
     * étale拓扑、平坦拓扑等概念的基础。
     *
     * @param category 范畴C（PRESET_TYPE_SET）
     * @param covering_family 覆盖族（PRESET_TYPE_SET）
     * @return Grothendieck拓扑结构（PRESET_TYPE_STRUCTURE）
     * @math J \\text{ 是Grothendieck拓扑} \\Leftrightarrow \\forall U, J(U) \\text{ 是筛（sieve）, 满足稳定性等公理}
     * @complexity O(|C|³)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_GROTHENDIECK_TOPOLOGY,
                "Grothendieck拓扑：在范畴C上构造Grothendieck拓扑J（由覆盖筛定义）。"
                "将开覆盖概念推广到任意范畴，是代数几何中étale/平坦拓扑的基础",
                inputs, 2, PRESET_TYPE_STRUCTURE,
                "\\text{Grothendieck拓扑 } J: \\forall U \\in \\mathcal{C}, J(U) \\text{ 是覆盖 } U \\text{ 的筛族},"
                "\\text{ 满足稳定性、传递性和基变换公理}",
                "O(|\\mathcal{C}|^3)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：伴随函子与泛性质进阶（4个）
     * ============================================================ */

    /**
     * @brief cat_adv_adjoint_unit_counit - 伴随的单位与余单位
     *
     * 从一对伴随函子F ⊣ G中提取单位和余单位：
     * - 单位 η: id_C → GF
     * - 余单位 ε: FG → id_D
     * 满足三角恒等式。单位-余单位表述是伴随函子的等价刻画，
     * 在证明中比Hom集同构表述更便于操作。
     *
     * @param F 左伴随函子（PRESET_TYPE_FUNCTION）
     * @param G 右伴随函子（PRESET_TYPE_FUNCTION）
     * @return 包含η和ε的结构（PRESET_TYPE_STRUCTURE）
     * @math F \\dashv G \\Leftrightarrow \\exists \\eta: 1_\\mathcal{C} \\Rightarrow GF, \\; "
     *        "\\varepsilon: FG \\Rightarrow 1_\\mathcal{D}, \\text{ 满足三角恒等式}
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_ADJOINT_UNIT_COUNIT,
                "伴随的单位与余单位：从伴随对F⊣G中提取单位η: 1_C→GF和余单位ε: FG→1_D，"
                "它们满足三角恒等式：εF∘Fη=1_F, Gε∘ηG=1_G",
                inputs, 2, PRESET_TYPE_STRUCTURE,
                "F \\dashv G: \\; \\eta: 1_\\mathcal{C} \\Rightarrow GF, \\; "
                "\\varepsilon: FG \\Rightarrow 1_\\mathcal{D}, \\quad "
                "\\varepsilon F \\circ F\\eta = 1_F, \\; G\\varepsilon \\circ \\eta G = 1_G",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_universal_property - 泛性质验证
     *
     * 验证给定构造是否满足某种泛性质。
     * 泛性质是范畴论的核心概念：一个对象满足泛性质当且仅当
     * 存在唯一的态射使其交换图成立。
     * 本预设接收一个构造和一个性质描述，返回验证结果。
     *
     * @param construction 待验证构造（PRESET_TYPE_STRUCTURE）
     * @param property_desc 泛性质形式化描述（PRESET_TYPE_EXPRESSION）
     * @return 验证结果（PRESET_TYPE_BOOLEAN）
     * @math \\forall X, \\; \\exists! f: A \\to X \\text{ 使得指定图表交换} \\Rightarrow A \\text{ 满足泛性质}
     * @complexity O(|C|·|P|)
     * @constructive true（若成立则给出唯一态射）
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_EXPRESSION};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_UNIVERSAL_PROPERTY,
                "泛性质验证：验证一个构造是否满足指定的泛性质（即存在唯一的态射使图表交换）。"
                "泛性质是范畴论的核心工具，刻画了对象'在同构意义下唯一'",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "\\forall X, \\; \\exists! f: A \\to X \\text{ s.t. diagram commutes} \\Rightarrow "
                "A \\text{ satisfies universal property}",
                "O(|\\mathcal{C}| \\cdot |P|)", true, false)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_free_functor - 自由函子
     *
     * 构造从具体范畴到抽象范畴的自由函子。
     * 自由函子是遗忘函子的左伴随：Free(C) ⊢ U。
     * 例子：从集合到群的自由群函子、从集合到向量空间的自由向量空间函子。
     * 自由函子体现了"用最少的约束从给定数据生成结构"的思想。
     *
     * @param base_object 基对象（PRESET_TYPE_SET）
     * @return 自由结构（PRESET_TYPE_ALGEBRA）
     * @math F(X) \\text{ 满足：} \\forall f: X \\to U(A), \\; \\exists! \\tilde{f}: F(X) \\to A
     *       \\text{ 使得 } U(\\tilde{f}) \\circ \\eta_X = f
     * @complexity O(|X|)
     * @constructive true
     * @reversible false（除非在变量范畴中）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_FREE_FUNCTOR,
                "自由函子：从基对象构造自由代数结构。自由函子是遗忘函子的左伴随F⊣U，"
                "体现了'用最少约束从给定数据生成结构'的思想。例：自由群、自由向量空间",
                inputs, 1, PRESET_TYPE_ALGEBRA,
                "F \\dashv U, \\; F(S) \\text{ 是 } S \\text{ 上的自由结构}, \\quad "
                "\\forall f: S \\to U(A), \\; \\exists! \\tilde{f}: F(S) \\to A",
                "O(|S|)", true, false)) {
            success_count++;
        }
    }

    /**
     * @brief cat_adv_forgetful_functor - 遗忘函子
     *
     * 构造遗忘函子，将具有附加结构的对象映射到其底层集合。
     * 遗忘函子从"结构化范畴"到"更简单的范畴"。
     * 例子：Grp → Set（遗忘群运算）、Ring → Ab（遗忘乘法）。
     * 遗忘函子通常有左伴随（自由函子）。
     *
     * @param structured_object 带结构对象（PRESET_TYPE_ALGEBRA）
     * @return 底层纯粹集合（PRESET_TYPE_SET）
     * @math U: \\mathrm{Grp} \\to \\mathbf{Set}, \\quad (G, \\cdot, e) \\mapsto G
     * @complexity O(1)
     * @constructive true
     * @reversible false（遗忘是有损的）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        if (register_cat_adv_preset(
                PRESET_CAT_ADV_FORGETFUL_FUNCTOR,
                "遗忘函子：将带结构的数学对象映射到其底层集合，遗忘所有附加结构。"
                "例如：Grp→Set遗忘群运算，Ring→Ab遗忘乘法。遗忘函子通常有左伴随（自由函子）",
                inputs, 1, PRESET_TYPE_SET,
                "U: \\mathcal{A} \\to \\mathbf{Set}, \\quad (A, \\text{structure}) \\mapsto A, \\quad "
                "U \\text{ 遗忘所有附加结构}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ==================== 注册结果验证 ==================== */

    if (success_count < CATEGORY_THEORY_ADV_PRESET_COUNT) {
        LV00_LOG_WARNING("进阶范畴论模块：仅成功注册 %d/%d 个预设",
                         success_count, CATEGORY_THEORY_ADV_PRESET_COUNT);
        return false;
    }

    return true;
}
