/* ============================================================================
 * 模块名称：几何数据压缩引擎 (geometry_compress)
 *
 * 功能概述：
 *   类 Draco 风格的几何网格数据压缩——基于 Edgebreaker CLERS 算法的
 *   拓扑编码 + 平行四边形预测编码 + 熵编码 + .lvzd 二进制文件 I/O。
 *
 * 核心流水线：
 *   geometry_compress()   压缩流水线：拓扑编码 → 坐标预测 → 熵编码 → 二进制打包
 *   geometry_decompress()  解压流水线：二进制解析 → 熵解码 → 坐标还原 → 拓扑重建
 *
 * 内部模块：
 *   - edgebreaker_encode()           CLERS 符号序列生成（网格拓扑编码）
 *   - predictive_encode_coords()     坐标预测编码（平行四边形 / 多平行四边形 / 增量）
 *   - 熵编码器                         Huffman + RLE 自适应选择
 *   - .lvzd I/O                       二进制文件读写（小端序）
 *
 * 数据结构：
 *   - TriangleFace                    三角面片（从约束图中提取 3-participant 约束）
 *   - HuffmanNode/MinHeap/HuffmanCode  Huffman 编码基础设施
 *   - BitWriter/BitReader             位级 I/O 工具
 *
 * 设计文档参考：Section 3.5 几何内核 · 网格压缩
 *
 * ============================================================================ */

#include "geometry_compress.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "node_deep_copy.h"
#include "symbolic_coord.h"

/* ========================================================================
 * Internal constants
 * ======================================================================== */

/** Boundary stack initial capacity */
#define BOUNDARY_STACK_INITIAL 64

/** CLERS sequence initial capacity */
#define CLERS_SEQUENCE_INITIAL 256

/** .lvzd read buffer initial size */
#define LVZD_READ_BUFFER_INITIAL 4096

/** Coordinate dimension (2D or 3D vectors) */
#ifndef COORD_DIM
#define COORD_DIM 2
#endif

/** Magic bytes for the combined compressed output format */
#define LVZD_COMPRESS_MAGIC 0x4C564300 /* "LVZC" */

/** Maximum number of adjacent faces for multi-parallelogram prediction */
#define MAX_ADJACENT_FACES 16

/** Huffman tree maximum node count (256 leaves + max 255 internal nodes) */
#define HUFFMAN_MAX_NODES 511

/** Huffman encoding maximum length */
#define HUFFMAN_MAX_CODE_LEN 256

/* ========================================================================
 * Internal helper structures
 * ======================================================================== */

/**
 * @brief Edge structure - used for Edgebreaker traversal
 */
typedef struct {
    int v0; /**< Edge start vertex ID */
    int v1; /**< Edge end vertex ID */
} Edge;

/**
 * @brief Triangle face structure - inferred from constraint relationships
 */
typedef struct {
    int verts[3]; /**< Three vertex node IDs */
    bool visited; /**< Whether visited during traversal */
} TriangleFace;

/**
 * @brief Huffman tree node
 */
typedef struct {
    int left;           /**< Left child index, -1 = none */
    int right;          /**< Right child index, -1 = none */
    int parent;         /**< Parent index, -1 = root */
    uint32_t freq;      /**< Node frequency weight */
    uint8_t byte_val;   /**< Leaf byte value (invalid for internal nodes) */
} HuffmanNode;

/**
 * @brief Min-heap structure (for building Huffman tree)
 */
typedef struct {
    int *nodes;             /**< Heap-stored Huffman node indices */
    int size;               /**< Current heap size */
    int capacity;           /**< Heap capacity */
    HuffmanNode *hnodes;    /**< Pointer to Huffman node array (for frequency comparison) */
} MinHeap;

/**
 * @brief Huffman encoding lookup table entry
 */
typedef struct {
    uint32_t code;      /**< Encoding bit sequence */
    int length;         /**< Encoding bit length */
} HuffmanCode;

/* Forward declarations for Huffman functions */
static void heap_swap(MinHeap *h, int i, int j);
static void heap_sift_up(MinHeap *h, int idx);
static void heap_sift_down(MinHeap *h, int idx);
static bool heap_push(MinHeap *h, int node_idx);
static int heap_pop(MinHeap *h);
static void huffman_generate_codes(const HuffmanNode *hnodes, int root, HuffmanCode *codes);

/* ========================================================================
 * Default compression config factory (internal)
 * ======================================================================== */

