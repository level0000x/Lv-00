/**
 * @file preset_differential_equations.h
 * @brief 微分方程预设函数块
 *
 * 提供理论数学研究中常用的微分方程求解与分析预设函数块。
 * 涵盖ODE求解方法、特殊ODE、PDE基本方法、存在唯一性判定以及稳定性分析。
 *
 * 包含的预设函数块：
 * - ODE求解（4个）：分离变量法、积分因子法、常数变易法、特征方程法
 * - 特殊ODE（3个）：伯努利方程、黎卡提方程、恰当方程
 * - ODE补充（3个）：一阶线性ODE、齐次ODE、Cauchy-Euler方程
 * - PDE基本（2个）：分离变量法、特征线法
 * - 存在唯一性（2个）：Picard-Lindelof定理、存在唯一性判定
 * - 稳定性分析（3个）：Lyapunov稳定性、渐近稳定性、相平面分析
 * - 数值与近似（3个）：Euler方法、Picard迭代、级数解法
 *
 * @module DifferentialEquations
 * @category PRESET_CATEGORY_ANALYSIS
 */

#ifndef LV00_PRESET_DIFFERENTIAL_EQUATIONS_H
#define LV00_PRESET_DIFFERENTIAL_EQUATIONS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== ODE求解方法 ==================== */

/**
 * @brief 分离变量法
 *
 * 数学定义：对于形如 $\frac{dy}{dx} = f(x)g(y)$ 的一阶ODE，
 * 通过分离变量得到 $$\int \frac{dy}{g(y)} = \int f(x)\,dx + C$$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - $f(x)$
 *   - g: 函数 (PRESET_TYPE_FUNCTION) - $g(y)$
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 通解 $y = y(x, C)$
 *
 * 复杂度：O(n)
 */
#define PRESET_DE_SEPARABLE_METHOD "de_separable_method"

/**
 * @brief 积分因子法
 *
 * 数学定义：对于一阶线性ODE $\frac{dy}{dx} + P(x)y = Q(x)$，
 * 积分因子为 $\mu(x) = e^{\int P(x)\,dx}$，
 * 通解为 $$y = \frac{1}{\mu(x)}\left(\int \mu(x)Q(x)\,dx + C\right)$$
 *
 * 输入：
 *   - P: 函数 (PRESET_TYPE_FUNCTION) - 系数函数 $P(x)$
 *   - Q: 函数 (PRESET_TYPE_FUNCTION) - 非齐次项 $Q(x)$
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 通解 $y(x)$
 *
 * 复杂度：O(n)
 */
#define PRESET_DE_INTEGRATING_FACTOR "de_integrating_factor"

/**
 * @brief 常数变易法
 *
 * 数学定义：对于非齐次线性ODE，先求齐次通解 $y_h = \sum c_i y_i(x)$，
 * 再将常数 $c_i$ 替换为函数 $u_i(x)$ 求特解：
 * $$y_p = \sum u_i(x) y_i(x)$$
 *
 * 输入：
 *   - y_h: 函数 (PRESET_TYPE_FUNCTION) - 齐次通解
 *   - g: 函数 (PRESET_TYPE_FUNCTION) - 非齐次项 $g(x)$
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 特解或通解
 *
 * 复杂度：O(n^2)
 */
#define PRESET_DE_VARIATION_CONSTANTS "de_variation_constants"

/**
 * @brief 特征方程法
 *
 * 数学定义：对于常系数齐次线性ODE $a_n y^{(n)} + \cdots + a_0 y = 0$，
 * 构造特征方程 $$a_n r^n + a_{n-1} r^{n-1} + \cdots + a_0 = 0$$
 * 根据特征根类型确定通解形式
 *
 * 输入：
 *   - coeffs: 列表 (PRESET_TYPE_LIST) - 系数列表 $[a_n, \ldots, a_0]$
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 通解 $y(x)$
 *
 * 复杂度：O(n^2)
 */
#define PRESET_DE_CHARACTERISTIC_EQ "de_characteristic_eq"

/* ==================== 特殊ODE ==================== */

