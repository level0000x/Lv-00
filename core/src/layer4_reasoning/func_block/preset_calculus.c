/**
 * @file preset_calculus.c
 * @brief 微积分预设函数块 - 实现
 *
 * 实现理论数学研究中常用的微积分运算预设函数块。
 * 涵盖极限、微分、积分、级数展开及多元微积分。
 *
 * @module Calculus
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 */

#include "preset_calculus.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 微积分模块预设函数块总数 */
#ifndef CALCULUS_PRESET_COUNT
#define CALCULUS_PRESET_COUNT 30
#endif

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个微积分预设
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义
 * @param complexity 时间复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_calculus_preset(
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

bool preset_calculus_register(void)
{
    int success_count = 0;

    ; /* 注册完成 */

    /* ============================================================
     * 第一部分：极限运算
     * ============================================================ */

    /* -------------------- 数列极限 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_calculus_preset(
                PRESET_LIMIT_SEQUENCE,
                "计算数列的极限 lim(n->inf) a_n",
                inputs, 1, PRESET_TYPE_LIMIT,
                "\\lim_{n \\to \\infty} a_n = L \\Leftrightarrow "
                "\\forall \\epsilon > 0, \\exists N: n > N \\Rightarrow |a_n - L| < \\epsilon",
                "O(inf)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 函数极限 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_calculus_preset(
                PRESET_LIMIT_FUNCTION,
                "计算函数在某点的极限 lim(x->a) f(x)",
                inputs, 2, PRESET_TYPE_LIMIT,
                "\\lim_{x \\to a} f(x) = L \\Leftrightarrow "
                "\\forall \\epsilon > 0, \\exists \\delta: 0 < |x-a| < \\delta "
                "\\Rightarrow |f(x)-L| < \\epsilon",
                "O(inf)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 左极限 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_calculus_preset(
                PRESET_LIMIT_LEFT,
                "计算函数在某点的左极限 lim(x->a^-) f(x)",
                inputs, 2, PRESET_TYPE_LIMIT,
                "\\lim_{x \\to a^-} f(x) = L",
                "O(inf)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 右极限 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_calculus_preset(
                PRESET_LIMIT_RIGHT,
                "计算函数在某点的右极限 lim(x->a^+) f(x)",
                inputs, 2, PRESET_TYPE_LIMIT,
                "\\lim_{x \\to a^+} f(x) = L",
                "O(inf)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 无穷极限 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_LIMIT_INFINITY,
                "计算函数在无穷远处的极限 lim(x->inf) f(x)",
                inputs, 1, PRESET_TYPE_LIMIT,
                "\\lim_{x \\to \\infty} f(x) = L",
                "O(inf)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 不定式极限（L'Hopital法则） -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_calculus_preset(
                PRESET_LIMIT_INDETERMINATE,
                "使用 L'Hopital 法则求解 0/0 或 inf/inf 型不定式极限",
                inputs, 3, PRESET_TYPE_LIMIT,
                "\\lim_{x \\to a} \\frac{f(x)}{g(x)} = "
                "\\lim_{x \\to a} \\frac{f'(x)}{g'(x)} "
                "\\quad (\\text{当 } f(a)=g(a)=0 \\text{ 或 } |f|=|g|=\\infty)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：微分运算
     * ============================================================ */

    /* -------------------- 导数定义 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_calculus_preset(
                PRESET_DERIVATIVE_DEFINITION,
                "由导数定义计算 f'(x) = lim_{h->0} [f(x+h)-f(x)]/h",
                inputs, 2, PRESET_TYPE_DERIVATIVE,
                "f'(x) = \\lim_{h \\to 0} \\frac{f(x+h) - f(x)}{h}",
                "O(inf)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 幂函数导数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_DERIVATIVE_POWER,
                "计算幂函数导数 d/dx[x^n] = nx^{n-1}",
                inputs, 1, PRESET_TYPE_DERIVATIVE,
                "\\frac{d}{dx} x^n = n x^{n-1}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 链式法则 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_DERIVATIVE_CHAIN,
                "应用链式法则 d/dx[f(g(x))] = f'(g(x))g'(x)",
                inputs, 2, PRESET_TYPE_DERIVATIVE,
                "\\frac{d}{dx}[f(g(x))] = f'(g(x)) \\cdot g'(x)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 乘积法则 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_DERIVATIVE_PRODUCT,
                "应用乘积法则 (fg)' = f'g + fg'",
                inputs, 2, PRESET_TYPE_DERIVATIVE,
                "(fg)' = f' \\cdot g + f \\cdot g'",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 商法则 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_DERIVATIVE_QUOTIENT,
                "应用商法则 (f/g)' = (f'g - fg')/g^2",
                inputs, 2, PRESET_TYPE_DERIVATIVE,
                "\\left(\\frac{f}{g}\\right)' = \\frac{f'g - fg'}{g^2}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 隐函数求导 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_EQUATION, PRESET_TYPE_SCALAR};
        if (register_calculus_preset(
                PRESET_DERIVATIVE_IMPLICIT,
                "对隐函数 F(x,y)=0 进行隐函数求导 dy/dx",
                inputs, 2, PRESET_TYPE_DERIVATIVE,
                "F(x, y) = 0 \\Rightarrow "
                "\\frac{dy}{dx} = -\\frac{\\partial F / \\partial x}{\\partial F / \\partial y}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 参数方程求导 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_calculus_preset(
                PRESET_DERIVATIVE_PARAMETRIC,
                "对参数方程 x=f(t), y=g(t) 求导 dy/dx",
                inputs, 3, PRESET_TYPE_DERIVATIVE,
                "\\frac{dy}{dx} = \\frac{dy/dt}{dx/dt} = \\frac{g'(t)}{f'(t)}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 偏导数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_INTEGER, PRESET_TYPE_SCALAR};
        if (register_calculus_preset(
                PRESET_DERIVATIVE_PARTIAL,
                "计算多元函数的偏导数 df/dx_i",
                inputs, 3, PRESET_TYPE_DERIVATIVE,
                "\\frac{\\partial f}{\\partial x_i} = "
                "\\lim_{h \\to 0} \\frac{f(x+he_i) - f(x)}{h}",
                "O(inf)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：积分运算
     * ============================================================ */

    /* -------------------- 不定积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_INTEGRAL_INDEFINITE,
                "计算不定积分 int f(x)dx",
                inputs, 1, PRESET_TYPE_INTEGRAL,
                "\\int f(x) \\, dx = F(x) + C, \\quad F'(x) = f(x)",
                "O(inf)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 定积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_calculus_preset(
                PRESET_INTEGRAL_DEFINITE,
                "计算定积分 int_a^b f(x)dx",
                inputs, 3, PRESET_TYPE_SCALAR,
                "\\int_a^b f(x) \\, dx = F(b) - F(a)",
                "O(inf)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 换元积分法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_INTEGRAL_SUBSTITUTION,
                "使用换元积分法计算积分 int f(g(x))g'(x)dx",
                inputs, 2, PRESET_TYPE_INTEGRAL,
                "\\int f(g(x)) g'(x) \\, dx = \\int f(u) \\, du, \\quad u = g(x)",
                "O(n)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 分部积分法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_INTEGRAL_BY_PARTS,
                "使用分部积分法 int u dv = uv - int v du",
                inputs, 2, PRESET_TYPE_INTEGRAL,
                "\\int u \\, dv = uv - \\int v \\, du",
                "O(n)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 部分分式积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_INTEGRAL_PARTIAL_FRACTION,
                "使用部分分式分解计算有理函数积分",
                inputs, 1, PRESET_TYPE_INTEGRAL,
                "\\int \\frac{P(x)}{Q(x)} \\, dx = "
                "\\int \\sum \\frac{A_i}{(x-a_i)^{k_i}} \\, dx",
                "O(n^2)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 三角积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_INTEGRAL_TRIGONOMETRIC,
                "计算含三角函数的积分",
                inputs, 1, PRESET_TYPE_INTEGRAL,
                "\\int \\sin^n x \\cos^m x \\, dx, \\quad "
                "\\int \\tan^n x \\, dx, \\quad \\int \\sec^n x \\, dx",
                "O(n)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 反常积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_calculus_preset(
                PRESET_INTEGRAL_IMPROPER,
                "计算反常积分（无穷区间或无界函数）",
                inputs, 3, PRESET_TYPE_SCALAR,
                "\\int_a^{\\infty} f(x) \\, dx = \\lim_{b \\to \\infty} \\int_a^b f(x) \\, dx",
                "O(inf)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 曲线积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_PATH};
        if (register_calculus_preset(
                PRESET_INTEGRAL_LINE,
                "计算沿曲线的曲线积分 int_C F . dr",
                inputs, 2, PRESET_TYPE_SCALAR,
                "\\int_C \\mathbf{F} \\cdot d\\mathbf{r} = "
                "\\int_a^b \\mathbf{F}(\\mathbf{r}(t)) \\cdot \\mathbf{r}'(t) \\, dt",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：级数展开
     * ============================================================ */

    /* -------------------- Taylor级数展开 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_calculus_preset(
                PRESET_SERIES_TAYLOR,
                "计算函数在 x=a 处的 Taylor 级数展开",
                inputs, 3, PRESET_TYPE_SERIES,
                "f(x) = \\sum_{n=0}^{\\infty} \\frac{f^{(n)}(a)}{n!}(x-a)^n",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Maclaurin级数展开 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_INTEGER};
        if (register_calculus_preset(
                PRESET_SERIES_MACLAURIN,
                "计算函数的 Maclaurin 级数展开（a=0 处的 Taylor 级数）",
                inputs, 2, PRESET_TYPE_SERIES,
                "f(x) = \\sum_{n=0}^{\\infty} \\frac{f^{(n)}(0)}{n!} x^n",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Fourier级数展开 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_calculus_preset(
                PRESET_SERIES_FOURIER,
                "计算周期函数的 Fourier 级数展开",
                inputs, 3, PRESET_TYPE_SERIES,
                "f(x) = \\frac{a_0}{2} + \\sum_{n=1}^{\\infty} "
                "\\left( a_n \\cos\\frac{n\\pi x}{L} + b_n \\sin\\frac{n\\pi x}{L} \\right)",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 幂级数展开 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_calculus_preset(
                PRESET_SERIES_POWER,
                "计算函数的幂级数展开及收敛半径",
                inputs, 2, PRESET_TYPE_SERIES,
                "\\sum_{n=0}^{\\infty} c_n (x - a)^n, \\quad "
                "R = \\frac{1}{\\limsup_{n \\to \\infty} \\sqrt[n]{|c_n|}}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：多元微积分
     * ============================================================ */

    /* -------------------- 梯度 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_MULTIVARIABLE_GRADIENT,
                "计算标量场的梯度 nabla f",
                inputs, 1, PRESET_TYPE_VECTOR,
                "\\nabla f = \\left(\\frac{\\partial f}{\\partial x_1}, "
                "\\ldots, \\frac{\\partial f}{\\partial x_n}\\right)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 散度 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_MULTIVARIABLE_DIVERGENCE,
                "计算向量场的散度 nabla . F",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\nabla \\cdot \\mathbf{F} = "
                "\\sum_{i=1}^{n} \\frac{\\partial F_i}{\\partial x_i}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 旋度 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_MULTIVARIABLE_CURL,
                "计算三维向量场的旋度 nabla x F",
                inputs, 1, PRESET_TYPE_VECTOR,
                "\\nabla \\times \\mathbf{F} = "
                "\\begin{vmatrix} \\mathbf{i} & \\mathbf{j} & \\mathbf{k} \\\\ "
                "\\frac{\\partial}{\\partial x} & \\frac{\\partial}{\\partial y} & "
                "\\frac{\\partial}{\\partial z} \\\\ "
                "F_x & F_y & F_z \\end{vmatrix}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Laplace算子 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_calculus_preset(
                PRESET_MULTIVARIABLE_LAPLACIAN,
                "计算标量场的 Laplace 算子 nabla^2 f",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\nabla^2 f = \\Delta f = "
                "\\sum_{i=1}^{n} \\frac{\\partial^2 f}{\\partial x_i^2}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    ; /* 注册完成 */
    return success_count == CALCULUS_PRESET_COUNT;
}

/**
 * @brief 获取微积分预设函数块数量
 *
 * @return int 微积分模块预设函数块总数
 */
int preset_calculus_count(void)
{
    return CALCULUS_PRESET_COUNT;
}

/**
 * @brief 获取微积分预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_calculus_get_names(char ***out_names, int *out_count)
{
    PRESET_CHECK_NULL(out_names, error);
    PRESET_CHECK_NULL(out_count, error);

    /* 分配名称数组 */
    char **names = (char **)lv00_malloc(CALCULUS_PRESET_COUNT * sizeof(char *));
    PRESET_CHECK_NULL(names, error);

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 极限运算 */
        PRESET_LIMIT_SEQUENCE,
        PRESET_LIMIT_FUNCTION,
        PRESET_LIMIT_LEFT,
        PRESET_LIMIT_RIGHT,
        PRESET_LIMIT_INFINITY,
        PRESET_LIMIT_INDETERMINATE,
        /* 微分运算 */
        PRESET_DERIVATIVE_DEFINITION,
        PRESET_DERIVATIVE_POWER,
        PRESET_DERIVATIVE_CHAIN,
        PRESET_DERIVATIVE_PRODUCT,
        PRESET_DERIVATIVE_QUOTIENT,
        PRESET_DERIVATIVE_IMPLICIT,
        PRESET_DERIVATIVE_PARAMETRIC,
        PRESET_DERIVATIVE_PARTIAL,
        /* 积分运算 */
        PRESET_INTEGRAL_INDEFINITE,
        PRESET_INTEGRAL_DEFINITE,
        PRESET_INTEGRAL_SUBSTITUTION,
        PRESET_INTEGRAL_BY_PARTS,
        PRESET_INTEGRAL_PARTIAL_FRACTION,
        PRESET_INTEGRAL_TRIGONOMETRIC,
        PRESET_INTEGRAL_IMPROPER,
        PRESET_INTEGRAL_LINE,
        /* 级数展开 */
        PRESET_SERIES_TAYLOR,
        PRESET_SERIES_MACLAURIN,
        PRESET_SERIES_FOURIER,
        PRESET_SERIES_POWER,
        /* 多元微积分 */
        PRESET_MULTIVARIABLE_GRADIENT,
        PRESET_MULTIVARIABLE_DIVERGENCE,
        PRESET_MULTIVARIABLE_CURL,
        PRESET_MULTIVARIABLE_LAPLACIAN
    };

    /* 复制每个名称字符串 */
    for (int i = 0; i < CALCULUS_PRESET_COUNT; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 内存分配失败，释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                { void *tmp = names[j]; lv00_free(&tmp); }
            }
            { void *tmp = names; lv00_free(&tmp); }
            /* PRESET_ERROR_LOG("复制微积分预设名称失败: 索引 %d", i); */
            return false;
        }
    }

    *out_names = names;
    *out_count = CALCULUS_PRESET_COUNT;
    return true;

error:
    /* PRESET_ERROR_LOG("获取微积分预设名称列表失败: 参数为空"); */
    return false;
}

/**
 * @brief 获取微积分预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_calculus_category(void)
{
    return PRESET_CATEGORY_ANALYSIS;
}
