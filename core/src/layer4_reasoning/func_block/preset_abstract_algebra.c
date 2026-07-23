/**
 * @file preset_abstract_algebra.c
 * @brief 抽象代数预设函数块 - 实现
 *
 * @details 实现理论数学研究中常用的抽象代数预设函数块。
 *          所有预设函数块都遵循模块化、确定性原则，
 *          为群论、环论、域论、模论和表示论提供基础运算。
 *
 * 数学基础：
 * - 群论基于抽象代数公理系统
 * - 环论基于模论和理想理论
 * - 域论基于伽罗瓦理论
 * - 模论基于范畴论基础
 * - 表示论基于线性代数和特征标理论
 *
 * @module AbstractAlgebra
 * @category PRESET_CATEGORY_ALGEBRA
 * @version 13.0.0
 */

#include "preset_abstract_algebra.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "preset_name_defs.h"

/* ==================== 兼容宏与常量定义 ==================== */

#define PRESET_REGISTER_CAT_COUNTED(cnt, name, desc, cat, inputs, in_cnt, out, math_def, comp, constructive, reversible) \
    do { \
        if (preset_blocks_register_simple(name, desc, (PresetCategory)(cat), inputs, in_cnt, out, math_def, comp, constructive, reversible)) { \
            (cnt)++; \
        } \
    } while(0)

/* 缺失的 PresetType 兼容定义 */
#define PRESET_TYPE_RING_ELEMENT       PRESET_TYPE_GROUP_ELEMENT
#define PRESET_TYPE_RING_HOMOMORPHISM  PRESET_TYPE_HOMOMORPHISM
#define PRESET_TYPE_DOMAIN             PRESET_TYPE_RING
#define PRESET_TYPE_ALGEBRAIC_ELEMENT  PRESET_TYPE_GROUP_ELEMENT
#define PRESET_TYPE_FIELD_EXTENSION    PRESET_TYPE_EXTENSION
#define PRESET_TYPE_SUBMODULE          PRESET_TYPE_SUBGROUP
#define PRESET_TYPE_MODULE_HOMOMORPHISM PRESET_TYPE_HOMOMORPHISM
#define PRESET_TYPE_REPRESENTATION     PRESET_TYPE_HOMOMORPHISM

/* 群论预设名称兼容 */
#define PRESET_GROUP_CYCLIC_GENERATOR       "group_cyclic_generator"
#define PRESET_GROUP_ELEMENT_ORDER          PRESET_ELEMENT_ORDER
#define PRESET_GROUP_COSET                  "group_coset"
#define PRESET_GROUP_NORMAL_SUBGROUP        "group_normal_subgroup"
#define PRESET_GROUP_QUOTIENT               "group_quotient"
#define PRESET_GROUP_ISOMORPHISM            "group_isomorphism"
#define PRESET_GROUP_AUTOMORPHISM           "group_automorphism"
#define PRESET_GROUP_CONJUGACY_CLASS        PRESET_CONJUGACY_CLASS
#define PRESET_GROUP_CENTRALIZER            "group_centralizer"
#define PRESET_GROUP_COMMUTATOR             "group_commutator"
#define PRESET_GROUP_DERIVED_SUBGROUP       "group_derived_subgroup"
#define PRESET_GROUP_SYLOW_SUBGROUP         "group_sylow_subgroup"

/* 环论预设名称兼容 */
#define PRESET_RING_IDEAL                   "ring_ideal"
#define PRESET_RING_PRINCIPAL_IDEAL         "ring_principal_ideal"
#define PRESET_RING_QUOTIENT                "ring_quotient"
#define PRESET_RING_HOMOMORPHISM_KERNEL     "ring_homomorphism_kernel"
#define PRESET_RING_HOMOMORPHISM_IMAGE      "ring_homomorphism_image"
#define PRESET_RING_PRIME_IDEAL             "ring_prime_ideal"
#define PRESET_RING_MAXIMAL_IDEAL           "ring_maximal_ideal"
#define PRESET_RING_JACOBSON_RADICAL        "ring_jacobson_radical"
#define PRESET_RING_NILRADICAL              "ring_nilradical"
#define PRESET_RING_FRACTION_FIELD          "ring_fraction_field"

