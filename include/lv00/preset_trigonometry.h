/**
 * @file preset_trigonometry.h
 * @brief 三角函数预设函数块
 *
 * 提供理论数学研究中常用的三角函数运算预设函数块。
 * 涵盖基本三角函数、反三角函数、双曲函数、三角恒等式、
 * 三角方程求解以及三角级数展开。
 *
 * 包含的预设函数块：
 * - 基本三角函数（6个）：sin, cos, tan, cot, sec, csc
 * - 反三角函数（4个）：arcsin, arccos, arctan, arccot
 * - 双曲函数（3个）：sinh, cosh, tanh
 * - 恒等式（4个）：和差化积、积化和差、倍角公式、半角公式
 * - 三角方程求解（1个）
 * - 三角级数展开（2个）
 *
 * @module Trigonometry
 * @category PRESET_CATEGORY_ALGEBRAIC
 */

#ifndef LV00_PRESET_TRIGONOMETRY_H
#define LV00_PRESET_TRIGONOMETRY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 基本三角函数 ==================== */

/**
 * @brief 正弦函数 sin(x)
 *
 * 数学定义：给定角度 $x$，计算其正弦值 $\sin x$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 角度值（弧度制）
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 正弦值，范围 $[-1, 1]$
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIG_SIN "trig_sin"

/**
 * @brief 余弦函数 cos(x)
 *
 * 数学定义：给定角度 $x$，计算其余弦值 $\cos x$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 角度值（弧度制）
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 余弦值，范围 $[-1, 1]$
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIG_COS "trig_cos"

/**
 * @brief 正切函数 tan(x)
 *
 * 数学定义：给定角度 $x$，计算其正切值 $\tan x = \frac{\sin x}{\cos x}$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 角度值（弧度制）
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 正切值
 *
 * 复杂度：O(1)
 * 注意：当 $\cos x = 0$ 时无定义
 */
#define PRESET_TRIG_TAN "trig_tan"

/**
 * @brief 余切函数 cot(x)
 *
 * 数学定义：给定角度 $x$，计算其余切值 $\cot x = \frac{\cos x}{\sin x} = \frac{1}{\tan x}$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 角度值（弧度制）
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 余切值
 *
 * 复杂度：O(1)
 * 注意：当 $\sin x = 0$ 时无定义
 */
#define PRESET_TRIG_COT "trig_cot"

/**
 * @brief 正割函数 sec(x)
 *
 * 数学定义：给定角度 $x$，计算其正割值 $\sec x = \frac{1}{\cos x}$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 角度值（弧度制）
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 正割值
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIG_SEC "trig_sec"

/**
 * @brief 余割函数 csc(x)
 *
 * 数学定义：给定角度 $x$，计算其余割值 $\csc x = \frac{1}{\sin x}$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 角度值（弧度制）
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 余割值
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIG_CSC "trig_csc"

/* ==================== 反三角函数 ==================== */

/**
 * @brief 反正弦函数 arcsin(x)
 *
 * 数学定义：给定 $x \in [-1, 1]$，计算 $\arcsin x$，满足 $\sin(\arcsin x) = x$，
 * 值域为 $[-\pi/2, \pi/2]$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 正弦值，范围 $[-1, 1]$
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 反正弦角度（弧度）
 *
 * 复杂度：O(1)
 * 可逆：是（与 sin 互为反函数）
 */
#define PRESET_TRIG_ARCSIN "trig_arcsin"

/**
 * @brief 反余弦函数 arccos(x)
 *
 * 数学定义：给定 $x \in [-1, 1]$，计算 $\arccos x$，值域为 $[0, \pi]$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 余弦值，范围 $[-1, 1]$
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 反余弦角度（弧度）
 *
 * 复杂度：O(1)
 * 可逆：是（与 cos 互为反函数）
 */
#define PRESET_TRIG_ARCCOS "trig_arccos"

/**
 * @brief 反正切函数 arctan(x)
 *
 * 数学定义：给定 $x \in \mathbb{R}$，计算 $\arctan x$，值域为 $(-\pi/2, \pi/2)$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 正切值
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 反正切角度（弧度）
 *
 * 复杂度：O(1)
 * 可逆：是（与 tan 互为反函数）
 */
#define PRESET_TRIG_ARCTAN "trig_arctan"

