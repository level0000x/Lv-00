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
#include "lv/lv_export_common.h"
#include "lv/lv_utils.h"
#include "lv/symbolic_coord.h"

#include "lv_internal.h"
#include "lv/lv_strbuf.h"

/* ── 核心导出 ── */

/** TikZ 头部文本（经 "%s" 包装避免 printf 格式串解析副作用） */
static const char LV_TIKZ_HEADER[] =
    "% Lv-00 TikZ Export\n"
    "\\begin{tikzpicture}[scale=1.0, x=1cm, y=1cm]\n";

/* ================================================================
 * 查找表：GeomType → TikZ 节点渲染函数
 * ================================================================ */

/** @brief TikZ 节点渲染函数类型 */
typedef void (*TikzNodeRenderFunc)(const GeomNode *node, lvStrBuf *out);

/**
 * @brief 渲染 GEOM_POINT 节点为 \\fill 圆点命令
 *
 * 仅当节点具有两个有效符号坐标时输出，否则跳过该节点。
 */
static void tikz_render_point(const GeomNode *node, lvStrBuf *out) {
    if (node->coord_count >= 2 && node->symbolic_coords && node->symbolic_coords[0] &&
        node->symbolic_coords[1]) {
        double x = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y = symbolic_coord_to_double(node->symbolic_coords[1]);
        lv_strbuf_printf(out, "  \\fill (%.4f, %.4f) circle (2pt);\n", x, y);
    }
}

/**
 * @brief 渲染 GEOM_LINE_SEGMENT 节点为 \\draw 线段命令
 *
 * 仅当节点具有四个有效符号坐标（两个端点）时输出，否则跳过该节点。
 */
static void tikz_render_line_segment(const GeomNode *node, lvStrBuf *out) {
    if (node->coord_count >= 4 && node->symbolic_coords && node->symbolic_coords[0] &&
        node->symbolic_coords[1] && node->symbolic_coords[2] && node->symbolic_coords[3]) {
        double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
        double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
        double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);
        lv_strbuf_printf(out, "  \\draw (%.4f, %.4f) -- (%.4f, %.4f);\n", x1, y1, x2, y2);
    }
}

/** @brief GeomType → 渲染函数映射项 */
typedef struct {
    GeomType type;          /**< 几何节点类型 */
    TikzNodeRenderFunc render; /**< 对应渲染函数 */
} TikzNodeRenderEntry;

/**
 * @brief TikZ 渲染函数查找表
 *
 * 当前仅支持 GEOM_POINT 与 GEOM_LINE_SEGMENT，其他节点类型不导出。
 * 新增节点类型时在此追加映射项即可，无需修改共享核心。
 */
static const TikzNodeRenderEntry s_tikz_renderers[] = {
    {GEOM_POINT, tikz_render_point},
    {GEOM_LINE_SEGMENT, tikz_render_line_segment},
};

/**
 * @brief 共享核心：将约束图导出为 TikZ 代码写入 lvStrBuf
 *
 * 两个公共导出函数的统一实现：
 * 1. 写入 TikZ 头部
 * 2. 遍历所有活跃节点，按 GeomType 在渲染查找表中查找并调用渲染函数
 * 3. 写入尾部
 *
 * @param graph 约束图指针（只读）
 * @param out   目标 lvStrBuf（自动扩展）
 * @return 0 成功，-1 参数非法
 */
static int tikz_export_to_buf(const ConstraintGraph *graph, lvStrBuf *out) {
    if (!graph || !out)
        return -1;

    /* 写入 TikZ 头部 */
    lv_strbuf_printf(out, "%s", LV_TIKZ_HEADER);

    /* 遍历所有节点，按类型导出 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active)
            continue;

        for (size_t r = 0; r < lv_ARRAY_SIZE(s_tikz_renderers); r++) {
            if (node->type == s_tikz_renderers[r].type) {
                s_tikz_renderers[r].render(node, out);
                break;
            }
        }
    }

    /* 写入尾部 */
    lv_strbuf_printf(out, "\\end{tikzpicture}\n");

    return 0;
}

/**
 * @brief 将约束图导出为 TikZ 代码到内存缓冲区
 *
 * @details 通过共享核心（tikz_export_to_buf）在 lvStrBuf 中构建完整输出，
 *          再拷入调用者提供的定长缓冲区。遍历约束图中的所有活跃节点，
 *          按类型生成对应的 TikZ 命令：
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

    /* 先构建完整输出，再拷入定长缓冲区 */
    lvStrBuf buf;
    lv_strbuf_init(&buf);

    if (tikz_export_to_buf((const ConstraintGraph *) graph, &buf) < 0) {
        lv_strbuf_destroy(&buf);
        return -1;
    }

    size_t full_len = buf.len;

    if (full_len + 1 > buf_size) {
        /* 缓冲区无法容纳完整输出：模拟旧版 snprintf 行为。
         * - 若连头部都无法容纳，视为失败（旧版 header snprintf 返回 -1）
         * - 否则截断拷贝，返回"本应写入的字符数"（与旧版累加语义一致） */
        if (buf_size <= sizeof(LV_TIKZ_HEADER) - 1) {
            lv_strbuf_destroy(&buf);
            lv_RETURN_ERROR(lv_ERROR_IO, "lv_tikz_export: header snprintf failed");
        }
        memcpy(out, buf.data, buf_size - 1);
        out[buf_size - 1] = '\0';
        lv_strbuf_destroy(&buf);
        return (int) full_len;
    }

    /* 完整拷贝（含终止符） */
    memcpy(out, buf.data, full_len + 1);
    lv_strbuf_destroy(&buf);
    return (int) full_len;
}

/**
 * @brief 将约束图导出为 TikZ 代码到文件
 *
 * @details 通过共享核心（tikz_export_to_buf）在内存中构建完整的 TikZ 代码，
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

    /* 先用 lvStrBuf 构建完整输出 */
    lvStrBuf buf;
    lv_strbuf_init(&buf);

    if (tikz_export_to_buf((const ConstraintGraph *) graph, &buf) < 0) {
        lv_strbuf_destroy(&buf);
        return -1;
    }

    /* 写入文件（复用公共文件写出辅助：fopen "w" + fwrite + fclose） */
    int written = lv_export_write_file(filename, buf.data, buf.len);
    lv_strbuf_destroy(&buf);
    if (written < 0)
        lv_RETURN_ERROR(lv_ERROR_IO, "lv_tikz_export_file: fopen failed");
    return written;
}