/**
 * @brief 伯努利方程
 *
 * 数学定义：Bernoulli方程 $y' + P(x)y = Q(x)y^n$（$n \neq 0, 1$），
 * 通过代换 $v = y^{1-n}$ 化为一阶线性ODE：
 * $$v' + (1-n)P(x)v = (1-n)Q(x)$$
 *
 * 输入：
 *   - P: 函数 (PRESET_TYPE_FUNCTION) - $P(x)$
 *   - Q: 函数 (PRESET_TYPE_FUNCTION) - $Q(x)$
 *   - n: 标量 (PRESET_TYPE_SCALAR) - 指数 $n$
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 通解
 *
 * 复杂度：O(n)
 */
#define PRESET_DE_BERNOULLI_EQ "de_bernoulli_eq"

/**
 * @brief 黎卡提方程
 *
 * 数学定义：Riccati方程 $y' = P(x) + Q(x)y + R(x)y^2$，
 * 若已知一个特解 $y_1(x)$，则可通过代换 $y = y_1 + 1/v$ 化为线性ODE求解
 *
 * 输入：
 *   - P: 函数 (PRESET_TYPE_FUNCTION) - $P(x)$
 *   - Q: 函数 (PRESET_TYPE_FUNCTION) - $Q(x)$
 *   - R: 函数 (PRESET_TYPE_FUNCTION) - $R(x)$
 *   - y1: 函数 (PRESET_TYPE_FUNCTION) - 已知特解（可为NULL表示仅判定类型）
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 通解
 *
 * 复杂度：O(n)
 */
#define PRESET_DE_RICCATI_EQ "de_riccati_eq"

/**
 * @brief 恰当方程
 *
 * 数学定义：对于ODE $M(x,y)dx + N(x,y)dy = 0$，
 * 若满足恰当性条件 $\frac{\partial M}{\partial y} = \frac{\partial N}{\partial x}$，
 * 则存在势函数 $F(x,y)$ 使得 $dF = Mdx + Ndy$，通解为 $F(x,y) = C$
 *
 * 输入：
 *   - M: 函数 (PRESET_TYPE_FUNCTION) - $M(x,y)$
 *   - N: 函数 (PRESET_TYPE_FUNCTION) - $N(x,y)$
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 势函数 $F(x,y)$
 *
 * 复杂度：O(n)
 */
#define PRESET_DE_EXACT_EQ "de_exact_eq"

/* ==================== ODE补充 ==================== */

/**
 * @brief 一阶线性ODE求解
 *
 * 数学定义：求解标准一阶线性ODE $y' + P(x)y = Q(x)$ 的通解和特解
 *
 * 输入：
 *   - P: 函数 (PRESET_TYPE_FUNCTION) - $P(x)$
 *   - Q: 函数 (PRESET_TYPE_FUNCTION) - $Q(x)$
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 通解
 *
 * 复杂度：O(n)
 */
#define PRESET_DE_LINEAR_FIRST_ORDER "de_linear_first_order"

/**
 * @brief 齐次ODE
 *
 * 数学定义：对于齐次方程 $y' = F(y/x)$，通过代换 $v = y/x$ 降阶为可分离变量方程：
 * $$x\frac{dv}{dx} + v = F(v) \Rightarrow \frac{dv}{F(v) - v} = \frac{dx}{x}$$
 *
 * 输入：
 *   - F: 函数 (PRESET_TYPE_FUNCTION) - $F(v) = F(y/x)$
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 通解
 *
 * 复杂度：O(n)
 */
#define PRESET_DE_HOMOGENEOUS_ODE "de_homogeneous_ode"

/**
 * @brief Cauchy-Euler方程
 *
 * 数学定义：求解Cauchy-Euler方程 $x^n y^{(n)} + a_{n-1}x^{n-1}y^{(n-1)} + \cdots + a_0 y = g(x)$
 * 通过代换 $x = e^t$ 转化为常系数线性ODE
 *
 * 输入：
 *   - coeffs: 列表 (PRESET_TYPE_LIST) - 系数列表 $[a_{n-1}, \ldots, a_0]$
 *   - g: 函数 (PRESET_TYPE_FUNCTION) - 非齐次项 $g(x)$
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 通解
 *
 * 复杂度：O(n^2)
 */
