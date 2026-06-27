/**
 * @file preset_representation_theory.c
 * @brief 表示论预设函数块 - 实现
 *
 * 实现理论数学研究中常用的表示论运算预设函数块。
 * 涵盖群表示、特征标理论、不可约表示、诱导表示和李代数表示五大领域。
 * 共18个预设函数块，均遵循模块化、确定性原则。
 *
 * 采用统一的 preset_blocks_register_simple 注册接口，
 * 使用 REGISTER_RT 宏模式简化注册代码。
 *
 * @module RepresentationTheory
 * @category PRESET_CATEGORY_GROUP_THEORY
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "preset_representation_theory.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 表示论模块预设函数块总数（与头文件中 REPRESENTATION_THEORY_PRESET_COUNT 一致） */
#define RT_PRESET_COUNT REPRESENTATION_THEORY_PRESET_COUNT

/* ==================== REGISTER_RT 宏定义 ==================== */

/**
 * @brief 注册单个表示论预设的便捷宏
 *
 * 封装 preset_blocks_register_simple 调用，简化注册代码。
 * 所有表示论预设使用 PRESET_CATEGORY_GROUP_THEORY 类别。
 *
 * @param preset_name   预设名称常量（头文件中定义的宏）
 * @param desc          中文描述
 * @param inputs        输入类型数组（PresetType 复合字面量）
 * @param n_inputs      输入数量
 * @param output        输出类型
 * @param math          数学定义（LaTeX 格式）
 * @param comp          时间复杂度
 * @param constructive  是否构造性
 * @param reversible    是否可逆
 */
