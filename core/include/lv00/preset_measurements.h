/**
 * @file preset_measurements.h
 * @brief 几何度量计算预设函数块
 *
 * 提供完整的几何度量计算功能，包括距离、角度、面积、体积等。
 * 这些度量是几何分析的基础，支持理论数学研究中的定量分析。
 *
 * 包含的预设函数块：
 * - 距离度量（欧几里得、曼哈顿、切比雪夫）
 * - 角度度量
 * - 面积计算
 * - 体积计算
 * - 曲率计算
 *
 * @module Measurements
 * @category PRESET_CATEGORY_MEASUREMENT
 */

#ifndef LV00_PRESET_MEASUREMENTS_H
#define LV00_PRESET_MEASUREMENTS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 距离度量 ==================== */

/**
 * @brief 欧几里得距离
 *
 * 数学定义：$d(A,B) = \sqrt{(x_B-x_A)^2 + (y_B-y_A)^2}$
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 距离 (PRESET_TYPE_DISTANCE)
 *
 * 复杂度：O(1)
 * 度量性质：满足三角不等式
 */
#define PRESET_DISTANCE_EUCLIDEAN "distance_euclidean"

/**
 * @brief 欧几里得距离平方
 *
 * 数学定义：$d^2(A,B) = (x_B-x_A)^2 + (y_B-y_A)^2$
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 距离平方 (PRESET_TYPE_SCALAR)
 *
 * 复杂度：O(1)
 * 用途：避免开方运算，用于比较
 */
#define PRESET_DISTANCE_SQUARED "distance_squared"

/**
 * @brief 曼哈顿距离（L1范数）
 *
 * 数学定义：$d_1(A,B) = |x_B-x_A| + |y_B-y_A|$
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 距离 (PRESET_TYPE_DISTANCE)
 *
 * 复杂度：O(1)
 */
#define PRESET_DISTANCE_MANHATTAN "distance_manhattan"

/**
 * @brief 切比雪夫距离（L∞范数）
 *
 * 数学定义：$d_\infty(A,B) = \max(|x_B-x_A|, |y_B-y_A|)$
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 距离 (PRESET_TYPE_DISTANCE)
 *
 * 复杂度：O(1)
 */
#define PRESET_DISTANCE_CHEBYSHEV "distance_chebyshev"

/**
 * @brief 点到直线的距离
 *
 * 数学定义：点 $P$ 到直线 $l$ 的最短距离
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT)
 *   - l_p1: 点 (PRESET_TYPE_POINT) - 直线上一点
 *   - l_p2: 点 (PRESET_TYPE_POINT) - 直线上另一点
 * 输出：
 *   - 距离 (PRESET_TYPE_DISTANCE)
 *
 * 复杂度：O(1)
 */
#define PRESET_DISTANCE_POINT_TO_LINE "distance_point_to_line"

/**
 * @brief 点到线段的距离
 *
 * 数学定义：点 $P$ 到线段 $\overline{AB}$ 的最短距离
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT)
 *   - A: 点 (PRESET_TYPE_POINT) - 线段端点
 *   - B: 点 (PRESET_TYPE_POINT) - 线段端点
 * 输出：
 *   - 距离 (PRESET_TYPE_DISTANCE)
 *
 * 复杂度：O(1)
 */
#define PRESET_DISTANCE_POINT_TO_SEGMENT "distance_point_to_segment"

/* ==================== 角度度量 ==================== */

/**
 * @brief 三点形成的角度
 *
 * 数学定义：给定顶点 $B$ 和两边上的点 $A, C$，计算 $\angle ABC$
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT) - 顶点
 *   - C: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 角度 (PRESET_TYPE_ANGLE) - 弧度制
 *
 * 复杂度：O(1)
 */
#define PRESET_ANGLE_THREE_POINTS "angle_three_points"

