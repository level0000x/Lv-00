/**
 * @file preset_trigonometry.c
 * @brief 三角函数预设函数块 - 实现
 *
 * @details 实现三角函数模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共20个预设，涵盖基本三角函数、反三角函数、双曲函数、
 *          三角恒等式、三角方程求解和三角级数展开。
 *
 * @module Trigonometry
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 1.0.0
 */

#include "preset_trigonometry.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 三角函数模块预设函数块总数 */

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 注册单个三角函数预设
 *
 * 辅助函数，用于简化预设注册过程。
 * 所有三角函数预设都属于 PRESET_CATEGORY_ALGEBRAIC 类别。
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
static bool register_trig_preset(
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
        PRESET_CATEGORY_ALGEBRAIC,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ============================================================
 * 模块注册实现
 * ============================================================ */

bool preset_trigonometry_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 基本三角函数 (6个)
     * ============================================================ */

    /* 正弦函数 sin(x) */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_trig_preset(
                PRESET_TRIG_SIN,
                "正弦函数：计算 sin(x)，输入 x 为弧度制角度",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\sin x = x - \\frac{x^3}{3!} + \\frac{x^5}{5!} - \\cdots",
                "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 余弦函数 cos(x) */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_trig_preset(
                PRESET_TRIG_COS,
                "余弦函数：计算 cos(x)，输入 x 为弧度制角度",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\cos x = 1 - \\frac{x^2}{2!} + \\frac{x^4}{4!} - \\cdots",
                "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 正切函数 tan(x) */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_trig_preset(
                PRESET_TRIG_TAN,
                "正切函数：计算 tan(x) = sin(x) / cos(x)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\tan x = \\frac{\\sin x}{\\cos x}, \\quad x \\neq \\frac{\\pi}{2} + k\\pi",
                "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 余切函数 cot(x) */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_trig_preset(
                PRESET_TRIG_COT,
                "余切函数：计算 cot(x) = cos(x) / sin(x)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\cot x = \\frac{\\cos x}{\\sin x}, \\quad x \\neq k\\pi",
                "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 正割函数 sec(x) */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_trig_preset(
                PRESET_TRIG_SEC,
                "正割函数：计算 sec(x) = 1 / cos(x)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\sec x = \\frac{1}{\\cos x}, \\quad |\\sec x| \\geq 1",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 余割函数 csc(x) */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_trig_preset(
                PRESET_TRIG_CSC,
                "余割函数：计算 csc(x) = 1 / sin(x)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\csc x = \\frac{1}{\\sin x}, \\quad |\\csc x| \\geq 1",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 反三角函数 (4个)
     * ============================================================ */

    /* 反正弦函数 arcsin(x) */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_trig_preset(
                PRESET_TRIG_ARCSIN,
                "反正弦函数：计算 arcsin(x)，x ∈ [-1, 1]，值域 [-π/2, π/2]",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\arcsin x = \\int_0^x \\frac{dt}{\\sqrt{1-t^2}}, \\quad x \\in [-1, 1]",
                "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 反余弦函数 arccos(x) */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_trig_preset(
                PRESET_TRIG_ARCCOS,
                "反余弦函数：计算 arccos(x)，x ∈ [-1, 1]，值域 [0, π]",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\arccos x = \\frac{\\pi}{2} - \\arcsin x, \\quad x \\in [-1, 1]",
                "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 反正切函数 arctan(x) */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_trig_preset(
                PRESET_TRIG_ARCTAN,
                "反正切函数：计算 arctan(x)，x ∈ R，值域 (-π/2, π/2)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\arctan x = \\int_0^x \\frac{dt}{1+t^2}, \\quad x \\in \\mathbb{R}",
                "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 反余切函数 arccot(x) */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_trig_preset(
                PRESET_TRIG_ARCCOT,
                "反余切函数：计算 arccot(x)，x ∈ R，值域 (0, π)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\operatorname{arccot} x = \\frac{\\pi}{2} - \\arctan x, \\quad x \\in \\mathbb{R}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 双曲函数 (3个)
     * ============================================================ */

    /* 双曲正弦 sinh(x) */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_trig_preset(
                PRESET_TRIG_SINH,
                "双曲正弦：计算 sinh(x) = (e^x - e^{-x}) / 2，奇函数",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\sinh x = \\frac{e^x - e^{-x}}{2}",
                "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 双曲余弦 cosh(x) */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_trig_preset(
                PRESET_TRIG_COSH,
                "双曲余弦：计算 cosh(x) = (e^x + e^{-x}) / 2，偶函数",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\cosh x = \\frac{e^x + e^{-x}}{2}, \\quad \\cosh x \\geq 1",
                "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 双曲正切 tanh(x) */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_trig_preset(
                PRESET_TRIG_TANH,
                "双曲正切：计算 tanh(x) = sinh(x) / cosh(x)，值域 (-1, 1)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\tanh x = \\frac{\\sinh x}{\\cosh x} = \\frac{e^x - e^{-x}}{e^x + e^{-x}}",
                "O(1)", true, true)) {
            success_count++;
        }
    }

    /* ============================================================
     * 三角恒等式 (4个)
     * ============================================================ */

    /* 和差化积 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_trig_preset(
                PRESET_TRIG_SUM_TO_PRODUCT,
                "和差化积：将三角函数的和差转化为积，包含4种公式",
                inputs, 3, PRESET_TYPE_SCALAR,
                "\\sin A \\pm \\sin B = 2\\sin\\frac{A\\pm B}{2}\\cos\\frac{A\\mp B}{2},"
                "\\quad \\cos A + \\cos B = 2\\cos\\frac{A+B}{2}\\cos\\frac{A-B}{2},"
                "\\quad \\cos A - \\cos B = -2\\sin\\frac{A+B}{2}\\sin\\frac{A-B}{2}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 积化和差 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_trig_preset(
                PRESET_TRIG_PRODUCT_TO_SUM,
                "积化和差：将三角函数的积转化为和差，包含4种公式",
                inputs, 3, PRESET_TYPE_SCALAR,
                "\\sin A \\cos B = \\frac{1}{2}[\\sin(A+B) + \\sin(A-B)],"
                "\\quad \\cos A \\cos B = \\frac{1}{2}[\\cos(A+B) + \\cos(A-B)],"
                "\\quad \\sin A \\sin B = -\\frac{1}{2}[\\cos(A+B) - \\cos(A-B)]",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 倍角公式 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_trig_preset(
                PRESET_TRIG_DOUBLE_ANGLE,
                "倍角公式：计算 sin(2θ), cos(2θ), tan(2θ)",
                inputs, 2, PRESET_TYPE_SCALAR,
                "\\sin 2\\theta = 2\\sin\\theta\\cos\\theta,"
                "\\quad \\cos 2\\theta = \\cos^2\\theta - \\sin^2\\theta,"
                "\\quad \\tan 2\\theta = \\frac{2\\tan\\theta}{1-\\tan^2\\theta}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 半角公式 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_trig_preset(
                PRESET_TRIG_HALF_ANGLE,
                "半角公式：计算 sin(θ/2), cos(θ/2), tan(θ/2)",
                inputs, 2, PRESET_TYPE_SCALAR,
                "\\sin\\frac{\\theta}{2} = \\pm\\sqrt{\\frac{1-\\cos\\theta}{2}},"
                "\\quad \\cos\\frac{\\theta}{2} = \\pm\\sqrt{\\frac{1+\\cos\\theta}{2}},"
                "\\quad \\tan\\frac{\\theta}{2} = \\frac{\\sin\\theta}{1+\\cos\\theta}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 三角方程求解 (1个)
     * ============================================================ */

    /* 标准三角方程求解 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_trig_preset(
                PRESET_TRIG_EQUATION_SOLVE,
                "三角方程求解：求解 sin x = a, cos x = a, tan x = a 的通解",
                inputs, 2, PRESET_TYPE_LIST,
                "\\sin x = a \\Rightarrow x = (-1)^k\\arcsin a + k\\pi,"
                "\\quad \\cos x = a \\Rightarrow x = \\pm\\arccos a + 2k\\pi,"
                "\\quad \\tan x = a \\Rightarrow x = \\arctan a + k\\pi",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 三角级数展开 (2个)
     * ============================================================ */

    /* Taylor级数展开 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_trig_preset(
                PRESET_TRIG_SERIES_EXPAND,
                "三角函数的Taylor级数展开：用有限项Taylor多项式近似 sin x 或 cos x",
                inputs, 3, PRESET_TYPE_SCALAR,
                "\\sin x = \\sum_{n=0}^{N} (-1)^n \\frac{x^{2n+1}}{(2n+1)!},"
                "\\quad \\cos x = \\sum_{n=0}^{N} (-1)^n \\frac{x^{2n}}{(2n)!}",
                "O(N)", true, false)) {
            success_count++;
        }
    }

    /* 傅里叶级数展开 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_trig_preset(
                PRESET_TRIG_FOURIER_SERIES,
                "傅里叶三角级数展开：计算周期函数的傅里叶系数 a_n, b_n",
                inputs, 3, PRESET_TYPE_LIST,
                "f(t) = \\frac{a_0}{2} + \\sum_{n=1}^{N} "
                "\\left(a_n\\cos\\frac{2\\pi nt}{T} + b_n\\sin\\frac{2\\pi nt}{T}\\right)",
                "O(N)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == TRIGONOMETRY_PRESET_COUNT;
}

/* ============================================================
 * 模块信息接口
 * ============================================================ */

int preset_trigonometry_count(void)
{
    return TRIGONOMETRY_PRESET_COUNT;
}

PresetCategory preset_trigonometry_category(void)
{
    return PRESET_CATEGORY_ALGEBRAIC;
}

bool preset_trigonometry_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    /* 分配名称数组 */
    char **names = (char**)lv00_malloc(TRIGONOMETRY_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 基本三角函数 */
        PRESET_TRIG_SIN,
        PRESET_TRIG_COS,
        PRESET_TRIG_TAN,
        PRESET_TRIG_COT,
        PRESET_TRIG_SEC,
        PRESET_TRIG_CSC,
        /* 反三角函数 */
        PRESET_TRIG_ARCSIN,
        PRESET_TRIG_ARCCOS,
        PRESET_TRIG_ARCTAN,
        PRESET_TRIG_ARCCOT,
        /* 双曲函数 */
        PRESET_TRIG_SINH,
        PRESET_TRIG_COSH,
        PRESET_TRIG_TANH,
        /* 三角恒等式 */
        PRESET_TRIG_SUM_TO_PRODUCT,
        PRESET_TRIG_PRODUCT_TO_SUM,
        PRESET_TRIG_DOUBLE_ANGLE,
        PRESET_TRIG_HALF_ANGLE,
        /* 三角方程求解 */
        PRESET_TRIG_EQUATION_SOLVE,
        /* 三角级数展开 */
        PRESET_TRIG_SERIES_EXPAND,
        PRESET_TRIG_FOURIER_SERIES,
    };

    int count = (int)(sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
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