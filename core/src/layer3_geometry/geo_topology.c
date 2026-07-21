/**
 * @file geo_topology.c
 * @brief Implementation of the geometric topology module.
 *
 * @details Implements simplicial complex operations including:
 *          - Creation/destruction with dynamic simplex arrays
 *          - Edge and triangle addition with canonical ordering
 *          - Euler characteristic computation (V - E + F)
 *          - Boundary operator for triangles
 *          - Connected component analysis via union-find
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include "lv00/geo_topology.h"


#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Internal helpers
 * ============================================================ */

/**
 * @brief Canonicalize edge vertex ordering (v0 < v1).
 */
static void canonicalize_edge(int *v0, int *v1) {
    if (*v0 > *v1) {
        int tmp = *v0;
        *v0 = *v1;
        *v1 = tmp;
    }
}

/**
 * @brief Canonicalize triangle vertex ordering (v0 < v1 < v2).
 */
static void canonicalize_triangle(int *v0, int *v1, int *v2) {
    /* Sort three values */
    if (*v0 > *v1) { int t = *v0; *v0 = *v1; *v1 = t; }
    if (*v1 > *v2) { int t = *v1; *v1 = *v2; *v2 = t; }
    if (*v0 > *v1) { int t = *v0; *v0 = *v1; *v1 = t; }
}

/**
 * @brief Check if an edge already exists in the edge array.
 */
static bool edge_exists(const Lv00Edge *edges, size_t n_edges, int v0, int v1) {
    for (size_t i = 0; i < n_edges; i++) {
        if (edges[i].v0 == v0 && edges[i].v1 == v1) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if a triangle already exists in the triangle array.
 */
static bool triangle_exists(const Lv00Triangle *triangles, size_t n_triangles,
                            int v0, int v1, int v2) {
    for (size_t i = 0; i < n_triangles; i++) {
        if (triangles[i].v0 == v0 && triangles[i].v1 == v1 && triangles[i].v2 == v2) {
            return true;
        }
    }
    return false;
}

/* ============================================================
 * Union-Find for connected components
 * ============================================================ */

/**
 * @brief Find the root of a set with path compression.
 */
static int uf_find(int *parent, int x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]]; /* Path compression */
        x = parent[x];
    }
    return x;
}

/**
 * @brief Union two sets.
 */
static void uf_union(int *parent, int *rank, int x, int y) {
    int rx = uf_find(parent, x);
    int ry = uf_find(parent, y);

    if (rx == ry) return;

    /* Union by rank */
    if (rank[rx] < rank[ry]) {
        parent[rx] = ry;
    } else if (rank[rx] > rank[ry]) {
        parent[ry] = rx;
    } else {
        parent[ry] = rx;
        rank[rx]++;
    }
}

/* ============================================================
 * API: Create
 * ============================================================ */

Lv00SimplicialComplex *geo_simplicial_create(int n_vertices) {
    if (n_vertices < 0) return NULL;

    Lv00SimplicialComplex *sc = (Lv00SimplicialComplex *)calloc(1, sizeof(Lv00SimplicialComplex));
    if (!sc) return NULL;

    sc->n_vertices  = n_vertices;
    sc->edges       = NULL;
    sc->n_edges     = 0;
    sc->triangles   = NULL;
    sc->n_triangles = 0;

    return sc;
}

/* ============================================================
 * API: Destroy
 * ============================================================ */

void geo_simplicial_destroy(Lv00SimplicialComplex *sc) {
    if (!sc) return;
    free(sc->edges);
    free(sc->triangles);
    free(sc);
}

/* ============================================================
 * API: Add edge
 * ============================================================ */

bool geo_simplicial_add_edge(Lv00SimplicialComplex *sc, int v0, int v1) {
    if (!sc) return false;
    if (v0 < 0 || v1 < 0 || v0 == v1) return false;
    if (v0 >= sc->n_vertices || v1 >= sc->n_vertices) return false;

    canonicalize_edge(&v0, &v1);

    /* Check for duplicates */
    if (edge_exists(sc->edges, sc->n_edges, v0, v1)) {
        return true; /* Already exists, not an error */
    }

    /* Grow edge array */
    size_t new_size = (sc->n_edges + 1) * sizeof(Lv00Edge);
    Lv00Edge *new_edges = (Lv00Edge *)realloc(sc->edges, new_size);
    if (!new_edges) return false;

    sc->edges = new_edges;
    sc->edges[sc->n_edges].v0 = v0;
    sc->edges[sc->n_edges].v1 = v1;
    sc->n_edges++;

    return true;
}

