#ifndef LV00_GEO_TOPOLOGY_H
#define LV00_GEO_TOPOLOGY_H
/* TODO: Geo topology module stub */
#include "lv00/geo_invariant_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** Simplicial complex. */
typedef struct { int n_vertices; int n_faces; int *faces; int dim; } Lv00SimplicialComplex;
#define geo_simplicial_create(d) ((Lv00SimplicialComplex){0,0,NULL,(d)})

/** Topology element type. */
typedef enum { LV00_TOP_VERTEX, LV00_TOP_EDGE, LV00_TOP_FACE, LV00_TOP_CELL } Lv00TopoElement;

/** Check Euler characteristic. */
int lv00_euler_characteristic(int vertices, int edges, int faces);
/** Check if simplicial complex is valid. */
int lv00_is_simplicial_complex(const int *faces, size_t n_faces, size_t dim);

#ifdef __cplusplus
}
#endif

#endif