/**
 * @brief 反余切函数 arccot(x)
 *
 * 数学定义：给定 $x \in \mathbb{R}$，计算 $\operatorname{arccot} x$，值域为 $(0, \pi)$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 余切值
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 反余切角度（弧度）
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIG_ARCCOT "trig_arccot"

/* ==================== 双曲函数 ==================== */

/**
 * @brief 双曲正弦 sinh(x)
 *
 * 数学定义：$\sinh x = \frac{e^x - e^{-x}}{2}$，奇函数，定义域和值域均为 $\mathbb{R}$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 双曲正弦值
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIG_SINH "trig_sinh"

/**
 * @brief 双曲余弦 cosh(x)
 *
 * 数学定义：$\cosh x = \frac{e^x + e^{-x}}{2}$，偶函数，值域为 $[1, +\infty)$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 双曲余弦值
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIG_COSH "trig_cosh"

/**
 * @brief 双曲正切 tanh(x)
 *
 * 数学定义：$\tanh x = \frac{\sinh x}{\cosh x} = \frac{e^x - e^{-x}}{e^x + e^{-x}}$，
 * 值域为 $(-1, 1)$
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 双曲正切值
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIG_TANH "trig_tanh"

/* ==================== 三角恒等式 ==================== */

/**
 * @brief 和差化积公式
 *
 * 数学定义：
 * \begin{aligned}
 * \sin A + \sin B &= 2\sin\frac{A+B}{2}\cos\frac{A-B}{2} \\
 * \sin A - \sin B &= 2\cos\frac{A+B}{2}\sin\frac{A-B}{2} \\
 * \cos A + \cos B &= 2\cos\frac{A+B}{2}\cos\frac{A-B}{2} \\
 * \cos A - \cos B &= -2\sin\frac{A+B}{2}\sin\frac{A-B}{2}
 * \end{aligned}
 *
 * 输入：
 *   - A: 标量 (PRESET_TYPE_SCALAR) - 第一个角度
 *   - B: 标量 (PRESET_TYPE_SCALAR) - 第二个角度
 *   - formula_type: 整数 (PRESET_TYPE_INTEGER) - 公式类型选择 (0:sin+sin, 1:sin-sin, 2:cos+cos, 3:cos-cos)
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR)
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIG_SUM_TO_PRODUCT "trig_sum_to_product"

/**
 * @brief 积化和差公式
 *
 * 数学定义：
 * \begin{aligned}
 * \sin A \cos B &= \frac{1}{2}[\sin(A+B) + \sin(A-B)] \\
 * \cos A \sin B &= \frac{1}{2}[\sin(A+B) - \sin(A-B)] \\
 * \cos A \cos B &= \frac{1}{2}[\cos(A+B) + \cos(A-B)] \\
 * \sin A \sin B &= -\frac{1}{2}[\cos(A+B) - \cos(A-B)]
 * \end{aligned}
 *
 * 输入：
 *   - A: 标量 (PRESET_TYPE_SCALAR) - 第一个角度
 *   - B: 标量 (PRESET_TYPE_SCALAR) - 第二个角度
 *   - formula_type: 整数 (PRESET_TYPE_INTEGER) - 公式类型选择
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR)
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIG_PRODUCT_TO_SUM "trig_product_to_sum"

/**
 * @brief 倍角公式
 *
 * 数学定义：
 * \begin{aligned}
 * \sin 2\theta &= 2\sin\theta\cos\theta \\
 * \cos 2\theta &= \cos^2\theta - \sin^2\theta = 2\cos^2\theta - 1 = 1 - 2\sin^2\theta \\
 * \tan 2\theta &= \frac{2\tan\theta}{1 - \tan^2\theta}
 * \end{aligned}
 *
 * 输入：
 *   - theta: 标量 (PRESET_TYPE_SCALAR) - 原始角度
 *   - func_type: 整数 (PRESET_TYPE_INTEGER) - 函数类型 (0:sin, 1:cos, 2:tan)
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 倍角函数值
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIG_DOUBLE_ANGLE "trig_double_angle"

/**
 * @brief 半角公式
 *
 * 数学定义：
 * \begin{aligned}
 * \sin\frac{\theta}{2} &= \pm\sqrt{\frac{1 - \cos\theta}{2}} \\
 * \cos\frac{\theta}{2} &= \pm\sqrt{\frac{1 + \cos\theta}{2}} \\
 * \tan\frac{\theta}{2} &= \frac{\sin\theta}{1 + \cos\theta} = \frac{1 - \cos\theta}{\sin\theta}
 * \end{aligned}
 *
 * 输入：
 *   - theta: 标量 (PRESET_TYPE_SCALAR) - 原始角度
 *   - func_type: 整数 (PRESET_TYPE_INTEGER) - 函数类型 (0:sin, 1:cos, 2:tan)
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - 半角函数值
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIG_HALF_ANGLE "trig_half_angle"

/* ==================== 三角方程求解 ==================== */