/* 域论预设名称兼容 */
#define PRESET_FIELD_EXTENSION_DEGREE       "field_extension_degree"
#define PRESET_FIELD_MINIMAL_POLYNOMIAL     "field_minimal_polynomial"
#define PRESET_FIELD_CONJUGATE              "field_conjugate"
#define PRESET_FIELD_SPLITTING_FIELD        "field_splitting_field"
#define PRESET_FIELD_GALOIS_GROUP           "field_galois_group"
#define PRESET_FIELD_GALOIS_CORRESPONDENCE  "field_galois_correspondence"
#define PRESET_FIELD_SEPARABLE_EXTENSION    "field_separable_extension"
#define PRESET_FIELD_NORMAL_BASIS           "field_normal_basis"

/* 模论预设名称兼容 */
#define PRESET_MODULE_FREE_RANK             "module_free_rank"
#define PRESET_MODULE_SUBMODULE             "module_submodule"
#define PRESET_MODULE_QUOTIENT              "module_quotient"
#define PRESET_MODULE_HOMOMORPHISM_KERNEL   "module_homomorphism_kernel"
#define PRESET_MODULE_HOMOMORPHISM_IMAGE    "module_homomorphism_image"
#define PRESET_MODULE_HOM                   "module_hom"
#define PRESET_MODULE_TENSOR_PRODUCT        "module_tensor_product"
#define PRESET_MODULE_EXACT_SEQUENCE        "module_exact_sequence"

/* 表示论预设名称兼容 */
#define PRESET_REPRESENTATION_GROUP         "representation_group"
#define PRESET_REPRESENTATION_EQUIVALENCE   "representation_equivalence"
#define PRESET_REPRESENTATION_CHARACTER     "representation_character"
#define PRESET_REPRESENTATION_DECOMPOSITION "representation_decomposition"

/* ==================== 预设函数块数量 ==================== */

/** 抽象代数模块预设函数块总数 */
#define ABSTRACT_ALGEBRA_PRESET_COUNT 40

/* ==================== 模块注册实现 ==================== */

/**
 * @brief 抽象代数模块默认类别
 *
 * 使用 PRESET_REGISTER_CAT_COUNTED 宏进行批量注册，
 * 每个预设包含完整的数学定义（LaTeX格式）和复杂度说明。
 */
#define ABSTRACT_ALGEBRA_DEFAULT_CATEGORY PRESET_CATEGORY_ALGEBRA

/**
 * @brief 注册抽象代数模块的所有预设函数块
 *
 * 本函数实现理论数学研究中抽象代数领域的40个核心预设：
 * - 群论运算（14个）：循环群、同构、正规子群、陪集、Sylow子群等
 * - 环论运算（10个）：理想、商环、素理想、极大理想、Jacobson根等
 * - 域论运算（8个）：域扩张、最小多项式、伽罗瓦群、伽罗瓦对应等
 * - 模论运算（5个）：自由模、子模、商模、Hom函子、张量积、正合序列
 * - 表示论基础（4个）：群表示、表示等价、特征标、表示分解
 *
 * @return true 所有预设注册成功
 * @return false 部分失败（记录错误日志但继续）
 */
