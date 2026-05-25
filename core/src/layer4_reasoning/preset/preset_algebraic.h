/**
 * @file preset_algebraic.h
 * @brief 代数运算预设函数块
 *
 * 提供代数运算相关的预设函数块，包括向量运算、多项式运算和方程求解。
 * 支持理论数学研究中的代数结构分析。
 *
 * 包含的预设函数块：
 * - 向量代数运算
 * - 多项式构造与运算
 * - 方程与方程组
 * - 矩阵运算基础
 *
 * @module Algebraic
 * @category PRESET_CATEGORY_ALGEBRAIC
 */

#ifndef LV00_PRESET_ALGEBRAIC_H
#define LV00_PRESET_ALGEBRAIC_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 向量代数 ==================== */

/**
 * @brief 向量加法
 *
 * 数学定义：给定 $\vec{OA}$ 和 $\vec{OB}$，构造 $\vec{OC} = \vec{OA} + \vec{OB}$
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT) - 原点
 *   - A: 点 (PRESET_TYPE_POINT) - 向量1终点
 *   - B: 点 (PRESET_TYPE_POINT) - 向量2终点
 * 输出：
 *   - C: 点 (PRESET_TYPE_POINT) - 和向量终点
 *
 * 复杂度：O(1)
 */
#define PRESET_VECTOR_ADD "vector_add"

/**
 * @brief 向量减法
 *
 * 数学定义：给定 $\vec{OA}$ 和 $\vec{OB}$，构造 $\vec{OC} = \vec{OA} - \vec{OB}$
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT)
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - C: 点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_VECTOR_SUB "vector_sub"

/**
 * @brief 向量数乘
 *
 * 数学定义：给定 $\vec{OA}$ 和标量 $k$，构造 $\vec{OB} = k \cdot \vec{OA}$
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT)
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - k: 标量 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - B: 点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_VECTOR_SCALE "vector_scale"

/**
 * @brief 向量线性组合
 *
 * 数学定义：$\vec{v} = k_1 \vec{v_1} + k_2 \vec{v_2}$
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT)
 *   - A1: 点 (PRESET_TYPE_POINT)
 *   - k1: 标量 (PRESET_TYPE_SCALAR)
 *   - A2: 点 (PRESET_TYPE_POINT)
 *   - k2: 标量 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - B: 点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_VECTOR_LINEAR_COMB "vector_linear_combination"

/**
 * @brief 向量归一化（单位向量）
 *
 * 数学定义：给定 $\vec{OA}$，构造单位向量 $\vec{OB} = \frac{\vec{OA}}{|\vec{OA}|}$
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT)
 *   - A: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - B: 点 (PRESET_TYPE_POINT) - 单位向量终点
 *
 * 复杂度：O(1)
 */
#define PRESET_VECTOR_NORMALIZE "vector_normalize"

/**
 * @brief 向量投影
 *
 * 数学定义：向量 $\vec{a}$ 在 $\vec{b}$ 上的投影
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT)
 *   - A: 点 (PRESET_TYPE_POINT) - 被投影向量
 *   - B: 点 (PRESET_TYPE_POINT) - 投影方向
 * 输出：
 *   - P: 点 (PRESET_TYPE_POINT) - 投影向量终点
 *
 * 复杂度：O(1)
 */
#define PRESET_VECTOR_PROJECT "vector_project"

/**
 * @brief 向量反射
 *
 * 数学定义：向量关于给定直线的反射
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT)
 *   - A: 点 (PRESET_TYPE_POINT) - 原向量
 *   - l_p1: 点 (PRESET_TYPE_POINT) - 反射轴上一点
 *   - l_p2: 点 (PRESET_TYPE_POINT) - 反射轴上另一点
 * 输出：
 *   - B: 点 (PRESET_TYPE_POINT) - 反射后向量
 *
 * 复杂度：O(1)
 */
#define PRESET_VECTOR_REFLECT "vector_reflect"

/* ==================== 坐标系与基底 ==================== */

