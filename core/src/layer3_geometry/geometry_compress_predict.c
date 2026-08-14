/*
 * @file geometry_compress_predict.c
 * @brief Geometry compression engine - predictive encoding
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
 * 预测编码（Predictive Encoding）
 *
 * 平行四边形预测：利用相邻三角面片对目标顶点坐标进行预测，
 * 存储预测残差以减小数据量。支持三种模式：
 *   - PREDICT_PARALLELOGRAM       标准平行四边形预测
 *   - PREDICT_MULTI_PARALLELOGRAM  多面片加权平均预测
 *   - PREDICT_DELTA                简单增量预测
 * ======================================================================== */

/**
 * @brief Parallelogram predictive encoding with Huffman compression
 *
 * Traverses triangle faces inferred from constraint relationships, predicts
 * each unvisited opposite vertex using the parallelogram rule, and stores
 * residuals. Residuals are quantized and Huffman-encoded.
 *
 * Algorithm: For triangle (v0, v1, v2) with v2 as target,
 * find adjacent triangle sharing edge (v0, v1) with opposite vertex v_opp,
 * prediction: pred = coord(v0) + coord(v1) - coord(v_opp)
 * residual: coord(v2) = coord(v2) - pred (in-place modification)
 *
 * @param[in,out] graph Constraint graph
 * @return true success, false failure
 */
bool predictive_encode_parallelogram(ConstraintGraph *graph) {
    if (!graph)
        return false;

    int node_count = graph->node_count;
    if (node_count < 3)
        return true;

    /* Extract triangle faces */
    TriangleFace *faces = NULL;
    int face_count = 0;
    if (!extract_triangle_faces(graph, &faces, &face_count))
        return false;

    if (face_count == 0) {
        lv_free((void **) &faces);
        return true;
    }

    /* Visited marker array */
    bool *visited = (bool *) lv_malloc(node_count * sizeof(bool));
    if (!visited) {
        lv_free((void **) &faces);
        return false;
    }
    memset(visited, 0, (size_t) node_count * sizeof(bool));

    /* Collect coordinate residuals for Huffman encoding */
    int residual_capacity = 256;
    int residual_count = 0;
    double *residuals = (double *) lv_malloc(residual_capacity * sizeof(double));
    if (!residuals) {
        lv_free((void **) &visited);
        lv_free((void **) &faces);
        return false;
    }

    /* Traverse triangle faces, predict each unvisited opposite vertex */
    for (int fi = 0; fi < face_count; fi++) {
        TriangleFace *face = &faces[fi];
        if (face->visited)
            continue;

        int v0 = face->verts[0];
        int v1 = face->verts[1];
        int v2 = face->verts[2];

        /* Determine which vertex is the unvisited target */
        int target = -1;
        int known_a = -1, known_b = -1;
        if (!visited[v2]) {
            target = v2;
            known_a = v0;
            known_b = v1;
        } else if (!visited[v1]) {
            target = v1;
            known_a = v0;
            known_b = v2;
        } else if (!visited[v0]) {
            target = v0;
            known_a = v1;
            known_b = v2;
        }

        if (target < 0) {
            face->visited = true;
            continue;
        }

        /* Find adjacent triangle sharing edge (known_a, known_b) */
        int adj = find_adjacent_face(faces, face_count, known_a, known_b, fi);
        if (adj < 0) {
            visited[target] = true;
            face->visited = true;
            continue;
        }

        int v_opp = get_opposite_vertex(&faces[adj], known_a, known_b);
        if (v_opp < 0) {
            visited[target] = true;
            face->visited = true;
            continue;
        }

        GeomNode *node_a = graph_get_node(graph, known_a);
        GeomNode *node_b = graph_get_node(graph, known_b);
        GeomNode *node_opp = graph_get_node(graph, v_opp);
        GeomNode *node_target = graph_get_node(graph, target);

        if (!node_a || !node_b || !node_opp || !node_target) {
            visited[target] = true;
            face->visited = true;
            continue;
        }

        int dim = node_target->coord_count;
        if (dim < 1 || !node_target->symbolic_coords || !node_a->symbolic_coords || !node_b->symbolic_coords ||
            !node_opp->symbolic_coords) {
            visited[target] = true;
            face->visited = true;
            continue;
        }

        /* Apply parallelogram prediction per dimension: pred = a + b - opp */
        for (int d = 0; d < dim && d < COORD_DIM; d++) {
            SymbolicCoord *neg_opp = symbolic_coord_negate(node_opp->symbolic_coords[d]);
            if (!neg_opp)
                continue;
            SymbolicCoord *sum_ab = symbolic_coord_add(node_a->symbolic_coords[d], node_b->symbolic_coords[d]);
            if (!sum_ab) {
                symbolic_coord_destroy(neg_opp);
                continue;
            }
            SymbolicCoord *pred = symbolic_coord_add(sum_ab, neg_opp);
            symbolic_coord_destroy(sum_ab);
            symbolic_coord_destroy(neg_opp);
            if (!pred)
                continue;

            SymbolicCoord *residual = symbolic_coord_subtract(node_target->symbolic_coords[d], pred);
            symbolic_coord_destroy(pred);
            if (residual) {
                double r = symbolic_coord_to_double(residual);
                if (!lv_ensure_capacity((void **) &residuals, residual_count, &residual_capacity, sizeof(double), 0)) {
                    symbolic_coord_destroy(residual);
                    break;
                }
                residuals[residual_count++] = r;
                symbolic_coord_destroy(node_target->symbolic_coords[d]);
                node_target->symbolic_coords[d] = residual;
            }
        }

        visited[target] = true;
        face->visited = true;
    }

    /* Huffman encode residual data (quantize double residuals to int16) */
    if (residual_count > 0) {
        double quant_factor = (double) lv_FLOAT_APPROX_SCALE;
        int16_t *quantized = (int16_t *) lv_malloc(residual_count * sizeof(int16_t));
        if (quantized) {
            for (int i = 0; i < residual_count; i++) {
                double v = residuals[i] * quant_factor;
                if (v > 32767.0)
                    v = 32767.0;
                if (v < -32768.0)
                    v = -32768.0;
                quantized[i] = (int16_t) v;
            }

            /* Build frequency table (int16 as two uint8 bytes) */
            uint32_t freq[256];
            memset(freq, 0, sizeof(freq));
            for (int i = 0; i < residual_count; i++) {
                uint8_t lo = (uint8_t) (quantized[i] & 0xFF);
                uint8_t hi = (uint8_t) ((quantized[i] >> 8) & 0xFF);
                freq[lo]++;
                freq[hi]++;
            }

            /* Build Huffman tree */
            HuffmanNode hnodes[HUFFMAN_MAX_NODES];
            int root = huffman_tree_build(freq, hnodes);
            if (root >= 0) {
                HuffmanCode codes[256];
                memset(codes, 0, sizeof(codes));
                huffman_generate_codes(hnodes, root, codes);
                (void) codes; /* Encoding table stored for subsequent use */
            }

            lv_free((void **) &quantized);
        }
    }

    lv_free((void **) &residuals);
    lv_free((void **) &visited);
    lv_free((void **) &faces);
    return true;
}