bool preset_abstract_algebra_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：群论运算
     * ============================================================
     *
     * 群论是抽象代数的基础，研究具有闭合性、结合律、单位元和逆元的代数结构。
     * 本模块涵盖有限群和无限群的核心运算。
     */

    /* -------------------- 循环群生成元计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_INTEGER};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_GROUP_CYCLIC_GENERATOR,
                                        "循环群生成元：计算群G中由元素g生成的循环子群 <g>",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_SUBGROUP,
                                        "\\langle g \\rangle = \\{ g^n : n \\in \\mathbb{Z} \\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 群的阶计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_GROUP_ORDER,
                                        "群的阶：计算有限群G中元素的个数 |G|",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_INTEGER,
                                        "|G| = \\text{群中元素的个数}",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 元素阶计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP_ELEMENT};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_GROUP_ELEMENT_ORDER,
                                        "元素阶：计算群中元素g的阶 ord(g)，即最小的正整数n使 g^n = e",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_INTEGER,
                                        "\\text{ord}(g) = \\min\\{n \\in \\mathbb{Z}^+ : g^n = e\\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 陪集构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SUBGROUP, PRESET_TYPE_GROUP_ELEMENT};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_GROUP_COSET,
                                        "左陪集：构造子群H关于元素g的左陪集 gH",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_SET,
                                        "gH = \\{ gh : h \\in H \\}",
                                        "O(|H|)",
                                        true, false);
    }

    /* -------------------- 正规子群检验 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_GROUP_NORMAL_SUBGROUP,
                                        "正规子群检验：判断子群H是否为群G的正规子群 (gHg^{-1} = H)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "H \\trianglelefteq G \\Leftrightarrow \\forall g \\in G: gHg^{-1} \\subseteq H",
                                        "O(n^2)",
                                        false, false);
    }

    /* -------------------- 商群构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_SUBGROUP};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_GROUP_QUOTIENT,
                                        "商群：构造群G关于正规子群N的商群 G/N",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_GROUP,
                                        "G/N = \\{ gN : g \\in G \\}, \\quad |G/N| = |G|/|N|",
                                        "O(n)",
                                        true, true);
    }

    /* -------------------- 同构判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_GROUP_ISOMORPHISM,
                                        "群同构判定：判断群G和H是否同构 (存在双射保持群运算)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "G \\cong H \\Leftrightarrow \\exists \\phi: G \\to H, \\text{双射且} \\phi(ab) = \\phi(a)\\phi(b)",
                                        "O(n^3)",
                                        false, false);
    }

    /* -------------------- 自同构群计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_GROUP_AUTOMORPHISM,
                                        "自同构群：计算群G的自同构群 Aut(G)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_GROUP,
                                        "\\text{Aut}(G) = \\{ \\phi: G \\to G | \\phi\\text{为双射同构} \\}",
                                        "O(n^2)",
                                        true, false);
    }

    /* -------------------- 共轭类计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_GROUP_CONJUGACY_CLASS,
                                        "共轭类：计算元素g在群G中的共轭类 [g]",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_SET,
                                        "[g] = \\{ hgh^{-1} : h \\in G \\}",
                                        "O(n^2)",
                                        true, false);
    }

    /* -------------------- 中心化子计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP_ELEMENT};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_GROUP_CENTRALIZER,
                                        "中心化子：计算元素g在群G中的中心化子 C_G(g)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_SUBGROUP,
                                        "C_G(g) = \\{ h \\in G : hg = gh \\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 换位子计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP_ELEMENT, PRESET_TYPE_GROUP_ELEMENT};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_GROUP_COMMUTATOR,
                                        "换位子：计算元素a和b的换位子 [a,b] = aba^{-1}b^{-1}",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_GROUP_ELEMENT,
                                        "[a,b] = aba^{-1}b^{-1}",
                                        "O(1)",
                                        true, true);
    }

    /* -------------------- 导群计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_GROUP_DERIVED_SUBGROUP,
                                        "导群：计算群G的导群 G'，即所有换位子生成的子群",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_SUBGROUP,
                                        "G' = \\langle [a,b] : a,b \\in G \\rangle",
                                        "O(n^2)",
                                        true, false);
    }

    /* -------------------- Sylow子群检验 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_INTEGER};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_GROUP_SYLOW_SUBGROUP,
                                        "Sylow子群：计算群G的p-Sylow子群（阶为p^k的极大子群）",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_SUBGROUP,
                                        "\\text{Syl}_p(G) = \\{ P \\leq G : |P| = p^k, p \\nmid [G:P] \\}",
                                        "O(n^2)",
                                        true, false);
    }

    /* ============================================================
     * 第二部分：环论运算
     * ============================================================
     *
     * 环论研究具有加法和乘法两种运算的代数结构，
     * 是代数几何和代数数论的基础。
     */

    /* -------------------- 理想构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING, PRESET_TYPE_LIST};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_RING_IDEAL,
                                        "理想生成：由生成元集合S构造环R的理想 (S)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_IDEAL,
                                        "(S) = \\{ \\sum r_i s_i : r_i \\in R, s_i \\in S \\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 主理想环生成 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING, PRESET_TYPE_RING_ELEMENT};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_RING_PRINCIPAL_IDEAL,
                                        "主理想：构造由元素a生成的主理想 (a)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_IDEAL,
                                        "(a) = \\{ ra : r \\in R \\}",
                                        "O(1)",
                                        true, false);
    }

    /* -------------------- 商环构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING, PRESET_TYPE_IDEAL};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_RING_QUOTIENT,
                                        "商环：构造环R关于理想I的商环 R/I",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_RING,
                                        "R/I = \\{ r+I : r \\in R \\}",
                                        "O(n)",
                                        true, true);
    }

    /* -------------------- 环同态核 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING_HOMOMORPHISM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_RING_HOMOMORPHISM_KERNEL,
                                        "环同态核：计算环同态phi的核 ker(phi)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_IDEAL,
                                        "\\ker(\\phi) = \\{ r \\in R : \\phi(r) = 0 \\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 环同态像 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING_HOMOMORPHISM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_RING_HOMOMORPHISM_IMAGE,
                                        "环同态像：计算环同态phi的像 im(phi)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_RING,
                                        "\\text{im}(\\phi) = \\{ \\phi(r) : r \\in R \\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 素理想检验 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING, PRESET_TYPE_IDEAL};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_RING_PRIME_IDEAL,
                                        "素理想检验：判断理想P是否为环R的素理想",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "P\\text{为素理想} \\Leftrightarrow R/P\\text{为整环}",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 极大理想检验 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING, PRESET_TYPE_IDEAL};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_RING_MAXIMAL_IDEAL,
                                        "极大理想检验：判断理想M是否为环R的极大理想",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "M\\text{为极大理想} \\Leftrightarrow R/M\\text{为域}",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- Jacobson根 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_RING_JACOBSON_RADICAL,
                                        "Jacobson根：计算环R的Jacobson根 J(R)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_IDEAL,
                                        "J(R) = \\bigcap_{\\text{极大理想 } M} M",
                                        "O(n^2)",
                                        true, false);
    }

    /* -------------------- Nilradical -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_RING_NILRADICAL,
                                        "Nilradical：计算环R的幂零根 Nil(R)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_IDEAL,
                                        "\\text{Nil}(R) = \\{ r \\in R : r^n = 0\\text{对某个}n>0 \\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 分式域构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_DOMAIN};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_RING_FRACTION_FIELD,
                                        "分式域：构造整环R的分式域 Q(R)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_FIELD,
                                        "Q(R) = \\{ a/b : a,b \\in R, b \\neq 0 \\}",
                                        "O(1)",
                                        true, true);
    }

    /* ============================================================
     * 第三部分：域论运算
     * ============================================================
     *
     * 域论研究伽罗瓦理论，是数论和代数几何的核心工具。
     */

    /* -------------------- 域扩张次数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_FIELD_EXTENSION_DEGREE,
                                        "域扩张次数：计算扩张 E/F 的次数 [E:F]",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_INTEGER,
                                        "[E:F] = \\dim_F(E)",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 最小多项式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_ALGEBRAIC_ELEMENT};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_FIELD_MINIMAL_POLYNOMIAL,
                                        "最小多项式：计算代数元alpha在域F上的最小多项式 m_alpha(x)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_POLYNOMIAL,
                                        "m_{\\alpha}(x) = \\min\\{ f(x) \\in F[x] : f \\neq 0, f(\\alpha) = 0 \\}",
                                        "O(n^2)",
                                        true, false);
    }

    /* -------------------- 代数元共轭 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_ALGEBRAIC_ELEMENT};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_FIELD_CONJUGATE,
                                        "代数共轭：计算代数元alpha在代数闭包中的所有共轭",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_LIST,
                                        "\\alpha_i = \\sigma_i(\\alpha), \\quad \\sigma_i \\in \\text{Gal}(\\bar{F}/F)",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 分裂域构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_POLYNOMIAL};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_FIELD_SPLITTING_FIELD,
                                        "分裂域：构造多项式f(x)在域F上的分裂域",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FIELD,
                                        "\\text{Spl}(f,F) = F(\\alpha_1,\\ldots,\\alpha_n), f(\\alpha_i)=0",
                                        "O(n^2)",
                                        true, false);
    }

    /* -------------------- 伽罗瓦群计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD_EXTENSION};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_FIELD_GALOIS_GROUP,
                                        "伽罗瓦群：计算域扩张 E/F 的伽罗瓦群 Gal(E/F)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_GROUP,
                                        "\\text{Gal}(E/F) = \\{ \\sigma : E \\to E | \\sigma|_F = \\text{id}, \\sigma\\text{为域同构} \\}",
                                        "O(n!)",
                                        true, false);
    }

    /* -------------------- 伽罗瓦对应 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD_EXTENSION, PRESET_TYPE_SUBGROUP};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_FIELD_GALOIS_CORRESPONDENCE,
                                        "伽罗瓦对应：根据子群H确定对应的中间域 E^H",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_FIELD,
                                        "E^H = \\{ e \\in E : \\sigma(e) = e\\ \\forall \\sigma \\in H \\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 可分扩张检验 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD_EXTENSION};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_FIELD_SEPARABLE_EXTENSION,
                                        "可分扩张检验：判断域扩张 E/F 是否为可分扩张",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_BOOLEAN,
                                        "E/F\\text{可分} \\Leftrightarrow \\forall \\alpha \\in E, m_\\alpha(x)\\text{无重根}",
                                        "O(n^2)",
                                        false, false);
    }

    /* -------------------- 正规基计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD_EXTENSION};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_FIELD_NORMAL_BASIS,
                                        "正规基：计算伽罗瓦扩张 E/F 的正规基",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_LIST,
                                        "\\{ \\sigma_1(\\alpha), \\ldots, \\sigma_n(\\alpha) \\}, \\alpha\\text{为生成元}",
                                        "O(n^2)",
                                        true, false);
    }

    /* ============================================================
     * 第四部分：模论运算
     * ============================================================
     *
     * 模论是环上的模的研究，是同调代数的基础。
     */

    /* -------------------- 自由模秩计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODULE_FREE_RANK,
                                        "自由模秩：计算自由R-模M的秩 rank(M)",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_INTEGER,
                                        "\\text{rank}(M) = n \\text{ 若 } M \\cong R^n",
                                        "O(n)",
                                        false, false);
    }

    /* -------------------- 子模构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE, PRESET_TYPE_LIST};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODULE_SUBMODULE,
                                        "子模生成：由生成元集合S构造M的子模",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_SUBMODULE,
                                        "\\langle S \\rangle_R = \\{ \\sum r_i s_i : r_i \\in R, s_i \\in S \\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 商模构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE, PRESET_TYPE_SUBMODULE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODULE_QUOTIENT,
                                        "商模：构造模M关于子模N的商模 M/N",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_MODULE,
                                        "M/N = \\{ m+N : m \\in M \\}",
                                        "O(n)",
                                        true, true);
    }

    /* -------------------- 模同态核 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE_HOMOMORPHISM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODULE_HOMOMORPHISM_KERNEL,
                                        "模同态核：计算模同态phi的核",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_SUBMODULE,
                                        "\\ker(\\phi) = \\{ m \\in M : \\phi(m) = 0 \\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 模同态像 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE_HOMOMORPHISM};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODULE_HOMOMORPHISM_IMAGE,
                                        "模同态像：计算模同态phi的像",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_MODULE,
                                        "\\text{im}(\\phi) = \\{ \\phi(m) : m \\in M \\}",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- Hom函子 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE, PRESET_TYPE_MODULE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODULE_HOM,
                                        "Hom函子：计算Hom_R(M,N)，即M到N的R-模同态全体",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_MODULE,
                                        "\\text{Hom}_R(M,N) = \\{ \\phi : M \\to N | \\phi(rx) = r\\phi(x) \\}",
                                        "O(n^2)",
                                        true, false);
    }

    /* -------------------- 张量积 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE, PRESET_TYPE_MODULE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODULE_TENSOR_PRODUCT,
                                        "张量积：计算模M和N的张量积 M \\otimes_ R N",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_MODULE,
                                        "M \\otimes_R N = (M \\times N)/\\sim, \\text{双线性性}",
                                        "O(n^2)",
                                        true, false);
    }

    /* -------------------- 正合序列检验 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_MODULE_EXACT_SEQUENCE,
                                        "正合序列检验：验证序列 0->A->B->C->0 是否为正合",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_BOOLEAN,
                                        "0 \\to A \\xrightarrow{f} B \\xrightarrow{g} C \\to 0\\text{ 正合} \\Leftrightarrow \\text{im}(f) = \\ker(g)",
                                        "O(n)",
                                        false, false);
    }

    /* ============================================================
     * 第五部分：表示论基础
     * ============================================================
     *
     * 表示论研究群和代数在向量空间上的作用，是理论物理的重要工具。
     */

    /* -------------------- 群表示构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP, PRESET_TYPE_MATRIX};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_REPRESENTATION_GROUP,
                                        "群表示：构造群G的线性表示（群同态到GL_n(C)）",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_HOMOMORPHISM,
                                        "\\rho: G \\to GL_n(\\mathbb{C}), \\quad \\rho(gh) = \\rho(g)\\rho(h)",
                                        "O(n^2)",
                                        true, true);
    }

    /* -------------------- 表示等价判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_REPRESENTATION, PRESET_TYPE_REPRESENTATION};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_REPRESENTATION_EQUIVALENCE,
                                        "表示等价判定：判断两个群表示是否等价",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "\\rho \\sim \\sigma \\Leftrightarrow \\exists P: \\rho(g) = P^{-1}\\sigma(g)P",
                                        "O(n^3)",
                                        false, false);
    }

    /* -------------------- 特征标计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_REPRESENTATION};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_REPRESENTATION_CHARACTER,
                                        "特征标：计算群表示rho的特征标 chi(g) = Tr(rho(g))",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_FUNCTION,
                                        "\\chi_\\rho(g) = \\text{Tr}(\\rho(g))",
                                        "O(n)",
                                        true, false);
    }

    /* -------------------- 表示分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_REPRESENTATION};
        PRESET_REGISTER_CAT_COUNTED(success_count,
                                        PRESET_REPRESENTATION_DECOMPOSITION,
                                        "表示分解：将群表示分解为不可约表示的直和",
                                        ABSTRACT_ALGEBRA_DEFAULT_CATEGORY,
                                        inputs, 1,
                                        PRESET_TYPE_LIST,
                                        "\\rho = m_1\\rho_1 \\oplus \\cdots \\oplus m_k\\rho_k",
                                        "O(n^2)",
                                        true, false);
    }

    /* 抽象代数模块预设注册完成 */

    /* 验证注册数量（用于调试，生产环境可移除） */
    (void) ABSTRACT_ALGEBRA_PRESET_COUNT;

    /* 返回是否所有预设都注册成功 */
    return success_count == ABSTRACT_ALGEBRA_PRESET_COUNT;
}

