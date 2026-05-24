/**
 * @file preset_mathematical_physics.c
 * @brief 数学物理方程预设函数块 - 实现
 *
 * @details 实现数学物理模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共25个预设，涵盖波动方程、热传导方程、位势方程、
 *          量子力学方程、电磁场方程和流体力学方程。
 *
 * @module MathematicalPhysics
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 */

#include "preset_mathematical_physics.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 数学物理模块预设函数块总数 */
#define MATHEMATICAL_PHYSICS_PRESET_COUNT 25

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个数学物理预设
 *
 * 统一调用 preset_blocks_register_simple，类别固定为
 * PRESET_CATEGORY_ANALYSIS。
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
static bool register_mp_preset(const char *name, const char *description, const PresetType *input_types,
                               int input_count, PresetType output_type, const char *math_def, const char *complexity,
                               bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_ANALYSIS, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_mathematical_physics_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：波动方程（4个预设）
     *
     * 波动方程是描述波传播的基本偏微分方程：
     *   ∂²u/∂t² = c²∇²u
     * 包括一维、二维、三维情形的解析解公式。
     * ============================================================ */

    /* 一维波动方程 d'Alembert 解 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_mp_preset(PRESET_MP_WAVE_1D_DALEMBERT,
                               "一维波动方程 d'Alembert 解：u(x,t) = [f(x-ct) + f(x+ct)]/2 + (1/2c)∫g(s)ds", inputs, 3,
                               PRESET_TYPE_FUNCTION,
                               "u(x,t) = \\frac{1}{2}[\\varphi(x-ct) + \\varphi(x+ct)] + "
                               "\\frac{1}{2c}\\int_{x-ct}^{x+ct} \\psi(s)\\,ds",
                               "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 二维波动方程 Kirchhoff 公式 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_mp_preset(PRESET_MP_WAVE_2D_KIRCHHOFF, "二维波动方程 Kirchhoff 公式：用圆周平均表示解", inputs, 3,
                               PRESET_TYPE_FUNCTION,
                               "u(x,y,t) = \\frac{1}{2\\pi c}\\frac{\\partial}{\\partial t}"
                               "\\iint_{r \\le ct} \\frac{\\varphi(\\xi,\\eta)}{\\sqrt{c^2t^2-r^2}}\\,d\\xi\\,d\\eta",
                               "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 三维波动方程 Poisson 公式 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_mp_preset(PRESET_MP_WAVE_3D_POISSON, "三维波动方程 Poisson 公式：球面平均表示解", inputs, 3,
                               PRESET_TYPE_FUNCTION,
                               "u(\\mathbf{x},t) = \\frac{1}{4\\pi c^2 t}"
                               "\\iint_{|\\mathbf{y}-\\mathbf{x}|=ct} \\psi(\\mathbf{y})\\,dS + "
                               "\\frac{\\partial}{\\partial t}\\left[\\frac{1}{4\\pi c^2 t}"
                               "\\iint_{|\\mathbf{y}-\\mathbf{x}|=ct} \\varphi(\\mathbf{y})\\,dS\\right]",
                               "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 波动方程分离变量解 */
    {
        PresetType inputs[] = {PRESET_TYPE_EQUATION, PRESET_TYPE_REGION};
        if (register_mp_preset(PRESET_MP_WAVE_SEPARATION,
                               "波动方程分离变量法：设 u(x,t) = X(x)T(t)，分解为空间和时间方程", inputs, 2,
                               PRESET_TYPE_LIST,
                               "u(\\mathbf{x},t) = X(\\mathbf{x})T(t), \\quad "
                               "\\nabla^2 X + \\lambda X = 0, \\quad "
                               "\\ddot{T} + c^2\\lambda T = 0",
                               "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：热传导方程（4个预设）
     *
     * 热传导方程描述热量的扩散过程：
     *   ∂u/∂t = α∇²u
     * 包括 Fourier 级数解、基本解和数值方法。
     * ============================================================ */

    /* 一维热传导方程 Fourier 解 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_mp_preset(PRESET_MP_HEAT_1D_FOURIER,
                               "一维热传导方程 Fourier 级数解：u(x,t) = Σaₙexp(-n²π²αt/L²)sin(nπx/L)", inputs, 3,
                               PRESET_TYPE_FUNCTION,
                               "u(x,t) = \\sum_{n=1}^{\\infty} a_n e^{-n^2\\pi^2\\alpha t/L^2} "
                               "\\sin\\left(\\frac{n\\pi x}{L}\\right), \\quad "
                               "a_n = \\frac{2}{L}\\int_0^L f(x)\\sin\\left(\\frac{n\\pi x}{L}\\right)dx",
                               "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 热传导方程基本解（热核） */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_mp_preset(PRESET_MP_HEAT_FUNDAMENTAL,
                               "热传导方程基本解（热核）：K(x,t) = (4παt)^(-d/2)exp(-|x|²/4αt)", inputs, 2,
                               PRESET_TYPE_FUNCTION,
                               "K(\\mathbf{x},t) = \\frac{1}{(4\\pi\\alpha t)^{d/2}}"
                               "\\exp\\left(-\\frac{|\\mathbf{x}|^2}{4\\alpha t}\\right), \\quad t > 0",
                               "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 热传导方程分离变量解 */
    {
        PresetType inputs[] = {PRESET_TYPE_EQUATION, PRESET_TYPE_REGION};
        if (register_mp_preset(PRESET_MP_HEAT_SEPARATION, "热传导方程分离变量法：设 u(x,t) = X(x)T(t)", inputs, 2,
                               PRESET_TYPE_LIST,
                               "u(\\mathbf{x},t) = X(\\mathbf{x})T(t), \\quad "
                               "\\nabla^2 X + \\lambda X = 0, \\quad "
                               "\\dot{T} + \\alpha\\lambda T = 0",
                               "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 热传导方程有限差分格式 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_mp_preset(PRESET_MP_HEAT_FINITE_DIFF, "热传导方程有限差分格式：显式/隐式/ Crank-Nicolson 格式",
                               inputs, 4, PRESET_TYPE_FUNCTION,
                               "\\frac{u_i^{n+1} - u_i^n}{\\Delta t} = \\alpha "
                               "\\frac{u_{i+1}^n - 2u_i^n + u_{i-1}^n}{(\\Delta x)^2} \\quad \\text{(显式)}, \\quad "
                               "r = \\frac{\\alpha\\Delta t}{(\\Delta x)^2} \\le \\frac{1}{2}",
                               "O(N)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：位势方程（4个预设）
     *
     * 位势方程包括 Laplace 方程和 Poisson 方程：
     *   ∇²u = 0 (Laplace)
     *   ∇²u = f (Poisson)
     * 是静电场、重力场等的基本方程。
     * ============================================================ */

    /* Laplace 方程分离变量解 */
    {
        PresetType inputs[] = {PRESET_TYPE_REGION, PRESET_TYPE_STRING};
        if (register_mp_preset(PRESET_MP_LAPLACE_SEPARATION, "Laplace 方程分离变量法：在矩形/圆形/球形区域分离变量",
                               inputs, 2, PRESET_TYPE_FUNCTION,
                               "\\nabla^2 u = 0, \\quad "
                               "\\text{矩形}: u = \\sum (A_n\\cosh + B_n\\sinh)(A'_n\\cos + B'_n\\sin), \\quad "
                               "\\text{圆}: u = \\sum (A_n r^n + B_n r^{-n})(C_n\\cos n\\theta + D_n\\sin n\\theta)",
                               "O(n)", true, false)) {
            success_count++;
        }
    }

    /* Poisson 方程 Green 函数法 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_REGION};
        if (register_mp_preset(
                PRESET_MP_POISSON_GREEN, "Poisson 方程 Green 函数法：u(x) = ∫G(x,y)f(y)dy + 边界项", inputs, 2,
                PRESET_TYPE_FUNCTION,
                "u(\\mathbf{x}) = \\int_\\Omega G(\\mathbf{x},\\mathbf{y})f(\\mathbf{y})\\,d\\mathbf{y} + "
                "\\int_{\\partial\\Omega} \\left[ G\\frac{\\partial u}{\\partial n} - "
                "u\\frac{\\partial G}{\\partial n} \\right]\\,dS",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 调和函数均值性质 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_mp_preset(PRESET_MP_HARMONIC_MEAN, "调和函数均值性质：u(x) = (1/|∂B|)∫∂B u dS = (1/|B|)∫B u dV",
                               inputs, 2, PRESET_TYPE_BOOLEAN,
                               "u(\\mathbf{x}_0) = \\frac{1}{|\\partial B_r|}\\int_{\\partial B_r} "
                               "u(\\mathbf{x})\\,dS = \\frac{1}{|B_r|}\\int_{B_r} u(\\mathbf{x})\\,dV, \\quad "
                               "\\forall B_r(\\mathbf{x}_0) \\subset \\Omega",
                               "O(1)", false, false)) {
            success_count++;
        }
    }

    /* 极大值原理判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_REGION};
        if (register_mp_preset(PRESET_MP_MAX_PRINCIPLE, "极大值原理判定：调和函数在内部不能达到极值", inputs, 2,
                               PRESET_TYPE_BOOLEAN,
                               "\\text{若 } u \\text{ 在 } \\Omega \\text{ 内调和, 则 } "
                               "\\max_\\Omega u = \\max_{\\partial\\Omega} u, \\quad "
                               "\\min_\\Omega u = \\min_{\\partial\\Omega} u",
                               "O(1)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：量子力学方程（4个预设）
     *
     * Schrödinger 方程是量子力学的基本方程：
     *   iℏ∂ψ/∂t = Ĥψ
     * 包括定态和含时情形。
     * ============================================================ */

    /* 定态 Schrödinger 方程求解 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_mp_preset(PRESET_MP_SCHRODINGER_STATIONARY, "定态 Schrödinger 方程：Ĥψ = Eψ，求解本征值问题",
                               inputs, 2, PRESET_TYPE_TUPLE,
                               "\\hat{H}\\psi = E\\psi, \\quad "
                               "\\hat{H} = -\\frac{\\hbar^2}{2m}\\nabla^2 + V(\\mathbf{x}), \\quad "
                               "\\text{输出: } (E_n, \\psi_n)",
                               "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 含时 Schrödinger 方程演化 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_mp_preset(PRESET_MP_SCHRODINGER_TIME, "含时 Schrödinger 方程演化：ψ(x,t) = Σcₙψₙ(x)exp(-iEₙt/ℏ)",
                               inputs, 3, PRESET_TYPE_FUNCTION,
                               "\\psi(\\mathbf{x},t) = \\sum_n c_n \\psi_n(\\mathbf{x}) "
                               "e^{-iE_n t/\\hbar}, \\quad "
                               "c_n = \\langle\\psi_n|\\psi_0\\rangle",
                               "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 一维势阱本征值问题 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_mp_preset(PRESET_MP_POTENTIAL_WELL, "一维无限深势阱：Eₙ = n²π²ℏ²/(2mL²)，ψₙ = √(2/L)sin(nπx/L)",
                               inputs, 2, PRESET_TYPE_TUPLE,
                               "E_n = \\frac{n^2\\pi^2\\hbar^2}{2mL^2}, \\quad "
                               "\\psi_n(x) = \\sqrt{\\frac{2}{L}}\\sin\\left(\\frac{n\\pi x}{L}\\right), \\quad "
                               "n = 1, 2, 3, \\ldots",
                               "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 谐振子本征态 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_mp_preset(PRESET_MP_HARMONIC_OSCILLATOR, "量子谐振子：Eₙ = ℏω(n+1/2)，ψₙ 用 Hermite 多项式表示",
                               inputs, 2, PRESET_TYPE_TUPLE,
                               "E_n = \\hbar\\omega\\left(n + \\frac{1}{2}\\right), \\quad "
                               "\\psi_n(x) = \\left(\\frac{m\\omega}{\\pi\\hbar}\\right)^{1/4} "
                               "\\frac{1}{\\sqrt{2^n n!}} H_n(\\xi) e^{-\\xi^2/2}, \\quad "
                               "\\xi = \\sqrt{\\frac{m\\omega}{\\hbar}}x",
                               "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：电磁场方程（4个预设）
     *
     * Maxwell 方程组是电磁学的基本方程：
     *   ∇·E = ρ/ε₀, ∇×E = -∂B/∂t
     *   ∇·B = 0, ∇×B = μ₀J + μ₀ε₀∂E/∂t
     * ============================================================ */

    /* Maxwell 方程组（时域） */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_mp_preset(PRESET_MP_MAXWELL_TIME, "Maxwell 方程组时域形式：描述电磁场的演化", inputs, 3,
                               PRESET_TYPE_TUPLE,
                               "\\nabla \\cdot \\mathbf{E} = \\frac{\\rho}{\\varepsilon_0}, \\quad "
                               "\\nabla \\times \\mathbf{E} = -\\frac{\\partial\\mathbf{B}}{\\partial t}, \\quad "
                               "\\nabla \\cdot \\mathbf{B} = 0, \\quad "
                               "\\nabla \\times \\mathbf{B} = \\mu_0\\mathbf{J} + \\mu_0\\varepsilon_0"
                               "\\frac{\\partial\\mathbf{E}}{\\partial t}",
                               "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 静电场 Poisson 方程 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_mp_preset(PRESET_MP_ELECTROSTATIC, "静电场 Poisson 方程：∇²φ = -ρ/ε₀", inputs, 1,
                               PRESET_TYPE_FUNCTION,
                               "\\nabla^2 \\phi = -\\frac{\\rho}{\\varepsilon_0}, \\quad "
                               "\\mathbf{E} = -\\nabla\\phi, \\quad "
                               "\\phi(\\mathbf{x}) = \\frac{1}{4\\pi\\varepsilon_0}"
                               "\\int \\frac{\\rho(\\mathbf{x}')}{|\\mathbf{x}-\\mathbf{x}'|}\\,d\\mathbf{x}'",
                               "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 静磁场 Biot-Savart 定律 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_mp_preset(PRESET_MP_MAGNETOSTATIC, "静磁场 Biot-Savart 定律：B = (μ₀/4π)∫J×r̂/r² dV", inputs, 1,
                               PRESET_TYPE_FUNCTION,
                               "\\mathbf{B}(\\mathbf{x}) = \\frac{\\mu_0}{4\\pi}"
                               "\\int \\frac{\\mathbf{J}(\\mathbf{x}') \\times "
                               "(\\mathbf{x}-\\mathbf{x}')}{|\\mathbf{x}-\\mathbf{x}'|^3}\\,d\\mathbf{x}', \\quad "
                               "\\nabla \\times \\mathbf{B} = \\mu_0\\mathbf{J}",
                               "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 电磁波传播方程 */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_mp_preset(
                PRESET_MP_EM_WAVE, "电磁波传播方程：∇²E = (1/c²)∂²E/∂t²", inputs, 2, PRESET_TYPE_FUNCTION,
                "\\nabla^2 \\mathbf{E} = \\frac{1}{c^2}\\frac{\\partial^2\\mathbf{E}}{\\partial t^2}, \\quad "
                "\\nabla^2 \\mathbf{B} = \\frac{1}{c^2}\\frac{\\partial^2\\mathbf{B}}{\\partial t^2}, \\quad "
                "c = \\frac{1}{\\sqrt{\\mu_0\\varepsilon_0}}",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：流体力学方程（5个预设）
     *
     * Navier-Stokes 方程描述粘性流体运动：
     *   ρ(∂u/∂t + u·∇u) = -∇p + μ∇²u + f
     * Euler 方程描述理想流体。
     * ============================================================ */

    /* Navier-Stokes 方程（不可压缩） */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_mp_preset(PRESET_MP_NAVIER_STOKES, "不可压缩 Navier-Stokes 方程：ρ(∂u/∂t + u·∇u) = -∇p + μ∇²u",
                               inputs, 3, PRESET_TYPE_FUNCTION,
                               "\\rho\\left(\\frac{\\partial\\mathbf{u}}{\\partial t} + "
                               "\\mathbf{u}\\cdot\\nabla\\mathbf{u}\\right) = "
                               "-\\nabla p + \\mu\\nabla^2\\mathbf{u} + \\mathbf{f}, \\quad "
                               "\\nabla \\cdot \\mathbf{u} = 0",
                               "O(n³)", true, false)) {
            success_count++;
        }
    }

    /* Euler 方程（理想流体） */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_mp_preset(PRESET_MP_EULER_FLUID, "Euler 方程（理想流体）：ρ(∂u/∂t + u·∇u) = -∇p", inputs, 2,
                               PRESET_TYPE_FUNCTION,
                               "\\rho\\left(\\frac{\\partial\\mathbf{u}}{\\partial t} + "
                               "\\mathbf{u}\\cdot\\nabla\\mathbf{u}\\right) = -\\nabla p + \\mathbf{f}, \\quad "
                               "\\nabla \\cdot \\mathbf{u} = 0, \\quad "
                               "\\text{Bernoulli: } \\frac{p}{\\rho} + \\frac{u^2}{2} + gz = \\text{const}",
                               "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 边界层方程 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_mp_preset(PRESET_MP_BOUNDARY_LAYER, "边界层方程（Prandtl）：描述高雷诺数下的边界层流动", inputs, 2,
                               PRESET_TYPE_FUNCTION,
                               "u\\frac{\\partial u}{\\partial x} + v\\frac{\\partial u}{\\partial y} = "
                               "U\\frac{dU}{dx} + \\nu\\frac{\\partial^2 u}{\\partial y^2}, \\quad "
                               "\\frac{\\partial u}{\\partial x} + \\frac{\\partial v}{\\partial y} = 0",
                               "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == MATHEMATICAL_PHYSICS_PRESET_COUNT;
}

/* ==================== 模块信息接口 ==================== */

int preset_mathematical_physics_count(void) {
    return MATHEMATICAL_PHYSICS_PRESET_COUNT;
}

PresetCategory preset_mathematical_physics_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}

bool preset_mathematical_physics_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    /* 分配名称数组 */
    char **names = (char **) lv00_malloc(MATHEMATICAL_PHYSICS_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    /* 预设名称列表 */
    const char *preset_names[] = {
        /* 波动方程 */
        PRESET_MP_WAVE_1D_DALEMBERT,
        PRESET_MP_WAVE_2D_KIRCHHOFF,
        PRESET_MP_WAVE_3D_POISSON,
        PRESET_MP_WAVE_SEPARATION,
        /* 热传导方程 */
        PRESET_MP_HEAT_1D_FOURIER,
        PRESET_MP_HEAT_FUNDAMENTAL,
        PRESET_MP_HEAT_SEPARATION,
        PRESET_MP_HEAT_FINITE_DIFF,
        /* 位势方程 */
        PRESET_MP_LAPLACE_SEPARATION,
        PRESET_MP_POISSON_GREEN,
        PRESET_MP_HARMONIC_MEAN,
        PRESET_MP_MAX_PRINCIPLE,
        /* 量子力学方程 */
        PRESET_MP_SCHRODINGER_STATIONARY,
        PRESET_MP_SCHRODINGER_TIME,
        PRESET_MP_POTENTIAL_WELL,
        PRESET_MP_HARMONIC_OSCILLATOR,
        /* 电磁场方程 */
        PRESET_MP_MAXWELL_TIME,
        PRESET_MP_ELECTROSTATIC,
        PRESET_MP_MAGNETOSTATIC,
        PRESET_MP_EM_WAVE,
        /* 流体力学方程 */
        PRESET_MP_NAVIER_STOKES,
        PRESET_MP_EULER_FLUID,
        PRESET_MP_BOUNDARY_LAYER,
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
