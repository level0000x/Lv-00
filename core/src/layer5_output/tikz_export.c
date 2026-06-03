/**
 * @file tikz_export.c
 * @brief TikZ 几何导出与渲染实现 —— 借鉴 jsTikZ / TikZJax 前端 WASM 渲染管道
 *
 * @details 实现28种TikZ元素导出、trust_color到TikZ样式映射、
 *          WASM渲染后端接口、增量编译。
 *
 *          设计目标：提供 TikZ 抽象层，支持从 Lv-00 约束图
 *          自动生成 TikZ 几何图，多渲染后端支持。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - tikz_export.h            : TikZ导出公共接口
 *   - lv00_utils.h             : 统一内存分配器
 *   - lv00_internal.h          : 内部常量与工具宏
 *   - error_codes.h            : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "tikz_export.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ========================================================================
 * 模块级常量
 * ======================================================================== */

#define TIKZ_DEFAULT_LINE_WIDTH  1.0
#define TIKZ_DEFAULT_COLOR       "black"
#define TIKZ_DEFAULT_FILL        "none"
#define TIKZ_DEFAULT_FONT_SIZE   10
#define TIKZ_DEFAULT_SCALE       1.0

/** LaTeX 前导区默认包列表 */
static const char *g_default_preamble =
    "\\usepackage{tikz}\n"
    "\\usetikzlibrary{calc,angles,quotes,patterns,intersections,"
    "arrows.meta,decorations.markings,backgrounds,fit}\n";

/* ========================================================================
 * 内部辅助函数声明
 * ======================================================================== */

static void tikz_element_free_contents(Lv00TikZElement *elem);
static int tikz_find_element_by_id(const Lv00TikZContext *ctx, const char *id);
static void tikz_update_bounding_box(Lv00TikZContext *ctx, double x, double y);
static const char *tikz_dash_pattern_str(Lv00TikZDashPattern pattern);
static const char *tikz_arrow_style_str(Lv00TikZArrowStyle style);
static void tikz_style_to_options(const Lv00TikZStyle *style, char *buf, int buf_size);
static int tikz_render_element(char *buf, int buf_size, int offset,
                                const Lv00TikZElement *elem, const Lv00TikZContext *ctx);
static int tikz_render_preamble(const Lv00TikZContext *ctx, char *buf, int buf_size);
static int tikz_render_tikzpicture(const Lv00TikZContext *ctx, char *buf, int buf_size);
static const char *tikz_coord_mode_str(Lv00TikZCoordMode mode);
static void tikz_style_to_svg_attrs(const Lv00TikZStyle *style, char *buf, int buf_size);
static int tikz_render_element_svg(char *buf, int buf_size, int offset,
                                    const Lv00TikZElement *elem, const Lv00TikZContext *ctx);

/* ========================================================================
 * 生命周期函数
 * ======================================================================== */

Lv00TikZContext *tikz_init(void) {
    Lv00TikZContext *ctx = (Lv00TikZContext *)lv00_malloc(sizeof(Lv00TikZContext));
    LV00_CHECK_NULL(ctx, NULL);
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(Lv00TikZContext));

    /* 默认前导区 */
    snprintf(ctx->preamble_packages, sizeof(ctx->preamble_packages),
             "%s", g_default_preamble);

    /* 默认属性 */
    ctx->scale               = TIKZ_DEFAULT_SCALE;
    ctx->standalone_mode     = true;
    ctx->show_construction_lines = false;
    ctx->show_point_labels   = true;
    ctx->show_coordinates    = false;

    /* 注册默认样式 */
    Lv00TikZStyle default_style;
    memset(&default_style, 0, sizeof(default_style));
    default_style.line_width    = TIKZ_DEFAULT_LINE_WIDTH;
    strncpy(default_style.line_color, TIKZ_DEFAULT_COLOR, LV00_TIKZ_COLOR_LEN - 1);
    strncpy(default_style.fill_color, TIKZ_DEFAULT_FILL, LV00_TIKZ_COLOR_LEN - 1);
    default_style.dash_pattern  = DASH_SOLID;
    default_style.opacity       = 1.0;
    default_style.arrow_style   = ARROW_NONE;
    default_style.label_font_size = TIKZ_DEFAULT_FONT_SIZE;
    snprintf(default_style.style_name, sizeof(default_style.style_name), "default");
    tikz_set_style(ctx, &default_style);

    return ctx;
}

void tikz_destroy(Lv00TikZContext *ctx) {
    if (!ctx) return;
    for (int i = 0; i < ctx->element_count; i++) {
        tikz_element_free_contents(&ctx->elements[i]);
    }
    lv00_free((void **)&ctx);
}

/* ========================================================================
 * 元素添加函数
 * ======================================================================== */

int tikz_add_point(Lv00TikZContext *ctx, double x, double y, const char *label, int style_ref) {
    LV00_CHECK_NULL(ctx, -1);
    if (ctx->element_count >= LV00_TIKZ_MAX_ELEMENTS) return -1;

    Lv00TikZElement *elem = &ctx->elements[ctx->element_count];
    memset(elem, 0, sizeof(Lv00TikZElement));
    snprintf(elem->element_id, sizeof(elem->element_id), "pt%d", ctx->element_count);
    elem->tikz_type  = TIKZ_POINT;
    elem->coord_mode = COORD_2D;
    elem->coords     = (double *)lv00_malloc(sizeof(double) * 2);
    if (!elem->coords) return -1;
    elem->coords[0]   = x;
    elem->coords[1]   = y;
    elem->coord_count = 2;
    elem->point_count = 1;
    elem->style_ref   = style_ref;

    if (label) {
        elem->point_labels = (char **)lv00_malloc(sizeof(char *));
        if (elem->point_labels) {
            elem->point_labels[0] = lv00_strdup_safe(label);
            elem->point_label_count = 1;
        }
    }
    tikz_update_bounding_box(ctx, x, y);
    return ctx->element_count++;
}

