/*
 * @file geometry_compress_triangle.c
 * @brief Geometry compression engine - triangle face extraction
 * @details Split from geometry_compress.c
 */

#include "geometry_compress.h"
#include "geometry_compress_internal.h"

#include "lv/lv_file.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_heap.h"
#include "lv/geo_utils.h"

#include "lv_internal.h"
#include "lv_utils.h"
#include "node_deep_copy.h"
#include "symbolic_coord.h"

/* ========================================================================
 * 三角面片提取（Triangle Face Extraction）
 *
 * 从约束图中识别 3-participant 约束作为三角面片，
 * 支持邻接面查找和面积计算。
 * ======================================================================== */

/**
 * @brief Extract triangle face list from constraint graph
 *
 * Iterates all constraints, finding those with exactly 3 participants (treated as triangle faces).
 *
 * @param[in]  graph       Constraint graph
 * @param[out] faces       Output triangle face array (caller responsible for free)
 * @param[out] face_count  Output triangle face count
 * @return true success, false failure
 */
bool extract_triangle_faces(const ConstraintGraph *graph, TriangleFace **faces, int *face_count) {
    if (!graph || !faces || !face_count)
        return false;

    int capacity = 64;
    TriangleFace *f = (TriangleFace *) lv_malloc(capacity * sizeof(TriangleFace));
    if (!f)
        return false;

    int count = 0;
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (!c)
            continue;
        if (c->participant_count == 3) {
            if (!lv_ensure_capacity((void **) &f, count, &capacity, sizeof(TriangleFace), 0)) {
                lv_free((void **) &f);
                return false;
            }
            f[count].verts[0] = c->participants[0];
            f[count].verts[1] = c->participants[1];
            f[count].verts[2] = c->participants[2];
            f[count].visited = false;
            count++;
        }
    }

    *faces = f;
    *face_count = count;
    return true;
}

/**
 * @brief Find adjacent triangle face sharing the specified edge (excluding given face)
 *
 * @param[in] faces      Triangle face array
 * @param[in] face_count Triangle face count
 * @param[in] v0, v1     Two vertices of the shared edge
 * @param[in] exclude    Face index to exclude (-1 = none)
 * @return Matching face index, -1 if not found
 */
int find_adjacent_face(const TriangleFace *faces, int face_count, int v0, int v1, int exclude) {
    for (int i = 0; i < face_count; i++) {
        if (i == exclude)
            continue;
        bool has_v0 = false, has_v1 = false;
        for (int k = 0; k < 3; k++) {
            if (faces[i].verts[k] == v0)
                has_v0 = true;
            if (faces[i].verts[k] == v1)
                has_v1 = true;
        }
        if (has_v0 && has_v1)
            return i;
    }
    return -1;
}

/**
 * @brief Find ALL adjacent triangle faces sharing the specified edge
 *
 * @param[in]  faces         Triangle face array
 * @param[in]  face_count    Triangle face count
 * @param[in]  v0, v1        Two vertices of the shared edge
 * @param[out] adj_indices   Output array of adjacent face indices
 * @param[in]  max_adj       Maximum number of adjacent faces to return
 * @return Number of adjacent faces found
 */
int find_all_adjacent_faces(const TriangleFace *faces, int face_count, int v0, int v1, int *adj_indices,
                                   int max_adj) {
    int count = 0;
    for (int i = 0; i < face_count && count < max_adj; i++) {
        bool has_v0 = false, has_v1 = false;
        for (int k = 0; k < 3; k++) {
            if (faces[i].verts[k] == v0)
                has_v0 = true;
            if (faces[i].verts[k] == v1)
                has_v1 = true;
        }
        if (has_v0 && has_v1) {
            adj_indices[count++] = i;
        }
    }
    return count;
}

/**
 * @brief Get the opposite vertex in a triangle face (not equal to v0 and v1)
 */
int get_opposite_vertex(const TriangleFace *face, int v0, int v1) {
    for (int k = 0; k < 3; k++) {
        if (face->verts[k] != v0 && face->verts[k] != v1)
            return face->verts[k];
    }
    return -1;
}

/**
 * @brief Compute triangle face area based on vertex coordinates
 *
 * @param[in] graph Constraint graph
 * @param[in] face  Triangle face
 * @return Area (absolute value), 0.0 if vertices invalid
 */
double triangle_face_area(const ConstraintGraph *graph, const TriangleFace *face) {
    if (!graph || !face)
        return 0.0;

    GeomNode *n0 = graph_get_node(graph, face->verts[0]);
    GeomNode *n1 = graph_get_node(graph, face->verts[1]);
    GeomNode *n2 = graph_get_node(graph, face->verts[2]);

    if (!n0 || !n1 || !n2)
        return 0.0;
    if (!n0->symbolic_coords || !n1->symbolic_coords || !n2->symbolic_coords)
        return 0.0;
    if (n0->coord_count < 2 || n1->coord_count < 2 || n2->coord_count < 2)
        return 0.0;

    double x0 = symbolic_coord_to_double(n0->symbolic_coords[0]);
    double y0 = symbolic_coord_to_double(n0->symbolic_coords[1]);
    double x1 = symbolic_coord_to_double(n1->symbolic_coords[0]);
    double y1 = symbolic_coord_to_double(n1->symbolic_coords[1]);
    double x2 = symbolic_coord_to_double(n2->symbolic_coords[0]);
    double y2 = symbolic_coord_to_double(n2->symbolic_coords[1]);

    /* Area = 0.5 * |cross product| */
    double cross = geo_signed_area_2x(x0, y0, x1, y1, x2, y2);
    return cross < 0 ? -cross : cross;
}

