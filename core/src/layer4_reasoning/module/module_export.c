/**
 * @file module_export.c
 * @brief 模块可视化导出（module_export_svg / tikz / pdf）
 *
 * @details 批次 C-㊹ 补齐：module.h 声明 module_export_svg/tikz/pdf 但
 *          原实现缺失（M5，零消费者故链接未暴露）——按头注释契约实现：
 *          - svg：生成含节点（按类型着色）与约束（连接线）的 SVG 文本
 *          - tikz：生成 LaTeX TikZ 代码（节点填充 + 约束线段）
 *          - pdf：生成可编译的 LaTeX 文档（documentclass + tikzpicture）
 *          三者均为自包含纯文本输出（PDF 需用户侧 pdflatex 编译，本函数
 *          仅生成 .tex 源文件），不依赖外部工具执行。
 *
 *          节点坐标取 symbolic_coords[0..1]（无坐标节点跳过渲染）；
 *          节点类型 → 颜色映射复用 LV_GEOM_TYPE_ENTRY 的 COLOR 字段
 *          （与 graph_dot_export.c 同一单一事实来源）。
 *
 * @author Lv-00 Project
 * @version 3.5.0
 */

#include "lv/module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_file.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/symbolic_coord.h"

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* ================================================================
 * 内部辅助
 * ================================================================ */

/** @brief 几何类型 → 填充色（单一事实来源：LV_GEOM_TYPE_ENTRY 的 COLOR 列） */
#define LV_GEOM_EXPORT_COLOR_ROW(ENUM, NAME, ALIAS, SHAPE, PREFIX, COLOR) [ENUM] = COLOR,
static const char *const kModuleExportFillColor[] = {
    LV_GEOM_TYPE_ENTRY(LV_GEOM_EXPORT_COLOR_ROW)
};
#undef LV_GEOM_EXPORT_COLOR_ROW

static const char *module_export_fillcolor(GeomType type) {
    if ((unsigned) type < lv_ARRAY_SIZE(kModuleExportFillColor))
        return kModuleExportFillColor[type];
    return "#d3d3d3";
}

/** @brief 提取节点的渲染坐标（x, y）；无坐标返回 false */
static bool module_export_node_xy(const GeomNode *node, double *out_x, double *out_y) {
    if (!node || node->coord_count < 2 || !node->symbolic_coords || !node->symbolic_coords[0] ||
        !node->symbolic_coords[1])
        return false;
    *out_x = symbolic_coord_to_double(node->symbolic_coords[0]);
    *out_y = symbolic_coord_to_double(node->symbolic_coords[1]);
    return true;
}

/** @brief 获取节点 ID 对应的渲染坐标（用于约束参与者连线） */
static bool module_export_node_xy_by_id(const ConstraintGraph *g, int node_id, double *out_x, double *out_y) {
    GeomNode *node = graph_get_node(g, node_id);
    return module_export_node_xy(node, out_x, out_y);
}

/* ================================================================
 * SVG 导出
 * ================================================================ */

bool module_export_svg(const Module *mod, const char *filepath) {
    if (!mod || !filepath)
        return false;
    const ConstraintGraph *g = module_get_graph(mod);
    if (!g)
        return false;

    FILE *f = lv_file_open(filepath, "w");
    if (!f)
        return false;

    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\" "
               "viewBox=\"-50 -50 100 100\">\n");
    fprintf(f, "  <rect width=\"100%%\" height=\"100%%\" fill=\"#ffffff\"/>\n");

    /* 约束 → 连接线（参与者链式连线） */
    for (int i = 0; i < g->constraint_count; i++) {
        const Constraint *c = g->constraints[i];
        if (!c || !c->is_active)
            continue;
        for (int j = 0; j + 1 < c->participant_count; j++) {
            double x1, y1, x2, y2;
            if (module_export_node_xy_by_id((ConstraintGraph *) g, c->participants[j], &x1, &y1) &&
                module_export_node_xy_by_id((ConstraintGraph *) g, c->participants[j + 1], &x2, &y2)) {
                fprintf(f, "  <line x1=\"%.4g\" y1=\"%.4g\" x2=\"%.4g\" y2=\"%.4g\" "
                           "stroke=\"#888888\" stroke-width=\"1.5\"/>\n", x1, y1, x2, y2);
            }
        }
    }

    /* 节点 → 圆 + 类型标签 */
    for (int i = 0; i < g->node_count; i++) {
        const GeomNode *n = g->nodes[i];
        if (!n || !n->is_active)
            continue;
        double x, y;
        if (!module_export_node_xy(n, &x, &y))
            continue;
        fprintf(f, "  <circle cx=\"%.4g\" cy=\"%.4g\" r=\"6\" fill=\"%s\" stroke=\"#333333\" stroke-width=\"1\"/>\n",
                x, y, module_export_fillcolor(n->type));
        /* K35：转义到独立缓冲（原实现 module_export_xml_escape(&lbl, lbl.data)
         * 把同一 lvStrBuf 既当输入又当输出——append 覆盖正在读的内容，
         * 别名 UB + 自转义双写；现改调权威 lv_str_escape_xml（K35 收编） */
        lvStrBuf lbl, escaped;
        lv_strbuf_init(&lbl);
        lv_strbuf_init(&escaped);
        lv_strbuf_printf(&lbl, "%s #%d", lv_geom_type_name((int) n->type), n->id);
        lv_str_escape_xml(&escaped, lbl.data, lbl.len);
        fprintf(f, "  <text x=\"%.4g\" y=\"%.4g\" font-size=\"8\" text-anchor=\"middle\">%s</text>\n",
                x, y - 9, escaped.data);
        lv_strbuf_destroy(&escaped);
        lv_strbuf_destroy(&lbl);
    }

    fprintf(f, "</svg>\n");
    fclose(f);
    return true;
}

