#ifndef lv_GEOMETRY_CONFIG_H
#define lv_GEOMETRY_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

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
const lvGeometryConfig *lv_geometry_get_config(void);

/** Set global geometry config. */
void lv_geometry_set_config(const lvGeometryConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif
