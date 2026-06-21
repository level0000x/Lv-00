#ifndef LV00_HIGH_DIM_H
#define LV00_HIGH_DIM_H
/* TODO: High-dimensional geometry module stub */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** High-dimensional manager. */
typedef struct { int dim; size_t n_points; } HighDimManager;
#define high_dim_manager_create(d) ((HighDimManager*)calloc(1, sizeof(HighDimManager)))

/** Compute convex hull in N dimensions. */
int lv00_high_dim_convex_hull(const double *points, size_t count, int dim, int *faces, size_t *n_faces);
/** Compute Delaunay triangulation in N dimensions. */
int lv00_high_dim_delaunay(const double *points, size_t count, int dim, int *simplices, size_t *n_simplices);
/** Point in polyhedron test. */
int lv00_high_dim_point_in_poly(const double *point, const double *vertices, size_t n_vertices, int dim);

#ifdef __cplusplus
}
#endif

#endif
