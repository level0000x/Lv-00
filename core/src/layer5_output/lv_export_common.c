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
#include "lv/lv_utils.h"

void lv_export_xml_escape(const char *src, char *dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0)
        return;
    lvStrBuf sb = {0};
    lv_str_escape_xml(&sb, src, strlen(src));
    lv_strlcpy(dst, lv_strbuf_cstr(&sb), dst_size);
    lv_strbuf_destroy(&sb);
}

/**
 * @brief 将数据写入文件（与 lv_file_write_all 功能相近，语义不同）
 *
 * 差异说明（有意保持，勿随意合并）：
 *  - 打开模式：本函数使用文本模式 "w"（Windows 下 \n 会转换为 \r\n），
 *    lv_file_write_all 使用二进制模式 "wb"（不做换行转换）。
 *  - 返回值：本函数返回实际写入的字节数（int，可能小于 len），
 *    lv_file_write_all 仅返回 0 成功 / -1 失败，不报告写入字节数。
 *  - 错误处理：本函数直接使用 fopen/fclose，无 lv_file 抽象的错误日志；
 *    lv_file_write_all 走 lv_file_open/lv_file_close 并记录 lv_ERROR。
 * 调用方 tikz_export.c 依赖返回值判断写入量，合并前需同步调整其语义。
 */
int lv_export_write_file(const char *path, const void *data, size_t len) {
    if (!path)
        return -1;
    FILE *fp = fopen(path, "w");
    if (!fp)
        return -1;
    /* 契约：data 可为 NULL，此时写入 0 字节。
     * fwrite(NULL, 1, n, fp) 是未定义行为（glibc 下直接 SEGFAULT），
     * 必须先判空（此前违反契约导致 Ubuntu CI SEGFAULT）。 */
    size_t written = 0;
    if (data != NULL && len > 0)
        written = fwrite(data, 1, len, fp);
    fclose(fp);
    return (int) written;
}