#define PRESET_DE_CAUCHY_EULER "de_cauchy_euler"

/* ==================== PDE基本 ==================== */

/**
 * @brief PDE分离变量法
 *
 * 数学定义：对于形如 $u_t = \alpha^2 u_{xx}$（热方程）或 $u_{tt} = c^2 u_{xx}$（波动方程）的偏微分方程，
 * 设 $u(x,t) = X(x)T(t)$，分离变量得到两个ODE：
 * $$\frac{X''}{X} = \frac{T'}{\alpha^2 T} = -\lambda$$
 *
 * 输入：
 *   - eq: 方程 (PRESET_TYPE_EQUATION) - 偏微分方程
 * 输出：
 *   - 列表 (PRESET_TYPE_LIST) - 分离后的两个常微分方程
 *
 * 复杂度：O(1)
 */
#define PRESET_DE_PDE_SEPARABLE "de_pde_separable"

/**
 * @brief 特征线法
 *
 * 数学定义：对于一阶拟线性PDE $a(x,y,u)u_x + b(x,y,u)u_y = c(x,y,u)$，
 * 构造特征线方程组：
 * $$\frac{dx}{a} = \frac{dy}{b} = \frac{du}{c}$$
 *
 * 输入：
 *   - a: 函数 (PRESET_TYPE_FUNCTION) - $a(x,y,u)$
 *   - b: 函数 (PRESET_TYPE_FUNCTION) - $b(x,y,u)$
 *   - c: 函数 (PRESET_TYPE_FUNCTION) - $c(x,y,u)$
 * 输出：
 *   - 方程 (PRESET_TYPE_EQUATION) - 特征线方程组
 *
 * 复杂度：O(1)
 */
#define PRESET_DE_CHARACTERISTIC_LINE "de_characteristic_line"

/* ==================== 存在唯一性 ==================== */

/**
 * @brief Picard-Lindelof存在唯一性定理
 *
 * 数学定义：对于初值问题 $y' = f(t, y), y(t_0) = y_0$，
 * 若 $f$ 在 $(t_0, y_0)$ 的邻域内连续且关于 $y$ 满足 Lipschitz 条件
 * $$|f(t, y_1) - f(t, y_2)| \leq L|y_1 - y_2|$$
 * 则存在唯一解
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - $f(t, y)$
 *   - t0: 标量 (PRESET_TYPE_SCALAR) - 初值点 $t_0$
 *   - y0: 标量 (PRESET_TYPE_SCALAR) - 初值 $y_0$
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否存在唯一解
 *
 * 复杂度：O(n)
 */
#define PRESET_DE_EXISTENCE_UNIQUENESS "de_existence_uniqueness"

/**
 * @brief Lipschitz条件判定
 *
 * 数学定义：判定函数 $f(t, y)$ 在给定区域内是否关于 $y$ 满足 Lipschitz 条件
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - $f(t, y)$
 *   - region: 区域 (PRESET_TYPE_REGION) - 考察区域
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否满足Lipschitz条件
 *
 * 复杂度：O(n)
 */
#define PRESET_DE_LIPSCHITZ_CHECK "de_lipschitz_check"

/* ==================== 稳定性分析 ==================== */

/**
 * @brief Lyapunov稳定性
 *
 * 数学定义：对于自治系统 $\dot{x} = f(x)$，若存在 Lyapunov 函数 $V(x)$ 满足
 * $V(x) > 0$（当 $x \neq 0$），$V(0) = 0$，且 $\dot{V}(x) \leq 0$，
 * 则平衡点 $x = 0$ 是稳定的
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 向量场 $\dot{x} = f(x)$
 *   - V: 函数 (PRESET_TYPE_FUNCTION) - 候选Lyapunov函数 $V(x)$
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否Lyapunov稳定
 *
 * 复杂度：O(n)
 */
#define PRESET_DE_LYAPUNOV_STABILITY "de_lyapunov_stability"

