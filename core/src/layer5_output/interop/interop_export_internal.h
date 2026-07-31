/**
 * @file interop_export_internal.h
 * @brief 互操作导出内部共享声明（interop_export_pdf.c 与 interop_export.c 共用）
 */

#ifndef lv_INTEROP_EXPORT_INTERNAL_H
#define lv_INTEROP_EXPORT_INTERNAL_H

#include "lv/constraint_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 计算约束图的包围盒（SVG 与 PDF 导出共用） */
void compute_bounding_box(const ConstraintGraph *graph, double *min_x, double *min_y, double *max_x,
                          double *max_y);

#ifdef __cplusplus
}
#endif

#endif /* lv_INTEROP_EXPORT_INTERNAL_H */
