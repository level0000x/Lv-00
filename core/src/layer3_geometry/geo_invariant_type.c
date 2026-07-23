/**
 * @file geo_invariant_type.c
 * @brief Implementation of geometric invariant types
 *
 * Provides creation, destruction, consistency checking, and
 * type-region attachment for geometric invariants with trust coloring.
 *
 * @version 1.0.0
 */

#include "geo_invariant_type.h"
#include "lv_utils.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ========================================================================
 * Internal helpers
 * ======================================================================== */

/**
 * @brief Get the expected value range for an invariant kind
 *
 * @param kind       Invariant kind
 * @param out_min    Output minimum value (may be -INFINITY)
 * @param out_max    Output maximum value (may be +INFINITY)
 */
static void get_invariant_range(GeoInvariantKind kind, double *out_min, double *out_max) {
    switch (kind) {
        case GEO_INV_DISTANCE:
        case GEO_INV_AREA:
        case GEO_INV_VOLUME:
        case GEO_INV_PERIMETER:
        case GEO_INV_MOMENT_OF_INERTIA:
            *out_min = 0.0;
            *out_max = 1e30; /* practical upper bound */
            break;

        case GEO_INV_ANGLE:
        case GEO_INV_DIHEDRAL_ANGLE:
        case GEO_INV_SOLID_ANGLE:
            *out_min = 0.0;
            *out_max = 360.0; /* degrees */
            break;

        case GEO_INV_CROSS_RATIO:
            *out_min = -1e30;
            *out_max = 1e30;
            break;

        case GEO_INV_CURVATURE:
        case GEO_INV_TORSION:
            *out_min = -1e30;
            *out_max = 1e30;
            break;

        case GEO_INV_BARYCENTER:
            *out_min = -1e30;
            *out_max = 1e30;
            break;

        case GEO_INV_PARALLELISM:
        case GEO_INV_ORTHOGONALITY:
            /* Boolean-like: 0.0 = false, 1.0 = true */
            *out_min = 0.0;
            *out_max = 1.0;
            break;

        default:
            *out_min = -1e30;
            *out_max = 1e30;
            break;
    }
}

/**
 * @brief Get a human-readable name for an invariant kind
 */
static const char *kind_default_name(GeoInvariantKind kind) {
    switch (kind) {
        case GEO_INV_DISTANCE:          return "distance";
        case GEO_INV_ANGLE:             return "angle";
        case GEO_INV_AREA:              return "area";
        case GEO_INV_VOLUME:            return "volume";
        case GEO_INV_CROSS_RATIO:       return "cross_ratio";
        case GEO_INV_CURVATURE:         return "curvature";
        case GEO_INV_TORSION:           return "torsion";
        case GEO_INV_PERIMETER:         return "perimeter";
        case GEO_INV_DIHEDRAL_ANGLE:    return "dihedral_angle";
        case GEO_INV_SOLID_ANGLE:       return "solid_angle";
        case GEO_INV_BARYCENTER:        return "barycenter";
        case GEO_INV_MOMENT_OF_INERTIA: return "moment_of_inertia";
        case GEO_INV_PARALLELISM:       return "parallelism";
        case GEO_INV_ORTHOGONALITY:     return "orthogonality";
        default:                        return "unknown";
    }
}

/* ========================================================================
 * Functions
 * ======================================================================== */

GeoInvariant *geo_invariant_create(GeoInvariantKind kind,
                                    const char *name,
                                    double value,
                                    double trust,
                                    const int *entity_ids,
                                    int entity_count) {
    GeoInvariant *inv = (GeoInvariant *)lv_malloc(sizeof(GeoInvariant));
    if (!inv) return NULL;
    memset(inv, 0, sizeof(GeoInvariant));

    inv->kind = kind;
    inv->value = value;
    inv->trust = trust;

    /* Do NOT clamp trust here; consistency check will validate the range.
     * Previously, clamping prevented geo_invariant_check_consistency from
     * detecting out-of-range trust values. */

    /* Copy name */
    if (name) {
        size_t name_len = strlen(name) + 1;
        inv->name = (char *)lv_malloc(name_len);
        if (inv->name) {
            memcpy(inv->name, name, name_len);
        }
    } else {
        /* Use default name from kind */
        const char *def = kind_default_name(kind);
        size_t def_len = strlen(def) + 1;
        inv->name = (char *)lv_malloc(def_len);
        if (inv->name) {
            memcpy(inv->name, def, def_len);
        }
    }

    /* Copy entity IDs */
    if (entity_ids && entity_count > 0) {
        inv->entity_ids = (int *)lv_malloc(sizeof(int) * (size_t)entity_count);
        if (inv->entity_ids) {
            memcpy(inv->entity_ids, entity_ids, sizeof(int) * (size_t)entity_count);
            inv->entity_count = entity_count;
        }
    }

    return inv;
}

void geo_invariant_destroy(GeoInvariant *inv) {
    if (!inv) return;
    if (inv->name) lv_free((void **)&inv->name);
    if (inv->entity_ids) lv_free((void **)&inv->entity_ids);
    if (inv->metadata) lv_free((void **)&inv->metadata);
    lv_free((void **)&inv);
}

bool geo_invariant_check_consistency(const GeoInvariant *inv) {
    if (!inv) return false;

    /* Check trust range */
    if (inv->trust < 0.0 || inv->trust > 1.0) return false;

    /* Check value range for the kind */
    double min_val, max_val;
    get_invariant_range(inv->kind, &min_val, &max_val);

    if (inv->value < min_val - 1e-10 || inv->value > max_val + 1e-10) {
        return false;
    }

    /* Check entity count is non-negative */
    if (inv->entity_count < 0) return false;

    /* If entity_count > 0, entity_ids must not be NULL */
    if (inv->entity_count > 0 && !inv->entity_ids) return false;

    return true;
}

int geo_invariant_attach_to_type(GeoInvariant *inv,
                                  int type_id,
                                  const char *region_name) {
    if (!inv || !region_name) return -1;
    if (type_id < 0) return -1;

    /* Store the type attachment as JSON metadata */
    size_t region_len = strlen(region_name);
    size_t meta_capacity = 128 + region_len;
    char *meta = (char *)lv_malloc(meta_capacity);
    if (!meta) return -1;

    snprintf(meta, meta_capacity,
             "{\"type_id\": %d, \"region\": \"%s\"}",
             type_id, region_name);

    /* Free existing metadata if any */
    if (inv->metadata) {
        lv_free((void **)&inv->metadata);
    }

    inv->metadata = meta;
    return 0;
}
