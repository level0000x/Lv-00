#ifndef LV00_GEO_CONSTRAINT_SOLVER_H
#define LV00_GEO_CONSTRAINT_SOLVER_H
/* TODO: Geo constraint solver module stub */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** Entity Dof enum (compat). */
typedef enum {
    LV00_ENTITY_POINT_2D = 2,
    LV00_ENTITY_CIRCLE_2D = 3
} Lv00EntityType;
#define lv00_entity_dof(t) ((int)(t))

/** Constraint type. */
typedef enum { LV00_CG_DISTANCE, LV00_CG_ANGLE, LV00_CG_PARALLEL, LV00_CG_PERPENDICULAR } Lv00ConstraintType;
/** Geometric constraint. */
typedef struct { Lv00ConstraintType type; double params[4]; } Lv00GeometricConstraint;

/** Solve geometric constraints. */
int lv00_solve_constraints(const Lv00GeometricConstraint *constraints, size_t count, double *points, size_t n_points);

#ifdef __cplusplus
}
#endif

#endif
