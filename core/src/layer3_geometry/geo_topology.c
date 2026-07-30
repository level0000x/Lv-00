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

#include "lv/geo_topology.h"

#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"

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
    if (*v0 > *v1) {
        int t = *v0;
        *v0 = *v1;
        *v1 = t;
    }
    if (*v1 > *v2) {
        int t = *v1;
        *v1 = *v2;
        *v2 = t;
    }
    if (*v0 > *v1) {
        int t = *v0;
        *v0 = *v1;
        *v1 = t;
    }
}

/**
 * @brief Check if an edge already exists in the edge array.
 */
static bool edge_exists(const lvEdge *edges, size_t n_edges, int v0, int v1) {
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
static bool triangle_exists(const lvTriangle *triangles, size_t n_triangles, int v0, int v1, int v2) {
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

    if (rx == ry)
        return;

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

lvSimplicialComplex *geo_simplicial_create(int n_vertices) {
    if (n_vertices < 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "geo_simplicial_create: n_vertices < 0");

    lvSimplicialComplex *sc = (lvSimplicialComplex *) lv_calloc(1, sizeof(lvSimplicialComplex));
    if (!sc)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "geo_simplicial_create: calloc sc failed");

    sc->n_vertices = n_vertices;
    sc->edges = NULL;
    sc->n_edges = 0;
    sc->triangles = NULL;
    sc->n_triangles = 0;

    return sc;
}

/* ============================================================
 * API: Destroy
 * ============================================================ */

void geo_simplicial_destroy(lvSimplicialComplex *sc) {
    if (!sc)
        return;
    lv_free((void **) &(sc->edges));
    lv_free((void **) &(sc->triangles));
    lv_free((void **) &(sc));
}

/* ============================================================
 * API: Add edge
 * ============================================================ */

bool geo_simplicial_add_edge(lvSimplicialComplex *sc, int v0, int v1) {
    if (!sc)
        return false;
    if (v0 < 0 || v1 < 0 || v0 == v1)
        return false;
    if (v0 >= sc->n_vertices || v1 >= sc->n_vertices)
        return false;

    canonicalize_edge(&v0, &v1);

    /* Check for duplicates */
    if (edge_exists(sc->edges, sc->n_edges, v0, v1)) {
        return true; /* Already exists, not an error */
    }

    /* Grow edge array */
    size_t new_size = (sc->n_edges + 1) * sizeof(lvEdge);
    lvEdge *new_edges = (lvEdge *) lv_realloc(sc->edges, new_size);
    if (!new_edges)
        return false;

    sc->edges = new_edges;
    sc->edges[sc->n_edges].v0 = v0;
    sc->edges[sc->n_edges].v1 = v1;
    sc->n_edges++;

    return true;
}

/* ============================================================
 * API: Add triangle
 * ============================================================ */

bool geo_simplicial_add_triangle(lvSimplicialComplex *sc, int v0, int v1, int v2) {
    if (!sc)
        return false;
    if (v0 < 0 || v1 < 0 || v2 < 0)
        return false;
    if (v0 == v1 || v1 == v2 || v0 == v2)
        return false;
    if (v0 >= sc->n_vertices || v1 >= sc->n_vertices || v2 >= sc->n_vertices)
        return false;

    canonicalize_triangle(&v0, &v1, &v2);

    /* Check for duplicates */
    if (triangle_exists(sc->triangles, sc->n_triangles, v0, v1, v2)) {
        return true;
    }

    /* Grow triangle array */
    size_t new_size = (sc->n_triangles + 1) * sizeof(lvTriangle);
    lvTriangle *new_triangles = (lvTriangle *) lv_realloc(sc->triangles, new_size);
    if (!new_triangles)
        return false;

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

int geo_simplicial_euler_characteristic(const lvSimplicialComplex *sc) {
    if (!sc)
        return 0;

    /* chi = V - E + F */
    return sc->n_vertices - (int) sc->n_edges + (int) sc->n_triangles;
}

/* ============================================================
 * API: Boundary
 * ============================================================ */

lvBoundary *geo_simplicial_boundary(const lvSimplicialComplex *sc, const lvTriangle *tri) {
    if (!tri && (!sc || sc->n_triangles == 0))
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "geo_simplicial_boundary: NULL params or empty complex");

    lvBoundary *bnd = (lvBoundary *) lv_calloc(1, sizeof(lvBoundary));
    if (!bnd)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "geo_simplicial_boundary: calloc bnd failed");

    bnd->edges = NULL;
    bnd->n_edges = 0;
    bnd->vertices = NULL;
    bnd->n_vertices = 0;

    if (sc && sc->n_triangles > 0) {
        /* 遍历复形 sc 中的所有三角形，提取所有唯一的边 */
        size_t max_edges = sc->n_triangles * 3;
        lvEdge *tmp_edges = (lvEdge *) lv_calloc(max_edges, sizeof(lvEdge));
        if (!tmp_edges) {
            lv_free((void **) &(bnd));
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "geo_simplicial_boundary: calloc tmp_edges failed");
        }
        size_t n_unique = 0;

        for (size_t i = 0; i < sc->n_triangles; i++) {
            int tri_verts[3] = {sc->triangles[i].v0, sc->triangles[i].v1, sc->triangles[i].v2};
            /* 每个三角形贡献三条边 */
            int edge_pairs[3][2] = {
                {tri_verts[0], tri_verts[1]},
                {tri_verts[1], tri_verts[2]},
                {tri_verts[2], tri_verts[0]}
            };
            for (int e = 0; e < 3; e++) {
                int v0 = edge_pairs[e][0];
                int v1 = edge_pairs[e][1];
                /* 规范化边顺序 */
                if (v0 > v1) {
                    int t = v0; v0 = v1; v1 = t;
                }
                /* 去重 */
                bool dup = false;
                for (size_t k = 0; k < n_unique; k++) {
                    if (tmp_edges[k].v0 == v0 && tmp_edges[k].v1 == v1) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) {
                    tmp_edges[n_unique].v0 = v0;
                    tmp_edges[n_unique].v1 = v1;
                    n_unique++;
                }
            }
        }

        bnd->edges = tmp_edges;
        bnd->n_edges = n_unique;

        /* 收集所有唯一顶点 */
        size_t max_verts = (size_t) sc->n_vertices;
        int *vert_set = (int *) lv_calloc(max_verts, sizeof(int));
        if (vert_set) {
            size_t nv = 0;
            for (size_t i = 0; i < n_unique; i++) {
                int found_v0 = 0, found_v1 = 0;
                for (size_t j = 0; j < nv; j++) {
                    if (vert_set[j] == tmp_edges[i].v0) found_v0 = 1;
                    if (vert_set[j] == tmp_edges[i].v1) found_v1 = 1;
                }
                if (!found_v0) vert_set[nv++] = tmp_edges[i].v0;
                if (!found_v1) vert_set[nv++] = tmp_edges[i].v1;
            }
            bnd->vertices = (int *) lv_calloc(nv, sizeof(int));
            if (bnd->vertices) {
                memcpy(bnd->vertices, vert_set, nv * sizeof(int));
                bnd->n_vertices = (int) nv;
            }
            lv_free((void **) &(vert_set));
        }
    } else if (tri) {
        /* 兼容旧用法：仅处理单个三角形 */
        bnd->edges = (lvEdge *) lv_calloc(3, sizeof(lvEdge));
        if (!bnd->edges) {
            lv_free((void **) &(bnd));
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "geo_simplicial_boundary: calloc edges failed");
        }
        bnd->n_edges = 3;

        bnd->edges[0].v0 = tri->v0;
        bnd->edges[0].v1 = tri->v1;
        bnd->edges[1].v0 = tri->v1;
        bnd->edges[1].v1 = tri->v2;
        bnd->edges[2].v0 = tri->v0;
        bnd->edges[2].v1 = tri->v2;
    }

    return bnd;
}