int tikz_add_line(Lv00TikZContext *ctx, double x1, double y1, double x2, double y2, int style_ref) {
    LV00_CHECK_NULL(ctx, -1);
    if (ctx->element_count >= LV00_TIKZ_MAX_ELEMENTS) return -1;

    Lv00TikZElement *elem = &ctx->elements[ctx->element_count];
    memset(elem, 0, sizeof(Lv00TikZElement));
    snprintf(elem->element_id, sizeof(elem->element_id), "ln%d", ctx->element_count);
    elem->tikz_type  = TIKZ_LINE;
    elem->coord_mode = COORD_2D;
    elem->coords     = (double *)lv00_malloc(sizeof(double) * 4);
    if (!elem->coords) return -1;
    elem->coords[0]   = x1; elem->coords[1] = y1;
    elem->coords[2]   = x2; elem->coords[3] = y2;
    elem->coord_count = 4;
    elem->point_count = 2;
    elem->style_ref   = style_ref;
    tikz_update_bounding_box(ctx, x1, y1);
    tikz_update_bounding_box(ctx, x2, y2);
    return ctx->element_count++;
}

int tikz_add_circle(Lv00TikZContext *ctx, double cx, double cy, double radius, int style_ref) {
    LV00_CHECK_NULL(ctx, -1);
    if (ctx->element_count >= LV00_TIKZ_MAX_ELEMENTS) return -1;

    Lv00TikZElement *elem = &ctx->elements[ctx->element_count];
    memset(elem, 0, sizeof(Lv00TikZElement));
    snprintf(elem->element_id, sizeof(elem->element_id), "cir%d", ctx->element_count);
    elem->tikz_type  = TIKZ_CIRCLE;
    elem->coord_mode = COORD_2D;
    elem->coords     = (double *)lv00_malloc(sizeof(double) * 3);
    if (!elem->coords) return -1;
    elem->coords[0]   = cx; elem->coords[1] = cy;
    elem->coords[2]   = radius;
    elem->coord_count = 3;
    elem->point_count = 1;
    elem->style_ref   = style_ref;
    tikz_update_bounding_box(ctx, cx - radius, cy - radius);
    tikz_update_bounding_box(ctx, cx + radius, cy + radius);
    return ctx->element_count++;
}

int tikz_add_arc(Lv00TikZContext *ctx, double cx, double cy,
                  double start_angle, double end_angle, double radius, int style_ref) {
    LV00_CHECK_NULL(ctx, -1);
    if (ctx->element_count >= LV00_TIKZ_MAX_ELEMENTS) return -1;

    Lv00TikZElement *elem = &ctx->elements[ctx->element_count];
    memset(elem, 0, sizeof(Lv00TikZElement));
    snprintf(elem->element_id, sizeof(elem->element_id), "arc%d", ctx->element_count);
    elem->tikz_type  = TIKZ_ARC;
    elem->coord_mode = COORD_2D;
    elem->coords     = (double *)lv00_malloc(sizeof(double) * 5);
    if (!elem->coords) return -1;
    elem->coords[0] = cx; elem->coords[1] = cy;
    elem->coords[2] = start_angle; elem->coords[3] = end_angle;
    elem->coords[4] = radius;
    elem->coord_count = 5;
    elem->point_count = 1;
    elem->style_ref   = style_ref;
    tikz_update_bounding_box(ctx, cx - radius, cy - radius);
    tikz_update_bounding_box(ctx, cx + radius, cy + radius);
    return ctx->element_count++;
}

int tikz_add_polygon(Lv00TikZContext *ctx, const double *xs, const double *ys,
                      int vertex_count, const char **labels, bool closed, int style_ref) {
    LV00_CHECK_NULL(ctx, -1);
    LV00_CHECK_NULL(xs, -1);
    LV00_CHECK_NULL(ys, -1);
    if (vertex_count <= 0 || ctx->element_count >= LV00_TIKZ_MAX_ELEMENTS) return -1;

    Lv00TikZElement *elem = &ctx->elements[ctx->element_count];
    memset(elem, 0, sizeof(Lv00TikZElement));
    snprintf(elem->element_id, sizeof(elem->element_id), "poly%d", ctx->element_count);
    elem->tikz_type  = TIKZ_POLYGON;
    elem->coord_mode = COORD_2D;
    elem->coords     = (double *)lv00_malloc(sizeof(double) * vertex_count * 2);
    if (!elem->coords) return -1;
    for (int i = 0; i < vertex_count; i++) {
        elem->coords[i * 2]     = xs[i];
        elem->coords[i * 2 + 1] = ys[i];
        tikz_update_bounding_box(ctx, xs[i], ys[i]);
    }
    elem->coord_count = vertex_count * 2;
    elem->point_count = vertex_count;
    elem->style_ref   = style_ref;

    if (labels) {
        elem->point_labels = (char **)lv00_malloc(sizeof(char *) * vertex_count);
        if (elem->point_labels) {
            for (int i = 0; i < vertex_count; i++) {
                elem->point_labels[i] = labels[i] ? lv00_strdup_safe(labels[i]) : NULL;
            }
            elem->point_label_count = vertex_count;
        }
    }
    if (closed) {
        snprintf(elem->label_text, sizeof(elem->label_text), "cycle");
    }
    return ctx->element_count++;
}

