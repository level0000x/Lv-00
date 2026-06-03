/**
 * @file preset_geometry_3d.c
 * @brief 三维几何预设函数块 - 实现
 *
 * @details 实现三维几何构造相关的所有预设函数块。
 *          包括空间点、线、面构造，三维图形，变换等。
 *
 * @module Geometry3D
 * @category PRESET_CATEGORY_GEOMETRY
 * @version 3.3.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_geometry_3d.h -> preset_blocks.h -> func_block_registry.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_simple() 声明
 *   -> 提供 PresetCategory 枚举（PRESET_CATEGORY_GEOMETRY 等）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 *   -> 提供 PRESET_SAFE_MALLOC 等安全内存操作宏
 * lv00_internal.h / lv00_utils.h
 *   -> 提供 lv00_malloc、lv00_free、lv00_strdup、lv00_log_* 等
 * ============================================================
 */
#include "preset_geometry_3d.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h" /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等日志宏） */

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 三维几何模块预设函数块总数 */
#define GEOMETRY_3D_PRESET_COUNT 65

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 注册单个三维几何预设
 *
 * 辅助函数，用于简化预设注册过程。
 *
 * @param name 预设名称
 * @param description 描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义
 * @param complexity 复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_geometry_3d_preset(const char *name, const char *description, const PresetType *input_types,
                                        int input_count, PresetType output_type, const char *math_def,
                                        const char *complexity, bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_GEOMETRY, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/**
 * @brief 简化预设注册的宏
 */
#define REGISTER_3D(name, desc, inputs, in_count, output, math, comp, cons, rev)                                \
    do {                                                                                                        \
        if (register_geometry_3d_preset((name), (desc), (inputs), (in_count), (output), (math), (comp), (cons), \
                                        (rev))) {                                                               \
            success_count++;                                                                                    \
        } else {                                                                                                \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */                                                 \
        }                                                                                                       \
    } while (0)

/* ============================================================
 * 模块注册实现
 * ============================================================ */

bool preset_geometry_3d_register(void) {
    int success_count = 0;

    /* ============================================================
     * 空间点构造 (7个)
     * ============================================================ */

    {
        /* 通过三维坐标构造空间点 P(x, y, z) */
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_3D(PRESET_POINT_3D_FROM_COORDS, "通过笛卡尔坐标构造空间点 P(x, y, z)", inputs, 3, PRESET_TYPE_POINT,
                    "P(x, y, z) 其中 x, y, z 为实数坐标", "O(1)", true, true);
    }

    {
        /* 通过两点构造中点 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_MIDPOINT_3D, "构造空间两点的中点 M = (A+B)/2", inputs, 2, PRESET_TYPE_POINT,
                    "M = (A + B) / 2，即中点公式", "O(1)", true, false);
    }

    {
        /* 构造三角形的重心 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_CENTROID_3D, "构造空间三角形的重心 G = (A+B+C)/3", inputs, 3, PRESET_TYPE_POINT,
                    "G = (A + B + C) / 3，三条中线的交点", "O(1)", true, false);
    }

    {
        /* 构造四面体的重心 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_CENTROID_TETRAHEDRON, "构造四面体的重心 G = (A+B+C+D)/4", inputs, 4, PRESET_TYPE_POINT,
                    "G = (A + B + C + D) / 4，四条顶点到对面重心连线的交点", "O(1)", true, false);
    }

    {
        /* 构造外接球心 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_CIRCUMCENTER_3D, "构造四面体的外接球心", inputs, 4, PRESET_TYPE_POINT,
                    "到四个顶点距离相等的点，六条棱的垂直平分面的交点", "O(1)", true, false);
    }

    {
        /* 构造内切球心 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_INCENTER_3D, "构造四面体的内切球心", inputs, 4, PRESET_TYPE_POINT,
                    "到四个面距离相等的点，六条二面角平分面的交点", "O(1)", true, false);
    }

    {
        /* 构造垂心 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_ORTHOCENTER_3D, "构造空间三角形的垂心", inputs, 3, PRESET_TYPE_POINT,
                    "三条高的交点（仅当三角形所在平面确定时）", "O(1)", true, false);
    }

    /* ============================================================
     * 空间直线构造 (5个)
     * ============================================================ */

    {
        /* 通过两点构造空间直线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_LINE_3D_FROM_POINTS, "通过两点构造空间直线", inputs, 2, PRESET_TYPE_LINE,
                    "通过空间两点 A, B 的唯一确定直线 L", "O(1)", true, false);
    }

    {
        /* 通过点和方向向量构造直线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_VECTOR};
        REGISTER_3D(PRESET_LINE_3D_FROM_POINT_DIRECTION, "通过点和方向向量构造空间直线", inputs, 2, PRESET_TYPE_LINE,
                    "L: P = P0 + t*v，其中 P0 为点，v 为方向向量", "O(1)", true, false);
    }

    {
        /* 构造两平面的交线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_LINE_3D_PLANE_INTERSECTION, "构造两平面的交线", inputs, 4, PRESET_TYPE_LINE,
                    "平面 π1(P1,P2,P3) 与 π2(P1,P2,P4) 的交线", "O(1)", true, false);
    }

    {
        /* 构造点到直线的垂线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_PERPENDICULAR_3D_TO_LINE, "构造点到空间直线的垂线", inputs, 3, PRESET_TYPE_LINE,
                    "过点 P 且垂直于直线 L(P1,P2) 的直线", "O(1)", true, false);
    }

    {
        /* 构造点到平面的垂线 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_PERPENDICULAR_3D_TO_PLANE, "构造点到平面的垂线", inputs, 4, PRESET_TYPE_LINE,
                    "过点 P 且垂直于平面 π(P1,P2,P3) 的直线", "O(1)", true, false);
    }

    /* ============================================================
     * 空间平面构造 (6个)
     * ============================================================ */

    {
        /* 通过三点构造平面 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_PLANE_FROM_POINTS, "通过不共线三点构造平面", inputs, 3, PRESET_TYPE_POLYGON,
                    "通过不共线三点 A, B, C 的唯一确定平面 π", "O(1)", true, false);
    }

    {
        /* 通过点和法向量构造平面 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_VECTOR};
        REGISTER_3D(PRESET_PLANE_FROM_POINT_NORMAL, "通过点和法向量构造平面", inputs, 2, PRESET_TYPE_POLYGON,
                    "π: n·(P - P0) = 0，其中 P0 为点，n 为法向量", "O(1)", true, false);
    }

    {
        /* 通过两直线构造平面 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_PLANE_FROM_LINES, "通过两相交或平行直线构造平面", inputs, 4, PRESET_TYPE_POLYGON,
                    "包含直线 L1(P1,P2) 和 L2(P3,P4) 的平面", "O(1)", true, false);
    }

    {
        /* 构造平行平面 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_PARALLEL_PLANE, "构造平行于给定平面且过给定点的平面", inputs, 4, PRESET_TYPE_POLYGON,
                    "过点 P 且平行于平面 π(P1,P2,P3) 的平面", "O(1)", true, false);
    }

    {
        /* 构造垂直平面 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_PERPENDICULAR_PLANE, "构造过给定直线且垂直于给定平面的平面", inputs, 4, PRESET_TYPE_POLYGON,
                    "过直线 L(P1,P2) 且垂直于平面 π(P1,P2,P3) 的平面", "O(1)", true, false);
    }

    {
        /* 构造角平分面 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_ANGLE_BISECTOR_PLANE, "构造两平面的角平分面", inputs, 4, PRESET_TYPE_POLYGON,
                    "平面 π1(P1,P2,P3) 与 π2(P1,P2,P4) 的角平分面", "O(1)", true, false);
    }

    /* ============================================================
     * 三维图形构造 (5个)
     * ============================================================ */

    {
        /* 通过球心和半径点构造球 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_SPHERE_CENTER_RADIUS, "通过球心和半径点构造球", inputs, 2, PRESET_TYPE_POLYGON,
                    "球心为 O，半径为 |OR| 的球面", "O(1)", true, false);
    }

    {
        /* 通过四点构造球 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_SPHERE_FOUR_POINTS, "通过不共面四点构造球", inputs, 4, PRESET_TYPE_POLYGON,
                    "通过不共面四点的唯一球面", "O(1)", true, false);
    }

    {
        /* 通过轴和半径构造圆柱 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        REGISTER_3D(PRESET_CYLINDER_AXIS_RADIUS, "通过轴和半径构造圆柱", inputs, 3, PRESET_TYPE_POLYGON,
                    "以直线 L(P1,P2) 为轴，半径为 r 的圆柱面", "O(1)", true, false);
    }

    {
        /* 通过底面圆和顶点构造圆锥 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_CONE_BASE_APEX, "通过底面圆心和半径点以及顶点构造圆锥", inputs, 3, PRESET_TYPE_POLYGON,
                    "底面为圆 C(O,R)，顶点为 A 的圆锥面", "O(1)", true, false);
    }

    {
        /* 通过中心和三个轴点构造椭球 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_ELLIPSOID_CENTER_AXES, "通过中心和三个半轴端点构造椭球", inputs, 4, PRESET_TYPE_POLYGON,
                    "中心为 C，三个半轴分别为 |CA|, |CB|, |CC'| 的椭球", "O(1)", true, false);
    }

    /* ============================================================
     * 多面体构造 (8个)
     * ============================================================ */

    {
        /* 构造正四面体 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_TETRAHEDRON_REGULAR, "通过一边构造正四面体", inputs, 2, PRESET_TYPE_POLYGON,
                    "以线段 AB 为边的正四面体", "O(1)", true, false);
    }

    {
        /* 构造正方体 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_CUBE, "通过一边构造正方体", inputs, 2, PRESET_TYPE_POLYGON, "以线段 AB 为边的正方体", "O(1)",
                    true, false);
    }

    {
        /* 构造长方体 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        REGISTER_3D(PRESET_CUBOID, "通过底边和高构造长方体", inputs, 3, PRESET_TYPE_POLYGON,
                    "底面为矩形（边 AB），高为 h 的长方体", "O(1)", true, false);
    }

    {
        /* 构造正八面体 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_OCTAHEDRON_REGULAR, "通过对角线构造正八面体", inputs, 2, PRESET_TYPE_POLYGON,
                    "以线段 AB 为对角线的正八面体", "O(1)", true, false);
    }

    {
        /* 构造正十二面体 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_DODECAHEDRON_REGULAR, "通过一边构造正十二面体", inputs, 2, PRESET_TYPE_POLYGON,
                    "以线段 AB 为边的正十二面体", "O(1)", true, false);
    }

    {
        /* 构造正二十面体 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_ICOSAHEDRON_REGULAR, "通过一边构造正二十面体", inputs, 2, PRESET_TYPE_POLYGON,
                    "以线段 AB 为边的正二十面体", "O(1)", true, false);
    }

    {
        /* 构造棱柱 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_SCALAR};
        REGISTER_3D(PRESET_PRISM, "通过底面多边形和高构造棱柱", inputs, 2, PRESET_TYPE_POLYGON,
                    "以多边形为底面，高为 h 的直棱柱", "O(n)", true, false);
    }

    {
        /* 构造棱锥 */
        PresetType inputs[] = {PRESET_TYPE_POLYGON, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_PYRAMID, "通过底面多边形和顶点构造棱锥", inputs, 2, PRESET_TYPE_POLYGON,
                    "以多边形为底面，A 为顶点的棱锥", "O(n)", true, false);
    }

    /* ============================================================
     * 三维变换 (9个)
     * ============================================================ */

    {
        /* 三维平移 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_VECTOR};
        REGISTER_3D(PRESET_TRANSLATION_3D, "三维平移变换", inputs, 2, PRESET_TYPE_POINT,
                    "P' = P + v，将点 P 沿向量 v 平移", "O(1)", true, true);
    }

    {
        /* 绕轴旋转 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        REGISTER_3D(PRESET_ROTATION_3D_AXIS, "绕空间轴旋转", inputs, 4, PRESET_TYPE_POINT,
                    "点 P 绕轴 L(P1,P2) 旋转角度 θ 后的位置", "O(1)", true, true);
    }

    {
        /* 绕原点旋转（欧拉角） */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_3D(PRESET_ROTATION_3D_EULER, "使用欧拉角绕原点旋转", inputs, 4, PRESET_TYPE_POINT,
                    "点 P 绕 X, Y, Z 轴分别旋转 α, β, γ 后的位置", "O(1)", true, true);
    }

    {
        /* 绕点旋转 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR,
                               PRESET_TYPE_SCALAR};
        REGISTER_3D(PRESET_ROTATION_3D_AROUND_POINT, "绕空间点使用欧拉角旋转", inputs, 5, PRESET_TYPE_POINT,
                    "点 P 绕点 O 使用欧拉角(α,β,γ)旋转后的位置", "O(1)", true, true);
    }

    {
        /* 三维缩放 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_3D(PRESET_SCALE_3D, "三维非均匀缩放", inputs, 4, PRESET_TYPE_POINT,
                    "P' = (sx*x, sy*y, sz*z)，相对于原点的缩放", "O(1)", true, true);
    }

    {
        /* 三维均匀缩放 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        REGISTER_3D(PRESET_SCALE_3D_UNIFORM, "三维均匀缩放", inputs, 2, PRESET_TYPE_POINT,
                    "P' = s*P，相对于原点的均匀缩放", "O(1)", true, true);
    }

    {
        /* 三维反射（关于平面） */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_REFLECTION_3D_PLANE, "关于平面的反射（镜像）", inputs, 4, PRESET_TYPE_POINT,
                    "点 P 关于平面 π(P1,P2,P3) 的对称点", "O(1)", true, true);
    }

    {
        /* 三维点反射（中心对称） */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_REFLECTION_3D_POINT, "关于点的中心对称", inputs, 2, PRESET_TYPE_POINT,
                    "点 P 关于中心 O 的对称点 P'，满足 O 是 PP' 中点", "O(1)", true, true);
    }

    {
        /* 三维剪切变换 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_3D(PRESET_SHEAR_3D, "三维剪切变换", inputs, 4, PRESET_TYPE_POINT,
                    "沿某方向的剪切变换，保持某一坐标不变", "O(1)", true, true);
    }

    /* ============================================================
     * 空间关系 (9个)
     * ============================================================ */

    {
        /* 判断点是否在平面上 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_POINT_ON_PLANE, "判断点是否在平面上", inputs, 4, PRESET_TYPE_BOOLEAN,
                    "判断点 P 是否在平面 π(P1,P2,P3) 上", "O(1)", false, false);
    }

    {
        /* 判断点是否在直线上 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_POINT_ON_LINE_3D, "判断点是否在空间直线上", inputs, 3, PRESET_TYPE_BOOLEAN,
                    "判断点 P 是否在直线 L(P1,P2) 上", "O(1)", false, false);
    }

    {
        /* 判断点是否在球内 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_POINT_INSIDE_SPHERE, "判断点是否在球内", inputs, 3, PRESET_TYPE_BOOLEAN,
                    "判断点 P 是否在球 S(O,R) 内部或表面上", "O(1)", false, false);
    }

    {
        /* 判断两直线是否平行 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_LINES_3D_PARALLEL, "判断两空间直线是否平行", inputs, 4, PRESET_TYPE_BOOLEAN,
                    "判断直线 L1(P1,P2) 与 L2(P3,P4) 是否平行", "O(1)", false, false);
    }

    {
        /* 判断两直线是否垂直 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_LINES_3D_PERPENDICULAR, "判断两空间直线是否垂直", inputs, 4, PRESET_TYPE_BOOLEAN,
                    "判断直线 L1(P1,P2) 与 L2(P3,P4) 是否垂直", "O(1)", false, false);
    }

    {
        /* 判断两平面是否平行 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_PLANES_PARALLEL, "判断两平面是否平行", inputs, 4, PRESET_TYPE_BOOLEAN,
                    "判断平面 π1(P1,P2,P3) 与 π2(P1,P2,P4) 是否平行", "O(1)", false, false);
    }

    {
        /* 判断两平面是否垂直 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_PLANES_PERPENDICULAR, "判断两平面是否垂直", inputs, 4, PRESET_TYPE_BOOLEAN,
                    "判断平面 π1(P1,P2,P3) 与 π2(P1,P2,P4) 是否垂直", "O(1)", false, false);
    }

    {
        /* 判断直线与平面是否平行 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_LINE_PLANE_PARALLEL, "判断直线与平面是否平行", inputs, 4, PRESET_TYPE_BOOLEAN,
                    "判断直线 L(P1,P2) 与平面 π(P1,P2,P3) 是否平行", "O(1)", false, false);
    }

    {
        /* 判断直线与平面是否垂直 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_LINE_PLANE_PERPENDICULAR, "判断直线与平面是否垂直", inputs, 4, PRESET_TYPE_BOOLEAN,
                    "判断直线 L(P1,P2) 与平面 π(P1,P2,P3) 是否垂直", "O(1)", false, false);
    }

    /* ============================================================
     * 交点计算 (5个)
     * ============================================================ */

    {
        /* 直线与平面交点 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_LINE_PLANE_INTERSECTION, "计算直线与平面的交点", inputs, 4, PRESET_TYPE_POINT,
                    "直线 L(P1,P2) 与平面 π(P1,P2,P3) 的交点", "O(1)", true, false);
    }

    {
        /* 三平面交点 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_THREE_PLANES_INTERSECTION, "计算三平面的交点", inputs, 4, PRESET_TYPE_POINT,
                    "三平面 π1, π2, π3 的公共交点（当三平面不平行于同一直线时）", "O(1)", true, false);
    }

    {
        /* 直线与球交点 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_LINE_SPHERE_INTERSECTION, "计算直线与球的交点", inputs, 4, PRESET_TYPE_POINT,
                    "直线 L(P1,P2) 与球 S(O,R) 的交点，0-2个解", "O(1)", true, false);
    }

    {
        /* 两球交线（圆） */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_SPHERE_SPHERE_INTERSECTION, "计算两球的交线（圆）", inputs, 4, PRESET_TYPE_CIRCLE,
                    "球 S1(O1,R1) 与 S2(O2,R2) 的交线（当两球相交时）", "O(1)", true, false);
    }

    {
        /* 平面与球交线（圆） */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_PLANE_SPHERE_INTERSECTION, "计算平面与球的交线（圆）", inputs, 4, PRESET_TYPE_CIRCLE,
                    "平面 π 与球 S(O,R) 的交线（当平面与球相交时）", "O(1)", true, false);
    }

    /* ============================================================
     * 距离和角度 (8个)
     * ============================================================ */

    {
        /* 三维空间两点距离 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_DISTANCE_3D, "计算三维空间两点距离", inputs, 2, PRESET_TYPE_SCALAR,
                    "d(A,B) = √((xB-xA)² + (yB-yA)² + (zB-zA)²)", "O(1)", false, false);
    }

    {
        /* 点到平面的距离 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_DISTANCE_POINT_PLANE, "计算点到平面的距离", inputs, 4, PRESET_TYPE_SCALAR,
                    "d(P,π) = |n·(P-P0)|/|n|，其中 n 为法向量", "O(1)", false, false);
    }

    {
        /* 点到直线的距离 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_DISTANCE_POINT_LINE_3D, "计算点到空间直线的距离", inputs, 3, PRESET_TYPE_SCALAR,
                    "d(P,L) = |(P-P0)×v|/|v|，其中 v 为方向向量", "O(1)", false, false);
    }

    {
        /* 两平行平面间的距离 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_DISTANCE_PARALLEL_PLANES, "计算两平行平面间的距离", inputs, 4, PRESET_TYPE_SCALAR,
                    "d(π1,π2) = |d2-d1|/|n|（当两平面平行时）", "O(1)", false, false);
    }

    {
        /* 两异面直线间的距离 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_DISTANCE_SKEW_LINES, "计算两异面直线间的距离", inputs, 4, PRESET_TYPE_SCALAR,
                    "d = |(P2-P1)·(v1×v2)|/|v1×v2|", "O(1)", false, false);
    }

    {
        /* 两平面夹角 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_ANGLE_PLANES, "计算两平面的夹角", inputs, 4, PRESET_TYPE_SCALAR,
                    "cos(θ) = |n1·n2|/(|n1||n2|)", "O(1)", false, false);
    }

    {
        /* 直线与平面夹角 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_ANGLE_LINE_PLANE, "计算直线与平面的夹角", inputs, 4, PRESET_TYPE_SCALAR,
                    "sin(θ) = |v·n|/(|v||n|)，其中 v 为方向向量，n 为法向量", "O(1)", false, false);
    }

    {
        /* 两空间直线夹角 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_ANGLE_LINES_3D, "计算两空间直线的夹角", inputs, 4, PRESET_TYPE_SCALAR,
                    "cos(θ) = |v1·v2|/(|v1||v2|)，取锐角", "O(1)", false, false);
    }

    /* ============================================================
     * 投影 (3个)
     * ============================================================ */

    {
        /* 点在平面上的投影 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_PROJECT_POINT_PLANE, "计算点在平面上的投影", inputs, 4, PRESET_TYPE_POINT,
                    "点 P 在平面 π 上的垂足", "O(1)", true, false);
    }

    {
        /* 点在直线上的投影 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_PROJECT_POINT_LINE_3D, "计算点在空间直线上的投影", inputs, 3, PRESET_TYPE_POINT,
                    "点 P 在直线 L 上的垂足", "O(1)", true, false);
    }

    {
        /* 直线在平面上的投影 */
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_3D(PRESET_PROJECT_LINE_PLANE, "计算直线在平面上的投影", inputs, 4, PRESET_TYPE_LINE,
                    "直线 L 在平面 π 上的正交投影", "O(1)", true, false);
    }

    /* 检查是否所有预设都注册成功 */
    ; /* 注册完成 */

    return success_count == GEOMETRY_3D_PRESET_COUNT;
}

/* ============================================================
 * 模块信息接口
 * ============================================================ */

int preset_geometry_3d_count(void) {
    return GEOMETRY_3D_PRESET_COUNT;
}

bool preset_geometry_3d_get_names(char ***out_names, int *out_count) {
    PRESET_CHECK_NULL(out_names, error);
    PRESET_CHECK_NULL(out_count, error);

    /* 分配名称数组 */
    char **names = (char **) lv00_malloc(GEOMETRY_3D_PRESET_COUNT * sizeof(char *));
    PRESET_CHECK_NULL(names, error);

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 空间点构造 */
        PRESET_POINT_3D_FROM_COORDS,
        PRESET_MIDPOINT_3D,
        PRESET_CENTROID_3D,
        PRESET_CENTROID_TETRAHEDRON,
        PRESET_CIRCUMCENTER_3D,
        PRESET_INCENTER_3D,
        PRESET_ORTHOCENTER_3D,
        /* 空间直线构造 */
        PRESET_LINE_3D_FROM_POINTS,
        PRESET_LINE_3D_FROM_POINT_DIRECTION,
        PRESET_LINE_3D_PLANE_INTERSECTION,
        PRESET_PERPENDICULAR_3D_TO_LINE,
        PRESET_PERPENDICULAR_3D_TO_PLANE,
        /* 空间平面构造 */
        PRESET_PLANE_FROM_POINTS,
        PRESET_PLANE_FROM_POINT_NORMAL,
        PRESET_PLANE_FROM_LINES,
        PRESET_PARALLEL_PLANE,
        PRESET_PERPENDICULAR_PLANE,
        PRESET_ANGLE_BISECTOR_PLANE,
        /* 三维图形构造 */
        PRESET_SPHERE_CENTER_RADIUS,
        PRESET_SPHERE_FOUR_POINTS,
        PRESET_CYLINDER_AXIS_RADIUS,
        PRESET_CONE_BASE_APEX,
        PRESET_ELLIPSOID_CENTER_AXES,
        /* 多面体构造 */
        PRESET_TETRAHEDRON_REGULAR,
        PRESET_CUBE,
        PRESET_CUBOID,
        PRESET_OCTAHEDRON_REGULAR,
        PRESET_DODECAHEDRON_REGULAR,
        PRESET_ICOSAHEDRON_REGULAR,
        PRESET_PRISM,
        PRESET_PYRAMID,
        /* 三维变换 */
        PRESET_TRANSLATION_3D,
        PRESET_ROTATION_3D_AXIS,
        PRESET_ROTATION_3D_EULER,
        PRESET_ROTATION_3D_AROUND_POINT,
        PRESET_SCALE_3D,
        PRESET_SCALE_3D_UNIFORM,
        PRESET_REFLECTION_3D_PLANE,
        PRESET_REFLECTION_3D_POINT,
        PRESET_SHEAR_3D,
        /* 空间关系 */
        PRESET_POINT_ON_PLANE,
        PRESET_POINT_ON_LINE_3D,
        PRESET_POINT_INSIDE_SPHERE,
        PRESET_LINES_3D_PARALLEL,
        PRESET_LINES_3D_PERPENDICULAR,
        PRESET_PLANES_PARALLEL,
        PRESET_PLANES_PERPENDICULAR,
        PRESET_LINE_PLANE_PARALLEL,
        PRESET_LINE_PLANE_PERPENDICULAR,
        /* 交点计算 */
        PRESET_LINE_PLANE_INTERSECTION,
        PRESET_THREE_PLANES_INTERSECTION,
        PRESET_LINE_SPHERE_INTERSECTION,
        PRESET_SPHERE_SPHERE_INTERSECTION,
        PRESET_PLANE_SPHERE_INTERSECTION,
        /* 距离和角度 */
        PRESET_DISTANCE_3D,
        PRESET_DISTANCE_POINT_PLANE,
        PRESET_DISTANCE_POINT_LINE_3D,
        PRESET_DISTANCE_PARALLEL_PLANES,
        PRESET_DISTANCE_SKEW_LINES,
        PRESET_ANGLE_PLANES,
        PRESET_ANGLE_LINE_PLANE,
        PRESET_ANGLE_LINES_3D,
        /* 投影 */
        PRESET_PROJECT_POINT_PLANE,
        PRESET_PROJECT_POINT_LINE_3D,
        PRESET_PROJECT_LINE_PLANE,
    };

    int count = sizeof(preset_names) / sizeof(preset_names[0]);

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                {
                    void *tmp = names[j];
                    lv00_free(&tmp);
                }
            }
            {
                void *tmp = names;
                lv00_free(&tmp);
            }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;

error:
    return false;
}
