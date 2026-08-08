/**
 * @file interop_export_pdf.c
 * @brief 约束图 PDF 导出实现（从 interop_export.c 拆分）
 *
 * @details 最小化纯 C 实现（无外部库依赖）：直接输出 PDF 1.4 文件结构，
 *          包含图形流、xref 交叉引用表与文本/图形状态管理。
 */

#include "interop_export_internal.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/interop.h"
#include "lv/lv_json.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_file.h"

/* ---- 约束类型 → PDF 图形状态窄适配（颜色/线宽取自公共核心表 kConstraintVisuals） ---- */
typedef struct {
    const char *dash_pattern; /**< dash 模式（可为 NULL） */
    bool reset_dash;          /**< 是否恢复实线 */
    bool reset_width;         /**< 是否恢复默认线宽 */
} ConstraintPdfSyntax;

static const ConstraintPdfSyntax constraint_pdf_syntax[] = {
    [INCIDENCE]    = { "[4.0 3.0] 0 d", true,  true  },
    [CONNECTION]   = { NULL,             false, false },
    [BETWEENNESS]  = { "[2.0 2.0] 0 d",  true,  true  },
    [INTERSECTION] = { NULL,             false, true  },
    [CONTAINMENT]  = { "[6.0 3.0 1.0 3.0] 0 d", true, true },
    [ANGLE]        = { "[4.0 2.0] 0 d",  true,  true  },
};

/** @brief 从公共核心表 rgb 生成 PDF 颜色三元组（如 "0.42 0.45 0.50"） */
static void constraint_rgb_to_pdf_rg(const ConstraintVisual *vis, char *rg, size_t rg_size) {
    snprintf(rg, rg_size, "%.2f %.2f %.2f",
             vis->rgb[0] / 255.0, vis->rgb[1] / 255.0, vis->rgb[2] / 255.0);
}

/* ---- PDF 约束渲染 ops（原 6 case 合并分支收敛为单渲染函数；
 *       经 constraint_render_dispatch 分发，PDF 坐标变换用 ctx.pdf_margin/pdf_ox/pdf_oy） ---- */

static bool pdf_render_constraint(const ConstraintRenderCtx *ctx) {
    /* 原语义：6 种合法类型全部走同一合并分支；非法类型走 default（不渲染） */
    if ((unsigned) ctx->c->type >= (unsigned) (sizeof(constraint_pdf_syntax) / sizeof(constraint_pdf_syntax[0])))
        return false;
    /* 公共核心表（颜色/线宽）+ 本语法窄适配（dash/恢复标志） */
    const ConstraintVisual *vis = constraint_visual_find(ctx->c->type);
    const ConstraintPdfSyntax *syn = &constraint_pdf_syntax[ctx->c->type];
    char rg[16];
    constraint_rgb_to_pdf_rg(vis ? vis : &kConstraintVisuals[0], rg, sizeof(rg));
    lv_strbuf_printf(ctx->sb, "%s RG\n", rg);
    if (syn->dash_pattern)
        lv_strbuf_printf(ctx->sb, "%s\n", syn->dash_pattern);
    lv_strbuf_printf(ctx->sb, "%.2f w\n", vis->line_width);
    lv_strbuf_printf(ctx->sb, "%.2f %.2f m\n",
                     ctx->pdf_margin + (ctx->x0 - ctx->pdf_ox),
                     ctx->pdf_margin + (ctx->y0 - ctx->pdf_oy));
    lv_strbuf_printf(ctx->sb, "%.2f %.2f l\n",
                     ctx->pdf_margin + (ctx->x1 - ctx->pdf_ox),
                     ctx->pdf_margin + (ctx->y1 - ctx->pdf_oy));
    lv_strbuf_printf(ctx->sb, "S\n");
    if (syn->reset_dash)
        lv_strbuf_printf(ctx->sb, "[] 0 d\n");
    if (syn->reset_width)
        lv_strbuf_printf(ctx->sb, "%.2f w\n", 1.5);
    return true;
}