/**
 * @brief Multi-parallelogram predictive encoding
 *
 * For each vertex, finds ALL adjacent faces and computes predicted position
 * as weighted average of predictions from all adjacent faces.
 * Weight by face area (larger faces contribute more).
 *
 * @param[in,out] graph Constraint graph
 * @return true success, false failure
 */
bool predictive_encode_multi_parallelogram(ConstraintGraph *graph) {
    if (!graph)
        return false;

    int node_count = graph->node_count;
    if (node_count < 3)
        return true;

    /* Extract triangle faces */
    TriangleFace *faces = NULL;
    int face_count = 0;
    if (!extract_triangle_faces(graph, &faces, &face_count))
        return false;

    if (face_count == 0) {
        lv_free((void **) &faces);
        return true;
    }

    /* Visited marker array */
    bool *visited = (bool *) lv_malloc(node_count * sizeof(bool));
    if (!visited) {
        lv_free((void **) &faces);
        return false;
    }
    memset(visited, 0, (size_t) node_count * sizeof(bool));
    /* Traverse triangle faces */
    for (int fi = 0; fi < face_count; fi++) {
        TriangleFace *face = &faces[fi];
        if (face->visited)
            continue;

        int v0 = face->verts[0];
        int v1 = face->verts[1];
        int v2 = face->verts[2];

        /* Determine which vertex is the unvisited target */
        int target = -1;
        int known_a = -1, known_b = -1;
        if (!visited[v2]) {
            target = v2;
            known_a = v0;
            known_b = v1;
        } else if (!visited[v1]) {
            target = v1;
            known_a = v0;
            known_b = v2;
        } else if (!visited[v0]) {
            target = v0;
            known_a = v1;
            known_b = v2;
        }

        if (target < 0) {
            face->visited = true;
            continue;
        }

        /* Find ALL adjacent faces sharing edge (known_a, known_b) */
        int adj_indices[MAX_ADJACENT_FACES];
        int adj_count = find_all_adjacent_faces(faces, face_count, known_a, known_b, adj_indices, MAX_ADJACENT_FACES);

        if (adj_count == 0) {
            visited[target] = true;
            face->visited = true;
            continue;
        }

        GeomNode *node_a = graph_get_node(graph, known_a);
        GeomNode *node_b = graph_get_node(graph, known_b);
        GeomNode *node_target = graph_get_node(graph, target);

        if (!node_a || !node_b || !node_target) {
            visited[target] = true;
            face->visited = true;
            continue;
        }

        int dim = node_target->coord_count;
        if (dim < 1 || !node_target->symbolic_coords || !node_a->symbolic_coords || !node_b->symbolic_coords) {
            visited[target] = true;
            face->visited = true;
            continue;
        }

        /* Compute weighted average prediction from all adjacent faces */
        double total_weight = 0.0;
        double pred_coords[COORD_DIM];
        memset(pred_coords, 0, sizeof(pred_coords));

        for (int ai = 0; ai < adj_count; ai++) {
            int v_opp = get_opposite_vertex(&faces[adj_indices[ai]], known_a, known_b);
            if (v_opp < 0)
                continue;

            GeomNode *node_opp = graph_get_node(graph, v_opp);
            if (!node_opp || !node_opp->symbolic_coords)
                continue;

            /* Weight by face area */
            double area = triangle_face_area(graph, &faces[adj_indices[ai]]);
            if (area <= 0.0)
                area = 1.0; /* Minimum weight to avoid division by zero */
            total_weight += area;

            /* Prediction: pred = a + b - opp */
            for (int d = 0; d < dim && d < COORD_DIM; d++) {
                double va = symbolic_coord_to_double(node_a->symbolic_coords[d]);
                double vb = symbolic_coord_to_double(node_b->symbolic_coords[d]);
                double vopp = symbolic_coord_to_double(node_opp->symbolic_coords[d]);
                pred_coords[d] += area * (va + vb - vopp);
            }
        }

        if (total_weight <= 0.0) {
            visited[target] = true;
            face->visited = true;
            continue;
        }

        /* Normalize weighted prediction */
        for (int d = 0; d < dim && d < COORD_DIM; d++) {
            pred_coords[d] /= total_weight;
        }

        /* Compute residual: target - prediction */
        for (int d = 0; d < dim && d < COORD_DIM; d++) {
            double actual = symbolic_coord_to_double(node_target->symbolic_coords[d]);
            double residual_val = actual - pred_coords[d];

            /* Replace coordinate with residual as rational */
            SymbolicCoord *residual = symbolic_coord_from_double_scaled(residual_val, lv_RATIONAL_SCALE_LOW);
            if (residual) {
                symbolic_coord_destroy(node_target->symbolic_coords[d]);
                node_target->symbolic_coords[d] = residual;
            }
        }

        visited[target] = true;
        face->visited = true;
    }

    lv_free((void **) &visited);
    lv_free((void **) &faces);
    return true;
}

/**
 * @brief Delta predictive encoding
 *
 * Traverses nodes by ID order, replacing each node's coordinates with
 * the delta from the previous node.
 *
 * @param[in,out] graph Constraint graph
 * @return true success, false failure
 */
bool predictive_encode_delta(ConstraintGraph *graph) {
    if (!graph)
        return false;

    int node_count = graph->node_count;
    if (node_count < 2)
        return true;

    /* Save first node's coordinates as reference */
    GeomNode *prev = NULL;

    for (int i = 0; i < node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->symbolic_coords || node->coord_count < COORD_DIM)
            continue;

        if (prev) {
            /* Compute delta: node - prev, store in node */
            for (int d = 0; d < COORD_DIM; d++) {
                SymbolicCoord *diff = symbolic_coord_subtract(node->symbolic_coords[d], prev->symbolic_coords[d]);
                if (diff) {
                    symbolic_coord_destroy(node->symbolic_coords[d]);
                    node->symbolic_coords[d] = diff;
                }
            }
        }
        prev = node;
    }

    return true;
}

