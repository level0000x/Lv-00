/**
 * @file preset_complex_analysis.h
 * @brief 复分析预设函数块
 *
 * 提供理论数学研究中常用的复分析运算预设函数块，包括：
 * - 复数运算：加法、乘法、除法、共轭、模、辐角、极坐标形式、直角坐标形式、n次方根
 * - 复变函数：复导数、复积分、柯西积分公式、留数计算、洛朗级数展开
 * - 解析性：解析性判定（Cauchy-Riemann条件）、调和函数判定、调和共轭、整函数判定、亚纯函数判定
 * - 保角映射：保角映射判定、Möbius变换、指数映射、对数分支
 * - 级数与积分定理：复泰勒级数、留数定理应用、柯西估计、Liouville定理判定、最大模原理判定、
 *   Rouché定理判定、辐角原理
 *
 * @module ComplexAnalysis
 * @category PRESET_EXT_ANALYSIS
 * @version 1.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_COMPLEX_ANALYSIS_H
#define LV00_PRESET_COMPLEX_ANALYSIS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 复数运算 -------------------- */

/**
 * @brief 复数加法
 *
 * 数学定义：给定 $z_1 = a + bi$ 和 $z_2 = c + di$，
 * 计算 $z_1 + z_2 = (a+c) + (b+d)i$
 *
 * 输入：
 *   - z1: 标量 (PRESET_TYPE_SCALAR) - 复数1
 *   - z2: 标量 (PRESET_TYPE_SCALAR) - 复数2
 * 输出：
 *   - z: 标量 (PRESET_TYPE_SCALAR) - 和
 *
 * 复杂度：O(1)
 */
#define PRESET_COMPLEX_ADD "complex_add"

/**
 * @brief 复数乘法
 *
 * 数学定义：给定 $z_1 = a + bi$ 和 $z_2 = c + di$，
 * 计算 $z_1 z_2 = (ac - bd) + (ad + bc)i$
 *
 * 输入：
 *   - z1: 标量 (PRESET_TYPE_SCALAR) - 复数1
 *   - z2: 标量 (PRESET_TYPE_SCALAR) - 复数2
 * 输出：
 *   - z: 标量 (PRESET_TYPE_SCALAR) - 积
 *
 * 复杂度：O(1)
 */
#define PRESET_COMPLEX_MULTIPLY "complex_multiply"

/**
 * @brief 复数除法
 *
 * 数学定义：给定 $z_1 = a + bi$ 和 $z_2 = c + di$（$z_2 \neq 0$），
 * 计算 $\frac{z_1}{z_2} = \frac{a + bi}{c + di} = \frac{(ac + bd) + (bc - ad)i}{c^2 + d^2}$
 *
 * 输入：
 *   - z1: 标量 (PRESET_TYPE_SCALAR) - 被除数
 *   - z2: 标量 (PRESET_TYPE_SCALAR) - 除数
 * 输出：
 *   - z: 标量 (PRESET_TYPE_SCALAR) - 商
 *
 * 复杂度：O(1)
 * 前置条件：z2 != 0
 */
#define PRESET_COMPLEX_DIVIDE "complex_divide"

/**
 * @brief 共轭复数
 *
 * 数学定义：给定 $z = a + bi$，计算 $\overline{z} = a - bi$
 *
 * 输入：
 *   - z: 标量 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - conj(z): 标量 (PRESET_TYPE_SCALAR) - 共轭复数
 *
 * 复杂度：O(1)
 */
#define PRESET_COMPLEX_CONJUGATE "complex_conjugate"

/**
 * @brief 模
 *
 * 数学定义：给定 $z = a + bi$，计算 $|z| = \sqrt{a^2 + b^2}$
 *
 * 输入：
 *   - z: 标量 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - |z|: 标量 (PRESET_TYPE_SCALAR) - 模
 *
 * 复杂度：O(1)
 */
#define PRESET_COMPLEX_MODULUS "complex_modulus"

