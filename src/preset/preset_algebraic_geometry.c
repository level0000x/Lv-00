/**
 * @file preset_algebraic_geometry.c
 * @brief 代数几何预设函数块模块 - 实现
 *
 * 实现理论数学研究中常用的代数几何预设函数块。
 * 涵盖仿射代数集、射影空间、曲线论、层与上同调，共20个预设。
 *
 * @module AlgebraicGeometry
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 2.0.0
 * @author Lv-00 Project
 */

#include "preset_algebraic_geometry.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 代数几何模块预设函数块总数 */
#define ALGEBRAIC_GEOMETRY_PRESET_COUNT 20

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个代数几何预设
 *
 * 统一调用 preset_blocks_register_simple，类别固定为
 * PRESET_CATEGORY_ALGEBRAIC。
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
static bool register_ag_preset(const char *name, const char *description, const PresetType *input_types,
                               int input_count, PresetType output_type, const char *math_def, const char *complexity,
                               bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_ALGEBRAIC, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_algebraic_geometry_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：仿射代数集（5个预设）
     *
     * 仿射代数集是代数几何的基本研究对象，定义为多项式组
     * 在仿射空间中的公共零点集。核心定理包括Hilbert零点定理、
     * 代数集的维数理论和不可约分解。
     * ============================================================ */

    /* -------------------- ag_affine_variety_construct：仿射代数集构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_IDEAL, PRESET_TYPE_INTEGER};
        if (register_ag_preset(PRESET_AG_AFFINE_VARIETY_CONSTRUCT,
                               "仿射代数集构造：由多项式理想 I 构造仿射代数集 V(I) = {x : f(x)=0, f in I}", inputs, 2,
                               PRESET_TYPE_REGION,
                               "V(I) = \\{x \\in \\mathbb{A}^n : f(x) = 0, \\forall f \\in I\\}, "
                               "\\quad I = \\langle f_1, \\ldots, f_m \\rangle \\subseteq k[x_1, \\ldots, x_n]",
                               "O(2^n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_ideal_variety_correspondence：理想-代数集对应 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_IDEAL};
        if (register_ag_preset(PRESET_AG_IDEAL_VARIETY_CORRESPONDENCE,
                               "理想-代数集对应：Hilbert零点定理 V(I(V(J))) = sqrt(J)，建立理想与代数集的对应", inputs,
                               1, PRESET_TYPE_IDEAL,
                               "I(V(J)) = \\sqrt{J}, \\quad "
                               "V(I) \\cup V(J) = V(I \\cap J), \\quad "
                               "V(I) \\cap V(J) = V(I + J), \\quad "
                               "\\text{Hilbert Nullstellensatz}",
                               "O(2^{O(n)})", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_variety_dimension：代数集维数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_REGION};
        if (register_ag_preset(PRESET_AG_VARIETY_DIMENSION,
                               "代数集维数：计算仿射代数集的Krull维数 dim V = tr.deg_k k(V)", inputs, 1,
                               PRESET_TYPE_INTEGER,
                               "\\dim V = \\dim \\mathcal{O}(V) = "
                               "\\mathrm{tr.deg}_k \\, k(V), \\quad "
                               "\\dim V = n - \\mathrm{rank}(\\mathrm{Jac}(f_1, \\ldots, f_m))",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_variety_decomposition：代数集分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_REGION};
        if (register_ag_preset(PRESET_AG_VARIETY_DECOMPOSITION,
                               "代数集分解：将代数集分解为不可约分支 V = V_1 ∪ ... ∪ V_r", inputs, 1, PRESET_TYPE_LIST,
                               "V = V_1 \\cup V_2 \\cup \\cdots \\cup V_r, \\quad "
                               "V_i \\text{ 不可约}, \\quad "
                               "V_i \\not\\subseteq V_j \\ (i \\neq j), \\quad "
                               "I(V) = I(V_1) \\cap \\cdots \\cap I(V_r) \\text{ 准素分解}",
                               "O(2^{O(n)})", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_variety_intersection：代数集交 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_REGION, PRESET_TYPE_REGION};
        if (register_ag_preset(PRESET_AG_VARIETY_INTERSECTION, "代数集交：计算两个代数集的交集 V(I) ∩ V(J) = V(I + J)",
                               inputs, 2, PRESET_TYPE_REGION,
                               "V(I) \\cap V(J) = V(I + J), \\quad "
                               "\\dim(V(I) \\cap V(J)) \\geq \\dim V(I) + \\dim V(J) - n",
                               "O(2^{O(n)})", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：射影空间（4个预设）
     *
     * 射影空间是代数几何的核心框架，通过添加无穷远点使
     * 几何定理更加完备。射影代数集由齐次多项式定义，
     * Bezout定理给出了平面曲线交点数的精确公式。
     * ============================================================ */

    /* -------------------- ag_projective_space_construct：射影空间构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_ag_preset(PRESET_AG_PROJECTIVE_SPACE_CONSTRUCT,
                               "射影空间构造：构造n维射影空间 P^n = (A^{n+1}\\{0}) / k^*", inputs, 1, PRESET_TYPE_SPACE,
                               "\\mathbb{P}^n = (\\mathbb{A}^{n+1} \\setminus \\{0\\}) / k^*, "
                               "\\quad [x_0 : \\cdots : x_n] \\sim [\\lambda x_0 : \\cdots : \\lambda x_n], "
                               "\\quad \\lambda \\neq 0",
                               "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_projective_variety：射影代数集 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_LIST};
        if (register_ag_preset(PRESET_AG_PROJECTIVE_VARIETY, "射影代数集：由齐次多项式组构造射影代数集 V(I) ⊂ P^n",
                               inputs, 2, PRESET_TYPE_REGION,
                               "V(I) = \\{[x_0 : \\cdots : x_n] \\in \\mathbb{P}^n : "
                               "f(x_0, \\ldots, x_n) = 0, \\forall f \\in I\\}, "
                               "\\quad I \\text{ 齐次理想}",
                               "O(2^n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_projective_closure：射影闭包 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_REGION};
        if (register_ag_preset(PRESET_AG_PROJECTIVE_CLOSURE, "射影闭包：计算仿射代数集的射影闭包，添加无穷远点", inputs,
                               1, PRESET_TYPE_REGION,
                               "\\overline{X} = V(I^h) \\subset \\mathbb{P}^n, \\quad "
                               "I^h = \\langle f^h : f \\in I(X) \\rangle, \\quad "
                               "f^h(x_0, \\ldots, x_n) = x_0^{\\deg f} f(x_1/x_0, \\ldots, x_n/x_0)",
                               "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_bezout_theorem：Bezout定理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_POLYNOMIAL};
        if (register_ag_preset(PRESET_AG_BEZOUT_THEOREM,
                               "Bezout定理：两个平面曲线 C_d 和 C_e 的交点数（计重数）= d * e", inputs, 2,
                               PRESET_TYPE_INTEGER,
                               "\\sum_{P \\in C_d \\cap C_e} I(P, C_d \\cap C_e) = d \\cdot e, "
                               "\\quad C_d : F(x,y) = 0 \\ (\\deg F = d), "
                               "\\quad C_e : G(x,y) = 0 \\ (\\deg G = e)",
                               "O(n^3)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：曲线论（5个预设）
     *
     * 代数曲线是代数几何中最经典的研究对象之一。
     * 包括平面曲线的构造与奇点分析、亏格计算、
     * 有理曲线判定以及椭圆曲线理论。
     * ============================================================ */

    /* -------------------- ag_plane_curve_construct：平面曲线构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL};
        if (register_ag_preset(PRESET_AG_PLANE_CURVE_CONSTRUCT, "平面曲线构造：由方程 f(x,y)=0 构造平面仿射曲线 C",
                               inputs, 1, PRESET_TYPE_PATH,
                               "C : f(x, y) = 0, \\quad f \\in k[x, y], \\quad "
                               "\\deg f = d, \\quad C \\subset \\mathbb{A}^2",
                               "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_curve_singularity：奇点分析 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_PATH, PRESET_TYPE_POINT};
        if (register_ag_preset(PRESET_AG_CURVE_SINGULARITY,
                               "奇点分析：判定曲线的奇点并分类（常点、结点、尖点、自交点等）", inputs, 2,
                               PRESET_TYPE_STRING,
                               "p \\in \\mathrm{Sing}(C) \\Leftrightarrow "
                               "f(p) = f_x(p) = f_y(p) = 0, \\quad "
                               "\\delta_P = \\dim_k \\mathcal{O}_{C,p} / \\hat{\\mathcal{O}}_{C,p}, "
                               "\\quad m_p = \\text{重数}",
                               "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_curve_genus：曲线亏格 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_PATH};
        if (register_ag_preset(PRESET_AG_CURVE_GENUS, "曲线亏格：计算平面曲线的亏格 g = (d-1)(d-2)/2 - sum(delta_P)",
                               inputs, 1, PRESET_TYPE_INTEGER,
                               "g = \\frac{(d-1)(d-2)}{2} - \\sum_{P \\in \\mathrm{Sing}(C)} \\delta_P, "
                               "\\quad \\delta_P = \\frac{m_P(m_P - 1)}{2} + \\text{高阶项}",
                               "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_rational_curve：有理曲线判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_PATH};
        if (register_ag_preset(PRESET_AG_RATIONAL_CURVE, "有理曲线判定：判定曲线是否为有理曲线（g=0 时存在有理参数化）",
                               inputs, 1, PRESET_TYPE_BOOLEAN,
                               "C \\text{ 有理} \\Leftrightarrow g(C) = 0 \\Leftrightarrow "
                               "k(C) \\cong k(t), \\quad "
                               "\\exists \\phi : \\mathbb{P}^1 \\dashrightarrow C \\text{ 双有理}",
                               "O(n!)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_elliptic_curve：椭圆曲线 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_ag_preset(PRESET_AG_ELLIPTIC_CURVE,
                               "椭圆曲线：由Weierstrass方程 y^2 = x^3 + ax + b 构造椭圆曲线（判别式 != 0）", inputs, 2,
                               PRESET_TYPE_PATH,
                               "E : y^2 = x^3 + ax + b, \\quad "
                               "\\Delta = -16(4a^3 + 27b^2) \\neq 0, \\quad "
                               "g(E) = 1, \\quad E \\cong \\mathbb{C}/\\Lambda",
                               "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：层与上同调（6个预设）
     *
     * 层论是现代代数几何的核心语言，通过层可以将局部信息
     * 粘合为全局信息。上同调群是层的全局不变量，
     * Riemann-Roch定理是曲线论中最深刻的结果之一。
     * ============================================================ */

    /* -------------------- ag_sheaf_construct：层的构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SPACE, PRESET_TYPE_FUNCTION};
        if (register_ag_preset(PRESET_AG_SHEAF_CONSTRUCT, "层的构造：在拓扑空间上构造预层/层，验证层公理", inputs, 2,
                               PRESET_TYPE_MODULE,
                               "\\mathcal{F} : \\mathrm{Open}(X)^{\\mathrm{op}} \\to \\mathfrak{A}, "
                               "\\quad U \\subseteq V \\Rightarrow \\rho_{VU} : \\mathcal{F}(V) \\to \\mathcal{F}(U), "
                               "\\quad \\text{局部截面可粘合}",
                               "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_sheaf_cohomology：层上同调 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SPACE, PRESET_TYPE_MODULE, PRESET_TYPE_INTEGER};
        if (register_ag_preset(PRESET_AG_SHEAF_COHOMOLOGY,
                               "层上同调：计算层的上同调群 H^i(X, F)，使用Cech上同调或导出函子", inputs, 3,
                               PRESET_TYPE_MODULE,
                               "H^i(X, \\mathcal{F}) = R^i \\Gamma(X, -)(\\mathcal{F}), \\quad "
                               "\\check{H}^i(\\mathcal{U}, \\mathcal{F}) "
                               "= \\ker(d^i) / \\mathrm{im}(d^{i-1})",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_structure_sheaf：结构层 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SPACE};
        if (register_ag_preset(PRESET_AG_STRUCTURE_SHEAF, "结构层：构造代数簇上的结构层 O_X，赋予正则函数层", inputs, 1,
                               PRESET_TYPE_MODULE,
                               "\\mathcal{O}_X(U) = \\{f : U \\to k \\mid f \\text{ 正则}\\}, "
                               "\\quad \\mathcal{O}_{X,p} = \\varinjlim_{p \\in U} \\mathcal{O}_X(U), "
                               "\\quad (X, \\mathcal{O}_X) \\text{ 环式空间}",
                               "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_tangent_sheaf：切层 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SPACE};
        if (register_ag_preset(PRESET_AG_TANGENT_SHEAF, "切层：构造代数簇上的切层 T_X = Der_k(O_X, O_X)", inputs, 1,
                               PRESET_TYPE_MODULE,
                               "\\mathcal{T}_X = \\mathcal{H}om_{\\mathcal{O}_X}"
                               "(\\Omega_{X/k}, \\mathcal{O}_X), \\quad "
                               "\\Omega_{X/k} \\text{ 为微分层}, \\quad "
                               "\\mathrm{rank}(\\mathcal{T}_X) = \\dim X",
                               "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_canonical_divisor：典范除子 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_PATH};
        if (register_ag_preset(
                PRESET_AG_CANONICAL_DIVISOR, "典范除子：计算代数簇的典范除子 K_X，由微分形式的零点与极点确定", inputs,
                1, PRESET_TYPE_EXPRESSION,
                "K_X = \\mathrm{div}(\\omega), \\quad \\omega \\in H^0(X, \\Omega^1_X) \\setminus \\{0\\}, "
                "\\quad \\deg K_X = 2g - 2 \\ (\\text{曲线情形})",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ag_riemann_roch：Riemann-Roch定理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_PATH, PRESET_TYPE_EXPRESSION};
        if (register_ag_preset(PRESET_AG_RIEMANN_ROCH, "Riemann-Roch定理：dim L(D) - dim L(K-D) = deg(D) + 1 - g",
                               inputs, 2, PRESET_TYPE_INTEGER,
                               "\\ell(D) - \\ell(K - D) = \\deg(D) + 1 - g, \\quad "
                               "\\ell(D) = \\dim_k H^0(X, \\mathcal{O}_X(D)), \\quad "
                               "\\ell(K - D) = h^1(X, \\mathcal{O}_X(D))",
                               "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == ALGEBRAIC_GEOMETRY_PRESET_COUNT;
}

/**
 * @brief 获取代数几何预设函数块数量
 *
 * @return int 代数几何模块预设函数块总数（20）
 */
int preset_algebraic_geometry_count(void) {
    return ALGEBRAIC_GEOMETRY_PRESET_COUNT;
}

/**
 * @brief 获取代数几何模块的预设类别
 *
 * 所有代数几何预设均属于代数学类别。
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_ALGEBRAIC
 */
PresetCategory preset_algebraic_geometry_category(void) {
    return PRESET_CATEGORY_ALGEBRAIC;
}

/**
 * @brief 获取代数几何模块的所有预设名称列表
 *
 * @param out_names 输出名称数组
 * @param out_count 输出名称数量
 * @return true 成功
 * @return false 失败
 */
bool preset_algebraic_geometry_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    /* 分配名称数组 */
    char **names = (char **) lv00_malloc(ALGEBRAIC_GEOMETRY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 仿射代数集 */
        PRESET_AG_AFFINE_VARIETY_CONSTRUCT,
        PRESET_AG_IDEAL_VARIETY_CORRESPONDENCE,
        PRESET_AG_VARIETY_DIMENSION,
        PRESET_AG_VARIETY_DECOMPOSITION,
        PRESET_AG_VARIETY_INTERSECTION,
        /* 射影空间 */
        PRESET_AG_PROJECTIVE_SPACE_CONSTRUCT,
        PRESET_AG_PROJECTIVE_VARIETY,
        PRESET_AG_PROJECTIVE_CLOSURE,
        PRESET_AG_BEZOUT_THEOREM,
        /* 曲线论 */
        PRESET_AG_PLANE_CURVE_CONSTRUCT,
        PRESET_AG_CURVE_SINGULARITY,
        PRESET_AG_CURVE_GENUS,
        PRESET_AG_RATIONAL_CURVE,
        PRESET_AG_ELLIPTIC_CURVE,
        /* 层与上同调 */
        PRESET_AG_SHEAF_CONSTRUCT,
        PRESET_AG_SHEAF_COHOMOLOGY,
        PRESET_AG_STRUCTURE_SHEAF,
        PRESET_AG_TANGENT_SHEAF,
        PRESET_AG_CANONICAL_DIVISOR,
        PRESET_AG_RIEMANN_ROCH,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv00_free(&tmp);
            }
            lv00_free((void **) &names);
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
