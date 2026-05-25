/**
 * @file preset_advanced_geometry.h
 * @brief 高级几何预设函数块 - 头文件
 *
 * @details 提供高级几何构造相关的预设函数块，包括：
 *          - 圆锥曲线（椭圆、抛物线、双曲线）
 *          - 贝塞尔曲线和样条曲线
 *          - 高级曲面构造
 *          - 几何优化算法
 *
 * @module AdvancedGeometry
 * @category PRESET_CATEGORY_GEOMETRY
 * @version 5.0.0
 */

#ifndef LV00_PRESET_ADVANCED_GEOMETRY_H
#define LV00_PRESET_ADVANCED_GEOMETRY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义 - 圆锥曲线
 * ============================================================ */

/** 通过焦点和准线构造椭圆 */
#define PRESET_ELLIPSE_FOCUS_DIRECTRIX "ellipse_focus_directrix"

/** 通过中心、长轴端点和短轴长度构造椭圆 */
#define PRESET_ELLIPSE_CENTER_AXES "ellipse_center_axes"

/** 通过五点构造圆锥曲线 */
#define PRESET_CONIC_FIVE_POINTS "conic_five_points"

/** 通过焦点和准线构造抛物线 */
#define PRESET_PARABOLA_FOCUS_DIRECTRIX "parabola_focus_directrix"

/** 通过顶点和焦点构造抛物线 */
#define PRESET_PARABOLA_VERTEX_FOCUS "parabola_vertex_focus"

/** 通过焦点和准线构造双曲线 */
#define PRESET_HYPERBOLA_FOCUS_DIRECTRIX "hyperbola_focus_directrix"

/** 通过中心和两顶点构造双曲线 */
#define PRESET_HYPERBOLA_CENTER_VERTICES "hyperbola_center_vertices"

/** 构造圆锥曲线的切线 */
#define PRESET_CONIC_TANGENT "conic_tangent"

/** 构造圆锥曲线的法线 */
#define PRESET_CONIC_NORMAL "conic_normal"

/** 求两圆锥曲线的交点 */
#define PRESET_CONIC_INTERSECTION "conic_intersection"

/* ============================================================
 * 预设名称常量定义 - 贝塞尔曲线
 * ============================================================ */

/** 构造二次贝塞尔曲线 */
#define PRESET_BEZIER_QUADRATIC "bezier_quadratic"

/** 构造三次贝塞尔曲线 */
#define PRESET_BEZIER_CUBIC "bezier_cubic"

/** 构造n次贝塞尔曲线 */
#define PRESET_BEZIER_N "bezier_n"

/** 计算贝塞尔曲线上的点 */
#define PRESET_BEZIER_EVALUATE "bezier_evaluate"

/** 构造贝塞尔曲线的切线 */
#define PRESET_BEZIER_TANGENT "bezier_tangent"

/** 构造贝塞尔曲线的法线 */
#define PRESET_BEZIER_NORMAL "bezier_normal"

/** 计算贝塞尔曲线的曲率 */
#define PRESET_BEZIER_CURVATURE "bezier_curvature"

/** 计算贝塞尔曲线的弧长 */
#define PRESET_BEZIER_ARCLENGTH "bezier_arclength"

/** 贝塞尔曲线细分 */
#define PRESET_BEZIER_SUBDIVIDE "bezier_subdivide"

/** 贝塞尔曲线升阶 */
#define PRESET_BEZIER_ELEVATE "bezier_elevate"

/* ============================================================
 * 预设名称常量定义 - B样条曲线
 * ============================================================ */

/** 构造B样条曲线 */
#define PRESET_BSPLINE_CREATE "bspline_create"

/** 计算B样条曲线上的点 */
#define PRESET_BSPLINE_EVALUATE "bspline_evaluate"

/** B样条曲线插入节点 */
#define PRESET_BSPLINE_INSERT_KNOT "bspline_insert_knot"

/** B样条曲线细分 */
#define PRESET_BSPLINE_SUBDIVIDE "bspline_subdivide"

/** 构造B样条曲线的导数曲线 */
#define PRESET_BSPLINE_DERIVATIVE "bspline_derivative"

/* ============================================================
 * 预设名称常量定义 - 高级曲面
 * ============================================================ */

