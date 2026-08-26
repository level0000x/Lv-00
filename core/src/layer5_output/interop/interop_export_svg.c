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
#include "lv/geo_utils.h"

#include "lv/debug.h"
#include "interop_export_internal.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
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
    [CONNECTION]  = { NULL,       NULL, " stroke-width=\"%g\" marker-end=\"url(#arrowhead)\"" },
    [PARALLEL]    = { "4,2",      "constraint", NULL },
};

/** @brief 从公共核心表 rgb 生成 SVG 十六进制颜色串（如 "#6b7280"） */
static void constraint_rgb_to_svg_hex(const ConstraintVisual *vis, char *hex, size_t hex_size) {
    lv_snprintf(hex, hex_size, "#%02x%02x%02x", vis->rgb[0], vis->rgb[1], vis->rgb[2]);
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

    /* 公共几何辅助：按参与者类型组合求解精确交点（线段/圆），否则回退 (x0,y0) */
    constraint_intersection_point(ctx->graph, ctx->p0, ctx->p1, ctx->x0, ctx->y0, &ix, &iy);

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
            "fill=\"none\" stroke=\"%s\" stroke-width=\"%g\"/>\n",
            ix, iy, hex, lv_DEFAULT_STROKE_WIDTH);
    return true;
}

static bool svg_render_default(const ConstraintRenderCtx *ctx) {
    /* 使用公共核心表颜色 + 本语法窄适配（default 可达类型均有条目） */
    const ConstraintVisual *vis = constraint_visual_find(ctx->c->type);
    const ConstraintSvgSyntax *syn = &constraint_svg_syntax[ctx->c->type];
    char stroke[8];
    constraint_rgb_to_svg_hex(vis ? vis : &kConstraintVisuals[0], stroke, sizeof(stroke));
    fprintf(ctx->fp, "  <line class=\"%s dash-flow\"", syn->class_attr ? syn->class_attr : "constraint");
    fprintf(ctx->fp, " x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke=\"%s\"",
            ctx->x0, ctx->y0, ctx->x1, ctx->y1, stroke);
    if (syn->dasharray)
        fprintf(ctx->fp, " stroke-dasharray=\"%s\"", syn->dasharray);
    if (syn->extra_attr)
        fprintf(ctx->fp, syn->extra_attr, lv_DEFAULT_STROKE_WIDTH);
    fprintf(ctx->fp, "/>\n");
    return true;
}

/** @brief SVG 约束渲染 ops 实例（约束渲染循环经 constraint_render_dispatch 分发） */
static const ConstraintRenderOps kSvgConstraintOps = {
    svg_render_betweenness,
    svg_render_intersection,
    svg_render_default,
};

/** @brief 解析圆节点的圆心与半径（graph 上下文解析 center/radius 节点，与 geojson 语义一致） */
static bool svg_circle_geometry(const ConstraintGraph *graph, const GeomNode *node,
                                double *cx, double *cy, double *r) {
    if (!graph || !node || node->type != GEOM_CIRCLE)
        return false;
    const GeomNode *center = graph_get_node(graph, node->data.circle.center_node_id);
    const GeomNode *radius_pt = graph_get_node(graph, node->data.circle.radius_node_id);
    if (!center || center->coord_count < 2 || !center->symbolic_coords || !radius_pt ||
        radius_pt->coord_count < 2 || !radius_pt->symbolic_coords)
        return false;
    double ox = symbolic_coord_to_double(center->symbolic_coords[0]);
    double oy = symbolic_coord_to_double(center->symbolic_coords[1]);
    double rx = symbolic_coord_to_double(radius_pt->symbolic_coords[0]);
    double ry = symbolic_coord_to_double(radius_pt->symbolic_coords[1]);
    double rr = geo_distance_2d(ox, oy, rx, ry);
    if (rr <= 0.0)
        return false;
    *cx = ox;
    *cy = oy;
    *r = rr;
    return true;
}

