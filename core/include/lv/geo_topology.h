#ifndef lv_GEO_TOPOLOGY_H
#define lv_GEO_TOPOLOGY_H

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
    lvTriangle *triangles;
    size_t n_triangles;
    int dim;
    int *faces;
    int n_faces;
} lvSimplicialComplex;

/* ── Topology element type ── */
typedef enum { lv_TOP_VERTEX, lv_TOP_EDGE, lv_TOP_FACE, lv_TOP_CELL } lvTopoElement;

/* ── API ── */
lvSimplicialComplex *geo_simplicial_create(int n_vertices);
void geo_simplicial_destroy(lvSimplicialComplex *sc);
bool geo_simplicial_add_edge(lvSimplicialComplex *sc, int v0, int v1);
bool geo_simplicial_add_triangle(lvSimplicialComplex *sc, int v0, int v1, int v2);
int geo_simplicial_euler_characteristic(const lvSimplicialComplex *sc);
lvBoundary *geo_simplicial_boundary(const lvSimplicialComplex *sc, const lvTriangle *tri);
void geo_simplicial_boundary_destroy(lvBoundary *boundary);
int geo_simplicial_connected_components(const lvSimplicialComplex *sc);

/* ── Legacy stubs ── */
int lv_euler_characteristic(int vertices, int edges, int faces);
int lv_is_simplicial_complex(const int *faces, size_t n_faces, size_t dim);

#ifdef __cplusplus
}
#endif
#endif