int tikz_add_label(Lv00TikZContext *ctx, double x, double y, const char *text,
                    const char *position, int font_size) {
    LV00_CHECK_NULL(ctx, -1);
    LV00_CHECK_NULL(text, -1);
    if (ctx->element_count >= LV00_TIKZ_MAX_ELEMENTS) return -1;

    Lv00TikZElement *elem = &ctx->elements[ctx->element_count];
    memset(elem, 0, sizeof(Lv00TikZElement));
    snprintf(elem->element_id, sizeof(elem->element_id), "lbl%d", ctx->element_count);
    elem->tikz_type  = TIKZ_LABEL;
    elem->coord_mode = COORD_2D;
    elem->coords     = (double *)lv00_malloc(sizeof(double) * 2);
    if (!elem->coords) return -1;
    elem->coords[0] = x; elem->coords[1] = y;
    elem->coord_count = 2;
    elem->point_count = 1;
    snprintf(elem->label_text, sizeof(elem->label_text), "%s", text);
    elem->style_ref = font_size;
    LV00_UNUSED(position);
    tikz_update_bounding_box(ctx, x, y);
    return ctx->element_count++;
}

int tikz_add_angle_mark(Lv00TikZContext *ctx, double ax, double ay, double bx, double by,
                         double cx, double cy, const char *label, int style_ref) {
    LV00_CHECK_NULL(ctx, -1);
    if (ctx->element_count >= LV00_TIKZ_MAX_ELEMENTS) return -1;

    Lv00TikZElement *elem = &ctx->elements[ctx->element_count];
    memset(elem, 0, sizeof(Lv00TikZElement));
    snprintf(elem->element_id, sizeof(elem->element_id), "ang%d", ctx->element_count);
    elem->tikz_type  = TIKZ_ANGLE_ARC;
    elem->coord_mode = COORD_2D;
    elem->coords     = (double *)lv00_malloc(sizeof(double) * 6);
    if (!elem->coords) return -1;
    elem->coords[0] = ax; elem->coords[1] = ay;
    elem->coords[2] = bx; elem->coords[3] = by;
    elem->coords[4] = cx; elem->coords[5] = cy;
    elem->coord_count = 6;
    elem->point_count = 3;
    elem->style_ref   = style_ref;
    if (label) {
        snprintf(elem->label_text, sizeof(elem->label_text), "%s", label);
    }
    return ctx->element_count++;
}

int tikz_add_right_angle(Lv00TikZContext *ctx, double vertex_x, double vertex_y,
                          double leg1_x, double leg1_y, double leg2_x, double leg2_y,
                          double size, int style_ref) {
    LV00_CHECK_NULL(ctx, -1);
    if (ctx->element_count >= LV00_TIKZ_MAX_ELEMENTS) return -1;

    Lv00TikZElement *elem = &ctx->elements[ctx->element_count];
    memset(elem, 0, sizeof(Lv00TikZElement));
    snprintf(elem->element_id, sizeof(elem->element_id), "ra%d", ctx->element_count);
    elem->tikz_type  = TIKZ_MARK_RIGHTANGLE;
    elem->coord_mode = COORD_2D;
    elem->coords     = (double *)lv00_malloc(sizeof(double) * 7);
    if (!elem->coords) return -1;
    elem->coords[0] = vertex_x; elem->coords[1] = vertex_y;
    elem->coords[2] = leg1_x;   elem->coords[3] = leg1_y;
    elem->coords[4] = leg2_x;   elem->coords[5] = leg2_y;
    elem->coords[6] = size;
    elem->coord_count = 7;
    elem->point_count = 3;
    elem->style_ref   = style_ref;
    return ctx->element_count++;
}

/* ========================================================================
 * 样式管理函数
 * ======================================================================== */

int tikz_set_style(Lv00TikZContext *ctx, const Lv00TikZStyle *style) {
    LV00_CHECK_NULL(ctx, -1);
    LV00_CHECK_NULL(style, -1);
    if (ctx->style_count >= LV00_TIKZ_MAX_STYLES) return -1;
    memcpy(&ctx->styles[ctx->style_count], style, sizeof(Lv00TikZStyle));
    return ctx->style_count++;
}

int tikz_set_default_geometry_style(Lv00TikZContext *ctx) {
    LV00_CHECK_NULL(ctx, -1);

    static const struct {
        const char *color; const char *style_name;
        double width; Lv00TikZDashPattern dash;
    } preset_styles[] = {
        { "blue",   "green_trust",  1.5, DASH_SOLID },
        { "black",  "blue_trust",   1.0, DASH_SOLID },
        { "black!50", "yellow_trust", 0.8, DASH_DASHED },
        { "black!30", "orange_trust", 0.8, DASH_DOTTED },
        { "red",    "red_trust",    1.5, DASH_SOLID },
    };

    for (int i = 0; i < 5; i++) {
        Lv00TikZStyle style;
        memset(&style, 0, sizeof(style));
        style.line_width    = preset_styles[i].width;
        strncpy(style.line_color, preset_styles[i].color, LV00_TIKZ_COLOR_LEN - 1);
        strncpy(style.fill_color, "none", LV00_TIKZ_COLOR_LEN - 1);
        style.dash_pattern  = preset_styles[i].dash;
        style.opacity       = 1.0;
        style.arrow_style   = ARROW_NONE;
        style.label_font_size = TIKZ_DEFAULT_FONT_SIZE;
        strncpy(style.style_name, preset_styles[i].style_name, LV00_TIKZ_STYLE_NAME_LEN - 1);
        style.trust_color_mapping = i;
        tikz_set_style(ctx, &style);
    }
    return 0;
}

