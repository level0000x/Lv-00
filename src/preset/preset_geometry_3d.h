/**
 * @file preset_geometry_3d.h
 * @brief 三维几何预设函数块 - 头文件
 *
 * @details 提供三维几何构造相关的预设函数块，包括：
 *          - 空间点、线、面的构造
 *          - 三维图形（球、柱、锥等）
 *          - 三维变换（旋转、平移、缩放）
 *          - 空间关系和计算
 *
 * @module Geometry3D
 * @category PRESET_CATEGORY_GEOMETRY
 * @version 5.0.0
 */

#ifndef PRESET_GEOMETRY_3D_H
#define PRESET_GEOMETRY_3D_H

#include "func_block_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 空间点构造 -------------------- */

/** 通过三维坐标构造空间点 P(x, y, z) */
#define PRESET_POINT_3D_FROM_COORDS "point_3d_from_coords"

/** 通过两点构造中点 */
#define PRESET_MIDPOINT_3D "midpoint_3d"

/** 构造三角形的重心 */
#define PRESET_CENTROID_3D "centroid_3d"

/** 构造四面体的重心 */
#define PRESET_CENTROID_TETRAHEDRON "centroid_tetrahedron"

/** 构造外接球心 */
#define PRESET_CIRCUMCENTER_3D "circumcenter_3d"

/** 构造内切球心 */
#define PRESET_INCENTER_3D "incenter_3d"

/** 构造垂心 */
#define PRESET_ORTHOCENTER_3D "orthocenter_3d"

/* -------------------- 空间直线构造 -------------------- */

/** 通过两点构造空间直线 */
#define PRESET_LINE_3D_FROM_POINTS "line_3d_from_points"

/** 通过点和方向向量构造直线 */
#define PRESET_LINE_3D_FROM_POINT_DIRECTION "line_3d_from_point_direction"

/** 构造两平面的交线 */
#define PRESET_LINE_3D_PLANE_INTERSECTION "line_3d_plane_intersection"

/** 构造点到直线的垂线 */
#define PRESET_PERPENDICULAR_3D_TO_LINE "perpendicular_3d_to_line"

/** 构造点到平面的垂线 */
#define PRESET_PERPENDICULAR_3D_TO_PLANE "perpendicular_3d_to_plane"

/* -------------------- 空间平面构造 -------------------- */

/** 通过三点构造平面 */
#define PRESET_PLANE_FROM_POINTS "plane_from_points"

/** 通过点和法向量构造平面 */
#define PRESET_PLANE_FROM_POINT_NORMAL "plane_from_point_normal"

/** 通过两直线构造平面（直线需相交或平行） */
#define PRESET_PLANE_FROM_LINES "plane_from_lines"

/** 构造平行于给定平面且过给定点的平面 */
#define PRESET_PARALLEL_PLANE "parallel_plane"

/** 构造垂直于给定平面且过给定点的平面 */
#define PRESET_PERPENDICULAR_PLANE "perpendicular_plane"

/** 构造两平面的角平分面 */
#define PRESET_ANGLE_BISECTOR_PLANE "angle_bisector_plane"

/* -------------------- 三维图形构造 -------------------- */

/** 通过球心和半径点构造球 */
#define PRESET_SPHERE_CENTER_RADIUS "sphere_center_radius"

/** 通过四点构造球（不共面） */
#define PRESET_SPHERE_FOUR_POINTS "sphere_four_points"

/** 通过轴和半径构造圆柱 */
#define PRESET_CYLINDER_AXIS_RADIUS "cylinder_axis_radius"

/** 通过底面圆和顶点构造圆锥 */
#define PRESET_CONE_BASE_APEX "cone_base_apex"

/** 通过中心和三个轴点构造椭球 */
#define PRESET_ELLIPSOID_CENTER_AXES "ellipsoid_center_axes"

/* -------------------- 多面体构造 -------------------- */

/** 构造正四面体 */
#define PRESET_TETRAHEDRON_REGULAR "tetrahedron_regular"

/** 构造正方体 */
#define PRESET_CUBE "cube"

/** 构造长方体 */
#define PRESET_CUBOID "cuboid"

/** 构造正八面体 */
#define PRESET_OCTAHEDRON_REGULAR "octahedron_regular"

/** 构造正十二面体 */
#define PRESET_DODECAHEDRON_REGULAR "dodecahedron_regular"

/** 构造正二十面体 */
#define PRESET_ICOSAHEDRON_REGULAR "icosahedron_regular"

/** 构造棱柱 */
#define PRESET_PRISM "prism"

/** 构造棱锥 */
#define PRESET_PYRAMID "pyramid"

