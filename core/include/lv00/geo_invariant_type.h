#ifndef LV00_GEO_INVARIANT_TYPE_H
#define LV00_GEO_INVARIANT_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* ── Invariant kind enum ── */
typedef enum {
    GEO_INV_DISTANCE = 0,
    GEO_INV_ANGLE,
    GEO_INV_AREA,
    GEO_INV_VOLUME,
    GEO_INV_CROSS_RATIO,
    GEO_INV_CURVATURE,
    GEO_INV_TORSION,
    GEO_INV_PERIMETER,
    GEO_INV_DIHEDRAL_ANGLE,
    GEO_INV_SOLID_ANGLE,
    GEO_INV_BARYCENTER,
    GEO_INV_MOMENT_OF_INERTIA,
    GEO_INV_PARALLELISM,
    GEO_INV_ORTHOGONALITY,
    GEO_INV_CONCURRENCY,
    GEO_INV_COLLINEARITY,
    GEO_INV_CONCYCLICITY
} GeoInvariantKind;

/* ── Legacy typedef for Lv00GeoInvariantType ── */
typedef GeoInvariantKind Lv00GeoInvariantType;
typedef GeoInvariantKind GeoInvariantType;

/* ── Main invariant struct ── */
typedef struct GeoInvariant {
    GeoInvariantKind kind;
    char            *name;
    double           value;
    double           trust;
    int             *entity_ids;
    int              entity_count;
    char            *metadata;
} GeoInvariant;

/* ── Legacy name ── */
typedef GeoInvariant Lv00GeoInvariant;

/* ── API ── */
GeoInvariant *geo_invariant_create(GeoInvariantKind kind,
                                    const char *name,
                                    double value,
                                    double trust,
                                    const int *entity_ids,
                                    int entity_count);
void geo_invariant_destroy(GeoInvariant *inv);
bool geo_invariant_check_consistency(const GeoInvariant *inv);
void geo_invariant_set_metadata(GeoInvariant *inv, const char *meta);
const char *geo_invariant_get_metadata(const GeoInvariant *inv);
int geo_invariant_to_json(const GeoInvariant *inv, char *buf, size_t buf_size);

/* ── Legacy alias ── */
#define lv00_geo_invariant_check(inv, pts, dim) \
        (geo_invariant_check_consistency(inv) ? 1 : 0)

#ifdef __cplusplus
}
#endif
#endif
