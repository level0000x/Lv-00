/**
 * @file preset_lie_theory_advanced.c
 * @brief 李理论预设函数块模块 - 实现
 *
 * 实现理论数学研究中常用的李理论预设函数块。
 * 涵盖李代数基础、半单李代数、李代数表示、泛包络代数、
 * 李群与李代数的对应，共25个预设。
 *
 * @module LieTheoryAdvanced
 * @category PRESET_CATEGORY_ALGEBRA
 * @version 1.0.0
 * @author Lv-00 Project
 */

#include "preset_lie_theory_advanced.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 李理论模块预设函数块总数 */
#define LIE_THEORY_PRESET_COUNT 25

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个李理论预设
 *
 * 统一调用 preset_blocks_register_simple，类别固定为
 * PRESET_CATEGORY_ALGEBRA。
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
static bool register_lt_preset(
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
        PRESET_CATEGORY_ALGEBRA,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_lie_theory_advanced_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：李代数基础（5个预设）
     *
     * 李代数是李群的无穷小邻域，由向量空间配备反对称
     * 双线性括号运算构成，满足Jacobi恒等式。
     * ============================================================ */

    /* -------------------- lie_bracket：李括号 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_VECTOR, PRESET_TYPE_VECTOR};
        if (register_lt_preset(
                PRESET_LIE_BRACKET,
                "李括号：计算李代数中两个元素的括号 [X, Y]",
                inputs, 3, PRESET_TYPE_VECTOR,
                "[X, Y] = -[Y, X], \\quad "
                "[X, [Y, Z]] + [Y, [Z, X]] + [Z, [X, Y]] = 0 \\text{ (Jacobi恒等式)}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- lie_algebra_ideal：李代数理想 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_SUBGROUP};
        if (register_lt_preset(
                PRESET_LIE_ALGEBRA_IDEAL,
                "李代数理想：判定子空间是否为理想（满足 [L, I] ⊆ I）",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "I \\trianglelefteq L \\Leftrightarrow [L, I] \\subseteq I, "
                "\\quad \\text{即 } \\forall x \\in L, y \\in I: [x, y] \\in I",
                "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- lie_algebra_homomorphism：李代数同态 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_ALGEBRA, PRESET_TYPE_FUNCTION};
        if (register_lt_preset(
                PRESET_LIE_ALGEBRA_HOMOMORPHISM,
                "李代数同态：判定映射是否为李代数同态（保持括号）",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "\\phi: L_1 \\to L_2 \\text{ 是李代数同态} \\Leftrightarrow "
                "\\phi([x, y]) = [\\phi(x), \\phi(y)], \\forall x, y \\in L_1",
                "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- derived_algebra：导出代数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        if (register_lt_preset(
                PRESET_DERIVED_ALGEBRA,
                "导出代数：计算导出代数 [L, L]（换位子生成的理想）",
                inputs, 1, PRESET_TYPE_IDEAL,
                "[L, L] = \\mathrm{span}\\{[x, y] : x, y \\in L\\}, "
                "\\quad L \\text{ 可解} \\Leftrightarrow L^{(n)} = 0 \\text{ 对某 } n",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- lie_algebra_center：李代数的中心 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        if (register_lt_preset(
                PRESET_LIE_ALGEBRA_CENTER,
                "李代数的中心：计算李代数的中心 Z(L) = {x : [x, y] = 0, ∀y}",
                inputs, 1, PRESET_TYPE_SUBGROUP,
                "Z(L) = \\{x \\in L : [x, y] = 0, \\forall y \\in L\\}, "
                "\\quad Z(L) \\trianglelefteq L, \\quad L \\text{ 单} \\Rightarrow Z(L) = 0",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：半单李代数（5个预设）
     *
     * 半单李代数是完全可约的李代数，其结构由根系完全决定。
     * Cartan矩阵和Dynkin图是分类半单李代数的关键工具。
     * ============================================================ */

    /* -------------------- root_system：根系 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        if (register_lt_preset(
                PRESET_ROOT_SYSTEM,
                "根系：计算半单李代数的根系 Φ ⊂ h*",
                inputs, 1, PRESET_TYPE_SET,
                "\\Phi = \\{\\alpha \\in \\mathfrak{h}^* : \\exists x_\\alpha \\neq 0, "
                "[h, x_\\alpha] = \\alpha(h) x_\\alpha\\}, "
                "\\quad \\Phi = \\Phi^+ \\cup \\Phi^-",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- simple_roots：单根 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        if (register_lt_preset(
                PRESET_SIMPLE_ROOTS,
                "单根：计算根系的基础单根 Δ = {α₁, ..., αₙ}",
                inputs, 1, PRESET_TYPE_SET,
                "\\Delta = \\{\\alpha_1, \\ldots, \\alpha_n\\} \\subset \\Phi^+, "
                "\\quad \\Phi^+ = \\{\\sum_{i=1}^{n} n_i \\alpha_i : n_i \\ge 0\\}, "
                "\\quad |\\Delta| = \\mathrm{rank}(L)",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- weyl_group：Weyl群 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        if (register_lt_preset(
                PRESET_WEYL_GROUP,
                "Weyl群：计算根系的Weyl群 W = <s_α : α ∈ Δ>",
                inputs, 1, PRESET_TYPE_GROUP,
                "W = \\langle s_\\alpha : \\alpha \\in \\Delta \\rangle, "
                "\\quad s_\\alpha(\\beta) = \\beta - \\frac{2(\\beta, \\alpha)}{(\\alpha, \\alpha)}\\alpha, "
                "\\quad |W| = n! \\cdot 2^n \\text{ (A_n型)}",
                "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- cartan_matrix：Cartan矩阵 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        if (register_lt_preset(
                PRESET_CARTAN_MATRIX,
                "Cartan矩阵：计算单根的Cartan矩阵 A = (a_{ij})",
                inputs, 1, PRESET_TYPE_MATRIX,
                "A = (a_{ij})_{i,j=1}^{n}, \\quad "
                "a_{ij} = \\frac{2(\\alpha_i, \\alpha_j)}{(\\alpha_i, \\alpha_i)}, "
                "\\quad a_{ii} = 2, a_{ij} \\in \\{0, -1, -2, -3\\} (i \\neq j)",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- dynkin_diagram：Dynkin图 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_lt_preset(
                PRESET_DYNKIN_DIAGRAM,
                "Dynkin图：构造根系的Dynkin图（分类半单李代数）",
                inputs, 1, PRESET_TYPE_GRAPH,
                "\\text{顶点} = \\Delta, \\quad "
                "\\alpha_i \\text{ 与 } \\alpha_j \\text{ 间边数} = a_{ij} a_{ji}, "
                "\\quad \\text{类型：} A_n, B_n, C_n, D_n, E_6, E_7, E_8, F_4, G_2",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：李代数表示（5个预设）
     *
     * 李代数表示是李代数到线性变换的同态，
     * 最高权理论完全分类了半单李代数的有限维不可约表示。
     * ============================================================ */

    /* -------------------- irreducible_representation：不可约表示 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_VECTOR};
        if (register_lt_preset(
                PRESET_IRREDUCIBLE_REPRESENTATION,
                "不可约表示：构造最高权为λ的不可约表示 V(λ)",
                inputs, 2, PRESET_TYPE_MODULE,
                "V(\\lambda), \\quad \\lambda \\in \\mathfrak{h}^* \\text{ 是支配整权}, "
                "\\quad \\lambda = \\sum_i n_i \\omega_i, n_i \\ge 0, "
                "\\quad \\omega_i \\text{ 是基本权}",
                "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- highest_weight：最高权 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE};
        if (register_lt_preset(
                PRESET_HIGHEST_WEIGHT,
                "最高权：计算表示的最高权 λ",
                inputs, 1, PRESET_TYPE_VECTOR,
                "\\lambda \\in P^+ = \\{\\mu \\in \\mathfrak{h}^* : "
                "(\\mu, \\alpha_i) \\ge 0, \\forall \\alpha_i \\in \\Delta\\}, "
                "\\quad V(\\lambda) \\text{ 由最高权向量生成}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- weight_space_decomposition：权空间分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE};
        if (register_lt_preset(
                PRESET_WEIGHT_SPACE_DECOMPOSITION,
                "权空间分解：计算表示的权空间分解 V = ⊕_{μ} V_μ",
                inputs, 1, PRESET_TYPE_SEQUENCE,
                "V = \\bigoplus_{\\mu \\in P(V)} V_\\mu, "
                "\\quad V_\\mu = \\{v \\in V : h \\cdot v = \\mu(h) v, \\forall h \\in \\mathfrak{h}\\}, "
                "\\quad P(V) \\subset \\mathfrak{h}^*",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- character_formula：特征标公式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE};
        if (register_lt_preset(
                PRESET_CHARACTER_FORMULA,
                "特征标公式：计算表示的特征标（Weyl特征标公式）",
                inputs, 1, PRESET_TYPE_FUNCTION,
                "\\mathrm{ch}_V = \\frac{\\sum_{w \\in W} \\varepsilon(w) e^{w(\\lambda + \\rho)}}"
                "{\\sum_{w \\in W} \\varepsilon(w) e^{w(\\rho)}}, "
                "\\quad \\rho = \\frac{1}{2} \\sum_{\\alpha \\in \\Phi^+} \\alpha",
                "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- dimension_formula：维数公式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_VECTOR};
        if (register_lt_preset(
                PRESET_DIMENSION_FORMULA,
                "维数公式：计算不可约表示的维数（Weyl维数公式）",
                inputs, 1, PRESET_TYPE_INTEGER,
                "\\dim V(\\lambda) = \\prod_{\\alpha \\in \\Phi^+} "
                "\\frac{(\\lambda + \\rho, \\alpha)}{(\\rho, \\alpha)}, "
                "\\quad \\rho = \\frac{1}{2} \\sum_{\\alpha \\in \\Phi^+} \\alpha",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：泛包络代数（5个预设）
     *
     * 泛包络代数是李代数的"扩张"，PBW定理给出其基。
     * Casimir算子是表示论中的重要工具。
     * ============================================================ */

    /* -------------------- universal_enveloping_algebra：泛包络代数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        if (register_lt_preset(
                PRESET_UNIVERSAL_ENVELOPING_ALGEBRA,
                "泛包络代数：构造李代数的泛包络代数 U(L)",
                inputs, 1, PRESET_TYPE_ALGEBRA,
                "U(L) = T(L) / \\langle x \\otimes y - y \\otimes x - [x, y] \\rangle, "
                "\\quad T(L) = \\bigoplus_{n \\ge 0} L^{\\otimes n}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- pbw_basis：PBW基 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_LIST};
        if (register_lt_preset(
                PRESET_PBW_BASIS,
                "PBW基：构造Poincaré-Birkhoff-Witt基",
                inputs, 2, PRESET_TYPE_LIST,
                "\\{x_{i_1}^{n_1} \\cdots x_{i_k}^{n_k} : i_1 < \\cdots < i_k, n_j \\ge 0\\}, "
                "\\quad \\{x_1, \\ldots, x_n\\} \\text{ 是 } L \\text{ 的基}",
                "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- casimir_operator：Casimir算子 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_FUNCTION};
        if (register_lt_preset(
                PRESET_CASIMIR_OPERATOR,
                "Casimir算子：构造Casimir算子 Ω = Σ x_i y_i（中心元素）",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "\\Omega = \\sum_{i=1}^{n} x_i y_i \\in Z(U(L)), "
                "\\quad \\{x_i\\}, \\{y_i\\} \\text{ 是对偶基}, "
                "\\quad (x_i, y_j) = \\delta_{ij} \\text{ (Killing型)}",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- infinitesimal_character：中心特征标 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE, PRESET_TYPE_FUNCTION};
        if (register_lt_preset(
                PRESET_INFinitESIMAL_CHARACTER,
                "中心特征标：计算中心 Z(U(L)) 在表示上的特征标",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "\\chi_\\lambda: Z(U(L)) \\to \\mathbb{C}, "
                "\\quad z \\mapsto \\lambda(z), "
                "\\quad \\chi_\\lambda = \\chi_\\mu \\Leftrightarrow \\lambda \\in W \\cdot \\mu",
                "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- verma_module：Verma模 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_VECTOR};
        if (register_lt_preset(
                PRESET_VERMA_MODULE,
                "Verma模：构造Verma模 M(λ) = U(L) ⊗_{U(b)} C_λ",
                inputs, 2, PRESET_TYPE_MODULE,
                "M(\\lambda) = U(L) \\otimes_{U(\\mathfrak{b})} \\mathbb{C}_\\lambda, "
                "\\quad \\mathfrak{b} = \\mathfrak{h} \\oplus \\mathfrak{n}^+, "
                "\\quad V(\\lambda) = M(\\lambda) / N(\\lambda)",
                "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：李群与李代数的对应（5个预设）
     *
     * 李群与李代数通过指数映射建立对应关系，
     * Killing型是半单李代数分类的关键。
     * ============================================================ */

    /* -------------------- lie_algebrization：李代数化 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_GROUP};
        if (register_lt_preset(
                PRESET_LIE_ALGEBRIZATION,
                "李代数化：计算李群的李代数 Lie(G) = T_e G",
                inputs, 1, PRESET_TYPE_ALGEBRA,
                "\\mathfrak{g} = T_e G, \\quad "
                "[X, Y] = \\left.\\frac{d}{dt}\\right|_{t=0} "
                "\\mathrm{Ad}(\\exp(tY))X, "
                "\\quad \\exp: \\mathfrak{g} \\to G",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- exponential_map：指数映射 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA, PRESET_TYPE_VECTOR};
        if (register_lt_preset(
                PRESET_EXPONENTIAL_MAP,
                "指数映射：计算李代数到李群的指数映射 exp: L → G",
                inputs, 2, PRESET_TYPE_GROUP_ELEMENT,
                "\\exp(X) = e^X = \\sum_{n=0}^{\\infty} \\frac{X^n}{n!}, "
                "\\quad \\exp: \\mathfrak{g} \\to G \\text{ 是局部微分同胚}",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- adjoint_representation：伴随表示 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        if (register_lt_preset(
                PRESET_ADJOINT_REPRESENTATION,
                "伴随表示：计算李代数的伴随表示 ad: L → gl(L)",
                inputs, 1, PRESET_TYPE_FUNCTION,
                "\\mathrm{ad}: \\mathfrak{g} \\to \\mathfrak{gl}(\\mathfrak{g}), "
                "\\quad \\mathrm{ad}(X)(Y) = [X, Y], "
                "\\quad \\mathrm{ad} \\text{ 是 } \\mathfrak{g} \\text{ 在自身上的表示}",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- killing_form：Killing型 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        if (register_lt_preset(
                PRESET_KILLING_FORM,
                "Killing型：计算李代数的Killing型 κ(x, y) = tr(ad x ∘ ad y)",
                inputs, 1, PRESET_TYPE_FUNCTION,
                "\\kappa(x, y) = \\mathrm{tr}(\\mathrm{ad}(x) \\circ \\mathrm{ad}(y)), "
                "\\quad \\kappa([x, y], z) = \\kappa(x, [y, z]), "
                "\\quad \\kappa \\text{ 是对称双线性型}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- semisimple_check：半单判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_ALGEBRA};
        if (register_lt_preset(
                PRESET_SEMISIMPLE_CHECK,
                "半单判定：判定李代数是否半单（Killing型非退化）",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\mathfrak{g} \\text{ 半单} \\Leftrightarrow \\kappa \\text{ 非退化} "
                "\\Leftrightarrow \\mathfrak{g} = \\mathfrak{g}_1 \\oplus \\cdots \\oplus \\mathfrak{g}_k, "
                "\\quad \\mathfrak{g}_i \\text{ 单}",
                "O(n^3)", false, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == LIE_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取李理论预设函数块数量
 *
 * @return int 李理论模块预设函数块总数（25）
 */
int preset_lie_theory_advanced_count(void)
{
    return LIE_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取李理论模块的预设类别
 *
 * 所有李理论预设均属于代数学类别。
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_ALGEBRA
 */
PresetCategory preset_lie_theory_advanced_category(void)
{
    return PRESET_CATEGORY_ALGEBRA;
}

/**
 * @brief 获取李理论模块的所有预设名称列表
 *
 * @param out_names 输出名称数组
 * @param out_count 输出名称数量
 * @return true 成功
 * @return false 失败
 */
bool preset_lie_theory_advanced_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    /* 分配名称数组 */
    char **names = (char**)lv00_malloc(LIE_THEORY_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 李代数基础 */
        PRESET_LIE_BRACKET,
        PRESET_LIE_ALGEBRA_IDEAL,
        PRESET_LIE_ALGEBRA_HOMOMORPHISM,
        PRESET_DERIVED_ALGEBRA,
        PRESET_LIE_ALGEBRA_CENTER,
        /* 半单李代数 */
        PRESET_ROOT_SYSTEM,
        PRESET_SIMPLE_ROOTS,
        PRESET_WEYL_GROUP,
        PRESET_CARTAN_MATRIX,
        PRESET_DYNKIN_DIAGRAM,
        /* 李代数表示 */
        PRESET_IRREDUCIBLE_REPRESENTATION,
        PRESET_HIGHEST_WEIGHT,
        PRESET_WEIGHT_SPACE_DECOMPOSITION,
        PRESET_CHARACTER_FORMULA,
        PRESET_DIMENSION_FORMULA,
        /* 泛包络代数 */
        PRESET_UNIVERSAL_ENVELOPING_ALGEBRA,
        PRESET_PBW_BASIS,
        PRESET_CASIMIR_OPERATOR,
        PRESET_INFinitESIMAL_CHARACTER,
        PRESET_VERMA_MODULE,
        /* 李群与李代数 */
        PRESET_LIE_ALGEBRIZATION,
        PRESET_EXPONENTIAL_MAP,
        PRESET_ADJOINT_REPRESENTATION,
        PRESET_KILLING_FORM,
        PRESET_SEMISIMPLE_CHECK,
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