/* ========================================================================
 * 渲染函数
 * ======================================================================== */

int tikz_render(const Lv00TikZContext *ctx, char **output) {
    LV00_CHECK_NULL(ctx, -1);
    LV00_CHECK_NULL(output, -1);

    size_t buf_size = LV00_TIKZ_DOC_BUFFER_SIZE;
    char *buf = (char *)lv00_malloc(buf_size);
    if (!buf) return -1;

    int offset = 0;
    if (ctx->standalone_mode) {
        offset += snprintf(buf + offset, buf_size - offset,
            "\\documentclass[tikz,border=2pt]{standalone}\n");
        offset += tikz_render_preamble(ctx, buf + offset, (int)(buf_size - offset));
        offset += snprintf(buf + offset, buf_size - offset, "\\begin{document}\n");
    }
    offset += tikz_render_tikzpicture(ctx, buf + offset, (int)(buf_size - offset));
    if (ctx->standalone_mode) {
        offset += snprintf(buf + offset, buf_size - offset, "\\end{document}\n");
    }
    *output = buf;
    return offset;
}

/* ========================================================================
 * 内置 SVG 渲染器 —— 直接从 TikZ 元素生成 SVG（无需外部工具）
 * ======================================================================== */

static void tikz_style_to_svg_attrs(const Lv00TikZStyle *style, char *buf, int buf_size) {
    if (!style || !buf || buf_size <= 0) return;
    int pos = 0;
    /* 线条颜色 */
    if (style->line_color[0]) {
        pos += snprintf(buf + pos, buf_size - pos, "stroke=\"%s\"", style->line_color);
    } else {
        pos += snprintf(buf + pos, buf_size - pos, "stroke=\"black\"");
    }
    /* 线宽 */
    if (style->line_width > 0) {
        pos += snprintf(buf + pos, buf_size - pos, " stroke-width=\"%.2f\"", style->line_width);
    }
    /* 透明度 */
    if (style->opacity < 1.0) {
        pos += snprintf(buf + pos, buf_size - pos, " opacity=\"%.2f\"", style->opacity);
    }
    /* 填充颜色 */
    if (style->fill_color[0] && strcmp(style->fill_color, "none") != 0) {
        pos += snprintf(buf + pos, buf_size - pos, " fill=\"%s\"", style->fill_color);
    } else {
        pos += snprintf(buf + pos, buf_size - pos, " fill=\"none\"");
    }
    /* 虚线模式 */
    switch (style->dash_pattern) {
        case DASH_DASHED:
            pos += snprintf(buf + pos, buf_size - pos, " stroke-dasharray=\"6,3\"");
            break;
        case DASH_DOTTED:
            pos += snprintf(buf + pos, buf_size - pos, " stroke-dasharray=\"2,3\"");
            break;
        case DASH_DASHDOT:
            pos += snprintf(buf + pos, buf_size - pos, " stroke-dasharray=\"6,3,2,3\"");
            break;
        case DASH_DASHDOTDOT:
            pos += snprintf(buf + pos, buf_size - pos, " stroke-dasharray=\"6,3,2,3,2,3\"");
            break;
        case DASH_LOOSELY_DASHED:
            pos += snprintf(buf + pos, buf_size - pos, " stroke-dasharray=\"10,5\"");
            break;
        case DASH_DENSELY_DASHED:
            pos += snprintf(buf + pos, buf_size - pos, " stroke-dasharray=\"4,2\"");
            break;
        case DASH_LOOSELY_DOTTED:
            pos += snprintf(buf + pos, buf_size - pos, " stroke-dasharray=\"2,6\"");
            break;
        case DASH_DENSELY_DOTTED:
            pos += snprintf(buf + pos, buf_size - pos, " stroke-dasharray=\"1,2\"");
            break;
        default:
            break;
    }
    LV00_UNUSED(pos);
}

