/**
 * @file interop_import_internal.h
 * @brief interop 导入子模块共享内部声明（由 interop_import.c 拆分子模块）
 *
 * @details 供 interop_import_ggb_zip.c / interop_import_ggb_xml.c /
 *          interop_import_svg.c 共享跨子模块的内部函数。
 *          公共 API（interop_import_geogebra / geojson / svg）仍由 lv/interop.h 声明。
 */

#ifndef lv_INTEROP_IMPORT_INTERNAL_H
#define lv_INTEROP_IMPORT_INTERNAL_H

#include "lv/constraint_graph.h" /* SymbolicCoord / ConstraintGraph 图 API */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── interop_import_ggb_zip.c 实现：ZIP 解析/解压，供 ggb_xml 模块的
 * interop_import_geogebra 复用 ── */
bool ggb_find_eocd(const uint8_t *data, size_t size, size_t *eocd_offset);
bool ggb_central_find_entry(const uint8_t *data, size_t data_size, size_t eocd_offset,
                            const char *target, size_t *local_offset, size_t *comp_size,
                            size_t *uncomp_size, uint16_t *comp_method);
bool ggb_local_data_offset(const uint8_t *data, size_t data_size, size_t local_offset,
                           size_t *data_offset);
bool ggb_extract_entry(const uint8_t *data, size_t data_size, size_t data_offset,
                       size_t comp_size, size_t uncomp_size, uint16_t comp_method,
                       uint8_t **out_buf, size_t *out_len);

/* ── interop_import_ggb_xml.c 实现：XML 属性提取 / double 有理化，供 svg 模块复用 ── */
bool ggb_extract_attr(const char *tag_start, size_t tag_len, const char *attr_name, char *out_value,
                      size_t out_size);
SymbolicCoord *ggb_double_to_rational(double value);

/* ── interop_import_svg.c 实现：SVG 圆采样，供 ggb_xml 模块的圆导入复用 ── */
int svg_parse_circle(double cx, double cy, double r, double *out_points, int max_points);

#ifdef __cplusplus
}
#endif

#endif /* lv_INTEROP_IMPORT_INTERNAL_H */