/**
 * @brief 构造标准正交基
 *
 * 数学定义：给定原点 $O$ 和 $x$ 轴方向点 $X$，构造标准正交基
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT) - 原点
 *   - X: 点 (PRESET_TYPE_POINT) - x轴方向
 * 输出：
 *   - Y: 点 (PRESET_TYPE_POINT) - y轴单位向量终点
 *
 * 复杂度：O(1)
 */
#define PRESET_STANDARD_BASIS "standard_basis"

/**
 * @brief 坐标变换（基底变换）
 *
 * 数学定义：将点从一组基变换到另一组基
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT) - 待变换点
 *   - old_O: 点 (PRESET_TYPE_POINT) - 原坐标系原点
 *   - old_X: 点 (PRESET_TYPE_POINT) - 原x轴方向
 *   - new_O: 点 (PRESET_TYPE_POINT) - 新坐标系原点
 *   - new_X: 点 (PRESET_TYPE_POINT) - 新x轴方向
 * 输出：
 *   - P_new: 点 (PRESET_TYPE_POINT) - 变换后的点
 *
 * 复杂度：O(1)
 */
#define PRESET_COORDINATE_TRANSFORM "coordinate_transform"

/**
 * @brief 极坐标转直角坐标
 *
 * 数学定义：$(r, \theta) \rightarrow (r\cos\theta, r\sin\theta)$
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT) - 极点
 *   - r: 标量 (PRESET_TYPE_SCALAR) - 极径
 *   - theta: 标量 (PRESET_TYPE_SCALAR) - 极角
 * 输出：
 *   - P: 点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_POLAR_TO_CARTESIAN "polar_to_cartesian"

/**
 * @brief 直角坐标转极坐标
 *
 * 数学定义：$(x, y) \rightarrow (\sqrt{x^2+y^2}, \arctan(y/x))$
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT)
 *   - P: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - r: 标量 (PRESET_TYPE_SCALAR)
 *   - theta: 标量 (PRESET_TYPE_SCALAR)
 *
 * 复杂度：O(1)
 */
#define PRESET_CARTESIAN_TO_POLAR "cartesian_to_polar"

/* ==================== 复数运算（几何表示） ==================== */

/**
 * @brief 复数乘法（几何表示）
 *
 * 数学定义：将复数乘法表示为几何变换
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT) - 原点
 *   - Z1: 点 (PRESET_TYPE_POINT) - 复数1
 *   - Z2: 点 (PRESET_TYPE_POINT) - 复数2
 * 输出：
 *   - Z: 点 (PRESET_TYPE_POINT) - 乘积
 *
 * 复杂度：O(1)
 */
#define PRESET_COMPLEX_MULTIPLY "complex_multiply"

/**
 * @brief 复数除法（几何表示）
 *
 * 数学定义：复数除法的几何构造
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT)
 *   - Z1: 点 (PRESET_TYPE_POINT) - 被除数
 *   - Z2: 点 (PRESET_TYPE_POINT) - 除数
 * 输出：
 *   - Z: 点 (PRESET_TYPE_POINT) - 商
 *
 * 复杂度：O(1)
 */
#define PRESET_COMPLEX_DIVIDE "complex_divide"

/**
 * @brief 复数幂运算
 *
 * 数学定义：$z^n$ 的几何构造
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT)
 *   - Z: 点 (PRESET_TYPE_POINT)
 *   - n: 整数 (PRESET_TYPE_INTEGER)
 * 输出：
 *   - W: 点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(log n)
 */
#define PRESET_COMPLEX_POWER "complex_power"

/**
 * @brief 复数开方
 *
 * 数学定义：$\sqrt[n]{z}$ 的几何构造
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT)
 *   - Z: 点 (PRESET_TYPE_POINT)
 *   - n: 整数 (PRESET_TYPE_INTEGER)
 * 输出：
 *   - W: 点 (PRESET_TYPE_POINT) - 主值
 *
 * 复杂度：O(1)
 * 注意：产生n个解，使用选择器指定
 */
