/**
 * @file tikz_export.c
 * @brief TikZ/LaTeX 导出 —— 将约束图导出为 TikZ 绘图代码
 *
 * @details 从约束图中提取几何节点（点、线段、圆），生成 TikZ picture 描述。
 *          提供两种导出方式：
 *          - lv_tikz_export：导出到内存缓冲区
 *          - lv_tikz_export_file：导出到文件
 *          内部使用动态字符串构建器（lvStrBuf）管理输出缓冲区。
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */

#include "tikz_export.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "lv/lv_check.h"

#include "lv/constraint_graph.h"
#include "lv/lv_utils.h"
#include "lv/symbolic_coord.h"

#include "lv_internal.h"
#include "lv/lv_strbuf.h"

/* ── 核心导出 ── */

/**
 * @brief 将约束图导出为 TikZ 代码到内存缓冲区
 *
 * @details 遍历约束图中的所有活跃节点，按类型生成对应的 TikZ 命令：
 *          - GEOM_POINT：生成 \\fill circle 命令绘制圆点
 *          - GEOM_LINE_SEGMENT：生成 \\draw -- 命令绘制线段
 *          其他节点类型暂不导出。
 *
 * @param graph   约束图指针（ConstraintGraph*）
 * @param out     输出缓冲区
 * @param buf_size 输出缓冲区大小
 * @return 成功时返回写入缓冲区的字符数（不含终止符），失败返回 -1
 */
int lv_tikz_export(void *graph, char *out, size_t buf_size) {
    lv_CHECK_NOT_NULL(graph);
    lv_CHECK_NOT_NULL(out);
    lv_CHECK_ARG(buf_size > 0, lv_ERROR_INVALID_PARAM, "buf_size is 0");

    ConstraintGraph *g = (ConstraintGraph *) graph;
    int written = 0;

    /* 写入 TikZ 头部 */
    int n = snprintf(out, buf_size,
                     "%% Lv-00 TikZ Export\n"
                     "\\begin{tikzpicture}[scale=1.0, x=1cm, y=1cm]\n");
    if (n < 0 || (size_t) n >= buf_size)
        lv_RETURN_ERROR(lv_ERROR_IO, "lv_tikz_export: header snprintf failed");
    written = n;

    /* 遍历所有节点，按类型导出 */
    for (int i = 0; i < g->node_count; i++) {
        GeomNode *node = g->nodes[i];
        if (!node || !node->is_active)
            continue;

        switch (node->type) {
            case GEOM_POINT: {
                if (node->coord_count >= 2 && node->symbolic_coords && node->symbolic_coords[0] &&
                    node->symbolic_coords[1]) {
                    double x = symbolic_coord_to_double(node->symbolic_coords[0]);
                    double y = symbolic_coord_to_double(node->symbolic_coords[1]);
                    n = snprintf(out + written, buf_size - (size_t) written, "  \\fill (%.4f, %.4f) circle (2pt);\n", x,
                                 y);
                    if (n < 0)
                        lv_RETURN_ERROR(lv_ERROR_IO, "lv_tikz_export: point snprintf failed");
                    written += n;
                }
                break;
            }
            case GEOM_LINE_SEGMENT: {
                if (node->coord_count >= 4 && node->symbolic_coords && node->symbolic_coords[0] &&
                    node->symbolic_coords[1] && node->symbolic_coords[2] && node->symbolic_coords[3]) {
                    double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
                    double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
                    double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
                    double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);
                    n = snprintf(out + written, buf_size - (size_t) written, "  \\draw (%.4f, %.4f) -- (%.4f, %.4f);\n",
                                 x1, y1, x2, y2);
                    if (n < 0)
                        lv_RETURN_ERROR(lv_ERROR_IO, "lv_tikz_export: segment snprintf failed");
                    written += n;
                }
                break;
            }
            default:
                /* 其他类型暂不导出 */
                break;
        }
    }

    /* 写入尾部 */
    n = snprintf(out + written, buf_size - (size_t) written, "\\end{tikzpicture}\n");
    if (n < 0)
        lv_RETURN_ERROR(lv_ERROR_IO, "lv_tikz_export: footer snprintf failed");
    written += n;

    return written;
}

/**
 * @brief 将约束图导出为 TikZ 代码到文件
 *
 * @details 使用内部缓冲区（lvStrBuf）在内存中构建完整的 TikZ 代码，
 *          然后一次性写入文件。相比 lv_tikz_export，此函数自动管理
 *          缓冲区大小，适合大型约束图导出。
 *
 * @param graph   约束图指针（ConstraintGraph*）
 * @param filename 目标文件路径
 * @return 成功时返回写入的字节数，失败返回 -1
 */
int lv_tikz_export_file(void *graph, const char *filename) {
    if (!graph || !filename)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_tikz_export_file: graph or filename is NULL");

    /* 先用缓冲区构建输出 */
    lvStrBuf buf;
    lv_strbuf_init(&buf);

    /* 写入头部 */
    lv_strbuf_printf(&buf,
                     "%s",
                     "% Lv-00 TikZ Export\n"
                     "\\begin{tikzpicture}[scale=1.0, x=1cm, y=1cm]\n");

    ConstraintGraph *g = (ConstraintGraph *) graph;
    for (int i = 0; i < g->node_count; i++) {
        GeomNode *node = g->nodes[i];
        if (!node || !node->is_active)
            continue;

        switch (node->type) {
            case GEOM_POINT: {
                if (node->coord_count >= 2 && node->symbolic_coords && node->symbolic_coords[0] &&
                    node->symbolic_coords[1]) {
                    double x = symbolic_coord_to_double(node->symbolic_coords[0]);
                    double y = symbolic_coord_to_double(node->symbolic_coords[1]);
                    lv_strbuf_printf(&buf, "  \\fill (%.4f, %.4f) circle (2pt);\n", x, y);
                }
                break;
            }
            case GEOM_LINE_SEGMENT: {
                if (node->coord_count >= 4 && node->symbolic_coords && node->symbolic_coords[0] &&
                    node->symbolic_coords[1] && node->symbolic_coords[2] && node->symbolic_coords[3]) {
                    double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
                    double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
                    double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
                    double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);
                    lv_strbuf_printf(&buf, "  \\draw (%.4f, %.4f) -- (%.4f, %.4f);\n", x1, y1, x2, y2);
                }
                break;
            }
            default:
                break;
        }
    }

    lv_strbuf_printf(&buf, "\\end{tikzpicture}\n");

    /* 写入文件 */
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        lv_strbuf_destroy(&buf);
        lv_RETURN_ERROR(lv_ERROR_IO, "lv_tikz_export_file: fopen failed");
    }
    size_t written = fwrite(buf.data, 1, buf.len, fp);
    fclose(fp);
    lv_strbuf_destroy(&buf);
    return (int) written;
}
