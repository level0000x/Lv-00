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
#include "lv/geo_utils.h"
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

/** @brief TikZ 节点渲染函数类型（graph 用于按 ID 解析圆/函数块的引用节点） */
typedef void (*TikzNodeRenderFunc)(const ConstraintGraph *graph, const GeomNode *node, lvStrBuf *out);

/** @brief 取节点前两个符号坐标到 (x, y)；坐标不可用返回 -1 */
static int tikz_node_xy(const GeomNode *node, double *x, double *y) {
    if (!node || node->coord_count < 2 || !node->symbolic_coords || !node->symbolic_coords[0] ||
        !node->symbolic_coords[1])
        return -1;
    *x = symbolic_coord_to_double(node->symbolic_coords[0]);
    *y = symbolic_coord_to_double(node->symbolic_coords[1]);
    return 0;
}

/**
 * @brief 渲染 GEOM_POINT 节点为 \\fill 圆点命令
 *
 * 仅当节点具有两个有效符号坐标时输出，否则跳过该节点。
 */
static void tikz_render_point(const ConstraintGraph *graph, const GeomNode *node, lvStrBuf *out) {
    lv_UNUSED(graph);
    double x, y;
    if (tikz_node_xy(node, &x, &y) == 0)
        lv_strbuf_printf(out, "  \\fill (%.4f, %.4f) circle (2pt);\n", x, y);
}

/**
 * @brief 渲染 GEOM_LINE_SEGMENT 节点为 \\draw 线段命令
 *
 * 仅当节点具有四个有效符号坐标（两个端点）时输出，否则跳过该节点。
 */
static void tikz_render_line_segment(const ConstraintGraph *graph, const GeomNode *node, lvStrBuf *out) {
    lv_UNUSED(graph);
    double x1, y1, x2, y2;
    if (symbolic_coord_get_segment(node->symbolic_coords, node->coord_count, &x1, &y1, &x2, &y2)) {
        lv_strbuf_printf(out, "  \\draw (%.4f, %.4f) -- (%.4f, %.4f);\n", x1, y1, x2, y2);
    }
}

/**
 * @brief 渲染 GEOM_CIRCLE 节点为 \\draw circle 命令
 *
 * 优先使用圆节点自身的 (cx, cy, r) 三个符号坐标；
 * 否则经图按 center_node_id / radius_node_id 解析圆心与半径端点，
 * 半径取圆心到半径端点的欧氏距离。
 */
static void tikz_render_circle(const ConstraintGraph *graph, const GeomNode *node, lvStrBuf *out) {
    double cx = 0.0, cy = 0.0, r = 0.0;
    if (node->coord_count >= 3 && node->symbolic_coords && node->symbolic_coords[0] &&
        node->symbolic_coords[1] && node->symbolic_coords[2]) {
        cx = symbolic_coord_to_double(node->symbolic_coords[0]);
        cy = symbolic_coord_to_double(node->symbolic_coords[1]);
        r = symbolic_coord_to_double(node->symbolic_coords[2]);
    } else {
        GeomNode *center = graph ? graph_get_node(graph, node->data.circle.center_node_id) : NULL;
        GeomNode *radius_pt = graph ? graph_get_node(graph, node->data.circle.radius_node_id) : NULL;
        if (tikz_node_xy(center, &cx, &cy) != 0)
            return;
        double rx, ry;
        if (tikz_node_xy(radius_pt, &rx, &ry) != 0)
            return;
        double dx = rx - cx;
        double dy = ry - cy;
        r = geo_norm_2d(dx, dy);
    }
    if (r <= 0.0)
        return;
    lv_strbuf_printf(out, "  \\draw (%.4f, %.4f) circle (%.4f);\n", cx, cy, r);
}

/**
 * @brief 渲染 GEOM_REGION 节点为 \\draw[fill] ... -- cycle; 多边形命令
 *
 * 依次取每条边界线段的首端点组成多边形顶点（与 SVG 导出一致），
 * 顶点数不足 3 时放弃导出。
 */
