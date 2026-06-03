/**
 * @file preset_model_theory.c
 * @brief 模型论预设函数块 - 实现
 *
 * @details 实现理论数学研究中模型论领域的预设函数块。
 *          所有预设函数块都遵循模块化、确定性原则，
 *          为结构理论、初等等价、模型构造等提供基础运算。
 *
 * 数学基础：
 * - 结构理论基于一阶逻辑的语义
 * - 初等等价基于Tarski的真值定义
 * - 模型构造基于Henkin构造法
 * - 紧致性基于超积定理
 * - 稳定性理论基于Shelah的分类理论
 *
 * @module ModelTheory
 * @category PRESET_CATEGORY_MATH_LOGIC
 * @version 13.0.0
 */

#include "preset_model_theory.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 模型论模块预设函数块总数 */
#define MODEL_THEORY_PRESET_COUNT 38

/* ==================== 模块注册实现 ==================== */

/**
 * @brief 模型论模块默认类别
 */
#define MODEL_THEORY_DEFAULT_CATEGORY PRESET_CATEGORY_MATH_LOGIC

/**
 * @brief 注册模型论模块的所有预设函数块
 *
 * 本函数实现模型论领域的38个核心预设：
 * - 结构理论（9个）：子结构、嵌入、同构、初等嵌入
 * - 初等等价（6个）：初等等价、初等子结构、Tarski-Vaught
 * - 模型构造（7个）：常量扩展、类型实现、素模型、饱和模型
 * - 紧致性（7个）：紧致性检验、力量子化、超积
 * - 稳定性理论（7个）：稳定性判定、分叉、极小集合
 * - 量词消去（6个）：量词消去、代数性、可判定性
 *
 * @return true 所有预设注册成功
 */
