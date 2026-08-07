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

bool compress_write_lvzd(const uint8_t *data, size_t size, const char *filename) {
    if (!data || size == 0 || !filename)
        return false;

    FILE *fp = lv_file_open(filename, "wb");
    if (!fp)
        return false;

    /* Build file header */
    uint8_t header[LVZD_HEADER_SIZE];
    memset(header, 0, LVZD_HEADER_SIZE);

    lv_store_le32(header, LVZD_MAGIC);
    lv_store_le32(header + 4, LVZD_VERSION_MAJOR);
    lv_store_le32(header + 8, LVZD_VERSION_MINOR);
    lv_store_le64(header + 12, (uint64_t) size);
    lv_store_le64(header + 20, (uint64_t) size);

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
    uint32_t magic = lv_load_le32(header);
    if (magic != LVZD_MAGIC) {
        lv_file_close(fp);
        return false;
    }

    /* Verify version */
    uint32_t ver_major = lv_load_le32(header + 4);
    uint32_t ver_minor = lv_load_le32(header + 8);
    if (ver_major > LVZD_VERSION_MAJOR) {
        lv_file_close(fp);
        return false;
    }
    (void) ver_minor;

    /* Read compressed data size */
    uint64_t comp_size = lv_load_le64(header + 20);
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
