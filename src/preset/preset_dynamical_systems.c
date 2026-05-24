/**
 * @file preset_dynamical_systems.c
 * @brief 动力系统预设函数块 - 实现
 *
 * @details 实现动力系统模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共25个预设，涵盖稳定性分析、分岔分析、极限环与周期解、
 *          混沌与吸引子、不变流形和渐近方法。
 *
 * @module DynamicalSystems
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 */

#include "preset_dynamical_systems.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 动力系统模块预设函数块总数 */
#define DYNAMICAL_SYSTEMS_PRESET_COUNT 25

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个动力系统预设
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
static bool register_ds_preset(
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

bool preset_dynamical_systems_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：稳定性分析（4个预设）
     *
     * 稳定性分析是动力系统理论的核心内容，研究平衡点附近
     * 轨道的行为。主要方法包括 Lyapunov 直接法和线性化方法。
     * ============================================================ */

    /* Lyapunov 直接法稳定性判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_LYAPUNOV_DIRECT,
                "Lyapunov 直接法：若存在正定函数 V 使 dV/dt ≤ 0，则平衡点稳定",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "V(\\mathbf{x}) > 0\\ (\\mathbf{x} \\neq 0),\\ V(0) = 0, \\quad "
                "\\dot{V} = \\nabla V \\cdot f(\\mathbf{x}) \\leq 0 "
                "\\Rightarrow \\text{稳定}",
                "O(n)", false, false)) {
            success_count++;
        }
    }

    /* 渐近稳定性判定（线性化） */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_ASYMPTOTIC_LINEAR,
                "线性化渐近稳定性：若 Jacobian 特征值实部全负，则渐近稳定",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "J = Df(\\mathbf{x}^*), \\quad "
                "\\text{Re}(\\lambda_i) < 0\\ \\forall i "
                "\\Rightarrow \\mathbf{x}^* \\text{ 渐近稳定}",
                "O(n³)", false, false)) {
            success_count++;
        }
    }

    /* 指数稳定性判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_EXPONENTIAL_STABILITY,
                "指数稳定性：存在 α,β > 0 使 ||x(t)|| ≤ β||x₀||e^(-αt)",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "\\|\\mathbf{x}(t)\\| \\leq \\beta\\|\\mathbf{x}_0\\| e^{-\\alpha t}, \\quad "
                "\\alpha, \\beta > 0, \\quad "
                "\\dot{V} \\leq -\\alpha V \\Rightarrow \\text{指数稳定}",
                "O(n)", false, false)) {
            success_count++;
        }
    }

    /* 中心流形约化 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_CENTER_MANIFOLD,
                "中心流形约化：将高维系统降维到中心特征空间",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "\\mathbf{x} = \\mathbf{y} + \\mathbf{h}(\\mathbf{y}), \\quad "
                "\\mathbf{y} \\in E^c, \\quad "
                "\\dot{\\mathbf{y}} = A_c\\mathbf{y} + f_c(\\mathbf{y}, \\mathbf{h}(\\mathbf{y}))",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：分岔分析（5个预设）
     *
     * 分岔理论研究参数变化时系统定性行为的改变。
     * 常见的分岔类型包括鞍点分岔、跨临界分岔、Pitchfork分岔和Hopf分岔。
     * ============================================================ */

    /* 鞍点分岔判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_SADDLE_NODE,
                "鞍点分岔（Saddle-Node）：平衡点随参数消失/产生",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "\\dot{x} = \\mu - x^2, \\quad "
                "\\mu < 0: \\text{无平衡点}, \\quad "
                "\\mu = 0: \\text{半稳定平衡点}, \\quad "
                "\\mu > 0: \\text{两个平衡点}",
                "O(1)", false, false)) {
            success_count++;
        }
    }

    /* 跨临界分岔判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_TRANSCRITICAL,
                "跨临界分岔（Transcritical）：两个平衡点交换稳定性",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "\\dot{x} = \\mu x - x^2, \\quad "
                "x^* = 0: \\mu < 0 \\text{ 稳定}, \\mu > 0 \\text{ 不稳定}, \\quad "
                "x^* = \\mu: \\text{相反}",
                "O(1)", false, false)) {
            success_count++;
        }
    }

    /* Pitchfork 分岔判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_BOOLEAN};
        if (register_ds_preset(
                PRESET_DS_PITCHFORK,
                "Pitchfork 分岔：对称系统中平衡点的对称性破缺",
                inputs, 4, PRESET_TYPE_BOOLEAN,
                "\\dot{x} = \\mu x - x^3\\ (\\text{超临界}), \\quad "
                "\\dot{x} = \\mu x + x^3\\ (\\text{亚临界}), \\quad "
                "\\mu = 0: \\text{分岔点}",
                "O(1)", false, false)) {
            success_count++;
        }
    }

    /* Hopf 分岔判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_HOPF,
                "Hopf 分岔：平衡点失稳产生极限环",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "\\text{条件: } \\text{Re}(\\lambda_{1,2}) = 0, \\quad "
                "\\frac{d}{d\\mu}\\text{Re}(\\lambda) \\neq 0, \\quad "
                "\\text{第一Lyapunov系数} \\neq 0",
                "O(n²)", false, false)) {
            success_count++;
        }
    }

    /* 分岔图计算 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_BIFURCATION_DIAGRAM,
                "分岔图计算：绘制平衡点/周期解随参数变化的曲线",
                inputs, 4, PRESET_TYPE_FUNCTION,
                "(x^*, \\mu): f(x^*, \\mu) = 0, \\quad "
                "\\text{稳定: 实线}, \\quad "
                "\\text{不稳定: 虚线}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：极限环与周期解（4个预设）
     *
     * 极限环是孤立的周期轨道，是自激振荡的数学描述。
     * Poincaré映射和Floquet理论是研究周期解的重要工具。
     * ============================================================ */

    /* Poincaré 映射构造 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SURFACE, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_POINCARE_MAP,
                "Poincaré 映射：周期轨道的截面映射",
                inputs, 3, PRESET_TYPE_FUNCTION,
                "P: \\Sigma \\to \\Sigma, \\quad "
                "\\mathbf{x}_{n+1} = P(\\mathbf{x}_n), \\quad "
                "\\text{不动点} \\Leftrightarrow \\text{周期轨道}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 极限环稳定性分析 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_LIMIT_CYCLE_STABILITY,
                "极限环稳定性：通过 Poincaré 映射的 Jacobian 判定",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "\\lambda_i = \\text{Floquet乘子}, \\quad "
                "|\\lambda_i| < 1\\ \\forall i \\Rightarrow \\text{稳定}, \\quad "
                "\\exists i: |\\lambda_i| > 1 \\Rightarrow \\text{不稳定}",
                "O(n²)", false, false)) {
            success_count++;
        }
    }

    /* 周期解求解（谐波平衡） */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_INTEGER};
        if (register_ds_preset(
                PRESET_DS_HARMONIC_BALANCE,
                "谐波平衡法：设 x(t) = Σaₙcos(nωt) + bₙsin(nωt) 求周期解",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "x(t) = \\sum_{n=0}^{N} a_n\\cos(n\\omega t) + b_n\\sin(n\\omega t), \\quad "
                "\\text{代入方程，平衡谐波分量}",
                "O(N²)", true, false)) {
            success_count++;
        }
    }

    /* Floquet 乘子计算 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_FLOQUET_MULTIPLIERS,
                "Floquet 乘子：周期系数线性系统的特征指数",
                inputs, 2, PRESET_TYPE_LIST,
                "\\dot{\\mathbf{x}} = A(t)\\mathbf{x}, \\quad "
                "A(t+T) = A(t), \\quad "
                "\\Phi(T) = \\text{单周期矩阵}, \\quad "
                "\\lambda_i = \\text{eig}(\\Phi(T))",
                "O(n³)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：混沌与吸引子（4个预设）
     *
     * 混沌是确定性系统中的非周期行为，具有对初值敏感依赖性。
     * Lyapunov 指数是刻画混沌的重要指标。
     * ============================================================ */

    /* Lyapunov 指数计算 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_ds_preset(
                PRESET_DS_LYAPUNOV_EXPONENTS,
                "Lyapunov 指数：刻画相邻轨道的指数分离率",
                inputs, 3, PRESET_TYPE_LIST,
                "\\lambda_i = \\lim_{t\\to\\infty} \\frac{1}{t}"
                "\\ln\\frac{\\|\\delta\\mathbf{x}_i(t)\\|}{\\|\\delta\\mathbf{x}_i(0)\\|}, \\quad "
                "\\lambda_{\\max} > 0 \\Rightarrow \\text{混沌}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 混沌判定（Devaney 定义） */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_REGION};
        if (register_ds_preset(
                PRESET_DS_CHAOS_DEVANEY,
                "Devaney 混沌定义：初值敏感、拓扑传递、周期点稠密",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "\\text{1. 初值敏感: } \\exists\\delta > 0, "
                "\\forall\\mathbf{x}, \\exists n: d(f^n(\\mathbf{x}), f^n(\\mathbf{y})) > \\delta, \\quad "
                "\\text{2. 拓扑传递}, \\quad "
                "\\text{3. 周期点稠密}",
                "O(n²)", false, false)) {
            success_count++;
        }
    }

    /* Lorenz 吸引子 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_LORENZ_ATTRACTOR,
                "Lorenz 吸引子：ẋ = σ(y-x), ẏ = x(ρ-z)-y, ż = xy-βz",
                inputs, 3, PRESET_TYPE_FUNCTION,
                "\\dot{x} = \\sigma(y-x), \\quad "
                "\\dot{y} = x(\\rho-z)-y, \\quad "
                "\\dot{z} = xy - \\beta z, \\quad "
                "\\sigma = 10, \\rho = 28, \\beta = 8/3",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* Henon 映射 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_HENON_MAP,
                "Henon 映射：x_{n+1} = 1 - ax_n² + y_n, y_{n+1} = bx_n",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "x_{n+1} = 1 - ax_n^2 + y_n, \\quad "
                "y_{n+1} = bx_n, \\quad "
                "a = 1.4, b = 0.3 \\Rightarrow \\text{奇怪吸引子}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：不变流形（3个预设）
     *
     * 不变流形是动力系统的重要几何对象，包括稳定流形、
     * 不稳定流形和惯性流形。
     * ============================================================ */

    /* 稳定流形 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_ds_preset(
                PRESET_DS_STABLE_MANIFOLD,
                "稳定流形 W^s(x*)：所有渐近趋近平衡点的轨道集合",
                inputs, 3, PRESET_TYPE_SURFACE,
                "W^s(\\mathbf{x}^*) = \\{\\mathbf{x}: \\phi_t(\\mathbf{x}) \\to \\mathbf{x}^* "
                "\\text{ as } t \\to +\\infty\\}, \\quad "
                "\\dim W^s = \\#\\{\\lambda_i: \\text{Re}(\\lambda_i) < 0\\}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 不稳定流形 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_ds_preset(
                PRESET_DS_UNSTABLE_MANIFOLD,
                "不稳定流形 W^u(x*)：所有远离平衡点的轨道集合",
                inputs, 3, PRESET_TYPE_SURFACE,
                "W^u(\\mathbf{x}^*) = \\{\\mathbf{x}: \\phi_t(\\mathbf{x}) \\to \\mathbf{x}^* "
                "\\text{ as } t \\to -\\infty\\}, \\quad "
                "\\dim W^u = \\#\\{\\lambda_i: \\text{Re}(\\lambda_i) > 0\\}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 惯性流形 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_INERTIAL_MANIFOLD,
                "惯性流形：有限维不变流形，吸引所有轨道",
                inputs, 3, PRESET_TYPE_SURFACE,
                "\\mathcal{M} = \\text{graph}(\\Phi), \\quad "
                "\\Phi: E^+ \\to E^-, \\quad "
                "\\text{吸引性: } \\text{dist}(u(t), \\mathcal{M}) \\leq Ce^{-\\alpha t}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：渐近方法（5个预设）
     *
     * 渐近方法是处理含小参数的动力系统的重要工具，
     * 包括平均法、多尺度方法和奇异摄动方法。
     * ============================================================ */

    /* 平均法 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_METHOD_AVG,
                "平均法：对快速振荡系统进行时间平均",
                inputs, 3, PRESET_TYPE_FUNCTION,
                "\\dot{\\mathbf{x}} = \\epsilon f(\\mathbf{x}, t), \\quad "
                "\\bar{f}(\\mathbf{x}) = \\lim_{T\\to\\infty} \\frac{1}{T}\\int_0^T f(\\mathbf{x}, t)\\,dt, \\quad "
                "\\dot{\\bar{\\mathbf{x}}} = \\epsilon\\bar{f}(\\bar{\\mathbf{x}})",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 多尺度方法 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_ds_preset(
                PRESET_DS_MULTIPLE_SCALES,
                "多尺度方法：引入多个时间尺度 T₀, T₁, T₂, ...",
                inputs, 3, PRESET_TYPE_FUNCTION,
                "x(t) = x_0(T_0, T_1, \\ldots) + \\epsilon x_1 + \\cdots, \\quad "
                "T_n = \\epsilon^n t, \\quad "
                "\\frac{d}{dt} = \\frac{\\partial}{\\partial T_0} + "
                "\\epsilon\\frac{\\partial}{\\partial T_1} + \\cdots",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 奇异摄动法 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_SINGULAR_PERTURBATION,
                "奇异摄动法：处理边界层问题",
                inputs, 3, PRESET_TYPE_FUNCTION,
                "\\epsilon\\dot{x} = f(x,y), \\quad "
                "\\dot{y} = g(x,y), \\quad "
                "\\text{外解} + \\text{边界层修正}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* WKBJ 近似 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_ds_preset(
                PRESET_DS_WKBJ,
                "WKBJ 近似：大参数渐近展开",
                inputs, 2, PRESET_TYPE_FUNCTION,
                "y(x) \\sim \\exp\\left[\\frac{1}{\\delta}\\int S_0(x)\\,dx + "
                "\\int S_1(x)\\,dx + \\cdots\\right], \\quad "
                "\\delta \\to 0",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == DYNAMICAL_SYSTEMS_PRESET_COUNT;
}

/* ==================== 模块信息接口 ==================== */

