#ifndef lv_GEOMETRY_CONFIG_H
#define lv_GEOMETRY_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stddef.h>

/** Geometry precision mode. */
typedef enum { lv_GEO_DOUBLE, lv_GEO_GMP } lvGeoPrecision;

/** Geometry configuration. */
typedef struct {
    lvGeoPrecision precision;
    double tolerance;
    int dimensions;
    double collinear_epsilon;
    double distance_epsilon;
    double perpendicular_epsilon;
    double parallel_epsilon;
    double angle_epsilon;
    double singular_threshold;
} lvGeometryConfig;

/** Default geometry config. */
lvGeometryConfig lv_geometry_config_default(void);

/** Get global geometry config. */
lv_PUBLIC_API const lvGeometryConfig *lv_geometry_get_config(void);

/** Set global geometry config. */
lv_PUBLIC_API void lv_geometry_set_config(const lvGeometryConfig *cfg);

/**
 * @brief 从配置系统 A（lvConfig，lv_config.c）同步几何相关键到全局几何配置
 *
 * 读取当前 lvConfig 中几何相关字段（geometry.geo_sym_coord_eps 等），
 * 构造 lvGeometryConfig 并经 lv_geometry_set_config() 应用，实现
 * JSON 配置加载后的显式同步（原注释所述"已知差异"现已实现）。
 */
lv_PUBLIC_API void lv_geometry_sync_config(void);

#ifdef __cplusplus
}
#endif

#endif