/** @brief PDF 约束渲染 ops 实例（原 6 case 合并分支收敛为单渲染函数） */
static const ConstraintRenderOps kPdfConstraintOps = {
    pdf_render_constraint, /* BETWEENNESS（原合并分支） */
    pdf_render_constraint, /* INTERSECTION（原合并分支） */
    pdf_render_constraint, /* 其余类型（原合并分支；非法类型内部拒绝，对应原 default break） */
};

/**
 * @brief 将约束图导出为 PDF 文档（最小化纯C实现，无外部库依赖）
 * @param graph  约束图指针
 * @param config 导出配置
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效，lv_ERROR_IO 文件错误
 */
int interop_export_pdf(const ConstraintGraph *graph, const InteropExportConfig *config) {
    if (!graph || !config)
        return lv_ERROR_INVALID_PARAM;

    /* ---- 流式事件：开始 PDF 导出 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "开始 PDF 导出", 0);
    }

    FILE *fp = lv_file_open(config->output_path, "wb");
    if (!fp)
        return lv_ERROR_IO;

    /*
     * PDF构建策略：
     *   1. 首先将页面内容写入内存缓冲区（content_buffer）
     *   2. 然后依次写入：PDF头 -> 对象定义 -> 内容流 -> xref表 -> trailer
     *   3. 所有坐标从"图形空间"变换到"PDF页面空间"（原点在左下角，Y轴向上）
     *
     *   页面尺寸根据约束图的包围盒动态计算。
     */

    /* ---- 计算边界框 ---- */
    double min_x, min_y, max_x, max_y;
    compute_bounding_box(graph, &min_x, &min_y, &max_x, &max_y);

    double g_width = max_x - min_x;
    double g_height = max_y - min_y;
    if (g_width < 50.0)
        g_width = 400.0;
    if (g_height < 50.0)
        g_height = 300.0;

    /* 添加边距 */
    double margin = 40.0;
    double page_w = g_width + 2.0 * margin;
    double page_h = g_height + 2.0 * margin;

/* ---- 辅助宏：将图形坐标映射到PDF坐标（PDF原点=左下角，Y向上） ---- */
/*
     * 图形空间:      (min_x, min_y) 为左下角原点
     * PDF页面空间:   (margin, margin) 对应图形空间的 (min_x, min_y)
     *
     * 变换公式:
     *   tx = margin + (x - min_x) * scale_x
     *   ty = margin + (y - min_y) * scale_y
     *   其中 scale_x = g_width / g_width = 1.0（使用1:1映射）
     *        scale_y = g_height / g_height = 1.0
     *
     * 简化（等比例）:
     *   tx = margin + (x - min_x)
     *   ty = margin + (y - min_y)
     */
#define GX(x) (margin + ((x) - min_x))
#define GY(y) (margin + ((y) - min_y))

    /* ---- 内容流缓冲区 ---- */
    /*
     * 将所有PDF图形操作先写入缓冲区，计算总字节数后用于对象定义。
     * 使用 lvStrBuf 自动扩容，替代手写 buf_len/buf_cap + 两遍 snprintf。
     */
    lvStrBuf content = {0};

    /* 内容流辅助：追加字符串到缓冲区 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#define BUF_APPEND(fmt, ...) lv_strbuf_printf(&content, fmt, ##__VA_ARGS__)

    /* ---- 设置基础图形状态 ---- */
    BUF_APPEND("q\n");           /* 保存图形状态 */
    BUF_APPEND("%.2f w\n", 1.5); /* 默认线宽 */

    /* ---- 渲染区域（半透明填充 + 描边，底层） ---- */
    /* 激活透明度 ExtGState */
    BUF_APPEND("/GS1 gs\n");

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_REGION)
            continue;
        if (node->data.region.segment_count < 3)
            continue;

        /*
         * 区域渲染：使用 f (fill) 填充 + S (stroke) 描边。
         * 先设置填充颜色（半透明），构建路径，然后 B (fill+stroke)。
         * 颜色查公共信任颜色全字段表（GREEN 绿 / AMBER 橙（已修复） / 其余灰）。
         */
        const TrustColorEntry *tce = interop_trust_color_find(node->trust);
        const char *pdf_rg = tce ? tce->pdf_rgba : "0.61 0.64 0.69";
        BUF_APPEND("%s rg\n", pdf_rg); /* 填充色 */
        BUF_APPEND("%s RG\n", pdf_rg); /* 描边色 */

        int first = 1;
        for (int s = 0; s < node->data.region.segment_count; s++) {
            GeomNode *seg = node->data.region.boundary_segments[s];
            if (seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4) {
                double sx = symbolic_coord_to_double(seg->symbolic_coords[0]);
                double sy = symbolic_coord_to_double(seg->symbolic_coords[1]);
                if (first) {
                    BUF_APPEND("%.2f %.2f m\n", GX(sx), GY(sy));
                    first = 0;
                } else {
                    BUF_APPEND("%.2f %.2f l\n", GX(sx), GY(sy));
                }
            }
        }
        BUF_APPEND("h\n"); /* 闭合路径 */
        BUF_APPEND("B\n"); /* 填充+描边 */
    }

    /* ---- 渲染线段 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_LINE_SEGMENT)
            continue;
        if (node->coord_count < 4)
            continue;

        double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
        double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
        double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);

        /* 线段颜色：GREEN 保持历史蓝色（与 SVG 的绿色存在历史漂移，此处维持原输出），
         * 其余颜色查公共信任颜色全字段表（AMBER 已修复为橙色，其余维持灰色）。 */
        const char *pdf_rg;
        if (node->trust == TRUST_GREEN) {
            pdf_rg = "0.15 0.50 0.92"; /* 蓝色：线段（历史行为） */
        } else {
            const TrustColorEntry *tce = interop_trust_color_find(node->trust);
            pdf_rg = tce ? tce->pdf_rgba : "0.61 0.64 0.69";
        }
        BUF_APPEND("%s RG\n", pdf_rg);

        BUF_APPEND("%.2f w\n", 2.0);

        /* 贝塞尔曲线：如果线段有 3 个以上坐标对，使用 PDF c 操作符 */
        if (node->coord_count >= 6) {
            int total_pairs = node->coord_count / 2;
            BUF_APPEND("%.2f %.2f m\n", GX(x1), GY(y1));

            for (int p = 1; p < total_pairs; p++) {
                double sx = symbolic_coord_to_double(node->symbolic_coords[p * 2]);
                double sy = symbolic_coord_to_double(node->symbolic_coords[p * 2 + 1]);
                double px = symbolic_coord_to_double(node->symbolic_coords[(p - 1) * 2]);
                double py = symbolic_coord_to_double(node->symbolic_coords[(p - 1) * 2 + 1]);

                /* 计算两个控制点（公共几何函数，与 SVG 共用） */
                double cp1x, cp1y, cp2x, cp2y;
                compute_bezier_control_points(px, py, sx, sy, &cp1x, &cp1y, &cp2x, &cp2y);

                /* PDF c 操作符: x1 y1 x2 y2 x3 y3 c */
                BUF_APPEND("%.2f %.2f %.2f %.2f %.2f %.2f c\n", GX(cp1x), GY(cp1y), GX(cp2x), GY(cp2y), GX(sx), GY(sy));
            }
            BUF_APPEND("S\n");
        } else {
            BUF_APPEND("%.2f %.2f m\n", GX(x1), GY(y1));
            BUF_APPEND("%.2f %.2f l\n", GX(x2), GY(y2));
            BUF_APPEND("S\n");
        }

        BUF_APPEND("%.2f w\n", 1.5); /* 恢复默认线宽 */
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

        /* 点颜色：查公共信任颜色全字段表（GREEN 绿 / AMBER 橙（已修复） / 其余灰） */
        const TrustColorEntry *ptce = interop_trust_color_find(node->trust);
        BUF_APPEND("%s RG\n", ptce ? ptce->pdf_rgba : "0.61 0.64 0.69");

        /*
         * 点渲染：使用填充圆（filled circle）。
         * 当前方法：用极短线段模拟点（line cap round + 粗线宽）。
         * 改进方法（后续版本）: 使用 Bezier 曲线构造圆。
         */
        BUF_APPEND("%.2f w\n", 6.0);
        BUF_APPEND("1 J\n"); /* 圆头线端 */
        BUF_APPEND("%.2f %.2f m\n", GX(px), GY(py));
        BUF_APPEND("%.2f %.2f l\n", GX(px + 0.01), GY(py));
        BUF_APPEND("S\n");
        BUF_APPEND("0 J\n"); /* 恢复平头线端 */
        BUF_APPEND("%.2f w\n", 1.5);
    }

    /* ---- 渲染函数块（作为圆角矩形） ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_FUNCTION_BLOCK)
            continue;
        if (node->coord_count < 2)
            continue;

        double bx = symbolic_coord_to_double(node->symbolic_coords[0]);
        double by = symbolic_coord_to_double(node->symbolic_coords[1]);
        double bw = 120.0, bh = 60.0;

        /* 函数块颜色：查公共信任颜色全字段表（GREEN 绿 / AMBER 橙（已修复） / 其余灰） */
        const TrustColorEntry *btce = interop_trust_color_find(node->trust);
        BUF_APPEND("%s RG\n", btce ? btce->pdf_rgba : "0.61 0.64 0.69");

        BUF_APPEND("%.2f w\n", 2.0);
        BUF_APPEND("%.2f %.2f %.2f %.2f re B\n", GX(bx) - bw / 2.0, GY(by) - bh / 2.0, bw, bh);
        BUF_APPEND("%.2f w\n", 1.5);
    }

    /* ---- 渲染约束关系（经公共分发表 ConstraintRenderOps 分发，替代原 6 case 合并 switch） ---- */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || c->participant_count < 2)
            continue;

        ConstraintRenderCtx ctx = {0};
        ctx.graph = graph;
        ctx.c = c;
        if (!constraint_render_prepare(graph, c, &ctx.p0, &ctx.p1, &ctx.x0, &ctx.y0, &ctx.x1, &ctx.y1))
            continue;
        ctx.sb = &content;
        ctx.pdf_margin = margin;
        ctx.pdf_ox = min_x;
        ctx.pdf_oy = min_y;
        constraint_render_dispatch(&kPdfConstraintOps, &ctx, c->type);
    }

    /* ---- 文本标签（最小化实现） ---- */
    /*
     * 文本渲染策略说明：
     *   当前版本使用 Helvetica 字体标注节点ID。完整的文本渲染需要：
     *   1. 精确的文本宽度计算（用于居中定位）—— 可通过 Tj 返回值或 FreeType 度量
     *   2. 中文字体支持（CID字体或TrueType嵌入）—— 需要字体文件和 CIDFont 字典
     *   3. 文本旋转和变换 —— 通过 Tm 矩阵的旋转分量实现
     *   4. LaTeX 数学公式渲染 —— 复杂，需要完整的数学排版引擎或预渲染位图嵌入
     */
    BUF_APPEND("BT\n");
    BUF_APPEND("/F1 8 Tf\n"); /* Helvetica 8pt */
    BUF_APPEND("0 0 0 rg\n"); /* 黑色文本 */

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->coord_count < 2)
            continue;

        double lx = symbolic_coord_to_double(node->symbolic_coords[0]);
        double ly = symbolic_coord_to_double(node->symbolic_coords[1]);

        /* 标签放置：点/端口上方，线段中点下方，块居中 */
        lvStrBuf sb = {0};
        if (node->type == GEOM_POINT) {
            lv_strbuf_printf(&sb, "P%d", node->id);
            BUF_APPEND("%.2f %.2f Td\n", GX(lx) - 6.0, GY(ly) + 8.0);
        } else if (node->type == GEOM_LINE_SEGMENT && node->coord_count >= 4) {
            double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
            double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);
            double mx = (lx + x2) / 2.0, my = (ly + y2) / 2.0;
            lv_strbuf_printf(&sb, "S%d", node->id);
            BUF_APPEND("%.2f %.2f Td\n", GX(mx) - 6.0, GY(my) - 6.0);
        } else if (node->type == GEOM_FUNCTION_BLOCK) {
            lv_strbuf_printf(&sb, "FB_%d", node->id);
            BUF_APPEND("%.2f %.2f Td\n", GX(lx) - 14.0, GY(ly) - 3.0);
        } else {
            continue;
        }

        /* 转义括号 */
        for (const char *p = sb.data; *p; p++) {
            if (*p == '(' || *p == ')' || *p == '\\')
                BUF_APPEND("\\%c", *p);
            else
                BUF_APPEND("%c", *p);
        }
        BUF_APPEND(" Tj\n");
        BUF_APPEND("%.2f %.2f Td\n", 0.0, 0.0); /* 重置文本位置到原点 */
        lv_strbuf_destroy(&sb);
    }
    BUF_APPEND("ET\n");

    BUF_APPEND("Q\n"); /* 恢复图形状态 */

