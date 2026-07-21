/**
 * @file preset_numerical_analysis.c
 * @brief 数值分析预设函数块 - 实现
 *
 * 实现理论数学研究中常用的数值分析预设函数块。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module NumericalAnalysis
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 4.0.0
 */

#include "preset_numerical_analysis.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 数值分析模块预设函数块总数 */

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个数值分析预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有数值分析预设都属于 PRESET_CATEGORY_ANALYSIS 类别。
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
static bool register_numerical_analysis_preset(
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

/* ==================== 模块注册实现 ==================== */

int preset_numerical_analysis_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：数值积分
     * ============================================================ */

    /* -------------------- 梯形法则 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_numerical_analysis_preset(
                PRESET_NUMERICAL_INTEGRAL_TRAPEZOID,
                "梯形法则数值积分：∫ₐᵇ f(x)dx ≈ h/2·[f(a) + 2Σf(xᵢ) + f(b)]，h = (b-a)/n",
                inputs, 4, PRESET_TYPE_SCALAR,
                "\\int_a^b f(x)\\,dx \\approx \\frac{h}{2}\\left[f(a) + 2\\sum_{i=1}^{n-1} f(x_i) + f(b)\\right]",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 辛普森法则 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_numerical_analysis_preset(
                PRESET_NUMERICAL_INTEGRAL_SIMPSON,
                "辛普森法则数值积分：∫ₐᵇ f(x)dx ≈ h/3·[f(a) + 4Σf(x₂ᵢ₋₁) + 2Σf(x₂ᵢ) + f(b)]",
                inputs, 4, PRESET_TYPE_SCALAR,
                "\\int_a^b f(x)\\,dx \\approx \\frac{h}{3}\\left[f(a) + 4\\sum f(x_{2i-1}) + 2\\sum f(x_{2i}) + f(b)\\right]",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 高斯求积 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_numerical_analysis_preset(
                PRESET_NUMERICAL_INTEGRAL_GAUSS,
                "高斯-勒让德求积：∫ₐᵇ f(x)dx ≈ (b-a)/2·Σ wᵢ·f((b-a)/2·tᵢ + (a+b)/2)",
                inputs, 4, PRESET_TYPE_SCALAR,
                "\\int_a^b f(x)\\,dx \\approx \\frac{b-a}{2} \\sum_{i=1}^{n} w_i \\cdot f\\left(\\frac{b-a}{2}t_i + \\frac{a+b}{2}\\right)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 龙贝格积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_numerical_analysis_preset(
                PRESET_NUMERICAL_INTEGRAL_ROMBERG,
                "龙贝格积分：通过理查森外推递推加速梯形序列，达到高精度积分",
                inputs, 4, PRESET_TYPE_SCALAR,
                "R_{m,k} = \\frac{4^k R_{m,k-1} - R_{m-1,k-1}}{4^k - 1}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 蒙特卡洛积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_numerical_analysis_preset(
                PRESET_NUMERICAL_INTEGRAL_MONTE_CARLO,
                "蒙特卡洛积分：∫ₐᵇ f(x)dx ≈ (b-a)/N·Σ f(xᵢ)，xᵢ 为均匀随机采样",
                inputs, 4, PRESET_TYPE_SCALAR,
                "\\int_a^b f(x)\\,dx \\approx \\frac{b-a}{N} \\sum_{i=1}^{N} f(x_i), \\quad x_i \\sim U(a,b)",
                "O(N)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：方程求解
     * ============================================================ */

    /* -------------------- 二分法求根 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_ROOT_BISECTION,
                "二分法求根：在区间 [a,b] 中迭代缩小区间，收敛精度 |b-a| < ε",
                inputs, 4, PRESET_TYPE_SCALAR,
                "x_{n+1} = \\frac{a_n + b_n}{2}, \\quad f(a_n) \\cdot f(b_n) < 0",
                "O(log((b-a)/ε))", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 牛顿法求根 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_ROOT_NEWTON,
                "牛顿法求根：xₙ₊₁ = xₙ - f(xₙ)/f'(xₙ)，二阶收敛速度",
                inputs, 4, PRESET_TYPE_SCALAR,
                "x_{n+1} = x_n - \\frac{f(x_n)}{f'(x_n)}",
                "O(log(1/ε))", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 割线法求根 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_ROOT_SECANT,
                "割线法求根：xₙ₊₁ = xₙ - f(xₙ)·(xₙ - xₙ₋₁)/(f(xₙ) - f(xₙ₋₁))，超线性收敛",
                inputs, 4, PRESET_TYPE_SCALAR,
                "x_{n+1} = x_n - f(x_n) \\cdot \\frac{x_n - x_{n-1}}{f(x_n) - f(x_{n-1})}",
                "O(log²(1/ε))", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 不动点迭代 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_ROOT_FIXED_POINT,
                "不动点迭代：xₙ₊₁ = g(xₙ)，当 |g'(x*)| < 1 时收敛到不动点 x*",
                inputs, 3, PRESET_TYPE_SCALAR,
                "x_{n+1} = g(x_n), \\quad x^* = g(x^*)",
                "O(log(1/ε))", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 非线性方程组牛顿法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_SYSTEM_NEWTON,
                "非线性方程组牛顿法：Xₙ₊₁ = Xₙ - J⁻¹(Xₙ)·F(Xₙ)，J 为雅可比矩阵",
                inputs, 3, PRESET_TYPE_TUPLE,
                "\\mathbf{X}_{n+1} = \\mathbf{X}_n - J^{-1}(\\mathbf{X}_n) \\cdot \\mathbf{F}(\\mathbf{X}_n)",
                "O(n³·log(1/ε))", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：插值方法
     * ============================================================ */

    /* -------------------- 拉格朗日插值 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_INTERPOLATION_LAGRANGE,
                "拉格朗日插值：L(x) = Σ yᵢ·Π(x - xⱼ)/(xᵢ - xⱼ)，j≠i",
                inputs, 3, PRESET_TYPE_SCALAR,
                "L(x) = \\sum_{i=0}^{n} y_i \\prod_{\\substack{j=0 \\\\ j \\ne i}}^{n} \\frac{x - x_j}{x_i - x_j}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 牛顿均差插值 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_INTERPOLATION_NEWTON,
                "牛顿均差插值：N(x) = f[x₀] + f[x₀,x₁](x-x₀) + f[x₀,x₁,x₂](x-x₀)(x-x₁) + ...",
                inputs, 3, PRESET_TYPE_SCALAR,
                "N(x) = f[x_0] + \\sum_{k=1}^{n} f[x_0, \\ldots, x_k] \\prod_{i=0}^{k-1}(x - x_i)",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 线性样条插值 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_INTERPOLATION_SPLINE_LINEAR,
                "线性样条插值：相邻节点间用线性函数连接，分段连续但一阶导数不连续",
                inputs, 3, PRESET_TYPE_SCALAR,
                "S_i(x) = y_i + \\frac{y_{i+1} - y_i}{x_{i+1} - x_i}(x - x_i)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 三次样条插值 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_INTERPOLATION_SPLINE_CUBIC,
                "三次样条插值：分段三次多项式，保证函数值、一阶和二阶导数连续",
                inputs, 3, PRESET_TYPE_SCALAR,
                "S_i(x) = a_i + b_i(x-x_i) + c_i(x-x_i)^2 + d_i(x-x_i)^3",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 切比雪夫插值 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_numerical_analysis_preset(
                PRESET_INTERPOLATION_CHEBYSHEV,
                "切比雪夫插值：在切比雪夫节点 xₖ = cos((2k+1)π/(2n)) 上插值，最小化龙格现象",
                inputs, 4, PRESET_TYPE_SCALAR,
                "x_k = \\cos\\left(\\frac{(2k+1)\\pi}{2n}\\right), \\quad k = 0, 1, \\ldots, n-1",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：数值微分
     * ============================================================ */

    /* -------------------- 前向差分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_DIFFERENTIATION_FORWARD,
                "前向差分近似导数：f'(x) ≈ (f(x+h) - f(x)) / h，一阶精度 O(h)",
                inputs, 3, PRESET_TYPE_SCALAR,
                "f'(x) \\approx \\frac{f(x+h) - f(x)}{h} + O(h)",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 后向差分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_DIFFERENTIATION_BACKWARD,
                "后向差分近似导数：f'(x) ≈ (f(x) - f(x-h)) / h，一阶精度 O(h)",
                inputs, 3, PRESET_TYPE_SCALAR,
                "f'(x) \\approx \\frac{f(x) - f(x-h)}{h} + O(h)",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 中心差分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_DIFFERENTIATION_CENTRAL,
                "中心差分近似导数：f'(x) ≈ (f(x+h) - f(x-h)) / (2h)，二阶精度 O(h²)",
                inputs, 3, PRESET_TYPE_SCALAR,
                "f'(x) \\approx \\frac{f(x+h) - f(x-h)}{2h} + O(h^2)",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 理查森外推 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_numerical_analysis_preset(
                PRESET_DIFFERENTIATION_RICHARDSON,
                "理查森外推：通过组合不同步长的差分结果消除低阶误差项，提高导数精度",
                inputs, 4, PRESET_TYPE_SCALAR,
                "D(h) = \\frac{2^{2n} D(h/2) - D(h)}{2^{2n} - 1} + O(h^{2n+2})",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：常微分方程
     * ============================================================ */

    /* -------------------- 欧拉方法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_numerical_analysis_preset(
                PRESET_ODE_EULER,
                "欧拉方法求解ODE：yₙ₊₁ = yₙ + h·f(xₙ, yₙ)，一阶精度 O(h)",
                inputs, 5, PRESET_TYPE_LIST,
                "y_{n+1} = y_n + h \\cdot f(x_n, y_n)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 四阶龙格-库塔方法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_numerical_analysis_preset(
                PRESET_ODE_RK4,
                "四阶龙格-库塔方法：yₙ₊₁ = yₙ + h/6·(k₁ + 2k₂ + 2k₃ + k₄)，四阶精度 O(h⁴)",
                inputs, 5, PRESET_TYPE_LIST,
                "y_{n+1} = y_n + \\frac{h}{6}(k_1 + 2k_2 + 2k_3 + k_4)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 自适应步长方法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_ODE_ADAPTIVE,
                "自适应步长方法（RK45）：根据局部误差估计自动调整步长，平衡精度与效率",
                inputs, 5, PRESET_TYPE_LIST,
                "h_{\\text{new}} = h \\cdot \\left(\\frac{\\varepsilon \\cdot h}{|y_4 - y_5|}\\right)^{1/5}",
                "O(n·log n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ODE方程组求解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_numerical_analysis_preset(
                PRESET_ODE_SYSTEM,
                "ODE方程组求解：使用RK4方法同时求解多个耦合的一阶常微分方程",
                inputs, 5, PRESET_TYPE_LIST,
                "\\mathbf{y}'(x) = \\mathbf{f}(x, \\mathbf{y}), \\quad \\mathbf{y}(x_0) = \\mathbf{y}_0",
                "O(n·m³)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：矩阵运算
     * ============================================================ */

    /* -------------------- LU分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_numerical_analysis_preset(
                PRESET_MATRIX_LU_DECOMPOSE,
                "LU分解：将矩阵 A 分解为下三角矩阵 L 和上三角矩阵 U，A = LU，用于高效求解线性方程组",
                inputs, 1, PRESET_TYPE_TUPLE,
                "A = LU, \\quad L_{ii} = 1, \\quad U_{ij} = 0 \\ (i > j), \\quad L_{ij} = 0 \\ (i < j)",
                "O(n³)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 特征值计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_SCALAR};
        if (register_numerical_analysis_preset(
                PRESET_MATRIX_EIGENVALUES,
                "特征值计算：使用QR算法迭代求解矩阵的所有特征值，Av = λv",
                inputs, 2, PRESET_TYPE_TUPLE,
                "A\\mathbf{v} = \\lambda \\mathbf{v}, \\quad \\det(A - \\lambda I) = 0",
                "O(n³)", true, false)) {
            success_count++;
        }
    }

    return (success_count == NUMERICAL_ANALYSIS_PRESET_COUNT);
}

int preset_numerical_analysis_count(void)
{
    return NUMERICAL_ANALYSIS_PRESET_COUNT;
}
