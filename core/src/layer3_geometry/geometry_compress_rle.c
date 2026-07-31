/*
 * @file geometry_compress_rle.c
 * @brief Geometry compression engine - run-length encoding
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
bool rle_encode(const uint8_t *data, size_t size, uint8_t **out_data, size_t *out_size) {
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
bool rle_decode(const uint8_t *data, size_t size, uint8_t **out_data, size_t *out_size) {
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