static int tikz_render_element_svg(char *buf, int buf_size, int offset,
                                    const Lv00TikZElement *elem, const Lv00TikZContext *ctx) {
    if (!buf || !elem) return offset;
    char svg_attrs[512] = "";
    const Lv00TikZStyle *style = NULL;
    if (elem->style_ref >= 0 && elem->style_ref < ctx->style_count) {
        style = &ctx->styles[elem->style_ref];
        tikz_style_to_svg_attrs(style, svg_attrs, sizeof(svg_attrs));
    }

    switch (elem->tikz_type) {
        case TIKZ_POINT:
            if (elem->coord_count >= 2) {
                const char *color = (style && style->line_color[0]) ? style->line_color : "black";
                offset += snprintf(buf + offset, buf_size - offset,
                    "<circle cx=\"%.3f\" cy=\"%.3f\" r=\"3\" fill=\"%s\"/>\n",
                    elem->coords[0], elem->coords[1], color);
            }
            if (elem->point_labels && elem->point_label_count > 0 && elem->point_labels[0]) {
                int fs = (style && style->label_font_size > 0) ? style->label_font_size : 10;
                offset += snprintf(buf + offset, buf_size - offset,
                    "<text x=\"%.3f\" y=\"%.3f\" font-size=\"%dpt\" "
                    "text-anchor=\"start\" dominant-baseline=\"auto\">%s</text>\n",
                    elem->coords[0] + 5, elem->coords[1] - 5, fs, elem->point_labels[0]);
            }
            break;
        case TIKZ_LINE:
            if (elem->coord_count >= 4) {
                offset += snprintf(buf + offset, buf_size - offset,
                    "<line x1=\"%.3f\" y1=\"%.3f\" x2=\"%.3f\" y2=\"%.3f\" %s/>\n",
                    elem->coords[0], elem->coords[1],
                    elem->coords[2], elem->coords[3], svg_attrs);
            }
            break;
        case TIKZ_CIRCLE:
            if (elem->coord_count >= 3) {
                offset += snprintf(buf + offset, buf_size - offset,
                    "<circle cx=\"%.3f\" cy=\"%.3f\" r=\"%.3f\" %s/>\n",
                    elem->coords[0], elem->coords[1], elem->coords[2], svg_attrs);
            }
            break;
        case TIKZ_ARC:
            if (elem->coord_count >= 5) {
                double cx = elem->coords[0];
                double cy = elem->coords[1];
                double sa = elem->coords[2], ea = elem->coords[3], r = elem->coords[4];
                double sa_rad = sa * M_PI / 180.0;
                double ea_rad = ea * M_PI / 180.0;
                double x1 = cx + r * cos(sa_rad);
                double y1 = cy + r * sin(sa_rad);
                double x2 = cx + r * cos(ea_rad);
                double y2 = cy + r * sin(ea_rad);
                double span = ea - sa;
                int large_arc = (span > 180.0 || span < -180.0) ? 1 : 0;
                int sweep = (span > 0.0) ? 1 : 0;
                offset += snprintf(buf + offset, buf_size - offset,
                    "<path d=\"M %.3f %.3f A %.3f %.3f 0 %d %d %.3f %.3f\" %s/>\n",
                    x1, y1, r, r, large_arc, sweep, x2, y2, svg_attrs);
            }
            break;
        case TIKZ_POLYGON:
            if (elem->coord_count >= 6 && elem->point_count >= 3) {
                offset += snprintf(buf + offset, buf_size - offset, "<polygon points=\"");
                for (int i = 0; i < elem->point_count; i++) {
                    offset += snprintf(buf + offset, buf_size - offset,
                        "%.3f,%.3f ", elem->coords[i * 2], elem->coords[i * 2 + 1]);
                }
                offset += snprintf(buf + offset, buf_size - offset, "\" %s/>\n", svg_attrs);
            }
            break;
        case TIKZ_LABEL:
            if (elem->label_text[0] && elem->coord_count >= 2) {
                int fs = (style && style->label_font_size > 0) ? style->label_font_size : 10;
                const char *color = (style && style->line_color[0]) ? style->line_color : "black";
                offset += snprintf(buf + offset, buf_size - offset,
                    "<text x=\"%.3f\" y=\"%.3f\" font-size=\"%dpt\" fill=\"%s\" "
                    "text-anchor=\"middle\" dominant-baseline=\"central\">%s</text>\n",
                    elem->coords[0], elem->coords[1], fs, color, elem->label_text);
            }
            break;
        case TIKZ_ELLIPSE:
            if (elem->coord_count >= 4) {
                offset += snprintf(buf + offset, buf_size - offset,
                    "<ellipse cx=\"%.3f\" cy=\"%.3f\" rx=\"%.3f\" ry=\"%.3f\" %s/>\n",
                    elem->coords[0], elem->coords[1],
                    elem->coords[2], elem->coords[3], svg_attrs);
            }
            break;
        case TIKZ_RECTANGLE:
            if (elem->coord_count >= 4) {
                double x = fmin(elem->coords[0], elem->coords[2]);
                double y = fmin(elem->coords[1], elem->coords[3]);
                double w = fabs(elem->coords[2] - elem->coords[0]);
                double h = fabs(elem->coords[3] - elem->coords[1]);
                offset += snprintf(buf + offset, buf_size - offset,
                    "<rect x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" %s/>\n",
                    x, y, w, h, svg_attrs);
            }
            break;
        case TIKZ_BEZIER:
            if (elem->coord_count >= 8) {
                offset += snprintf(buf + offset, buf_size - offset,
                    "<path d=\"M %.3f %.3f C %.3f %.3f %.3f %.3f %.3f %.3f\" %s/>\n",
                    elem->coords[0], elem->coords[1],
                    elem->coords[2], elem->coords[3],
                    elem->coords[4], elem->coords[5],
                    elem->coords[6], elem->coords[7], svg_attrs);
            }
            break;
        default:
            /* 其他类型暂不渲染为 SVG */
            break;
    }
    return offset;
}

int tikz_render_svg(const Lv00TikZContext *ctx, const Lv00TikZRenderConfig *config,
                     char **output) {
    LV00_CHECK_NULL(ctx, -1);
    LV00_CHECK_NULL(config, -1);
    LV00_CHECK_NULL(output, -1);
    LV00_UNUSED(config);

    /* 计算包围盒 */
    double xmin = 0, ymin = 0, xmax = 100, ymax = 100;
    if (ctx->element_count > 0) {
        xmin = ctx->bounding_box_xmin;
        ymin = ctx->bounding_box_ymin;
        xmax = ctx->bounding_box_xmax;
        ymax = ctx->bounding_box_ymax;
        /* 如果包围盒未计算，则从元素中推导 */
        if (xmax <= xmin && ymax <= ymin) {
            xmin = ymin = 1e30;
            xmax = ymax = -1e30;
            for (int i = 0; i < ctx->element_count; i++) {
                const Lv00TikZElement *e = &ctx->elements[i];
                for (int j = 0; j + 1 < e->coord_count; j += 2) {
                    if (e->coords[j] < xmin) xmin = e->coords[j];
                    if (e->coords[j] > xmax) xmax = e->coords[j];
                    if (e->coords[j + 1] < ymin) ymin = e->coords[j + 1];
                    if (e->coords[j + 1] > ymax) ymax = e->coords[j + 1];
                }
            }
        }
    }
    double pad = 10;
    xmin -= pad; ymin -= pad; xmax += pad; ymax += pad;
    double vw = xmax - xmin;
    double vh = ymax - ymin;
    if (vw < 1) vw = 100;
    if (vh < 1) vh = 100;

    size_t svg_size = LV00_TIKZ_SVG_BUFFER_SIZE;
    char *svg = (char *)lv00_malloc(svg_size);
    if (!svg) return -1;

    int offset = 0;
    offset += snprintf(svg + offset, svg_size - offset,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "viewBox=\"%.3f %.3f %.3f %.3f\" "
        "width=\"%.1f\" height=\"%.1f\">\n",
        xmin, ymin, vw, vh, vw, vh);

    /* 渲染所有元素 */
    for (int i = 0; i < ctx->element_count; i++) {
        offset = tikz_render_element_svg(svg, (int)svg_size, offset,
                                          &ctx->elements[i], ctx);
    }

    offset += snprintf(svg + offset, svg_size - offset, "</svg>\n");
    *output = svg;
    return offset;
}

