/*
 * @file geometry_compress_edgebreaker.c
 * @brief Geometry compression engine - edgebreaker CLERS encoding
 * @details Split from geometry_compress.c
 */

#include "lv/geometry_compress.h"
#include "geometry_compress_internal.h"

#include "lv/lv_file.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_heap.h"

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/node_deep_copy.h"
#include "lv/symbolic_coord.h"

/* ========================================================================
 * Edgebreaker CLERS 拓扑编码
 *
 * Rossignac 算法：将三角网格拓扑压缩为 C/L/E/R/S 五符号序列，
 * 通过边界栈遍历面片，根据邻接状态输出对应符号。
 * ======================================================================== */

/**
 * @brief Search for a vertex in the boundary stack
 *
 * @param[in] boundary      Boundary edge array
 * @param[in] boundary_top  Current boundary stack top
 * @param[in] vertex_id     Vertex ID to search for
 * @param[out] edge_index   Output: index of the boundary edge containing the vertex
 * @param[out] is_v0        Output: true if vertex is the v0 of the edge, false if v1
 * @return true if found, false otherwise
 */
static bool find_vertex_in_boundary(const Edge *boundary, int boundary_top, int vertex_id, int *edge_index,
                                    bool *is_v0) {
    for (int i = 0; i < boundary_top; i++) {
        if (boundary[i].v0 == vertex_id) {
            *edge_index = i;
            *is_v0 = true;
            return true;
        }
        if (boundary[i].v1 == vertex_id) {
            *edge_index = i;
            *is_v0 = false;
            return true;
        }
    }
    return false;
}