/** 构造旋转曲面 */
#define PRESET_SURFACE_REVOLUTION "surface_revolution"

/** 构造直纹面 */
#define PRESET_SURFACE_RULED "surface_ruled"

/** 构造双线性曲面 */
#define PRESET_SURFACE_BILINEAR "surface_bilinear"

/** 构造双三次贝塞尔曲面 */
#define PRESET_SURFACE_BEZIER_BICUBIC "surface_bezier_bicubic"

/** 构造B样条曲面 */
#define PRESET_SURFACE_BSPLINE "surface_bspline"

/** 构造Coons曲面 */
#define PRESET_SURFACE_COONS "surface_coons"

/** 构造Gordon曲面 */
#define PRESET_SURFACE_GORDON "surface_gordon"

/* ============================================================
 * 预设名称常量定义 - 几何优化
 * ============================================================ */

/** 计算凸包 */
#define PRESET_CONVEX_HULL "convex_hull"

/** 计算Delaunay三角剖分 */
#define PRESET_DELAUNAY_TRIANGULATION "delaunay_triangulation"

/** 计算Voronoi图 */
#define PRESET_VORONOI_DIAGRAM "voronoi_diagram"

/** 计算最小包围圆 */
#define PRESET_MINIMUM_ENCLOSING_CIRCLE "minimum_enclosing_circle"

/** 计算最小包围球 */
#define PRESET_MINIMUM_ENCLOSING_SPHERE "minimum_enclosing_sphere"

/** 计算点集的最小包围矩形 */
#define PRESET_MINIMUM_BOUNDING_BOX "minimum_bounding_box"

/** 计算点集的质心 */
#define PRESET_CENTROID_POINTS "centroid_points"

/** 计算主成分分析（PCA） */
#define PRESET_PRINCIPAL_COMPONENTS "principal_components"

/* ============================================================
 * 预设名称常量定义 - 几何查询
 * ============================================================ */

/** 判断点是否在多边形内 */
#define PRESET_POINT_IN_POLYGON "point_in_polygon"

/** 判断点是否在凸包内 */
#define PRESET_POINT_IN_CONVEX_HULL "point_in_convex_hull"

/** 计算点到多边形的距离 */
#define PRESET_DISTANCE_POINT_POLYGON "distance_point_polygon"

/** 计算线段与多边形的交点 */
#define PRESET_SEGMENT_POLYGON_INTERSECTION "segment_polygon_intersection"

/** 计算两多边形的交集 */
#define PRESET_POLYGON_INTERSECTION "polygon_intersection"

/** 计算两多边形的并集 */
#define PRESET_POLYGON_UNION "polygon_union"

/** 计算多边形的差集 */
#define PRESET_POLYGON_DIFFERENCE "polygon_difference"

/* ============================================================
 * 预设名称常量定义 - 曲线曲面分析
 * ============================================================ */

/** 计算曲线的Frenet标架 */
#define PRESET_FRENET_FRAME "frenet_frame"

/** 计算曲面的第一基本形式 */
#define PRESET_FIRST_FUNDAMENTAL_FORM "first_fundamental_form"

/** 计算曲面的第二基本形式 */
#define PRESET_SECOND_FUNDAMENTAL_FORM "second_fundamental_form"

/** 计算曲面的高斯曲率 */
#define PRESET_GAUSSIAN_CURVATURE "gaussian_curvature"

/** 计算曲面的平均曲率 */
#define PRESET_MEAN_CURVATURE "mean_curvature"

/** 计算曲面的主曲率 */
#define PRESET_PRINCIPAL_CURVATURES "principal_curvatures"

/** 计算曲率线 */
#define PRESET_LINES_OF_CURVATURE "lines_of_curvature"

/** 计算渐近线 */
#define PRESET_ASYMPTOTIC_LINES "asymptotic_lines"

/* ============================================================
 * 模块接口
 * ============================================================ */

/**
 * @brief 注册所有高级几何预设
 *
 * @return true 所有预设注册成功
 * @return false 部分或全部预设注册失败
 */
bool preset_advanced_geometry_register(void);

/**
 * @brief 获取高级几何预设数量
 *
 * @return 预设数量
 */
int preset_advanced_geometry_count(void);

/**
 * @brief 获取高级几何预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_advanced_geometry_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_ADVANCED_GEOMETRY_H */