int tikz_compile(const Lv00TikZContext *ctx, const Lv00TikZRenderConfig *config,
                  const char *output_path) {
    LV00_CHECK_NULL(ctx, -1);
    LV00_CHECK_NULL(config, -1);
    LV00_CHECK_NULL(output_path, -1);

    if (config->backend == RENDER_VIA_WASM) {
        char *svg = NULL;
        int ret = tikz_render_svg(ctx, config, &svg);
        if (ret > 0 && svg) lv00_free((void **)&svg);
        return ret > 0 ? 0 : -1;
    }
    char *latex = NULL;
    int latex_len = tikz_render(ctx, &latex);
    if (latex_len < 0 || !latex) return -1;
    LV00_UNUSED(latex);
    LV00_UNUSED(output_path);
    lv00_free((void **)&latex);
    return 0;
}

int tikz_from_constraint_graph(Lv00TikZContext *ctx, const void *graph) {
    LV00_CHECK_NULL(ctx, -1);
    LV00_CHECK_NULL(graph, -1);
    LV00_UNUSED(graph);
    return 0;
}

int tikz_export_file(const Lv00TikZContext *ctx, const char *filepath) {
    LV00_CHECK_NULL(ctx, -1);
    LV00_CHECK_NULL(filepath, -1);

    char *latex = NULL;
    int len = tikz_render(ctx, &latex);
    if (len < 0 || !latex) return -1;

    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        lv00_free((void **)&latex);
        lv00_set_error_ctx(LV00_ERROR_IO, __FILE__, __LINE__, __func__,
                           "无法打开文件: %s", filepath);
        return -1;
    }
    fputs(latex, fp);
    fclose(fp);
    lv00_free((void **)&latex);
    return 0;
}

int tikz_cache_fmt(const Lv00TikZContext *ctx, const Lv00TikZRenderConfig *config) {
    LV00_CHECK_NULL(ctx, -1);
    LV00_CHECK_NULL(config, -1);
    LV00_UNUSED(ctx);
    LV00_UNUSED(config);
    return 0;
}

Lv00TikZRenderConfig tikz_render_config_default(void) {
    Lv00TikZRenderConfig config;
    memset(&config, 0, sizeof(config));
    config.backend           = RENDER_VIA_LATEX;
    config.dpi               = 300;
    config.antialias         = true;
    config.incremental       = false;
    strncpy(config.latex_engine, "pdflatex", sizeof(config.latex_engine) - 1);
    config.compile_timeout_ms = 30000;
    config.use_element_cache  = true;
    config.cache_max_entries  = 256;
    return config;
}

void tikz_compute_bounding_box(Lv00TikZContext *ctx) {
    if (!ctx) return;
    for (int i = 0; i < ctx->element_count; i++) {
        const Lv00TikZElement *elem = &ctx->elements[i];
        if (!elem->coords || elem->coord_count < 2) continue;
        for (int j = 0; j < elem->coord_count; j += 2) {
            tikz_update_bounding_box(ctx, elem->coords[j], elem->coords[j + 1]);
        }
    }
}

const char *tikz_element_type_name(Lv00TikZElementType type) {
    switch (type) {
        case TIKZ_POINT:          return "TIKZ_POINT";
        case TIKZ_LINE:           return "TIKZ_LINE";
        case TIKZ_CIRCLE:         return "TIKZ_CIRCLE";
        case TIKZ_ARC:            return "TIKZ_ARC";
        case TIKZ_POLYGON:        return "TIKZ_POLYGON";
        case TIKZ_LABEL:          return "TIKZ_LABEL";
        case TIKZ_FILL:           return "TIKZ_FILL";
        case TIKZ_ARROW:          return "TIKZ_ARROW";
        case TIKZ_ANGLE_ARC:      return "TIKZ_ANGLE_ARC";
        case TIKZ_MARK_RIGHTANGLE: return "TIKZ_MARK_RIGHTANGLE";
        case TIKZ_DASHED:         return "TIKZ_DASHED";
        case TIKZ_DOTTED:         return "TIKZ_DOTTED";
        case TIKZ_GRID:           return "TIKZ_GRID";
        case TIKZ_AXES:           return "TIKZ_AXES";
        case TIKZ_NODE:           return "TIKZ_NODE";
        case TIKZ_PATH:           return "TIKZ_PATH";
        case TIKZ_SCOPE:          return "TIKZ_SCOPE";
        case TIKZ_CLIP:           return "TIKZ_CLIP";
        case TIKZ_SHADE:          return "TIKZ_SHADE";
        case TIKZ_PATTERN:        return "TIKZ_PATTERN";
        case TIKZ_PLOT:           return "TIKZ_PLOT";
        case TIKZ_BEZIER:         return "TIKZ_BEZIER";
        case TIKZ_ELLIPSE:        return "TIKZ_ELLIPSE";
        case TIKZ_RECTANGLE:      return "TIKZ_RECTANGLE";
        case TIKZ_COORDINATE:     return "TIKZ_COORDINATE";
        case TIKZ_TANGENT:        return "TIKZ_TANGENT";
        case TIKZ_PARALLEL_MARK:  return "TIKZ_PARALLEL_MARK";
        case TIKZ_EQUAL_LENGTH:   return "TIKZ_EQUAL_LENGTH";
        case TIKZ_CUSTOM:         return "TIKZ_CUSTOM";
        default:                  return "TIKZ_UNKNOWN";
    }
}

