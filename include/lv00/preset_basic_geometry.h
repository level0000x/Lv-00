/**
 * @file preset_basic_geometry.h
 * @brief 基础几何构造预设函数块 - 头文件（v5.0 重构版）
 *
 * @details 提供基础几何构造相关的预设函数块，包括：
 *          - 点的构造（中点、重心、外心、内心、垂心等）
 *          - 线段操作（构造、垂直平分线、中垂线上的点）
 *          - 直线和射线（直线、平行线、垂线、射线）
 *          - 圆的构造（圆心+半径点、三点定圆、切线）
 *          - 交点计算（两直线、直线与圆、两圆）
 *          - 反射与对称（关于直线、关于点）
 *          - 特殊点构造（比例分割点、调和共轭点）
 *
 * @module BasicGeometry
 * @category PRESET_CATEGORY_CONSTRUCTION
 * @version 5.0.0
 */

#ifndef LV00_PRESET_BASIC_GEOMETRY_H
#define LV00_PRESET_BASIC_GEOMETRY_H

#include "func_block_registry.h"  /* PresetCategory 枚举定义 */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义 - 点的构造
 * ============================================================ */

/** 通过直角坐标构造点 P(x, y)，输入：标量x, 标量y，输出：点 */
#define PRESET_POINT_FROM_COORDS "point_from_coords"

/** 构造两点的中点 M = (A+B)/2，输入：点A, 点B，输出：点 */
#define PRESET_MIDPOINT "midpoint"

/** 构造三角形的重心 G = (A+B+C)/3，输入：点A, 点B, 点C，输出：点 */
#define PRESET_CENTROID "centroid"

/** 构造三角形的外心（外接圆圆心），输入：三个顶点，输出：点 */
#define PRESET_CIRCUMCENTER "circumcenter"

/** 构造三角形的内心（内切圆圆心），输入：三个顶点，输出：点 */
#define PRESET_INCENTER "incenter"

/** 构造三角形的垂心（高线交点），输入：三个顶点，输出：点 */
#define PRESET_ORTHOCENTER "orthocenter"

/* ============================================================
 * 预设名称常量定义 - 线段操作
 * ============================================================ */

/** 通过两点构造线段 AB，输入：点A, 点B，输出：线段 */
#define PRESET_SEGMENT_FROM_POINTS "segment_from_points"

/** 构造线段的垂直平分线，输入：端点A, 端点B，输出：直线 */
#define PRESET_PERPENDICULAR_BISECTOR "perpendicular_bisector"

/** 在中垂线上构造距离中点为 d 的点，输入：端点A, 端点B, 距离d，输出：点 */
#define PRESET_POINT_ON_PERP_BISECTOR "point_on_perp_bisector"

/* ============================================================
 * 预设名称常量定义 - 直线和射线
 * ============================================================ */

/** 通过两点构造无限直线，输入：点A, 点B，输出：直线 */
#define PRESET_LINE_FROM_POINTS "line_from_points"

/** 过点作平行于给定线段的直线，输入：点P, 线段的端点A, 端点B，输出：直线 */
#define PRESET_PARALLEL_LINE "parallel_line"

/** 过点作垂直于给定线段的直线，输入：点P, 线段的端点A, 端点B，输出：直线 */
#define PRESET_PERPENDICULAR_LINE "perpendicular_line"

/** 通过起点和方向点构造射线，输入：起点A, 方向点B，输出：射线 */
#define PRESET_RAY_FROM_POINTS "ray_from_points"

/* ============================================================
 * 预设名称常量定义 - 圆的构造
 * ============================================================ */

/** 通过圆心和半径点构造圆，输入：圆心O, 半径点P，输出：圆 */
#define PRESET_CIRCLE_CENTER_RADIUS "circle_center_radius"

/** 通过三点构造外接圆，输入：点A, 点B, 点C，输出：圆 */
#define PRESET_CIRCLE_THREE_POINTS "circle_three_points"

/** 从外部点向圆作切线，输入：外部点P, 圆心O, 半径点R，输出：直线 */
#define PRESET_TANGENT_FROM_POINT "tangent_from_point"

/* ============================================================
 * 预设名称常量定义 - 交点计算
 * ============================================================ */

/** 计算两直线的交点，输入：线1端点A1,A2, 线2端点B1,B2，输出：点 */
#define PRESET_LINE_INTERSECTION "line_intersection"

/** 计算直线与圆的交点，输入：线端点A1,A2, 圆心O, 半径点R，输出：点 */
#define PRESET_LINE_CIRCLE_INTERSECTION "line_circle_intersection"

/** 计算两圆的交点，输入：圆1圆心O1,半径点R1, 圆2圆心O2,半径点R2，输出：点 */
#define PRESET_CIRCLE_CIRCLE_INTERSECTION "circle_circle_intersection"

/* ============================================================
 * 预设名称常量定义 - 反射与对称
 * ============================================================ */

/** 点关于直线的反射（对称点），输入：点P, 直线的点A, 点B，输出：点 */
#define PRESET_REFLECT_POINT_OVER_LINE "reflect_point_over_line"

/** 点关于点的中心对称，输入：点P, 中心C，输出：点 */
#define PRESET_REFLECT_POINT_OVER_POINT "reflect_point_over_point"

/* ============================================================
 * 预设名称常量定义 - 特殊点构造
 * ============================================================ */

/** 按比例分割线段的点，输入：端点A, 端点B, 比例t，输出：点 */
#define PRESET_POINT_DIVIDE_SEGMENT "point_divide_segment"

/** 构造调和共轭点 (A,B;C,D) = -1，输入：共线三点A,B,C，输出：点D */
#define PRESET_HARMONIC_CONJUGATE "harmonic_conjugate"

/* ============================================================
 * 模块接口
 * ============================================================ */

/**
 * @brief 注册所有基础几何预设函数块
 *
 * 此函数使用统一的 preset_blocks_register_simple() 接口
 * 注册基础几何模块的全部 25 个预设函数块，
 * 包含完整的元数据（输入/输出类型、数学定义、复杂度等）。
 * 所有预设的类别均为 PRESET_CATEGORY_CONSTRUCTION。
 *
 * 应在预设库初始化后调用。
 *
 * @return true  所有 25 个预设注册成功
 * @return false 部分或全部预设注册失败
 */
bool preset_basic_geometry_register(void);

/**
 * @brief 获取基础几何预设函数块数量
 *
 * 返回编译期常量 BASIC_GEOMETRY_PRESET_COUNT，
 * 避免运行时计算，性能最优。
 *
 * @return int 预设数量（固定为 25）
 */
int preset_basic_geometry_count(void);

/**
 * @brief 获取基础几何预设的类别
 *
 * 基础几何模块中所有预设均属于同一类别。
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_CONSTRUCTION
 */
PresetCategory preset_basic_geometry_category(void);

/**
 * @brief 获取基础几何预设名称列表
 *
 * 分配并返回基础几何模块中所有预设的名称字符串数组。
 * 调用者负责释放：先释放每个名称元素，再释放数组本身。
 *
 * @param out_names 输出：名称字符串数组（调用者需释放每个元素和数组）
 * @param out_count 输出：名称数量
 * @return true  成功获取名称列表
 * @return false 参数为 NULL 或内存分配失败
 */
bool preset_basic_geometry_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_BASIC_GEOMETRY_H */
