/*
 * @file geometry_compress_decompress.c
 * @brief Geometry compression engine - decompress pipeline
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

#include "lv_internal.h"
#include "lv_utils.h"
#include "node_deep_copy.h"
#include "symbolic_coord.h"

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

    int32_t len = (int32_t)lv_load_le32(data);
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

    int32_t node_count = (int32_t)lv_load_le32(ptr);
    ptr += sizeof(int32_t);

    for (int i = 0; i < node_count && ptr + 2 * sizeof(int32_t) <= end; i++) {
        int32_t nid = (int32_t)lv_load_le32(ptr);
        ptr += sizeof(int32_t);
        int32_t cc = (int32_t)lv_load_le32(ptr);
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
                coords[d] = symbolic_coord_from_double_scaled(val, lv_RATIONAL_SCALE_DEFAULT);
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