/**
 * @brief 辐角
 *
 * 数学定义：给定 $z = a + bi$（$z \neq 0$），计算 $\arg(z)$，
 * 满足 $z = |z| e^{i \arg(z)}$
 *
 * 输入：
 *   - z: 标量 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - arg(z): 标量 (PRESET_TYPE_SCALAR) - 辐角（主值）
 *
 * 复杂度：O(1)
 * 前置条件：z != 0
 */
#define PRESET_COMPLEX_ARGUMENT "complex_argument"

/**
 * @brief 极坐标形式
 *
 * 数学定义：给定 $z = a + bi$，转换为极坐标形式 $z = r(\cos\theta + i\sin\theta) = re^{i\theta}$
 *
 * 输入：
 *   - z: 标量 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - (r, θ): 元组 (PRESET_TYPE_TUPLE) - 模和辐角
 *
 * 复杂度：O(1)
 */
#define PRESET_COMPLEX_POLAR_FORM "complex_polar_form"

/**
 * @brief 直角坐标形式
 *
 * 数学定义：给定极坐标 $(r, \theta)$，转换为直角坐标形式 $z = r\cos\theta + ir\sin\theta$
 *
 * 输入：
 *   - r: 标量 (PRESET_TYPE_SCALAR) - 模
 *   - theta: 标量 (PRESET_TYPE_SCALAR) - 辐角
 * 输出：
 *   - z: 标量 (PRESET_TYPE_SCALAR) - 直角坐标形式
 *
 * 复杂度：O(1)
 */
#define PRESET_COMPLEX_RECTANGULAR_FORM "complex_rectangular_form"

/**
 * @brief n次方根
 *
 * 数学定义：给定 $z = re^{i\theta}$ 和正整数 $n$，
 * 计算 $z^{1/n} = r^{1/n} e^{i(\theta + 2k\pi)/n}$，$k = 0, 1, \ldots, n-1$
 *
 * 输入：
 *   - z: 标量 (PRESET_TYPE_SCALAR)
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 根的次数
 * 输出：
 *   - roots: 标量列表 (PRESET_TYPE_TUPLE) - n个方根
 *
 * 复杂度：O(n)
 * 前置条件：n >= 1
 */
#define PRESET_COMPLEX_NTH_ROOT "complex_nth_root"

/* -------------------- 复变函数 -------------------- */

/**
 * @brief 复导数
 *
 * 数学定义：给定复变函数 $f(z)$ 和点 $z_0$，计算
 * $f'(z_0) = \lim_{\Delta z \to 0} \frac{f(z_0 + \Delta z) - f(z_0)}{\Delta z}$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
 *   - z0: 标量 (PRESET_TYPE_SCALAR) - 求导点
 * 输出：
 *   - f'(z0): 标量 (PRESET_TYPE_SCALAR) - 导数值
 *
 * 复杂度：O(1)
 */
#define PRESET_COMPLEX_DERIVATIVE "complex_derivative"

/**
 * @brief 复积分（路径积分）
 *
 * 数学定义：给定复变函数 $f(z)$ 和路径 $\gamma$，计算
 * $\int_{\gamma} f(z) \, dz$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 被积函数
 *   - gamma: 路径 (PRESET_TYPE_PATH) - 积分路径
 * 输出：
 *   - integral: 标量 (PRESET_TYPE_SCALAR) - 积分值
 *
 * 复杂度：O(n)
 */
#define PRESET_COMPLEX_INTEGRAL "complex_integral"

/**
 * @brief 柯西积分公式
 *
 * 数学定义：若 $f$ 在简单闭曲线 $\gamma$ 及其内部解析，$z_0$ 在 $\gamma$ 内部，则
 * $f(z_0) = \frac{1}{2\pi i} \oint_{\gamma} \frac{f(z)}{z - z_0} \, dz$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 解析函数
 *   - z0: 标量 (PRESET_TYPE_SCALAR) - 内部点
 *   - gamma: 路径 (PRESET_TYPE_PATH) - 积分路径
 * 输出：
 *   - f(z0): 标量 (PRESET_TYPE_SCALAR) - 函数值
 *
 * 复杂度：O(n)
 * 前置条件：f 在 gamma 及其内部解析，z0 在 gamma 内部
 */