/* ================================================================
 * TikZ 导出
 * ================================================================ */

bool module_export_tikz(const Module *mod, const char *filepath) {
    if (!mod || !filepath)
        return false;
    const ConstraintGraph *g = module_get_graph(mod);
    if (!g)
        return false;

    FILE *f = lv_file_open(filepath, "w");
    if (!f)
        return false;

    fprintf(f, "%% Lv-00 module graph export (TikZ)\n");
    fprintf(f, "\\begin{tikzpicture}\n");

    /* 约束 → 线段 */
    for (int i = 0; i < g->constraint_count; i++) {
        const Constraint *c = g->constraints[i];
        if (!c || !c->is_active)
            continue;
        for (int j = 0; j + 1 < c->participant_count; j++) {
            double x1, y1, x2, y2;
            if (module_export_node_xy_by_id((ConstraintGraph *) g, c->participants[j], &x1, &y1) &&
                module_export_node_xy_by_id((ConstraintGraph *) g, c->participants[j + 1], &x2, &y2)) {
                fprintf(f, "  \\draw[gray] (%.4g,%.4g) -- (%.4g,%.4g);\n", x1, y1, x2, y2);
            }
        }
    }

    /* 节点 → 填充圆 + 标签 */
    for (int i = 0; i < g->node_count; i++) {
        const GeomNode *n = g->nodes[i];
        if (!n || !n->is_active)
            continue;
        double x, y;
        if (!module_export_node_xy(n, &x, &y))
            continue;
        fprintf(f, "  \\filldraw[fill=%s] (%.4g,%.4g) circle (2pt);\n",
                module_export_fillcolor(n->type), x, y);
        fprintf(f, "  \\node[above] at (%.4g,%.4g) {\\small %s\\#%d};\n", x, y,
                lv_geom_type_name((int) n->type), n->id);
    }

    fprintf(f, "\\end{tikzpicture}\n");
    fclose(f);
    return true;
}

/* ================================================================
 * PDF（LaTeX 源）导出
 * ================================================================ */

bool module_export_pdf(const Module *mod, const char *filepath) {
    if (!mod || !filepath)
        return false;
    const ConstraintGraph *g = module_get_graph(mod);
    if (!g)
        return false;

    FILE *f = lv_file_open(filepath, "w");
    if (!f)
        return false;

    fprintf(f, "%% Lv-00 module graph export (LaTeX/PDF source)\n");
    fprintf(f, "\\documentclass{article}\n");
    fprintf(f, "\\usepackage[margin=1in]{geometry}\n");
    fprintf(f, "\\usepackage{tikz}\n");
    fprintf(f, "\\begin{document}\n");
    fprintf(f, "\\begin{center}\n\\begin{tikzpicture}\n");

    for (int i = 0; i < g->constraint_count; i++) {
        const Constraint *c = g->constraints[i];
        if (!c || !c->is_active)
            continue;
        for (int j = 0; j + 1 < c->participant_count; j++) {
            double x1, y1, x2, y2;
            if (module_export_node_xy_by_id((ConstraintGraph *) g, c->participants[j], &x1, &y1) &&
                module_export_node_xy_by_id((ConstraintGraph *) g, c->participants[j + 1], &x2, &y2)) {
                fprintf(f, "    \\draw[gray] (%.4g,%.4g) -- (%.4g,%.4g);\n", x1, y1, x2, y2);
            }
        }
    }
    for (int i = 0; i < g->node_count; i++) {
        const GeomNode *n = g->nodes[i];
        if (!n || !n->is_active)
            continue;
        double x, y;
        if (!module_export_node_xy(n, &x, &y))
            continue;
        fprintf(f, "    \\filldraw[fill=%s] (%.4g,%.4g) circle (2pt);\n",
                module_export_fillcolor(n->type), x, y);
        fprintf(f, "    \\node[above] at (%.4g,%.4g) {\\small %s\\#%d};\n", x, y,
                lv_geom_type_name((int) n->type), n->id);
    }

    fprintf(f, "\\end{tikzpicture}\n\\end{center}\n");
    fprintf(f, "\\end{document}\n");
    fclose(f);
    return true;
}
