/**
 * @file preset_geometry_3d.c
 * @brief 三维几何预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/geometry_3d.lvz 数据驱动（convert_presets.py 生成）。
 *
 * @details 实现三维几何构造相关的所有预设函数块。
 *          包括空间点、线、面构造，三维图形，变换等。
 *
 * @module Geometry3D
 * @category PRESET_CATEGORY_GEOMETRY
 * @version 4.0.0
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
 * lv_internal.h / lv_utils.h
 *   -> 提供 lv_malloc、lv_free、lv_strdup、lv_log_* 等
 * ============================================================
 */
#include "preset_geometry_3d.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h" /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等日志宏） */

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 三维几何模块预设函数块总数：65（与头文件中 GEOMETRY_3D_PRESET_COUNT 一致） */

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
    char **names = (char **) lv_malloc(GEOMETRY_3D_PRESET_COUNT * sizeof(char *));
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
        names[i] = lv_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                lv_free((void **) &names[j]);
            }
            {
                void *tmp = names;
                lv_free(&tmp);
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
