/**
 * @file preset_integral_transforms.c
 * @brief 积分变换预设函数块 - 实现
 *
 * 实现理论数学研究中常用的积分变换运算预设函数块。
 * 涵盖傅里叶分析、拉普拉斯变换、Z变换、其他积分变换和卷积运算五大领域。
 * 共19个预设函数块，均遵循模块化、确定性原则。
 *
 * 采用统一的 preset_blocks_register_simple 注册接口，
 * 使用 REGISTER_IT 宏模式简化注册代码。
 *
 * @module IntegralTransforms
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "preset_integral_transforms.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 积分变换模块预设函数块总数（与头文件中 INTEGRAL_TRANSFORMS_PRESET_COUNT 一致） */
#define IT_PRESET_COUNT INTEGRAL_TRANSFORMS_PRESET_COUNT

/* ==================== REGISTER_IT 宏定义 ==================== */

/**
 * @brief 注册单个积分变换预设的便捷宏
 *
 * 封装 preset_blocks_register_simple 调用，简化注册代码。
 * 所有积分变换预设使用 PRESET_CATEGORY_ANALYSIS 类别。
 *
 * @param preset_name   预设名称常量（头文件中定义的宏）
 * @param desc          中文描述
 * @param inputs        输入类型数组（PresetType 复合字面量）
 * @param n_inputs      输入数量
 * @param output        输出类型
 * @param math          数学定义（LaTeX 格式）
 * @param comp          时间复杂度
 * @param constructive  是否构造性
 * @param reversible    是否可逆
 */
