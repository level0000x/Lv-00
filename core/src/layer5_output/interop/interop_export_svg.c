/**
 * @file interop_export_svg.c
 * @brief 导出 —— SVG 导出
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
#include "lv/interop.h"
#include "lv/lv_json.h"

#include "debug.h"
#include "interop_export_internal.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_export_common.h"
#include "lv/lv_file.h"

/* ---- 约束类型 → SVG 属性窄适配（颜色/线宽取自公共核心表 kConstraintVisuals） ---- */
typedef struct {
    const char *dasharray;  /**< dasharray 值（可为 NULL） */
    const char *class_attr; /**< class 属性值（可为 NULL） */
    const char *extra_attr; /**< 额外属性（可为 NULL） */
} ConstraintSvgSyntax;

static const ConstraintSvgSyntax constraint_svg_syntax[] = {
    [INCIDENCE]   = { NULL,       "constraint", NULL },
    [CONTAINMENT] = { "2,4",      "constraint", NULL },
    [ANGLE]       = { "4,2",      "constraint", NULL },
    [CONNECTION]  = { NULL,       NULL, "stroke-width=\"1.5\" marker-end=\"url(#arrowhead)\"" },
};

/** @brief 从公共核心表 rgb 生成 SVG 十六进制颜色串（如 "#6b7280"） */
static void constraint_rgb_to_svg_hex(const ConstraintVisual *vis, char *hex, size_t hex_size) {
    snprintf(hex, hex_size, "#%02x%02x%02x", vis->rgb[0], vis->rgb[1], vis->rgb[2]);
}

/* ---- SVG 约束渲染 ops（BETWEENNESS/INTERSECTION 特判 + default 核心表驱动，
 *       原约束渲染 switch 收敛为 kSvgConstraintOps 经 constraint_render_dispatch 分发） ---- */

static bool svg_render_betweenness(const ConstraintRenderCtx *ctx) {
    /* 之间约束：三点之间用标签标注（颜色取自公共核心表 kConstraintVisuals） */
    double mx = (ctx->x0 + ctx->x1) / 2.0;
    double my = (ctx->y0 + ctx->y1) / 2.0;
    char hex[8];
    constraint_rgb_to_svg_hex(&kConstraintVisuals[BETWEENNESS], hex, sizeof(hex));
    fprintf(ctx->fp,
            "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
            "text-anchor=\"middle\" fill=\"%s\" font-style=\"italic\">"
            "B(%d,%d",
            mx, my, hex, ctx->c->participants[0], ctx->c->participants[1]);
    if (ctx->c->participant_count >= 3) {
        fprintf(ctx->fp, ",%d", ctx->c->participants[2]);
    }
    fprintf(ctx->fp, ")</text>\n");
    return true;
}

static bool svg_render_intersection(const ConstraintRenderCtx *ctx) {
    /* 相交约束：计算精确交点并标记紫色十字（颜色取自公共核心表 kConstraintVisuals） */
    double ix = ctx->x0, iy = ctx->y0; /* 默认交点为第一个参与者 */
    double a1x = ctx->x0, a1y = ctx->y0;
    double b1x = ctx->x1, b1y = ctx->y1;

    /* 公共几何辅助：两线段时求精确交点，否则回退 (x0,y0)（与原内联实现语义一致） */
    constraint_intersection_point(ctx->p0, ctx->p1, ctx->x0, ctx->y0, &ix, &iy);

    char hex[8];
    constraint_rgb_to_svg_hex(&kConstraintVisuals[INTERSECTION], hex, sizeof(hex));

    fprintf(ctx->fp,
            "  <line class=\"constraint\" x1=\"%.2f\" y1=\"%.2f\" "
            "x2=\"%.2f\" y2=\"%.2f\" stroke=\"%s\"/>\n",
            a1x, a1y, b1x, b1y, hex);

    /* 在精确交点处绘制紫色十字标记 */
    double cross_r = 5.0;
    fprintf(ctx->fp,
            "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
            "stroke=\"%s\" stroke-width=\"2\"/>\n",
            ix - cross_r, iy - cross_r, ix + cross_r, iy + cross_r, hex);
    fprintf(ctx->fp,
            "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
            "stroke=\"%s\" stroke-width=\"2\"/>\n",
            ix - cross_r, iy + cross_r, ix + cross_r, iy - cross_r, hex);
    fprintf(ctx->fp,
            "  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"4\" "
            "fill=\"none\" stroke=\"%s\" stroke-width=\"1.5\"/>\n",
            ix, iy, hex);
    return true;
}

