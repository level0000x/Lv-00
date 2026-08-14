/*
 * @file geometry_compress_main.c
 * @brief Geometry compression engine - compress pipeline
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
 * Geometry compression main API
 *
 * 完整压缩流水线：graph_copy → edgebreaker → predict → entropy → pack
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
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "serialize_coords_raw: graph or out_size is NULL");

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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "serialize_coords_raw: malloc failed");

    /* 端序说明：与读端 deserialize_coords（lv_load_le32 / lv_load_le64）约定一致，
     * 显式小端序写入（x86 上输出与原先 memcpy 主机序逐字节一致） */
    uint8_t *ptr = buf;
    int32_t count = (int32_t) graph->node_count;
    lv_store_le32(ptr, (uint32_t) count);
    ptr += sizeof(int32_t);

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;

        int32_t nid = (int32_t) node->id;
        int32_t cc = (int32_t) node->coord_count;
        lv_store_le32(ptr, (uint32_t) nid);
        ptr += sizeof(int32_t);
        lv_store_le32(ptr, (uint32_t) cc);
        ptr += sizeof(int32_t);

        for (int d = 0; d < node->coord_count; d++) {
            double val = symbolic_coord_to_double(node->symbolic_coords[d]);
            uint64_t bits;
            memcpy(&bits, &val, sizeof(double)); /* 取位模式，按小端序写 8 字节 */
            lv_store_le64(ptr, bits);
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
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "serialize_clers: out_size is NULL");

    if (!seq || seq_len <= 0) {
        *out_size = 0;
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "serialize_clers: seq is NULL or len <= 0");
    }

    size_t total = sizeof(int32_t) + seq_len;
    uint8_t *buf = (uint8_t *) lv_malloc(total);
    if (!buf)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "serialize_clers: malloc failed");

    int32_t len = (int32_t) seq_len;
    lv_store_le32(buf, (uint32_t) len); /* 小端序，与读端 deserialize_clers 的 lv_load_le32 约定一致 */

    for (int i = 0; i < seq_len; i++) {
        buf[sizeof(int32_t) + i] = (uint8_t) seq[i];
    }

    *out_size = total;
    return buf;
}

typedef bool (*EntropyEncodeFn)(const uint8_t *data, size_t size, uint8_t **out, size_t *out_size);

static bool encode_none(const uint8_t *data, size_t size, uint8_t **out, size_t *out_size) {
    *out = (uint8_t *)data;  /* transfer ownership */
    *out_size = size;
    return true;
}
static bool encode_huffman(const uint8_t *data, size_t size, uint8_t **out, size_t *out_size) {
    return entropy_encode_huffman(data, size, out, out_size);
}
static bool encode_rans(const uint8_t *data, size_t size, uint8_t **out, size_t *out_size) {
    return entropy_encode_real(data, size, out, out_size);
}
static bool encode_arithmetic(const uint8_t *data, size_t size, uint8_t **out, size_t *out_size) {
    return entropy_encode_real(data, size, out, out_size);
}

static const EntropyEncodeFn kEntropyEncoders[] = {
    [ENTROPY_NONE] = encode_none,
    [ENTROPY_HUFFMAN] = encode_huffman,
    [ENTROPY_RANS] = encode_rans,
    [ENTROPY_ARITHMETIC] = encode_arithmetic,
};

bool geometry_compress(const ConstraintGraph *graph, const CompressConfig *config, uint8_t **out_data, size_t *out_size,
                       CompressMetadata *out_meta) {
    if (!graph || !out_data || !out_size)
        return false;

    CompressConfig cfg = config ? *config : compress_config_default();

    /* Step 1: Estimate original size */
    size_t original_sz = estimate_original_size(graph);

    /* Step 2: Deep copy constraint graph for in-place modification (predictive encoding modifies coordinates) */
    ConstraintGraph *work_graph = graph_copy(graph);
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
    lv_store_le32(combined, csz);

    /* Copy CLERS data */
    if (clers_serial && clers_serial_size > 0) {
        memcpy(combined + combined_header, clers_serial, clers_serial_size);
    }
    lv_free((void **) &clers_serial);

    /* Copy coordinate data */
    memcpy(combined + combined_header + clers_serial_size, raw_buf, raw_size);
    lv_free((void **) &raw_buf);

    /* Apply entropy encoding based on configuration */
    uint8_t *encoded = NULL;
    size_t encoded_size = 0;
    bool enc_ok = false;

    if ((unsigned)cfg.entropy < sizeof(kEntropyEncoders)/sizeof(kEntropyEncoders[0]) && kEntropyEncoders[cfg.entropy]) {
        enc_ok = kEntropyEncoders[cfg.entropy](combined, combined_size, &encoded, &encoded_size);
        if (cfg.entropy == ENTROPY_NONE) combined = NULL; /* ownership transferred */
    } else {
        enc_ok = entropy_encode_real(combined, combined_size, &encoded, &encoded_size);
    }

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
        out_meta->original_size = (int) original_sz;
        out_meta->compressed_size = (int) encoded_size;
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

