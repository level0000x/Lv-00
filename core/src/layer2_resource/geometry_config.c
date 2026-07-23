#include "lv/geometry_config.h"

#include <string.h>

static lvGeometryConfig g_geometry_config;

lvGeometryConfig lv_geometry_config_default(void) {
    lvGeometryConfig cfg;
    cfg.precision = lv_GEO_DOUBLE;
    cfg.tolerance = 1e-9;
    cfg.dimensions = 3;
    cfg.collinear_epsilon = 1e-9;
    cfg.distance_epsilon = 1e-9;
    cfg.perpendicular_epsilon = 1e-8;
    cfg.parallel_epsilon = 1e-8;
    cfg.angle_epsilon = 1e-6;
    cfg.singular_threshold = 1e-12;
    return cfg;
}

const lvGeometryConfig *lv_geometry_get_config(void) {
    static int initialized = 0;
    if (!initialized) {
        g_geometry_config = lv_geometry_config_default();
        initialized = 1;
    }
    return &g_geometry_config;
}

void lv_geometry_set_config(const lvGeometryConfig *cfg) {
    if (cfg != NULL) {
        g_geometry_config = *cfg;
    }
}