bool preset_model_theory_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：结构理论
     * ============================================================
     *
     * 结构理论研究数学结构的性质和关系。
     */

    /* -------------------- 子结构判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_STRUCTURE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_STRUCTURE_SUBSTRUCTURE,
                                        "子结构判定：判断结构B是否为结构A的子结构",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "B \\subseteq A \\Leftrightarrow \\text{论域闭包且函数/关系保持}",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 嵌入构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_STRUCTURE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_STRUCTURE_EMBEDDING,
                                        "嵌入构造：从A到B的初等嵌入（保持所有一阶公式）",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FUNCTION,
                                        "f: A \\hookrightarrow B \\Leftrightarrow \\text{单射且} A \\equiv_f B",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 同构判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_STRUCTURE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_STRUCTURE_ISOMORPHISM,
                                        "同构判定：判断两个结构是否同构",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "A \\cong B \\Leftrightarrow \\exists f:\\text{双射}, f\\text{为同构}",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 初等嵌入 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_STRUCTURE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_STRUCTURE_ELEMENTARY_EMBEDDING,
                                        "初等嵌入：构造保持所有一阶真理的嵌入映射",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FUNCTION,
                                        "f:A \\preceq B \\Leftrightarrow \\forall \\phi(x): A \\models \\phi(a) \\iff B \\models \\phi(f(a))",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 归约计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_SIGNATURE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_STRUCTURE_REDUCTION,
                                        "归约：计算结构在子签名上的归约",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_STRUCTURE,
                                        "A|_{\\sigma} = \\text{仅保留}\\sigma\\text{中符号的结构}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 膨胀计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_SIGNATURE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_STRUCTURE_EXPANSION,
                                        "膨胀：在结构上添加新符号得到膨胀结构",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_STRUCTURE,
                                        "(A, \\bar{a}) = A\\text{带新常数}\\bar{a}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 消去域计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_STRUCTURE_ELIMINATION_DOMAIN,
                                        "消去域：计算结构的消去域（量词消去的基础）",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_SET,
                                        "dcl(A) = \\{ b \\in M : \\exists \\text{公式}\\phi(x,\\bar{a}), M \\models \\phi(b,\\bar{a}) \\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 代数闭包 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_SET};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_STRUCTURE_ALGEBRAIC_CLOSURE,
                                        "代数闭包：计算集合A的代数闭包 acl(A)",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_SET,
                                        "acl(A) = \\{ b : \\exists \\text{代数公式}\\phi(x,\\bar{a}), M \\models \\phi(b,\\bar{a}) \\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- definable闭包 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_SET};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_STRUCTURE_DEFINABLE_CLOSURE,
                                        "可定义闭包：计算集合A的可定义闭包 dcl(A)",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_SET,
                                        "dcl(A) = \\{ b : \\exists \\text{公式}\\phi(x,\\bar{a}), M \\models \\phi(b,\\bar{a}) \\}",
                                        "O(n)",
                                        true, false);
    }

    /* ============================================================
     * 第二部分：初等等价
     * ============================================================
     *
     * 初等等价研究结构之间的一阶可辨别性。
     */

    /* -------------------- 初等等价判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_STRUCTURE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_ELEMENTARY_EQUIVALENCE,
                                        "初等等价判定：判断两个结构是否初等等价",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "A \\equiv B \\Leftrightarrow \\forall \\phi: A \\models \\phi \\iff B \\models \\phi",
                                        "O(2^n)",
                                        false, false);
    }

    /* -------------------- 初等子结构判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_STRUCTURE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_ELEMENTARY_SUBSTRUCTURE,
                                        "初等子结构判定：判断B是否为A的初等子结构",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "B \\preceq A \\Leftrightarrow B \\subseteq A \\land \\forall \\phi(x): B \\models \\phi(b) \\iff A \\models \\phi(b)",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- Tarski-Vaught测试 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_STRUCTURE, PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_TARSKI_VAUGHT_TEST,
                                        "Tarski-Vaught测试：检验子结构是否为初等子结构",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 3,
                                        PRESET_TYPE_BOOLEAN,
                                        "B \\preceq A \\Leftrightarrow \\forall \\phi(x), \\bar{b}: A \\models \\exists x \\phi(x,\\bar{b}) \\Rightarrow \\exists a \\in B: A \\models \\phi(a,\\bar{b})",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- Scott同构定理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_INTEGER};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_SCOTT_ISOMORPHISM,
                                        "Scott同构：有限结构由Scott语句（深度k公式）唯一描述",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FORMULA,
                                        "\\delta_k(A) = \\text{描述}A\\text{的深度}k\\text{Scott语句}",
                                        "O(n^k)",
                                        true, false);
    }

    /* -------------------- Back-and-forth系统 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_STRUCTURE, PRESET_TYPE_INTEGER};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_BACK_FORTH_SYSTEM,
                                        "Back-and-forth系统：构造partial isomorphism的链",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 3,
                                        PRESET_TYPE_LIST,
                                        "I_k(A,B) = \\{ p: A \\to B \\text{ 的}k\\text{维部分同构} \\}",
                                        "O(n^k)",
                                        true, false);
    }

    /* -------------------- Ehrenfeucht-Fraisse游戏 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_STRUCTURE, PRESET_TYPE_INTEGER};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_EHRENFEUCHT_GAME,
                                        "EF游戏：判断A和B的n轮游戏是否I_n有必胜策略",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 3,
                                        PRESET_TYPE_STRING,
                                        "EF_n(A,B) = \\text{玩家}\\\\exists\\text{在}n\\text{轮后的胜率}",
                                        "O(2^n)",
                                        false, false);
    }

    /* ============================================================
     * 第三部分：模型构造
     * ============================================================
     *
     * 模型构造研究如何构造满足给定理论的模型。
     */

    /* -------------------- 常量扩展 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_LIST};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODEL_CONSTANT_EXTENSION,
                                        "常量扩展：为结构添加新常量得到膨胀结构",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_STRUCTURE,
                                        "M(\\bar{a}) = M \\cup \\{ \\text{新常数}\\bar{a} \\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 类型实现 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_TYPE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODEL_TYPE_REALIZATION,
                                        "类型实现：在M中构造实现类型p的元组",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_TUPLE,
                                        "p \\in S_n(T) \\Rightarrow \\exists \\bar{a} \\in M^n: p = tp(\\bar{a}/M)",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 素模型构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_THEORY};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODEL_PRIME_MODEL,
                                        "素模型：构造理论T的素模型（嵌入到其他所有模型中）",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_STRUCTURE,
                                        "M\\text{为素模型} \\Leftrightarrow \\forall N \\models T: \\exists f:M \\hookrightarrow N",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 饱和模型构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_CARDINAL};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODEL_SATURATED_MODEL,
                                        "饱和模型：构造κ-饱和模型（实现所有小于κ的类型）",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_STRUCTURE,
                                        "M\\ \\kappa\\text{-饱和} \\Leftrightarrow \\forall p \\in S^1(M): |p| < \\kappa \\Rightarrow p\\text{实现于}M",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 均值模型构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_THEORY, PRESET_TYPE_CARDINAL};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODEL_UNIVERSAL_MODEL,
                                        "均值模型：构造在给定基数上的均值模型",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_STRUCTURE,
                                        "M\\text{为均值模型} \\Leftrightarrow M \\models T \\land |M| = \\kappa",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 原子模型构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_THEORY};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODEL_ATOMIC_MODEL,
                                        "原子模型：构造理论T的原子模型（初等且素）",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_STRUCTURE,
                                        "M\\text{为原子模型} \\Leftrightarrow M \\models T \\land M\\text{初等} \\land M\\text{素}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 模型构造（Henkin构造） -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_THEORY};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODEL_CONSTRUCTION,
                                        "Henkin构造：使用Henkin证明法构造模型",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_STRUCTURE,
                                        "T\\text{协调} \\Rightarrow \\exists M: M \\models T",
                                        "O(n)",
                                        true, false);
    }

    /* ============================================================
     * 第四部分：紧致性
     * ============================================================
     *
     * 紧致性是模型论的核心定理。
     */

    /* -------------------- 紧致性检验 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_THEORY};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_COMPACTNESS,
                                        "紧致性检验：检验理论T是否有限协调",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_BOOLEAN,
                                        "T\\text{有限协调} \\Rightarrow T\\text{协调}",
                                        "O(2^n)",
                                        false, false);
    }

    /* -------------------- 力量子化 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FORMULA, PRESET_TYPE_CARDINAL};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_POWER_QUANTIFIER,
                                        "力量子化：将∃y∀x<y φ(x)转换为力量子形式",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FORMULA,
                                        "(\\exists y)(\\forall x \\preceq y) \\phi(x) \\equiv \\exists Y \\phi(Y)",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- Lindenbaum代数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_THEORY};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_LINDENBAUM_ALGEBRA,
                                        "Lindenbaum代数：构造理论T的Lindenbaum-Boolean代数",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_ALGEBRA,
                                        "B(T) = \\{ [\\phi] : \\phi\\text{为公式} \\} / T",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 完备化构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_THEORY};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_COMPLETION_CONSTRUCTION,
                                        "完备化：构造理论T的所有完备化",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_LIST,
                                        "C(T) = \\{ S \\supseteq T : S\\text{完备且协调} \\}",
                                        "O(2^n)",
                                        true, false);
    }

    /* -------------------- Ultrafilter扩展 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FILTER};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_ULTRAFILTER_EXTENSION,
                                        "Ultrafilter扩展：将滤子扩展为超滤子",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_ULTRAFILTER,
                                        "F\\text{为滤子} \\Rightarrow \\exists U \\supseteq F: U\\text{为超滤子}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- ultraproduct -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_ULTRAFILTER};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_ULTRAPRODUCT,
                                        "超积：构造结构的超积",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_STRUCTURE,
                                        "\\prod_{i \\in I} M_i / U",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- ultrapower -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_ULTRAFILTER};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_ULTRAPOWER,
                                        "超幂：构造结构关于U的超幂",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_STRUCTURE,
                                        "M^I / U",
                                        "O(n)",
                                        true, false);
    }

    /* ============================================================
     * 第五部分：稳定性理论
     * ============================================================
     *
     * 稳定性理论研究理论按照复杂性分类。
     */

    /* -------------------- 稳定性判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_THEORY};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_STABILITY,
                                        "稳定性判定：判断理论T是否λ-稳定",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_BOOLEAN,
                                        "T\\ \\lambda\\text{-稳定} \\Leftrightarrow |S_n(T)| \\le \\lambda",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 不稳定性证明 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_THEORY, PRESET_TYPE_CARDINAL};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_INSTABILITY,
                                        "不稳定性：构造理论T在λ上的反例证明其不稳定",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_PROOF,
                                        "|S_n(T)| > \\lambda",
                                        "O(2^\\lambda)",
                                        true, false);
    }

    /* -------------------- omega-稳定性判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_THEORY};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_OMEGA_STABILITY,
                                        "omega-稳定性判定：判断理论T是否omega-稳定",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_BOOLEAN,
                                        "T\\ \\omega\\text{-稳定} \\Leftrightarrow T\\ |\\omega|\\text{-稳定}",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- superstable判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_THEORY};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_SUPERSTABLE,
                                        "superstable判定：判断理论T是否superstable",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_BOOLEAN,
                                        "T\\text{ superstable} \\Leftrightarrow \\forall \\lambda \\ge 2^{|T|}: T\\ \\lambda\\text{-稳定}",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 稳定型计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_TYPE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_STABLE_TYPE,
                                        "稳定型：判断给定型是否稳定",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "p \\in S(A)\\text{稳定} \\Leftrightarrow \\text{IMH holds for }p",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 分叉点计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TYPE, PRESET_TYPE_TYPE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_FORKING,
                                        "分叉点：计算型p和q的分叉点（独立性）",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_STRING,
                                        "p \\not\\perp^q \\Leftrightarrow p\\text{在}q\\text{上不分叉}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 强极小集合 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_DEFINABLE_SET};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_STRONG_MINIMAL,
                                        "强极小集合：判定可定义集合是否强极小",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "D\\text{强极小} \\Leftrightarrow D\\text{极小且} acl(A \\cap D) = A",
                                        "O(n)",
                                        false, false);
    }

    /* ============================================================
     * 第六部分：量词消去
     * ============================================================
     *
     * 量词消去是模型论的核心算法技术。
     */

    /* -------------------- 量词消去 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_FORMULA};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_QUANTIFIER_ELIMINATION,
                                        "量词消去：将公式φ(x)转换为无量词等价形式",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FORMULA,
                                        "\\exists x \\psi(x,\\bar{y}) \\Rightarrow \\theta(\\bar{y})",
                                        "O(n^2)",
                                        true, false);
    }

    /* -------------------- 模型完备性检验 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_THEORY};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODEL_COMPLETENESS,
                                        "模型完备性：检验理论T是否模型完备",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_BOOLEAN,
                                        "T\\text{模型完备} \\Leftrightarrow \\forall M,N \\models T: M \\subseteq N \\Rightarrow M \\preceq N",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 互模拟判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_STRUCTURE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_BISIMULATION,
                                        "互模拟：判断两个结构是否存在互模拟关系",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "A \\sim B \\Leftrightarrow \\exists I: I\\text{为互模拟关系}",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 可判定性检验 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_THEORY};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_DECIDABILITY,
                                        "可判定性：检验理论T的句子集合是否可判定",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_BOOLEAN,
                                        "Th(M)\\text{可判定} \\Leftrightarrow \\exists\\text{算法判定任意句子}",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 代数性判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_TERM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_ALGEBRAICITY,
                                        "代数性：判断元素a是否代数依赖于A",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "a \\in acl(A) \\Leftrightarrow \\exists \\text{代数公式}\\phi(x,\\bar{a}): M \\models \\phi(a,\\bar{a})",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 传递模型 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_TRANSITIVE_MODEL,
                                        "传递模型：判断结构是否为传递模型",
                                        MODEL_THEORY_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_BOOLEAN,
                                        "M\\text{传递} \\Leftrightarrow (\\forall x \\in M)(x \\subseteq M)",
                                        "O(n)",
                                        false, false);
    }

    /* 模型论模块预设注册完成 */

    (void) MODEL_THEORY_PRESET_COUNT;

    return success_count == MODEL_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取模型论预设函数块数量
 */
