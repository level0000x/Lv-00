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

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "union_find_util.h"

/* ============================================================
 * Internal helpers
 * ============================================================ */

/**
 * @brief Canonicalize edge vertex ordering (v0 < v1).
 */
static void canonicalize_edge(int *v0, int *v1) {
    if (*v0 > *v1) {
        lv_SWAP(int, *v0, *v1);
    }
}

/**
 * @brief Canonicalize triangle vertex ordering (v0 < v1 < v2).
 */
static void canonicalize_triangle(int *v0, int *v1, int *v2) {
    /* Sort three values */
    if (*v0 > *v1) {
        lv_SWAP(int, *v0, *v1);
    }
    if (*v1 > *v2) {
        lv_SWAP(int, *v1, *v2);
    }
    if (*v0 > *v1) {
        lv_SWAP(int, *v0, *v1);
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

    /* Grow edge array（倍增扩容，消除逐边 +1 realloc 的 O(n^2) 拷贝；
     * 失败时 sc->edges / n_edges 保持不变，与原实现一致） */
    if (!lv_ensure_capacity((void **) &sc->edges, (int) sc->n_edges, (int *) &sc->edges_capacity,
                            sizeof(lvEdge), 1))
        return false;

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

    /* Grow triangle array（倍增扩容，消除逐三角形 +1 realloc 的 O(n^2) 拷贝；
     * 失败时 sc->triangles / n_triangles 保持不变，与原实现一致） */
    if (!lv_ensure_capacity((void **) &sc->triangles, (int) sc->n_triangles, (int *) &sc->triangles_capacity,
                            sizeof(lvTriangle), 1))
        return false;

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
                    lv_SWAP(int, v0, v1);
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

    /* Initialize union-find（共享工具 uf_create：parent[i]=i，rank 按秩合并） */
    int *rank = NULL;
    int *parent = uf_create(n, &rank);
    if (!parent)
        return 0;

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

    uf_destroy(parent, rank);
    return components;
}

/* ========================================================================
 * Legacy compatibility wrappers
 * ======================================================================== */

/**
 * @brief 计算欧拉示性数 chi = V - E + F，并验证曲面边界公式
 *
 * 对三角剖分曲面，2E = 3F + B（B 为边界边数），恒有 2E >= 3F；
 * 闭曲面（B == 0）时 chi = 2 - 2g（可定向，g 为亏格）或 2 - k（不可定向），
 * 故 chi <= 2。计数与上述公式冲突时视为无效输入，返回 -1 并置错误。
 *
 * @return 欧拉示性数 V - E + F；参数无效返回 -1
 */
int lv_euler_characteristic(int vertices, int edges, int faces) {
    if (vertices < 0 || edges < 0 || faces < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_euler_characteristic: negative counts");

    /* 曲面边界公式：边界边数 B = 2E - 3F（闭曲面 B == 0），必有 2E >= 3F */
    long boundary_edges = 2L * edges - 3L * faces;
    if (boundary_edges < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_euler_characteristic: inconsistent counts (2E < 3F)");

    int chi = vertices - edges + faces;

    /* 闭曲面（无边界）χ = 2 - 2g 或 2 - k，必有 χ <= 2 */
    if (boundary_edges == 0 && chi > 2)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_euler_characteristic: closed surface chi must be <= 2");

    return chi;
}

/**
 * @brief 校验 faces 是否构成单纯复形（顶点集/边集/面集一致性）
 *
 * 约定：faces 按 face_size = dim + 1 个顶点为一组平铺。
 *   - dim == 1：faces 为边列表（每条边 2 个顶点）
 *   - dim == 2：faces 为三角形列表（每个面 3 个顶点）
 *   - dim == 3：faces 为四面体列表（每个面 4 个顶点）
 *
 * 一致性检查：
 *   1. 顶点集：收集所有出现过的顶点，并校验索引非负、面内顶点互异；
 *   2. 面集/边集一致性：每个面的所有边（C(face_size,2) 条无向边）
 *      都必须位于由全体面导出的边集中；
 *   3. 边集无重复边（dim == 1 时输入边列表不允许重复出现同一无向边；
 *      更高维共享边属于正常情况，去重后即无重复边）；
 *   4. 边集/顶点集一致性：每条边的两个端点都必须位于顶点集中。
 *
 * @return 满足单纯复形一致性返回 1，否则返回 0
 */
int lv_is_simplicial_complex(const int *faces, size_t n_faces, size_t dim) {
    if (!faces || n_faces == 0)
        return 0;
    if (dim < 1 || dim > 3)
        return 0;

    const size_t face_size = dim + 1;

    /* 1) 顶点集：收集全部出现过的顶点，并校验索引非负 */
    int max_vertex = -1;
    size_t total = n_faces * face_size;
    size_t i;
    for (i = 0; i < total; i++) {
        if (faces[i] < 0)
            return 0;
        if (faces[i] > max_vertex)
            max_vertex = faces[i];
    }

    unsigned char *vertex_set = NULL;
    if (max_vertex >= 0) {
        vertex_set = (unsigned char *) lv_calloc((size_t) max_vertex + 1, sizeof(unsigned char));
        if (!vertex_set)
            return 0;
        for (i = 0; i < total; i++)
            vertex_set[faces[i]] = 1;
    }

    /* 面内顶点互异：退化单纯形（重复顶点/自环边）直接拒绝 */
    for (i = 0; i < n_faces; i++) {
        size_t a, b;
        for (a = 0; a < face_size; a++) {
            for (b = a + 1; b < face_size; b++) {
                if (faces[i * face_size + a] == faces[i * face_size + b]) {
                    lv_free((void **) &vertex_set);
                    return 0;
                }
            }
        }
    }

    /* 2) 边集：从所有面提取规范化的无向边并去重 */
    size_t pairs_per_face = face_size * (face_size - 1) / 2;
    size_t max_edges = n_faces * pairs_per_face;
    int *edge_set = (int *) lv_calloc(max_edges * 2, sizeof(int)); /* [v0,v1] 成对平铺 */
    if (!edge_set) {
        lv_free((void **) &vertex_set);
        return 0;
    }
    size_t n_edges = 0;
    /* dim == 1 时输入即边列表：同一无向边重复出现视为重复边 */
    int reject_dup_edge = (dim == 1);

    for (i = 0; i < n_faces; i++) {
        size_t a, b;
        for (a = 0; a < face_size; a++) {
            for (b = a + 1; b < face_size; b++) {
                int v0 = faces[i * face_size + a];
                int v1 = faces[i * face_size + b];
                canonicalize_edge(&v0, &v1);

                int found = -1;
                size_t k;
                for (k = 0; k < n_edges; k++) {
                    if (edge_set[k * 2] == v0 && edge_set[k * 2 + 1] == v1) {
                        found = (int) k;
                        break;
                    }
                }
                if (found >= 0) {
                    if (reject_dup_edge) {
                        lv_free((void **) &edge_set);
                        lv_free((void **) &vertex_set);
                        return 0;
                    }
                    continue;
                }
                edge_set[n_edges * 2] = v0;
                edge_set[n_edges * 2 + 1] = v1;
                n_edges++;
            }
        }
    }

    /* 3) 面集/边集一致性：每个面的每条边都必须位于边集中 */
    for (i = 0; i < n_faces; i++) {
        size_t a, b;
        for (a = 0; a < face_size; a++) {
            for (b = a + 1; b < face_size; b++) {
                int v0 = faces[i * face_size + a];
                int v1 = faces[i * face_size + b];
                canonicalize_edge(&v0, &v1);
                int found = 0;
                size_t k;
                for (k = 0; k < n_edges; k++) {
                    if (edge_set[k * 2] == v0 && edge_set[k * 2 + 1] == v1) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    lv_free((void **) &edge_set);
                    lv_free((void **) &vertex_set);
                    return 0;
                }
            }
        }
    }

    /* 4) 边集/顶点集一致性：每条边的两个端点都必须位于顶点集中 */
    for (i = 0; i < n_edges; i++) {
        if (!vertex_set[edge_set[i * 2]] || !vertex_set[edge_set[i * 2 + 1]]) {
            lv_free((void **) &edge_set);
            lv_free((void **) &vertex_set);
            return 0;
        }
    }

    lv_free((void **) &edge_set);
    lv_free((void **) &vertex_set);
    return 1;
}
