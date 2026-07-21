/**
 * @file preset_differential_equations.c
 * @brief 微分方程预设函数块 - 实现
 *
 * @details 实现微分方程模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共20个预设，涵盖ODE求解方法、特殊ODE、PDE基本方法、
 *          存在唯一性判定、稳定性分析和数值近似方法。
 *
 * @module DifferentialEquations
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 */

#include "preset_differential_equations.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 微分方程模块预设函数块总数 */

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 注册单个微分方程预设
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
static bool register_de_preset(
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
        PRESET_CATEGORY_ANALYSIS,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ============================================================
 * 模块注册实现
 * ============================================================ */

int preset_differential_equations_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：ODE求解方法 (4个)
     * ============================================================ */

    /* 分离变量法 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_de_preset(
                PRESET_DE_SEPARABLE_METHOD,
                "分离变量法：求解 dy/dx = f(x)g(y)，分离变量后积分得通解",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "\\int \\frac{dy}{g(y)} = \\int f(x)\\,dx + C",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 积分因子法 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_de_preset(
                PRESET_DE_INTEGRATING_FACTOR,
                "积分因子法：求解 y' + P(x)y = Q(x)，μ(x) = exp(∫P dx)",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "\\mu(x) = e^{\\int P(x)\\,dx},"
                "\\quad y = \\frac{1}{\\mu(x)}\\left(\\int \\mu(x)Q(x)\\,dx + C\\right)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 常数变易法 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_de_preset(
                PRESET_DE_VARIATION_CONSTANTS,
                "常数变易法：用齐次解构造非齐次线性ODE的特解",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "y_p = \\sum_{i=1}^{n} u_i(x) y_i(x),"
                "\\quad \\sum u_i' y_i^{(k)} = 0\\ (k < n-1),"
                "\\quad \\sum u_i' y_i^{(n-1)} = g(x)",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* 特征方程法 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        if (register_de_preset(
                PRESET_DE_CHARACTERISTIC_EQ,
                "特征方程法：构造并求解常系数齐次线性ODE的特征方程",
                inputs, 1, PRESET_TYPE_FUNCTION,
                "a_n r^n + a_{n-1} r^{n-1} + \\cdots + a_0 = 0",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：特殊ODE (3个)
     * ============================================================ */

    /* Bernouli方程 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_de_preset(
                PRESET_DE_BERNOULLI_EQ,
                "Bernoulli方程：y' + P(x)y = Q(x)y^n，代换 v = y^{1-n} 化为一阶线性方程",
                inputs, 3, PRESET_TYPE_FUNCTION,
                "y' + P(x)y = Q(x)y^n,"
                "\\quad v = y^{1-n},"
                "\\quad v' + (1-n)P(x)v = (1-n)Q(x)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* Riccati方程 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION,
                               PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_de_preset(
                PRESET_DE_RICCATI_EQ,
                "Riccati方程：y' = P(x) + Q(x)y + R(x)y^2，已知特解时通过 y = y_1 + 1/v 降阶",
                inputs, 4, PRESET_TYPE_FUNCTION,
                "y' = P(x) + Q(x)y + R(x)y^2,"
                "\\quad y = y_1 + \\frac{1}{v}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 恰当方程 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_de_preset(
                PRESET_DE_EXACT_EQ,
                "恰当方程：Mdx + Ndy = 0，满足 dM/dy = dN/dx 时求势函数 F(x,y) = C",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "M(x,y)dx + N(x,y)dy = 0,"
                "\\quad \\frac{\\partial M}{\\partial y} = \\frac{\\partial N}{\\partial x},"
                "\\quad F(x,y) = C",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：ODE补充 (3个)
     * ============================================================ */

    /* 一阶线性ODE */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_de_preset(
                PRESET_DE_LINEAR_FIRST_ORDER,
                "一阶线性ODE：求解标准形式 y' + P(x)y = Q(x) 的解析通解",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "y = e^{-\\int P dx}\\left(\\int Q \\cdot e^{\\int P dx} dx + C\\right)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 齐次ODE */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_de_preset(
                PRESET_DE_HOMOGENEOUS_ODE,
                "齐次ODE：y' = F(y/x)，代换 v = y/x 化为可分离变量方程",
                inputs, 1, PRESET_TYPE_FUNCTION,
                "\\frac{dy}{dx} = F\\left(\\frac{y}{x}\\right),"
                "\\quad v = \\frac{y}{x},\\quad \\frac{dv}{F(v)-v} = \\frac{dx}{x}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* Cauchy-Euler方程 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_FUNCTION};
        if (register_de_preset(
                PRESET_DE_CAUCHY_EULER,
                "Cauchy-Euler方程：x^n y^{(n)} + ... + a_0 y = g(x)，代换 x = e^t 化为常系数方程",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "x^n y^{(n)} + a_{n-1}x^{n-1}y^{(n-1)} + \\cdots + a_0 y = g(x)",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：PDE基本 (2个)
     * ============================================================ */

    /* PDE分离变量法 */
    {
        PresetType inputs[] = {PRESET_TYPE_EQUATION};
        if (register_de_preset(
                PRESET_DE_PDE_SEPARABLE,
                "PDE分离变量法：设 u(x,t) = X(x)T(t) 将偏微分方程分离为两个ODE",
                inputs, 1, PRESET_TYPE_LIST,
                "u(x,t) = X(x)T(t) \\Rightarrow "
                "\\frac{X''}{X} = \\frac{T'}{\\alpha^2 T} = -\\lambda",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 特征线法 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_de_preset(
                PRESET_DE_CHARACTERISTIC_LINE,
                "特征线法：对 a(x,y,u)u_x + b(x,y,u)u_y = c(x,y,u) 构造特征线方程组",
                inputs, 3, PRESET_TYPE_EQUATION,
                "\\frac{dx}{a} = \\frac{dy}{b} = \\frac{du}{c}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：存在唯一性 (2个)
     * ============================================================ */

    /* Picard-Lindelof存在唯一性 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_de_preset(
                PRESET_DE_EXISTENCE_UNIQUENESS,
                "存在唯一性判定：对 y' = f(t,y), y(t_0)=y_0，判定是否满足Picard-Lindelof条件",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "|f(t, y_1) - f(t, y_2)| \\leq L|y_1 - y_2| \\text{ (Lipschitz条件)}",
                "O(n)", false, false)) {
            success_count++;
        }
    }

    /* Lipschitz条件判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_REGION};
        if (register_de_preset(
                PRESET_DE_LIPSCHITZ_CHECK,
                "Lipschitz条件判定：验证 f(t,y) 在给定区域上是否关于 y 满足Lipschitz条件",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "\\exists L > 0: |f(t, y_1) - f(t, y_2)| \\leq L|y_1 - y_2|,"
                "\\ \\forall (t,y_1),(t,y_2) \\in D",
                "O(n)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：稳定性分析 (3个)
     * ============================================================ */

    /* Lyapunov稳定性 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_de_preset(
                PRESET_DE_LYAPUNOV_STABILITY,
                "Lyapunov稳定性：用Lyapunov函数 V(x) 判定平衡点的稳定性",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "V(x) > 0\\ (x \\neq 0),\\ V(0) = 0,"
                "\\quad \\dot{V}(x) = \\nabla V \\cdot f(x) \\leq 0",
                "O(n)", false, false)) {
            success_count++;
        }
    }

    /* 渐近稳定性 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_de_preset(
                PRESET_DE_ASYMPTOTIC_STABILITY,
                "渐近稳定性：若 V 正定且 dV/dt 负定，则平衡点渐近稳定",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "V(x) > 0,\\ \\dot{V}(x) < 0\\ (x \\neq 0) "
                "\\Rightarrow \\lim_{t\\to\\infty} x(t) = 0",
                "O(n)", false, false)) {
            success_count++;
        }
    }

    /* 相平面分析 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_de_preset(
                PRESET_DE_PHASE_PLANE,
                "相平面分析：对二维自治系统进行平衡点分类与定性分析",
                inputs, 2, PRESET_TYPE_STRING,
                "\\dot{x} = f(x,y),\\ \\dot{y} = g(x,y),"
                "\\quad J = \\begin{pmatrix} f_x & f_y \\\\ g_x & g_y \\end{pmatrix}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第七部分：数值与近似 (3个)
     * ============================================================ */

    /* Euler方法 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR,
                               PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_de_preset(
                PRESET_DE_EULER_METHOD,
                "Euler方法：用显式Euler法数值求解初值问题 y' = f(t,y)",
                inputs, 5, PRESET_TYPE_LIST,
                "y_{n+1} = y_n + h\\,f(t_n, y_n),\\quad t_{n+1} = t_n + h",
                "O(N)", true, false)) {
            success_count++;
        }
    }

    /* Picard迭代 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_de_preset(
                PRESET_DE_PICARD_ITERATION,
                "Picard迭代：y_{n+1}(t) = y_0 + ∫_{t_0}^t f(s, y_n(s)) ds，逼近初值问题真解",
                inputs, 3, PRESET_TYPE_FUNCTION,
                "y_{n+1}(t) = y_0 + \\int_{t_0}^{t} f(s, y_n(s))\\,ds",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 幂级数解法 */
    {
        PresetType inputs[] = {PRESET_TYPE_EQUATION, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_de_preset(
                PRESET_DE_SERIES_SOLUTION,
                "幂级数解法：设 y = Σ a_n (x-x0)^n，代入ODE逐次比较系数求级数解",
                inputs, 3, PRESET_TYPE_FUNCTION,
                "y = \\sum_{n=0}^{N} a_n (x - x_0)^n",
                "O(N^2)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == DIFFERENTIAL_EQUATIONS_PRESET_COUNT;
}

/* ============================================================
 * 模块信息接口
 * ============================================================ */

int preset_differential_equations_count(void)
{
    return DIFFERENTIAL_EQUATIONS_PRESET_COUNT;
}

PresetCategory preset_differential_equations_category(void)
{
    return PRESET_CATEGORY_ANALYSIS;
}

bool preset_differential_equations_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    /* 分配名称数组 */
    char **names = (char**)lv00_malloc(DIFFERENTIAL_EQUATIONS_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* ODE求解方法 */
        PRESET_DE_SEPARABLE_METHOD,
        PRESET_DE_INTEGRATING_FACTOR,
        PRESET_DE_VARIATION_CONSTANTS,
        PRESET_DE_CHARACTERISTIC_EQ,
        /* 特殊ODE */
        PRESET_DE_BERNOULLI_EQ,
        PRESET_DE_RICCATI_EQ,
        PRESET_DE_EXACT_EQ,
        /* ODE补充 */
        PRESET_DE_LINEAR_FIRST_ORDER,
        PRESET_DE_HOMOGENEOUS_ODE,
        PRESET_DE_CAUCHY_EULER,
        /* PDE基本 */
        PRESET_DE_PDE_SEPARABLE,
        PRESET_DE_CHARACTERISTIC_LINE,
        /* 存在唯一性 */
        PRESET_DE_EXISTENCE_UNIQUENESS,
        PRESET_DE_LIPSCHITZ_CHECK,
        /* 稳定性分析 */
        PRESET_DE_LYAPUNOV_STABILITY,
        PRESET_DE_ASYMPTOTIC_STABILITY,
        PRESET_DE_PHASE_PLANE,
        /* 数值与近似 */
        PRESET_DE_EULER_METHOD,
        PRESET_DE_PICARD_ITERATION,
        PRESET_DE_SERIES_SOLUTION,
    };

    int count = (int)(sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) {
                { void *tmp = names[j]; lv00_free(&tmp); }
            }
            { void *tmp = names; lv00_free(&tmp); }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}