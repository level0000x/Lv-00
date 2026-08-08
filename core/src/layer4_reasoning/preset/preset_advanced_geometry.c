/**
 * @file preset_advanced_geometry.c
 * @brief 高级几何预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/advanced_geometry.lvz 数据驱动（convert_presets.py 生成）。
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
 * lv_internal.h / lv_utils.h
 *   -> 提供 lv_malloc、lv_free、lv_strdup、lv_log_* 等
 * ============================================================
 */
#include "preset_advanced_geometry.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h" /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等日志宏） */

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 高等几何模块预设函数块总数：55（与头文件中 ADVANCED_GEOMETRY_PRESET_COUNT 一致） */

/* ============================================================
 * 模块信息接口
 * ============================================================ */

PresetCategory preset_advanced_geometry_category(void) {
    return PRESET_CATEGORY_GEOMETRY;
}

int preset_advanced_geometry_count(void) {
    return ADVANCED_GEOMETRY_PRESET_COUNT;
}

bool preset_advanced_geometry_get_names(char ***out_names, int *out_count) {
    PRESET_CHECK_NULL(out_names, error);
    PRESET_CHECK_NULL(out_count, error);

    /* 分配名称数组 */
    char **names = (char **) lv_malloc(ADVANCED_GEOMETRY_PRESET_COUNT * sizeof(char *));
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
