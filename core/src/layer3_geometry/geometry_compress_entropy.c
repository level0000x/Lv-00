/*
 * @file geometry_compress_entropy.c
 * @brief Geometry compression engine - real entropy coding
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
bool entropy_encode_real(const uint8_t *raw_data, size_t raw_size, uint8_t **out_data, size_t *out_size) {
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
    lv_store_le32(output, (uint32_t) LVZD_COMPRESS_MAGIC);

    /* Write original size */
    uint32_t orig_sz = (uint32_t) raw_size;
    lv_store_le32(output + 4, orig_sz);

    /* Write RLE size */
    uint32_t rle_sz = (uint32_t) rle_size;
    lv_store_le32(output + 8, rle_sz);

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
bool entropy_decode_real(const uint8_t *data, size_t size, uint8_t **out_data, size_t *out_size) {
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
    uint32_t magic =
        ((uint32_t) data[0]) | ((uint32_t) data[1] << 8) | ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
    if (magic != LVZD_COMPRESS_MAGIC) {
        /* Fall back to legacy Huffman-only format */
        return entropy_decode_huffman(data, size, out_data, out_size);
    }

    uint32_t orig_sz =
        ((uint32_t) data[4]) | ((uint32_t) data[5] << 8) | ((uint32_t) data[6] << 16) | ((uint32_t) data[7] << 24);
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