/**
 * @brief 渐近稳定性
 *
 * 数学定义：若平衡点 Lyapunov 稳定且 $\dot{V}(x) < 0$（严格负定），
 * 则平衡点是渐近稳定的，即所有从足够接近平衡点的初始条件出发的轨线
 * 都趋于该平衡点
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 向量场 $\dot{x} = f(x)$
 *   - V: 函数 (PRESET_TYPE_FUNCTION) - Lyapunov函数 $V(x)$
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否渐近稳定
 *
 * 复杂度：O(n)
 */
#define PRESET_DE_ASYMPTOTIC_STABILITY "de_asymptotic_stability"

/**
 * @brief 相平面分析
 *
 * 数学定义：对二维自治系统 $\dot{x} = f(x, y), \dot{y} = g(x, y)$
 * 进行相平面分析，判定平衡点类型（结点、鞍点、焦点、中心）并绘制定性相图
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - $\dot{x} = f(x, y)$
 *   - g: 函数 (PRESET_TYPE_FUNCTION) - $\dot{y} = g(x, y)$
 * 输出：
 *   - 字符串 (PRESET_TYPE_STRING) - 平衡点分类及定性描述
 *
 * 复杂度：O(n)
 */
#define PRESET_DE_PHASE_PLANE "de_phase_plane"

/* ==================== 数值与近似 ==================== */

/**
 * @brief Euler方法
 *
 * 数学定义：对初值问题 $y' = f(t, y), y(t_0) = y_0$，
 * Euler递推公式为 $$y_{n+1} = y_n + h f(t_n, y_n)$$
 * 其中 $h$ 为步长，$t_{n+1} = t_n + h$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - $f(t, y)$
 *   - t0: 标量 (PRESET_TYPE_SCALAR) - 初始时间
 *   - y0: 标量 (PRESET_TYPE_SCALAR) - 初始值
 *   - h: 标量 (PRESET_TYPE_SCALAR) - 步长
 *   - N: 整数 (PRESET_TYPE_INTEGER) - 步数
 * 输出：
 *   - 列表 (PRESET_TYPE_LIST) - 近似解序列 $[(t_0,y_0), \ldots, (t_N,y_N)]$
 *
 * 复杂度：O(N)
 */
#define PRESET_DE_EULER_METHOD "de_euler_method"

/**
 * @brief Picard迭代
 *
 * 数学定义：对初值问题 $y' = f(t, y), y(t_0) = y_0$，
 * Picard迭代序列为 $$y_{n+1}(t) = y_0 + \int_{t_0}^t f(s, y_n(s))\,ds$$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - $f(t, y)$
 *   - y0: 标量 (PRESET_TYPE_SCALAR) - 初值
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 迭代次数
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 第n次迭代近似解 $y_n(t)$
 *
 * 复杂度：O(n)
 */
#define PRESET_DE_PICARD_ITERATION "de_picard_iteration"

/**
 * @brief 幂级数解法
 *
 * 数学定义：在常点附近用幂级数 $y = \sum a_n x^n$ 代入ODE，
 * 通过比较同次幂系数得到系数递推关系，从而获得级数解
 *
 * 输入：
 *   - eq: 方程 (PRESET_TYPE_EQUATION) - 常微分方程
 *   - x0: 标量 (PRESET_TYPE_SCALAR) - 展开点
 *   - N: 整数 (PRESET_TYPE_INTEGER) - 截断项数
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - N项级数近似解
 *
 * 复杂度：O(N^2)
 */
#define PRESET_DE_SERIES_SOLUTION "de_series_solution"

/* ==================== 模块初始化 ==================== */

/**
 * @brief 注册微分方程预设函数块
 *
 * 此函数由 preset_blocks_init() 自动调用，用户无需直接调用。
 *
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_differential_equations_register(void);

/**
 * @brief 获取微分方程模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_differential_equations_count(void);

/**
 * @brief 获取微分方程模块的预设类别
 *
 * @return 预设类别
 */
PresetCategory preset_differential_equations_category(void);

/**
 * @brief 获取微分方程模块的所有预设名称
 *
 * @param out_names 输出名称数组（调用者负责释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_differential_equations_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_DIFFERENTIAL_EQUATIONS_H */