void geo_simplicial_boundary_destroy(lvBoundary *boundary) {
    if (!boundary)
        return;
    lv_free((void **) &(boundary->edges));
    lv_free((void **) &(boundary->vertices));
    lv_free((void **) &(boundary));
}

/* ============================================================
 * API: Connected components
 * ============================================================ */

int geo_simplicial_connected_components(const lvSimplicialComplex *sc) {
    if (!sc || sc->n_vertices <= 0)
        return 0;

    int n = sc->n_vertices;

    /* Initialize union-find */
    int *parent = (int *) lv_calloc((size_t) n, sizeof(int));
    int *rank = (int *) lv_calloc((size_t) n, sizeof(int));
    if (!parent || !rank) {
        lv_free((void **) &(parent));
        lv_free((void **) &(rank));
        return 0;
    }

    for (int i = 0; i < n; i++) {
        parent[i] = i;
        rank[i] = 0;
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

    lv_free((void **) &(parent));
    lv_free((void **) &(rank));
    return components;
}

/* ========================================================================
 * Legacy stub implementations
 * ======================================================================== */

int lv_euler_characteristic(int vertices, int edges, int faces) {
    return vertices - edges + faces;
}

int lv_is_simplicial_complex(const int *faces, size_t n_faces, size_t dim) {
    if (!faces || n_faces == 0)
        return 0;
    if (dim < 1 || dim > 3)
        return 0;

    /* Verify all faces have valid vertex indices (non-negative) */
    size_t face_size = dim + 1;
    for (size_t i = 0; i < n_faces; i++) {
        for (size_t j = 0; j < face_size; j++) {
            if (faces[i * face_size + j] < 0)
                return 0;
        }
    }

    /* For dim == 2 (triangles), verify simplicial complex properties */
    if (dim == 2) {
        /*
         * 收集所有边并计数每条边被多少个三角形共享。
         * 单纯复形条件：每条边至多被 2 个三角形共享。
         */
        size_t max_edges = n_faces * 3;
        int *edge_data = (int *) calloc(max_edges * 3, sizeof(int));
        /* edge_data[k*3 + 0] = v0, edge_data[k*3 + 1] = v1, edge_data[k*3 + 2] = triangle_count */
        if (!edge_data)
            return 0;
        size_t n_edges = 0;

        for (size_t i = 0; i < n_faces; i++) {
            int tri[3] = {faces[i * 3], faces[i * 3 + 1], faces[i * 3 + 2]};
            int edge_pairs[3][2] = {{tri[0], tri[1]}, {tri[1], tri[2]}, {tri[2], tri[0]}};

            for (int e = 0; e < 3; e++) {
                int v0 = edge_pairs[e][0];
                int v1 = edge_pairs[e][1];
                if (v0 > v1) {
                    int t = v0; v0 = v1; v1 = t;
                }

                /* 查找或添加边 */
                int idx = -1;
                for (size_t k = 0; k < n_edges; k++) {
                    if (edge_data[k * 3] == v0 && edge_data[k * 3 + 1] == v1) {
                        idx = (int) k;
                        break;
                    }
                }

                if (idx < 0) {
                    /* 新边 */
                    edge_data[n_edges * 3] = v0;
                    edge_data[n_edges * 3 + 1] = v1;
                    edge_data[n_edges * 3 + 2] = 1;
                    n_edges++;
                } else {
                    /* 已有边，递增计数 */
                    edge_data[idx * 3 + 2]++;
                    /* 单纯复形条件：每条边至多被 2 个三角形共享 */
                    if (edge_data[idx * 3 + 2] > 2) {
                        free(edge_data);
                        return 0;
                    }
                }
            }
        }

        free(edge_data);
    }

    /*
     * 对于 dim == 1（线段），每条边（即线段本身）至多出现一次（无重复）。
     * 对于 dim == 3（四面体），每条边至多被多个四面体共享，
     * 此处暂不实现完整的 3-复形验证。
     */

    return 1;
}