static CompressConfig compress_config_default(void) {
    CompressConfig cfg;
    cfg.pred_mode = PREDICT_PARALLELOGRAM;
    cfg.entropy = ENTROPY_RANS;
    cfg.quantization_bits = 0;
    cfg.lossless = true;
    cfg.max_error = 0.0;
    return cfg;
}

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
static bool extract_triangle_faces(const ConstraintGraph *graph, TriangleFace **faces, int *face_count) {
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
            if (count >= capacity) {
                capacity *= 2;
                TriangleFace *nf = (TriangleFace *) lv_realloc(f, capacity * sizeof(TriangleFace));
                if (!nf) {
                    lv_free((void **) &f);
                    return false;
                }
                f = nf;
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
static int find_adjacent_face(const TriangleFace *faces, int face_count, int v0, int v1, int exclude) {
    for (int i = 0; i < face_count; i++) {
        if (i == exclude)
            continue;
        bool has_v0 = false, has_v1 = false;
        for (int k = 0; k < 3; k++) {
            if (faces[i].verts[k] == v0) has_v0 = true;
            if (faces[i].verts[k] == v1) has_v1 = true;
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
static int find_all_adjacent_faces(const TriangleFace *faces, int face_count, int v0, int v1,
                                   int *adj_indices, int max_adj) {
    int count = 0;
    for (int i = 0; i < face_count && count < max_adj; i++) {
        bool has_v0 = false, has_v1 = false;
        for (int k = 0; k < 3; k++) {
            if (faces[i].verts[k] == v0) has_v0 = true;
            if (faces[i].verts[k] == v1) has_v1 = true;
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
static int get_opposite_vertex(const TriangleFace *face, int v0, int v1) {
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
static double triangle_face_area(const ConstraintGraph *graph, const TriangleFace *face) {
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
    double cross = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    return cross < 0 ? -cross : cross;
}

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
static bool predictive_encode_parallelogram(ConstraintGraph *graph) {
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
    memset(visited, 0, (size_t)node_count * sizeof(bool));

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
            target = v2; known_a = v0; known_b = v1;
        } else if (!visited[v1]) {
            target = v1; known_a = v0; known_b = v2;
        } else if (!visited[v0]) {
            target = v0; known_a = v1; known_b = v2;
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
        if (dim < 1 || !node_target->symbolic_coords ||
            !node_a->symbolic_coords || !node_b->symbolic_coords || !node_opp->symbolic_coords) {
            visited[target] = true;
            face->visited = true;
            continue;
        }

        /* Apply parallelogram prediction per dimension: pred = a + b - opp */
        for (int d = 0; d < dim && d < COORD_DIM; d++) {
            SymbolicCoord *neg_opp = symbolic_coord_negate(node_opp->symbolic_coords[d]);
            if (!neg_opp) continue;
            SymbolicCoord *sum_ab = symbolic_coord_add(node_a->symbolic_coords[d], node_b->symbolic_coords[d]);
            if (!sum_ab) { symbolic_coord_destroy(neg_opp); continue; }
            SymbolicCoord *pred = symbolic_coord_add(sum_ab, neg_opp);
            symbolic_coord_destroy(sum_ab);
            symbolic_coord_destroy(neg_opp);
            if (!pred) continue;

            SymbolicCoord *residual = symbolic_coord_subtract(node_target->symbolic_coords[d], pred);
            symbolic_coord_destroy(pred);
            if (residual) {
                double r = symbolic_coord_to_double(residual);
                if (residual_count >= residual_capacity) {
                    residual_capacity *= 2;
                    double *nr = (double *) lv_realloc(residuals, residual_capacity * sizeof(double));
                    if (!nr) {
                        symbolic_coord_destroy(residual);
                        break;
                    }
                    residuals = nr;
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
        double quant_factor = 1000.0;
        int16_t *quantized = (int16_t *) lv_malloc(residual_count * sizeof(int16_t));
        if (quantized) {
            for (int i = 0; i < residual_count; i++) {
                double v = residuals[i] * quant_factor;
                if (v > 32767.0) v = 32767.0;
                if (v < -32768.0) v = -32768.0;
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
            memset(hnodes, 0, sizeof(hnodes));
            int hnode_count = 0;

            for (int i = 0; i < 256; i++) {
                if (freq[i] > 0) {
                    hnodes[hnode_count].left = -1;
                    hnodes[hnode_count].right = -1;
                    hnodes[hnode_count].parent = -1;
                    hnodes[hnode_count].freq = freq[i];
                    hnodes[hnode_count].byte_val = (uint8_t) i;
                    hnode_count++;
                }
            }

            if (hnode_count == 1) {
                hnodes[hnode_count].left = 0;
                hnodes[hnode_count].right = -1;
                hnodes[hnode_count].parent = -1;
                hnodes[hnode_count].freq = hnodes[0].freq;
                hnodes[hnode_count].byte_val = 0;
                hnodes[0].parent = hnode_count;
                hnode_count++;
            }

            if (hnode_count >= 2) {
                int heap_nodes[HUFFMAN_MAX_NODES];
                MinHeap heap;
                heap.nodes = heap_nodes;
                heap.size = 0;
                heap.capacity = HUFFMAN_MAX_NODES;
                heap.hnodes = hnodes;

                for (int i = 0; i < hnode_count; i++) {
                    heap_push(&heap, i);
                }

                while (heap.size > 1) {
                    int left = heap_pop(&heap);
                    int right = heap_pop(&heap);
                    hnodes[hnode_count].left = left;
                    hnodes[hnode_count].right = right;
                    hnodes[hnode_count].parent = -1;
                    hnodes[hnode_count].freq = hnodes[left].freq + hnodes[right].freq;
                    hnodes[hnode_count].byte_val = 0;
                    hnodes[left].parent = hnode_count;
                    hnodes[right].parent = hnode_count;
                    heap_push(&heap, hnode_count);
                    hnode_count++;
                }

                int root = heap_pop(&heap);
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
static bool predictive_encode_multi_parallelogram(ConstraintGraph *graph) {
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
    memset(visited, 0, (size_t)node_count * sizeof(bool));
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
            target = v2; known_a = v0; known_b = v1;
        } else if (!visited[v1]) {
            target = v1; known_a = v0; known_b = v2;
        } else if (!visited[v0]) {
            target = v0; known_a = v1; known_b = v2;
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
        if (dim < 1 || !node_target->symbolic_coords ||
            !node_a->symbolic_coords || !node_b->symbolic_coords) {
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
            if (v_opp < 0) continue;

            GeomNode *node_opp = graph_get_node(graph, v_opp);
            if (!node_opp || !node_opp->symbolic_coords) continue;

            /* Weight by face area */
            double area = triangle_face_area(graph, &faces[adj_indices[ai]]);
            if (area <= 0.0) area = 1.0; /* Minimum weight to avoid division by zero */
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
            double scaled_val = residual_val * 1000.0;
            /* 钳制到 int64 安全范围再转换，避免大值时未定义行为 */
            if (scaled_val > 9223372036854774784.0) scaled_val = 9223372036854774784.0;
            if (scaled_val < -9223372036854774784.0) scaled_val = -9223372036854774784.0;
            SymbolicCoord *residual = symbolic_coord_create_rational(
                (int64_t)scaled_val, 1000);
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
static bool predictive_encode_delta(ConstraintGraph *graph) {
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

/* ========================================================================
 * Public predictive encoding interface
 * ======================================================================== */

bool predictive_encode_coords(ConstraintGraph *graph, PredictionMode mode) {
    if (!graph)
        return false;

    switch (mode) {
        case PREDICT_PARALLELOGRAM:
            return predictive_encode_parallelogram(graph);

        case PREDICT_MULTI_PARALLELOGRAM:
            return predictive_encode_multi_parallelogram(graph);

        case PREDICT_DELTA:
            return predictive_encode_delta(graph);

        case PREDICT_NONE:
            /* No prediction: keep original coordinates */
            return true;

        default:
            return false;
    }
}

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
static bool find_vertex_in_boundary(const Edge *boundary, int boundary_top, int vertex_id,
                                    int *edge_index, bool *is_v0) {
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
    memset(visited, 0, (size_t)graph->node_count * sizeof(bool));

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
                    capacity *= 2;
                    EdgebreakerMode *new_seq =
                        (EdgebreakerMode *) lv_realloc(seq, capacity * sizeof(EdgebreakerMode));
                    if (!new_seq)
                        break;
                    seq = new_seq;
                }
                seq[len++] = EDGEBREAKER_C;
                visited[opposite_id] = true;

                /* Push new edges onto boundary stack */
                if (boundary_top >= boundary_capacity) {
                    boundary_capacity *= 2;
                    Edge *new_b = (Edge *) lv_realloc(boundary, boundary_capacity * sizeof(Edge));
                    if (!new_b)
                        break;
                    boundary = new_b;
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
                bool opp_in_boundary = find_vertex_in_boundary(boundary, boundary_top, opposite_id,
                                                               &opp_edge_index, &opp_is_v0);

                if (!opp_in_boundary) {
                    /* Opposite vertex not in boundary -> S mode (split) */
                    if (len >= capacity) {
                        capacity *= 2;
                        EdgebreakerMode *new_seq =
                            (EdgebreakerMode *) lv_realloc(seq, capacity * sizeof(EdgebreakerMode));
                        if (!new_seq)
                            break;
                        seq = new_seq;
                    }
                    seq[len++] = EDGEBREAKER_S;
                } else {
                    /* Check if opposite vertex is the next boundary vertex -> S mode */
                    if (boundary_top > 0) {
                        Edge next_edge = boundary[boundary_top - 1];
                        if (next_edge.v0 == opposite_id) {
                            /* Opposite vertex IS the next boundary vertex -> S mode */
                            if (len >= capacity) {
                                capacity *= 2;
                                EdgebreakerMode *new_seq =
                                    (EdgebreakerMode *) lv_realloc(seq, capacity * sizeof(EdgebreakerMode));
                                if (!new_seq)
                                    break;
                                seq = new_seq;
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
                                    capacity *= 2;
                                    EdgebreakerMode *new_seq =
                                        (EdgebreakerMode *) lv_realloc(seq, capacity * sizeof(EdgebreakerMode));
                                    if (!new_seq)
                                        break;
                                    seq = new_seq;
                                }
                                seq[len++] = EDGEBREAKER_R;
                            } else {
                                /* Opposite is v1 of its boundary edge -> LEFT side */
                                if (len >= capacity) {
                                    capacity *= 2;
                                    EdgebreakerMode *new_seq =
                                        (EdgebreakerMode *) lv_realloc(seq, capacity * sizeof(EdgebreakerMode));
                                    if (!new_seq)
                                        break;
                                    seq = new_seq;
                                }
                                seq[len++] = EDGEBREAKER_L;
                            }
                        }
                    } else {
                        /* No more boundary edges -> default to L */
                        if (len >= capacity) {
                            capacity *= 2;
                            EdgebreakerMode *new_seq =
                                (EdgebreakerMode *) lv_realloc(seq, capacity * sizeof(EdgebreakerMode));
                            if (!new_seq)
                                break;
                            seq = new_seq;
                        }
                        seq[len++] = EDGEBREAKER_L;
                    }
                }
            } else {
                /* No opposite vertex -> E mode */
                if (len >= capacity) {
                    capacity *= 2;
                    EdgebreakerMode *new_seq =
                        (EdgebreakerMode *) lv_realloc(seq, capacity * sizeof(EdgebreakerMode));
                    if (!new_seq)
                        break;
                    seq = new_seq;
                }
                seq[len++] = EDGEBREAKER_E;
            }
        }

        if (!found_opposite) {
            /* No related constraint -> mark as E */
            if (len >= capacity) {
                capacity *= 2;
                EdgebreakerMode *new_seq = (EdgebreakerMode *) lv_realloc(seq, capacity * sizeof(EdgebreakerMode));
                if (!new_seq)
                    break;
                seq = new_seq;
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

/* ========================================================================
 * Huffman encoder/decoder
 * ======================================================================== */

/**
 * @brief Bit writer - supports bit-level writing to output buffer
 */
typedef struct {
    uint8_t *buf;       /**< Output buffer */
    size_t capacity;    /**< Buffer capacity (bytes) */
    size_t byte_pos;    /**< Current write byte position */
    int bit_pos;        /**< Current bit position within byte (7=MSB, 0=LSB) */
} BitWriter;

/**
 * @brief Bit reader - supports bit-level reading from input buffer
 */
typedef struct {
    const uint8_t *buf; /**< Input buffer */
    size_t size;        /**< Buffer size (bytes) */
    size_t byte_pos;    /**< Current read byte position */
    int bit_pos;        /**< Current bit position within byte (7=MSB, 0=LSB) */
} BitReader;

static bool bitwriter_write_bit(BitWriter *bw, int bit) {
    if (bit) {
        bw->buf[bw->byte_pos] |= (uint8_t) (1 << bw->bit_pos);
    }
    bw->bit_pos--;
    if (bw->bit_pos < 0) {
        bw->bit_pos = 7;
        bw->byte_pos++;
        if (bw->byte_pos >= bw->capacity) {
            size_t new_cap = bw->capacity * 2;
            uint8_t *new_buf = (uint8_t *) lv_realloc(bw->buf, new_cap);
            if (!new_buf)
                return false;
            bw->buf = new_buf;
            bw->capacity = new_cap;
        }
        bw->buf[bw->byte_pos] = 0;
    }
    return true;
}

static bool bitwriter_write_bits(BitWriter *bw, uint32_t code, int bit_count) {
    for (int i = bit_count - 1; i >= 0; i--) {
        if (!bitwriter_write_bit(bw, (code >> i) & 1))
            return false;
    }
    return true;
}

static size_t bitwriter_flush(const BitWriter *bw) {
    return (bw->bit_pos < 7) ? bw->byte_pos + 1 : bw->byte_pos;
}

/* ========================================================================
 * Bit reader operations
 * ======================================================================== */

static void bitreader_init(BitReader *br, const uint8_t *buf, size_t size) {
    br->buf = buf;
    br->size = size;
    br->byte_pos = 0;
    br->bit_pos = 7;
}

static int bitreader_read_bit(BitReader *br) {
    if (br->byte_pos >= br->size)
        return -1;
    int bit = (br->buf[br->byte_pos] >> br->bit_pos) & 1;
    br->bit_pos--;
    if (br->bit_pos < 0) {
        br->bit_pos = 7;
        br->byte_pos++;
    }
    return bit;
}

/* ========================================================================
 * Min-heap operations (for building Huffman tree)
 * ======================================================================== */

static void heap_swap(MinHeap *h, int i, int j) {
    int tmp = h->nodes[i];
    h->nodes[i] = h->nodes[j];
    h->nodes[j] = tmp;
}

static void heap_sift_up(MinHeap *h, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (h->hnodes[h->nodes[idx]].freq < h->hnodes[h->nodes[parent]].freq) {
            heap_swap(h, idx, parent);
            idx = parent;
        } else {
            break;
        }
    }
}

static void heap_sift_down(MinHeap *h, int idx) {
    int size = h->size;
    while (1) {
        int smallest = idx;
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;
        if (left < size && h->hnodes[h->nodes[left]].freq < h->hnodes[h->nodes[smallest]].freq)
            smallest = left;
        if (right < size && h->hnodes[h->nodes[right]].freq < h->hnodes[h->nodes[smallest]].freq)
            smallest = right;
        if (smallest == idx)
            break;
        heap_swap(h, idx, smallest);
        idx = smallest;
    }
}

static bool heap_push(MinHeap *h, int node_idx) {
    if (h->size >= h->capacity)
        return false;
    h->nodes[h->size] = node_idx;
    heap_sift_up(h, h->size);
    h->size++;
    return true;
}

static int heap_pop(MinHeap *h) {
    if (h->size <= 0)
        return -1;
    int result = h->nodes[0];
    h->size--;
    if (h->size > 0) {
        h->nodes[0] = h->nodes[h->size];
        heap_sift_down(h, 0);
    }
    return result;
}

/* ========================================================================
 * Huffman encoding table generation
 * ======================================================================== */

static void huffman_generate_codes(const HuffmanNode *hnodes, int root, HuffmanCode *codes) {
    int stack[HUFFMAN_MAX_NODES];
    uint32_t code_stack[HUFFMAN_MAX_NODES];
    int len_stack[HUFFMAN_MAX_NODES];
    int top = 0;

    stack[0] = root;
    code_stack[0] = 0;
    len_stack[0] = 0;
    top = 1;

    while (top > 0) {
        top--;
        int node = stack[top];
        uint32_t code = code_stack[top];
        int len = len_stack[top];

        if (hnodes[node].left < 0 && hnodes[node].right < 0) {
            codes[hnodes[node].byte_val].code = code;
            codes[hnodes[node].byte_val].length = len;
        } else {
            if (hnodes[node].left >= 0) {
                stack[top] = hnodes[node].left;
                code_stack[top] = (code << 1) | 0;
                len_stack[top] = len + 1;
                top++;
            }
            if (hnodes[node].right >= 0) {
                stack[top] = hnodes[node].right;
                code_stack[top] = (code << 1) | 1;
                len_stack[top] = len + 1;
                top++;
            }
        }
    }
}

/* ========================================================================
 * Huffman encoding (replaces original entropy_encode_stub)
 * ======================================================================== */

/**
 * @brief Huffman entropy encoder
 *
 * Full Huffman compression encoding:
 *   1. Count byte frequencies
 *   2. Build Huffman tree using min-heap
 *   3. Generate variable-length codes for each byte value
 *   4. Encode raw data as bit stream
 *
 * Output format:
 *   [frequency table: 256 x 4 bytes, int32_t little-endian] +
 *   [original size: 4 bytes, int32_t little-endian] +
 *   [encoded bit stream: variable length]
 *
 * @param[in]  raw_data   Raw data
 * @param[in]  raw_size   Raw data size (bytes)
 * @param[out] out_data   Encoded data (caller responsible for free)
 * @param[out] out_size   Encoded data size (bytes)
 * @return true success, false failure
 */
static bool entropy_encode_huffman(const uint8_t *raw_data, size_t raw_size, uint8_t **out_data, size_t *out_size) {
    if (!raw_data || raw_size == 0) {
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    /* Step 1: Count byte frequencies */
    uint32_t freq[256];
    memset(freq, 0, sizeof(freq));
    for (size_t i = 0; i < raw_size; i++) {
        freq[raw_data[i]]++;
    }

    /* Step 2: Build Huffman tree */
    HuffmanNode hnodes[HUFFMAN_MAX_NODES];
    memset(hnodes, 0, sizeof(hnodes));
    int node_count = 0;

    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            hnodes[node_count].left = -1;
            hnodes[node_count].right = -1;
            hnodes[node_count].parent = -1;
            hnodes[node_count].freq = freq[i];
            hnodes[node_count].byte_val = (uint8_t) i;
            node_count++;
        }
    }

    if (node_count == 1) {
        hnodes[node_count].left = 0;
        hnodes[node_count].right = -1;
        hnodes[node_count].parent = -1;
        hnodes[node_count].freq = hnodes[0].freq;
        hnodes[node_count].byte_val = 0;
        hnodes[0].parent = node_count;
        node_count++;
    }

    int heap_nodes[HUFFMAN_MAX_NODES];
    MinHeap heap;
    heap.nodes = heap_nodes;
    heap.size = 0;
    heap.capacity = HUFFMAN_MAX_NODES;
    heap.hnodes = hnodes;

    for (int i = 0; i < node_count; i++) {
        heap_push(&heap, i);
    }

    while (heap.size > 1) {
        int left = heap_pop(&heap);
        int right = heap_pop(&heap);

        hnodes[node_count].left = left;
        hnodes[node_count].right = right;
        hnodes[node_count].parent = -1;
        hnodes[node_count].freq = hnodes[left].freq + hnodes[right].freq;
        hnodes[node_count].byte_val = 0;
        hnodes[left].parent = node_count;
        hnodes[right].parent = node_count;

        heap_push(&heap, node_count);
        node_count++;
    }

    int root = heap_pop(&heap);

    /* Step 3: Generate Huffman encoding table */
    HuffmanCode codes[256];
    memset(codes, 0, sizeof(codes));
    huffman_generate_codes(hnodes, root, codes);

    /* Step 4: Encode output */
    size_t header_size = 256 * sizeof(uint32_t) + sizeof(uint32_t);
    size_t bitstream_capacity = (raw_size * 8 + 7) / 8 + 16;
    size_t total_capacity = header_size + bitstream_capacity;

    uint8_t *output = (uint8_t *) lv_malloc(total_capacity);
    if (!output)
        return false;

    /* Write frequency table (256 x int32_t, little-endian) */
    for (int i = 0; i < 256; i++) {
        uint32_t f = freq[i];
        output[i * 4 + 0] = (uint8_t) (f & 0xFF);
        output[i * 4 + 1] = (uint8_t) ((f >> 8) & 0xFF);
        output[i * 4 + 2] = (uint8_t) ((f >> 16) & 0xFF);
        output[i * 4 + 3] = (uint8_t) ((f >> 24) & 0xFF);
    }

    /* Write original size */
    size_t offset = 256 * sizeof(uint32_t);
    uint32_t raw_sz = (uint32_t) raw_size;
    output[offset + 0] = (uint8_t) (raw_sz & 0xFF);
    output[offset + 1] = (uint8_t) ((raw_sz >> 8) & 0xFF);
    output[offset + 2] = (uint8_t) ((raw_sz >> 16) & 0xFF);
    output[offset + 3] = (uint8_t) ((raw_sz >> 24) & 0xFF);
    offset += sizeof(uint32_t);

    /* Use bit writer to encode data */
    BitWriter bw;
    bw.buf = output + offset;
    bw.capacity = bitstream_capacity;
    bw.byte_pos = 0;
    bw.bit_pos = 7;
    bw.buf[0] = 0;

    for (size_t i = 0; i < raw_size; i++) {
        uint8_t byte_val = raw_data[i];
        HuffmanCode *hc = &codes[byte_val];
        if (hc->length == 0) {
            lv_free((void **) &output);
            return false;
        }
        if (!bitwriter_write_bits(&bw, hc->code, hc->length)) {
            lv_free((void **) &output);
            return false;
        }
    }

    size_t bitstream_bytes = bitwriter_flush(&bw);
    *out_data = output;
    *out_size = offset + bitstream_bytes;
    return true;
}

/* ========================================================================
 * Huffman decoding (replaces original entropy_decode_stub)
 * ======================================================================== */

/**
 * @brief Huffman entropy decoder
 *
 * Reconstructs raw data from compressed bit stream:
 *   1. Read frequency table (256 x uint32_t) and rebuild Huffman tree
 *   2. Read original data size
 *   3. Bit-by-bit decode using Huffman tree
 *
 * @param[in]  data      Compressed data
 * @param[in]  size      Compressed data size (bytes)
 * @param[out] out_data  Decompressed raw data (caller responsible for free)
 * @param[out] out_size  Decompressed data size (bytes)
 * @return true success, false failure
 */
static bool entropy_decode_huffman(const uint8_t *data, size_t size, uint8_t **out_data, size_t *out_size) {
    if (!data || size == 0) {
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    size_t min_size = 256 * sizeof(uint32_t) + sizeof(uint32_t);
    if (size < min_size)
        return false;

    /* Step 1: Read frequency table and rebuild Huffman tree */
    uint32_t freq[256];
    for (int i = 0; i < 256; i++) {
        freq[i] = ((uint32_t) data[i * 4 + 0]) | ((uint32_t) data[i * 4 + 1] << 8)
                | ((uint32_t) data[i * 4 + 2] << 16) | ((uint32_t) data[i * 4 + 3] << 24);
    }

    size_t offset = 256 * sizeof(uint32_t);
    uint32_t raw_sz = ((uint32_t) data[offset + 0]) | ((uint32_t) data[offset + 1] << 8)
                    | ((uint32_t) data[offset + 2] << 16) | ((uint32_t) data[offset + 3] << 24);
    offset += sizeof(uint32_t);

    if (raw_sz == 0) {
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    uint8_t *output = (uint8_t *) lv_malloc(raw_sz);
    if (!output)
        return false;

    /* Rebuild Huffman tree */
    HuffmanNode hnodes[HUFFMAN_MAX_NODES];
    memset(hnodes, 0, sizeof(hnodes));
    int node_count = 0;

    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            hnodes[node_count].left = -1;
            hnodes[node_count].right = -1;
            hnodes[node_count].parent = -1;
            hnodes[node_count].freq = freq[i];
            hnodes[node_count].byte_val = (uint8_t) i;
            node_count++;
        }
    }

    if (node_count == 1) {
        hnodes[node_count].left = 0;
        hnodes[node_count].right = -1;
        hnodes[node_count].parent = -1;
        hnodes[node_count].freq = hnodes[0].freq;
        hnodes[node_count].byte_val = 0;
        hnodes[0].parent = node_count;
        node_count++;
    }

    int heap_nodes[HUFFMAN_MAX_NODES];
    MinHeap heap;
    heap.nodes = heap_nodes;
    heap.size = 0;
    heap.capacity = HUFFMAN_MAX_NODES;
    heap.hnodes = hnodes;

    for (int i = 0; i < node_count; i++) {
        heap_push(&heap, i);
    }

    while (heap.size > 1) {
        int left = heap_pop(&heap);
        int right = heap_pop(&heap);
        hnodes[node_count].left = left;
        hnodes[node_count].right = right;
        hnodes[node_count].parent = -1;
        hnodes[node_count].freq = hnodes[left].freq + hnodes[right].freq;
        hnodes[node_count].byte_val = 0;
        hnodes[left].parent = node_count;
        hnodes[right].parent = node_count;
        heap_push(&heap, node_count);
        node_count++;
    }

    int root = heap_pop(&heap);

    /* Step 2: Bit-by-bit decode */
    BitReader br;
    bitreader_init(&br, data + offset, size - offset);

    size_t decoded = 0;

    if (node_count == 2 && hnodes[root].right < 0) {
        uint8_t byte_val = hnodes[hnodes[root].left].byte_val;
        for (size_t i = 0; i < raw_sz; i++) {
            output[i] = byte_val;
        }
    } else {
        while (decoded < raw_sz) {
            int node = root;
            while (hnodes[node].left >= 0 || hnodes[node].right >= 0) {
                int bit = bitreader_read_bit(&br);
                if (bit < 0) {
                    lv_free((void **) &output);
                    return false;
                }
                if (bit == 0) {
                    node = hnodes[node].left;
                } else {
                    node = hnodes[node].right;
                }
                if (node < 0) {
                    lv_free((void **) &output);
                    return false;
                }
            }
            output[decoded++] = hnodes[node].byte_val;
        }
    }

    *out_data = output;
    *out_size = raw_sz;
    return true;
}

/* ========================================================================
 * RLE (Run-Length Encoding) helpers for T-012
 * ======================================================================== */

/**
 * @brief Apply Run-Length Encoding to input data
 *
 * Format: for each run: [byte_value][run_count_as_uint8]
 * Runs longer than 255 are split into multiple entries.
 *
 * @param[in]  data     Input data
 * @param[in]  size     Input data size
 * @param[out] out_data RLE encoded data (caller responsible for free)
 * @param[out] out_size RLE encoded data size
 * @return true success, false failure
 */
static bool rle_encode(const uint8_t *data, size_t size, uint8_t **out_data, size_t *out_size) {
    if (!data || size == 0) {
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    /* Worst case: each byte is different -> 2x expansion */
    size_t max_out = size * 2;
    uint8_t *output = (uint8_t *) lv_malloc(max_out);
    if (!output)
        return false;

    size_t out_pos = 0;
    size_t i = 0;

    while (i < size) {
        uint8_t current = data[i];
        size_t run = 1;
        while (i + run < size && data[i + run] == current && run < 255) {
            run++;
        }
        output[out_pos++] = current;
        output[out_pos++] = (uint8_t) run;
        i += run;
    }

    *out_data = output;
    *out_size = out_pos;
    return true;
}

/**
 * @brief Decode RLE-encoded data
 *
 * @param[in]  data     RLE encoded data
 * @param[in]  size     RLE encoded data size
 * @param[out] out_data Decoded data (caller responsible for free)
 * @param[out] out_size Decoded data size
 * @return true success, false failure
 */
static bool rle_decode(const uint8_t *data, size_t size, uint8_t **out_data, size_t *out_size) {
    if (!data || size == 0 || size % 2 != 0) {
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    /* First pass: compute output size */
    size_t total = 0;
    for (size_t i = 0; i < size; i += 2) {
        total += data[i + 1];
    }

    if (total == 0) {
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    uint8_t *output = (uint8_t *) lv_malloc(total);
    if (!output)
        return false;

    size_t out_pos = 0;
    for (size_t i = 0; i < size; i += 2) {
        uint8_t val = data[i];
        uint8_t count = data[i + 1];
        for (int j = 0; j < count; j++) {
            output[out_pos++] = val;
        }
    }

    *out_data = output;
    *out_size = total;
    return true;
}

/* ========================================================================
 * Real entropy encoding: RLE + Huffman (T-012)
 * ======================================================================== */

/**
 * @brief Combined RLE + Huffman entropy encoder
 *
 * Two-pass encoding:
 *   1. Apply Run-Length Encoding as first pass
 *   2. Apply Huffman coding on the RLE output
 *
 * Output format:
 *   [magic: 4 bytes "LVZC"]
 *   [original_size: 4 bytes]
 *   [rle_size: 4 bytes]
 *   [huffman_encoded_rle_data: variable]
 *
 * @param[in]  raw_data   Raw data
 * @param[in]  raw_size   Raw data size
 * @param[out] out_data   Encoded data (caller responsible for free)
 * @param[out] out_size   Encoded data size
 * @return true success, false failure
 */
static bool entropy_encode_real(const uint8_t *raw_data, size_t raw_size, uint8_t **out_data, size_t *out_size) {
    if (!raw_data || raw_size == 0) {
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    /* Pass 1: RLE encoding */
    uint8_t *rle_data = NULL;
    size_t rle_size = 0;
    if (!rle_encode(raw_data, raw_size, &rle_data, &rle_size))
        return false;

    if (!rle_data || rle_size == 0) {
        lv_free((void **) &rle_data);
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    /* Pass 2: Huffman encoding on RLE output */
    uint8_t *huffman_data = NULL;
    size_t huffman_size = 0;
    bool huff_ok = entropy_encode_huffman(rle_data, rle_size, &huffman_data, &huffman_size);
    lv_free((void **) &rle_data);

    if (!huff_ok) {
        return false;
    }

    /* Build combined output: [magic(4)][original_size(4)][rle_size(4)][huffman_data] */
    size_t header_size = 4 + 4 + 4; /* magic + original_size + rle_size */
    size_t total_size = header_size + huffman_size;
    uint8_t *output = (uint8_t *) lv_malloc(total_size);
    if (!output) {
        lv_free((void **) &huffman_data);
        return false;
    }

    /* Write magic bytes */
    output[0] = (uint8_t) (LVZD_COMPRESS_MAGIC & 0xFF);
    output[1] = (uint8_t) ((LVZD_COMPRESS_MAGIC >> 8) & 0xFF);
    output[2] = (uint8_t) ((LVZD_COMPRESS_MAGIC >> 16) & 0xFF);
    output[3] = (uint8_t) ((LVZD_COMPRESS_MAGIC >> 24) & 0xFF);

    /* Write original size */
    uint32_t orig_sz = (uint32_t) raw_size;
    output[4] = (uint8_t) (orig_sz & 0xFF);
    output[5] = (uint8_t) ((orig_sz >> 8) & 0xFF);
    output[6] = (uint8_t) ((orig_sz >> 16) & 0xFF);
    output[7] = (uint8_t) ((orig_sz >> 24) & 0xFF);

    /* Write RLE size */
    uint32_t rle_sz = (uint32_t) rle_size;
    output[8] = (uint8_t) (rle_sz & 0xFF);
    output[9] = (uint8_t) ((rle_sz >> 8) & 0xFF);
    output[10] = (uint8_t) ((rle_sz >> 16) & 0xFF);
    output[11] = (uint8_t) ((rle_sz >> 24) & 0xFF);

    /* Copy Huffman data */
    memcpy(output + header_size, huffman_data, huffman_size);
    lv_free((void **) &huffman_data);

    *out_data = output;
    *out_size = total_size;
    return true;
}

/**
 * @brief Combined RLE + Huffman entropy decoder
 *
 * Reverse of entropy_encode_real:
 *   1. Parse header (magic, original_size, rle_size)
 *   2. Huffman decode to get RLE data
 *   3. RLE decode to get original data
 *
 * @param[in]  data      Compressed data
 * @param[in]  size      Compressed data size
 * @param[out] out_data  Decompressed data (caller responsible for free)
 * @param[out] out_size  Decompressed data size
 * @return true success, false failure
 */
static bool entropy_decode_real(const uint8_t *data, size_t size, uint8_t **out_data, size_t *out_size) {
    if (!data || size == 0) {
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    /* Parse header */
    size_t header_size = 4 + 4 + 4; /* magic + original_size + rle_size */
    if (size < header_size)
        return false;

    /* Verify magic */
    uint32_t magic = ((uint32_t) data[0]) | ((uint32_t) data[1] << 8)
                   | ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
    if (magic != LVZD_COMPRESS_MAGIC) {
        /* Fall back to legacy Huffman-only format */
        return entropy_decode_huffman(data, size, out_data, out_size);
    }

    uint32_t orig_sz = ((uint32_t) data[4]) | ((uint32_t) data[5] << 8)
                      | ((uint32_t) data[6] << 16) | ((uint32_t) data[7] << 24);
    (void) orig_sz; /* Used for validation */

    /* Huffman decode to get RLE data */
    uint8_t *rle_data = NULL;
    size_t rle_decoded_size = 0;
    if (!entropy_decode_huffman(data + header_size, size - header_size, &rle_data, &rle_decoded_size)) {
        return false;
    }

    /* RLE decode to get original data */
    bool rle_ok = rle_decode(rle_data, rle_decoded_size, out_data, out_size);
    lv_free((void **) &rle_data);

    return rle_ok;
}

/* ========================================================================
 * graph_clone for deep copy (T-010/011)
 * ======================================================================== */

/**
 * @brief Deep copy a ConstraintGraph
 *
 * Creates a new graph with deep copies of all nodes (including coordinates),
 * all constraints, and all edges.
 *
 * @param[in] graph Source constraint graph
 * @return Deep-copied graph, NULL on failure
 */
static ConstraintGraph *graph_clone(const ConstraintGraph *graph) {
    if (!graph)
        return NULL;

    ConstraintGraph *cloned = graph_create();
    if (!cloned)
        return NULL;

    /* Deep copy all nodes */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *orig = graph->nodes[i];
        if (!orig)
            continue;

        GeomNode *copy = node_deep_copy_geom_node(orig, NULL);
        if (!copy) {
            graph_destroy(cloned);
            return NULL;
        }

        /* Add node to cloned graph using the same ID */
        GeomNode *added = graph_add_node_with_id(cloned, copy->id, copy->type,
                                                  copy->symbolic_coords, copy->coord_count);
        if (!added) {
            /* Clean up the copy we made */
            if (copy->symbolic_coords) {
                for (int d = 0; d < copy->coord_count; d++) {
                    if (copy->symbolic_coords[d])
                        symbolic_coord_destroy(copy->symbolic_coords[d]);
                }
                lv_free((void **) &copy->symbolic_coords);
            }
            if (copy->numeric_assumption_declaration)
                lv_free((void **) &copy->numeric_assumption_declaration);
            lv_free((void **) &copy);
            graph_destroy(cloned);
            return NULL;
        }

        /* Copy additional fields that graph_add_node_with_id may not set */
        added->trust = copy->trust;
        added->lo_subtype = copy->lo_subtype;
        added->numeric_precision = copy->numeric_precision;
        added->namespace_depth = copy->namespace_depth;
        added->parent_block_id = copy->parent_block_id;

        /* Free the intermediate copy (graph_add_node_with_id made its own deep copy) */
        if (copy->numeric_assumption_declaration)
            lv_free((void **) &copy->numeric_assumption_declaration);
        lv_free((void **) &copy);
    }

    /* Deep copy all constraints */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *orig = graph->constraints[i];
        if (!orig)
            continue;

        /* Allocate participant array copy */
        int *parts_copy = (int *) lv_malloc(orig->participant_count * sizeof(int));
        if (!parts_copy) {
            graph_destroy(cloned);
            return NULL;
        }
        memcpy(parts_copy, orig->participants, (size_t)orig->participant_count * sizeof(int));

        Constraint *added = graph_add_constraint_with_id(cloned, orig->id, orig->type,
                                                          parts_copy, orig->participant_count);
        if (!added) {
            lv_free((void **) &parts_copy);
            graph_destroy(cloned);
            return NULL;
        }
        lv_free((void **) &parts_copy); /* graph_add_constraint_with_id makes its own copy */
    }

    return cloned;
}

/* ========================================================================
 * Geometry compression main API
 *
 * 完整压缩流水线：graph_clone → edgebreaker → predict → entropy → pack
 * ======================================================================== */

/**
 * @brief Estimate original byte size of geometry data in constraint graph
 */
static size_t estimate_original_size(const ConstraintGraph *graph) {
    if (!graph)
        return 0;

    size_t total = 0;
    total += graph->node_count * (sizeof(int) * 3);
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (node && node->symbolic_coords) {
            total += node->coord_count * sizeof(SymbolicCoord *);
        }
    }
    total += graph->constraint_count * sizeof(Constraint);

    return total;
}

/**
 * @brief Serialize node coordinates to raw byte stream
 *
 * Format: node_count(4B) + [node_id(4B) + coord_count(4B) + coord_doubles(8B*coord_count*dim)]*
 */
static uint8_t *serialize_coords_raw(const ConstraintGraph *graph, size_t *out_size) {
    if (!graph || !out_size)
        return NULL;

    size_t header = sizeof(int32_t);
    size_t body = 0;

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;
        body += 2 * sizeof(int32_t);
        if (node->symbolic_coords) {
            body += node->coord_count * sizeof(double);
        }
    }

    size_t total = header + body;
    uint8_t *buf = (uint8_t *) lv_malloc(total);
    if (!buf)
        return NULL;

    uint8_t *ptr = buf;
    int32_t count = (int32_t) graph->node_count;
    memcpy(ptr, &count, sizeof(int32_t));
    ptr += sizeof(int32_t);

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;

        int32_t nid = (int32_t) node->id;
        int32_t cc = (int32_t) node->coord_count;
        memcpy(ptr, &nid, sizeof(int32_t));
        ptr += sizeof(int32_t);
        memcpy(ptr, &cc, sizeof(int32_t));
        ptr += sizeof(int32_t);

        for (int d = 0; d < node->coord_count; d++) {
            double val = symbolic_coord_to_double(node->symbolic_coords[d]);
            memcpy(ptr, &val, sizeof(double));
            ptr += sizeof(double);
        }
    }

    *out_size = (size_t) (ptr - buf);
    return buf;
}

/**
 * @brief Serialize CLERS sequence to byte stream
 *
 * Format: seq_len(4B) + [mode_byte(seq_len)]*
 */
static uint8_t *serialize_clers(const EdgebreakerMode *seq, int seq_len, size_t *out_size) {
    if (!out_size)
        return NULL;

    if (!seq || seq_len <= 0) {
        *out_size = 0;
        return NULL;
    }

    size_t total = sizeof(int32_t) + seq_len;
    uint8_t *buf = (uint8_t *) lv_malloc(total);
    if (!buf)
        return NULL;

    int32_t len = (int32_t) seq_len;
    memcpy(buf, &len, sizeof(int32_t));

    for (int i = 0; i < seq_len; i++) {
        buf[sizeof(int32_t) + i] = (uint8_t) seq[i];
    }

    *out_size = total;
    return buf;
}

bool geometry_compress(const ConstraintGraph *graph, const CompressConfig *config, uint8_t **out_data, size_t *out_size,
                       CompressMetadata *out_meta) {
    if (!graph || !out_data || !out_size)
        return false;

    CompressConfig cfg = config ? *config : compress_config_default();

    /* Step 1: Estimate original size */
    size_t original_sz = estimate_original_size(graph);

    /* Step 2: Deep copy constraint graph for in-place modification (predictive encoding modifies coordinates) */
    ConstraintGraph *work_graph = graph_clone(graph);
    if (!work_graph) {
        return false;
    }

    /* Step 3: Predictive encoding - replace coordinates with residuals */
    predictive_encode_coords(work_graph, cfg.pred_mode);

    /* Step 4: Edgebreaker encoding - generate CLERS symbol sequence */
    EdgebreakerMode *clers_seq = NULL;
    int clers_len = 0;
    edgebreaker_encode(work_graph, &clers_seq, &clers_len);

    /* Step 5: Serialize coordinate data to raw byte stream */
    size_t raw_size = 0;
    uint8_t *raw_buf = serialize_coords_raw(work_graph, &raw_size);
    if (!raw_buf) {
        lv_free((void **) &clers_seq);
        graph_destroy(work_graph);
        return false;
    }

    /* Step 6: Serialize CLERS sequence */
    size_t clers_serial_size = 0;
    uint8_t *clers_serial = serialize_clers(clers_seq, clers_len, &clers_serial_size);

    /* Step 7: Combine CLERS and coordinate data, then apply real entropy encoding */
    /* Combined format: [clers_serial_size(4B)][clers_serial][coord_data] */
    size_t combined_header = sizeof(uint32_t);
    size_t combined_size = combined_header + clers_serial_size + raw_size;
    uint8_t *combined = (uint8_t *) lv_malloc(combined_size);
    if (!combined) {
        lv_free((void **) &clers_serial);
        lv_free((void **) &raw_buf);
        lv_free((void **) &clers_seq);
        graph_destroy(work_graph);
        return false;
    }

    /* Write CLERS section size */
    uint32_t csz = (uint32_t) clers_serial_size;
    combined[0] = (uint8_t) (csz & 0xFF);
    combined[1] = (uint8_t) ((csz >> 8) & 0xFF);
    combined[2] = (uint8_t) ((csz >> 16) & 0xFF);
    combined[3] = (uint8_t) ((csz >> 24) & 0xFF);

    /* Copy CLERS data */
    if (clers_serial && clers_serial_size > 0) {
        memcpy(combined + combined_header, clers_serial, clers_serial_size);
    }
    lv_free((void **) &clers_serial);

    /* Copy coordinate data */
    memcpy(combined + combined_header + clers_serial_size, raw_buf, raw_size);
    lv_free((void **) &raw_buf);

    /* Apply real entropy encoding (RLE + Huffman) */
    uint8_t *encoded = NULL;
    size_t encoded_size = 0;
    bool enc_ok = entropy_encode_real(combined, combined_size, &encoded, &encoded_size);
    lv_free((void **) &combined);

    graph_destroy(work_graph);

    if (!enc_ok) {
        lv_free((void **) &clers_seq);
        return false;
    }

    *out_data = encoded;
    *out_size = encoded_size;

    /* Fill metadata */
    if (out_meta) {
        out_meta->original_size = (int)original_sz;
        out_meta->compressed_size = (int)encoded_size;
        out_meta->compression_ratio = (encoded_size > 0) ? (double) original_sz / (double) encoded_size : 1.0;
        out_meta->node_count = graph->node_count;
        out_meta->constraint_count = graph->constraint_count;
        out_meta->edgebreaker_sequence = clers_seq;
        out_meta->sequence_len = clers_len;
    } else {
        lv_free((void **) &clers_seq);
    }

    return true;
}

/* ========================================================================
 * Geometry decompression main API
 *
 * 完整解压流水线：unpack → entropy decode → clers decode → coords restore
 * ======================================================================== */

/**
 * @brief Deserialize CLERS sequence from byte stream
 *
 * @param[in]  data     Byte stream
 * @param[in]  size     Byte stream size
 * @param[out] seq      Output CLERS sequence (caller responsible for free)
 * @param[out] seq_len  Output sequence length
 * @param[out] consumed Number of bytes consumed
 * @return true success, false failure
 */
static bool deserialize_clers(const uint8_t *data, size_t size, EdgebreakerMode **seq, int *seq_len, size_t *consumed) {
    if (!data || size < sizeof(int32_t) || !seq || !seq_len || !consumed)
        return false;

    int32_t len;
    memcpy(&len, data, sizeof(int32_t));
    *consumed = sizeof(int32_t);

    if (len <= 0 || (size_t) len > size - sizeof(int32_t)) {
        *seq = NULL;
        *seq_len = 0;
        return true;
    }

    EdgebreakerMode *s = (EdgebreakerMode *) lv_malloc(len * sizeof(EdgebreakerMode));
    if (!s)
        return false;

    for (int i = 0; i < len; i++) {
        s[i] = (EdgebreakerMode) data[sizeof(int32_t) + i];
    }

    *seq = s;
    *seq_len = len;
    *consumed = sizeof(int32_t) + len;
    return true;
}

/**
 * @brief Deserialize coordinate data and create nodes in output graph
 *
 * @param[in]  data     Byte stream (starting after CLERS section)
 * @param[in]  size     Byte stream size
 * @param[out] graph    Output constraint graph
 * @return true success, false failure
 */
static bool deserialize_coords(const uint8_t *data, size_t size, ConstraintGraph *graph) {
    if (!data || size < sizeof(int32_t) || !graph)
        return false;

    const uint8_t *ptr = data;
    const uint8_t *end = data + size;

    int32_t node_count;
    memcpy(&node_count, ptr, sizeof(int32_t));
    ptr += sizeof(int32_t);

    for (int i = 0; i < node_count && ptr + 2 * sizeof(int32_t) <= end; i++) {
        int32_t nid;
        int32_t cc;
        memcpy(&nid, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);
        memcpy(&cc, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);

        if (cc < 0 || cc > 100) /* Sanity check */
            continue;

        SymbolicCoord **coords = NULL;
        if (cc > 0) {
            coords = (SymbolicCoord **) lv_malloc(cc * sizeof(SymbolicCoord *));
            if (!coords)
                return false;

            bool ok = true;
            for (int d = 0; d < cc; d++) {
                if (ptr + sizeof(double) > end) {
                    ok = false;
                    break;
                }
                double val;
                memcpy(&val, ptr, sizeof(double));
                ptr += sizeof(double);
                double scaled_val = val * 1000000.0;
                if (scaled_val > 9223372036854774784.0) scaled_val = 9223372036854774784.0;
                if (scaled_val < -9223372036854774784.0) scaled_val = -9223372036854774784.0;
                coords[d] = symbolic_coord_create_rational((int64_t)scaled_val, 1000000);
                if (!coords[d]) {
                    ok = false;
                    break;
                }
            }

            if (!ok) {
                for (int d = 0; d < cc; d++) {
                    if (coords[d])
                        symbolic_coord_destroy(coords[d]);
                }
                lv_free((void **) &coords);
                return false;
            }
        }

        graph_add_node_with_id(graph, (int) nid, GEOM_POINT, coords, cc);

        /* Clean up coords (graph_add_node_with_id makes its own copy) */
        if (coords) {
            for (int d = 0; d < cc; d++) {
                if (coords[d])
                    symbolic_coord_destroy(coords[d]);
            }
            lv_free((void **) &coords);
        }
    }

    return true;
}

bool geometry_decompress(const uint8_t *data, size_t size, ConstraintGraph **out_graph) {
    if (!data || size == 0 || !out_graph)
        return false;

    /* Step 1: Entropy decode */
    uint8_t *decoded = NULL;
    size_t decoded_size = 0;
    if (!entropy_decode_real(data, size, &decoded, &decoded_size)) {
        return false;
    }

    if (!decoded || decoded_size == 0) {
        lv_free((void **) &decoded);
        *out_graph = graph_create();
        return (*out_graph != NULL);
    }

    /* Step 2: Create output graph */
    ConstraintGraph *graph = graph_create();
    if (!graph) {
        lv_free((void **) &decoded);
        return false;
    }

    /* Step 3: Deserialize CLERS sequence */
    EdgebreakerMode *clers_seq = NULL;
    int clers_len = 0;
    size_t clers_consumed = 0;
    if (!deserialize_clers(decoded, decoded_size, &clers_seq, &clers_len, &clers_consumed)) {
        lv_free((void **) &decoded);
        graph_destroy(graph);
        return false;
    }
    lv_free((void **) &clers_seq); /* We don't need the CLERS sequence for basic deserialization */

    /* Step 4: Deserialize coordinate data and reconstruct nodes */
    /* The combined format is: [clers_serial_size(4B)][clers_serial][coord_data] */
    if (decoded_size < clers_consumed) {
        lv_free((void **) &decoded);
        graph_destroy(graph);
        return false;
    }

    /* Skip CLERS section header (4 bytes) and CLERS data to get to coordinate data */
    if (!deserialize_coords(decoded + clers_consumed, decoded_size - clers_consumed, graph)) {
        lv_free((void **) &decoded);
        graph_destroy(graph);
        return false;
    }

    lv_free((void **) &decoded);

    *out_graph = graph;
    return true;
}

/* ========================================================================
 * .lvzd 二进制文件 I/O
 *
 * 小端序二进制格式：Magic(4B) + Header + Payload + Checksum
 * ======================================================================== */

static void write_uint32_le(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t) (val & 0xFF);
    buf[1] = (uint8_t) ((val >> 8) & 0xFF);
    buf[2] = (uint8_t) ((val >> 16) & 0xFF);
    buf[3] = (uint8_t) ((val >> 24) & 0xFF);
}

static void write_uint64_le(uint8_t *buf, uint64_t val) {
    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t) ((val >> (i * 8)) & 0xFF);
    }
}

static uint32_t read_uint32_le(const uint8_t *buf) {
    return ((uint32_t) buf[0]) | ((uint32_t) buf[1] << 8) | ((uint32_t) buf[2] << 16) | ((uint32_t) buf[3] << 24);
}

static uint64_t read_uint64_le(const uint8_t *buf) {
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= ((uint64_t) buf[i]) << (i * 8);
    }
    return val;
}

bool compress_write_lvzd(const uint8_t *data, size_t size, const char *filename) {
    if (!data || size == 0 || !filename)
        return false;

    FILE *fp = fopen(filename, "wb");
    if (!fp)
        return false;

    /* Build file header */
    uint8_t header[LVZD_HEADER_SIZE];
    memset(header, 0, LVZD_HEADER_SIZE);

    write_uint32_le(header, LVZD_MAGIC);
    write_uint32_le(header + 4, LVZD_VERSION_MAJOR);
    write_uint32_le(header + 8, LVZD_VERSION_MINOR);
    write_uint64_le(header + 12, (uint64_t) size);
    write_uint64_le(header + 20, (uint64_t) size);

    /* Write file header */
    size_t written = fwrite(header, 1, LVZD_HEADER_SIZE, fp);
    if (written != LVZD_HEADER_SIZE) {
        fclose(fp);
        return false;
    }

    /* Write compressed data */
    written = fwrite(data, 1, size, fp);
    fclose(fp);
    return (written == size);
}

bool compress_read_lvzd(const char *filename, uint8_t **out_data, size_t *out_size) {
    if (!filename || !out_data || !out_size)
        return false;

    FILE *fp = fopen(filename, "rb");
    if (!fp)
        return false;

    /* Read file header */
    uint8_t header[LVZD_HEADER_SIZE];
    size_t read_bytes = fread(header, 1, LVZD_HEADER_SIZE, fp);
    if (read_bytes != LVZD_HEADER_SIZE) {
        fclose(fp);
        return false;
    }

    /* Verify magic */
    uint32_t magic = read_uint32_le(header);
    if (magic != LVZD_MAGIC) {
        fclose(fp);
        return false;
    }

    /* Verify version */
    uint32_t ver_major = read_uint32_le(header + 4);
    uint32_t ver_minor = read_uint32_le(header + 8);
    if (ver_major > LVZD_VERSION_MAJOR) {
        fclose(fp);
        return false;
    }
    (void) ver_minor;

    /* Read compressed data size */
    uint64_t comp_size = read_uint64_le(header + 20);
    if (comp_size == 0) {
        fclose(fp);
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    /* Allocate buffer and read compressed data */
    uint8_t *buf = (uint8_t *) lv_malloc((size_t) comp_size);
    if (!buf) {
        fclose(fp);
        return false;
    }

    read_bytes = fread(buf, 1, (size_t) comp_size, fp);
    fclose(fp);

    if (read_bytes != (size_t) comp_size) {
        lv_free((void **) &buf);
        return false;
    }

    *out_data = buf;
    *out_size = (size_t) comp_size;
    return true;
}