/* -------------------- 三维变换 -------------------- */

/** 三维平移 */
#define PRESET_TRANSLATION_3D "translation_3d"

/** 绕轴旋转 */
#define PRESET_ROTATION_3D_AXIS "rotation_3d_axis"

/** 绕原点旋转（欧拉角） */
#define PRESET_ROTATION_3D_EULER "rotation_3d_euler"

/** 绕点旋转 */
#define PRESET_ROTATION_3D_AROUND_POINT "rotation_3d_around_point"

/** 三维缩放 */
#define PRESET_SCALE_3D "scale_3d"

/** 三维均匀缩放 */
#define PRESET_SCALE_3D_UNIFORM "scale_3d_uniform"

/** 三维反射（关于平面） */
#define PRESET_REFLECTION_3D_PLANE "reflection_3d_plane"

/** 三维点反射（中心对称） */
#define PRESET_REFLECTION_3D_POINT "reflection_3d_point"

/** 三维剪切变换 */
#define PRESET_SHEAR_3D "shear_3d"

/* -------------------- 空间关系 -------------------- */

/** 判断点是否在平面上 */
#define PRESET_POINT_ON_PLANE "point_on_plane"

/** 判断点是否在直线上 */
#define PRESET_POINT_ON_LINE_3D "point_on_line_3d"

/** 判断点是否在球内 */
#define PRESET_POINT_INSIDE_SPHERE "point_inside_sphere"

/** 判断两直线是否平行 */
#define PRESET_LINES_3D_PARALLEL "lines_3d_parallel"

/** 判断两直线是否垂直 */
#define PRESET_LINES_3D_PERPENDICULAR "lines_3d_perpendicular"

/** 判断两平面是否平行 */
#define PRESET_PLANES_PARALLEL "planes_parallel"

/** 判断两平面是否垂直 */
#define PRESET_PLANES_PERPENDICULAR "planes_perpendicular"

/** 判断直线与平面是否平行 */
#define PRESET_LINE_PLANE_PARALLEL "line_plane_parallel"

/** 判断直线与平面是否垂直 */
#define PRESET_LINE_PLANE_PERPENDICULAR "line_plane_perpendicular"

/* -------------------- 交点计算 -------------------- */

/** 直线与平面交点 */
#define PRESET_LINE_PLANE_INTERSECTION "line_plane_intersection"

/** 三平面交点 */
#define PRESET_THREE_PLANES_INTERSECTION "three_planes_intersection"

/** 直线与球交点 */
#define PRESET_LINE_SPHERE_INTERSECTION "line_sphere_intersection"

/** 两球交线（圆） */
#define PRESET_SPHERE_SPHERE_INTERSECTION "sphere_sphere_intersection"

/** 平面与球交线（圆） */
#define PRESET_PLANE_SPHERE_INTERSECTION "plane_sphere_intersection"

/* -------------------- 距离和角度 -------------------- */

/** 三维空间两点距离 */
#define PRESET_DISTANCE_3D "distance_3d"

/** 点到平面的距离 */
#define PRESET_DISTANCE_POINT_PLANE "distance_point_plane"

/** 点到直线的距离 */
#define PRESET_DISTANCE_POINT_LINE_3D "distance_point_line_3d"

/** 两平行平面间的距离 */
#define PRESET_DISTANCE_PARALLEL_PLANES "distance_parallel_planes"

/** 两异面直线间的距离 */
#define PRESET_DISTANCE_SKEW_LINES "distance_skew_lines"

/** 两平面夹角 */
#define PRESET_ANGLE_PLANES "angle_planes"

/** 直线与平面夹角 */
#define PRESET_ANGLE_LINE_PLANE "angle_line_plane"

/** 两直线夹角 */
#define PRESET_ANGLE_LINES_3D "angle_lines_3d"

/* -------------------- 投影 -------------------- */

/** 点在平面上的投影 */
#define PRESET_PROJECT_POINT_PLANE "project_point_plane"

/** 点在直线上的投影 */
#define PRESET_PROJECT_POINT_LINE_3D "project_point_line_3d"

/** 直线在平面上的投影 */
#define PRESET_PROJECT_LINE_PLANE "project_line_plane"

/* ============================================================
 * 模块接口
 * ============================================================ */

/**
 * @brief 注册所有三维几何预设
 *
 * @return true 所有预设注册成功
 * @return false 部分或全部预设注册失败
 */
bool preset_geometry_3d_register(void);

/**
 * @brief 获取三维几何预设数量
 *
 * @return 预设数量
 */
int preset_geometry_3d_count(void);

/**
 * @brief 获取三维几何预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_geometry_3d_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* PRESET_GEOMETRY_3D_H */