/* ============================================================
 * API: Add triangle
 * ============================================================ */

bool geo_simplicial_add_triangle(Lv00SimplicialComplex *sc, int v0, int v1, int v2) {
    if (!sc) return false;
    if (v0 < 0 || v1 < 0 || v2 < 0) return false;
    if (v0 == v1 || v1 == v2 || v0 == v2) return false;
    if (v0 >= sc->n_vertices || v1 >= sc->n_vertices || v2 >= sc->n_vertices) return false;

    canonicalize_triangle(&v0, &v1, &v2);

    /* Check for duplicates */
    if (triangle_exists(sc->triangles, sc->n_triangles, v0, v1, v2)) {
        return true;
    }

    /* Grow triangle array */
    size_t new_size = (sc->n_triangles + 1) * sizeof(Lv00Triangle);
    Lv00Triangle *new_triangles = (Lv00Triangle *)realloc(sc->triangles, new_size);
    if (!new_triangles) return false;

    sc->triangles = new_triangles;
    sc->triangles[sc->n_triangles].v0 = v0;
    sc->triangles[sc->n_triangles].v1 = v1;
    sc->triangles[sc->n_triangles].v2 = v2;
    sc->n_triangles++;

    /* Also add the three boundary edges */
    geo_simplicial_add_edge(sc, v0, v1);
    geo_simplicial_add_edge(sc, v1, v2);
    geo_simplicial_add_edge(sc, v0, v2);

    return true;
}

/* ============================================================
 * API: Euler characteristic
 * ============================================================ */

int geo_simplicial_euler_characteristic(const Lv00SimplicialComplex *sc) {
    if (!sc) return 0;

    /* chi = V - E + F */
    return sc->n_vertices - (int)sc->n_edges + (int)sc->n_triangles;
}

/* ============================================================
 * API: Boundary
 * ============================================================ */

Lv00Boundary *geo_simplicial_boundary(const Lv00SimplicialComplex *sc,
                                       const Lv00Triangle *tri) {
    (void)sc; /* Reserved for future use */

    if (!tri) return NULL;

    Lv00Boundary *bnd = (Lv00Boundary *)calloc(1, sizeof(Lv00Boundary));
    if (!bnd) return NULL;

    bnd->edges = (Lv00Edge *)calloc(3, sizeof(Lv00Edge));
    if (!bnd->edges) {
        free(bnd);
        return NULL;
    }
    bnd->n_edges = 3;

    /* Boundary of (v0, v1, v2) is {(v0,v1), (v1,v2), (v0,v2)} */
    bnd->edges[0].v0 = tri->v0;
    bnd->edges[0].v1 = tri->v1;
    bnd->edges[1].v0 = tri->v1;
    bnd->edges[1].v1 = tri->v2;
    bnd->edges[2].v0 = tri->v0;
    bnd->edges[2].v1 = tri->v2;

    bnd->vertices   = NULL;
    bnd->n_vertices = 0;

    return bnd;
}

void geo_simplicial_boundary_destroy(Lv00Boundary *boundary) {
    if (!boundary) return;
    free(boundary->edges);
    free(boundary->vertices);
    free(boundary);
}

/* ============================================================
 * API: Connected components
 * ============================================================ */

int geo_simplicial_connected_components(const Lv00SimplicialComplex *sc) {
    if (!sc || sc->n_vertices <= 0) return 0;

    int n = sc->n_vertices;

    /* Initialize union-find */
    int *parent = (int *)calloc((size_t)n, sizeof(int));
    int *rank   = (int *)calloc((size_t)n, sizeof(int));
    if (!parent || !rank) {
        free(parent);
        free(rank);
        return 0;
    }

    for (int i = 0; i < n; i++) {
        parent[i] = i;
        rank[i]   = 0;
    }

    /* Union all edges */
    for (size_t i = 0; i < sc->n_edges; i++) {
        uf_union(parent, rank, sc->edges[i].v0, sc->edges[i].v1);
    }

    /* Count distinct roots */
    int components = 0;
    for (int i = 0; i < n; i++) {
        if (parent[i] == i) {
            components++;
        }
    }

    free(parent);
    free(rank);
    return components;
}
