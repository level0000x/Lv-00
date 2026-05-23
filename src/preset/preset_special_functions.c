/**
 * @file preset_special_functions.c
 * @brief 特殊函数预设函数块模块 - 实现
 *
 * 实现理论数学研究中常用的特殊函数预设函数块。
 * 涵盖 Gamma/Beta 函数族、误差与指数积分、Bessel 函数族、
 * 正交多项式以及 Riemann Zeta 函数，共 20 个预设。
 *
 * @module SpecialFunctions
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_special_functions.h -> preset_blocks.h -> func_block_registry.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_simple() 声明
 *   -> 提供 PresetCategory 枚举（PRESET_CATEGORY_ANALYSIS 等）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 *   -> 提供 PRESET_SAFE_MALLOC 等安全内存操作宏
 * lv00_internal.h / lv00_utils.h
 *   -> 提供 lv00_malloc、lv00_free、lv00_strdup、lv00_log_* 等
 * ============================================================
 */
#include "preset_special_functions.h"
#include "preset_blocks.h"
#include "preset_common.h"     /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等） */
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 特殊函数模块预设函数块总数 */
#define SPECIAL_FUNCTIONS_PRESET_COUNT 20

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个特殊函数预设
 *
 * 统一调用 preset_blocks_register_simple，类别固定为
 * PRESET_CATEGORY_ANALYSIS。支持自定义输出类型以处理
 * 元组输出（如 sf_sin_cos_integral）。
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型（通常为 PRESET_TYPE_SCALAR）
 * @param math_def 数学定义（LaTeX 格式）
 * @param complexity 时间复杂度描述
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_sf_preset(
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

bool preset_special_functions_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一组：Gamma & Beta 函数（5个预设）
     *
     * 涵盖 Gamma 函数族及其相关函数：
     *  - Gamma 函数 Γ(z)
     *  - 对数 Gamma 函数 lnΓ(z)
     *  - Digamma 函数 ψ(z)（即 Gamma 函数的对数导数）
     *  - Beta 函数 B(a,b)
     *  - 不完全 Beta 函数 B(x;a,b)
     *
     * 这些函数在数论、统计物理和特殊函数论中广泛使用。
     * ============================================================ */

    /* -------------------- sf_gamma：Gamma函数 Γ(z) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_GAMMA,
                "Gamma函数 Γ(z) — 阶乘在复平面上的解析延拓",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\Gamma(z) = \\int_{0}^{\\infty} t^{z-1} e^{-t} \\, dt, "
                "\\quad \\text{Re}(z) > 0; \\quad "
                "\\Gamma(n+1) = n! \\; (n \\in \\mathbb{N})",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_log_gamma：对数Gamma函数 ln(Γ(z)) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_LOG_GAMMA,
                "对数Gamma函数 ln(Γ(z)) — Gamma函数的自然对数",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\ln\\Gamma(z) = -\\gamma z - \\ln z + "
                "\\sum_{k=1}^{\\infty} \\left[ \\frac{z}{k} - "
                "\\ln\\left(1 + \\frac{z}{k}\\right) \\right], "
                "\\quad z \\neq 0, -1, -2, \\ldots",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_digamma：Digamma函数 ψ(z) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_DIGAMMA,
                "Digamma函数 ψ(z) — Gamma函数的对数导数",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\psi(z) = \\frac{d}{dz}\\ln\\Gamma(z) = "
                "\\frac{\\Gamma'(z)}{\\Gamma(z)} = "
                "-\\gamma + \\sum_{k=0}^{\\infty} "
                "\\left(\\frac{1}{k+1} - \\frac{1}{k+z}\\right), "
                "\\quad z \\neq 0, -1, -2, \\ldots",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_beta：Beta函数 B(a,b) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_BETA,
                "Beta函数 B(a,b) — 二元Euler积分",
                inputs, 2, PRESET_TYPE_SCALAR,
                "B(a,b) = \\int_{0}^{1} t^{a-1} (1-t)^{b-1} \\, dt "
                "= \\frac{\\Gamma(a)\\Gamma(b)}{\\Gamma(a+b)}, "
                "\\quad \\text{Re}(a) > 0, \\; \\text{Re}(b) > 0",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_incomplete_beta：不完全Beta函数 B(x;a,b) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_INCOMPLETE_BETA,
                "不完全Beta函数 B(x;a,b) — 截断的Beta积分",
                inputs, 3, PRESET_TYPE_SCALAR,
                "B(x;a,b) = \\int_{0}^{x} t^{a-1} (1-t)^{b-1} \\, dt, "
                "\\quad 0 \\le x \\le 1, \\; "
                "\\text{Re}(a) > 0, \\; \\text{Re}(b) > 0",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二组：误差与指数积分（5个预设）
     *
     * 涵盖概率论与数理统计中常用的误差函数族，
     * 以及复分析和渐近分析中重要的指数/对数积分函数：
     *  - 误差函数 erf(x) 与补误差函数 erfc(x)
     *  - 指数积分 Ei(x) 与对数积分 li(x)
     *  - 正弦/余弦积分 Si(x)/Ci(x)（元组输出）
     * ============================================================ */

    /* -------------------- sf_erf：误差函数 erf(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_ERF,
                "误差函数 erf(x) — 正态分布的累积分函数基础",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\operatorname{erf}(x) = "
                "\\frac{2}{\\sqrt{\\pi}} \\int_{0}^{x} e^{-t^{2}} \\, dt, "
                "\\quad \\operatorname{erf}(-x) = -\\operatorname{erf}(x), "
                "\\; \\operatorname{erf}(\\infty) = 1",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_erfc：补误差函数 erfc(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_ERFC,
                "补误差函数 erfc(x) — 误差函数的互补部分",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\operatorname{erfc}(x) = 1 - \\operatorname{erf}(x) "
                "= \\frac{2}{\\sqrt{\\pi}} \\int_{x}^{\\infty} "
                "e^{-t^{2}} \\, dt, \\quad "
                "\\operatorname{erfc}(x) \\sim "
                "\\frac{e^{-x^{2}}}{x\\sqrt{\\pi}} \\; (x \\to \\infty)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_exp_integral：指数积分 Ei(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_EXP_INTEGRAL,
                "指数积分 Ei(x) — 指数型积分的特殊函数",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\operatorname{Ei}(x) = "
                "-\\!\\!\\!\\!\\int_{-x}^{\\infty} "
                "\\frac{e^{-t}}{t} \\, dt = "
                "\\gamma + \\ln|x| + "
                "\\sum_{k=1}^{\\infty} \\frac{x^{k}}{k \\cdot k!}, "
                "\\quad x \\neq 0",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_log_integral：对数积分 li(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_LOG_INTEGRAL,
                "对数积分 li(x) — 素数定理中的核心函数",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\operatorname{li}(x) = "
                "\\int_{0}^{x} \\frac{dt}{\\ln t} = "
                "\\operatorname{Ei}(\\ln x), \\quad x > 0, \\; x \\neq 1; "
                "\\quad \\pi(x) \\sim \\operatorname{li}(x) \\; "
                "(\\text{素数定理})",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_sin_cos_integral：正弦/余弦积分 Si(x)/Ci(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_SIN_COS_INTEGRAL,
                "正弦/余弦积分 Si(x)/Ci(x) — 输出元组 (Si(x), Ci(x))",
                inputs, 1, PRESET_TYPE_TUPLE,
                "\\operatorname{Si}(x) = \\int_{0}^{x} "
                "\\frac{\\sin t}{t} \\, dt, \\quad "
                "\\operatorname{Ci}(x) = -\\int_{x}^{\\infty} "
                "\\frac{\\cos t}{t} \\, dt = "
                "\\gamma + \\ln x + "
                "\\int_{0}^{x} \\frac{\\cos t - 1}{t} \\, dt",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三组：Bessel 与相关函数（5个预设）
     *
     * 涵盖柱坐标系中分离变量法得到的 Bessel 微分方程
     * 的两类标准解，以及修正 Bessel 函数和球 Bessel 函数：
     *  - 第一类 Bessel 函数 J_ν(x)（正则解）
     *  - 第二类 Bessel 函数 Y_ν(x)（Neumann 函数）
     *  - 修正 Bessel 函数 I_ν(x) 与 K_ν(x)
     *  - 球 Bessel 函数 j_n(x)（3D 自由粒子波动方程径向解）
     * ============================================================ */

    /* -------------------- sf_bessel_j：第一类Bessel函数 J_ν(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_BESSEL_J,
                "第一类Bessel函数 J_ν(x) — Bessel微分方程的正则解",
                inputs, 2, PRESET_TYPE_SCALAR,
                "J_{\\nu}(x) = "
                "\\sum_{k=0}^{\\infty} "
                "\\frac{(-1)^{k}}{k! \\, \\Gamma(k+\\nu+1)} "
                "\\left(\\frac{x}{2}\\right)^{2k+\\nu}, "
                "\\quad \\nu \\ge 0",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_bessel_y：第二类Bessel函数 Y_ν(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_BESSEL_Y,
                "第二类Bessel函数 Y_ν(x) — Neumann函数",
                inputs, 2, PRESET_TYPE_SCALAR,
                "Y_{\\nu}(x) = "
                "\\frac{J_{\\nu}(x)\\cos(\\nu\\pi) - "
                "J_{-\\nu}(x)}{\\sin(\\nu\\pi)}, "
                "\\quad \\nu \\notin \\mathbb{Z}; \\quad "
                "x > 0",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_modified_bessel_i：修正Bessel函数 I_ν(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_MODIFIED_BESSEL_I,
                "修正Bessel函数 I_ν(x) — 修正Bessel方程的第一类解",
                inputs, 2, PRESET_TYPE_SCALAR,
                "I_{\\nu}(x) = i^{-\\nu} J_{\\nu}(ix) = "
                "\\sum_{k=0}^{\\infty} "
                "\\frac{1}{k! \\, \\Gamma(k+\\nu+1)} "
                "\\left(\\frac{x}{2}\\right)^{2k+\\nu}, "
                "\\quad \\nu \\ge 0",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_modified_bessel_k：修正Bessel函数 K_ν(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_MODIFIED_BESSEL_K,
                "修正Bessel函数 K_ν(x) — 修正Bessel方程的第二类解",
                inputs, 2, PRESET_TYPE_SCALAR,
                "K_{\\nu}(x) = "
                "\\frac{\\pi}{2} \\frac{I_{-\\nu}(x) - "
                "I_{\\nu}(x)}{\\sin(\\nu\\pi)}, \\quad "
                "x > 0, \\; K_{\\nu}(x) "
                "\\sim \\sqrt{\\frac{\\pi}{2x}} "
                "e^{-x} \\; (x \\to \\infty)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_spherical_bessel：球Bessel函数 j_n(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_SPHERICAL_BESSEL,
                "球Bessel函数 j_n(x) — 三维Helmholtz方程径向解",
                inputs, 2, PRESET_TYPE_SCALAR,
                "j_{n}(x) = "
                "\\sqrt{\\frac{\\pi}{2x}} \\, "
                "J_{n+\\frac{1}{2}}(x) = "
                "(-x)^{n} \\left(\\frac{1}{x}\\frac{d}{dx}\\right)^{n} "
                "\\frac{\\sin x}{x}, \\quad n \\in \\mathbb{N}_{0}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四组：正交多项式与其他（5个预设）
     *
     * 涵盖经典正交多项式家族（Legendre、Hermite、Laguerre、
     * Chebyshev）以及 Riemann Zeta 函数：
     *  - Legendre 多项式 P_n(x)：[-1,1] 上关于权 1 正交
     *  - Hermite 多项式 H_n(x)：(-∞,∞) 上关于权 e^(-x^2) 正交
     *  - Laguerre 多项式 L_n(x)：[0,∞) 上关于权 e^(-x) 正交
     *  - Chebyshev 多项式 T_n(x)：[-1,1] 上关于权 1/√(1-x^2) 正交
     *  - Riemann Zeta 函数 ζ(s)：解析数论核心对象
     * ============================================================ */

    /* -------------------- sf_legendre_p：Legendre多项式 P_n(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_LEGENDRE_P,
                "Legendre多项式 P_n(x) — 球谐函数的角向分量",
                inputs, 2, PRESET_TYPE_SCALAR,
                "P_{n}(x) = "
                "\\frac{1}{2^{n} n!} "
                "\\frac{d^{n}}{dx^{n}}(x^{2}-1)^{n} "
                "\\quad (\\text{Rodrigues公式}), \\; "
                "\\int_{-1}^{1} P_{m}(x)P_{n}(x) \\, dx "
                "= \\frac{2}{2n+1}\\delta_{mn}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_hermite_h：Hermite多项式 H_n(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_HERMITE_H,
                "Hermite多项式 H_n(x) — 量子谐振子波函数的基础",
                inputs, 2, PRESET_TYPE_SCALAR,
                "H_{n}(x) = "
                "(-1)^{n} e^{x^{2}} "
                "\\frac{d^{n}}{dx^{n}} e^{-x^{2}} "
                "\\quad (\\text{Rodrigues公式}), \\; "
                "\\int_{-\\infty}^{\\infty} "
                "H_{m}(x)H_{n}(x) e^{-x^{2}} \\, dx "
                "= \\sqrt{\\pi} \\, 2^{n} n! \\, \\delta_{mn}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_laguerre_l：Laguerre多项式 L_n(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_LAGUERRE_L,
                "Laguerre多项式 L_n(x) — 氢原子径向波函数的基础",
                inputs, 2, PRESET_TYPE_SCALAR,
                "L_{n}(x) = "
                "\\frac{e^{x}}{n!} "
                "\\frac{d^{n}}{dx^{n}}(x^{n} e^{-x}) "
                "\\quad (\\text{Rodrigues公式}), \\; "
                "\\int_{0}^{\\infty} "
                "L_{m}(x)L_{n}(x) e^{-x} \\, dx "
                "= \\delta_{mn}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_chebyshev_t：Chebyshev多项式 T_n(x) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_CHEBYSHEV_T,
                "Chebyshev多项式 T_n(x) — 最优一致逼近的基础",
                inputs, 2, PRESET_TYPE_SCALAR,
                "T_{n}(x) = \\cos(n\\arccos x), \\; "
                "|x| \\le 1; \\quad "
                "T_{n}(x) = "
                "\\frac{n}{2} \\sum_{k=0}^{\\lfloor n/2 \\rfloor} "
                "(-1)^{k} \\frac{(n-k-1)!}{k! \\, (n-2k)!} "
                "(2x)^{n-2k}, \\; "
                "\\int_{-1}^{1} "
                "\\frac{T_{m}(x)T_{n}(x)}{\\sqrt{1-x^{2}}} \\, dx "
                "= \\frac{\\pi}{2}\\delta_{mn} \\; (n>0)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- sf_zeta：Riemann Zeta函数 ζ(s) -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_sf_preset(
                PRESET_SF_ZETA,
                "Riemann Zeta函数 ζ(s) — 解析数论的核心研究对象",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\zeta(s) = "
                "\\sum_{n=1}^{\\infty} \\frac{1}{n^{s}} = "
                "\\prod_{p \\; \\text{prime}} "
                "\\frac{1}{1 - p^{-s}}, \\quad "
                "\\text{Re}(s) > 1; \\; "
                "\\zeta(s) = 2^{s} \\pi^{s-1} "
                "\\sin\\left(\\frac{\\pi s}{2}\\right) "
                "\\Gamma(1-s) \\, \\zeta(1-s) "
                "\\quad (\\text{函数方程})",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == SPECIAL_FUNCTIONS_PRESET_COUNT;
}

/**
 * @brief 获取特殊函数预设函数块数量
 *
 * @return int 特殊函数模块预设函数块总数（20）
 */
int preset_special_functions_count(void)
{
    return SPECIAL_FUNCTIONS_PRESET_COUNT;
}

/**
 * @brief 获取特殊函数模块的预设类别
 *
 * 所有特殊函数预设均属于分析学类别。
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_ANALYSIS
 */
PresetCategory preset_special_functions_category(void)
{
    return PRESET_CATEGORY_ANALYSIS;
}

/**
 * @brief 获取特殊函数模块的所有预设名称列表
 *
 * 分配并返回模块内 4 组共 20 个预设的名称数组，按注册顺序排列。
 * 调用者负责通过 lv00_free 释放每个元素和数组本身。
 *
 * @param out_names 输出名称数组（调用者负责释放每个元素和数组本身）
 * @param out_count 输出名称数量
 * @return true 内存分配成功
 * @return false 内存分配失败
 */
bool preset_special_functions_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    /* 分配名称数组 */
    char **names = (char**)lv00_malloc(SPECIAL_FUNCTIONS_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

    /* 填充预设名称列表，按 4 组顺序排列 */
    const char *preset_names[] = {
        /* 第一组：Gamma & Beta 函数 */
        PRESET_SF_GAMMA,
        PRESET_SF_LOG_GAMMA,
        PRESET_SF_DIGAMMA,
        PRESET_SF_BETA,
        PRESET_SF_INCOMPLETE_BETA,
        /* 第二组：误差与指数积分 */
        PRESET_SF_ERF,
        PRESET_SF_ERFC,
        PRESET_SF_EXP_INTEGRAL,
        PRESET_SF_LOG_INTEGRAL,
        PRESET_SF_SIN_COS_INTEGRAL,
        /* 第三组：Bessel 与相关函数 */
        PRESET_SF_BESSEL_J,
        PRESET_SF_BESSEL_Y,
        PRESET_SF_MODIFIED_BESSEL_I,
        PRESET_SF_MODIFIED_BESSEL_K,
        PRESET_SF_SPHERICAL_BESSEL,
        /* 第四组：正交多项式与其他 */
        PRESET_SF_LEGENDRE_P,
        PRESET_SF_HERMITE_H,
        PRESET_SF_LAGUERRE_L,
        PRESET_SF_CHEBYSHEV_T,
        PRESET_SF_ZETA,
    };

    int count = (int)(sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) { void *tmp = names[j]; lv00_free(&tmp); }
            { void *tmp = names; lv00_free(&tmp); }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
