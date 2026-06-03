/**
 * @file geo_topology.h
 * @brief Geometric topology module for simplicial complexes.
 *
 * @details Provides data structures and algorithms for working with
 *          simplicial complexes, including:
 *          - Creation and destruction of simplicial complexes
 *          - Euler characteristic computation
 *          - Boundary operator
 *          - Connected component analysis
 *
 *          Inspired by GUDHI (computational topology), GeometryCentral
 *          (discrete differential geometry), and Polyscope
 *          (structure/quantity visualization).
 *
 *          A simplicial complex is represented by its vertices (0-simplices),
 *          edges (1-simplices), and triangles (2-simplices).
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#ifndef LV00_GEO_TOPOLOGY_H
#define LV00_GEO_TOPOLOGY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv00.h"

/* ============================================================
 * Edge structure (1-simplex)
 * ============================================================ */

/**
 * @brief Represents an edge as a pair of vertex indices.
 *
 * Vertices are ordered such that v0 < v1 for canonical representation.
 */
typedef struct Lv00Edge {
    int v0; /**< First vertex index */
    int v1; /**< Second vertex index */
} Lv00Edge;

/* ============================================================
 * Triangle structure (2-simplex)
 * ============================================================ */

/**
 * @brief Represents a triangle as a triple of vertex indices.
 *
 * Vertices are ordered such that v0 < v1 < v2 for canonical representation.
 */
typedef struct Lv00Triangle {
    int v0; /**< First vertex index */
    int v1; /**< Second vertex index */
    int v2; /**< Third vertex index */
} Lv00Triangle;

/* ============================================================
 * Simplicial complex
 * ============================================================ */

/**
 * @brief A simplicial complex consisting of vertices, edges, and triangles.
 *
 * The complex is stored as explicit arrays of simplices. Vertices are
 * implicitly represented by their indices (0 to n_vertices-1).
 */
typedef struct Lv00SimplicialComplex {
    int          n_vertices; /**< Number of vertices (0-simplices) */
    Lv00Edge    *edges;      /**< Array of edges (1-simplices) */
    size_t       n_edges;    /**< Number of edges */
    Lv00Triangle *triangles; /**< Array of triangles (2-simplices) */
    size_t       n_triangles; /**< Number of triangles */
} Lv00SimplicialComplex;

/* ============================================================
 * Boundary result
 * ============================================================ */

/**
 * @brief Result of a boundary operation.
 *
 * For a triangle, the boundary is a set of 3 edges.
 * For an edge, the boundary is a set of 2 vertices.
 */
typedef struct Lv00Boundary {
    Lv00Edge    *edges;      /**< Boundary edges (for triangle boundary) */
    size_t       n_edges;    /**< Number of boundary edges */
    int         *vertices;   /**< Boundary vertices (for edge boundary) */
    size_t       n_vertices; /**< Number of boundary vertices */
} Lv00Boundary;

/* ============================================================
 * API: Create / Destroy
 * ============================================================ */

/**
 * @brief Create a simplicial complex with the given number of vertices.
 *
 * @param n_vertices  Number of vertices (must be >= 0)
 * @return Pointer to the new complex, or NULL on failure
 */
LV00_PUBLIC_API Lv00SimplicialComplex *geo_simplicial_create(int n_vertices);

/**
 * @brief Destroy a simplicial complex and free all associated memory.
 *
 * @param sc  The complex to destroy (may be NULL)
 */
LV00_PUBLIC_API void geo_simplicial_destroy(Lv00SimplicialComplex *sc);

/* ============================================================
 * API: Add simplices
 * ============================================================ */

/**
 * @brief Add an edge to the simplicial complex.
 *
 * @param sc  The simplicial complex
 * @param v0  First vertex index
 * @param v1  Second vertex index
 * @return true on success, false on failure
 */
LV00_PUBLIC_API bool geo_simplicial_add_edge(Lv00SimplicialComplex *sc, int v0, int v1);

/**
 * @brief Add a triangle to the simplicial complex.
 *
 * @param sc  The simplicial complex
 * @param v0  First vertex index
 * @param v1  Second vertex index
 * @param v2  Third vertex index
 * @return true on success, false on failure
 */
LV00_PUBLIC_API bool geo_simplicial_add_triangle(Lv00SimplicialComplex *sc,
                                                  int v0, int v1, int v2);

/* ============================================================
 * API: Topological invariants
 * ============================================================ */

/**
 * @brief Compute the Euler characteristic of the simplicial complex.
 *
 * The Euler characteristic is defined as:
 *   chi = V - E + F
 * where V = number of vertices, E = number of edges, F = number of triangles.
 *
 * @param sc  The simplicial complex
 * @return The Euler characteristic (int), or 0 on error
 */
LV00_PUBLIC_API int geo_simplicial_euler_characteristic(const Lv00SimplicialComplex *sc);

/* ============================================================
 * API: Boundary operator
 * ============================================================ */

/**
 * @brief Compute the boundary of a triangle.
 *
 * The boundary of triangle (v0, v1, v2) is the set of three edges:
 * (v0, v1), (v1, v2), (v0, v2).
 *
 * @param sc  The simplicial complex (unused but kept for API consistency)
 * @param tri The triangle
 * @return Pointer to the boundary result, or NULL on failure
 */
LV00_PUBLIC_API Lv00Boundary *geo_simplicial_boundary(const Lv00SimplicialComplex *sc,
                                                       const Lv00Triangle *tri);

/**
 * @brief Destroy a boundary result.
 *
 * @param boundary  The boundary to destroy (may be NULL)
 */
LV00_PUBLIC_API void geo_simplicial_boundary_destroy(Lv00Boundary *boundary);

/* ============================================================
 * API: Connected components
 * ============================================================ */

/**
 * @brief Compute the number of connected components of the 1-skeleton.
 *
 * Uses a union-find (disjoint set) algorithm on the graph formed by
 * vertices and edges.
 *
 * @param sc  The simplicial complex
 * @return Number of connected components, or 0 on error
 */
LV00_PUBLIC_API int geo_simplicial_connected_components(const Lv00SimplicialComplex *sc);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_TOPOLOGY_H */