#define REGISTER_RT(preset_name, desc, n_inputs, output, math, comp, constructive, reversible, ...) \
    do { \
        PresetType _in[] = { __VA_ARGS__ }; \
        if (register_rt_preset(preset_name, desc, _in, n_inputs, output, math, comp, constructive, reversible)) { \
            success_count++; \
        } \
    } while (0)

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个表示论预设
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
static bool register_rt_preset(
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
        PRESET_CATEGORY_GROUP_THEORY,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_representation_theory_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：群表示（6个）
     * ============================================================ */

    /**
     * @brief 线性表示
     *
     * @details 构造群 G 在向量空间 V 上的线性表示，即群同态 rho: G -> GL(V)。
     *          线性表示是表示论的核心研究对象，
     *          通过将抽象的群元素对应为线性变换来研究群的结构。
     * @param 群结构（PRESET_TYPE_GROUP）和向量空间（PRESET_TYPE_SPACE）
     * @return 表示映射（PRESET_TYPE_HOMOMORPHISM）
     * @math \rho: G \to GL(V), \quad \rho(gh) = \rho(g)\rho(h), \quad \rho(e) = I_V
     * @complexity O(|G|·n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_LINEAR_REPRESENTATION,
        "线性表示：构造群 G 在向量空间 V 上的群同态 rho: G -> GL(V)，用线性变换刻画群结构",
        2, PRESET_TYPE_HOMOMORPHISM,
        "\\rho: G \\to GL(V), \\quad "
        "\\rho(gh) = \\rho(g)\\rho(h), \\quad "
        "\\rho(e) = I_V",
        "O(|G| \\cdot n^2)", true, false,
        PRESET_TYPE_GROUP, PRESET_TYPE_SPACE);

    /**
     * @brief 置换表示
     *
     * @details 由群 G 在有限集合 X 上的作用构造的置换表示。
     *          对每个 g ∈ G 构造置换矩阵 P(g)，
     *          维数等于 |X|，是研究群论与组合学联系的基本工具。
     * @param 群（PRESET_TYPE_GROUP）和有限集合（PRESET_TYPE_SET）
     * @return 置换表示矩阵组（PRESET_TYPE_LIST）
     * @math P_g: \mathbb{C}X \to \mathbb{C}X, \quad P_g(e_x) = e_{g\cdot x}
     * @complexity O(|G|·|X|^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_PERMUTATION_REP,
        "置换表示：由群 G 在有限集 X 上的作用构造维数为 |X| 的置换矩阵表示",
        2, PRESET_TYPE_LIST,
        "P_g: \\mathbb{C}X \\to \\mathbb{C}X, \\quad "
        "P_g(e_x) = e_{g\\cdot x}",
        "O(|G| \\cdot |X|^2)", true, false,
        PRESET_TYPE_GROUP, PRESET_TYPE_SET);

    /**
     * @brief 正则表示
     *
     * @details 构造群 G 在群代数 C[G] 上的左正则表示。
     *          正则表示是有限群最重要的表示之一，
     *          其维数为 |G|，每个不可约表示出现的次数等于其维数。
     * @param 群结构（PRESET_TYPE_GROUP）
     * @return 正则表示（PRESET_TYPE_HOMOMORPHISM）
     * @math \lambda: G \to GL(\mathbb{C}G), \quad \lambda(g)(h) = gh, \quad \mathbb{C}G \cong \bigoplus_\rho (\dim\rho)\rho
     * @complexity O(|G|^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_REGULAR_REPRESENTATION,
        "正则表示：构造群代数上的左平移表示，每个不可约表示出现 dim(rho) 次",
        1, PRESET_TYPE_HOMOMORPHISM,
        "\\lambda: G \\to GL(\\mathbb{C}G), \\quad "
        "\\lambda(g)(h) = gh, \\quad "
        "\\mathbb{C}G \\cong \\bigoplus_\\rho (\\dim\\rho)\\rho",
        "O(|G|^2)", true, false,
        PRESET_TYPE_GROUP);

    /**
     * @brief 酉表示
     *
     * @details 在复 Hilbert 空间上构造酉（Unitary）表示。
     *          酉表示是无限维表示论和量子力学的基础，
     *          有限群的任何复表示均可酉化（通过取平均内积）。
     * @param 表示（PRESET_TYPE_HOMOMORPHISM）
     * @return 酉表示（PRESET_TYPE_HOMOMORPHISM）
     * @math \rho: G \to U(\mathcal{H}), \quad \langle\rho(g)v,\rho(g)w\rangle = \langle v,w\rangle
     * @complexity O(|G|·n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_UNITARY_REPRESENTATION,
        "酉表示：在 Hilbert 空间上构造保持内积的酉表示 rho: G -> U(H)（可酉化）",
        1, PRESET_TYPE_HOMOMORPHISM,
        "\\rho: G \\to U(\\mathcal{H}), \\quad "
        "\\langle\\rho(g)v,\\rho(g)w\\rangle = \\langle v,w\\rangle",
        "O(|G| \\cdot n^2)", true, false,
        PRESET_TYPE_HOMOMORPHISM);

    /**
     * @brief 表示维数
     *
     * @details 计算给定线性表示 rho: G -> GL(V) 的维数，即向量空间 V 的维数。
     *          表示的维数是表示最基础的数值不变量。
     * @param 表示（PRESET_TYPE_HOMOMORPHISM）
     * @return 表示维数（PRESET_TYPE_INTEGER）
     * @math \dim\rho = \dim V = \chi_\rho(e)
     * @complexity O(1)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_REPRESENTATION_DIMENSION,
        "表示维数：计算线性表示 rho: G -> GL(V) 的维数 dim(rho) = dim V = chi(e)",
        1, PRESET_TYPE_INTEGER,
        "\\dim\\rho = \\dim V = \\chi_\\rho(e)",
        "O(1)", true, false,
        PRESET_TYPE_HOMOMORPHISM);

    /**
     * @brief 表示的直和与张量积
     *
     * @details 从两个已知表示构造其直和表示（rho1 ⊕ rho2）和张量积表示（rho1 ⊗ rho2）。
     *          直和的维数是维数之和，张量积的维数是维数之积，
     *          这些构造使得 Rep(G) 形成张量范畴。
     * @param 两个表示（PRESET_TYPE_HOMOMORPHISM, PRESET_TYPE_HOMOMORPHISM）和操作类型（PRESET_TYPE_BOOLEAN）
     * @return 新表示（PRESET_TYPE_HOMOMORPHISM）
     * @math (\rho_1 \oplus \rho_2)(g) = \begin{pmatrix} \rho_1(g) & 0 \\ 0 & \rho_2(g) \end{pmatrix}, \quad (\rho_1 \otimes \rho_2)(g) = \rho_1(g) \otimes \rho_2(g)
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_REP_DIRECT_SUM_TENSOR,
        "表示的直和与张量积：构造函数 rho1⊕rho2 (维数相加) 或 rho1⊗rho2 (维数相乘)",
        3, PRESET_TYPE_HOMOMORPHISM,
        "(\\rho_1 \\oplus \\rho_2)(g) = \\begin{pmatrix} "
        "\\rho_1(g) & 0 \\\\ 0 & \\rho_2(g) \\end{pmatrix}, \\quad "
        "(\\rho_1 \\otimes \\rho_2)(g) = \\rho_1(g) \\otimes \\rho_2(g)",
        "O(n^2)", true, false,
        PRESET_TYPE_HOMOMORPHISM, PRESET_TYPE_HOMOMORPHISM, PRESET_TYPE_BOOLEAN);

    /* ============================================================
     * 第二部分：特征标理论（4个）
     * ============================================================ */

    /**
     * @brief 特征标计算
     *
     * @details 计算表示的特征标 chi_rho: G -> C，定义为 chi_rho(g) = Tr(rho(g))。
     *          特征标是表示的"指纹"——两个表示等价当且仅当特征标相同。
     *          特征标是类函数（在共轭类上取常值）。
     * @param 表示（PRESET_TYPE_HOMOMORPHISM）
     * @return 特征标（PRESET_TYPE_FUNCTION）
     * @math \chi_\rho(g) = \operatorname{Tr}(\rho(g))
     * @complexity O(|G|·n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_CHARACTER_CALCULATION,
        "特征标计算：对表示 rho 计算特征标函数 chi(g) = Tr(rho(g))（等价不变量）",
        1, PRESET_TYPE_FUNCTION,
        "\\chi_\\rho(g) = \\operatorname{Tr}(\\rho(g))",
        "O(|G| \\cdot n^2)", true, false,
        PRESET_TYPE_HOMOMORPHISM);

    /**
     * @brief 特征标表
     *
     * @details 构造有限群的不可约特征标表（行：不可约表示，列：共轭类）。
     *          特征标表是有限群表示论的"周期表"，
     *          行满足正交关系，行数等于共轭类数。
     * @param 群结构（PRESET_TYPE_GROUP）
     * @return 特征标表矩阵（PRESET_TYPE_MATRIX）
     * @math \chi_{i}(C_j) = \operatorname{Tr}(\rho_i(g_j)), \quad g_j \in C_j
     * @complexity O(|G|^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_CHARACTER_TABLE,
        "特征标表：构造有限群的不可约特征标表（行=不可约表示，列=共轭类），行正交",
        1, PRESET_TYPE_MATRIX,
        "\\chi_{i}(C_j) = \\operatorname{Tr}(\\rho_i(g_j)), \\quad "
        "\\sum_{g\\in G} \\chi_i(g)\\overline{\\chi_j(g)} = |G|\\,\\delta_{ij}",
        "O(|G|^2)", true, false,
        PRESET_TYPE_GROUP);

    /**
     * @brief 特征标内积
     *
     * @details 计算两个类函数（特征标）的内积。
     *          特征标内积是正交性和不可约判定的基础：
     *          不可约特征标满足 (chi_i, chi_j) = delta_{ij}。
     * @param 两个特征标（PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION）和群（PRESET_TYPE_GROUP）
     * @return 内积值（PRESET_TYPE_SCALAR）
     * @math \langle\chi, \psi\rangle_G = \frac{1}{|G|}\sum_{g\in G} \chi(g)\overline{\psi(g)}
     * @complexity O(|G|)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_CHARACTER_INNER_PRODUCT,
        "特征标内积：计算类函数的内积 (chi, psi)_G = 1/|G| ∑ chi(g)·conj(psi(g))",
        3, PRESET_TYPE_SCALAR,
        "\\langle\\chi, \\psi\\rangle_G = "
        "\\frac{1}{|G|}\\sum_{g\\in G} \\chi(g)\\overline{\\psi(g)}",
        "O(|G|)", true, false,
        PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_GROUP);

    /**
     * @brief 不可约特征标正交性
     *
     * @details 验证不可约特征标的两种正交关系：
     *          行正交关系：sum_{g∈G} chi_i(g)·conj(chi_j(g)) = |G|·delta_{ij}
     *          列正交关系：sum_{chi} chi(C_i)·conj(chi(C_j)) = |G|/|C_i|·delta_{ij}
     * @param 特征标表（PRESET_TYPE_MATRIX）和群（PRESET_TYPE_GROUP）
     * @return 是否满足正交性（PRESET_TYPE_BOOLEAN）
     * @math \sum_{g\in G} \chi_i(g)\overline{\chi_j(g)} = |G|\,\delta_{ij}
     * @complexity O(|G|^2)
     * @constructive 否
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_CHARACTER_ORTHOGONALITY,
        "不可约特征标正交性：验证 Schur 正交关系 sum χ_i(g)·conj(χ_j(g)) = |G|·δ_ij",
        2, PRESET_TYPE_BOOLEAN,
        "\\sum_{g\\in G} \\chi_i(g)\\overline{\\chi_j(g)} = |G|\\,\\delta_{ij}, \\quad "
        "\\sum_{\\chi} \\chi(C_i)\\overline{\\chi(C_j)} = \\frac{|G|}{|C_i|}\\,\\delta_{ij}",
        "O(|G|^2)", false, false,
        PRESET_TYPE_MATRIX, PRESET_TYPE_GROUP);

    /* ============================================================
     * 第三部分：不可约表示（4个）
     * ============================================================ */

    /**
     * @brief 不可约表示判定
     *
     * @details 判定给定表示是否不可约（无真不变子空间）。
     *          利用特征标内积：(chi, chi) = 1 当且仅当表示不可约（复情况）。
     *          这是有限群表示论中最重要的判定准则。
     * @param 表示（PRESET_TYPE_HOMOMORPHISM）和群（PRESET_TYPE_GROUP）
     * @return 是否不可约（PRESET_TYPE_BOOLEAN）
     * @math \rho \text{ 不可约} \Leftrightarrow \langle\chi_\rho, \chi_\rho\rangle = 1
     * @complexity O(|G|·n^2)
     * @constructive 否
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_IRREDUCIBILITY_TEST,
        "不可约表示判定：验证表示是否不可约（特征标内积 (chi, chi) = 1 则不可约）",
        2, PRESET_TYPE_BOOLEAN,
        "\\rho \\text{ 不可约} \\Leftrightarrow "
        "\\langle\\chi_\\rho, \\chi_\\rho\\rangle_G = 1",
        "O(|G| \\cdot n^2)", false, false,
        PRESET_TYPE_HOMOMORPHISM, PRESET_TYPE_GROUP);

    /**
     * @brief Maschke定理
     *
     * @details 验证并应用 Maschke 定理：
     *          有限群在特征不整除 |G| 的域上的表示是完全可约的。
     *          即任何不变子空间都有不变补空间。
     * @param 表示（PRESET_TYPE_HOMOMORPHISM）和域特征（PRESET_TYPE_INTEGER）
     * @return 是否完全可约（PRESET_TYPE_BOOLEAN）
     * @math \operatorname{char}(F) \nmid |G| \Rightarrow \rho = \bigoplus_i \rho_i \text{（完全可约）}
     * @complexity O(|G|·n^2)
     * @constructive 否
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_MASCHKE_THEOREM,
        "Maschke定理：验证有限群在 char(F)∤|G| 的域上的表示完全可约",
        2, PRESET_TYPE_BOOLEAN,
        "\\operatorname{char}(F) \\nmid |G| \\Rightarrow "
        "\\rho = \\bigoplus_i \\rho_i",
        "O(|G| \\cdot n^2)", false, false,
        PRESET_TYPE_HOMOMORPHISM, PRESET_TYPE_INTEGER);

    /**
     * @brief 不可约表示分解
     *
     * @details 将可约表示分解为不可约表示的直和。
     *          利用特征标的内积公式计算每个不可约表示出现的重数：
     *          (chi_rho, chi_i) = 重数。
     * @param 表示（PRESET_TYPE_HOMOMORPHISM）和群（PRESET_TYPE_GROUP）
     * @return 分解结果（PRESET_TYPE_LIST）
     * @math \rho \cong \bigoplus_i d_i \rho_i, \quad d_i = \langle\chi_\rho, \chi_i\rangle
     * @complexity O(|G|^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_IRREDUCIBLE_DECOMPOSITION,
        "不可约表示分解：将可约表示分解为不可约表示的直和 rho ≅ ⊕ d_i·rho_i，d_i = (chi, chi_i)",
        2, PRESET_TYPE_LIST,
        "\\rho \\cong \\bigoplus_i d_i\\,\\rho_i, \\quad "
        "d_i = \\langle\\chi_\\rho, \\chi_i\\rangle_G",
        "O(|G|^2)", true, false,
        PRESET_TYPE_HOMOMORPHISM, PRESET_TYPE_GROUP);

    /**
     * @brief Schur引理
     *
     * @details Schur引理断言：不可约表示之间的 intertwiner
     *          Hom_G(rho1, rho2) 要么是零空间（rho1 不等价于 rho2），
     *          要么是一维空间（等价时，由标量乘子构成）。
     *          这是表示论中最基本的引理。
     * @param 两个表示（PRESET_TYPE_HOMOMORPHISM, PRESET_TYPE_HOMOMORPHISM）和群（PRESET_TYPE_GROUP）
     * @return Intertwiner空间维数（PRESET_TYPE_INTEGER）
     * @math \dim\operatorname{Hom}_G(\rho_1, \rho_2) = \begin{cases} 1, & \rho_1 \cong \rho_2 \text{（不可约）} \\ 0, & \text{否则} \end{cases}
     * @complexity O(n^2)
     * @constructive 否
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_SCHURS_LEMMA,
        "Schur引理：计算 Intertwiner 空间 Hom_G(rho1, rho2) 维数（不可约时 0 或 1）",
        3, PRESET_TYPE_INTEGER,
        "\\dim\\operatorname{Hom}_G(\\rho_1, \\rho_2) = "
        "\\begin{cases} 1, & \\rho_1 \\cong \\rho_2 \\\\ 0, & \\text{否则} \\end{cases}",
        "O(n^2)", false, false,
        PRESET_TYPE_HOMOMORPHISM, PRESET_TYPE_HOMOMORPHISM, PRESET_TYPE_GROUP);

    /* ============================================================
     * 第四部分：诱导表示（2个）
     * ============================================================ */

    /**
     * @brief Frobenius互反律
     *
     * @details Frobenius互反律描述了诱导表示和限制表示之间的对偶关系：
     *          (Ind_H^G chi, psi)_G = (chi, Res_H^G psi)_H。
     *          这是表示论中最优美的公式之一，用于计算诱导特征标的内积。
     * @param 子群表示的特征标（PRESET_TYPE_FUNCTION）和群表示的特征标（PRESET_TYPE_FUNCTION）
     * @return 内积值（PRESET_TYPE_SCALAR）
     * @math \langle\operatorname{Ind}_H^G \chi, \psi\rangle_G = \langle\chi, \operatorname{Res}_H^G \psi\rangle_H
     * @complexity O(|G|)
     * @constructive 否
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_FROBENIUS_RECIPROCITY,
        "Frobenius互反律：(Ind_H^G chi, psi)_G = (chi, Res_H^G psi)_H，诱导与限制对偶",
        2, PRESET_TYPE_SCALAR,
        "\\langle\\operatorname{Ind}_H^G \\chi, \\psi\\rangle_G = "
        "\\langle\\chi, \\operatorname{Res}_H^G \\psi\\rangle_H",
        "O(|G|)", false, false,
        PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION);

    /**
     * @brief 诱导特征标
     *
     * @details 计算从子群 H 诱导到 G 的表示的特征标。
     *          诱导特征标的显式公式为：
     *          Ind_H^G chi(g) = (1/|H|) sum_{x∈G, xgx^{-1}∈H} chi(xgx^{-1})
     * @param 子群特征标（PRESET_TYPE_FUNCTION）、子群（PRESET_TYPE_SUBGROUP）和群（PRESET_TYPE_GROUP）
     * @return 诱导特征标（PRESET_TYPE_FUNCTION）
     * @math \operatorname{Ind}_H^G \chi(g) = \frac{1}{|H|}\sum_{\substack{x\in G \\ xgx^{-1}\in H}} \chi(xgx^{-1})
     * @complexity O(|G|^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_INDUCED_CHARACTER,
        "诱导特征标：计算 Ind_H^G chi(g) = 1/|H|·sum_{x∈G, xgx^{-1}∈H} chi(xgx^{-1})",
        3, PRESET_TYPE_FUNCTION,
        "\\operatorname{Ind}_H^G \\chi(g) = "
        "\\frac{1}{|H|}\\sum_{\\substack{x\\in G \\\\ xgx^{-1}\\in H}} "
        "\\chi(xgx^{-1})",
        "O(|G|^2)", true, false,
        PRESET_TYPE_FUNCTION, PRESET_TYPE_SUBGROUP, PRESET_TYPE_GROUP);

    /* ============================================================
     * 第五部分：李代数表示（2个）
     * ============================================================ */

    /**
     * @brief 伴随表示
     *
     * @details 构造李代数的伴随表示 ad: g -> gl(g)，定义为 ad_X(Y) = [X, Y]。
     *          伴随表示是李代数理论的核心构造，
     *          Killing 形式由伴随表示的迹定义：K(X,Y) = Tr(ad_X ad_Y)。
     * @param 李代数结构（PRESET_TYPE_ALGEBRA）
     * @return 伴随表示（PRESET_TYPE_MATRIX）
     * @math \operatorname{ad}: \mathfrak{g} \to \mathfrak{gl}(\mathfrak{g}), \quad \operatorname{ad}_X(Y) = [X, Y]
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_RT(PRESET_RT_ADJOINT_REPRESENTATION,
        "伴随表示：构造李代数的伴随表示 ad_X(Y) = [X, Y]，Killing 形式 B(X,Y)=Tr(ad_X ad_Y)",
        1, PRESET_TYPE_MATRIX,
        "\\operatorname{ad}: \\mathfrak{g} \\to \\mathfrak{gl}(\\mathfrak{g}), \\quad "
        "\\operatorname{ad}_X(Y) = [X, Y]",
        "O(n^2)", true, true,
        PRESET_TYPE_ALGEBRA);

    /**
     * @brief 最高权表示
     *
     * @details 从统治整权（dominant integral weight）lambda 构造有限维不可约表示。
     *          Cartan-Weyl 理论断言：每个有限维不可约表示都由
     *          唯一的最高权完全决定（在同构意义下）。
     * @param 权向量（PRESET_TYPE_VECTOR）和李代数根系数据（PRESET_TYPE_MATRIX）
     * @return 不可约表示（PRESET_TYPE_HOMOMORPHISM）
     * @math V_\lambda \text{ 由最高权 } \lambda \in P^+ \text{ 唯一决定}, \quad \dim V_\lambda = \prod_{\alpha>0} \frac{\langle\lambda+\rho,\alpha^\vee\rangle}{\langle\rho,\alpha^\vee\rangle}
     * @complexity O(n^3)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_RT(PRESET_RT_HIGHEST_WEIGHT_REP,
        "最高权表示：从统治整权 lambda 构造有限维不可约表示 V_lambda（Cartan-Weyl理论）",
        2, PRESET_TYPE_HOMOMORPHISM,
        "V_\\lambda \\text{ 由 } \\lambda \\in P^+ \\text{ 唯一决定}, \\quad "
        "\\dim V_\\lambda = \\prod_{\\alpha>0} "
        "\\frac{\\langle\\lambda+\\rho,\\alpha^\\vee\\rangle}"
        "{\\langle\\rho,\\alpha^\\vee\\rangle}",
        "O(n^3)", true, false,
        PRESET_TYPE_VECTOR, PRESET_TYPE_MATRIX);

    /* 返回是否所有预设都注册成功 */
    if (success_count == RT_PRESET_COUNT) {
        /* lv00_log_info("表示论模块注册成功：%d/%d 个预设", success_count, RT_PRESET_COUNT) */
        return true;
    }

    /* lv00_log_info("表示论模块注册部分失败：%d/%d 个预设", success_count, RT_PRESET_COUNT) */
    return false;
}

