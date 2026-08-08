/**
 * @file lv_export_common.h
 * @brief 导出公共工具（layer1 与 layer5 导出器共用）
 *
 * @details 收敛跨层导出器的重复样板：
 *   1. 定长缓冲 XML 转义（SVG/HTML 导出共用；实体语义与 lv_str_escape_xml 一致）
 *   2. 文件写出辅助（fopen "w" + fwrite + fclose 样板）
 *   其余转义（HTML/LaTeX/JSON）与公共颜色表（kConstraintVisuals /
 *   kTrustColorEntries）分别位于 lv_str_utils.h 与 layer5_output/interop/
 *   interop_export_internal.h，此处不再重复定义。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef lv_EXPORT_COMMON_H
#define lv_EXPORT_COMMON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 将字符串按 XML 实体转义后写入定长缓冲（snprintf 语义）
 *
 * @details 转义表与 lv_str_escape_xml 完全一致（& < > " ' 分别转义为
 *          &amp; &lt; &gt; &quot; &apos;）。空间不足时截断到 dst_size-1
 *          并 NUL 结尾，保证输出字节与历史实现逐字节一致。
 *
 * @param src      源字符串（C 字符串，NULL 时直接返回）
 * @param dst      目标缓冲（NULL 或 dst_size==0 时直接返回）
 * @param dst_size 目标缓冲大小（字节）
 */
void lv_export_xml_escape(const char *src, char *dst, size_t dst_size);

/**
 * @brief 将内存缓冲一次性写入文件（fopen "w" + fwrite + fclose 样板）
 *
 * @param path 目标文件路径
 * @param data 待写入数据（可为 NULL，此时写入 0 字节）
 * @param len  数据长度（字节）
 * @return 实际写入字节数；文件打开失败返回 -1
 */
int lv_export_write_file(const char *path, const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* lv_EXPORT_COMMON_H */
