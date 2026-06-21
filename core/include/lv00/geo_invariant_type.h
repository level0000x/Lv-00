#ifndef LV00_GEO_INVARIANT_TYPE_H
#define LV00_GEO_INVARIANT_TYPE_H
/* TODO: Geo invariant type module stub */

#ifdef __cplusplus
extern "C" {
#endif

/** Geometric invariant types. */
typedef enum {
    LV00_INV_DISTANCE,
    LV00_INV_ANGLE,
    LV00_INV_AREA,
    LV00_INV_VOLUME,
    LV00_INV_CROSS_RATIO,
    LV00_INV_PARALLELISM,
    LV00_INV_CONCURRENCY,
    LV00_INV_COLLINEARITY,
    LV00_INV_CONCYCLICITY
} Lv00GeoInvariantType;

/** Invariant descriptor. */
typedef struct {
    Lv00GeoInvariantType type;
    int point_indices[4];
} Lv00GeoInvariant;

/** Compatibility typedefs for test code. */
typedef Lv00GeoInvariant GeoInvariant;
typedef Lv00GeoInvariantType GeoInvariantType;
#define geo_invariant_create(t) ((GeoInvariant){(t),{0,0,0,0}})
#define GEO_INV_DISTANCE LV00_INV_DISTANCE
#define GEO_INV_ANGLE LV00_INV_ANGLE
#define GEO_INV_AREA LV00_INV_AREA

/** Check if invariant holds. */
int lv00_geo_invariant_check(const Lv00GeoInvariant *inv, const double *points, int dim);

#ifdef __cplusplus
}
#endif

#endif
