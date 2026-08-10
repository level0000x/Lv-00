/**
 * @file interop_export.c
 * @brief 导出（Coq/Lean/HTML/SVG/TikZ/GeoJSON/PDF/Canonical）
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/geo_utils.h"
#include "lv/interop.h"
#include "lv/lv_json.h"

#include "debug.h"
#include "interop_export_internal.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"

/** @brief 单个约束节点涉及的最大约束数量（统一在 interop.h 中定义） */

/* ── 导出模块 ── */

/**
 * @brief 计算图的边界框（用于 SVG viewBox 和 PDF 页面尺寸）
 * @details 遍历约束图中所有节点的坐标，计算最小/最大 x、y 值，
 *          并添加自适应边距。
 * @param graph 约束图指针（可为 NULL）
 * @param min_x [out] 最小 x 坐标
 * @param min_y [out] 最小 y 坐标
 * @param max_x [out] 最大 x 坐标
 * @param max_y [out] 最大 y 坐标
 */
void compute_bounding_box(const ConstraintGraph *graph, double *min_x, double *min_y, double *max_x,
                                 double *max_y) {
    *min_x = 0.0;
    *min_y = 0.0;
    *max_x = 100.0;
    *max_y = 100.0;

    if (!graph || graph->node_count == 0)
        return;

    bool first = true;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->coord_count < 2 || !node->symbolic_coords)
            continue;

        /* 修复：添加 symbolic_coords 数组元素的 NULL 检查 */
        if (!node->symbolic_coords[0] || !node->symbolic_coords[1])
            continue;

        double x = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y = symbolic_coord_to_double(node->symbolic_coords[1]);

        if (first) {
            *min_x = x;
            *min_y = y;
            *max_x = x;
            *max_y = y;
            first = false;
        } else {
            if (x < *min_x)
                *min_x = x;
            if (y < *min_y)
                *min_y = y;
            if (x > *max_x)
                *max_x = x;
            if (y > *max_y)
                *max_y = y;
        }
    }

    /* 添加边距 */
    double margin_x = (*max_x - *min_x) * 0.15 + 20.0;
    double margin_y = (*max_y - *min_y) * 0.15 + 20.0;
    *min_x -= margin_x;
    *min_y -= margin_y;
    *max_x += margin_x;
    *max_y += margin_y;
}

/**
 * @brief 计算贝塞尔曲线的两个控制点（SVG cubic 与 PDF c 操作符共用）
 * @details 原实现分别在 interop_export_svg.c 与 interop_export_pdf.c 中逐字重复，
 *          现统一收敛到此处。公式与历史行为完全一致：
 *          CP1 = P1 + 0.3*(P2-P1) + 垂直法向偏移；
 *          CP2 = P2 - 0.3*(P2-P1) + 垂直法向偏移。
 *          法向 n = (-dy, dx) / (|P2-P1| + 0.001)，偏移量 0.15*|P2-P1|（过短固定 5.0）。
 * @param p1x,p1y 起点坐标
 * @param p2x,p2y 终点坐标
 * @param cp1x,cp1y [out] 第一控制点
 * @param cp2x,cp2y [out] 第二控制点
 */
void compute_bezier_control_points(double p1x, double p1y, double p2x, double p2y,
                                   double *cp1x, double *cp1y, double *cp2x, double *cp2y) {
    double dx = p2x - p1x;
    double dy = p2y - p1y;
    double dist = geo_distance_2d(p1x, p1y, p2x, p2y);

    double offset = 0.15 * dist;
    if (offset < 0.01)
        offset = 5.0;

    double nx = -dy / (dist + lv_GEO_LENGTH_GUARD);
    double ny = dx / (dist + lv_GEO_LENGTH_GUARD);

    *cp1x = p1x + 0.3 * dx + nx * offset;
    *cp1y = p1y + 0.3 * dy + ny * offset;
    *cp2x = p2x - 0.3 * dx + nx * offset;
    *cp2y = p2y - 0.3 * dy + ny * offset;
}

/**
 * @brief 计算两条线段的交点（SVG 相交约束渲染共用）
 * @details 解线段参数方程：P1 + t*(P2-P1) = Q1 + s*(Q2-Q1)。
 *          平行/共线（|cross|<=1e-10）或交点参数 t 超出 [-0.05, 1.05] 范围时
 *          返回 false 且不写 ix/iy（与原 SVG 内联实现语义一致）。
 * @return true 交点有效；false 无有效交点
 */
bool segment_intersection(double p1x, double p1y, double p2x, double p2y,
                          double q1x, double q1y, double q2x, double q2y,
                          double *ix, double *iy) {
    double d1x = p2x - p1x, d1y = p2y - p1y;
    double d2x = q2x - q1x, d2y = q2y - q1y;
    double cross = d1x * d2y - d1y * d2x;

    if (fabs(cross) > lv_EPSILON_HIGH) {
        double dx0 = q1x - p1x;
        double dy0 = q1y - p1y;
        double t = (dx0 * d2y - dy0 * d2x) / cross;
        if (t >= -0.05 && t <= 1.05) {
            *ix = p1x + t * d1x;
            *iy = p1y + t * d1y;
            return true;
        }
    }
    return false;
}

/**
 * @brief SVG转义XML特殊字符
 *
 * 将字符串中的 XML 特殊字符（&、<、>、"、'）转义为对应的实体引用，
 * 防止在 SVG/XML 输出中出现解析错误。
 *
 * @param src      源字符串
 * @param dst      输出缓冲区，用于存储转义后的字符串
 * @param dst_size 输出缓冲区大小（字节）
 */

/* ==================== GeoGebra 导入辅助：ZIP 解析 ==================== */

/**
 * @brief ZIP 文件结构常量
 *
 * ZIP 格式规范（PKWARE APPNOTE.TXT）定义的核心结构签名。
 * 所有多字节整数均为小端序（Little-Endian）。
 */
#define GGB_LOCAL_FILE_SIG 0x04034b50U      /**< 本地文件头签名 */
#define GGB_CENTRAL_DIR_SIG 0x02014b50U     /**< 中央目录签名 */
#define GGB_EOCD_SIG 0x06054b50U            /**< 结束中心目录签名 */
#define GGB_LOCAL_HEADER_MIN 30             /**< 本地文件头最小字节数 */
#define GGB_CENTRAL_DIR_MIN 46              /**< 中央目录条目最小字节数 */
#define GGB_EOCD_MIN_SIZE 22                /**< EOCD 最小字节数 */
#define GGB_MAX_XML_SIZE (16 * 1024 * 1024) /**< XML 最大大小 16MB */
#define GGB_COMPRESSION_STORE 0             /**< 无压缩（STORE） */
#define GGB_COMPRESSION_DEFLATE 8           /**< Deflate 压缩 */
