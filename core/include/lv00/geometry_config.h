#ifndef LV00_GEOMETRY_CONFIG_H
#define LV00_GEOMETRY_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** Geometry precision mode. */
typedef enum { LV00_GEO_DOUBLE, LV00_GEO_GMP } Lv00GeoPrecision;

/** Geometry configuration. */
typedef struct {
    Lv00GeoPrecision precision;
    double tolerance;
    int dimensions;
    double collinear_epsilon;
    double distance_epsilon;
} Lv00GeometryConfig;

/** Default geometry config. */
Lv00GeometryConfig lv00_geometry_config_default(void);

/** Get global geometry config. */
const Lv00GeometryConfig *lv00_geometry_get_config(void);

/** Set global geometry config. */
void lv00_geometry_set_config(const Lv00GeometryConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif
