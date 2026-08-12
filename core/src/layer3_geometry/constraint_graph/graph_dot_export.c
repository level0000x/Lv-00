/**
 * @file graph_dot_export.c
 * @brief ConstraintGraph DOT（Graphviz）导出实现
 *
 * @details 实现 constraint_graph.h 中声明的 DOT 导出 API 组：
 *          - dot_export_config_default: 默认导出配置
 *          - graph_export_dot:          约束图 → DOT 文本
 *          - graph_export_dot_file:     约束图 → DOT 文件
 *          - graph_export_dot_to_svg:   约束图 → DOT → SVG（需系统 graphviz）
 *
 *          节点渲染为矩形（按类型取形状），标注类型颜色（点=蓝、线段=绿、
 *          区域=橙、端口=灰、函数块=紫）和坐标/维度信息；
 *          约束渲染为有向边，标注约束类型。支持按命名空间分簇、信任颜色、
 *          HTML-like 标签等完整配置项。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_dot_writer.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/symbolic_coord.h"

#include "lv_internal.h"
#include "lv_utils.h"
#include "../../layer4_reasoning/proof/trust_color_x.h"

/* ================================================================
 * 内部辅助
 * ================================================================ */

/** @brief 将字符串经 JSON/DOT 转义后追加到 lvStrBuf（与 lv_dot_writer 同法） */
static void dot_escape_append(lvStrBuf *sb, const char *s) {
    if (!sb || !s)
        return;
    char *esc = lv_str_json_escape_alloc(s, strlen(s), NULL);
    if (esc) {
        lv_strbuf_append_str(sb, esc);
        lv_free((void **) &esc);
    }
}

/** @brief 几何类型 → DOT 填充色（自 LV_GEOM_TYPE_ENTRY 生成，单一事实来源） */
#define LV_GEOM_FILL_ROW(ENUM, NAME, ALIAS, SHAPE, PREFIX, COLOR) [ENUM] = COLOR,
static const char *const kGeomFillColorMap[] = {
    LV_GEOM_TYPE_ENTRY(LV_GEOM_FILL_ROW)
};
#undef LV_GEOM_FILL_ROW

static const char *geom_type_fillcolor(GeomType type) {
    if ((unsigned) type < lv_ARRAY_SIZE(kGeomFillColorMap))
        return kGeomFillColorMap[type];
    return "#d3d3d3";
}

/** @brief 信任颜色 → DOT 填充色（show_trust_colors 时覆盖类型色） */
#define LV_TRUST_COLOR_TO_FILL(sym, disp, ser, dot, tex) [sym] = dot,
static const char *const kTrustFillColorMap[] = {
    LV_TRUST_COLOR_X(LV_TRUST_COLOR_TO_FILL)
};
#undef LV_TRUST_COLOR_TO_FILL

static const char *trust_fillcolor(TrustColor trust) {
    if ((unsigned) trust < lv_ARRAY_SIZE(kTrustFillColorMap))
        return kTrustFillColorMap[trust];
    return "#d3d3d3";
}

/** @brief 信任颜色 → 显示名（追加到节点 label） */
#define LV_TRUST_COLOR_TO_NAME(sym, disp, ser, dot, tex) {ser, sym},
static const lvStrToEnumEntry s_trust_color_name_entries[] = {
    LV_TRUST_COLOR_X(LV_TRUST_COLOR_TO_NAME)
};
#undef LV_TRUST_COLOR_TO_NAME

static const char *trust_color_name(TrustColor trust) {
    return lv_enum_to_str(s_trust_color_name_entries, lv_ARRAY_SIZE(s_trust_color_name_entries), (int) trust, "UNKNOWN");
}

/** @brief 布局引擎 → rankdir（层级布局适合 TB，其余 LR） */
static const char *layout_rankdir(DOTLayoutEngine layout) {
    return layout == DOT_LAYOUT_HIERARCHY ? "TB" : "LR";
}

/** @brief 布局引擎 → graphviz 命令行工具名（graph_export_dot_to_svg 使用） */
static const char *layout_tool(DOTLayoutEngine layout) {
    switch (layout) {
    case DOT_LAYOUT_HIERARCHY:  return "dot";
    case DOT_LAYOUT_SPRING:     return "neato";
    case DOT_LAYOUT_FORCE:      return "fdp";
    case DOT_LAYOUT_SCALABLE:   return "sfdp";
    case DOT_LAYOUT_CIRCULAR:   return "circo";
    case DOT_LAYOUT_RADIAL:     return "twopi";
    default:                    return "dot";
    }
}

/**
 * @brief 输出一个节点语句（含 label / 形状 / 填充色，完整支持各配置项）
 *
 * label 内容由类型名 + 可选 #id / 坐标 / 维度 / 信任颜色 / 命名空间深度组成。
 * html_labels=true 时输出 HTML-like label（<TABLE>），否则输出转义文本 label。
 */
static void dot_emit_node(lvStrBuf *sb, const GeomNode *node, const DOTExportConfig *cfg) {
    char idbuf[32];
    snprintf(idbuf, sizeof(idbuf), "node%d", node->id);

    const char *shape = "box";
    if (node->type >= GEOM_POINT && node->type <= GEOM_FUNCTION_BLOCK)
        shape = lv_geom_type_dot_shape((int) node->type);

    const char *fill = cfg->show_trust_colors ? trust_fillcolor(node->trust)
                                              : geom_type_fillcolor(node->type);

    lvStrBuf lbl;
    lv_strbuf_init(&lbl);
    if (cfg->html_labels) {
        lv_strbuf_printf(&lbl, "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"3\"><TR><TD>");
    }

    lv_strbuf_append_str(&lbl, lv_geom_type_name((int) node->type));
    if (cfg->show_node_ids)
        lv_strbuf_printf(&lbl, " #%d", node->id);

    const char *sep = cfg->html_labels ? "<BR/>" : "\n";

    if (cfg->show_coords && node->coord_count > 0) {
        lv_strbuf_append_str(&lbl, sep);
        lv_strbuf_append_raw(&lbl, "(", 1);
        for (int i = 0; i < node->coord_count; i++) {
            if (i > 0)
                lv_strbuf_append_str(&lbl, ", ");
            double v = node->symbolic_coords[i] ? symbolic_coord_to_double(node->symbolic_coords[i]) : 0.0;
            lv_strbuf_printf(&lbl, "%.4g", v);
        }
        lv_strbuf_append_raw(&lbl, ")", 1);
    }

    if (cfg->show_dimensions) {
        lv_strbuf_append_str(&lbl, sep);
        lv_strbuf_printf(&lbl, "dim=%d", node->coord_count);
    }

    if (cfg->show_trust_colors) {
        lv_strbuf_append_str(&lbl, sep);
        lv_strbuf_append_str(&lbl, trust_color_name(node->trust));
    }

    if (cfg->show_namespace_depth) {
        lv_strbuf_append_str(&lbl, sep);
        lv_strbuf_printf(&lbl, "ns=%d", node->namespace_depth);
    }

    if (cfg->html_labels) {
        lv_strbuf_append_str(&lbl, "</TD></TR></TABLE>");
        lv_strbuf_printf(sb, "    %s [label=<%s>, shape=%s, style=filled, fillcolor=\"%s\"];\n",
                         idbuf, lv_strbuf_cstr(&lbl), shape, fill);
    } else {
        lv_strbuf_printf(sb, "    %s [label=\"", idbuf);
        dot_escape_append(sb, lv_strbuf_cstr(&lbl));
        lv_strbuf_printf(sb, "\", shape=%s, style=filled, fillcolor=\"%s\"];\n", shape, fill);
    }

    lv_strbuf_destroy(&lbl);
}

/* ================================================================
 * 公共 API
 * ================================================================ */

DOTExportConfig dot_export_config_default(void) {
    DOTExportConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.layout = DOT_LAYOUT_SPRING;
    cfg.show_node_ids = true;
    cfg.show_coords = true;
    cfg.show_constraint_labels = true;
    return cfg;
}

