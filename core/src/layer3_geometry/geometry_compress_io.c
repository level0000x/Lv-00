/*
 * @file geometry_compress_io.c
 * @brief Geometry compression engine - lvzd file I/O
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
 * .lvzd 二进制文件 I/O
 *
 * 小端序二进制格式：Magic(4B) + Header + Payload + Checksum
 * ======================================================================== */

static void write_uint32_le(uint8_t *buf, uint32_t val) {
    lv_store_le32(buf, val);
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

    FILE *fp = lv_file_open(filename, "wb");
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
        lv_file_close(fp);
        return false;
    }

    /* Write compressed data */
    written = fwrite(data, 1, size, fp);
    lv_file_close(fp);
    return (written == size);
}

bool compress_read_lvzd(const char *filename, uint8_t **out_data, size_t *out_size) {
    if (!filename || !out_data || !out_size)
        return false;

    FILE *fp = lv_file_open(filename, "rb");
    if (!fp)
        return false;

    /* Read file header */
    uint8_t header[LVZD_HEADER_SIZE];
    size_t read_bytes = fread(header, 1, LVZD_HEADER_SIZE, fp);
    if (read_bytes != LVZD_HEADER_SIZE) {
        lv_file_close(fp);
        return false;
    }

    /* Verify magic */
    uint32_t magic = read_uint32_le(header);
    if (magic != LVZD_MAGIC) {
        lv_file_close(fp);
        return false;
    }

    /* Verify version */
    uint32_t ver_major = read_uint32_le(header + 4);
    uint32_t ver_minor = read_uint32_le(header + 8);
    if (ver_major > LVZD_VERSION_MAJOR) {
        lv_file_close(fp);
        return false;
    }
    (void) ver_minor;

    /* Read compressed data size */
    uint64_t comp_size = read_uint64_le(header + 20);
    if (comp_size == 0) {
        lv_file_close(fp);
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    /* Allocate buffer and read compressed data */
    uint8_t *buf = (uint8_t *) lv_malloc((size_t) comp_size);
    if (!buf) {
        lv_file_close(fp);
        return false;
    }

    read_bytes = fread(buf, 1, (size_t) comp_size, fp);
    lv_file_close(fp);

    if (read_bytes != (size_t) comp_size) {
        lv_free((void **) &buf);
        return false;
    }

    *out_data = buf;
    *out_size = (size_t) comp_size;
    return true;
}