int preset_model_theory_count(void) {
    return MODEL_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取模型论模块的预设类别
 */
PresetCategory preset_model_theory_category(void) {
    return PRESET_CATEGORY_MATH_LOGIC;
}

/**
 * @brief 获取模型论预设函数块名称列表
 */
bool preset_model_theory_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv00_malloc(MODEL_THEORY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        /* 结构理论 */
        PRESET_STRUCTURE_SUBSTRUCTURE,
        PRESET_STRUCTURE_EMBEDDING,
        PRESET_STRUCTURE_ISOMORPHISM,
        PRESET_STRUCTURE_ELEMENTARY_EMBEDDING,
        PRESET_STRUCTURE_REDUCTION,
        PRESET_STRUCTURE_EXPANSION,
        PRESET_STRUCTURE_ELIMINATION_DOMAIN,
        PRESET_STRUCTURE_ALGEBRAIC_CLOSURE,
        PRESET_STRUCTURE_DEFINABLE_CLOSURE,
        /* 初等等价 */
        PRESET_ELEMENTARY_EQUIVALENCE,
        PRESET_ELEMENTARY_SUBSTRUCTURE,
        PRESET_TARSKI_VAUGHT_TEST,
        PRESET_SCOTT_ISOMORPHISM,
        PRESET_BACK_FORTH_SYSTEM,
        PRESET_EHRENFEUCHT_GAME,
        /* 模型构造 */
        PRESET_MODEL_CONSTANT_EXTENSION,
        PRESET_MODEL_TYPE_REALIZATION,
        PRESET_MODEL_PRIME_MODEL,
        PRESET_MODEL_SATURATED_MODEL,
        PRESET_MODEL_UNIVERSAL_MODEL,
        PRESET_MODEL_ATOMIC_MODEL,
        PRESET_MODEL_CONSTRUCTION,
        /* 紧致性 */
        PRESET_COMPACTNESS,
        PRESET_POWER_QUANTIFIER,
        PRESET_LINDENBAUM_ALGEBRA,
        PRESET_COMPLETION_CONSTRUCTION,
        PRESET_ULTRAFILTER_EXTENSION,
        PRESET_ULTRAPRODUCT,
        PRESET_ULTRAPOWER,
        /* 稳定性理论 */
        PRESET_STABILITY,
        PRESET_INSTABILITY,
        PRESET_OMEGA_STABILITY,
        PRESET_SUPERSTABLE,
        PRESET_STABLE_TYPE,
        PRESET_FORKING,
        PRESET_STRONG_MINIMAL,
        /* 量词消去 */
        PRESET_QUANTIFIER_ELIMINATION,
        PRESET_MODEL_COMPLETENESS,
        PRESET_BISIMULATION,
        PRESET_DECIDABILITY,
        PRESET_ALGEBRAICITY,
        PRESET_TRANSITIVE_MODEL,
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