/**
 * @brief 获取表示论预设函数块数量
 *
 * @return int 表示论模块预设函数块总数（18）
 */
int preset_representation_theory_count(void)
{
    return RT_PRESET_COUNT;
}

/**
 * @brief 获取表示论预设的类别
 *
 * @return PresetCategory 预设类别（PRESET_CATEGORY_GROUP_THEORY）
 */
PresetCategory preset_representation_theory_category(void)
{
    return PRESET_CATEGORY_GROUP_THEORY;
}

/**
 * @brief 获取表示论预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_representation_theory_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    char **names = (char**)lv00_malloc(RT_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

    const char *preset_names[] = {
        /* 群表示 */
        PRESET_RT_LINEAR_REPRESENTATION,
        PRESET_RT_PERMUTATION_REP,
        PRESET_RT_REGULAR_REPRESENTATION,
        PRESET_RT_UNITARY_REPRESENTATION,
        PRESET_RT_REPRESENTATION_DIMENSION,
        PRESET_RT_REP_DIRECT_SUM_TENSOR,
        /* 特征标理论 */
        PRESET_RT_CHARACTER_CALCULATION,
        PRESET_RT_CHARACTER_TABLE,
        PRESET_RT_CHARACTER_INNER_PRODUCT,
        PRESET_RT_CHARACTER_ORTHOGONALITY,
        /* 不可约表示 */
        PRESET_RT_IRREDUCIBILITY_TEST,
        PRESET_RT_MASCHKE_THEOREM,
        PRESET_RT_IRREDUCIBLE_DECOMPOSITION,
        PRESET_RT_SCHURS_LEMMA,
        /* 诱导表示 */
        PRESET_RT_FROBENIUS_RECIPROCITY,
        PRESET_RT_INDUCED_CHARACTER,
        /* 李代数表示 */
        PRESET_RT_ADJOINT_REPRESENTATION,
        PRESET_RT_HIGHEST_WEIGHT_REP,
    };

    int count = (int)(sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) lv00_free((void**)&names[j]);
            lv00_free((void**)&names);
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