/**
 * @brief 两直线夹角
 *
 * 数学定义：给定直线 $l_1$ 和 $l_2$，计算它们之间的夹角
 *
 * 输入：
 *   - l1_p1: 点 (PRESET_TYPE_POINT) - 直线1上的点
 *   - l1_p2: 点 (PRESET_TYPE_POINT) - 直线1上的另一点
 *   - l2_p1: 点 (PRESET_TYPE_POINT) - 直线2上的点
 *   - l2_p2: 点 (PRESET_TYPE_POINT) - 直线2上的另一点
 * 输出：
 *   - 角度 (PRESET_TYPE_ANGLE) - 弧度制
 *
 * 复杂度：O(1)
 */
#define PRESET_ANGLE_TWO_LINES "angle_two_lines"

/**
 * @brief 有向角
 *
 * 数学定义：从射线 $\overrightarrow{BA}$ 到射线 $\overrightarrow{BC}$ 的有向角
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT) - 顶点
 *   - C: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 有向角 (PRESET_TYPE_ANGLE) - 范围 $(-\pi, \pi]$
 *
 * 复杂度：O(1)
 */
#define PRESET_DIRECTED_ANGLE "directed_angle"

/**
 * @brief 三角形内角和验证
 *
 * 数学定义：验证三角形内角和是否等于 $\pi$
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 *   - C: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 内角和 (PRESET_TYPE_ANGLE)
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIANGLE_ANGLE_SUM "triangle_angle_sum"

/* ==================== 面积计算 ==================== */

/**
 * @brief 三角形面积（坐标公式）
 *
 * 数学定义：$S = \frac{1}{2}|(x_B-x_A)(y_C-y_A) - (x_C-x_A)(y_B-y_A)|$
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 *   - C: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 面积 (PRESET_TYPE_AREA)
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIANGLE_AREA "triangle_area"

/**
 * @brief 三角形面积（海伦公式）
 *
 * 数学定义：$S = \sqrt{s(s-a)(s-b)(s-c)}$，其中 $s = \frac{a+b+c}{2}$
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 *   - C: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 面积 (PRESET_TYPE_AREA)
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIANGLE_AREA_HERON "triangle_area_heron"

/**
 * @brief 多边形面积（鞋带公式）
 *
 * 数学定义：$S = \frac{1}{2}|\sum_{i=0}^{n-1}(x_i y_{i+1} - x_{i+1} y_i)|$
 *
 * 输入：
 *   - vertices: 点数组 (PRESET_TYPE_POINT, 可变参数) - 按顺序排列的顶点
 * 输出：
 *   - 面积 (PRESET_TYPE_AREA)
 *
 * 复杂度：O(n)，n为顶点数
 */
#define PRESET_POLYGON_AREA "polygon_area"

/**
 * @brief 圆面积
 *
 * 数学定义：$S = \pi r^2$
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT) - 圆心
 *   - R: 点 (PRESET_TYPE_POINT) - 半径点
 * 输出：
 *   - 面积 (PRESET_TYPE_AREA)
 *
 * 复杂度：O(1)
 */
#define PRESET_CIRCLE_AREA "circle_area"

/**
 * @brief 扇形面积
 *
 * 数学定义：$S = \frac{1}{2}r^2\theta$
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT) - 圆心
 *   - R: 点 (PRESET_TYPE_POINT) - 半径点
 *   - angle: 标量 (PRESET_TYPE_SCALAR) - 圆心角（弧度）
 * 输出：
 *   - 面积 (PRESET_TYPE_AREA)
 *
 * 复杂度：O(1)
 */
#define PRESET_SECTOR_AREA "sector_area"

/* ==================== 长度计算 ==================== */

/**
 * @brief 线段长度
 *
 * 数学定义：$|AB| = \sqrt{(x_B-x_A)^2 + (y_B-y_A)^2}$
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 长度 (PRESET_TYPE_DISTANCE)
 *
 * 复杂度：O(1)
 */
#define PRESET_SEGMENT_LENGTH "segment_length"

/**
 * @brief 多边形周长
 *
 * 数学定义：$P = \sum_{i=0}^{n-1}|V_i V_{i+1}|$
 *
 * 输入：
 *   - vertices: 点数组 (PRESET_TYPE_POINT, 可变参数)
 * 输出：
 *   - 周长 (PRESET_TYPE_DISTANCE)
 *
 * 复杂度：O(n)
 */