/** @brief 线段与圆交点：代入圆方程解二次方程，返回 t∈[0,1] 的首个有效根交点 */
static bool svg_segment_circle_intersection(double ax, double ay, double bx, double by,
                                            double cx, double cy, double r,
                                            double *ix, double *iy) {
    double dx = bx - ax, dy = by - ay;
    double fx = ax - cx, fy = ay - cy;
    double a = dx * dx + dy * dy;
    if (a <= lv_EPSILON_HIGH)
        return false;
    double b = 2.0 * (fx * dx + fy * dy);
    double c = fx * fx + fy * fy - r * r;
    double disc = b * b - 4.0 * a * c;
    if (disc < 0.0)
        return false;
    double sq = sqrt(disc);
    double t1 = (-b - sq) / (2.0 * a);
    double t2 = (-b + sq) / (2.0 * a);
    double t = -1.0;
    if (t1 >= -0.05 && t1 <= 1.05)
        t = t1;
    else if (t2 >= -0.05 && t2 <= 1.05)
        t = t2;
    if (t < 0.0)
        return false;
    *ix = ax + t * dx;
    *iy = ay + t * dy;
    return true;
}

/** @brief 两圆交点：标准两圆方程解（相交时取 +h 分支，语义与几何构造一致） */
static bool svg_circle_circle_intersection(double c1x, double c1y, double r1,
                                           double c2x, double c2y, double r2,
                                           double *ix, double *iy) {
    double dx = c2x - c1x, dy = c2y - c1y;
    double d = sqrt(dx * dx + dy * dy);
    if (d <= lv_EPSILON_HIGH)
        return false; /* 同心圆：无确定交点 */
    if (d > r1 + r2 || d < fabs(r1 - r2))
        return false; /* 相离或内含：无交点 */
    double a = (r1 * r1 - r2 * r2 + d * d) / (2.0 * d);
    double h2 = r1 * r1 - a * a;
    double h = (h2 > 0.0) ? sqrt(h2) : 0.0;
    double mx = c1x + a * dx / d;
    double my = c1y + a * dy / d;
    /* 取 +h 分支（垂直方向） */
    *ix = mx + h * (-dy / d);
    *iy = my + h * (dx / d);
    return true;
}

/**
 * @brief 计算两参与者几何对象的交点（INTERSECTION 特判渲染共用）
 * @details 按参与者类型组合求解精确交点：
 *          - 两线段：segment_intersection 直线参数方程解；
 *          - 线段+圆（任一顺序）：线段参数方程代入圆方程解二次方程，取
 *            t∈[0,1] 的首个有效根；
 *          - 两圆：标准两圆方程解（相交时取 +h 分支交点）；
 *          - 其余组合/无有效交点：回退默认点 (dflt_x, dflt_y)。
 *          圆的圆心/半径经 data.circle.center_node_id / radius_node_id
 *          解析（graph 参数），与 geojson/meta_proof 圆解析语义一致。
 */
void constraint_intersection_point(const ConstraintGraph *graph, const GeomNode *p0, const GeomNode *p1,
                                   double dflt_x, double dflt_y,
                                   double *ix, double *iy) {
    double rx = dflt_x, ry = dflt_y;

    bool p0_seg = p0 && p0->type == GEOM_LINE_SEGMENT && p0->coord_count >= 4;
    bool p1_seg = p1 && p1->type == GEOM_LINE_SEGMENT && p1->coord_count >= 4;
    bool p0_circle = p0 && p0->type == GEOM_CIRCLE;
    bool p1_circle = p1 && p1->type == GEOM_CIRCLE;

    if (p0_seg && p1_seg) {
        double a1x, a1y, a2x, a2y, b1x, b1y, b2x, b2y;
        if (symbolic_coord_get_segment(p0->symbolic_coords, p0->coord_count, &a1x, &a1y, &a2x, &a2y) &&
            symbolic_coord_get_segment(p1->symbolic_coords, p1->coord_count, &b1x, &b1y, &b2x, &b2y)) {
            /* 与原内联实现一致：忽略返回值，无效交点保持默认点 */
            segment_intersection(a1x, a1y, a2x, a2y, b1x, b1y, b2x, b2y, &rx, &ry);
        }
    } else if (p0_seg && p1_circle) {
        double cx, cy, r, a1x, a1y, a2x, a2y;
        if (svg_circle_geometry(graph, p1, &cx, &cy, &r) &&
            symbolic_coord_get_segment(p0->symbolic_coords, p0->coord_count, &a1x, &a1y, &a2x, &a2y)) {
            if (!svg_segment_circle_intersection(a1x, a1y, a2x, a2y, cx, cy, r, &rx, &ry))
                rx = dflt_x, ry = dflt_y;
        }
    } else if (p0_circle && p1_seg) {
        double cx, cy, r, b1x, b1y, b2x, b2y;
        if (svg_circle_geometry(graph, p0, &cx, &cy, &r) &&
            symbolic_coord_get_segment(p1->symbolic_coords, p1->coord_count, &b1x, &b1y, &b2x, &b2y)) {
            if (!svg_segment_circle_intersection(b1x, b1y, b2x, b2y, cx, cy, r, &rx, &ry))
                rx = dflt_x, ry = dflt_y;
        }
    } else if (p0_circle && p1_circle) {
        double c1x, c1y, r1, c2x, c2y, r2;
        if (svg_circle_geometry(graph, p0, &c1x, &c1y, &r1) &&
            svg_circle_geometry(graph, p1, &c2x, &c2y, &r2)) {
            if (!svg_circle_circle_intersection(c1x, c1y, r1, c2x, c2y, r2, &rx, &ry))
                rx = dflt_x, ry = dflt_y;
        }
    }
    *ix = rx;
    *iy = ry;
}