#define REGISTER_IT(preset_name, desc, n_inputs, output, math, comp, constructive, reversible, ...)               \
    do {                                                                                                          \
        PresetType _in[] = {__VA_ARGS__};                                                                         \
        if (register_it_preset(preset_name, desc, _in, n_inputs, output, math, comp, constructive, reversible)) { \
            success_count++;                                                                                      \
        }                                                                                                         \
    } while (0)

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个积分变换预设
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
static bool register_it_preset(const char *name, const char *description, const PresetType *input_types,
                               int input_count, PresetType output_type, const char *math_def, const char *complexity,
                               bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_ANALYSIS, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_integral_transforms_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：傅里叶分析（6个）
     * ============================================================ */

    /**
     * @brief 傅里叶级数
     *
     * @details 将周期为 T 的函数展开为三角级数形式。
     *          傅里叶级数将周期信号分解为直流分量与各次谐波之和，
     *          是频域分析和信号处理的基础。
     * @param 周期函数（PRESET_TYPE_FUNCTION）和周期（PRESET_TYPE_SCALAR）
     * @return 傅里叶级数系数（PRESET_TYPE_SERIES）
     * @math f(t) = \frac{a_0}{2} + \sum_{n=1}^{\infty} [a_n \cos(n\omega t) + b_n \sin(n\omega t)]
     * @complexity O(N)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_IT(PRESET_IT_FOURIER_SERIES, "傅里叶级数：将周期函数 f(t) 展开为正弦/余弦项之和，计算系数 {a_n, b_n}", 2,
                PRESET_TYPE_SERIES,
                "f(t) = \\frac{a_0}{2} + \\sum_{n=1}^{\\infty} "
                "[a_n \\cos(n\\omega t) + b_n \\sin(n\\omega t)], \\quad "
                "a_n = \\frac{2}{T}\\int_0^T f(t)\\cos(n\\omega t)\\,dt, \\quad "
                "b_n = \\frac{2}{T}\\int_0^T f(t)\\sin(n\\omega t)\\,dt",
                "O(N^2)", true, true, PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR);
    /**
     * @brief 傅里叶变换
     *
     * @details 将时域函数变换到频域，得到复值频谱函数。
     *          傅里叶变换是信号处理和微分方程求解的核心工具，
     *          将微分方程转化为代数方程。
     * @param 时域函数（PRESET_TYPE_FUNCTION）
     * @return 频域函数（PRESET_TYPE_FUNCTION）
     * @math \hat{f}(\xi) = \int_{-\infty}^{\infty} f(x) e^{-2\pi i x\xi}\,dx
     * @complexity O(N log N)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_IT(PRESET_IT_FOURIER_TRANSFORM,
                "傅里叶变换：将时域信号 f(t) 变换为频域表示 F(omega) = ∫ f(t) e^{-i omega t} dt", 1,
                PRESET_TYPE_FUNCTION, "\\hat{f}(\\xi) = \\int_{-\\infty}^{\\infty} f(x)\\,e^{-2\\pi i x\\xi}\\,dx",
                "O(N \\log N)", true, true, PRESET_TYPE_FUNCTION);
    /**
     * @brief 逆傅里叶变换
     *
     * @details 将频域函数恢复为时域信号。
     *          逆傅里叶变换与傅里叶变换构成可逆对，
     *          在数学上几乎处处互为逆运算。
     * @param 频域函数（PRESET_TYPE_FUNCTION）
     * @return 时域函数（PRESET_TYPE_FUNCTION）
     * @math f(x) = \int_{-\infty}^{\infty} \hat{f}(\xi) e^{2\pi i x\xi}\,d\xi
     * @complexity O(N log N)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_IT(PRESET_IT_INVERSE_FOURIER,
                "逆傅里叶变换：将频域信号 F(omega) 恢复为时域信号 f(t)（与正变换构成可逆对）", 1, PRESET_TYPE_FUNCTION,
                "f(x) = \\int_{-\\infty}^{\\infty} \\hat{f}(\\xi)\\,e^{2\\pi i x\\xi}\\,d\\xi", "O(N \\log N)", true,
                true, PRESET_TYPE_FUNCTION);
    /**
     * @brief 离散傅里叶变换（DFT）
     *
     * @details 对长度为 N 的离散序列计算其频谱。
     *          DFT 是数字信号处理的基础算法，
     *          直接计算复杂度为 O(N^2)。
     * @param 离散序列（PRESET_TYPE_SEQUENCE）
     * @return 频谱序列（PRESET_TYPE_SEQUENCE）
     * @math X_k = \sum_{n=0}^{N-1} x_n \cdot e^{-2\pi i kn/N}, \quad k = 0, 1, \dots, N-1
     * @complexity O(N^2)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_IT(PRESET_IT_DFT, "离散傅里叶变换（DFT）：对长度为N的离散序列 x[n] 计算频谱 X[k] = sum x[n]·exp(-2πikn/N)",
                1, PRESET_TYPE_SEQUENCE,
                "X_k = \\sum_{n=0}^{N-1} x_n \\cdot e^{-2\\pi i kn/N}, \\quad k = 0,\\dots,N-1", "O(N^2)", true, true,
                PRESET_TYPE_SEQUENCE);
    /**
     * @brief 快速傅里叶变换（FFT）
     *
     * @details 使用 Cooley-Tukey 算法将 DFT 的计算复杂度由 O(N^2)
     *          降低到 O(N log N)。FFT 是现代数字信号处理中最广泛使用的算法之一。
     * @param 离散序列（PRESET_TYPE_SEQUENCE）
     * @return 频谱序列（PRESET_TYPE_SEQUENCE）
     * @math X_k = \sum_{n=0}^{N-1} x_n \cdot \omega_N^{kn}, \quad \omega_N = e^{-2\pi i/N} \text{（分治算法加速）}
     * @complexity O(N log N)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_IT(PRESET_IT_FFT, "快速傅里叶变换（FFT）：用 Cooley-Tukey 分治算法将 DFT 从 O(N^2) 加速至 O(N log N)", 1,
                PRESET_TYPE_SEQUENCE,
                "X_k = \\sum_{n=0}^{N-1} x_n \\cdot \\omega_N^{kn}, \\quad "
                "\\omega_N = e^{-2\\pi i/N}",
                "O(N \\log N)", true, true, PRESET_TYPE_SEQUENCE);
    /**
     * @brief 傅里叶余弦/正弦变换
     *
     * @details 对于实偶/实奇函数分别使用傅里叶余弦变换（DCT）
     *          和傅里叶正弦变换（DST）。在图像压缩（JPEG 使用 DCT）
     *          和偏微分方程求解中有重要应用。
     * @param 实函数（PRESET_TYPE_FUNCTION）和奇偶性标志（PRESET_TYPE_BOOLEAN）
     * @return 实频谱（PRESET_TYPE_FUNCTION）
     * @math \text{DCT: } C_k = \sum_{n=0}^{N-1} x_n \cos\left[\frac{\pi}{N}(n+\frac{1}{2})k\right], \quad \text{DST: } S_k = \sum_{n=0}^{N-1} x_n \sin\left[\frac{\pi}{N}(n+1)(k+1)\right]
     * @complexity O(N)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_IT(PRESET_IT_FOURIER_SINE_COSINE,
                "傅里叶余弦/正弦变换：将实偶/实奇函数进行对称展开，用于 JPEG 压缩和 PDE 求解", 2, PRESET_TYPE_FUNCTION,
                "C_k = \\sum_{n=0}^{N-1} x_n \\cos\\left[\\frac{\\pi}{N}"
                "\\left(n+\\frac{1}{2}\\right)k\\right], \\quad "
                "S_k = \\sum_{n=0}^{N-1} x_n \\sin\\left[\\frac{\\pi}{N}"
                "(n+1)(k+1)\\right]",
                "O(N)", true, true, PRESET_TYPE_FUNCTION, PRESET_TYPE_BOOLEAN);
    /* ============================================================
     * 第二部分：拉普拉斯变换（5个）
     * ============================================================ */

    /**
     * @brief 拉普拉斯变换
     *
     * @details 将因果（t >= 0）时域函数变换到复频域（s 域）。
     *          拉普拉斯变换是经典控制理论和电路分析的核心工具，
     *          能将微分方程转化为代数方程。
     * @param 时域函数（PRESET_TYPE_FUNCTION）
     * @return s 域函数（PRESET_TYPE_FUNCTION）
     * @math F(s) = \mathcal{L}\{f(t)\} = \int_0^{\infty} f(t) e^{-st}\,dt, \quad s \in \mathbb{C}
     * @complexity O(N)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_IT(PRESET_IT_LAPLACE_TRANSFORM,
                "拉普拉斯变换：将因果信号 f(t), t>=0 变换到 s 域 F(s) = ∫_{0}^{∞} f(t) e^{-st} dt", 1,
                PRESET_TYPE_FUNCTION,
                "F(s) = \\mathcal{L}\\{f(t)\\} = "
                "\\int_0^{\\infty} f(t) e^{-st}\\,dt, \\quad s \\in \\mathbb{C}",
                "O(N)", true, true, PRESET_TYPE_FUNCTION);
    /**
     * @brief 逆拉普拉斯变换
     *
     * @details 通过 Bromwich 围道积分将 s 域函数恢复为时域信号。
     *          常用部分分式展开和拉普拉斯变换表配合使用。
     * @param s 域函数（PRESET_TYPE_FUNCTION）
     * @return 时域函数（PRESET_TYPE_FUNCTION）
     * @math f(t) = \frac{1}{2\pi i} \int_{\gamma-i\infty}^{\gamma+i\infty} F(s) e^{st}\,ds
     * @complexity O(N)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_IT(PRESET_IT_INVERSE_LAPLACE, "逆拉普拉斯变换：通过 Bromwich 积分将 s 域函数恢复为时域信号 f(t)", 1,
                PRESET_TYPE_FUNCTION,
                "f(t) = \\mathcal{L}^{-1}\\{F(s)\\} = "
                "\\frac{1}{2\\pi i} \\int_{\\gamma-i\\infty}^{\\gamma+i\\infty} "
                "F(s) e^{st}\\,ds",
                "O(N)", true, true, PRESET_TYPE_FUNCTION);
    /**
     * @brief 卷积定理
     *
     * @details 拉普拉斯变换下的卷积定理：时域卷积对应 s 域乘积。
     *          该性质允许将复杂的卷积计算转化为简单的乘法，
     *          是控制理论中传递函数分析的基础。
     * @param 两个 s 域函数（PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION）
     * @return 乘积结果（PRESET_TYPE_FUNCTION）
     * @math \mathcal{L}\{(f*g)(t)\} = F(s) \cdot G(s)
     * @complexity O(N)
     * @constructive 否
     * @reversible 否
     */
    REGISTER_IT(PRESET_IT_CONVOLUTION_THEOREM, "卷积定理：时域卷积对应 s 域乘积 L{f*g} = F(s)G(s)，简化系统分析", 2,
                PRESET_TYPE_FUNCTION, "\\mathcal{L}\\{(f*g)(t)\\} = F(s) \\cdot G(s)", "O(N)", false, false,
                PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION);
    /**
     * @brief 传递函数
     *
     * @details 由线性时不变系统的微分方程推导传递函数 H(s) =
     *          Y(s)/X(s)。传递函数完整描述了系统的输入-输出关系，
     *          是经典控制理论的基石。
     * @param 系统微分方程系数（PRESET_TYPE_LIST, PRESET_TYPE_LIST）
     * @return 传递函数（PRESET_TYPE_FUNCTION）
     * @math H(s) = \frac{Y(s)}{X(s)} = \frac{b_m s^m + \cdots + b_0}{a_n s^n + \cdots + a_0}
     * @complexity O(1)
     * @constructive 否
     * @reversible 否
     */
    REGISTER_IT(PRESET_IT_TRANSFER_FUNCTION, "传递函数：由 LTI 系统的微分方程推导 H(s) = Y(s)/X(s)，描述输入-输出关系",
                2, PRESET_TYPE_FUNCTION,
                "H(s) = \\frac{Y(s)}{X(s)} = "
                "\\frac{b_m s^m + \\cdots + b_1 s + b_0}"
                "{a_n s^n + \\cdots + a_1 s + a_0}",
                "O(1)", false, false, PRESET_TYPE_LIST, PRESET_TYPE_LIST);
    /**
     * @brief 初值定理/终值定理
     *
     * @details 利用 s 域函数直接获取时域信号的初始值和稳态终值。
     *          初值定理：f(0+) = lim_{s->∞} sF(s)
     *          终值定理：f(∞) = lim_{s->0} sF(s)（需极点均在左半平面）
     * @param s 域函数（PRESET_TYPE_FUNCTION）
     * @return 初值和终值对（PRESET_TYPE_TUPLE）
     * @math f(0^+) = \lim_{s\to\infty} sF(s), \quad f(\infty) = \lim_{s\to 0} sF(s)
     * @complexity O(1)
     * @constructive 否
     * @reversible 否
     */
    REGISTER_IT(PRESET_IT_INITIAL_FINAL_THEOREM, "初值定理/终值定理：由 s 域 F(s) 直接求 f(0+) 和 f(∞)，无需逆变换", 1,
                PRESET_TYPE_TUPLE,
                "f(0^+) = \\lim_{s\\to\\infty} sF(s), \\quad "
                "f(\\infty) = \\lim_{s\\to 0} sF(s) \\; "
                "\\text{（极点均在 LHP 条件）}",
                "O(1)", false, false, PRESET_TYPE_FUNCTION);
    /* ============================================================
     * 第三部分：Z变换（3个）
     * ============================================================ */

    /**
     * @brief Z变换
     *
     * @details 将离散时间序列变换到 z 域（复频域）。
     *          Z变换是离散时间信号处理的拉普拉斯变换对应物，
     *          在数字滤波器和离散控制系统分析中必不可少。
     * @param 离散序列（PRESET_TYPE_SEQUENCE）
     * @return z 域函数（PRESET_TYPE_FUNCTION）
     * @math X(z) = \mathcal{Z}\{x[n]\} = \sum_{n=0}^{\infty} x[n]\,z^{-n}
     * @complexity O(N)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_IT(PRESET_IT_Z_TRANSFORM, "Z变换：将离散序列 x[n] 变换到 z 域 X(z) = sum x[n]·z^{-n}", 1,
                PRESET_TYPE_FUNCTION,
                "X(z) = \\mathcal{Z}\\{x[n]\\} = "
                "\\sum_{n=0}^{\\infty} x[n]\\,z^{-n}",
                "O(N)", true, true, PRESET_TYPE_SEQUENCE);
    /**
     * @brief 逆Z变换
     *
     * @details 由 z 域函数恢复离散时间序列。
     *          常用方法包括留数法、幂级数展开法和部分分式展开法。
     * @param z 域函数（PRESET_TYPE_FUNCTION）
     * @return 离散序列（PRESET_TYPE_SEQUENCE）
     * @math x[n] = \frac{1}{2\pi i} \oint_C X(z) z^{n-1}\,dz
     * @complexity O(N)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_IT(PRESET_IT_INVERSE_Z, "逆Z变换：将 z 域函数 X(z) 恢复为离散序列 x[n]（留数法/幂级数展开法）", 1,
                PRESET_TYPE_SEQUENCE, "x[n] = \\frac{1}{2\\pi i} \\oint_C X(z) z^{n-1}\\,dz", "O(N)", true, true,
                PRESET_TYPE_FUNCTION);
    /**
     * @brief Z域卷积
     *
     * @details 离散序列卷积的 Z 变换等于各自 Z 变换的乘积。
     *          该性质使得数字滤波器的频域设计变得极为简便。
     * @param 两个离散序列（PRESET_TYPE_SEQUENCE, PRESET_TYPE_SEQUENCE）
     * @return 卷积序列（PRESET_TYPE_SEQUENCE）
     * @math \mathcal{Z}\{x[n]*y[n]\} = X(z) \cdot Y(z), \quad x*y[n] = \sum_{k} x[k]y[n-k]
     * @complexity O(N^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_IT(PRESET_IT_Z_CONVOLUTION, "Z域卷积：离散序列卷积的 Z 变换等于各自 Z 变换的乘积 Z{x*y} = X(z)Y(z)", 2,
                PRESET_TYPE_SEQUENCE,
                "\\mathcal{Z}\\{x[n] * y[n]\\} = X(z) \\cdot Y(z), \\quad "
                "(x*y)[n] = \\sum_{k} x[k]\\,y[n-k]",
                "O(N^2)", true, false, PRESET_TYPE_SEQUENCE, PRESET_TYPE_SEQUENCE);
    /* ============================================================
     * 第四部分：其他变换（3个）
     * ============================================================ */

    /**
     * @brief 梅林变换
     *
     * @details 梅林变换将函数 f(x) 映射为其"乘性"频谱。
     *          梅林变换可视为乘性群上的傅里叶变换，
     *          在解析数论、特殊函数理论和尺度不变性分析中有重要应用。
     * @param 函数（PRESET_TYPE_FUNCTION）
     * @return 梅林变换结果（PRESET_TYPE_FUNCTION）
     * @math M\{f\}(s) = \int_0^{\infty} x^{s-1} f(x)\,dx
     * @complexity O(N)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_IT(PRESET_IT_MELLIN_TRANSFORM,
                "梅林变换：计算乘性频谱 M{f}(s) = ∫_{0}^{∞} x^{s-1} f(x) dx，乘性群上傅里叶变换", 1,
                PRESET_TYPE_FUNCTION, "\\mathcal{M}\\{f\\}(s) = \\int_0^{\\infty} x^{s-1} f(x)\\,dx", "O(N)", true,
                true, PRESET_TYPE_FUNCTION);
    /**
     * @brief 希尔伯特变换
     *
     * @details 希尔伯特变换是对函数施加 90 度相移的积分变换。
     *          与原始信号结合可构造解析信号（analytic signal），
     *          在通信理论（单边带调制）和信号包络检测中广泛应用。
     * @param 实函数（PRESET_TYPE_FUNCTION）
     * @return 希尔伯特变换结果（PRESET_TYPE_FUNCTION）
     * @math H\{f\}(t) = \frac{1}{\pi} \operatorname{p.v.} \int_{-\infty}^{\infty} \frac{f(\tau)}{t-\tau}\,d\tau
     * @complexity O(N log N)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_IT(PRESET_IT_HILBERT_TRANSFORM,
                "希尔伯特变换：90度相移积分变换 H{f}(t) = (1/pi)·p.v.∫ f(tau)/(t-tau) dtau", 1, PRESET_TYPE_FUNCTION,
                "\\mathcal{H}\\{f\\}(t) = "
                "\\frac{1}{\\pi} \\operatorname{p.v.}"
                "\\int_{-\\infty}^{\\infty} \\frac{f(\\tau)}{t-\\tau}\\,d\\tau",
                "O(N \\log N)", true, true, PRESET_TYPE_FUNCTION);
    /**
     * @brief 小波变换
     *
     * @details 连续小波变换（CWT）提供信号的多分辨率时频分析。
     *          通过伸缩和平移母小波函数，小波变换能同时
     *          定位信号的时域和频域特征，优于传统傅里叶分析。
     * @param 信号函数（PRESET_TYPE_FUNCTION）和小波基（PRESET_TYPE_FUNCTION）
     * @return 小波系数（PRESET_TYPE_MATRIX）
     * @math W_f(a,b) = \frac{1}{\sqrt{|a|}} \int_{-\infty}^{\infty} f(t) \overline{\psi\left(\frac{t-b}{a}\right)}\,dt
     * @complexity O(N^2)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_IT(PRESET_IT_WAVELET_TRANSFORM, "小波变换（CWT）：通过伸缩和平移母小波进行多分辨率时频分析 W_f(a,b)", 2,
                PRESET_TYPE_MATRIX,
                "W_f(a,b) = \\frac{1}{\\sqrt{|a|}} "
                "\\int_{-\\infty}^{\\infty} f(t)\\,"
                "\\overline{\\psi\\!\\left(\\frac{t-b}{a}\\right)}\\,dt",
                "O(N^2)", true, true, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION);
    /* ============================================================
     * 第五部分：卷积运算（2个）
     * ============================================================ */

    /**
     * @brief 连续卷积
     *
     * @details 计算两个连续函数的卷积积分。
     *          卷积是线性时不变系统分析的核心运算，
     *          系统的输出等于输入与冲激响应的卷积。
     * @param 两个连续函数（PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION）
     * @return 卷积结果函数（PRESET_TYPE_FUNCTION）
     * @math (f * g)(t) = \int_{-\infty}^{\infty} f(\tau)\,g(t-\tau)\,d\tau
     * @complexity O(N^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_IT(PRESET_IT_CONTINUOUS_CONVOLUTION, "连续卷积：计算两个连续函数的卷积 (f*g)(t) = ∫ f(tau)·g(t-tau) dtau",
                2, PRESET_TYPE_FUNCTION,
                "(f * g)(t) = \\int_{-\\infty}^{\\infty} "
                "f(\\tau)\\,g(t-\\tau)\\,d\\tau",
                "O(N^2)", true, false, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION);
    /**
     * @brief 离散/循环卷积
     *
     * @details 计算两个有限长度序列的卷积（线性卷积或循环卷积）。
     *          采用 FFT 加速可实现 O(N log N) 的循环卷积。
     *          循环卷积与线性卷积通过补零操作相关联。
     * @param 两个离散序列（PRESET_TYPE_SEQUENCE, PRESET_TYPE_SEQUENCE）和模式标志（PRESET_TYPE_BOOLEAN）
     * @return 卷积结果序列（PRESET_TYPE_SEQUENCE）
     * @math (x * y)[n] = \sum_{k=0}^{N-1} x[k]\,y[(n-k)\bmod N] \quad \text{（循环卷积）}
     * @complexity O(N log N)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_IT(PRESET_IT_DISCRETE_CONVOLUTION, "离散/循环卷积：计算有限序列的线性或循环卷积（FFT 加速至 O(N log N)）",
                3, PRESET_TYPE_SEQUENCE,
                "(x * y)[n] = \\sum_{k=0}^{N-1} x[k]\\,y[(n-k)\\bmod N] \\quad \\text{（循环）}", "O(N \\log N)", true,
                false, PRESET_TYPE_SEQUENCE, PRESET_TYPE_SEQUENCE, PRESET_TYPE_BOOLEAN);
    /* 返回是否所有预设都注册成功 */
    if (success_count == IT_PRESET_COUNT) {
        /* lv00_log_info("积分变换模块注册成功：%d/%d 个预设", success_count, IT_PRESET_COUNT) */
        return true;
    }

    /* lv00_log_info("积分变换模块注册部分失败：%d/%d 个预设", success_count, IT_PRESET_COUNT) */
    return false;
}

