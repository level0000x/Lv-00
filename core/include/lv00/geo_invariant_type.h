/**
 * @file geo_invariant_type.h
 * @brief Geometric invariant types for trust-colored geometry
 *
 * Geometric invariants are properties that remain unchanged under
 * specific transformations (isometries, similarities, affine maps, etc.).
 * In the Lv-00 system, invariants carry a trust color that reflects
 * the confidence level of their computation.
 *
 * This module provides 14 fundamental geometric invariant kinds:
 *   1.  GEO_INV_DISTANCE          -- Euclidean distance between two points
 *   2.  GEO_INV_ANGLE             -- Angle between two lines/vectors
 *   3.  GEO_INV_AREA              -- Area of a polygon/triangle
 *   4.  GEO_INV_VOLUME            -- Volume of a polyhedron
 *   5.  GEO_INV_CROSS_RATIO       -- Projective cross-ratio of four collinear points
 *   6.  GEO_INV_CURVATURE         -- Curvature of a curve at a point
 *   7.  GEO_INV_TORSION           -- Torsion of a space curve at a point
 *   8.  GEO_INV_PERIMETER         -- Perimeter of a polygon
 *   9.  GEO_INV_DIHEDRAL_ANGLE    -- Dihedral angle between two planes
 *  10.  GEO_INV_SOLID_ANGLE       -- Solid angle subtended at a vertex
 *  11.  GEO_INV_BARYCENTER        -- Barycenter (centroid) of a point set
 *  12.  GEO_INV_MOMENT_OF_INERTIA -- Moment of inertia about an axis
 *  13.  GEO_INV_PARALLELISM       -- Parallelism relation between lines/planes
 *  14.  GEO_INV_ORTHOGONALITY    -- Orthogonality relation between vectors
 *
 * @version 1.0.0
 */

#ifndef LV00_GEO_INVARIANT_TYPE_H
#define LV00_GEO_INVARIANT_TYPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv00.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Enumerations
 * ======================================================================== */

/**
 * @brief Kinds of geometric invariants
 *
 * Each invariant kind specifies a particular geometric property that
 * is preserved under a class of transformations.
 */
typedef enum GeoInvariantKind {
    GEO_INV_DISTANCE,           /**< Euclidean distance */
    GEO_INV_ANGLE,              /**< Angle between lines/vectors */
    GEO_INV_AREA,               /**< Area of a polygon */
    GEO_INV_VOLUME,             /**< Volume of a polyhedron */
    GEO_INV_CROSS_RATIO,        /**< Projective cross-ratio */
    GEO_INV_CURVATURE,          /**< Curve curvature */
    GEO_INV_TORSION,            /**< Space curve torsion */
    GEO_INV_PERIMETER,          /**< Polygon perimeter */
    GEO_INV_DIHEDRAL_ANGLE,     /**< Dihedral angle */
    GEO_INV_SOLID_ANGLE,        /**< Solid angle */
    GEO_INV_BARYCENTER,         /**< Barycenter */
    GEO_INV_MOMENT_OF_INERTIA,  /**< Moment of inertia */
    GEO_INV_PARALLELISM,        /**< Parallelism relation */
    GEO_INV_ORTHOGONALITY       /**< Orthogonality relation */
} GeoInvariantKind;

/* ========================================================================
 * Structures
 * ======================================================================== */

/**
 * @brief A geometric invariant with trust coloring
 *
 * Encapsulates a named geometric invariant along with its numeric value,
 * trust level, and optional metadata.
 *
 * @param kind           The kind of invariant
 * @param name           Human-readable name (owned string)
 * @param value          Numeric value of the invariant
 * @param trust          Trust level (0.0 = untrusted, 1.0 = fully trusted)
 * @param entity_ids     IDs of geometric entities involved (owned array)
 * @param entity_count   Number of entity IDs
 * @param metadata       Optional key-value metadata (owned string, JSON format)
 */
typedef struct GeoInvariant {
    GeoInvariantKind  kind;          /**< Invariant kind */
    char             *name;          /**< Human-readable name (owned) */
    double            value;         /**< Numeric value */
    double            trust;         /**< Trust level (0.0..1.0) */
    int              *entity_ids;    /**< Entity IDs involved (owned array) */
    int               entity_count;  /**< Number of entities */
    char             *metadata;      /**< Optional JSON metadata (owned, may be NULL) */
} GeoInvariant;

/* ========================================================================
 * Functions
 * ======================================================================== */

/**
 * @brief Create a new geometric invariant
 *
 * @param kind          The invariant kind
 * @param name          Human-readable name (copied internally)
 * @param value         Numeric value
 * @param trust         Trust level (0.0..1.0)
 * @param entity_ids    Entity IDs array (copied internally, may be NULL)
 * @param entity_count  Number of entity IDs (0 if entity_ids is NULL)
 * @return A new GeoInvariant (caller owns, free with geo_invariant_destroy)
 */
LV00_PUBLIC_API GeoInvariant *geo_invariant_create(GeoInvariantKind kind,
                                                    const char *name,
                                                    double value,
                                                    double trust,
                                                    const int *entity_ids,
                                                    int entity_count);

/**
 * @brief Destroy a geometric invariant and free its resources
 * @param inv  Invariant to destroy (may be NULL)
 */
LV00_PUBLIC_API void geo_invariant_destroy(GeoInvariant *inv);

/**
 * @brief Check consistency of an invariant's metadata
 *
 * Verifies that the invariant's value is within expected ranges
 * for its kind, and that entity references are valid.
 *
 * @param inv  Invariant to check
 * @return true if consistent, false if issues detected
 */
LV00_PUBLIC_API bool geo_invariant_check_consistency(const GeoInvariant *inv);

/**
 * @brief Attach an invariant to a type region for type-level tracking
 *
 * Associates the invariant with a type identifier so that the type
 * system can track and verify geometric properties.
 *
 * @param inv          Invariant to attach
 * @param type_id      Type region identifier
 * @param region_name  Name of the type region (copied internally)
 * @return 0 on success, -1 on failure
 */
LV00_PUBLIC_API int geo_invariant_attach_to_type(GeoInvariant *inv,
                                                  int type_id,
                                                  const char *region_name);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_INVARIANT_TYPE_H */