static bool svg_render_default(const ConstraintRenderCtx *ctx) {
    /* 使用公共核心表颜色 + 本语法窄适配（default 可达类型均有条目） */
    const ConstraintVisual *vis = constraint_visual_find(ctx->c->type);
    const ConstraintSvgSyntax *syn = &constraint_svg_syntax[ctx->c->type];
    char stroke[8];
    constraint_rgb_to_svg_hex(vis ? vis : &kConstraintVisuals[0], stroke, sizeof(stroke));
    fprintf(ctx->fp, "  <line");
    if (syn->class_attr)
        fprintf(ctx->fp, " class=\"%s\"", syn->class_attr);
    fprintf(ctx->fp, " x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke=\"%s\"",
            ctx->x0, ctx->y0, ctx->x1, ctx->y1, stroke);
    if (syn->dasharray)
        fprintf(ctx->fp, " stroke-dasharray=\"%s\"", syn->dasharray);
    if (syn->extra_attr)
        fprintf(ctx->fp, " %s", syn->extra_attr);
    fprintf(ctx->fp, "/>\n");
    return true;
}

/** @brief SVG 约束渲染 ops 实例（约束渲染循环经 constraint_render_dispatch 分发） */
static const ConstraintRenderOps kSvgConstraintOps = {
    svg_render_betweenness,
    svg_render_intersection,
    svg_render_default,
};

/** @brief 计算两参与者线段交点（公共几何辅助；内部复用 segment_intersection） */
void constraint_intersection_point(const GeomNode *p0, const GeomNode *p1,
                                   double dflt_x, double dflt_y,
                                   double *ix, double *iy) {
    double rx = dflt_x, ry = dflt_y;
    if (p0 && p1 && p0->type == GEOM_LINE_SEGMENT && p0->coord_count >= 4 &&
        p1->type == GEOM_LINE_SEGMENT && p1->coord_count >= 4) {
        double a1x, a1y, a2x, a2y, b1x, b1y, b2x, b2y;
        if (symbolic_coord_get_segment(p0->symbolic_coords, p0->coord_count, &a1x, &a1y, &a2x, &a2y) &&
            symbolic_coord_get_segment(p1->symbolic_coords, p1->coord_count, &b1x, &b1y, &b2x, &b2y)) {
            /* 与原内联实现一致：忽略返回值，无效交点保持默认点 */
            segment_intersection(a1x, a1y, a2x, a2y, b1x, b1y, b2x, b2y, &rx, &ry);
        }
    }
    *ix = rx;
    *iy = ry;
}

/**
 * @brief TikZ转义特殊字符
 *
 * 将字符串中的 LaTeX/TikZ 特殊字符（\、{、}、$、#、%、_、&）
 * 转义为对应的 LaTeX 命令序列，防止在 TikZ 输出中出现编译错误。
 *
 * 修复：将循环条件从 j < dst_size - 2 改为 j < dst_size - 16，
 * 确保最长转义序列（\textbackslash{} = 16字节）不会导致缓冲区溢出。
 * 对于非反斜杠字符，实际只需要 1 字节空间，但统一使用最严格的边界检查。
 *
 * @param src      源字符串
 * @param dst      输出缓冲区，用于存储转义后的字符串
 * @param dst_size 输出缓冲区大小（字节）
 */