static void tikz_render_region(const ConstraintGraph *graph, const GeomNode *node, lvStrBuf *out) {
    lv_UNUSED(graph);
    if (node->data.region.segment_count < 3)
        return;
    lvStrBuf tmp;
    lv_strbuf_init(&tmp);
    int emitted = 0;
    for (int s = 0; s < node->data.region.segment_count; s++) {
        GeomNode *seg = node->data.region.boundary_segments[s];
        if (!seg || seg->type != GEOM_LINE_SEGMENT || seg->coord_count < 4 || !seg->symbolic_coords ||
            !seg->symbolic_coords[0] || !seg->symbolic_coords[1])
            continue;
        double sx = symbolic_coord_to_double(seg->symbolic_coords[0]);
        double sy = symbolic_coord_to_double(seg->symbolic_coords[1]);
        if (emitted > 0)
            lv_strbuf_printf(&tmp, " -- ");
        lv_strbuf_printf(&tmp, "(%.4f, %.4f)", sx, sy);
        emitted++;
    }
    if (emitted >= 3)
        lv_strbuf_printf(out, "  \\draw[fill=gray!20] %s -- cycle;\n", lv_strbuf_cstr(&tmp));
    lv_strbuf_destroy(&tmp);
}

/**
 * @brief 渲染 GEOM_FUNCTION_BLOCK 节点为 \\draw rectangle + 端口标记
 *
 * 以节点符号坐标为矩形中心绘制 120x60 矩形，
 * 并在各输入/输出端口节点的坐标处绘制实心圆作为端口标记。
 */
static void tikz_render_function_block(const ConstraintGraph *graph, const GeomNode *node, lvStrBuf *out) {
    double bx, by;
    if (tikz_node_xy(node, &bx, &by) != 0)
        return;
    double bw = 120.0, bh = 60.0;
    lv_strbuf_printf(out, "  \\draw (%.4f, %.4f) rectangle (%.4f, %.4f);\n", bx - bw / 2.0, by - bh / 2.0,
                     bx + bw / 2.0, by + bh / 2.0);
    for (int i = 0; i < node->data.func_block.input_count; i++) {
        GeomNode *port = graph ? graph_get_node(graph, node->data.func_block.input_port_ids[i]) : NULL;
        double px, py;
        if (tikz_node_xy(port, &px, &py) != 0)
            continue;
        lv_strbuf_printf(out, "  \\fill (%.4f, %.4f) circle (2pt); %% input port %d\n", px, py, port->id);
    }
    for (int i = 0; i < node->data.func_block.output_count; i++) {
        GeomNode *port = graph ? graph_get_node(graph, node->data.func_block.output_port_ids[i]) : NULL;
        double px, py;
        if (tikz_node_xy(port, &px, &py) != 0)
            continue;
        lv_strbuf_printf(out, "  \\fill (%.4f, %.4f) circle (2pt); %% output port %d\n", px, py, port->id);
    }
}

/**
 * @brief TikZ 渲染函数查找表（GeomType 直接索引，未覆盖槽位自动 NULL）
 *
 * 当前支持 GEOM_POINT、GEOM_LINE_SEGMENT、GEOM_CIRCLE、
 * GEOM_REGION 与 GEOM_FUNCTION_BLOCK（GEOM_PORT 槽位为 NULL，不单独导出）。
 * 新增节点类型时在此追加映射项即可，无需修改共享核心。
 */
static const TikzNodeRenderFunc s_tikz_renderers[] = {
    [GEOM_POINT] = tikz_render_point,
    [GEOM_LINE_SEGMENT] = tikz_render_line_segment,
    [GEOM_REGION] = tikz_render_region,
    [GEOM_CIRCLE] = tikz_render_circle,
    [GEOM_FUNCTION_BLOCK] = tikz_render_function_block,
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
    lv_CHECK_NULL(graph, -1);
    lv_CHECK_NULL(out, -1);

    /* 写入 TikZ 头部 */
    lv_strbuf_printf(out, "%s", LV_TIKZ_HEADER);

    /* 遍历所有节点，按类型导出 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active)
            continue;

        LV_DISPATCH_VOID(s_tikz_renderers, node->type, graph, node, out);
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
 *          - GEOM_CIRCLE：生成 \\draw circle 命令绘制圆
 *          - GEOM_REGION：生成 \\draw[fill] ... -- cycle; 多边形命令
 *          - GEOM_FUNCTION_BLOCK：生成 \\draw rectangle 矩形与端口标记
 *          其余节点类型（如 GEOM_PORT）暂不单独导出。
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
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "TikZ 导出内部失败");
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
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "TikZ 导出内部失败");
    }

    /* 写入文件（复用公共文件写出辅助：fopen "w" + fwrite + fclose） */
    int written = lv_export_write_file(filename, buf.data, buf.len);
    lv_strbuf_destroy(&buf);
    if (written < 0)
        lv_RETURN_ERROR(lv_ERROR_IO, "lv_tikz_export_file: fopen failed");
    return written;
}
