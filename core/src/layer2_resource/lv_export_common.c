/**
 * @file lv_export_common.c
 * @brief 导出公共工具实现（见 lv_export_common.h）
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/lv_export_common.h"

#include <stdio.h>
#include <string.h>

#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"

void lv_export_xml_escape(const char *src, char *dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0)
        return;
    lvStrBuf sb = {0};
    lv_str_escape_xml(&sb, src, strlen(src));
    size_t n = sb.len;
    if (n >= dst_size)
        n = dst_size - 1;
    memcpy(dst, lv_strbuf_cstr(&sb), n);
    dst[n] = '\0';
    lv_strbuf_destroy(&sb);
}

int lv_export_write_file(const char *path, const void *data, size_t len) {
    if (!path)
        return -1;
    FILE *fp = fopen(path, "w");
    if (!fp)
        return -1;
    size_t written = fwrite(data, 1, len, fp);
    fclose(fp);
    return (int) written;
}
