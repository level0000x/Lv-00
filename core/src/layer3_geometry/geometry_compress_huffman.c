/*
 * @file geometry_compress_huffman.c
 * @brief Geometry compression engine - huffman coding
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
 * Huffman tree building (shared kernel from 3 copies)
 * ======================================================================== */

/**
 * @brief Build Huffman tree from frequency table
 *
 * Shared kernel converging 3 copies in entropy_encode_huffman,
 * entropy_decode_huffman, and predictive_encode_parallelogram.
 *
 * @param[in]  freq   256-element frequency table
 * @param[out] hnodes Pre-allocated HUFFMAN_MAX_NODES array (filled with tree)
 * @return root node index on success, -1 on failure (no symbols or OOM)
 */
int huffman_tree_build(const uint32_t freq[256], HuffmanNode hnodes[HUFFMAN_MAX_NODES]) {
    memset(hnodes, 0, HUFFMAN_MAX_NODES * sizeof(HuffmanNode));
    int node_count = 0;

    /* Initialize leaf nodes from frequency table */
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

    /* Single-symbol case: create a dummy parent */
    if (node_count == 1) {
        hnodes[node_count].left = 0;
        hnodes[node_count].right = -1;
        hnodes[node_count].parent = -1;
        hnodes[node_count].freq = hnodes[0].freq;
        hnodes[node_count].byte_val = 0;
        hnodes[0].parent = node_count;
        node_count++;
    }

    if (node_count < 2)
        return -1;

    /* Build min-heap and combine lowest-frequency nodes */
    lvHeap heap;
    if (!lv_heap_init(&heap, sizeof(HuffHeapElem), lv_MIN_HEAP, huff_heap_compare, HUFFMAN_MAX_NODES))
        return -1;

    for (int i = 0; i < node_count; i++) {
        HuffHeapElem e = {i, hnodes[i].freq};
        lv_heap_push(&heap, &e);
    }

    while (lv_heap_size(&heap) > 1) {
        HuffHeapElem le, re;
        lv_heap_pop(&heap, &le);
        lv_heap_pop(&heap, &re);
        int left = le.node_index;
        int right = re.node_index;

        hnodes[node_count].left = left;
        hnodes[node_count].right = right;
        hnodes[node_count].parent = -1;
        hnodes[node_count].freq = hnodes[left].freq + hnodes[right].freq;
        hnodes[node_count].byte_val = 0;
        hnodes[left].parent = node_count;
        hnodes[right].parent = node_count;

        HuffHeapElem ne = {node_count, hnodes[node_count].freq};
        lv_heap_push(&heap, &ne);
        node_count++;
    }

    HuffHeapElem root_elem;
    lv_heap_pop(&heap, &root_elem);
    lv_heap_destroy(&heap);
    return root_elem.node_index;
}

/* ========================================================================
 * Huffman encoding table generation
 * ======================================================================== */

void huffman_generate_codes(const HuffmanNode *hnodes, int root, HuffmanCode *codes) {
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
bool entropy_encode_huffman(const uint8_t *raw_data, size_t raw_size, uint8_t **out_data, size_t *out_size) {
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
    int root = huffman_tree_build(freq, hnodes);
    if (root < 0)
        return false;

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
        lv_store_le32(output + i * 4, f);
    }

    /* Write original size */
    size_t offset = 256 * sizeof(uint32_t);
    uint32_t raw_sz = (uint32_t) raw_size;
    lv_store_le32(output + offset, raw_sz);
    offset += sizeof(uint32_t);

    /* Use a separate buffer for bit writer to avoid realloc on interior pointer */
    uint8_t *bw_buf = (uint8_t *)lv_malloc(bitstream_capacity);
    if (!bw_buf) { lv_free((void**)&output); return false; }
    BitWriter bw;
    bitwriter_init(&bw, bw_buf, bitstream_capacity);

    for (size_t i = 0; i < raw_size; i++) {
        uint8_t byte_val = raw_data[i];
        HuffmanCode *hc = &codes[byte_val];
        if (hc->length == 0) {
            lv_free((void **) &output);
            lv_free((void **) &bw_buf);
            return false;
        }
        if (!bitwriter_write_bits(&bw, hc->code, hc->length)) {
            lv_free((void **) &output);
            lv_free((void **) &bw_buf);
            return false;
        }
    }

    size_t bitstream_bytes = bitwriter_flush(&bw);
    memcpy(output + offset, bw_buf, bitstream_bytes);
    lv_free((void **) &bw_buf);
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
bool entropy_decode_huffman(const uint8_t *data, size_t size, uint8_t **out_data, size_t *out_size) {
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
        freq[i] = lv_load_le32(data + i * 4); /* 小端序，与写端 lv_store_le32 约定一致 */
    }

    size_t offset = 256 * sizeof(uint32_t);
    uint32_t raw_sz = lv_load_le32(data + offset);
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
    int root = huffman_tree_build(freq, hnodes);
    if (root < 0) {
        lv_free((void **) &output);
        return false;
    }

    /* Step 2: Bit-by-bit decode */
    BitReader br;
    bitreader_init(&br, data + offset, size - offset);

    size_t decoded = 0;

    if (hnodes[root].right < 0) {
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