int interop_export_svg(const ConstraintGraph *graph, const InteropExportConfig *config) {
    /**
     * @brief 将约束图导出为SVG矢量图
     *
     * 【已实现功能】
     *   本函数已将SVG导出的核心渲染管线完整实现，能够生成独立可用的SVG文件：
     *   1. 边界框计算 —— 自动遍历约束图中所有节点的符号坐标，计算包围盒
     *   2. 区域（Region）渲染 —— 在底层渲染多边形区域，带透明度填充
     *   3. 函数块（Function Block）渲染 —— 渲染为圆角矩形，居中显示名称和ID
     *   4. 线段（Line Segment）渲染 —— 渲染为带颜色的直线段，中点显示标签
     *   5. 端口（Port）渲染 —— 输入/输出端口渲染为小圆圈，标注类型和ID
     *   6. 点（Point）渲染 —— 渲染为填充圆形，标注P+ID
     *   7. 约束关系渲染 —— 支持四种约束类型的可视化：
     *      - 关联约束（INCIDENCE）：灰色虚线
     *      - 之间约束（BETWEENNESS）：紫色斜体标签标注三点关系
     *      - 相交约束（INTERSECTION）：紫色十字标记
     *      - 包含约束（CONTAINMENT）：青色点线
     *      - 连接约束（CONNECTION）：橙色箭头线
     *   8. 图例（Legend） —— 左上角半透明图例，说明各几何类型和信任颜色含义
     *   9. 信任颜色映射 —— 根据TrustColor为不同信用级别的元素使用不同颜色：
     *      绿色（受约束）、灰色（自由）、红色（冲突）
     *  10. 样式定义 —— 通过 <style> 标签统一定义 class 样式，clean SVG结构
     *
     * 【简化实现的部分（完整功能需要额外依赖或后续版本）】
     *   1. 贝塞尔曲线/圆弧段的精确渲染 —— 当前仅使用直线端点连接；
     *      完整实现需要解析曲线控制点并生成 SVG <path> 的 C/Q/A 弧命令。
     *      所需数据：从 GeomNode 的 coord_count > 4 时提取控制点坐标。
     *   2. 区域边界的曲线路径 —— 当前使用 polygon 直线顶点连接；
     *      完整实现需要使用 SVG <path> 的贝塞尔命令绘制曲线边界。
     *   3. 包含/相交约束的精确几何交点 —— 当前使用参与者节点坐标
     *      作为端点；完整实现需要调用几何求解器计算实际的交点位置。
     *   4. 交互式JavaScript增强 —— 当前为纯静态SVG图形；
     *      完整实现需要嵌入JS代码实现点击高亮、悬停提示等交互。
     *   5. 数学公式渲染 —— 当前仅输出纯文本坐标；
     *      完整实现需要嵌入 LaTeX/MathML 的 SVG foreignObject。
     *   6. 多图层分组 —— 当前所有元素在同一层级；
     *      完整实现需要使用 <g> 标签按信任级别/几何类型分组。
     *   7. CSS动画/过渡 —— 当前无动画支持；
     *      完整实现需要 CSS keyframes 或 SMIL 动画演示求解过程。
     *
     * 【外部依赖说明】
     *   本函数完全使用标准C的 fprintf 生成纯文本SVG，不依赖任何外部XML或
     *   图形库。辅助函数（compute_bounding_box、trust_color_to_svg）为本文件内部实现，
     *   XML 转义复用公共实现 lv_export_xml_escape（lv/lv_export_common.h）。
     *
     * 【使用示例】
     *   InteropExportConfig cfg;
     *   lv_strlcpy(cfg.output_path, "output.svg", sizeof(cfg.output_path));
     *   int ret = interop_export_svg(graph, &cfg);
     *
     * @param graph 约束图指针（包含所有节点和约束）
     * @param config 导出配置（主要使用 output_path 指定输出文件路径）
     * @return lv_OK 成功导出
     *         lv_ERROR_INVALID_PARAM 参数无效（graph或config为NULL）
     *         lv_ERROR_IO 文件无法创建或写入
     */
    if (!graph || !config)
        return lv_ERROR_INVALID_PARAM;

    FILE *fp = lv_file_open(config->output_path, "w");
    if (!fp)
        return lv_ERROR_IO;

    /* 计算边界框 */
    double min_x, min_y, max_x, max_y;
    compute_bounding_box(graph, &min_x, &min_y, &max_x, &max_y);

    double width = max_x - min_x;
    double height = max_y - min_y;
    if (width < 1.0)
        width = 200.0;
    if (height < 1.0)
        height = 200.0;

    /* SVG头部 */
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp,
            "<svg xmlns=\"http://www.w3.org/2000/svg\" "
            "width=\"%.1f\" height=\"%.1f\" "
            "viewBox=\"%.2f %.2f %.2f %.2f\">\n",
            width, height, min_x, min_y, width, height);
    fprintf(fp, "  <title>Lv-00 Geometry Export</title>\n");
    fprintf(fp, "  <desc>Generated by Lv-00 v%s</desc>\n", lv_VERSION_STRING);

    /* 定义样式 */
    fprintf(fp, "  <defs>\n");
    fprintf(fp, "    <style>\n");
    fprintf(fp, "      .point { stroke-width: 1.5; }\n");
    fprintf(fp, "      .line { stroke-width: 2; fill: none; }\n");
    fprintf(fp, "      .region { stroke-width: 1.5; opacity: 0.3; }\n");
    fprintf(fp, "      .constraint { stroke-width: 1; stroke-dasharray: 5,3; fill: none; }\n");
    fprintf(fp, "      .label { font-family: 'Segoe UI', Arial, sans-serif; font-size: 12px; }\n");
    fprintf(fp, "      .block { stroke-width: 2; rx: 8; ry: 8; }\n");
    fprintf(fp, "      .port { stroke-width: 1.5; }\n");
    fprintf(fp, "    </style>\n");
    fprintf(fp, "  </defs>\n\n");

    /* 背景网格（可选） */
    fprintf(fp,
            "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
            "fill=\"#fafafa\" stroke=\"#e5e7eb\" stroke-width=\"1\"/>\n",
            min_x, min_y, width, height);

    /* ---- 渲染区域（先渲染，在底层） ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_REGION)
            continue;
        if (node->data.region.segment_count < 3)
            continue;

        const char *color = trust_color_to_svg(node->trust);
        char escaped_name[256];
        lv_export_xml_escape(geom_type_name(node->type), escaped_name, sizeof(escaped_name));

        fprintf(fp, "  <!-- Region id=%d -->\n", node->id);
        fprintf(fp, "  <polygon class=\"region\" fill=\"%s\" stroke=\"%s\" points=\"", color, color);

        /* 收集区域边界顶点：遍历边界线段的端点 */
        for (int s = 0; s < node->data.region.segment_count; s++) {
            GeomNode *seg = node->data.region.boundary_segments[s];
            if (seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4) {
                /* 线段有两个端点，每个端点2个坐标(x1,y1,x2,y2) */
                double sx1 = symbolic_coord_to_double(seg->symbolic_coords[0]);
                double sy1 = symbolic_coord_to_double(seg->symbolic_coords[1]);
                fprintf(fp, "%.2f,%.2f ", sx1, sy1);
            }
        }
        fprintf(fp, "\"/>\n");
    }

    /* ---- 渲染函数块 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_FUNCTION_BLOCK)
            continue;
        if (node->coord_count < 2)
            continue;

        double bx = symbolic_coord_to_double(node->symbolic_coords[0]);
        double by = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_svg(node->trust);
        char escaped_name[256];
        lv_export_xml_escape(geom_type_name(node->type), escaped_name, sizeof(escaped_name));

        /* 函数块：圆角矩形 */
        double bw = 120.0, bh = 60.0;
        fprintf(fp, "  <!-- Function Block id=%d -->\n", node->id);
        fprintf(fp,
                "  <rect class=\"block\" x=\"%.2f\" y=\"%.2f\" "
                "width=\"%.2f\" height=\"%.2f\" "
                "fill=\"%s\" fill-opacity=\"0.15\" stroke=\"%s\"/>\n",
                bx - bw / 2.0, by - bh / 2.0, bw, bh, color, color);
        fprintf(fp,
                "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                "text-anchor=\"middle\" dominant-baseline=\"central\" "
                "fill=\"%s\">%s_%d</text>\n",
                bx, by, color, escaped_name, node->id);
    }

    /* ---- 渲染线段 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_LINE_SEGMENT)
            continue;
        if (node->coord_count < 4)
            continue;

        double x1, y1, x2, y2;
        if (!symbolic_coord_get_segment(node->symbolic_coords, node->coord_count, &x1, &y1, &x2, &y2))
            continue;

        const char *color = trust_color_to_svg(node->trust);

        /* 贝塞尔曲线渲染：如果线段有 3 个以上坐标对，使用 SVG cubic Bezier */
        if (node->coord_count >= 6) {
            /* 使用前两对为端点，中间对为控制点 */
            int total_pairs = node->coord_count / 2;
            fprintf(fp, "  <!-- Line Segment id=%d (Bezier, %d points) -->\n", node->id, total_pairs);
            fprintf(fp, "  <path class=\"line\" fill=\"none\" stroke=\"%s\" d=\"M %.2f,%.2f", color, x1, y1);

            /* 构建贝塞尔曲线链：每两个端点间使用 2 个控制点 */
            for (int p = 0; p < total_pairs - 1; p++) {
                double seg_x1 = symbolic_coord_to_double(node->symbolic_coords[p * 2]);
                double seg_y1 = symbolic_coord_to_double(node->symbolic_coords[p * 2 + 1]);
                double seg_x2 = symbolic_coord_to_double(node->symbolic_coords[(p + 1) * 2]);
                double seg_y2 = symbolic_coord_to_double(node->symbolic_coords[(p + 1) * 2 + 1]);

                /* CP1 = P0 + 0.3*(P1-P0) + 垂直偏移（公共几何函数，与 PDF 共用） */
                double cp1x, cp1y, cp2x, cp2y;
                compute_bezier_control_points(seg_x1, seg_y1, seg_x2, seg_y2, &cp1x, &cp1y, &cp2x, &cp2y);

                fprintf(fp, " C %.2f,%.2f %.2f,%.2f %.2f,%.2f", cp1x, cp1y, cp2x, cp2y, seg_x2, seg_y2);
            }
            fprintf(fp, "\"/>\n");
        } else {
            fprintf(fp, "  <!-- Line Segment id=%d -->\n", node->id);
            fprintf(fp,
                    "  <line class=\"line\" x1=\"%.2f\" y1=\"%.2f\" "
                    "x2=\"%.2f\" y2=\"%.2f\" stroke=\"%s\"/>\n",
                    x1, y1, x2, y2, color);
        }

        /* 线段标签 */
        double mx = (x1 + x2) / 2.0;
        double my = (y1 + y2) / 2.0;
        fprintf(fp,
                "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                "text-anchor=\"middle\" fill=\"%s\">seg_%d</text>\n",
                mx, my - 6.0, color, node->id);
    }

    /* ---- 渲染端口 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_PORT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_svg(node->trust);
        const char *port_type_str = (node->data.port && node->data.port->type == PORT_INPUT) ? "in" : "out";

        fprintf(fp, "  <!-- Port id=%d type=%s -->\n", node->id, port_type_str);
        fprintf(fp,
                "  <circle class=\"port\" cx=\"%.2f\" cy=\"%.2f\" r=\"5\" "
                "fill=\"white\" stroke=\"%s\"/>\n",
                px, py, color);
        fprintf(fp,
                "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                "text-anchor=\"middle\" fill=\"%s\" font-size=\"9px\">%s_%d</text>\n",
                px, py - 9.0, color, port_type_str, node->id);
    }

    /* ---- 渲染点 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_svg(node->trust);

        fprintf(fp, "  <!-- Point id=%d -->\n", node->id);

        /* 数学公式渲染：为符号坐标添加 <title> 注释（分数/根式表示） */
        if (node->symbolic_coords && node->symbolic_coords[0] && node->symbolic_coords[1]) {
            char *sx = symbolic_coord_serialize(node->symbolic_coords[0]);
            char *sy = symbolic_coord_serialize(node->symbolic_coords[1]);
            if (sx && sy) {
                /* 坐标串经 XML 实体转义后写入 <title>（SVG/XML 文本内容，防止注入） */
                lvStrBuf esc_title = {0};
                lv_strbuf_printf(&esc_title, "P%d = (", node->id);
                lv_str_escape_xml(&esc_title, sx, strlen(sx));
                lv_strbuf_printf(&esc_title, ", ");
                lv_str_escape_xml(&esc_title, sy, strlen(sy));
                lv_strbuf_printf(&esc_title, ")");
                fprintf(fp, "  <g>\n");
                fprintf(fp, "    <title>%s</title>\n", lv_strbuf_cstr(&esc_title));
                lv_strbuf_destroy(&esc_title);
                fprintf(fp, "    <desc>Symbolic: P%d at rational/quadratic coords</desc>\n", node->id);
            }
            fprintf(fp,
                    "  <circle class=\"point\" cx=\"%.2f\" cy=\"%.2f\" r=\"4\" "
                    "fill=\"%s\"/>\n",
                    px, py, color);
            fprintf(fp,
                    "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                    "text-anchor=\"middle\" fill=\"#374151\">P%d</text>\n",
                    px, py - 8.0, node->id);
            if (sx && sy) {
                fprintf(fp, "  </g>\n");
            }
            lv_free((void **) &sx);
            lv_free((void **) &sy);
        } else {
            fprintf(fp,
                    "  <circle class=\"point\" cx=\"%.2f\" cy=\"%.2f\" r=\"4\" "
                    "fill=\"%s\"/>\n",
                    px, py, color);
            fprintf(fp,
                    "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                    "text-anchor=\"middle\" fill=\"#374151\">P%d</text>\n",
                    px, py - 8.0, node->id);
        }
    }

    /* ---- 渲染约束（经公共分发表 ConstraintRenderOps 分发，替代原 switch） ---- */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || c->participant_count < 2)
            continue;

        fprintf(fp, "  <!-- Constraint id=%d type=%s -->\n", c->id, constraint_type_name(c->type));

        ConstraintRenderCtx ctx = {0};
        ctx.graph = graph;
        ctx.c = c;
        if (!constraint_render_prepare(graph, c, &ctx.p0, &ctx.p1, &ctx.x0, &ctx.y0, &ctx.x1, &ctx.y1))
            continue;
        ctx.fp = fp;
        constraint_render_dispatch(&kSvgConstraintOps, &ctx, c->type);
    }

    /* ---- 图例 ---- */
    double legend_x = min_x + 15.0;
    double legend_y = min_y + 20.0;
    fprintf(fp, "\n  <!-- Legend -->\n");
    fprintf(fp, "  <g transform=\"translate(%.2f, %.2f)\">\n", legend_x, legend_y);
    fprintf(fp,
            "    <rect x=\"0\" y=\"0\" width=\"150\" height=\"130\" "
            "fill=\"white\" fill-opacity=\"0.9\" stroke=\"#d1d5db\" rx=\"4\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"10\" y=\"18\" font-weight=\"bold\">Legend</text>\n");

    /* 点 */
    fprintf(fp, "    <circle cx=\"20\" cy=\"35\" r=\"4\" fill=\"#22c55e\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"32\" y=\"39\">Point</text>\n");

    /* 线段 */
    fprintf(fp, "    <line x1=\"12\" y1=\"52\" x2=\"28\" y2=\"52\" stroke=\"#3b82f6\" stroke-width=\"2\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"32\" y=\"56\">Line Segment</text>\n");

    /* 区域 */
    fprintf(fp,
            "    <rect x=\"12\" y=\"64\" width=\"16\" height=\"12\" fill=\"#eab308\" fill-opacity=\"0.3\" "
            "stroke=\"#eab308\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"32\" y=\"75\">Region</text>\n");

    /* 约束 */
    fprintf(fp, "    <line x1=\"12\" y1=\"90\" x2=\"28\" y2=\"90\" stroke=\"#6b7280\" stroke-dasharray=\"5,3\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"32\" y=\"94\">Constraint</text>\n");

    /* 信任颜色 */
    fprintf(fp, "    <circle cx=\"16\" cy=\"110\" r=\"4\" fill=\"#22c55e\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"24\" y=\"114\" font-size=\"9px\">Constrained</text>\n");
    fprintf(fp, "    <circle cx=\"86\" cy=\"110\" r=\"4\" fill=\"#9ca3af\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"94\" y=\"114\" font-size=\"9px\">Free</text>\n");
    fprintf(fp, "    <circle cx=\"120\" cy=\"110\" r=\"4\" fill=\"#ef4444\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"128\" y=\"114\" font-size=\"9px\">Conflict</text>\n");

    fprintf(fp, "  </g>\n");

    /* 箭头标记定义（放在最后，因为connection可能引用） */
    fprintf(fp, "\n  <defs>\n");
    fprintf(fp,
            "    <marker id=\"arrowhead\" markerWidth=\"8\" markerHeight=\"6\" "
            "refX=\"8\" refY=\"3\" orient=\"auto\">\n");
    fprintf(fp, "      <polygon points=\"0 0, 8 3, 0 6\" fill=\"#f59e0b\"/>\n");
    fprintf(fp, "    </marker>\n");
    fprintf(fp, "  </defs>\n");

    fprintf(fp, "\n</svg>\n");

    lv_file_close(fp);

    return lv_OK;
}

/**
 * @brief 将约束图导出为 LaTeX TikZ 代码文件
 * @param graph  约束图指针
 * @param config 导出配置（output_path 指定 .tex 文件路径）
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效，lv_ERROR_IO 文件错误
 */