bool edgebreaker_encode(const ConstraintGraph *graph, EdgebreakerMode **modes, int *seq_len) {
    if (!graph || !modes || !seq_len)
        return false;

    /* Initialize CLERS sequence buffer */
    int capacity = CLERS_SEQUENCE_INITIAL;
    EdgebreakerMode *seq = (EdgebreakerMode *) lv_malloc(capacity * sizeof(EdgebreakerMode));
    if (!seq)
        return false;

    int len = 0;

    /* Boundary stack: simple array model */
    Edge *boundary = (Edge *) lv_malloc(BOUNDARY_STACK_INITIAL * sizeof(Edge));
    if (!boundary) {
        lv_free((void **) &seq);
        return false;
    }
    int boundary_top = 0;
    int boundary_capacity = BOUNDARY_STACK_INITIAL;

    /* Node visit markers */
    bool *visited = (bool *) lv_malloc(graph->node_count * sizeof(bool));
    if (!visited) {
        lv_free((void **) &seq);
        lv_free((void **) &boundary);
        return false;
    }
    memset(visited, 0, (size_t) graph->node_count * sizeof(bool));

    /* Find initial edge: take first two point-type nodes */
    int start_v0 = -1, start_v1 = -1;
    for (int i = 0; i < graph->node_count && start_v1 < 0; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;
        if (node->type == GEOM_POINT) {
            if (start_v0 < 0) {
                start_v0 = node->id;
                visited[node->id] = true;
            } else if (start_v1 < 0) {
                start_v1 = node->id;
                visited[node->id] = true;
            }
        }
    }

    if (start_v0 < 0 || start_v1 < 0) {
        /* Not enough geometry points, generate empty sequence */
        lv_free((void **) &seq);
        lv_free((void **) &boundary);
        lv_free((void **) &visited);
        *modes = NULL;
        *seq_len = 0;
        return true;
    }

    /* Push initial edge onto boundary stack */
    boundary[0].v0 = start_v0;
    boundary[0].v1 = start_v1;
    boundary_top = 1;

    /* Main traversal loop: queue-based traversal of constraint graph */
    while (boundary_top > 0) {
        boundary_top--;
        Edge cur = boundary[boundary_top];

        /* Find constraint associated with current edge (INCIDENCE/CONTAINMENT) */
        int constr_count = graph->constraint_count;
        bool found_opposite = false;

        for (int ci = 0; ci < constr_count && !found_opposite; ci++) {
            Constraint *c = graph->constraints[ci];
            if (!c)
                continue;

            /* Find constraint containing both cur.v0 and cur.v1 */
            bool has_v0 = false, has_v1 = false;
            int opposite_id = -1;

            for (int pi = 0; pi < c->participant_count; pi++) {
                int pid = c->participants[pi];
                if (pid == cur.v0)
                    has_v0 = true;
                else if (pid == cur.v1)
                    has_v1 = true;
                else
                    opposite_id = pid;
            }

            if (!has_v0 || !has_v1)
                continue;

            /* Found opposite vertex */
            found_opposite = true;

            if (opposite_id >= 0 && !visited[opposite_id]) {
                /* Opposite vertex unvisited -> C mode */
                if (len >= capacity) {
                    if (!lv_ensure_capacity((void **) &seq, len + 1, &capacity, sizeof(EdgebreakerMode), 0))
                        break;
                }
                seq[len++] = EDGEBREAKER_C;
                visited[opposite_id] = true;

                /* Push new edges onto boundary stack */
                if (boundary_top >= boundary_capacity) {
                    if (!lv_ensure_capacity((void **) &boundary, boundary_top + 1, &boundary_capacity, sizeof(Edge), 0))
                        break;
                }
                boundary[boundary_top].v0 = cur.v1;
                boundary[boundary_top].v1 = opposite_id;
                boundary_top++;
                boundary[boundary_top].v0 = opposite_id;
                boundary[boundary_top].v1 = cur.v0;
                boundary_top++;

            } else if (opposite_id >= 0) {
                /* Opposite vertex already visited -> need L/R/S classification */
                int opp_edge_index = -1;
                bool opp_is_v0 = false;
                bool opp_in_boundary =
                    find_vertex_in_boundary(boundary, boundary_top, opposite_id, &opp_edge_index, &opp_is_v0);

                if (!opp_in_boundary) {
                    /* Opposite vertex not in boundary -> S mode (split) */
                    if (len >= capacity) {
                        if (!lv_ensure_capacity((void **) &seq, len + 1, &capacity, sizeof(EdgebreakerMode), 0))
                            break;
                    }
                    seq[len++] = EDGEBREAKER_S;
                } else {
                    /* Check if opposite vertex is the next boundary vertex -> S mode */
                    if (boundary_top > 0) {
                        Edge next_edge = boundary[boundary_top - 1];
                        if (next_edge.v0 == opposite_id) {
                            /* Opposite vertex IS the next boundary vertex -> S mode */
                            if (len >= capacity) {
                                if (!lv_ensure_capacity((void **) &seq, len + 1, &capacity, sizeof(EdgebreakerMode), 0))
                                    break;
                            }
                            seq[len++] = EDGEBREAKER_S;
                        } else {
                            /* Determine LEFT or RIGHT based on position in boundary */
                            /*
                             * In the boundary stack, the current gate is (cur.v0, cur.v1).
                             * The LEFT side of the gate is the v1 side (top of stack grows upward).
                             * The RIGHT side is the v0 side.
                             *
                             * If the opposite vertex appears at a boundary edge where it is
                             * the v1 (end) of an edge, it is on the LEFT -> EDGEBREAKER_L.
                             * If it is the v0 (start) of an edge, it is on the RIGHT -> EDGEBREAKER_R.
                             */
                            if (opp_is_v0) {
                                /* Opposite is v0 of its boundary edge -> RIGHT side */
                                if (len >= capacity) {
                                    if (!lv_ensure_capacity((void **) &seq, len + 1, &capacity, sizeof(EdgebreakerMode), 0))
                                        break;
                                }
                                seq[len++] = EDGEBREAKER_R;
                            } else {
                                /* Opposite is v1 of its boundary edge -> LEFT side */
                                if (len >= capacity) {
                                    if (!lv_ensure_capacity((void **) &seq, len + 1, &capacity, sizeof(EdgebreakerMode), 0))
                                        break;
                                }
                                seq[len++] = EDGEBREAKER_L;
                            }
                        }
                    } else {
                        /* No more boundary edges -> default to L */
                        if (len >= capacity) {
                            if (!lv_ensure_capacity((void **) &seq, len + 1, &capacity, sizeof(EdgebreakerMode), 0))
                                break;
                        }
                        seq[len++] = EDGEBREAKER_L;
                    }
                }
            } else {
                /* No opposite vertex -> E mode */
                if (len >= capacity) {
                    if (!lv_ensure_capacity((void **) &seq, len + 1, &capacity, sizeof(EdgebreakerMode), 0))
                        break;
                }
                seq[len++] = EDGEBREAKER_E;
            }
        }

        if (!found_opposite) {
            /* No related constraint -> mark as E */
            if (len >= capacity) {
                if (!lv_ensure_capacity((void **) &seq, len + 1, &capacity, sizeof(EdgebreakerMode), 0))
                    break;
            }
            seq[len++] = EDGEBREAKER_E;
        }
    }

    lv_free((void **) &boundary);
    lv_free((void **) &visited);

    *modes = seq;
    *seq_len = len;
    return true;
}
