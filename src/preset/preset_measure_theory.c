/**
 * @file preset_measure_theory.c
 * @brief 测度论预设函数块 - 实现
 *
 * @details 实现测度论模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共20个预设，涵盖σ代数、测度构造、可测函数与积分、
 *          收敛定理、乘积测度与Fubini定理、Radon-Nikodym导数。
 *
 * @module MeasureTheory
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 */

#include "preset_measure_theory.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 测度论模块预设函数块总数 */
#define MEASURE_THEORY_PRESET_COUNT 20

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 注册单个测度论预设
 */
static bool register_mt_preset(const char *name, const char *description, const PresetType *input_types,
                               int input_count, PresetType output_type, const char *math_def, const char *complexity,
                               bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_ANALYSIS, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ============================================================
 * 模块注册实现
 * ============================================================ */

bool preset_measure_theory_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：σ代数与测度基础 (4个)
     * ============================================================ */

    /* σ代数构造 */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        if (register_mt_preset(
                PRESET_MT_SIGMA_ALGEBRA, "σ代数构造：由集合X和子集族C生成最小σ-代数 σ(C)", inputs, 2, PRESET_TYPE_SET,
                "\\sigma(\\mathcal{C}) = \\bigcap\\{"
                "\\Sigma : \\mathcal{C} \\subseteq \\Sigma, \\Sigma \\text{ 为 } \\sigma\\text{-代数}\\}",
                "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* Borel σ-代数 */
    {
        PresetType inputs[] = {PRESET_TYPE_TOPOLOGY};
        if (register_mt_preset(PRESET_MT_BOREL_ALGEBRA, "Borel σ-代数：由拓扑空间所有开集生成Borel σ-代数 B(X)", inputs,
                               1, PRESET_TYPE_SET, "\\mathcal{B}(X) = \\sigma(\\{U \\subseteq X : U \\text{ 开集}\\})",
                               "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* 测度空间 */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_FUNCTION};
        if (register_mt_preset(PRESET_MT_MEASURE_SPACE, "测度空间：构造测度空间 (X, Σ, μ) 满足 μ(∅) = 0", inputs, 3,
                               PRESET_TYPE_SPACE,
                               "(X, \\Sigma, \\mu),\\quad \\mu: \\Sigma \\to [0,+\\infty],\\quad \\mu(\\emptyset)=0",
                               "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 零测集判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_FUNCTION};
        if (register_mt_preset(PRESET_MT_NULL_SET, "零测集判定：判定集合 N 是否满足 μ(N) = 0", inputs, 2,
                               PRESET_TYPE_BOOLEAN, "\\mu(N) = 0 \\Rightarrow N \\text{ 为零测集}", "O(1)", false,
                               false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：测度构造 (4个)
     * ============================================================ */

    /* Lebesgue测度 */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_mt_preset(PRESET_MT_LEBESGUE_MEASURE, "Lebesgue测度：在 R^n 上构造Lebesgue测度 m", inputs, 1,
                               PRESET_TYPE_FUNCTION,
                               "m([a_1,b_1]\\times\\cdots\\times[a_n,b_n]) = \\prod_{i=1}^{n}(b_i-a_i)", "O(1)", true,
                               false)) {
            success_count++;
        }
    }

    /* 计数测度 */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        if (register_mt_preset(
                PRESET_MT_COUNTING_MEASURE, "计数测度：对任意集合赋予其元素个数", inputs, 1, PRESET_TYPE_FUNCTION,
                "\\#(A) = \\begin{cases} n & A \\text{ 有限}, |A|=n \\\\ +\\infty & A \\text{ 无穷} \\end{cases}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* Dirac测度 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT};
        if (register_mt_preset(PRESET_MT_DIRAC_MEASURE, "Dirac测度：在点 x0 处的集中测度 δ_{x0}", inputs, 1,
                               PRESET_TYPE_FUNCTION,
                               "\\delta_{x_0}(A) = \\begin{cases} 1 & x_0 \\in A \\\\ 0 & x_0 \\notin A \\end{cases}",
                               "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 外测度 */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_FUNCTION};
        if (register_mt_preset(PRESET_MT_OUTER_MEASURE, "外测度：计算集合A的外测度 μ*(A)，由可测集覆盖的下确界定义",
                               inputs, 2, PRESET_TYPE_SCALAR,
                               "\\mu^*(A) = \\inf\\left\\{\\sum_{i=1}^{\\infty}\\mu(E_i):"
                               "A\\subseteq\\bigcup_{i=1}^{\\infty}E_i, E_i\\in\\Sigma\\right\\}",
                               "O(\\infty)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：可测函数与积分 (4个)
     * ============================================================ */

    /* 可测函数判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SET};
        if (register_mt_preset(PRESET_MT_MEASURABLE_FUNCTION, "可测函数判定：f 可测 ⇔ ∀B ∈ B(R), f^{-1}(B) ∈ Σ", inputs,
                               2, PRESET_TYPE_BOOLEAN,
                               "f \\text{ 可测} \\Leftrightarrow "
                               "\\forall B\\in\\mathcal{B}(\\mathbb{R}): f^{-1}(B)\\in\\Sigma",
                               "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* 简单函数逼近 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_INTEGER};
        if (register_mt_preset(PRESET_MT_SIMPLE_FUNCTION, "简单函数逼近：用简单函数序列 s_n 逼近非负可测函数 f", inputs,
                               2, PRESET_TYPE_FUNCTION,
                               "s_n(x) = \\sum_{k=0}^{n2^n-1}\\frac{k}{2^n}"
                               "\\chi_{f^{-1}([k/2^n,(k+1)/2^n))}(x) + n\\chi_{f^{-1}([n,+\\infty))}(x)",
                               "O(n\\cdot 2^n)", true, false)) {
            success_count++;
        }
    }

    /* Lebesgue积分 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_FUNCTION};
        if (register_mt_preset(PRESET_MT_LEBESGUE_INTEGRAL,
                               "Lebesgue积分：计算 f 在 A 上关于 μ 的Lebesgue积分 ∫_A f dμ", inputs, 3,
                               PRESET_TYPE_SCALAR, "\\int_A f\\,d\\mu = \\int_A f^+\\,d\\mu - \\int_A f^-\\,d\\mu",
                               "O(\\infty)", true, false)) {
            success_count++;
        }
    }

    /* L^p空间范数 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_FUNCTION};
        if (register_mt_preset(
                PRESET_MT_LP_NORM, "L^p空间范数：计算 f 的 L^p 范数 ||f||_p", inputs, 3, PRESET_TYPE_SCALAR,
                "\\|f\\|_p = \\left(\\int |f|^p\\,d\\mu\\right)^{1/p},\\quad p \\geq 1", "O(\\infty)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：收敛定理 (4个)
     * ============================================================ */

    /* 单调收敛定理 */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_FUNCTION};
        if (register_mt_preset(PRESET_MT_MONOTONE_CONVERGENCE,
                               "单调收敛定理：若 0≤f1≤f2≤... 且 fn→f a.e.，则 lim∫fn dμ = ∫f dμ", inputs, 2,
                               PRESET_TYPE_SCALAR,
                               "0\\leq f_1\\leq f_2\\leq\\cdots,\\ f_n\\to f \\text{ a.e.}"
                               "\\Rightarrow \\lim\\int f_n\\,d\\mu = \\int f\\,d\\mu",
                               "O(\\infty)", true, false)) {
            success_count++;
        }
    }

    /* Fatou引理 */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_FUNCTION};
        if (register_mt_preset(PRESET_MT_FATOU_LEMMA, "Fatou引理：∫liminf fn dμ ≤ liminf ∫fn dμ", inputs, 2,
                               PRESET_TYPE_BOOLEAN,
                               "\\int\\liminf_{n\\to\\infty} f_n\\,d\\mu\\leq"
                               "\\liminf_{n\\to\\infty}\\int f_n\\,d\\mu",
                               "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* 控制收敛定理 */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_mt_preset(PRESET_MT_DOMINATED_CONVERGENCE,
                               "控制收敛定理：若 fn→f a.e. 且 |fn|≤g∈L^1，则lim∫fn dμ=∫f dμ", inputs, 3,
                               PRESET_TYPE_SCALAR,
                               "f_n\\to f\\text{ a.e.},\\ |f_n|\\leq g\\in L^1"
                               "\\Rightarrow \\lim\\int f_n\\,d\\mu = \\int f\\,d\\mu",
                               "O(\\infty)", true, false)) {
            success_count++;
        }
    }

    /* 几乎处处收敛 */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_mt_preset(PRESET_MT_ALMOST_EVERYWHERE, "几乎处处收敛：判定 fn 是否几乎处处收敛于 f", inputs, 3,
                               PRESET_TYPE_BOOLEAN, "\\mu(\\{x:\\lim f_n(x)\\neq f(x)\\}) = 0", "O(\\infty)", false,
                               false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：乘积测度与Fubini定理 (2个)
     * ============================================================ */

    /* 乘积测度 */
    {
        PresetType inputs[] = {PRESET_TYPE_SPACE, PRESET_TYPE_SPACE};
        if (register_mt_preset(PRESET_MT_PRODUCT_MEASURE, "乘积测度：构造两个σ-有限测度空间的乘积测度", inputs, 2,
                               PRESET_TYPE_SPACE, "(\\mu\\times\\nu)(A\\times B) = \\mu(A)\\cdot\\nu(B)", "O(1)", true,
                               false)) {
            success_count++;
        }
    }

    /* Fubini定理 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_mt_preset(PRESET_MT_FUBINI_THEOREM, "Fubini定理：若 f∈L^1(μ×ν)，则重积分可交换次序", inputs, 3,
                               PRESET_TYPE_SCALAR,
                               "\\int_{X\\times Y} f\\,d(\\mu\\times\\nu)="
                               "\\int_X\\left(\\int_Y f(x,y)\\,d\\nu(y)\\right)d\\mu(x)",
                               "O(\\infty)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：Radon-Nikodym导数 (2个)
     * ============================================================ */

    /* 绝对连续性 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_mt_preset(PRESET_MT_ABSOLUTE_CONTINUITY, "绝对连续性：ν≪μ ⇔ μ(A)=0 ⇒ ν(A)=0", inputs, 2,
                               PRESET_TYPE_BOOLEAN, "\\nu\\ll\\mu \\Leftrightarrow \\mu(A)=0\\Rightarrow\\nu(A)=0",
                               "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* Radon-Nikodym导数 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_mt_preset(PRESET_MT_RADON_NIKODYM, "Radon-Nikodym导数：当 ν≪μ 时，dν/dμ 满足 ν(A)=∫_A(dν/dμ)dμ",
                               inputs, 2, PRESET_TYPE_FUNCTION,
                               "\\frac{d\\nu}{d\\mu} = f,\\quad \\nu(A) = \\int_A f\\,d\\mu,\\ \\forall A\\in\\Sigma",
                               "O(\\infty)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == MEASURE_THEORY_PRESET_COUNT;
}

/* ============================================================
 * 模块信息接口
 * ============================================================ */

int preset_measure_theory_count(void) {
    return MEASURE_THEORY_PRESET_COUNT;
}

PresetCategory preset_measure_theory_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}

bool preset_measure_theory_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv00_malloc(MEASURE_THEORY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        /* σ代数与测度基础 */
        PRESET_MT_SIGMA_ALGEBRA,
        PRESET_MT_BOREL_ALGEBRA,
        PRESET_MT_MEASURE_SPACE,
        PRESET_MT_NULL_SET,
        /* 测度构造 */
        PRESET_MT_LEBESGUE_MEASURE,
        PRESET_MT_COUNTING_MEASURE,
        PRESET_MT_DIRAC_MEASURE,
        PRESET_MT_OUTER_MEASURE,
        /* 可测函数与积分 */
        PRESET_MT_MEASURABLE_FUNCTION,
        PRESET_MT_SIMPLE_FUNCTION,
        PRESET_MT_LEBESGUE_INTEGRAL,
        PRESET_MT_LP_NORM,
        /* 收敛定理 */
        PRESET_MT_MONOTONE_CONVERGENCE,
        PRESET_MT_FATOU_LEMMA,
        PRESET_MT_DOMINATED_CONVERGENCE,
        PRESET_MT_ALMOST_EVERYWHERE,
        /* 乘积测度与Fubini */
        PRESET_MT_PRODUCT_MEASURE,
        PRESET_MT_FUBINI_THEOREM,
        /* Radon-Nikodym */
        PRESET_MT_ABSOLUTE_CONTINUITY,
        PRESET_MT_RADON_NIKODYM,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) {
                {
                    void *tmp = names[j];
                    lv00_free(&tmp);
                }
            }
            {
                void *tmp = names;
                lv00_free(&tmp);
            }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
