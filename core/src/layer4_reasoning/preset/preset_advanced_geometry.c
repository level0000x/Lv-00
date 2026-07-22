/**
 * @file preset_advanced_geometry.c
 * @brief 高级几何预设函数块 - 实现
 *
 * @details 实现高级几何构造相关的所有预设函数块。
 *          包括圆锥曲线、贝塞尔曲线、样条曲线、高级曲面等。
 *
 * @module AdvancedGeometry
 * @category PRESET_CATEGORY_GEOMETRY
 * @version 4.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_advanced_geometry.h -> preset_blocks.h -> func_block_registry.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_simple() 声明
 *   -> 提供 PresetCategory 枚举（PRESET_CATEGORY_GEOMETRY 等）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 *   -> 提供 PRESET_SAFE_MALLOC 等安全内存操作宏
 * lv00_internal.h / lv00_utils.h
 *   -> 提供 lv00_malloc、lv00_free、lv00_strdup、lv00_log_* 等
 * ============================================================
 */
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_advanced_geometry.h"
#include "preset_blocks.h"
#include "preset_common.h"  /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等日志宏） */

#include <string.h>

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 高等几何模块预设函数块总数 */

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 注册单个高级几何预设
 */
static bool register_advanced_geometry_preset(
    const char *name,
    const char *description,
    const PresetType *input_types,
    int input_count,
    PresetType output_type,
    const char *math_def,
    const char *complexity,
    bool is_constructive,
    bool is_reversible)
{
    return preset_blocks_register_simple(
        name, description,
        PRESET_CATEGORY_GEOMETRY,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/**
 * @brief 简化预设注册的宏
 */
#define REGISTER_ADV(name, desc, inputs, in_count, output, math, comp, cons, rev) \
    do { \
        if (register_advanced_geometry_preset( \
                (name), (desc), (inputs), (in_count), (output), \
                (math), (comp), (cons), (rev))) { \
            success_count++; \
        } else { \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */ \
        } \
    } while (0)

/* ============================================================
 * 模块注册实现
 * ============================================================ */

bool preset_advanced_geometry_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 圆锥曲线 (10个)
     * ============================================================ */

    {
        /* 通过焦点和准线构造椭圆 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_LINE, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_ELLIPSE_FOCUS_DIRECTRIX,
            "通过焦点、准线和离心率构造椭圆",
            inputs, 3, PRESET_TYPE_POLYGON,
            "到焦点距离与到准线距离之比为 e (0<e<1) 的点的轨迹",
            "O(1)", true, false);
    }

    {
        /* 通过中心、长轴端点和短轴长度构造椭圆 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_ELLIPSE_CENTER_AXES,
            "通过中心、长轴端点和短轴长度构造椭圆",
            inputs, 3, PRESET_TYPE_POLYGON,
            "中心为 C，长轴端点为 A，短轴长为 2b 的椭圆",
            "O(1)", true, false);
    }

    {
        /* 通过五点构造圆锥曲线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, 
                               PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ADV(
            PRESET_CONIC_FIVE_POINTS,
            "通过五点确定圆锥曲线",
            inputs, 5, PRESET_TYPE_POLYGON,
            "通过不共线五点的唯一圆锥曲线",
            "O(1)", true, false);
    }

    {
        /* 通过焦点和准线构造抛物线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_LINE};
        REGISTER_ADV(
            PRESET_PARABOLA_FOCUS_DIRECTRIX,
            "通过焦点和准线构造抛物线",
            inputs, 2, PRESET_TYPE_POLYGON,
            "到焦点距离等于到准线距离的点的轨迹",
            "O(1)", true, false);
    }

    {
        /* 通过顶点和焦点构造抛物线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ADV(
            PRESET_PARABOLA_VERTEX_FOCUS,
            "通过顶点和焦点构造抛物线",
            inputs, 2, PRESET_TYPE_POLYGON,
            "顶点为 V，焦点为 F 的抛物线",
            "O(1)", true, false);
    }

    {
        /* 通过焦点和准线构造双曲线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_LINE, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_HYPERBOLA_FOCUS_DIRECTRIX,
            "通过焦点、准线和离心率构造双曲线",
            inputs, 3, PRESET_TYPE_POLYGON,
            "到焦点距离与到准线距离之比为 e (e>1) 的点的轨迹",
            "O(1)", true, false);
    }

    {
        /* 通过中心和两顶点构造双曲线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ADV(
            PRESET_HYPERBOLA_CENTER_VERTICES,
            "通过中心和两顶点构造双曲线",
            inputs, 3, PRESET_TYPE_POLYGON,
            "中心为 C，顶点为 V1, V2 的双曲线",
            "O(1)", true, false);
    }

    {
        /* 构造圆锥曲线的切线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, 
                               PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ADV(
            PRESET_CONIC_TANGENT,
            "构造圆锥曲线在某点的切线",
            inputs, 4, PRESET_TYPE_LINE,
            "圆锥曲线在某点处的切线",
            "O(1)", true, false);
    }

    {
        /* 构造圆锥曲线的法线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, 
                               PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ADV(
            PRESET_CONIC_NORMAL,
            "构造圆锥曲线在某点的法线",
            inputs, 4, PRESET_TYPE_LINE,
            "圆锥曲线在某点处的法线",
            "O(1)", true, false);
    }

    {
        /* 求两圆锥曲线的交点 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT,
                               PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ADV(
            PRESET_CONIC_INTERSECTION,
            "求两圆锥曲线的交点",
            inputs, 6, PRESET_TYPE_POINT,
            "两圆锥曲线的交点，最多4个",
            "O(1)", true, false);
    }

    /* ============================================================
     * 贝塞尔曲线 (10个)
     * ============================================================ */

    {
        /* 构造二次贝塞尔曲线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ADV(
            PRESET_BEZIER_QUADRATIC,
            "通过三点构造二次贝塞尔曲线",
            inputs, 3, PRESET_TYPE_POLYGON,
            "B(t) = (1-t)²P0 + 2(1-t)tP1 + t²P2, t∈[0,1]",
            "O(1)", true, false);
    }

    {
        /* 构造三次贝塞尔曲线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, 
                               PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ADV(
            PRESET_BEZIER_CUBIC,
            "通过四点构造三次贝塞尔曲线",
            inputs, 4, PRESET_TYPE_POLYGON,
            "B(t) = (1-t)³P0 + 3(1-t)²tP1 + 3(1-t)t²P2 + t³P3, t∈[0,1]",
            "O(1)", true, false);
    }

    {
        /* 构造n次贝塞尔曲线 */
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_INTEGER};
        REGISTER_ADV(
            PRESET_BEZIER_N,
            "通过控制点列表构造n次贝塞尔曲线",
            inputs, 2, PRESET_TYPE_POLYGON,
            "B(t) = Σ C(n,i)(1-t)^(n-i)t^i Pi, t∈[0,1]",
            "O(n)", true, false);
    }

    {
        /* 计算贝塞尔曲线上的点 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, 
                               PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_BEZIER_EVALUATE,
            "计算三次贝塞尔曲线在参数t处的点",
            inputs, 5, PRESET_TYPE_POINT,
            "B(t) 的值",
            "O(1)", false, false);
    }

    {
        /* 构造贝塞尔曲线的切线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, 
                               PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_BEZIER_TANGENT,
            "构造贝塞尔曲线在某参数处的切线",
            inputs, 5, PRESET_TYPE_LINE,
            "B'(t) 方向上的切线",
            "O(1)", true, false);
    }

    {
        /* 构造贝塞尔曲线的法线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, 
                               PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_BEZIER_NORMAL,
            "构造贝塞尔曲线在某参数处的法线",
            inputs, 5, PRESET_TYPE_LINE,
            "垂直于切线的法线",
            "O(1)", true, false);
    }

    {
        /* 计算贝塞尔曲线的曲率 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, 
                               PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_BEZIER_CURVATURE,
            "计算贝塞尔曲线在某参数处的曲率",
            inputs, 5, PRESET_TYPE_SCALAR,
            "κ = |B'(t)×B''(t)|/|B'(t)|³",
            "O(1)", false, false);
    }

    {
        /* 计算贝塞尔曲线的弧长 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, 
                               PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ADV(
            PRESET_BEZIER_ARCLENGTH,
            "计算三次贝塞尔曲线的弧长",
            inputs, 4, PRESET_TYPE_SCALAR,
            "L = ∫|B'(t)|dt, t∈[0,1]（数值积分）",
            "O(n)", false, false);
    }

    {
        /* 贝塞尔曲线细分 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, 
                               PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_BEZIER_SUBDIVIDE,
            "在参数t处细分贝塞尔曲线",
            inputs, 5, PRESET_TYPE_TUPLE,
            "德卡斯特里奥算法细分后的两段曲线控制点",
            "O(1)", true, false);
    }

    {
        /* 贝塞尔曲线升阶 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, 
                               PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ADV(
            PRESET_BEZIER_ELEVATE,
            "将三次贝塞尔曲线升阶为四次",
            inputs, 4, PRESET_TYPE_TUPLE,
            "升阶后的五个控制点",
            "O(1)", true, false);
    }

    /* ============================================================
     * B样条曲线 (5个)
     * ============================================================ */

    {
        /* 构造B样条曲线 */
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_INTEGER};
        REGISTER_ADV(
            PRESET_BSPLINE_CREATE,
            "通过控制点、节点向量和阶数构造B样条曲线",
            inputs, 3, PRESET_TYPE_POLYGON,
            "S(u) = Σ Ni,p(u)Pi，其中 Ni,p 为p次B样条基函数",
            "O(n)", true, false);
    }

    {
        /* 计算B样条曲线上的点 */
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST, 
                               PRESET_TYPE_INTEGER, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_BSPLINE_EVALUATE,
            "计算B样条曲线在参数u处的点",
            inputs, 4, PRESET_TYPE_POINT,
            "S(u) 的值（使用de Boor算法）",
            "O(p²)", false, false);
    }

    {
        /* B样条曲线插入节点 */
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST, 
                               PRESET_TYPE_INTEGER, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_BSPLINE_INSERT_KNOT,
            "在B样条曲线中插入节点",
            inputs, 4, PRESET_TYPE_TUPLE,
            "插入节点后的新控制点和节点向量",
            "O(n)", true, false);
    }

    {
        /* B样条曲线细分 */
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST, 
                               PRESET_TYPE_INTEGER, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_BSPLINE_SUBDIVIDE,
            "在参数u处细分B样条曲线",
            inputs, 4, PRESET_TYPE_TUPLE,
            "细分后的两段曲线控制点和节点向量",
            "O(n)", true, false);
    }

    {
        /* 构造B样条曲线的导数曲线 */
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_INTEGER};
        REGISTER_ADV(
            PRESET_BSPLINE_DERIVATIVE,
            "构造B样条曲线的导数曲线",
            inputs, 3, PRESET_TYPE_POLYGON,
            "降阶后的导数曲线控制点",
            "O(n)", true, false);
    }

    /* ============================================================
     * 高级曲面 (7个)
     * ============================================================ */

    {
        /* 构造旋转曲面 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_LINE};
        REGISTER_ADV(
            PRESET_SURFACE_REVOLUTION,
            "通过母线绕轴旋转构造旋转曲面",
            inputs, 2, PRESET_TYPE_POLYGON,
            "S(u,v) = (x(u)cos(v), x(u)sin(v), z(u))",
            "O(n)", true, false);
    }

    {
        /* 构造直纹面 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_POLYGON};
        REGISTER_ADV(
            PRESET_SURFACE_RULED,
            "通过两条曲线构造直纹面",
            inputs, 2, PRESET_TYPE_POLYGON,
            "S(u,v) = (1-v)C1(u) + vC2(u)",
            "O(n)", true, false);
    }

    {
        /* 构造双线性曲面 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, 
                               PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ADV(
            PRESET_SURFACE_BILINEAR,
            "通过四点构造双线性曲面",
            inputs, 4, PRESET_TYPE_POLYGON,
            "S(u,v) = (1-u)(1-v)P00 + u(1-v)P10 + (1-u)vP01 + uvP11",
            "O(1)", true, false);
    }

    {
        /* 构造双三次贝塞尔曲面 */
        PresetType inputs[] = {PRESET_TYPE_LIST};
        REGISTER_ADV(
            PRESET_SURFACE_BEZIER_BICUBIC,
            "通过16个控制点构造双三次贝塞尔曲面",
            inputs, 1, PRESET_TYPE_POLYGON,
            "S(u,v) = ΣΣ Bi,3(u)Bj,3(v)Pi,j",
            "O(1)", true, false);
    }

    {
        /* 构造B样条曲面 */
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST, 
                               PRESET_TYPE_LIST, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        REGISTER_ADV(
            PRESET_SURFACE_BSPLINE,
            "通过控制点网格和节点向量构造B样条曲面",
            inputs, 5, PRESET_TYPE_POLYGON,
            "S(u,v) = ΣΣ Ni,p(u)Nj,q(v)Pi,j",
            "O(mn)", true, false);
    }

    {
        /* 构造Coons曲面 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_POLYGON, 
                               PRESET_TYPE_POLYGON, PRESET_TYPE_POLYGON};
        REGISTER_ADV(
            PRESET_SURFACE_COONS,
            "通过四条边界曲线构造Coons曲面",
            inputs, 4, PRESET_TYPE_POLYGON,
            "双线性Coons曲面插值",
            "O(n)", true, false);
    }

    {
        /* 构造Gordon曲面 */
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST};
        REGISTER_ADV(
            PRESET_SURFACE_GORDON,
            "通过两组曲线构造Gordon曲面",
            inputs, 2, PRESET_TYPE_POLYGON,
            "Gordon曲面插值",
            "O(mn)", true, false);
    }

    /* ============================================================
     * 几何优化 (8个)
     * ============================================================ */

    {
        /* 计算凸包 */
        PresetType inputs[] = {PRESET_TYPE_LIST};
        REGISTER_ADV(
            PRESET_CONVEX_HULL,
            "计算点集的凸包",
            inputs, 1, PRESET_TYPE_POLYGON,
            "包含所有点的最小凸多边形（Graham扫描或Jarvis步进）",
            "O(n log n)", true, false);
    }

    {
        /* 计算Delaunay三角剖分 */
        PresetType inputs[] = {PRESET_TYPE_LIST};
        REGISTER_ADV(
            PRESET_DELAUNAY_TRIANGULATION,
            "计算点集的Delaunay三角剖分",
            inputs, 1, PRESET_TYPE_LIST,
            "最大化最小角的三角剖分",
            "O(n log n)", true, false);
    }

    {
        /* 计算Voronoi图 */
        PresetType inputs[] = {PRESET_TYPE_LIST};
        REGISTER_ADV(
            PRESET_VORONOI_DIAGRAM,
            "计算点集的Voronoi图",
            inputs, 1, PRESET_TYPE_LIST,
            "每个区域包含距离某点最近的所有点",
            "O(n log n)", true, false);
    }

    {
        /* 计算最小包围圆 */
        PresetType inputs[] = {PRESET_TYPE_LIST};
        REGISTER_ADV(
            PRESET_MINIMUM_ENCLOSING_CIRCLE,
            "计算点集的最小包围圆",
            inputs, 1, PRESET_TYPE_CIRCLE,
            "包含所有点的最小圆（Welzl算法）",
            "O(n)", true, false);
    }

    {
        /* 计算最小包围球 */
        PresetType inputs[] = {PRESET_TYPE_LIST};
        REGISTER_ADV(
            PRESET_MINIMUM_ENCLOSING_SPHERE,
            "计算点集的最小包围球",
            inputs, 1, PRESET_TYPE_POLYGON,
            "包含所有点的最小球（Welzl算法）",
            "O(n)", true, false);
    }

    {
        /* 计算点集的最小包围矩形 */
        PresetType inputs[] = {PRESET_TYPE_LIST};
        REGISTER_ADV(
            PRESET_MINIMUM_BOUNDING_BOX,
            "计算点集的最小面积包围矩形",
            inputs, 1, PRESET_TYPE_POLYGON,
            "旋转卡壳算法求最小面积包围矩形",
            "O(n log n)", true, false);
    }

    {
        /* 计算点集的质心 */
        PresetType inputs[] = {PRESET_TYPE_LIST};
        REGISTER_ADV(
            PRESET_CENTROID_POINTS,
            "计算点集的几何质心",
            inputs, 1, PRESET_TYPE_POINT,
            "C = (ΣPi)/n",
            "O(n)", false, false);
    }

    {
        /* 计算主成分分析（PCA） */
        PresetType inputs[] = {PRESET_TYPE_LIST};
        REGISTER_ADV(
            PRESET_PRINCIPAL_COMPONENTS,
            "计算点集的主成分分析",
            inputs, 1, PRESET_TYPE_TUPLE,
            "返回质心、主轴方向和方差",
            "O(n)", false, false);
    }

    /* ============================================================
     * 几何查询 (7个)
     * ============================================================ */

    {
        /* 判断点是否在多边形内 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POLYGON};
        REGISTER_ADV(
            PRESET_POINT_IN_POLYGON,
            "判断点是否在多边形内",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "射线法判断点与多边形的关系",
            "O(n)", false, false);
    }

    {
        /* 判断点是否在凸包内 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POLYGON};
        REGISTER_ADV(
            PRESET_POINT_IN_CONVEX_HULL,
            "判断点是否在凸包内",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "二分查找判断点与凸包的关系",
            "O(log n)", false, false);
    }

    {
        /* 计算点到多边形的距离 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POLYGON};
        REGISTER_ADV(
            PRESET_DISTANCE_POINT_POLYGON,
            "计算点到多边形的距离",
            inputs, 2, PRESET_TYPE_SCALAR,
            "点到多边形各边的最小距离",
            "O(n)", false, false);
    }

    {
        /* 计算线段与多边形的交点 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POLYGON};
        REGISTER_ADV(
            PRESET_SEGMENT_POLYGON_INTERSECTION,
            "计算线段与多边形的交点",
            inputs, 3, PRESET_TYPE_LIST,
            "线段与多边形各边的交点列表",
            "O(n)", true, false);
    }

    {
        /* 计算两多边形的交集 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_POLYGON};
        REGISTER_ADV(
            PRESET_POLYGON_INTERSECTION,
            "计算两多边形的交集",
            inputs, 2, PRESET_TYPE_POLYGON,
            "Sutherland-Hodgman或Weiler-Atherton裁剪",
            "O(n+m)", true, false);
    }

    {
        /* 计算两多边形的并集 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_POLYGON};
        REGISTER_ADV(
            PRESET_POLYGON_UNION,
            "计算两简单多边形的并集",
            inputs, 2, PRESET_TYPE_POLYGON,
            "多边形并集运算",
            "O(n+m)", true, false);
    }

    {
        /* 计算多边形的差集 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_POLYGON};
        REGISTER_ADV(
            PRESET_POLYGON_DIFFERENCE,
            "计算两多边形的差集",
            inputs, 2, PRESET_TYPE_POLYGON,
            "多边形差集运算",
            "O(n+m)", true, false);
    }

    /* ============================================================
     * 曲线曲面分析 (8个)
     * ============================================================ */

    {
        /* 计算Frenet标架 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_FRENET_FRAME,
            "计算曲线在某点的Frenet标架(切向量、主法向量、次法向量)",
            inputs, 2, PRESET_TYPE_TUPLE,
            "Frenet标架 {T,N,B}，其中 T=r'/|r'|, N=T'/|T'|, B=T×N",
            "O(1)", false, false);
    }

    {
        /* 计算第一基本形式 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_FIRST_FUNDAMENTAL_FORM,
            "计算曲面的第一基本形式系数(E,F,G)",
            inputs, 3, PRESET_TYPE_TUPLE,
            "I = E du² + 2F du dv + G dv²，E=ru·ru, F=ru·rv, G=rv·rv",
            "O(1)", false, false);
    }

    {
        /* 计算第二基本形式 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_SECOND_FUNDAMENTAL_FORM,
            "计算曲面的第二基本形式系数(L,M,N)",
            inputs, 3, PRESET_TYPE_TUPLE,
            "II = L du² + 2M du dv + N dv²，L=ruu·n, M=ruv·n, N=rvv·n",
            "O(1)", false, false);
    }

    {
        /* 计算高斯曲率 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_GAUSSIAN_CURVATURE,
            "计算曲面在某点的高斯曲率",
            inputs, 3, PRESET_TYPE_SCALAR,
            "K = (LN - M²) / (EG - F²)，高斯曲率是内蕴不变量",
            "O(1)", false, false);
    }

    {
        /* 计算平均曲率 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_MEAN_CURVATURE,
            "计算曲面在某点的平均曲率",
            inputs, 3, PRESET_TYPE_SCALAR,
            "H = (EN - 2FM + GL) / (2(EG - F²))",
            "O(1)", false, false);
    }

    {
        /* 计算主曲率 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_ADV(
            PRESET_PRINCIPAL_CURVATURES,
            "计算曲面在某点的主曲率κ₁和κ₂",
            inputs, 3, PRESET_TYPE_TUPLE,
            "κ = H ± √(H²-K)，主曲率是法曲率的极值",
            "O(1)", false, false);
    }

    {
        /* 计算曲率线 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON};
        REGISTER_ADV(
            PRESET_LINES_OF_CURVATURE,
            "计算曲面上的曲率线",
            inputs, 1, PRESET_TYPE_LIST,
            "曲率线是曲面上每点切方向为主方向的曲线",
            "O(n)", true, false);
    }

    {
        /* 计算渐近线 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON};
        REGISTER_ADV(
            PRESET_ASYMPTOTIC_LINES,
            "计算曲面上的渐近线",
            inputs, 1, PRESET_TYPE_LIST,
            "渐近线是法曲率为零的方向构成的曲线，满足 II=0",
            "O(n)", true, false);
    }

    /* 检查是否所有预设都注册成功 */
    ; /* 注册完成 */
    
    return success_count == ADVANCED_GEOMETRY_PRESET_COUNT;
}

/* ============================================================
 * 模块信息接口
 * ============================================================ */

PresetCategory preset_advanced_geometry_category(void)
{
    return PRESET_CATEGORY_GEOMETRY;
}

int preset_advanced_geometry_count(void)
{
    return ADVANCED_GEOMETRY_PRESET_COUNT;
}

bool preset_advanced_geometry_get_names(char ***out_names, int *out_count)
{
    PRESET_CHECK_NULL(out_names, error);
    PRESET_CHECK_NULL(out_count, error);
    
    /* 分配名称数组 */
    char **names = (char**)lv00_malloc(ADVANCED_GEOMETRY_PRESET_COUNT * sizeof(char*));
    PRESET_CHECK_NULL(names, error);
    
    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 圆锥曲线 */
        PRESET_ELLIPSE_FOCUS_DIRECTRIX,
        PRESET_ELLIPSE_CENTER_AXES,
        PRESET_CONIC_FIVE_POINTS,
        PRESET_PARABOLA_FOCUS_DIRECTRIX,
        PRESET_PARABOLA_VERTEX_FOCUS,
        PRESET_HYPERBOLA_FOCUS_DIRECTRIX,
        PRESET_HYPERBOLA_CENTER_VERTICES,
        PRESET_CONIC_TANGENT,
        PRESET_CONIC_NORMAL,
        PRESET_CONIC_INTERSECTION,
        /* 贝塞尔曲线 */
        PRESET_BEZIER_QUADRATIC,
        PRESET_BEZIER_CUBIC,
        PRESET_BEZIER_N,
        PRESET_BEZIER_EVALUATE,
        PRESET_BEZIER_TANGENT,
        PRESET_BEZIER_NORMAL,
        PRESET_BEZIER_CURVATURE,
        PRESET_BEZIER_ARCLENGTH,
        PRESET_BEZIER_SUBDIVIDE,
        PRESET_BEZIER_ELEVATE,
        /* B样条曲线 */
        PRESET_BSPLINE_CREATE,
        PRESET_BSPLINE_EVALUATE,
        PRESET_BSPLINE_INSERT_KNOT,
        PRESET_BSPLINE_SUBDIVIDE,
        PRESET_BSPLINE_DERIVATIVE,
        /* 高级曲面 */
        PRESET_SURFACE_REVOLUTION,
        PRESET_SURFACE_RULED,
        PRESET_SURFACE_BILINEAR,
        PRESET_SURFACE_BEZIER_BICUBIC,
        PRESET_SURFACE_BSPLINE,
        PRESET_SURFACE_COONS,
        PRESET_SURFACE_GORDON,
        /* 几何优化 */
        PRESET_CONVEX_HULL,
        PRESET_DELAUNAY_TRIANGULATION,
        PRESET_VORONOI_DIAGRAM,
        PRESET_MINIMUM_ENCLOSING_CIRCLE,
        PRESET_MINIMUM_ENCLOSING_SPHERE,
        PRESET_MINIMUM_BOUNDING_BOX,
        PRESET_CENTROID_POINTS,
        PRESET_PRINCIPAL_COMPONENTS,
        /* 几何查询 */
        PRESET_POINT_IN_POLYGON,
        PRESET_POINT_IN_CONVEX_HULL,
        PRESET_DISTANCE_POINT_POLYGON,
        PRESET_SEGMENT_POLYGON_INTERSECTION,
        PRESET_POLYGON_INTERSECTION,
        PRESET_POLYGON_UNION,
        PRESET_POLYGON_DIFFERENCE,
        /* 曲线曲面分析 */
        PRESET_FRENET_FRAME,
        PRESET_FIRST_FUNDAMENTAL_FORM,
        PRESET_SECOND_FUNDAMENTAL_FORM,
        PRESET_GAUSSIAN_CURVATURE,
        PRESET_MEAN_CURVATURE,
        PRESET_PRINCIPAL_CURVATURES,
        PRESET_LINES_OF_CURVATURE,
        PRESET_ASYMPTOTIC_LINES,
    };
    
    int count = sizeof(preset_names) / sizeof(preset_names[0]);
    
    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                lv00_free((void**)&names[j]);
            }
            { void *tmp = names; lv00_free(&tmp); }
            return false;
        }
    }
    
    *out_names = names;
    *out_count = count;
    return true;
    
error:
    return false;
}