#define PRESET_COMPLEX_ROOT "complex_root"

/* ==================== 多项式几何 ==================== */

/**
 * @brief 构造多项式曲线上的点
 *
 * 数学定义：给定多项式系数和参数t，构造曲线上的点
 *
 * 输入：
 *   - coefficients: 标量数组 (PRESET_TYPE_SCALAR, 可变) - 多项式系数
 *   - t: 标量 (PRESET_TYPE_SCALAR) - 参数
 * 输出：
 *   - P: 点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(n)，n为多项式次数
 */
#define PRESET_POLYNOMIAL_POINT "polynomial_point"

/**
 * @brief 构造抛物线
 *
 * 数学定义：给定焦点和准线，构造抛物线上的点
 *
 * 输入：
 *   - focus: 点 (PRESET_TYPE_POINT) - 焦点
 *   - directrix_p1: 点 (PRESET_TYPE_POINT) - 准线上一点
 *   - directrix_p2: 点 (PRESET_TYPE_POINT) - 准线上另一点
 *   - t: 标量 (PRESET_TYPE_SCALAR) - 参数
 * 输出：
 *   - P: 点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_PARABOLA_POINT "parabola_point"

/**
 * @brief 构造椭圆
 *
 * 数学定义：给定两焦点和长轴长度，构造椭圆上的点
 *
 * 输入：
 *   - F1: 点 (PRESET_TYPE_POINT) - 焦点1
 *   - F2: 点 (PRESET_TYPE_POINT) - 焦点2
 *   - major_axis: 标量 (PRESET_TYPE_SCALAR) - 长轴长度
 *   - t: 标量 (PRESET_TYPE_SCALAR) - 参数（角度）
 * 输出：
 *   - P: 点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_ELLIPSE_POINT "ellipse_point"

/**
 * @brief 构造双曲线
 *
 * 数学定义：给定两焦点和实轴长度，构造双曲线上的点
 *
 * 输入：
 *   - F1: 点 (PRESET_TYPE_POINT) - 焦点1
 *   - F2: 点 (PRESET_TYPE_POINT) - 焦点2
 *   - transverse_axis: 标量 (PRESET_TYPE_SCALAR) - 实轴长度
 *   - t: 标量 (PRESET_TYPE_SCALAR) - 参数
 * 输出：
 *   - P: 点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_HYPERBOLA_POINT "hyperbola_point"

/* ==================== 方程求解 ==================== */

/**
 * @brief 求解线性方程组（几何法）
 *
 * 数学定义：用几何方法求解二元一次方程组
 *
 * 输入：
 *   - line1_p1: 点 (PRESET_TYPE_POINT) - 方程1对应的直线1
 *   - line1_p2: 点 (PRESET_TYPE_POINT)
 *   - line2_p1: 点 (PRESET_TYPE_POINT) - 方程2对应的直线
 *   - line2_p2: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - solution: 点 (PRESET_TYPE_POINT) - 方程组的解
 *
 * 复杂度：O(1)
 */
#define PRESET_SOLVE_LINEAR_SYSTEM "solve_linear_system"

/**
 * @brief 求解二次方程（几何法）
 *
 * 数学定义：用圆和直线的交点求解二次方程
 *
 * 输入：
 *   - a: 标量 (PRESET_TYPE_SCALAR) - 二次项系数
 *   - b: 标量 (PRESET_TYPE_SCALAR) - 一次项系数
 *   - c: 标量 (PRESET_TYPE_SCALAR) - 常数项
 * 输出：
 *   - root: 标量 (PRESET_TYPE_SCALAR) - 一个根
 *
 * 复杂度：O(1)
 * 注意：产生两个根，使用选择器指定
 */
#define PRESET_SOLVE_QUADRATIC "solve_quadratic"

/* ==================== 模块初始化 ==================== */

/**
 * @brief 注册代数运算预设函数块
 *
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_algebraic_register(void);

/**
 * @brief 获取代数模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_algebraic_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_ALGEBRAIC_H */
