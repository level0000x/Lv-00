/**
 * @file preset_basic_geometry.h
 * @brief 基础几何构造预设函数块 - 头文件（重构版）
 *
 * @details 提供基础几何构造相关的预设函数块，包括：
 *          - 点的构造（中点、重心、外心等）
 *          - 线段操作
 *          - 直线和射线
 *          - 圆的构造
 *          - 交点计算
 *          - 反射与对称
 *
 * @module BasicGeometry
 * @category PRESET_CATEGORY_CONSTRUCTION
 * @version 5.0.0
 */

#ifndef PRESET_BASIC_GEOMETRY_H
#define PRESET_BASIC_GEOMETRY_H

#include "func_block_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义 - 点的构造
 * ============================================================ */

/** 通过坐标构造点 P(x, y) */
#define PRESET_POINT_FROM_COORDS "point_from_coords"

/** 构造两点的中点 M = (A+B)/2 */
#define PRESET_MIDPOINT "midpoint"

/** 构造三角形的重心 G = (A+B+C)/3 */
#define PRESET_CENTROID "centroid"

/** 构造三角形的外心 */
#define PRESET_CIRCUMCENTER "circumcenter"

/** 构造三角形的内心 */
#define PRESET_INCENTER "incenter"

/** 构造三角形的垂心 */
#define PRESET_ORTHOCENTER "orthocenter"

/** 构造三角形的旁心 */
#define PRESET_EXCENTER "excenter"

/* ============================================================
 * 预设名称常量定义 - 线段操作
 * ============================================================ */

/** 通过两点构造线段 */
#define PRESET_SEGMENT_FROM_POINTS "segment_from_points"

/** 构造线段的垂直平分线 */
#define PRESET_PERPENDICULAR_BISECTOR "perpendicular_bisector"

/** 在中垂线上构造距离中点为 d 的点 */
#define PRESET_POINT_ON_PERP_BISECTOR "point_on_perp_bisector"

/* ============================================================
 * 预设名称常量定义 - 直线和射线
 * ============================================================ */

/** 通过两点构造无限直线 */
#define PRESET_LINE_FROM_POINTS "line_from_points"

/** 过点作平行于给定线段的直线 */
#define PRESET_PARALLEL_LINE "parallel_line"

/** 过点作垂直于给定线段的直线 */
#define PRESET_PERPENDICULAR_LINE "perpendicular_line"

/** 通过起点和方向点构造射线 */
#define PRESET_RAY_FROM_POINTS "ray_from_points"

/** 构造角平分线 */
#define PRESET_ANGLE_BISECTOR "angle_bisector"

/* ============================================================
 * 预设名称常量定义 - 圆的构造
 * ============================================================ */

/** 通过圆心和半径点构造圆 */
#define PRESET_CIRCLE_CENTER_RADIUS "circle_center_radius"

/** 通过三点构造外接圆 */
#define PRESET_CIRCLE_THREE_POINTS "circle_three_points"

/** 从外部点向圆作切线 */
#define PRESET_TANGENT_FROM_POINT "tangent_from_point"

/** 构造两圆的公切线 */
#define PRESET_COMMON_TANGENTS "common_tangents"

/* ============================================================
 * 预设名称常量定义 - 交点计算
 * ============================================================ */

/** 计算两直线的交点 */
#define PRESET_LINE_INTERSECTION "line_intersection"

/** 计算直线与圆的交点 */
#define PRESET_LINE_CIRCLE_INTERSECTION "line_circle_intersection"

/** 计算两圆的交点 */
#define PRESET_CIRCLE_CIRCLE_INTERSECTION "circle_circle_intersection"

/** 计算圆与圆的根轴 */
#define PRESET_RADICAL_AXIS "radical_axis"

/* ============================================================
 * 预设名称常量定义 - 反射与对称
 * ============================================================ */

/** 点关于直线的反射（对称点） */
#define PRESET_REFLECT_POINT_OVER_LINE "reflect_point_over_line"

/** 点关于点的中心对称 */
#define PRESET_REFLECT_POINT_OVER_POINT "reflect_point_over_point"

/** 点关于圆的反射（反演） */
#define PRESET_INVERT_POINT_IN_CIRCLE "invert_point_in_circle"

/* ============================================================
 * 预设名称常量定义 - 特殊点构造
 * ============================================================ */

/** 按比例分割线段的点 */
#define PRESET_POINT_DIVIDE_SEGMENT "point_divide_segment"

/** 构造调和共轭点 */
#define PRESET_HARMONIC_CONJUGATE "harmonic_conjugate"

/** 构造费马点 */
#define PRESET_FERMAT_POINT "fermat_point"

/** 构造拿破仑点 */
#define PRESET_NAPOLEON_POINT "napoleon_point"

/** 构造布罗卡点 */
#define PRESET_BROCARD_POINT "brocard_point"

/* ============================================================
 * 预设名称常量定义 - 多边形构造
 * ============================================================ */

/** 构造正三角形 */
#define PRESET_EQUILATERAL_TRIANGLE "equilateral_triangle"

/** 构造正方形 */
#define PRESET_SQUARE "square"

/** 构造正多边形 */
#define PRESET_REGULAR_POLYGON "regular_polygon"

/** 构造平行四边形 */
#define PRESET_PARALLELOGRAM "parallelogram"

/** 构造菱形 */
#define PRESET_RHOMBUS "rhombus"

/** 构造矩形 */
#define PRESET_RECTANGLE "rectangle"

/** 构造梯形 */
#define PRESET_TRAPEZOID "trapezoid"

/* ============================================================
 * 模块接口
 * ============================================================ */

/**
 * @brief 注册所有基础几何预设
 *
 * 此函数注册基础几何模块的所有预设函数块。
 * 应在预设库初始化后调用。
 *
 * @return true 所有预设注册成功
 * @return false 部分或全部预设注册失败
 */
bool preset_basic_geometry_register(void);

/**
 * @brief 获取基础几何预设数量
 *
 * @return 预设数量
 */
int preset_basic_geometry_count(void);

/**
 * @brief 获取基础几何预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_basic_geometry_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取基础几何预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_basic_geometry_category(void);

#ifdef __cplusplus
}
#endif

#endif /* PRESET_BASIC_GEOMETRY_H */