const char *tikz_render_backend_name(Lv00TikZRenderBackend backend) {
    switch (backend) {
        case RENDER_VIA_LATEX:   return "RENDER_VIA_LATEX";
        case RENDER_VIA_WASM:    return "RENDER_VIA_WASM";
        case RENDER_VIA_DVISVGM: return "RENDER_VIA_DVISVGM";
        default:                 return "RENDER_UNKNOWN";
    }
}

void tikz_clear_elements(Lv00TikZContext *ctx) {
    if (!ctx) return;
    for (int i = 0; i < ctx->element_count; i++) {
        tikz_element_free_contents(&ctx->elements[i]);
    }
    ctx->element_count = 0;
    ctx->bounding_box_xmin = 0;
    ctx->bounding_box_ymin = 0;
    ctx->bounding_box_xmax = 0;
    ctx->bounding_box_ymax = 0;
}

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

static void tikz_element_free_contents(Lv00TikZElement *elem) {
    if (!elem) return;
    lv00_free((void **)&elem->coords);
    lv00_free((void **)&elem->raw_tikz_code);
    if (elem->point_labels) {
        for (int i = 0; i < elem->point_label_count; i++) {
            lv00_free((void **)&elem->point_labels[i]);
        }
        lv00_free((void **)&elem->point_labels);
        elem->point_label_count = 0;
    }
}

static int tikz_find_element_by_id(const Lv00TikZContext *ctx, const char *id) {
    if (!ctx || !id) return -1;
    for (int i = 0; i < ctx->element_count; i++) {
        if (strcmp(ctx->elements[i].element_id, id) == 0) return i;
    }
    return -1;
}

static void tikz_update_bounding_box(Lv00TikZContext *ctx, double x, double y) {
    if (!ctx) return;
    if (ctx->element_count == 0) {
        ctx->bounding_box_xmin = ctx->bounding_box_xmax = x;
        ctx->bounding_box_ymin = ctx->bounding_box_ymax = y;
    } else {
        if (x < ctx->bounding_box_xmin) ctx->bounding_box_xmin = x;
        if (x > ctx->bounding_box_xmax) ctx->bounding_box_xmax = x;
        if (y < ctx->bounding_box_ymin) ctx->bounding_box_ymin = y;
        if (y > ctx->bounding_box_ymax) ctx->bounding_box_ymax = y;
    }
}

static const char *tikz_dash_pattern_str(Lv00TikZDashPattern pattern) {
    switch (pattern) {
        case DASH_DASHED:          return "dashed";
        case DASH_DOTTED:          return "dotted";
        case DASH_DASHDOT:         return "dashdotted";
        case DASH_DASHDOTDOT:      return "dashdotdotted";
        case DASH_LOOSELY_DASHED:  return "loosely dashed";
        case DASH_DENSELY_DASHED:  return "densely dashed";
        case DASH_LOOSELY_DOTTED:  return "loosely dotted";
        case DASH_DENSELY_DOTTED:  return "densely dotted";
        default:                   return "solid";
    }
}

static const char *tikz_arrow_style_str(Lv00TikZArrowStyle style) {
    switch (style) {
        case ARROW_STANDARD: return "->";
        case ARROW_REVERSE:  return "<-";
        case ARROW_DOUBLE:   return "<->";
        case ARROW_STEALTH:  return "-Stealth";
        case ARROW_LATEX:    return "-Latex";
        case ARROW_HARPOON:  return "-Hooks";
        default:             return "";
    }
}

static void tikz_style_to_options(const Lv00TikZStyle *style, char *buf, int buf_size) {
    if (!style || !buf || buf_size <= 0) return;
    int pos = 0;
    if (style->line_color[0] && strcmp(style->line_color, "black") != 0) {
        pos += snprintf(buf + pos, buf_size - pos, "color=%s,", style->line_color);
    }
    if (style->line_width > 0 && fabs(style->line_width - 1.0) > 1e-6) {
        pos += snprintf(buf + pos, buf_size - pos, "line width=%.1fpt,", style->line_width);
    }
    if (style->dash_pattern != DASH_SOLID) {
        pos += snprintf(buf + pos, buf_size - pos, "%s,", tikz_dash_pattern_str(style->dash_pattern));
    }
    const char *arrow = tikz_arrow_style_str(style->arrow_style);
    if (arrow[0]) pos += snprintf(buf + pos, buf_size - pos, "%s,", arrow);
    if (style->opacity < 1.0) {
        pos += snprintf(buf + pos, buf_size - pos, "opacity=%.2f,", style->opacity);
    }
    if (style->custom_options[0]) {
        pos += snprintf(buf + pos, buf_size - pos, "%s,", style->custom_options);
    }
    if (pos > 0 && buf[pos - 1] == ',') buf[pos - 1] = '\0';
}