/**
 * @brief 获取抽象代数预设函数块数量
 *
 * @return int 抽象代数模块预设函数块总数
 */
int preset_abstract_algebra_count(void) {
    return ABSTRACT_ALGEBRA_PRESET_COUNT;
}

/**
 * @brief 获取抽象代数模块的预设类别
 *
 * @return PresetCategory 抽象代数模块所属类别
 */
PresetCategory preset_abstract_algebra_category(void) {
    return PRESET_CATEGORY_ALGEBRA;
}

/**
 * @brief 获取抽象代数预设函数块名称列表
 *
 * @param out_names 输出名称数组（需调用者释放）
 * @param out_count 输出名称数量
 * @return true 获取成功
 * @return false 参数无效或内存不足
 */
bool preset_abstract_algebra_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    /* 分配名称数组 */
    char **names = (char **) lv_malloc(ABSTRACT_ALGEBRA_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 群论运算 */
        PRESET_GROUP_CYCLIC_GENERATOR,
        PRESET_GROUP_ORDER,
        PRESET_GROUP_ELEMENT_ORDER,
        PRESET_GROUP_COSET,
        PRESET_GROUP_NORMAL_SUBGROUP,
        PRESET_GROUP_QUOTIENT,
        PRESET_GROUP_ISOMORPHISM,
        PRESET_GROUP_AUTOMORPHISM,
        PRESET_GROUP_CONJUGACY_CLASS,
        PRESET_GROUP_CENTRALIZER,
        PRESET_GROUP_COMMUTATOR,
        PRESET_GROUP_DERIVED_SUBGROUP,
        PRESET_GROUP_SYLOW_SUBGROUP,
        /* 环论运算 */
        PRESET_RING_IDEAL,
        PRESET_RING_PRINCIPAL_IDEAL,
        PRESET_RING_QUOTIENT,
        PRESET_RING_HOMOMORPHISM_KERNEL,
        PRESET_RING_HOMOMORPHISM_IMAGE,
        PRESET_RING_PRIME_IDEAL,
        PRESET_RING_MAXIMAL_IDEAL,
        PRESET_RING_JACOBSON_RADICAL,
        PRESET_RING_NILRADICAL,
        PRESET_RING_FRACTION_FIELD,
        /* 域论运算 */
        PRESET_FIELD_EXTENSION_DEGREE,
        PRESET_FIELD_MINIMAL_POLYNOMIAL,
        PRESET_FIELD_CONJUGATE,
        PRESET_FIELD_SPLITTING_FIELD,
        PRESET_FIELD_GALOIS_GROUP,
        PRESET_FIELD_GALOIS_CORRESPONDENCE,
        PRESET_FIELD_SEPARABLE_EXTENSION,
        PRESET_FIELD_NORMAL_BASIS,
        /* 模论运算 */
        PRESET_MODULE_FREE_RANK,
        PRESET_MODULE_SUBMODULE,
        PRESET_MODULE_QUOTIENT,
        PRESET_MODULE_HOMOMORPHISM_KERNEL,
        PRESET_MODULE_HOMOMORPHISM_IMAGE,
        PRESET_MODULE_HOM,
        PRESET_MODULE_TENSOR_PRODUCT,
        PRESET_MODULE_EXACT_SEQUENCE,
        /* 表示论基础 */
        PRESET_REPRESENTATION_GROUP,
        PRESET_REPRESENTATION_EQUIVALENCE,
        PRESET_REPRESENTATION_CHARACTER,
        PRESET_REPRESENTATION_DECOMPOSITION,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                char *p = names[j];
                lv_free((void **) &p);
                names[j] = NULL;
            }
            lv_free((void **) &names);
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