/**
 * @brief 获取积分变换预设函数块数量
 *
 * @return int 积分变换模块预设函数块总数（19）
 */
int preset_integral_transforms_count(void) {
    return IT_PRESET_COUNT;
}

/**
 * @brief 获取积分变换预设的类别
 *
 * @return PresetCategory 预设类别（PRESET_CATEGORY_ANALYSIS）
 */
PresetCategory preset_integral_transforms_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}

/**
 * @brief 获取积分变换预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_integral_transforms_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv00_malloc(IT_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        /* 傅里叶分析 */
        PRESET_IT_FOURIER_SERIES,
        PRESET_IT_FOURIER_TRANSFORM,
        PRESET_IT_INVERSE_FOURIER,
        PRESET_IT_DFT,
        PRESET_IT_FFT,
        PRESET_IT_FOURIER_SINE_COSINE,
        /* 拉普拉斯变换 */
        PRESET_IT_LAPLACE_TRANSFORM,
        PRESET_IT_INVERSE_LAPLACE,
        PRESET_IT_CONVOLUTION_THEOREM,
        PRESET_IT_TRANSFER_FUNCTION,
        PRESET_IT_INITIAL_FINAL_THEOREM,
        /* Z变换 */
        PRESET_IT_Z_TRANSFORM,
        PRESET_IT_INVERSE_Z,
        PRESET_IT_Z_CONVOLUTION,
        /* 其他变换 */
        PRESET_IT_MELLIN_TRANSFORM,
        PRESET_IT_HILBERT_TRANSFORM,
        PRESET_IT_WAVELET_TRANSFORM,
        /* 卷积运算 */
        PRESET_IT_CONTINUOUS_CONVOLUTION,
        PRESET_IT_DISCRETE_CONVOLUTION,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
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