#define PRESET_CAUCHY_INTEGRAL_FORMULA "cauchy_integral_formula"

/**
 * @brief 留数计算
 *
 * 数学定义：给定函数 $f(z)$ 和孤立奇点 $z_0$，计算
 * $\text{Res}(f, z_0) = \frac{1}{2\pi i} \oint_{\gamma} f(z) \, dz$
 * （$\gamma$ 为围绕 $z_0$ 的正向简单闭曲线）
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
 *   - z0: 标量 (PRESET_TYPE_SCALAR) - 奇点
 * 输出：
 *   - Res(f, z0): 标量 (PRESET_TYPE_SCALAR) - 留数值
 *
 * 复杂度：O(1)
 */
#define PRESET_RESIDUE_COMPUTE "residue_compute"

/**
 * @brief 洛朗级数展开
 *
 * 数学定义：给定函数 $f(z)$ 和孤立奇点 $z_0$，在环形区域 $0 < |z - z_0| < R$ 内展开为
 * $f(z) = \sum_{n=-\infty}^{\infty} a_n (z - z_0)^n$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
 *   - z0: 标量 (PRESET_TYPE_SCALAR) - 展开中心
 * 输出：
 *   - coefficients: 标量列表 (PRESET_TYPE_TUPLE) - 洛朗级数系数
 *
 * 复杂度：O(n)
 */
#define PRESET_LAURENT_SERIES "laurent_series"

/* -------------------- 解析性 -------------------- */

/**
 * @brief 解析性判定（Cauchy-Riemann条件）
 *
 * 数学定义：给定 $f(z) = u(x,y) + iv(x,y)$，判定是否满足 Cauchy-Riemann 方程
 * $\frac{\partial u}{\partial x} = \frac{\partial v}{\partial y}$，
 * $\frac{\partial u}{\partial y} = -\frac{\partial v}{\partial x}$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
 *   - z0: 标量 (PRESET_TYPE_SCALAR) - 判定点
 * 输出：
 *   - is_holomorphic: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否解析
 *
 * 复杂度：O(1)
 */
#define PRESET_HOLOMORPHIC_TEST "holomorphic_test"

/**
 * @brief 调和函数判定
 *
 * 数学定义：给定实值函数 $u(x, y)$，判定是否满足拉普拉斯方程
 * $\Delta u = \frac{\partial^2 u}{\partial x^2} + \frac{\partial^2 u}{\partial y^2} = 0$
 *
 * 输入：
 *   - u: 函数 (PRESET_TYPE_FUNCTION) - 实值函数
 *   - z0: 标量 (PRESET_TYPE_SCALAR) - 判定点
 * 输出：
 *   - is_harmonic: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否调和
 *
 * 复杂度：O(1)
 */
#define PRESET_HARMONIC_TEST "harmonic_test"

/**
 * @brief 调和共轭
 *
 * 数学定义：给定调和函数 $u(x, y)$，求其调和共轭 $v(x, y)$，
 * 使得 $f(z) = u + iv$ 为解析函数
 *
 * 输入：
 *   - u: 函数 (PRESET_TYPE_FUNCTION) - 调和函数
 * 输出：
 *   - v: 函数 (PRESET_TYPE_FUNCTION) - 调和共轭
 *
 * 复杂度：O(1)
 */
#define PRESET_HARMONIC_CONJUGATE "harmonic_conjugate"

/**
 * @brief 整函数判定
 *
 * 数学定义：判定函数 $f(z)$ 是否在复平面 $\mathbb{C}$ 上处处解析
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
 * 输出：
 *   - is_entire: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否为整函数
 *
 * 复杂度：O(∞)
 */
#define PRESET_ENTIRE_FUNCTION_TEST "entire_function_test"