int preset_dynamical_systems_count(void)
{
    return DYNAMICAL_SYSTEMS_PRESET_COUNT;
}

PresetCategory preset_dynamical_systems_category(void)
{
    return PRESET_CATEGORY_ANALYSIS;
}

bool preset_dynamical_systems_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    /* 分配名称数组 */
    char **names = (char**)lv00_malloc(DYNAMICAL_SYSTEMS_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

    /* 预设名称列表 */
    const char *preset_names[] = {
        /* 稳定性分析 */
        PRESET_DS_LYAPUNOV_DIRECT,
        PRESET_DS_ASYMPTOTIC_LINEAR,
        PRESET_DS_EXPONENTIAL_STABILITY,
        PRESET_DS_CENTER_MANIFOLD,
        /* 分岔分析 */
        PRESET_DS_SADDLE_NODE,
        PRESET_DS_TRANSCRITICAL,
        PRESET_DS_PITCHFORK,
        PRESET_DS_HOPF,
        PRESET_DS_BIFURCATION_DIAGRAM,
        /* 极限环与周期解 */
        PRESET_DS_POINCARE_MAP,
        PRESET_DS_LIMIT_CYCLE_STABILITY,
        PRESET_DS_HARMONIC_BALANCE,
        PRESET_DS_FLOQUET_MULTIPLIERS,
        /* 混沌与吸引子 */
        PRESET_DS_LYAPUNOV_EXPONENTS,
        PRESET_DS_CHAOS_DEVANEY,
        PRESET_DS_LORENZ_ATTRACTOR,
        PRESET_DS_HENON_MAP,
        /* 不变流形 */
        PRESET_DS_STABLE_MANIFOLD,
        PRESET_DS_UNSTABLE_MANIFOLD,
        PRESET_DS_INERTIAL_MANIFOLD,
        /* 渐近方法 */
        PRESET_DS_METHOD_AVG,
        PRESET_DS_MULTIPLE_SCALES,
        PRESET_DS_SINGULAR_PERTURBATION,
        PRESET_DS_WKBJ,
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