char *graph_export_dot(const ConstraintGraph *graph, const DOTExportConfig *config) {
    if (!graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_export_dot: graph is NULL");

    DOTExportConfig cfg = config ? *config : dot_export_config_default();

    lvStrBuf sb;
    lv_strbuf_init(&sb);

    lv_dot_begin(&sb, "ConstraintGraph", layout_rankdir(cfg.layout), NULL, NULL);

    /* 图级属性：标题 / 字体 / 理想边长 */
    if (cfg.graph_label && cfg.graph_label[0]) {
        lv_strbuf_append_str(&sb, "    label=\"");
        dot_escape_append(&sb, cfg.graph_label);
        lv_strbuf_append_str(&sb, "\";\n");
    }
    if (cfg.font_name && cfg.font_name[0]) {
        lv_strbuf_append_str(&sb, "    fontname=\"");
        dot_escape_append(&sb, cfg.font_name);
        lv_strbuf_append_str(&sb, "\";\n");
    }
    if (cfg.font_size > 0)
        lv_strbuf_printf(&sb, "    fontsize=%d;\n", cfg.font_size);
    if (cfg.edge_len > 0.0 && (cfg.layout == DOT_LAYOUT_SPRING || cfg.layout == DOT_LAYOUT_FORCE))
        lv_strbuf_printf(&sb, "    edge [len=%.4g];\n", cfg.edge_len);

    lv_strbuf_append_str(&sb, "\n");

    /* 节点：支持按命名空间分簇（subgraph cluster_N） */
    if (cfg.cluster_by_namespace) {
        int depths[256];
        int nd = 0;
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *node = graph->nodes[i];
            if (!node || !node->is_active)
                continue;
            int d = node->namespace_depth;
            if (nd < (int) lv_ARRAY_SIZE(depths))
                lv_int_append_unique(depths, &nd, d);
        }
        for (int k = 0; k < nd; k++) {
            lv_strbuf_printf(&sb, "    subgraph cluster_ns%d {\n", depths[k]);
            lv_strbuf_printf(&sb, "        label=\"namespace depth %d\";\n", depths[k]);
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *node = graph->nodes[i];
                if (!node || !node->is_active || node->namespace_depth != depths[k])
                    continue;
                dot_emit_node(&sb, node, &cfg);
            }
            lv_strbuf_append_str(&sb, "    }\n\n");
        }
    } else {
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *node = graph->nodes[i];
            if (!node || !node->is_active)
                continue;
            dot_emit_node(&sb, node, &cfg);
        }
    }

    /* 约束 → 有向边：participants 链式 p0→p1→p2... */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *con = graph->constraints[i];
        if (!con || !con->is_active)
            continue;
        const char *label = cfg.show_constraint_labels ? lv_constraint_type_name(con->type) : NULL;
        for (int j = 0; j + 1 < con->participant_count; j++) {
            char frombuf[32], tobuf[32];
            snprintf(frombuf, sizeof(frombuf), "node%d", con->participants[j]);
            snprintf(tobuf, sizeof(tobuf), "node%d", con->participants[j + 1]);
            lv_dot_edge(&sb, frombuf, tobuf, label, NULL);
        }
    }

    lv_dot_end(&sb);
    return lv_strbuf_to_string(&sb);
}

int graph_export_dot_file(const ConstraintGraph *graph, const DOTExportConfig *config,
                          const char *filepath) {
    if (!graph || !filepath)
        return lv_ERROR_INVALID_PARAM;

    char *dot = graph_export_dot(graph, config);
    if (!dot)
        return lv_ERROR_INTERNAL;

    bool ok = lv_dot_write_file(filepath, dot, strlen(dot));
    lv_free((void **) &dot);
    return ok ? lv_OK : lv_ERROR_IO;
}

int graph_export_dot_to_svg(const ConstraintGraph *graph, const DOTExportConfig *config,
                            const char *output_svg) {
    if (!graph || !output_svg)
        return lv_ERROR_INVALID_PARAM;

    DOTExportConfig cfg = config ? *config : dot_export_config_default();

    char *dot = graph_export_dot(graph, config);
    if (!dot)
        return lv_ERROR_INTERNAL;

    /* 临时 DOT 文件：与输出 SVG 同目录（便于清理） */
    char tmp_dot[1024];
    snprintf(tmp_dot, sizeof(tmp_dot), "%s.tmp.dot", output_svg);

    if (!lv_dot_write_file(tmp_dot, dot, strlen(dot))) {
        lv_free((void **) &dot);
        return lv_ERROR_IO;
    }
    lv_free((void **) &dot);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "\"%s\" -Tsvg \"%s\" -o \"%s\"", layout_tool(cfg.layout), tmp_dot, output_svg);
    int rc = system(cmd);
    remove(tmp_dot);
    return rc == 0 ? lv_OK : lv_ERROR_IO;
}