/**
 * @brief 标准三角方程求解
 *
 * 数学定义：求解形如 $\sin x = a$、$\cos x = a$ 或 $\tan x = a$ 的基本三角方程的通解：
 * \begin{aligned}
 * \sin x = a &\Rightarrow x = (-1)^k \arcsin a + k\pi,\ k \in \mathbb{Z} \\
 * \cos x = a &\Rightarrow x = \pm \arccos a + 2k\pi,\ k \in \mathbb{Z} \\
 * \tan x = a &\Rightarrow x = \arctan a + k\pi,\ k \in \mathbb{Z}
 * \end{aligned}
 *
 * 输入：
 *   - a: 标量 (PRESET_TYPE_SCALAR) - 方程右端的值
 *   - eq_type: 整数 (PRESET_TYPE_INTEGER) - 方程类型 (0:sin, 1:cos, 2:tan)
 * 输出：
 *   - 列表 (PRESET_TYPE_LIST) - 通解参数列表 $[x_0, T]$，通解为 $x = x_0 + kT$
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIG_EQUATION_SOLVE "trig_equation_solve"

/* ==================== 三角级数展开 ==================== */

/**
 * @brief 三角函数的Taylor级数展开
 *
 * 数学定义：
 * \begin{aligned}
 * \sin x &= \sum_{n=0}^{\infty} (-1)^n \frac{x^{2n+1}}{(2n+1)!}
 *        &= x - \frac{x^3}{3!} + \frac{x^5}{5!} - \cdots \\
 * \cos x &= \sum_{n=0}^{\infty} (-1)^n \frac{x^{2n}}{(2n)!}
 *        &= 1 - \frac{x^2}{2!} + \frac{x^4}{4!} - \cdots
 * \end{aligned}
 *
 * 输入：
 *   - x: 标量 (PRESET_TYPE_SCALAR) - 展开点
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 展开项数
 *   - func_type: 整数 (PRESET_TYPE_INTEGER) - 函数类型 (0:sin, 1:cos)
 * 输出：
 *   - 标量 (PRESET_TYPE_SCALAR) - n阶Taylor近似值
 *
 * 复杂度：O(n)
 */
#define PRESET_TRIG_SERIES_EXPAND "trig_series_expand"

/**
 * @brief 傅里叶三角级数展开
 *
 * 数学定义：对于周期为 $T$ 的函数 $f(t)$，计算其傅里叶系数 $a_n$ 和 $b_n$：
 * \begin{aligned}
 * f(t) &= \frac{a_0}{2} + \sum_{n=1}^{\infty} \left(a_n\cos\frac{2\pi nt}{T} + b_n\sin\frac{2\pi nt}{T}\right) \\
 * a_n &= \frac{2}{T}\int_0^T f(t)\cos\frac{2\pi nt}{T}\,dt \\
 * b_n &= \frac{2}{T}\int_0^T f(t)\sin\frac{2\pi nt}{T}\,dt
 * \end{aligned}
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 周期函数
 *   - T: 标量 (PRESET_TYPE_SCALAR) - 周期
 *   - N: 整数 (PRESET_TYPE_INTEGER) - 谐波次数上限
 * 输出：
 *   - 列表 (PRESET_TYPE_LIST) - 傅里叶系数列表 $(a_0, a_1, b_1, \ldots, a_N, b_N)$
 *
 * 复杂度：O(N)
 */
#define PRESET_TRIG_FOURIER_SERIES "trig_fourier_series"

/* ==================== 模块初始化 ==================== */

/**
 * @brief 注册三角函数预设函数块
 *
 * 此函数由 preset_blocks_init() 自动调用，用户无需直接调用。
 *
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_trigonometry_register(void);

/**
 * @brief 获取三角函数模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_trigonometry_count(void);

/**
 * @brief 获取三角函数模块的预设类别
 *
 * @return 预设类别
 */
PresetCategory preset_trigonometry_category(void);

/**
 * @brief 获取三角函数模块的所有预设名称
 *
 * @param out_names 输出名称数组（调用者负责释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_trigonometry_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_TRIGONOMETRY_H */
