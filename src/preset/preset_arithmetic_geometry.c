/**
 * @file preset_arithmetic_geometry.c
 * @brief 算术几何预设函数块 - 实现
 *
 * @details 实现算术几何模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共25个预设，涵盖椭圆曲线、模形式、Diophantine方程、
 *          代数数论、p-adic分析和有理点理论。
 *
 * @module ArithmeticGeometry
 * @category PRESET_CATEGORY_NUMBER_THEORY
 * @version 1.0.0
 */

#include "preset_arithmetic_geometry.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 算术几何模块预设函数块总数 */
#define ARITHMETIC_GEOMETRY_PRESET_COUNT 25

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个算术几何预设
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义（LaTeX 格式）
 * @param complexity 时间复杂度描述
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_ag_preset(
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
        PRESET_CATEGORY_NUMBER_THEORY,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_arithmetic_geometry_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：椭圆曲线（6个预设）
     *
     * 椭圆曲线是算术几何的核心对象，定义为亏格1的光滑射影曲线。
     * 其有理点构成有限生成的Abel群（Mordell-Weil定理）。
     * ============================================================ */

    /* Weierstrass 标准形式 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_ag_preset(
                PRESET_AG_WEIERSTRASS_FORM,
                "Weierstrass 标准形式：y² = x³ + ax + b，判别式 Δ = -16(4a³ + 27b²) ≠ 0",
                inputs, 3, PRESET_TYPE_EQUATION,
                "y^2 = x^3 + ax + b, \\quad "
                "\\Delta = -16(4a^3 + 27b^2) \\neq 0, \\quad "
                "j = 1728\\frac{4a^3}{4a^3 + 27b^2}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 椭圆曲线群运算 */
    {
        PresetType inputs[] = {PRESET_TYPE_TUPLE, PRESET_TYPE_TUPLE, PRESET_TYPE_EQUATION};
        if (register_ag_preset(
                PRESET_AG_ELLIPTIC_ADD,
                "椭圆曲线群运算：P + Q，弦切法定义加法",
                inputs, 3, PRESET_TYPE_TUPLE,
                "P + Q = R, \\quad "
                "\\lambda = \\frac{y_Q - y_P}{x_Q - x_P}, \\quad "
                "x_R = \\lambda^2 - x_P - x_Q, \\quad "
                "y_R = \\lambda(x_P - x_R) - y_P",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 椭圆曲线点加倍 */
    {
        PresetType inputs[] = {PRESET_TYPE_TUPLE, PRESET_TYPE_EQUATION};
        if (register_ag_preset(
                PRESET_AG_ELLIPTIC_DOUBLE,
                "椭圆曲线点加倍：2P，切线法定义",
                inputs, 2, PRESET_TYPE_TUPLE,
                "2P = R, \\quad "
                "\\lambda = \\frac{3x_P^2 + a}{2y_P}, \\quad "
                "x_R = \\lambda^2 - 2x_P, \\quad "
                "y_R = \\lambda(x_P - x_R) - y_P",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 椭圆曲线标量乘法 */
    {
        PresetType inputs[] = {PRESET_TYPE_TUPLE, PRESET_TYPE_INTEGER, PRESET_TYPE_EQUATION};
        if (register_ag_preset(
                PRESET_AG_ELLIPTIC_SCALAR,
                "椭圆曲线标量乘法：nP = P + P + ... + P（n次）",
                inputs, 3, PRESET_TYPE_TUPLE,
                "nP = \\underbrace{P + P + \\cdots + P}_{n \\text{次}}, \\quad "
                "\\text{double-and-add 算法}, \\quad "
                "\\text{复杂度: } O(\\log n)",
                "O(log n)", true, false)) {
            success_count++;
        }
    }

    /* 椭圆曲线判别式与 j 不变量 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_ag_preset(
                PRESET_AG_ELLIPTIC_INVARIANTS,
                "椭圆曲线判别式 Δ 与 j-不变量：曲线的同构类由 j 唯一确定",
                inputs, 2, PRESET_TYPE_TUPLE,
                "\\Delta = -16(4a^3 + 27b^2), \\quad "
                "j = 1728\\frac{4a^3}{4a^3 + 27b^2}, \\quad "
                "E_1 \\cong E_2 \\Leftrightarrow j(E_1) = j(E_2)",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 椭圆曲线 torsion 点 */
    {
        PresetType inputs[] = {PRESET_TYPE_EQUATION, PRESET_TYPE_INTEGER};
        if (register_ag_preset(
                PRESET_AG_ELLIPTIC_TORSION,
                "椭圆曲线 torsion 点：E[n] = {P : nP = O}，Mazur定理限制可能结构",
                inputs, 2, PRESET_TYPE_SET,
                "E[n] = \\{P \\in E : nP = O\\}, \\quad "
                "\\text{Mazur定理: } E(\\mathbb{Q})_{\\text{tors}} \\cong "
                "\\mathbb{Z}/n\\mathbb{Z}\\ (n \\le 12, n \\ne 11) \\text{ 或 } "
                "\\mathbb{Z}/2\\mathbb{Z} \\times \\mathbb{Z}/2n\\mathbb{Z}\\ (n \\le 4)",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：模形式（4个预设）
     *
     * 模形式是上半平面上的全纯函数，满足特定的变换性质。
     * 椭圆曲线的模参数化是模形式的重要应用。
     * ============================================================ */

    /* 模群作用 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_MATRIX};
        if (register_ag_preset(
                PRESET_AG_MODULAR_GROUP,
                "模群 SL₂(ℤ) 作用：τ ↦ (aτ+b)/(cτ+d)",
                inputs, 2, PRESET_TYPE_SCALAR,
                "\\text{SL}_2(\\mathbb{Z}) = \\left\\{\\begin{pmatrix} a & b \\\\ c & d \\end{pmatrix} : "
                "ad - bc = 1\\right\\}, \\quad "
                "\\gamma \\cdot \\tau = \\frac{a\\tau + b}{c\\tau + d}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* Eisenstein 级数 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_ag_preset(
                PRESET_AG_EISENSTEIN_SERIES,
                "Eisenstein 级数：G_k(τ) = Σ' (mτ+n)^(-k)",
                inputs, 2, PRESET_TYPE_SCALAR,
                "G_k(\\tau) = \\sum_{(m,n) \\ne (0,0)} \\frac{1}{(m\\tau + n)^k}, \\quad "
                "k \\ge 4, \\quad k \\equiv 0 \\pmod{2}, \\quad "
                "G_4^3 - G_6^2 = 1728^{-1}\\Delta",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 模判别式 Δ */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_ag_preset(
                PRESET_AG_MODULAR_DISCRIMINANT,
                "模判别式 Δ(τ) = qΠ(1-qⁿ)²⁴，q = exp(2πiτ)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\Delta(\\tau) = q\\prod_{n=1}^{\\infty}(1 - q^n)^{24}, \\quad "
                "q = e^{2\\pi i\\tau}, \\quad "
                "\\Delta = (2\\pi)^{12}\\left(G_4^3 - G_6^2\\right)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* j-不变量 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_ag_preset(
                PRESET_AG_J_INVARIANT,
                "模 j-不变量：j(τ) = 1728G₄³/(G₄³ - G₆²)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "j(\\tau) = 1728\\frac{G_4^3}{G_4^3 - G_6^2} = "
                "\\frac{1}{q} + 744 + 196884q + \\cdots, \\quad "
                "j(\\tau_1) = j(\\tau_2) \\Leftrightarrow \\tau_1, \\tau_2 \\text{ 模等价}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：Diophantine 方程（4个预设）
     *
     * Diophantine 方程是整数解或 有理数解的多项式方程。
     * 是数论的核心研究对象。
     * ============================================================ */

    /* Pell 方程求解 */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_ag_preset(
                PRESET_AG_PELL_EQUATION,
                "Pell 方程：x² - Dy² = 1，连分数法求最小解",
                inputs, 1, PRESET_TYPE_TUPLE,
                "x^2 - Dy^2 = 1, \\quad D > 0, \\quad "
                "\\sqrt{D} = [a_0; \\overline{a_1, \\ldots, a_k}], \\quad "
                "(x_1, y_1) = \\text{最小解}",
                "O(√D log D)", true, false)) {
            success_count++;
        }
    }

    /* Thue 方程 */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_INTEGER};
        if (register_ag_preset(
                PRESET_AG_THUE_EQUATION,
                "Thue 方程：F(x,y) = m，有限解定理",
                inputs, 2, PRESET_TYPE_LIST,
                "F(x,y) = m, \\quad "
                "F \\in \\mathbb{Z}[x,y] \\text{ 不可约}, \\quad "
                "\\deg F \\ge 3 \\Rightarrow \\text{有限解}",
                "O(n³)", true, false)) {
            success_count++;
        }
    }

    /* Mordell 方程 */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_ag_preset(
                PRESET_AG_MORDELL_EQUATION,
                "Mordell 方程：y² = x³ + k，椭圆曲线特殊情形",
                inputs, 1, PRESET_TYPE_LIST,
                "y^2 = x^3 + k, \\quad k \\in \\mathbb{Z}, \\quad "
                "\\text{有理点有限生成}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* Fermat 方程判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_ag_preset(
                PRESET_AG_FERMAT_EQUATION,
                "Fermat 方程：xⁿ + yⁿ = zⁿ，n > 2 时无非平凡整数解（Wiles）",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "x^n + y^n = z^n, \\quad n > 2, \\quad "
                "\\text{无非平凡整数解} \\quad "
                "\\text{(Fermat大定理, Wiles 1995)}",
                "O(1)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：代数数论（4个预设）
     *
     * 代数数论研究代数数域的算术性质，包括整数环、
     * 理想类群和单位群等核心结构。
     * ============================================================ */

    /* 代数整数环 */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL};
        if (register_ag_preset(
                PRESET_AG_INTEGER_RING,
                "代数整数环 O_K：K 中所有代数整数的集合",
                inputs, 1, PRESET_TYPE_RING,
                "\\mathcal{O}_K = \\{\\alpha \\in K : "
                "\\alpha \\text{ 是首一整系数多项式的根}\\}, \\quad "
                "\\mathcal{O}_K \\text{ 是Dedekind环}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 理想类群 */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ag_preset(
                PRESET_AG_CLASS_GROUP,
                "理想类群 Cl(K)：衡量主理想整环的偏离程度",
                inputs, 1, PRESET_TYPE_GROUP,
                "\\text{Cl}(K) = \\frac{\\text{理想群}}{\\text{主理想}}, \\quad "
                "h_K = |\\text{Cl}(K)| = \\text{类数}, \\quad "
                "\\text{唯一分解} \\Leftrightarrow h_K = 1",
                "O(n³)", true, false)) {
            success_count++;
        }
    }

    /* 单位群 */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ag_preset(
                PRESET_AG_UNIT_GROUP,
                "单位群 O_K^×：Dirichlet单位定理描述其结构",
                inputs, 1, PRESET_TYPE_GROUP,
                "\\mathcal{O}_K^{\\times} = \\{u \\in \\mathcal{O}_K : "
                "u^{-1} \\in \\mathcal{O}_K\\}, \\quad "
                "\\mathcal{O}_K^{\\times} \\cong \\mu_K \\times \\mathbb{Z}^{r_1 + r_2 - 1}, \\quad "
                "\\text{(Dirichlet单位定理)}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* Dedekind zeta 函数 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_RING};
        if (register_ag_preset(
                PRESET_AG_DEDEKIND_ZETA,
                "Dedekind zeta 函数：ζ_K(s) = ∏(1 - N(p)^(-s))^(-1)",
                inputs, 2, PRESET_TYPE_SCALAR,
                "\\zeta_K(s) = \\prod_{\\mathfrak{p}} \\frac{1}{1 - N(\\mathfrak{p})^{-s}}, \\quad "
                "\\text{Re}(s) > 1, \\quad "
                "\\text{解析延拓到 } \\mathbb{C}, \\quad "
                "\\text{函数方程}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：p-adic 分析（4个预设）
     *
     * p-adic 数是实数的另一种完备化，在数论中有重要应用。
     * Hensel 引理是 p-adic 分析的核心工具。
     * ============================================================ */

    /* p-adic 赋值 */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_PRIME};
        if (register_ag_preset(
                PRESET_AG_PADIC_VALUATION,
                "p-adic 赋值：v_p(n) = n 中 p 的最高幂次",
                inputs, 2, PRESET_TYPE_INTEGER,
                "v_p(n) = \\max\\{k : p^k | n\\}, \\quad "
                "v_p(mn) = v_p(m) + v_p(n), \\quad "
                "v_p(m+n) \\ge \\min(v_p(m), v_p(n))",
                "O(log n)", true, false)) {
            success_count++;
        }
    }

    /* p-adic 范数 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_PRIME};
        if (register_ag_preset(
                PRESET_AG_PADIC_NORM,
                "p-adic 范数：|x|_p = p^(-v_p(x))",
                inputs, 2, PRESET_TYPE_SCALAR,
                "|x|_p = p^{-v_p(x)}, \\quad "
                "|xy|_p = |x|_p|y|_p, \\quad "
                "|x+y|_p \\le \\max(|x|_p, |y|_p), \\quad "
                "\\text{超度量性质}",
                "O(log n)", true, false)) {
            success_count++;
        }
    }

    /* Hensel 引理 */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_PRIME, PRESET_TYPE_INTEGER};
        if (register_ag_preset(
                PRESET_AG_HENSEL_LEMMA,
                "Hensel 引理：模 p 解可提升为 p-adic 解",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "f(x_0) \\equiv 0 \\pmod{p}, \\quad "
                "f'(x_0) \\not\\equiv 0 \\pmod{p} \\Rightarrow "
                "\\exists! x \\in \\mathbb{Z}_p: f(x) = 0, x \\equiv x_0 \\pmod{p}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* p-adic 数域扩张 */
    {
        PresetType inputs[] = {PRESET_TYPE_PRIME, PRESET_TYPE_INTEGER};
        if (register_ag_preset(
                PRESET_AG_PADIC_EXTENSION,
                "p-adic 数域扩张：Q_p 的有限扩张",
                inputs, 2, PRESET_TYPE_FIELD,
                "\\mathbb{Q}_p \\subset K, \\quad "
                "[K:\\mathbb{Q}_p] < \\infty, \\quad "
                "\\text{Galois群}, \\quad "
                "\\text{分歧理论}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：有理点（3个预设）
     *
     * 代数簇上的有理点是算术几何的核心研究对象。
     * Mordell-Weil 定理和 Faltings 定理是核心结果。
     * ============================================================ */

    /* 有理点高度 */
    {
        PresetType inputs[] = {PRESET_TYPE_TUPLE};
        if (register_ag_preset(
                PRESET_AG_HEIGHT_FUNCTION,
                "有理点高度：H(P) = max(|x|, |y|)，P = (x:y:z) 既约",
                inputs, 1, PRESET_TYPE_SCALAR,
                "H(P) = \\max(|x|, |y|, |z|), \\quad "
                "P = (x:y:z) \\in \\mathbb{P}^n(\\mathbb{Q}), \\quad "
                "\\gcd(x,y,z) = 1, \\quad "
                "h(P) = \\log H(P)",
                "O(log n)", true, false)) {
            success_count++;
        }
    }

    /* Mordell-Weil 定理 */
    {
        PresetType inputs[] = {PRESET_TYPE_EQUATION};
        if (register_ag_preset(
                PRESET_AG_MORDELL_WEIL,
                "Mordell-Weil 定理：椭圆曲线有理点群有限生成",
                inputs, 1, PRESET_TYPE_TUPLE,
                "E(\\mathbb{Q}) \\cong \\mathbb{Z}^r \\times E(\\mathbb{Q})_{\\text{tors}}, \\quad "
                "r = \\text{秩}, \\quad "
                "\\text{有限生成Abel群}",
                "O(n³)", false, false)) {
            success_count++;
        }
    }

    /* Faltings 定理 */
    {
        PresetType inputs[] = {PRESET_TYPE_EQUATION, PRESET_TYPE_INTEGER};
        if (register_ag_preset(
                PRESET_AG_FALTINGS,
                "Faltings 定理（Mordell 猜想）：亏格 ≥ 2 的曲线有有限多有理点",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "C/\\mathbb{Q}, \\quad g(C) \\ge 2 \\Rightarrow "
                "|C(\\mathbb{Q})| < \\infty, \\quad "
                "\\text{(Faltings 1983, Fields Medal)}",
                "O(1)", false, false)) {
            success_count++;
        }
    }

    /* Chabauty 方法 */
    {
        PresetType inputs[] = {PRESET_TYPE_EQUATION, PRESET_TYPE_PRIME};
        if (register_ag_preset(
                PRESET_AG_CHABAUTY,
                "Chabauty 方法：用 p-adic 积分计算有理点上界",
                inputs, 2, PRESET_TYPE_INTEGER,
                "r < g \\Rightarrow |C(\\mathbb{Q})| \\le \\text{有限上界}, \\quad "
                "\\text{p-adic积分} \\int_\\omega, \\quad "
                "\\text{Coleman上界}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == ARITHMETIC_GEOMETRY_PRESET_COUNT;
}

/* ==================== 模块信息接口 ==================== */

int preset_arithmetic_geometry_count(void)
{
    return ARITHMETIC_GEOMETRY_PRESET_COUNT;
}

PresetCategory preset_arithmetic_geometry_category(void)
{
    return PRESET_CATEGORY_NUMBER_THEORY;
}

bool preset_arithmetic_geometry_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    /* 分配名称数组 */
    char **names = (char**)lv00_malloc(ARITHMETIC_GEOMETRY_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

    /* 预设名称列表 */
    const char *preset_names[] = {
        /* 椭圆曲线 */
        PRESET_AG_WEIERSTRASS_FORM,
        PRESET_AG_ELLIPTIC_ADD,
        PRESET_AG_ELLIPTIC_DOUBLE,
        PRESET_AG_ELLIPTIC_SCALAR,
        PRESET_AG_ELLIPTIC_INVARIANTS,
        PRESET_AG_ELLIPTIC_TORSION,
        /* 模形式 */
        PRESET_AG_MODULAR_GROUP,
        PRESET_AG_EISENSTEIN_SERIES,
        PRESET_AG_MODULAR_DISCRIMINANT,
        PRESET_AG_J_INVARIANT,
        /* Diophantine 方程 */
        PRESET_AG_PELL_EQUATION,
        PRESET_AG_THUE_EQUATION,
        PRESET_AG_MORDELL_EQUATION,
        PRESET_AG_FERMAT_EQUATION,
        /* 代数数论 */
        PRESET_AG_INTEGER_RING,
        PRESET_AG_CLASS_GROUP,
        PRESET_AG_UNIT_GROUP,
        PRESET_AG_DEDEKIND_ZETA,
        /* p-adic 分析 */
        PRESET_AG_PADIC_VALUATION,
        PRESET_AG_PADIC_NORM,
        PRESET_AG_HENSEL_LEMMA,
        PRESET_AG_PADIC_EXTENSION,
        /* 有理点 */
        PRESET_AG_HEIGHT_FUNCTION,
        PRESET_AG_MORDELL_WEIL,
        PRESET_AG_FALTINGS,
        PRESET_AG_CHABAUTY,
    };

    int count = (int)(sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv00_free(&tmp);
            }
            lv00_free((void **)&names);
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