#undef BUF_APPEND
#pragma GCC diagnostic pop
#undef GX
#undef GY

    /* ================================================================ */
    /*   PDF 文件结构写入（基于PDF 1.4规范）                             */
    /*                                                                  */
    /*   PDF 对象编号方案：                                              */
    /*     对象1: Catalog（目录）                                       */
    /*     对象2: Pages（页面树根节点）                                  */
    /*     对象3: Page（单页，含 ExtGState 引用）                       */
    /*     对象4: Content（内容流，包含上述所有图形操作）                */
    /*     对象5: Font（字体字典 - Helvetica）                          */
    /*     对象6: ExtGState（透明度图形状态）                           */
    /*     对象7: Info（页面元数据）                                    */
    /*                                                                  */
    /*   注意：对象编号和字节偏移量紧密耦合，修改内容流时需同步更新     */
    /*         xref表中的偏移量。                                       */
    /* ================================================================ */

    /* ---- 对象4的内容流长度（字节数） ---- */
    long content_length = (long) content.len;

    /*
     * 对象1: Catalog
     * 根目录对象，指向Pages树
     */
    long cat_start = ftell(fp);
    fprintf(fp, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    /*
     * 对象2: Pages（页面树根节点）
     * 包含页面数量和子页面引用
     */
    long pages_start = ftell(fp);
    fprintf(fp, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");

    /*
     * 对象3: Page（单页定义）
     * 定义页面尺寸（MediaBox）、内容流引用、字体资源和 ExtGState
     * MediaBox格式：[llx lly urx ury] = [0 0 page_w page_h]
     */
    long page_start = ftell(fp);
    fprintf(fp,
            "3 0 obj\n<< /Type /Page /Parent 2 0 R\n"
            "   /MediaBox [0 0 %.2f %.2f]\n"
            "   /Contents 4 0 R\n"
            "   /Resources << /Font << /F1 5 0 R >>\n"
            "                 /ExtGState << /GS1 6 0 R >> >>\n"
            ">>\nendobj\n",
            page_w, page_h);

    /*
     * 对象4: Content（内容流）
     * 包含所有PDF图形描述操作符
     * 写入前计算并声明精确的流长度
     */
    long content_start = ftell(fp);
    fprintf(fp, "4 0 obj\n<< /Length %ld >>\nstream\n", content_length);
    size_t written = fwrite(lv_strbuf_cstr(&content), 1, content_length, fp);
    if (written != (size_t) content_length) {
        lv_LOG_WARNING("PDF内容流写入不完整（期望 %ld, 实际 %zu）", content_length, written);
    }
    fprintf(fp, "\nendstream\nendobj\n");

    /*
     * 对象5: Font（字体字典）
     * 使用PDF标准14种字体之一的Helvetica，无需嵌入字体文件
     */
    long font_start = ftell(fp);
    fprintf(fp,
            "5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica"
            " /Encoding /WinAnsiEncoding >>\nendobj\n");

    /*
     * 对象6: ExtGState（透明度图形状态）
     * 设置 CA 0.3（描边透明度）和 ca 0.3（填充透明度）
     * 用于区域渲染的半透明效果。
     */
    long gs_start = ftell(fp);
    fprintf(fp, "6 0 obj\n<< /Type /ExtGState /CA 0.3 /ca 0.3 >>\nendobj\n");

    /*
     * 对象7: Info（页面元数据）
     * 包含文档标题、作者、创建者和创建日期。
     */
    long info_start = ftell(fp);
    {
        /* 获取当前日期时间字符串 */
        time_t now = time(NULL);
        struct tm lt;
        char date_str[64] = {0};
        lv_LOCALTIME(&now, &lt);
        strftime(date_str, sizeof(date_str), "D:%Y%m%d%H%M%S", &lt);

        fprintf(fp,
                "7 0 obj\n<< /Title (Lv-00 Geometry Export)\n"
                "   /Author (Lv-00 Project)\n"
                "   /Creator (Lv-00 v%s)\n"
                "   /CreationDate (%s) >>\nendobj\n",
                lv_VERSION_STRING, date_str);
    }

    /* ---- 交叉引用表（Cross-Reference Table） ---- */
    /*
     * xref表记录了每个PDF对象的字节偏移量，是PDF随机访问的关键结构。
     * 格式：
     *   xref
     *   0 8                    (对象0到7，共8个对象)
     *   0000000000 65535 f     (对象0=空闲条目)
     *   nnnnnnnnnn 00000 n     (对象1-7的字节偏移)
     */
    long xref_start = ftell(fp);
    fprintf(fp, "xref\n");
    fprintf(fp, "0 8\n");
    fprintf(fp, "0000000000 65535 f \n"); /* 对象0：空闲条目 */
    fprintf(fp, "%010ld 00000 n \n", cat_start);
    fprintf(fp, "%010ld 00000 n \n", pages_start);
    fprintf(fp, "%010ld 00000 n \n", page_start);
    fprintf(fp, "%010ld 00000 n \n", content_start);
    fprintf(fp, "%010ld 00000 n \n", font_start);
    fprintf(fp, "%010ld 00000 n \n", gs_start);
    fprintf(fp, "%010ld 00000 n \n", info_start);

    /* ---- Trailer ---- */
    /*
     * Trailer包含：
     * - /Size: 交叉引用表条目总数（8 = 对象0-7）
     * - /Root: 指向Catalog对象（对象1）
     * - /Info: 指向元数据对象（对象7）
     * - startxref: xref表起始偏移量（用于快速定位）
     * - %%EOF: PDF文件结束标记
     */
    fprintf(fp, "trailer\n");
    fprintf(fp, "<< /Size 8 /Root 1 0 R /Info 7 0 R >>\n");
    fprintf(fp, "startxref\n");
    fprintf(fp, "%ld\n", xref_start);
    fprintf(fp, "%%%%EOF\n");

    lv_file_close(fp);
    lv_strbuf_destroy(&content);

    /*
     * PDF已成功导出：告知调用者文件路径、页面尺寸、节点/约束数量。
     * 当前PDF为纯C最小化实现（无外部库依赖），
     * 区域以线框模式渲染，文本标签为基础版本。
     * 完整功能改进方案见函数注释中【简化实现的部分】列表。
     */

    /* ---- 流式事件：PDF 导出完成 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "PDF 导出完成", 0);
    }

    return lv_OK;
}
