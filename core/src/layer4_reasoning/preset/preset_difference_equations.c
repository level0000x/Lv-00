/**
 * @file preset_difference_equations.c
 * @brief 差分方程预设函数块模块 - 实现
 *
 * @details 实现差分方程模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共18个预设，涵盖线性差分方程、非线性差分方程、
 *          Z变换方法以及差分方程应用。
 *
 * 注意：宏名前缀使用 DE_DIFF_ 而非 DE_，以避免与微分方程模块冲突。
 *
 * @module DifferenceEquations
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 * @author Lv-00 Project
 */

#include "preset_difference_equations.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 差分方程模块预设函数块总数 */
#define DIFFERENCE_EQUATIONS_PRESET_COUNT 18

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 注册单个差分方程预设
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
static bool register_diff_preset(const char *name, const char *description, const PresetType *input_types,
                                 int input_count, PresetType output_type, const char *math_def, const char *complexity,
                                 bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_ANALYSIS, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ============================================================
 * 模块注册实现
 * ============================================================ */

bool preset_difference_equations_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：线性差分方程 (6个)
     *
     * 线性差分方程是离散时间系统的基本数学模型，
     * 广泛应用于数字信号处理、控制理论和组合数学。
     * 常系数线性差分方程可通过特征方程法求解。
     * ============================================================ */

    /* 线性齐次差分方程 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST};
        if (register_diff_preset(PRESET_DE_DIFF_LINEAR_HOMOGENEOUS,
                                 "线性齐次差分方程：求解 a_k y_{n+k} + ... + a_0 y_n = 0 的通解", inputs, 2,
                                 PRESET_TYPE_FUNCTION,
                                 "a_k y_{n+k} + a_{k-1} y_{n+k-1} + \\cdots + a_0 y_n = 0, "
                                 "\\quad y_n = \\sum_{i=1}^{k} c_i r_i^n "
                                 "\\text{（特征根互异时）}",
                                 "O(k^2)", true, false)) {
            success_count++;
        }
    }

    /* 线性非齐次差分方程 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_FUNCTION};
        if (register_diff_preset(PRESET_DE_DIFF_LINEAR_NONHOMOGENEOUS,
                                 "线性非齐次差分方程：求特解与齐次通解之和 y_n = y_h(n) + y_p(n)", inputs, 2,
                                 PRESET_TYPE_FUNCTION,
                                 "a_k y_{n+k} + \\cdots + a_0 y_n = f(n), "
                                 "\\quad y_n = y_h(n) + y_p(n), "
                                 "\\quad y_p \\text{ 由待定系数法或算子法求出}",
                                 "O(k^2)", true, false)) {
            success_count++;
        }
    }

    /* 特征方程法 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        if (register_diff_preset(PRESET_DE_DIFF_CHARACTERISTIC_EQUATION,
                                 "特征方程法：构造特征方程 a_k r^k + ... + a_0 = 0，根据根的类型确定通解形式", inputs,
                                 1, PRESET_TYPE_FUNCTION,
                                 "a_k r^k + a_{k-1} r^{k-1} + \\cdots + a_0 = 0, "
                                 "\\quad r \\text{ 单根}: c r^n; "
                                 "\\quad r \\text{ m重根}: (c_0 + c_1 n + \\cdots + c_{m-1} n^{m-1}) r^n",
                                 "O(k^3)", true, false)) {
            success_count++;
        }
    }

    /* 广义Fibonacci数列 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_diff_preset(PRESET_DE_DIFF_FIBONACCI_GENERALIZED,
                                 "广义Fibonacci数列：F_n = aF_{n-1} + bF_{n-2}，给定初始条件求通项公式", inputs, 4,
                                 PRESET_TYPE_FUNCTION,
                                 "F_n = a F_{n-1} + b F_{n-2}, \\quad F_0 = \\alpha, \\ F_1 = \\beta, "
                                 "\\quad r^2 - ar - b = 0, "
                                 "\\quad F_n = A r_1^n + B r_2^n",
                                 "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 线性差分方程组 */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_VECTOR};
        if (register_diff_preset(PRESET_DE_DIFF_SYSTEM_LINEAR,
                                 "线性差分方程组：矩阵形式 x_{n+1} = A x_n 求解，x_n = A^n x_0", inputs, 2,
                                 PRESET_TYPE_FUNCTION,
                                 "\\mathbf{x}_{n+1} = A \\mathbf{x}_n, "
                                 "\\quad \\mathbf{x}_n = A^n \\mathbf{x}_0, "
                                 "\\quad A^n = P \\Lambda^n P^{-1} \\text{（可对角化时）}",
                                 "O(k^3)", true, false)) {
            success_count++;
        }
    }

    /* 离散稳定性分析 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        if (register_diff_preset(PRESET_DE_DIFF_STABILITY_DISCRETE,
                                 "离散稳定性分析：判定差分方程的稳定性（特征根模均<1时渐近稳定）", inputs, 1,
                                 PRESET_TYPE_BOOLEAN,
                                 "\\text{渐近稳定} \\Leftrightarrow |r_i| < 1, \\forall i, "
                                 "\\quad \\text{稳定} \\Leftrightarrow |r_i| \\leq 1, "
                                 "\\ |r_i|=1 \\Rightarrow \\text{单根}",
                                 "O(k^2)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：非线性差分方程 (4个)
     *
     * 非线性差分方程可以展现丰富的动力学行为，
     * 包括不动点、周期轨道和混沌。Logistic映射是
     * 混沌理论中最经典的例子之一。
     * ============================================================ */

    /* Riccati差分方程 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR,
                               PRESET_TYPE_SCALAR};
        if (register_diff_preset(PRESET_DE_DIFF_RICCATI_DIFFERENCE,
                                 "Riccati差分方程：y_{n+1} = (a*y_n + b)/(c*y_n + d)，可化为线性方程组求解", inputs, 5,
                                 PRESET_TYPE_FUNCTION,
                                 "y_{n+1} = \\frac{a y_n + b}{c y_n + d}, "
                                 "\\quad \\begin{pmatrix} y_{n+1} \\\\ 1 \\end{pmatrix} "
                                 "\\propto \\begin{pmatrix} a & b \\\\ c & d \\end{pmatrix}"
                                 "\\begin{pmatrix} y_n \\\\ 1 \\end{pmatrix}",
                                 "O(n)", true, false)) {
            success_count++;
        }
    }

    /* Logistic映射 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_diff_preset(PRESET_DE_DIFF_LOGISTIC_MAP, "Logistic映射：x_{n+1} = rx_n(1-x_n)，分析分岔与混沌行为",
                                 inputs, 2, PRESET_TYPE_SEQUENCE,
                                 "x_{n+1} = r x_n (1 - x_n), \\quad 0 \\leq x_n \\leq 1, "
                                 "\\quad r \\in [0, 4], "
                                 "\\quad r > 3.57 \\Rightarrow \\text{混沌}",
                                 "O(n)", true, false)) {
            success_count++;
        }
    }

    /* Mandelbrot迭代 */
    {
        PresetType inputs[] = {PRESET_TYPE_COMPLEX, PRESET_TYPE_INTEGER};
        if (register_diff_preset(PRESET_DE_DIFF_MANDELBROT_ITERATION,
                                 "Mandelbrot迭代：z_{n+1} = z_n^2 + c，判定c是否属于Mandelbrot集", inputs, 2,
                                 PRESET_TYPE_BOOLEAN,
                                 "z_{n+1} = z_n^2 + c, \\quad z_0 = 0, "
                                 "\\quad c \\in \\mathcal{M} \\Leftrightarrow "
                                 "\\sup_n |z_n| < \\infty, "
                                 "\\quad |z_n| > 2 \\Rightarrow \\text{发散}",
                                 "O(n)", false, false)) {
            success_count++;
        }
    }

    /* 离散Lyapunov指数 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_diff_preset(PRESET_DE_DIFF_LYAPUNOV_EXPONENT_DISCRETE,
                                 "离散Lyapunov指数：lambda = lim (1/N) sum ln|f'(x_n)|，判定混沌行为", inputs, 2,
                                 PRESET_TYPE_SCALAR,
                                 "\\lambda = \\lim_{N \\to \\infty} \\frac{1}{N} "
                                 "\\sum_{n=0}^{N-1} \\ln |f'(x_n)|, "
                                 "\\quad \\lambda > 0 \\Rightarrow \\text{混沌}, "
                                 "\\quad \\lambda < 0 \\Rightarrow \\text{稳定周期}",
                                 "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：Z变换 (5个)
     *
     * Z变换是分析线性时不变离散系统的重要工具，
     * 类似于连续系统中的Laplace变换。通过Z变换
     * 可以将差分方程转化为代数方程求解。
     * ============================================================ */

    /* Z变换 */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_diff_preset(PRESET_DE_DIFF_Z_TRANSFORM,
                                 "Z变换：Z{f_n} = sum_{n=0}^{inf} f_n z^{-n}，将时域序列映射到Z域", inputs, 1,
                                 PRESET_TYPE_FUNCTION,
                                 "F(z) = \\mathcal{Z}\\{f_n\\} = \\sum_{n=0}^{\\infty} f_n z^{-n}, "
                                 "\\quad \\mathcal{Z}\\{a^n u_n\\} = \\frac{z}{z - a}, "
                                 "\\quad |z| > |a|",
                                 "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 逆Z变换 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_diff_preset(PRESET_DE_DIFF_INVERSE_Z_TRANSFORM, "逆Z变换：部分分式展开法，将Z域函数还原为时域序列",
                                 inputs, 1, PRESET_TYPE_SEQUENCE,
                                 "f_n = \\mathcal{Z}^{-1}\\{F(z)\\} = "
                                 "\\frac{1}{2\\pi j} \\oint F(z) z^{n-1} dz, "
                                 "\\quad \\text{或部分分式展开 + 查表}",
                                 "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* Z传递函数 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST};
        if (register_diff_preset(PRESET_DE_DIFF_Z_TRANSFER_FUNCTION,
                                 "Z传递函数：H(z) = Y(z)/X(z)，由差分方程系数构造传递函数", inputs, 2,
                                 PRESET_TYPE_FUNCTION,
                                 "H(z) = \\frac{Y(z)}{X(z)} = "
                                 "\\frac{b_0 + b_1 z^{-1} + \\cdots + b_m z^{-m}}"
                                 "{a_0 + a_1 z^{-1} + \\cdots + a_k z^{-k}}",
                                 "O(n)", true, false)) {
            success_count++;
        }
    }

    /* Z域稳定性 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_diff_preset(PRESET_DE_DIFF_Z_STABILITY, "Z域稳定性：判定离散系统稳定性（所有极点在单位圆内）",
                                 inputs, 1, PRESET_TYPE_BOOLEAN,
                                 "\\text{BIBO稳定} \\Leftrightarrow "
                                 "H(z) \\text{ 所有极点 } p_i \\text{ 满足 } |p_i| < 1, "
                                 "\\quad \\text{或 Jury 稳定性判据}",
                                 "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* Z域频率响应 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_diff_preset(PRESET_DE_DIFF_Z_FREQUENCY_RESPONSE,
                                 "Z域频率响应：H(e^{j*omega})，计算离散系统的幅频和相频特性", inputs, 2,
                                 PRESET_TYPE_COMPLEX,
                                 "H(e^{j\\omega}) = |H(e^{j\\omega})| e^{j\\angle H(e^{j\\omega})}, "
                                 "\\quad \\omega \\in [-\\pi, \\pi], "
                                 "\\quad z = e^{j\\omega} \\text{ 单位圆上的值}",
                                 "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：差分方程应用 (3个)
     *
     * 差分方程在数值分析和组合数学中有广泛应用。
     * 有限差分法是微分方程数值求解的基础方法，
     * 生成函数法是求解递推关系的经典工具。
     * ============================================================ */

    /* 有限差分法 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_diff_preset(PRESET_DE_DIFF_FINITE_DIFFERENCE,
                                 "有限差分法：用差分近似微分，构造差分格式求解微分方程", inputs, 3, PRESET_TYPE_LIST,
                                 "f'(x) \\approx \\frac{f(x+h) - f(x)}{h}, "
                                 "\\quad f''(x) \\approx \\frac{f(x+h) - 2f(x) + f(x-h)}{h^2}, "
                                 "\\quad \\text{向前/向后/中心差分}",
                                 "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 递推关系求解 */
    {
        PresetType inputs[] = {PRESET_TYPE_EQUATION, PRESET_TYPE_LIST};
        if (register_diff_preset(PRESET_DE_DIFF_RECURRENCE_SOLVE,
                                 "递推关系求解：用生成函数法求解递推关系 a_n = f(a_{n-1}, ..., a_{n-k})", inputs, 2,
                                 PRESET_TYPE_FUNCTION,
                                 "G(x) = \\sum_{n=0}^{\\infty} a_n x^n, "
                                 "\\quad \\text{由递推关系构造 } G(x) \\text{ 的方程}, "
                                 "\\quad a_n = [x^n] G(x)",
                                 "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* 组合计数递推 */
    {
        PresetType inputs[] = {PRESET_TYPE_STRING};
        if (register_diff_preset(PRESET_DE_DIFF_COMBINATORIAL_RECURRENCE,
                                 "组合计数递推：求解Catalan数、Stirling数、Bell数等经典组合递推", inputs, 1,
                                 PRESET_TYPE_FUNCTION,
                                 "C_n = \\frac{1}{n+1}\\binom{2n}{n}, \\quad "
                                 "S(n,k) = k S(n-1,k) + S(n-1,k-1), \\quad "
                                 "B_n = \\sum_{k=0}^{n} S(n,k)",
                                 "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == DIFFERENCE_EQUATIONS_PRESET_COUNT;
}

/* ============================================================
 * 模块信息接口
 * ============================================================ */

int preset_difference_equations_count(void) {
    return DIFFERENCE_EQUATIONS_PRESET_COUNT;
}

PresetCategory preset_difference_equations_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}

bool preset_difference_equations_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    /* 分配名称数组 */
    char **names = (char **) lv00_malloc(DIFFERENCE_EQUATIONS_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 线性差分方程 */
        PRESET_DE_DIFF_LINEAR_HOMOGENEOUS,
        PRESET_DE_DIFF_LINEAR_NONHOMOGENEOUS,
        PRESET_DE_DIFF_CHARACTERISTIC_EQUATION,
        PRESET_DE_DIFF_FIBONACCI_GENERALIZED,
        PRESET_DE_DIFF_SYSTEM_LINEAR,
        PRESET_DE_DIFF_STABILITY_DISCRETE,
        /* 非线性差分方程 */
        PRESET_DE_DIFF_RICCATI_DIFFERENCE,
        PRESET_DE_DIFF_LOGISTIC_MAP,
        PRESET_DE_DIFF_MANDELBROT_ITERATION,
        PRESET_DE_DIFF_LYAPUNOV_EXPONENT_DISCRETE,
        /* Z变换 */
        PRESET_DE_DIFF_Z_TRANSFORM,
        PRESET_DE_DIFF_INVERSE_Z_TRANSFORM,
        PRESET_DE_DIFF_Z_TRANSFER_FUNCTION,
        PRESET_DE_DIFF_Z_STABILITY,
        PRESET_DE_DIFF_Z_FREQUENCY_RESPONSE,
        /* 差分方程应用 */
        PRESET_DE_DIFF_FINITE_DIFFERENCE,
        PRESET_DE_DIFF_RECURRENCE_SOLVE,
        PRESET_DE_DIFF_COMBINATORIAL_RECURRENCE,
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