#define PRESET_POLYGON_PERIMETER "polygon_perimeter"

/**
 * @brief 圆周长
 *
 * 数学定义：$C = 2\pi r$
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT) - 圆心
 *   - R: 点 (PRESET_TYPE_POINT) - 半径点
 * 输出：
 *   - 周长 (PRESET_TYPE_DISTANCE)
 *
 * 复杂度：O(1)
 */
#define PRESET_CIRCLE_CIRCUMFERENCE "circle_circumference"

/* ==================== 向量运算 ==================== */

/**
 * @brief 向量模长
 *
 * 数学定义：$|\vec{v}| = \sqrt{v_x^2 + v_y^2}$
 *
 * 输入：
 *   - v_start: 点 (PRESET_TYPE_POINT) - 向量起点
 *   - v_end: 点 (PRESET_TYPE_POINT) - 向量终点
 * 输出：
 *   - 模长 (PRESET_TYPE_SCALAR)
 *
 * 复杂度：O(1)
 */
#define PRESET_VECTOR_MAGNITUDE "vector_magnitude"

/**
 * @brief 向量点积
 *
 * 数学定义：$\vec{a} \cdot \vec{b} = a_x b_x + a_y b_y$
 *
 * 输入：
 *   - a_start: 点 (PRESET_TYPE_POINT)
 *   - a_end: 点 (PRESET_TYPE_POINT)
 *   - b_start: 点 (PRESET_TYPE_POINT)
 *   - b_end: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 点积 (PRESET_TYPE_SCALAR)
 *
 * 复杂度：O(1)
 */
#define PRESET_VECTOR_DOT_PRODUCT "vector_dot_product"

/**
 * @brief 向量叉积（二维）
 *
 * 数学定义：$\vec{a} \times \vec{b} = a_x b_y - a_y b_x$
 *
 * 输入：
 *   - a_start: 点 (PRESET_TYPE_POINT)
 *   - a_end: 点 (PRESET_TYPE_POINT)
 *   - b_start: 点 (PRESET_TYPE_POINT)
 *   - b_end: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 叉积 (PRESET_TYPE_SCALAR) - 有向面积的两倍
 *
 * 复杂度：O(1)
 */
#define PRESET_VECTOR_CROSS_PRODUCT "vector_cross_product"

/**
 * @brief 向量夹角
 *
 * 数学定义：$\theta = \arccos\left(\frac{\vec{a} \cdot \vec{b}}{|\vec{a}||\vec{b}|}\right)$
 *
 * 输入：
 *   - a_start: 点 (PRESET_TYPE_POINT)
 *   - a_end: 点 (PRESET_TYPE_POINT)
 *   - b_start: 点 (PRESET_TYPE_POINT)
 *   - b_end: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 夹角 (PRESET_TYPE_ANGLE)
 *
 * 复杂度：O(1)
 */
#define PRESET_VECTOR_ANGLE "vector_angle"

/* ==================== 曲率计算 ==================== */

/**
 * @brief 圆的曲率
 *
 * 数学定义：$\kappa = \frac{1}{r}$
 *
 * 输入：
 *   - O: 点 (PRESET_TYPE_POINT) - 圆心
 *   - R: 点 (PRESET_TYPE_POINT) - 半径点
 * 输出：
 *   - 曲率 (PRESET_TYPE_SCALAR)
 *
 * 复杂度：O(1)
 */
#define PRESET_CIRCLE_CURVATURE "circle_curvature"

/**
 * @brief 三角形外接圆曲率
 *
 * 数学定义：三角形外接圆的曲率
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 *   - C: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 曲率 (PRESET_TYPE_SCALAR)
 *
 * 复杂度：O(1)
 */
#define PRESET_CIRCUMCIRCLE_CURVATURE "circumcircle_curvature"

/* ==================== 模块初始化 ==================== */

/**
 * @brief 注册度量计算预设函数块
 *
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_measurements_register(void);

/**
 * @brief 获取度量模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_measurements_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_MEASUREMENTS_H */
