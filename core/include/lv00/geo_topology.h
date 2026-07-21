#ifndef LV00_GEO_TOPOLOGY_H
#define LV00_GEO_TOPOLOGY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

/* ── Basic topology elements ── */
typedef struct {
    int v0;
    int v1;
} Lv00Edge;

typedef struct {
    int v0;
    int v1;
    int v2;
} Lv00Triangle;

typedef struct {
    Lv00Edge      *edges;
    size_t         n_edges;
    Lv00Triangle  *triangles;
    size_t         n_triangles;
    int           *vertices;
    size_t         n_vertices;
} Lv00Boundary;

/* ── Simplicial complex ── */
typedef struct {
    int            n_vertices;
    Lv00Edge      *edges;
    size_t         n_edges;
    Lv00Triangle  *triangles;
    size_t         n_triangles;
    int            dim;
    int           *faces;
    int            n_faces;
} Lv00SimplicialComplex;

/* ── Topology element type ── */
typedef enum {
    LV00_TOP_VERTEX,
    LV00_TOP_EDGE,
    LV00_TOP_FACE,
    LV00_TOP_CELL
} Lv00TopoElement;

/* ── API ── */
Lv00SimplicialComplex *geo_simplicial_create(int n_vertices);
void geo_simplicial_destroy(Lv00SimplicialComplex *sc);
int geo_simplicial_add_edge(Lv00SimplicialComplex *sc, int v0, int v1);
int geo_simplicial_add_triangle(Lv00SimplicialComplex *sc, int v0, int v1, int v2);
int  geo_simplicial_euler_characteristic(const Lv00SimplicialComplex *sc);
Lv00Boundary *geo_simplicial_boundary(const Lv00SimplicialComplex *sc,
                                       const Lv00Triangle *tri);
void geo_simplicial_boundary_destroy(Lv00Boundary *boundary);
int  geo_simplicial_connected_components(const Lv00SimplicialComplex *sc);

/* ── Legacy stubs ── */
int lv00_euler_characteristic(int vertices, int edges, int faces);
int lv00_is_simplicial_complex(const int *faces, size_t n_faces, size_t dim);

#ifdef __cplusplus
}
#endif
#endif
