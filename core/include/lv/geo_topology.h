#ifndef lv_GEO_TOPOLOGY_H
#define lv_GEO_TOPOLOGY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

/* ── Basic topology elements ── */
typedef struct {
    int v0;
    int v1;
} lvEdge;

typedef struct {
    int v0;
    int v1;
    int v2;
} lvTriangle;

typedef struct {
    lvEdge *edges;
    size_t n_edges;
    lvTriangle *triangles;
    size_t n_triangles;
    int *vertices;
    size_t n_vertices;
} lvBoundary;

/* ── Simplicial complex ── */
typedef struct {
    int n_vertices;
    lvEdge *edges;
    size_t n_edges;
    size_t edges_capacity;      /* 边数组容量（lv_ensure_capacity 倍增维护） */
    lvTriangle *triangles;
    size_t n_triangles;
    size_t triangles_capacity;  /* 三角形数组容量（lv_ensure_capacity 倍增维护） */
    int dim;
    int *faces;
    int n_faces;
} lvSimplicialComplex;

/* ── Topology element type ── */
typedef enum { lv_TOP_VERTEX, lv_TOP_EDGE, lv_TOP_FACE, lv_TOP_CELL } lvTopoElement;

/* ── API ── */
lvSimplicialComplex *geo_simplicial_create(int n_vertices);
lv_PUBLIC_API void geo_simplicial_destroy(lvSimplicialComplex *sc);
lv_PUBLIC_API bool geo_simplicial_add_edge(lvSimplicialComplex *sc, int v0, int v1);
lv_PUBLIC_API bool geo_simplicial_add_triangle(lvSimplicialComplex *sc, int v0, int v1, int v2);
lv_PUBLIC_API int geo_simplicial_euler_characteristic(const lvSimplicialComplex *sc);
lvBoundary *geo_simplicial_boundary(const lvSimplicialComplex *sc, const lvTriangle *tri);
lv_PUBLIC_API void geo_simplicial_boundary_destroy(lvBoundary *boundary);
lv_PUBLIC_API int geo_simplicial_connected_components(const lvSimplicialComplex *sc);

/* ── Legacy compatibility wrappers (implemented in geo_topology.c) ── */
lv_PUBLIC_API int lv_euler_characteristic(int vertices, int edges, int faces);
lv_PUBLIC_API int lv_is_simplicial_complex(const int *faces, size_t n_faces, size_t dim);

#ifdef __cplusplus
}
#endif
#endif