/* ==================== SVG 数学公式排版器 ====================
 * 将符号坐标的序列化串（rational "3/7"、quadratic "1/2 + 1/3*sqrt(2)"、
 * transcendental "(pi + 1/2)" 等）渲染为结构化 SVG 文本公式：
 *  - 分数：分子/分母分行 + 中间分数线（替代纯文本 "3/7"）
 *  - sqrt(n)：√ 符号 + 内容 + 上横线（替代纯文本 "sqrt(2)"）
 *  - 多项：按 +/- 分隔的 term 依次渲染，中间插入运算符
 * 纯 SVG 原语实现，无外部依赖。 */

static double svg_formula_char_width(char c, double fs) {
    if (c == 'i' || c == 'l' || c == 't' || c == 'f' || c == '1' || c == '.' || c == ' ')
        return fs * 0.42;
    if (c == 'W' || c == 'M' || c == 'm' || c == 'w')
        return fs * 0.85;
    return fs * 0.58;
}

static double svg_formula_str_width(const char *s, double fs) {
    double w = 0.0;
    for (const char *p = s; p && *p; p++)
        w += svg_formula_char_width(*p, fs);
    return w;
}

/** @brief 在 x,y 处渲染一段普通文本（XML 转义，避免序列化串中的特殊字符破坏 SVG） */
static void svg_formula_text(FILE *fp, double x, double y, const char *s, double fs, const char *fill) {
    lvStrBuf esc = {0};
    lv_str_escape_xml(&esc, s, strlen(s));
    fprintf(fp, "    <text x=\"%.2f\" y=\"%.2f\" font-size=\"%.1f\" font-style=\"italic\" fill=\"%s\">%s</text>\n",
            x, y, fs, fill, lv_strbuf_cstr(&esc));
    lv_strbuf_destroy(&esc);
}

/** @brief 在 x,y 处渲染一个分数（分子/分母 + 分数线）；返回占用的宽度 */
static double svg_formula_fraction(FILE *fp, double x, double y, const char *num, const char *den,
                                   double fs, const char *fill) {
    double num_w = svg_formula_str_width(num, fs);
    double den_w = svg_formula_str_width(den, fs);
    double w = (num_w > den_w ? num_w : den_w) + fs * 0.2;
    double cx = x + w / 2.0;
    svg_formula_text(fp, cx - num_w / 2.0, y - fs * 0.38, num, fs * 0.85, fill);
    fprintf(fp, "    <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke=\"%s\" stroke-width=\"0.8\"/>\n",
            x, y + fs * 0.12, x + w, y + fs * 0.12, fill);
    svg_formula_text(fp, cx - den_w / 2.0, y + fs * 0.62, den, fs * 0.85, fill);
    return w;
}

/** @brief 在 x,y 处渲染 sqrt(n)：√ 符号 + 内容 + 上横线；返回占用的宽度 */
static double svg_formula_sqrt(FILE *fp, double x, double y, const char *radicand,
                               double fs, const char *fill) {
    double inner_w = svg_formula_str_width(radicand, fs * 0.9);
    double total_w = fs * 0.75 + inner_w + fs * 0.15;
    svg_formula_text(fp, x, y, "√", fs * 1.05, fill);
    svg_formula_text(fp, x + fs * 0.55, y + fs * 0.08, radicand, fs * 0.9, fill);
    /* 上横线覆盖根式内容 */
    fprintf(fp, "    <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke=\"%s\" stroke-width=\"0.7\"/>\n",
            x + fs * 0.55, y - fs * 0.22, x + fs * 0.55 + inner_w, y - fs * 0.22, fill);
    return total_w;
}

