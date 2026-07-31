/*
 * @file geometry_compress_bitio.c
 * @brief Geometry compression engine - bit-level I/O
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
 * Huffman encoder/decoder
 * ======================================================================== */

bool bitwriter_write_bit(BitWriter *bw, int bit) {
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

bool bitwriter_write_bits(BitWriter *bw, uint32_t code, int bit_count) {
    for (int i = bit_count - 1; i >= 0; i--) {
        if (!bitwriter_write_bit(bw, (code >> i) & 1))
            return false;
    }
    return true;
}

size_t bitwriter_flush(const BitWriter *bw) {
    return (bw->bit_pos < 7) ? bw->byte_pos + 1 : bw->byte_pos;
}

/* ========================================================================
 * Bit reader operations
 * ======================================================================== */

void bitreader_init(BitReader *br, const uint8_t *buf, size_t size) {
    br->buf = buf;
    br->size = size;
    br->byte_pos = 0;
    br->bit_pos = 7;
}

int bitreader_read_bit(BitReader *br) {
    if (br->byte_pos >= br->size)
        lv_RETURN_ERROR(lv_ERROR_OVERFLOW, "bitreader_read_bit: past end of buffer");
    int bit = (br->buf[br->byte_pos] >> br->bit_pos) & 1;
    br->bit_pos--;
    if (br->bit_pos < 0) {
        br->bit_pos = 7;
        br->byte_pos++;
    }
    return bit;
}