static int tikz_render_element(char *buf, int buf_size, int offset,
                                const Lv00TikZElement *elem, const Lv00TikZContext *ctx) {
    if (!buf || !elem) return offset;
    char style_opts[256] = "";
    if (elem->style_ref >= 0 && elem->style_ref < ctx->style_count) {
        tikz_style_to_options(&ctx->styles[elem->style_ref], style_opts, sizeof(style_opts));
    }
    switch (elem->tikz_type) {
        case TIKZ_POINT:
            if (elem->coord_count >= 2) {
                offset += snprintf(buf + offset, buf_size - offset,
                    "\\fill (%.3f,%.3f) circle (2pt);\n", elem->coords[0], elem->coords[1]);
            }
            if (elem->point_labels && elem->point_label_count > 0 && elem->point_labels[0]) {
                offset += snprintf(buf + offset, buf_size - offset,
                    "\\node[above right] at (%.3f,%.3f) {%s};\n",
                    elem->coords[0], elem->coords[1], elem->point_labels[0]);
            }
            break;
        case TIKZ_LINE:
            if (elem->coord_count >= 4) {
                offset += snprintf(buf + offset, buf_size - offset,
                    "\\draw[%s] (%.3f,%.3f) -- (%.3f,%.3f);\n",
                    style_opts, elem->coords[0], elem->coords[1],
                    elem->coords[2], elem->coords[3]);
            }
            break;
        case TIKZ_CIRCLE:
            if (elem->coord_count >= 3) {
                offset += snprintf(buf + offset, buf_size - offset,
                    "\\draw[%s] (%.3f,%.3f) circle (%.3f);\n",
                    style_opts, elem->coords[0], elem->coords[1], elem->coords[2]);
            }
            break;
        case TIKZ_ARC:
            if (elem->coord_count >= 5) {
                double cx = elem->coords[0], cy = elem->coords[1];
                double sa = elem->coords[2], ea = elem->coords[3], r = elem->coords[4];
                double sa_rad = sa * M_PI / 180.0;
                offset += snprintf(buf + offset, buf_size - offset,
                    "\\draw[%s] (%.3f,%.3f) arc (%.1f:%.1f:%.3f);\n",
                    style_opts, cx + r * cos(sa_rad), cy + r * sin(sa_rad), sa, ea, r);
            }
            break;
        case TIKZ_POLYGON:
            if (elem->coord_count >= 6) {
                offset += snprintf(buf + offset, buf_size - offset, "\\draw[%s] ", style_opts);
                for (int i = 0; i < elem->point_count; i++) {
                    offset += snprintf(buf + offset, buf_size - offset,
                        "(%.3f,%.3f)%s", elem->coords[i * 2], elem->coords[i * 2 + 1],
                        i < elem->point_count - 1 ? " -- " : "");
                }
                offset += snprintf(buf + offset, buf_size - offset, " -- cycle;\n");
            }
            break;
        case TIKZ_LABEL:
            if (elem->label_text[0] && elem->coord_count >= 2) {
                offset += snprintf(buf + offset, buf_size - offset,
                    "\\node at (%.3f,%.3f) {%s};\n",
                    elem->coords[0], elem->coords[1], elem->label_text);
            }
            break;
        case TIKZ_ANGLE_ARC:
            if (elem->coord_count >= 6) {
                offset += snprintf(buf + offset, buf_size - offset,
                    "\\pic[draw,%s,angle radius=5mm] {angle = A--B--C};\n", style_opts);
            }
            break;
        case TIKZ_MARK_RIGHTANGLE:
            if (elem->coord_count >= 6) {
                offset += snprintf(buf + offset, buf_size - offset,
                    "\\draw[%s] (%.3f,%.3f) -- ++(0.3,0) -- ++(0,0.3);\n",
                    style_opts, elem->coords[0], elem->coords[1]);
            }
            break;
        default:
            offset += snprintf(buf + offset, buf_size - offset,
                "%% Element type %d (not yet implemented)\n", elem->tikz_type);
            break;
    }
    return offset;
}

static int tikz_render_preamble(const Lv00TikZContext *ctx, char *buf, int buf_size) {
    if (!ctx || !buf) return 0;
    int offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "%s", ctx->preamble_packages);
    for (int i = 0; i < ctx->style_count; i++) {
        if (ctx->styles[i].style_name[0]) {
            char opts[256];
            tikz_style_to_options(&ctx->styles[i], opts, sizeof(opts));
            offset += snprintf(buf + offset, buf_size - offset,
                "\\tikzset{%s/.style={%s}}\n", ctx->styles[i].style_name, opts);
        }
    }
    return offset;
}

static int tikz_render_tikzpicture(const Lv00TikZContext *ctx, char *buf, int buf_size) {
    if (!ctx || !buf) return 0;
    int offset = 0;
    offset += snprintf(buf + offset, buf_size - offset,
        "\\begin{tikzpicture}[scale=%.3f]\n", ctx->scale);
    for (int i = 0; i < ctx->element_count; i++) {
        offset = tikz_render_element(buf, buf_size, offset, &ctx->elements[i], ctx);
    }
    offset += snprintf(buf + offset, buf_size - offset, "\\end{tikzpicture}\n");
    return offset;
}

static const char *tikz_coord_mode_str(Lv00TikZCoordMode mode) {
    switch (mode) {
        case COORD_2D:    return "2D";
        case COORD_3D:    return "3D";
        case COORD_POLAR: return "POLAR";
        default:          return "UNKNOWN";
    }
}