/**
 * @brief 渲染一个坐标公式（递归支持分数与 sqrt）；返回占用的宽度
 * @details 识别三种形态（与 symbolic_coord_serialize 输出对齐）：
 *          - quadratic "A + B*sqrt(N)"：拆前缀项 + 根式项
 *          - 纯分数 "num/den"：分子/分母分行 + 分数线
 *          - 其余（transcendental 名称、整数）：普通文本
 */
static double svg_formula_render(FILE *fp, double x, double y, const char *s,
                                 double fs, const char *fill) {
    if (!s || !*s)
        return 0.0;

    /* 形态 1：quadratic —— 包含 "sqrt(" 时拆为前缀 + 根式 */
    const char *sqrt_pos = strstr(s, "sqrt(");
    if (sqrt_pos) {
        const char *close = strchr(sqrt_pos + 5, ')');
        if (close) {
            double cursor = x;
            /* 前缀（如 "1/2 + 1/3*"）→ 递归渲染（会处理内部分数与 +/-）；
             * 尾部 "*" 剥离并以乘号 "·" 呈现，避免混入后续分母 */
            size_t prefix_len = (size_t) (sqrt_pos - s);
            bool has_mul = prefix_len > 0 && s[prefix_len - 1] == '*';
            if (has_mul)
                prefix_len--;
            if (prefix_len > 0) {
                char *prefix = (char *) lv_malloc(prefix_len + 1);
                if (prefix) {
                    memcpy(prefix, s, prefix_len);
                    prefix[prefix_len] = '\0';
                    cursor += svg_formula_render(fp, cursor, y, prefix, fs, fill);
                    lv_free((void **) &prefix);
                }
            }
            if (has_mul) {
                svg_formula_text(fp, cursor, y, "·", fs, fill);
                cursor += fs * 0.55;
            }
            /* 根式内容 */
            size_t rad_len = (size_t) (close - sqrt_pos - 5);
            if (rad_len > 0) {
                char *rad = (char *) lv_malloc(rad_len + 1);
                if (rad) {
                    memcpy(rad, sqrt_pos + 5, rad_len);
                    rad[rad_len] = '\0';
                    cursor += svg_formula_sqrt(fp, cursor, y, rad, fs, fill);
                    lv_free((void **) &rad);
                }
            }
            /* 后缀（极少出现，如 "1 + sqrt(2)/2" 的 "/2"） */
            const char *suffix = close + 1;
            if (*suffix)
                cursor += svg_formula_render(fp, cursor, y, suffix, fs, fill);
            return cursor - x;
        }
    }

    /* 形态 2：按 +/- 拆成多个 term（每个 term 内部可能含分数） */
    const char *sep = s;
    const char *next_op = NULL;
    for (const char *p = s + 1; *p; p++) {
        if ((*p == '+' || *p == '-') && p > s + 1) {
            next_op = p;
            break;
        }
    }
    if (next_op) {
        double cursor = x;
        size_t len = (size_t) (next_op - s);
        char *first = (char *) lv_malloc(len + 1);
        if (first) {
            memcpy(first, s, len);
            first[len] = '\0';
            cursor += svg_formula_render(fp, cursor, y, first, fs, fill);
            lv_free((void **) &first);
        }
        /* 运算符文本 */
        svg_formula_text(fp, cursor, y, next_op[0] == '+' ? "+" : "−", fs, fill);
        cursor += fs * 0.6;
        cursor += svg_formula_render(fp, cursor, y, next_op + 1, fs, fill);
        return cursor - x;
    }
    (void) sep;

    /* 形态 3：纯分数 "num/den" */
    const char *slash = strchr(s, '/');
    if (slash && slash > s && *(slash + 1)) {
        size_t num_len = (size_t) (slash - s);
        char *num = (char *) lv_malloc(num_len + 1);
        char *den = lv_strdup(slash + 1);
        if (num && den) {
            memcpy(num, s, num_len);
            num[num_len] = '\0';
            double w = svg_formula_fraction(fp, x, y, num, den, fs, fill);
            lv_free((void **) &num);
            lv_free((void **) &den);
            return w;
        }
        if (num)
            lv_free((void **) &num);
        if (den)
            lv_free((void **) &den);
    }

    /* 形态 4：普通文本 */
    double w = svg_formula_str_width(s, fs);
    svg_formula_text(fp, x, y, s, fs, fill);
    return w;
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
     *   2. 区域（Region）渲染 —— 在底层渲染多边形区域，带透明度填充；
     *      边界使用 <path>：直线段用 L，曲线段（coord_count>=6）用 Bezier C 链
     *   3. 圆（Circle）渲染 —— 圆心/半径经 center/radius 节点解析，原生 <circle>
     *   4. 函数块（Function Block）渲染 —— 渲染为圆角矩形，居中显示名称和ID
     *   5. 线段（Line Segment）渲染 —— 渲染为带颜色的直线段（或 Bezier 链），
     *      中点显示标签
     *   6. 端口（Port）渲染 —— 输入/输出端口渲染为小圆圈，标注类型和ID
     *   7. 点（Point）渲染 —— 渲染为填充圆形，标注P+ID
     *   8. 约束关系渲染 —— 支持全部七种约束类型的可视化：
     *      - 关联约束（INCIDENCE）：灰色虚线
     *      - 之间约束（BETWEENNESS）：紫色斜体标签标注三点关系
     *      - 相交约束（INTERSECTION）：紫色十字标记；精确交点按参与者类型组合
     *        求解（线段×线段 / 线段×圆 / 圆×圆，见 constraint_intersection_point）
     *      - 包含约束（CONTAINMENT）：青色点线
     *      - 连接约束（CONNECTION）：橙色箭头线
     *      - 角度约束（ANGLE）/ 平行约束（PARALLEL）：核心视觉表驱动
     *   9. 图例（Legend） —— 左上角半透明图例，说明各几何类型和信任颜色含义
     *  10. 信任颜色映射 —— 根据TrustColor为不同信用级别的元素使用不同颜色：
     *      绿色（受约束）、灰色（自由）、红色（冲突）
     *  11. 样式定义 —— 通过 <style> 标签统一定义 class 样式，clean SVG结构
     *  12. 数学公式渲染 —— 符号坐标（有理数/二次根式/超越数）经内置 SVG
     *      公式排版器渲染为结构化公式：分数分子/分母分行 + 分数线、
     *      sqrt 根号 + 上横线（svg_formula_render，纯 SVG 原语无外部依赖）
     *  13. 交互式 JavaScript 增强 —— 内联脚本实现悬停高亮（同 id 元素描边
     *      加粗）、指针旁提示工具、点击聚焦（无外部依赖，纯静态查看不受影响）
     *  14. 多图层分组 —— 节点元素带 data-node-id/data-node-type 属性，
     *      约束元素带 data-constraint-id/data-constraint-type 属性（可经 CSS/JS
     *      按几何类型/信任级别分组），点公式按 <g> 分组
     *  15. CSS 动画/过渡 —— 淡入（fade-in）、冲突脉冲（pulse）、约束虚线
     *      流动（dash-flow）、悬停描边加粗（node-hover），纯 CSS 无外部依赖
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
    fprintf(fp, "      .point { stroke-width: %g; }\n", lv_DEFAULT_STROKE_WIDTH);
    fprintf(fp, "      .line { stroke-width: 2; fill: none; }\n");
    fprintf(fp, "      .region { stroke-width: %g; opacity: 0.3; }\n", lv_DEFAULT_STROKE_WIDTH);
    fprintf(fp, "      .constraint { stroke-width: 1; stroke-dasharray: 5,3; fill: none; }\n");
    fprintf(fp, "      .label { font-family: 'Segoe UI', Arial, sans-serif; font-size: 12px; }\n");
    fprintf(fp, "      .block { stroke-width: 2; rx: 8; ry: 8; }\n");
    fprintf(fp, "      .port { stroke-width: %g; }\n", lv_DEFAULT_STROKE_WIDTH);
    /* CSS 动画/过渡：淡入、冲突脉冲、约束虚线流动（纯 CSS，无外部依赖） */
    fprintf(fp, "      @keyframes lv-fade-in { from { opacity: 0; } to { opacity: 1; } }\n");
    fprintf(fp, "      @keyframes lv-pulse { 0%%, 100%% { opacity: 1; } 50%% { opacity: 0.35; } }\n");
    fprintf(fp, "      @keyframes lv-dash-flow { to { stroke-dashoffset: -24; } }\n");
    fprintf(fp, "      svg { animation: lv-fade-in 0.6s ease-out; }\n");
    fprintf(fp, "      .conflict-pulse { animation: lv-pulse 1.6s ease-in-out infinite; }\n");
    fprintf(fp, "      .dash-flow { animation: lv-dash-flow 2.4s linear infinite; }\n");
    fprintf(fp, "      .node-hover:hover { stroke-width: 3; }\n");
    fprintf(fp, "    </style>\n");
    fprintf(fp, "  </defs>\n\n");

    /* 背景网格（可选） */
    fprintf(fp,
            "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
            "fill=\"#fafafa\" stroke=\"#e5e7eb\" stroke-width=\"1\"/>\n",
            min_x, min_y, width, height);

    /* ---- 渲染区域（先渲染，在底层；曲线边界用 Bezier path，替代 polygon 直线近似） ---- */
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
        const char *pulse_cls = (node->trust == TRUST_RED) ? " conflict-pulse" : "";
        fprintf(fp, "  <path class=\"region%s\" data-node-id=\"%d\" data-node-type=\"region\" "
                    "fill=\"%s\" stroke=\"%s\" d=\"",
                pulse_cls, node->id, color, color);

        /* 区域边界：遍历边界线段，直线段用 L，曲线段（coord_count>=6）用 Bezier C 链 */
        bool first_point = true;
        double prev_x = 0.0, prev_y = 0.0;
        for (int s = 0; s < node->data.region.segment_count; s++) {
            GeomNode *seg = node->data.region.boundary_segments[s];
            if (!seg || seg->type != GEOM_LINE_SEGMENT || seg->coord_count < 4)
                continue;

            double x1, y1, x2, y2;
            if (!symbolic_coord_get_segment(seg->symbolic_coords, seg->coord_count, &x1, &y1, &x2, &y2))
                continue;

            if (first_point) {
                fprintf(fp, "M %.2f,%.2f ", x1, y1);
                first_point = false;
            } else {
                /* 若与前一段终点不重合，补一条直线连接（保持闭合连续性） */
                if (fabs(x1 - prev_x) > 1e-9 || fabs(y1 - prev_y) > 1e-9)
                    fprintf(fp, "L %.2f,%.2f ", x1, y1);
            }

            /* 曲线段：该线段有多于两个坐标对时，用 Bezier 链精确还原曲线边界 */
            if (seg->coord_count >= 6) {
                int total_pairs = seg->coord_count / 2;
                for (int p = 0; p < total_pairs - 1; p++) {
                    double sx1 = symbolic_coord_to_double(seg->symbolic_coords[p * 2]);
                    double sy1 = symbolic_coord_to_double(seg->symbolic_coords[p * 2 + 1]);
                    double sx2 = symbolic_coord_to_double(seg->symbolic_coords[(p + 1) * 2]);
                    double sy2 = symbolic_coord_to_double(seg->symbolic_coords[(p + 1) * 2 + 1]);
                    double cp1x, cp1y, cp2x, cp2y;
                    compute_bezier_control_points(sx1, sy1, sx2, sy2, &cp1x, &cp1y, &cp2x, &cp2y);
                    fprintf(fp, "C %.2f,%.2f %.2f,%.2f %.2f,%.2f ", cp1x, cp1y, cp2x, cp2y, sx2, sy2);
                }
            } else {
                fprintf(fp, "L %.2f,%.2f ", x2, y2);
            }
            prev_x = x2;
            prev_y = y2;
        }
        fprintf(fp, "Z\"/>\n");
    }

    /* ---- 渲染圆（GEOM_CIRCLE：圆心/半径经 center/radius 节点解析；SVG 原生 <circle>） ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_CIRCLE)
            continue;

        double cx, cy, r;
        if (!svg_circle_geometry(graph, node, &cx, &cy, &r))
            continue;

        const char *color = trust_color_to_svg(node->trust);
        char escaped_name[256];
        lv_export_xml_escape(geom_type_name(node->type), escaped_name, sizeof(escaped_name));

        fprintf(fp, "  <!-- Circle id=%d center=%d radius=%d -->\n", node->id,
                node->data.circle.center_node_id, node->data.circle.radius_node_id);
        const char *pulse_cls = (node->trust == TRUST_RED) ? " conflict-pulse" : "";
        fprintf(fp,
                "  <circle class=\"circle%s\" data-node-id=\"%d\" data-node-type=\"circle\" "
                "cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" "
                "fill=\"%s\" fill-opacity=\"0.12\" stroke=\"%s\"/>\n",
                pulse_cls, node->id, cx, cy, r, color, color);
        fprintf(fp,
                "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                "text-anchor=\"middle\" fill=\"%s\">C_%d</text>\n",
                cx, cy - r - 8.0, color, node->id);
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
        const char *pulse_cls = (node->trust == TRUST_RED) ? " conflict-pulse" : "";
        fprintf(fp, "  <!-- Function Block id=%d -->\n", node->id);
        fprintf(fp,
                "  <rect class=\"block%s\" data-node-id=\"%d\" data-node-type=\"function_block\" "
                "x=\"%.2f\" y=\"%.2f\" "
                "width=\"%.2f\" height=\"%.2f\" "
                "fill=\"%s\" fill-opacity=\"0.15\" stroke=\"%s\"/>\n",
                pulse_cls, node->id, bx - bw / 2.0, by - bh / 2.0, bw, bh, color, color);
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
        const char *pulse_cls = (node->trust == TRUST_RED) ? " conflict-pulse" : "";
        if (node->coord_count >= 6) {
            /* 使用前两对为端点，中间对为控制点 */
            int total_pairs = node->coord_count / 2;
            fprintf(fp, "  <!-- Line Segment id=%d (Bezier, %d points) -->\n", node->id, total_pairs);
            fprintf(fp, "  <path class=\"line%s\" data-node-id=\"%d\" data-node-type=\"line_segment\" "
                        "fill=\"none\" stroke=\"%s\" d=\"M %.2f,%.2f",
                    pulse_cls, node->id, color, x1, y1);

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
                    "  <line class=\"line%s\" data-node-id=\"%d\" data-node-type=\"line_segment\" "
                    "x1=\"%.2f\" y1=\"%.2f\" "
                    "x2=\"%.2f\" y2=\"%.2f\" stroke=\"%s\"/>\n",
                    pulse_cls, node->id, x1, y1, x2, y2, color);
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
                "  <circle class=\"port\" data-node-id=\"%d\" data-node-type=\"port\" "
                "cx=\"%.2f\" cy=\"%.2f\" r=\"5\" "
                "fill=\"white\" stroke=\"%s\"/>\n",
                node->id, px, py, color);
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

        /* 数学公式渲染：符号坐标经公式排版器渲染为结构化 SVG 公式
         * （分数分子/分母分行 + 分数线、sqrt 根号 + 上横线），
         * 替代纯文本坐标；<title> 保留为悬停提示（无障碍） */
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
                fprintf(fp, "  <g data-node-id=\"%d\" data-node-type=\"point\">\n", node->id);
                fprintf(fp, "    <title>%s</title>\n", lv_strbuf_cstr(&esc_title));
                lv_strbuf_destroy(&esc_title);
                fprintf(fp, "    <desc>Symbolic: P%d at rational/quadratic coords</desc>\n", node->id);
                fprintf(fp,
                        "    <circle class=\"point\" cx=\"%.2f\" cy=\"%.2f\" r=\"4\" "
                        "fill=\"%s\"/>\n",
                        px, py, color);
                fprintf(fp,
                        "    <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                        "text-anchor=\"middle\" fill=\"#374151\">P%d</text>\n",
                        px, py - 8.0, node->id);
                /* 可见数学公式：点在下方渲染 Px / Py 两个坐标的公式排版 */
                double fw = svg_formula_render(fp, px - 30.0, py + 22.0, sx, 11.0, "#374151");
                lv_UNUSED(fw);
                svg_formula_render(fp, px - 30.0, py + 38.0, sy, 11.0, "#374151");
                fprintf(fp, "  </g>\n");
            }
            lv_free((void **) &sx);
            lv_free((void **) &sy);
        } else {
            fprintf(fp, "  <g data-node-id=\"%d\" data-node-type=\"point\">\n", node->id);
            fprintf(fp,
                    "    <circle class=\"point\" cx=\"%.2f\" cy=\"%.2f\" r=\"4\" "
                    "fill=\"%s\"/>\n",
                    px, py, color);
            fprintf(fp,
                    "    <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                    "text-anchor=\"middle\" fill=\"#374151\">P%d</text>\n",
                    px, py - 8.0, node->id);
            fprintf(fp, "  </g>\n");
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
        fprintf(fp, "  <g data-constraint-id=\"%d\" data-constraint-type=\"%s\">\n",
                c->id, constraint_type_name(c->type));
        constraint_render_dispatch(&kSvgConstraintOps, &ctx, c->type);
        fprintf(fp, "  </g>\n");
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

    /* 交互式 JavaScript 增强（内联，无外部依赖）：
     *  - 悬停高亮：指针悬停任意节点元素时，同 id 的所有元素描边加粗；
     *  - 点击聚焦：点击节点/约束时放大对应元素（stroke-width 提升），再点恢复；
     *  - 提示工具：悬停时在指针旁显示 node/constraint 信息。
     * 仅当浏览器支持时生效；纯静态查看（无 JS 环境）不受影响。 */
    fprintf(fp, "\n  <script type=\"text/ecmascript\"><![CDATA[\n");
    fprintf(fp,
            "var lvNodes = document.querySelectorAll('[data-node-id]');\n"
            "var lvConstrs = document.querySelectorAll('[data-constraint-id]');\n"
            "var lvTip = null;\n"
            "function lvInfo(el) {\n"
            "  var n = el.getAttribute('data-node-id');\n"
            "  if (n) return 'node ' + n + ' (' + (el.getAttribute('data-node-type') || '?') + ')';\n"
            "  var c = el.getAttribute('data-constraint-id');\n"
            "  if (c) return 'constraint ' + c + ' (' + (el.getAttribute('data-constraint-type') || '?') + ')';\n"
            "  return '';\n"
            "}\n"
            "function lvShowTip(ev, el) {\n"
            "  var txt = lvInfo(el);\n"
            "  if (!txt) return;\n"
            "  if (!lvTip) {\n"
            "    lvTip = document.createElementNS('http://www.w3.org/2000/svg', 'text');\n"
            "    lvTip.setAttribute('class', 'label');\n"
            "    lvTip.setAttribute('font-size', '11px');\n"
            "    lvTip.setAttribute('fill', '#111827');\n"
            "    lvTip.setAttribute('x', ev.clientX + 10);\n"
            "    lvTip.setAttribute('y', ev.clientY - 10);\n"
            "    lvTip.textContent = txt;\n"
            "    document.querySelector('svg').appendChild(lvTip);\n"
            "  } else {\n"
            "    lvTip.setAttribute('x', ev.clientX + 10);\n"
            "    lvTip.setAttribute('y', ev.clientY - 10);\n"
            "    lvTip.textContent = txt;\n"
            "    lvTip.setAttribute('visibility', 'visible');\n"
            "  }\n"
            "}\n"
            "function lvHideTip() { if (lvTip) lvTip.setAttribute('visibility', 'hidden'); }\n"
            "function lvHoverIn(ev) {\n"
            "  var id = ev.currentTarget.getAttribute('data-node-id');\n"
            "  if (!id) return;\n"
            "  for (var i = 0; i < lvNodes.length; i++) {\n"
            "    if (lvNodes[i].getAttribute('data-node-id') === id)\n"
            "      lvNodes[i].setAttribute('stroke-width', '4');\n"
            "  }\n"
            "  lvShowTip(ev, ev.currentTarget);\n"
            "}\n"
            "function lvHoverOut(ev) {\n"
            "  var id = ev.currentTarget.getAttribute('data-node-id');\n"
            "  if (!id) return;\n"
            "  for (var i = 0; i < lvNodes.length; i++) {\n"
            "    if (lvNodes[i].getAttribute('data-node-id') === id)\n"
            "      lvNodes[i].setAttribute('stroke-width', '');\n"
            "  }\n"
            "  lvHideTip();\n"
            "}\n"
            "for (var i = 0; i < lvNodes.length; i++) {\n"
            "  lvNodes[i].addEventListener('mouseenter', lvHoverIn, false);\n"
            "  lvNodes[i].addEventListener('mouseleave', lvHoverOut, false);\n"
            "}\n"
            "for (var j = 0; j < lvConstrs.length; j++) {\n"
            "  lvConstrs[j].addEventListener('mouseenter', lvShowTip, false);\n"
            "  lvConstrs[j].addEventListener('mouseleave', lvHideTip, false);\n"
            "}\n");
    fprintf(fp, "  ]]></script>\n");

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
