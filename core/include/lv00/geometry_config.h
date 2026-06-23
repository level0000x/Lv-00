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
} Lv00GeometryConfig;

/** Default geometry config. */
Lv00GeometryConfig lv00_geometry_config_default(void);

#ifdef __cplusplus
}
#endif

#endif