/**
 * @brief 亚纯函数判定
 *
 * 数学定义：判定函数 $f(z)$ 在区域 $\Omega$ 内是否为亚纯函数
 * （除孤立极点外处处解析）
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
 *   - region: 区域 (PRESET_TYPE_REGION) - 判定区域
 * 输出：
 *   - is_meromorphic: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否为亚纯函数
 *
 * 复杂度：O(∞)
 */
#define PRESET_MEROMORPHIC_TEST "meromorphic_test"

/* -------------------- 保角映射 -------------------- */

/**
 * @brief 保角映射判定
 *
 * 数学定义：给定解析函数 $f(z)$ 和点 $z_0$，判定 $f$ 在 $z_0$ 处是否为保角映射
 * （$f'(z_0) \neq 0$）
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
 *   - z0: 标量 (PRESET_TYPE_SCALAR) - 判定点
 * 输出：
 *   - is_conformal: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否为保角映射
 *
 * 复杂度：O(1)
 */
#define PRESET_CONFORMAL_MAP_TEST "conformal_map_test"

/**
 * @brief Möbius变换
 *
 * 数学定义：给定 $a, b, c, d \in \mathbb{C}$（$ad - bc \neq 0$），计算
 * $T(z) = \frac{az + b}{cz + d}$
 *
 * 输入：
 *   - z: 标量 (PRESET_TYPE_SCALAR) - 自变量
 *   - a: 标量 (PRESET_TYPE_SCALAR)
 *   - b: 标量 (PRESET_TYPE_SCALAR)
 *   - c: 标量 (PRESET_TYPE_SCALAR)
 *   - d: 标量 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - T(z): 标量 (PRESET_TYPE_SCALAR) - 变换结果
 *
 * 复杂度：O(1)
 * 前置条件：ad - bc != 0
 */
#define PRESET_MOBIUS_TRANSFORM "mobius_transform"

/**
 * @brief 指数映射
 *
 * 数学定义：给定 $z = x + iy$，计算 $e^z = e^x (\cos y + i \sin y)$
 *
 * 输入：
 *   - z: 标量 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - e^z: 标量 (PRESET_TYPE_SCALAR) - 指数值
 *
 * 复杂度：O(1)
 */
#define PRESET_EXPONENTIAL_MAP "exponential_map"

/**
 * @brief 对数分支
 *
 * 数学定义：给定 $z = re^{i\theta}$（$z \neq 0$），计算对数的主值分支
 * $\text{Log}(z) = \ln r + i\Theta$，其中 $\Theta \in (-\pi, \pi]$
 *
 * 输入：
 *   - z: 标量 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - Log(z): 标量 (PRESET_TYPE_SCALAR) - 对数主值
 *
 * 复杂度：O(1)
 * 前置条件：z != 0
 */
#define PRESET_LOGARITHM_BRANCH "logarithm_branch"

/* -------------------- 级数与积分定理 -------------------- */

/**
 * @brief 复泰勒级数
 *
 * 数学定义：给定在 $z_0$ 处解析的函数 $f(z)$，展开为
 * $f(z) = \sum_{n=0}^{\infty} \frac{f^{(n)}(z_0)}{n!} (z - z_0)^n$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 解析函数
 *   - z0: 标量 (PRESET_TYPE_SCALAR) - 展开中心
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 展开阶数
 * 输出：
 *   - coefficients: 标量列表 (PRESET_TYPE_TUPLE) - 泰勒级数系数
 *
 * 复杂度：O(n)
 */
#define PRESET_TAYLOR_SERIES_COMPLEX "taylor_series_complex"

/**
 * @brief 留数定理应用
 *
 * 数学定义：若 $f$ 在简单闭曲线 $\gamma$ 内部除有限个孤立奇点 $z_1, \ldots, z_n$ 外解析，
 * 则 $\oint_{\gamma} f(z) \, dz = 2\pi i \sum_{k=1}^{n} \text{Res}(f, z_k)$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
 *   - singularities: 标量列表 (PRESET_TYPE_TUPLE) - 奇点列表
 *   - gamma: 路径 (PRESET_TYPE_PATH) - 积分路径
 * 输出：
 *   - integral: 标量 (PRESET_TYPE_SCALAR) - 积分值
 *
 * 复杂度：O(n)
 */
#define PRESET_RESIDUE_THEOREM_APPLY "residue_theorem_apply"

/**
 * @brief 柯西估计
 *
 * 数学定义：若 $f$ 在 $|z - z_0| \le R$ 上解析且 $|f(z)| \le M$，则
 * $|f^{(n)}(z_0)| \le \frac{M \cdot n!}{R^n}$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 解析函数
 *   - z0: 标量 (PRESET_TYPE_SCALAR) - 中心点
 *   - R: 标量 (PRESET_TYPE_SCALAR) - 半径
 *   - M: 标量 (PRESET_TYPE_SCALAR) - 界
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 导数阶数
 * 输出：
 *   - bound: 标量 (PRESET_TYPE_SCALAR) - 导数上界
 *
 * 复杂度：O(1)
 */
#define PRESET_CAUCHY_ESTIMATE "cauchy_estimate"

/**
 * @brief Liouville定理判定
 *
 * 数学定义：判定有界整函数是否为常数函数
 * （Liouville定理：若 $f$ 为整函数且 $|f(z)| \le M$ 对所有 $z$ 成立，则 $f$ 为常数）
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 整函数
 * 输出：
 *   - is_constant: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否为常数函数
 *
 * 复杂度：O(∞)
 */
#define PRESET_LIOUVILLE_TEST "liouville_test"

/**
 * @brief 最大模原理判定
 *
 * 数学定义：若 $f$ 在区域 $\Omega$ 内解析且在 $\overline{\Omega}$ 上连续，
 * 则 $|f(z)|$ 的最大值在 $\partial\Omega$ 上取得
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 解析函数
 *   - region: 区域 (PRESET_TYPE_REGION) - 有界区域
 * 输出：
 *   - max_on_boundary: 布尔值 (PRESET_TYPE_BOOLEAN) - 最大值是否在边界上取得
 *
 * 复杂度：O(∞)
 */
#define PRESET_MAXIMUM_MODULUS_TEST "maximum_modulus_test"

/**
 * @brief Rouché定理判定
 *
 * 数学定义：若 $f$ 和 $g$ 在简单闭曲线 $\gamma$ 上及其内部解析，
 * 且在 $\gamma$ 上 $|f(z)| > |g(z)|$，则 $f$ 和 $f + g$ 在 $\gamma$ 内部有相同个数的零点
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 函数f
 *   - g: 函数 (PRESET_TYPE_FUNCTION) - 函数g
 *   - gamma: 路径 (PRESET_TYPE_PATH) - 简单闭曲线
 * 输出：
 *   - same_zeros: 布尔值 (PRESET_TYPE_BOOLEAN) - f和f+g在gamma内部零点个数是否相同
 *
 * 复杂度：O(n)
 */
#define PRESET_ROUCHE_TEST "rouche_test"

/**
 * @brief 辐角原理
 *
 * 数学定义：若 $f$ 在简单闭曲线 $\gamma$ 上及其内部解析（$\gamma$ 上无零点和极点），
 * 则 $\frac{1}{2\pi i} \oint_{\gamma} \frac{f'(z)}{f(z)} \, dz = N - P$，
 * 其中 $N$ 为零点个数，$P$ 为极点个数（计重数）
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 亚纯函数
 *   - gamma: 路径 (PRESET_TYPE_PATH) - 简单闭曲线
 * 输出：
 *   - (N, P): 元组 (PRESET_TYPE_TUPLE) - 零点个数和极点个数
 *
 * 复杂度：O(n)
 */
#define PRESET_ARGUMENT_PRINCIPLE "argument_principle"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有复分析预设函数块
 *
 * 将复分析模块的所有预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_complex_analysis_register(void);

/**
 * @brief 获取复分析预设函数块数量
 *
 * @return int 复分析模块预设函数块总数
 */
int preset_complex_analysis_count(void);

/**
 * @brief 获取复分析模块的预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_complex_analysis_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取复分析预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_complex_analysis_category(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_COMPLEX_ANALYSIS_H */
