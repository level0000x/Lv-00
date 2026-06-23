#ifndef LV00_GEO_SPEC_H
#define LV00_GEO_SPEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/lv00.h"

typedef struct {
    double x, y;
} Lv00GeoSpecPoint;

typedef struct {
    Lv00GeoSpecPoint *pts;
    int count;
} Lv00GeoSpecPolygon;

int lv00_geo_spec_parse(const char *json, void *out);
void lv00_geo_spec_free(void *spec);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_SPEC_H */
