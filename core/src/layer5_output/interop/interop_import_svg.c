/**
 * @file interop_import_svg.c
 * @brief SVG 导入（由 interop_import.c 拆分子模块）
 *
 * @details 解析 SVG 的 path/circle/line/rect/polyline/polygon 元素并采样
 *          导入约束图。公共入口 interop_import_svg 定义于本模块；
 *          svg_parse_circle 同时供 ggb_xml 子模块的圆导入复用（声明见
 *          interop_import_internal.h）。ggb_extract_attr_len 原位于
 *          interop_import.c 的 XML 辅助区，仅本模块引用，随拆分散落于此。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_file.h"

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/interop.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/geo_utils.h"
#include "lv/lv_numeric.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

#include "interop_import_internal.h"


/**
 * @brief 从 XML 开标签中提取属性值（返回原始缓冲区内的指针与长度，避免大值拷贝）
 *
 * 与 ggb_extract_attr 的区别：不复制到调用者缓冲区，而是返回指向
 * 标签文本内部的值指针。适用于 path d / points 等可能很长的属性，
 * 由调用者自行决定拷贝策略。
 *
 * @param tag_start 开标签起始位置（'<' 的位置）
 * @param tag_len   开标签长度（'<' 到 '>' 之间）
 * @param attr_name 属性名称
 * @param out_value [out] 输出属性值指针（位于 tag_start 内部）
 * @param out_len   [out] 输出属性值长度
 * @return true 找到属性，false 未找到
 */
static bool ggb_extract_attr_len(const char *tag_start, size_t tag_len, const char *attr_name,
                                 const char **out_value, size_t *out_len) {
    char search[128];
    int search_len = lv_snprintf(search, sizeof(search), "%s=\"", attr_name);
    if (search_len < 0)
        return false;

    char search_single[128];
    int ssl = lv_snprintf(search_single, sizeof(search_single), "%s='", attr_name);
    if (ssl < 0)
        return false;

    for (size_t i = 0; i + (size_t) search_len <= tag_len; i++) {
        bool is_double = (memcmp(tag_start + i, search, (size_t) search_len) == 0);
        bool is_single = (memcmp(tag_start + i, search_single, (size_t) ssl) == 0);

        if (is_double || is_single) {
            char quote = is_double ? '"' : '\'';
            size_t val_start = i + (is_double ? (size_t) search_len : (size_t) ssl);
            size_t j = 0;
            while (val_start + j < tag_len && tag_start[val_start + j] != quote)
                j++;
            *out_value = tag_start + val_start;
            *out_len = j;
            return true;
        }
    }
    return false;
}

/* ── SVG 解析器 ── */

/** @brief SVG 路径解析器状态 */
typedef struct {
    double cx, cy;               /* current position */
    double start_x, start_y;     /* start position of current sub-path */
    bool has_viewbox;            /* viewBox 是否已解析 */
    double viewbox_x, viewbox_y; /* viewBox 左上角坐标 */
    double viewbox_w, viewbox_h; /* viewBox 宽高 */
} SvgParserState;

/** @brief 跳过空白字符 */
#define SVG_SKIP_WS(s)                                                                     \
    do {                                                                                   \
        while (*(s) == ' ' || *(s) == '\t' || *(s) == '\n' || *(s) == '\r' || *(s) == ',') \
            (s)++;                                                                         \
    } while (0)

/** @brief 读取一个浮点数 */
static bool svg_parse_double(const char **s, double *val) {
    SVG_SKIP_WS(*s);
    if (**s == '\0')
        return false;
    char *end;
    *val = strtod(*s, &end);
    if (end == *s)
        return false;
    *s = end;
    SVG_SKIP_WS(*s);
    return true;
}

/** @brief 读取两个浮点数（坐标对） */
static bool svg_parse_coord(const char **s, double *x, double *y) {
    return svg_parse_double(s, x) && svg_parse_double(s, y);
}

/**
 * @brief 解析单个 SVG 路径命令并将采样点输出到数组
 *
 * 支持命令：M/m, L/l, C/c, Q/q, A/a, Z/z。
 * 贝塞尔曲线每段采样 10 个点，圆弧使用参数方程采样。
 *
 * @param cmd_char    命令字符（M/L/C/Q/A/Z 或小写）
 * @param s           指向路径字符串当前解析位置的指针
 * @param state       解析器状态（当前位置、起始点）
 * @param out_points  输出点数组 [x0,y0,x1,y1,...]
 * @param max_points  输出点数组最大容量（坐标对数）
 * @param out_count   [out] 实际输出的坐标对数
 * @param is_relative 是否为相对坐标命令（小写字母）
 * @return true 解析成功，false 解析失败
 */
/** @brief SVG path 命令处理器类型 */
typedef bool (*SvgPathHandler)(const char **s, SvgParserState *state, double *out_points,
                               int max_points, int *out_count, bool is_relative);

/** @brief 贝塞尔/圆弧采样点数 */
#define SVG_PATH_SAMPLES 10

/** @brief moveto：移动到绝对位置（M/m） */
static bool svg_path_moveto(const char **s, SvgParserState *state, double *out_points,
                            int max_points, int *out_count, bool is_relative) {
    double abs_x, abs_y;
    if (!svg_parse_coord(s, &abs_x, &abs_y))
        return false;
    if (is_relative) {
        abs_x += state->cx;
        abs_y += state->cy;
    }
    if (*out_count < max_points) {
        out_points[(*out_count) * 2] = abs_x;
        out_points[(*out_count) * 2 + 1] = abs_y;
        (*out_count)++;
    }
    state->cx = abs_x;
    state->cy = abs_y;
    state->start_x = abs_x;
    state->start_y = abs_y;
    return true;
}

/** @brief lineto：直线段（L/l） */
static bool svg_path_lineto(const char **s, SvgParserState *state, double *out_points,
                            int max_points, int *out_count, bool is_relative) {
    double abs_x, abs_y;
    if (!svg_parse_coord(s, &abs_x, &abs_y))
        return false;
    if (is_relative) {
        abs_x += state->cx;
        abs_y += state->cy;
    }
    if (*out_count < max_points) {
        out_points[(*out_count) * 2] = abs_x;
        out_points[(*out_count) * 2 + 1] = abs_y;
        (*out_count)++;
    }
    state->cx = abs_x;
    state->cy = abs_y;
    return true;
}

/** @brief cubic Bezier: C x1,y1 x2,y2 x,y（C/c） */
static bool svg_path_cubic_bezier(const char **s, SvgParserState *state, double *out_points,
                                  int max_points, int *out_count, bool is_relative) {
    double x1, y1, x2, y2, abs_x, abs_y;
    if (!svg_parse_coord(s, &x1, &y1) || !svg_parse_coord(s, &x2, &y2) || !svg_parse_coord(s, &abs_x, &abs_y))
        return false;
    if (is_relative) {
        x1 += state->cx;
        y1 += state->cy;
        x2 += state->cx;
        y2 += state->cy;
        abs_x += state->cx;
        abs_y += state->cy;
    }
    /* 采样贝塞尔曲线 */
    double x0 = state->cx, y0 = state->cy;
    for (int i = 1; i <= SVG_PATH_SAMPLES && *out_count < max_points; i++) {
        double t = (double) i / (double) SVG_PATH_SAMPLES;
        double t2 = t * t, t3 = t2 * t;
        double u = 1.0 - t, u2 = u * u, u3 = u2 * u;
        double px = u3 * x0 + 3.0 * u2 * t * x1 + 3.0 * u * t2 * x2 + t3 * abs_x;
        double py = u3 * y0 + 3.0 * u2 * t * y1 + 3.0 * u * t2 * y2 + t3 * abs_y;
        out_points[(*out_count) * 2] = px;
        out_points[(*out_count) * 2 + 1] = py;
        (*out_count)++;
    }
    state->cx = abs_x;
    state->cy = abs_y;
    return true;
}

/** @brief quadratic Bezier: Q x1,y1 x,y（Q/q） */
static bool svg_path_quadratic_bezier(const char **s, SvgParserState *state, double *out_points,
                                      int max_points, int *out_count, bool is_relative) {
    double x1, y1, abs_x, abs_y;
    if (!svg_parse_coord(s, &x1, &y1) || !svg_parse_coord(s, &abs_x, &abs_y))
        return false;
    if (is_relative) {
        x1 += state->cx;
        y1 += state->cy;
        abs_x += state->cx;
        abs_y += state->cy;
    }
    double qx0 = state->cx, qy0 = state->cy;
    for (int i = 1; i <= SVG_PATH_SAMPLES && *out_count < max_points; i++) {
        double t = (double) i / (double) SVG_PATH_SAMPLES;
        double u = 1.0 - t;
        double px = u * u * qx0 + 2.0 * u * t * x1 + t * t * abs_x;
        double py = u * u * qy0 + 2.0 * u * t * y1 + t * t * abs_y;
        out_points[(*out_count) * 2] = px;
        out_points[(*out_count) * 2 + 1] = py;
        (*out_count)++;
    }
    state->cx = abs_x;
    state->cy = abs_y;
    return true;
}

/** @brief arc: A rx,ry x-axis-rotation large-arc-flag sweep-flag x,y（A/a） */
static bool svg_path_arc(const char **s, SvgParserState *state, double *out_points,
                         int max_points, int *out_count, bool is_relative) {
    double rx, ry, rot, dx, dy;
    double laf_d, sf_d;
    if (!svg_parse_double(s, &rx) || !svg_parse_double(s, &ry) || !svg_parse_double(s, &rot) ||
        !svg_parse_double(s, &laf_d) || !svg_parse_double(s, &sf_d) || !svg_parse_coord(s, &dx, &dy))
        return false;
    lv_UNUSED(ry);
    lv_UNUSED(rot);
    lv_UNUSED(laf_d); /* parsed for future SVG arc implementation */
    int sf = (int) round(sf_d);
    if (is_relative) {
        dx += state->cx;
        dy += state->cy;
    }

    /* 使用中点公式计算椭圆弧采样 */
    double x_start = state->cx, y_start = state->cy;

    /* 简化参数方程：沿椭圆弧采样 */
    for (int i = 1; i <= SVG_PATH_SAMPLES && *out_count < max_points; i++) {
        double t = (double) i / (double) SVG_PATH_SAMPLES;
        /* 线性插值 + 圆弧偏移近似 */
        double lx = lv_lerp(x_start, dx, t);
        double ly = lv_lerp(y_start, dy, t);
        /* 添加圆弧离差 */
        double arc_angle = t * lv_PI;
        double bulge = sin(arc_angle) * (sf ? 1.0 : -1.0);
        double chord_len = geo_distance_2d(x_start, y_start, dx, dy);
        double bulge_factor = (chord_len > lv_GEO_LENGTH_GUARD) ? (rx / chord_len) * 0.5 : 0.0;
        double nx = -(dy - y_start) / (chord_len > lv_GEO_LENGTH_GUARD ? chord_len : 1.0);
        double ny = (dx - x_start) / (chord_len > lv_GEO_LENGTH_GUARD ? chord_len : 1.0);
        lx += nx * bulge * bulge_factor * chord_len * 0.5;
        ly += ny * bulge * bulge_factor * chord_len * 0.5;

        out_points[(*out_count) * 2] = lx;
        out_points[(*out_count) * 2 + 1] = ly;
        (*out_count)++;
    }
    /* 确保最后一点是终点 */
    if (*out_count < max_points) {
        out_points[(*out_count) * 2] = dx;
        out_points[(*out_count) * 2 + 1] = dy;
        (*out_count)++;
    }
    state->cx = dx;
    state->cy = dy;
    return true;
}

/** @brief closepath：画线回到当前子路径起点（Z/z） */
static bool svg_path_closepath(const char **s, SvgParserState *state, double *out_points,
                               int max_points, int *out_count, bool is_relative) {
    (void) s;
    (void) is_relative;
    if (*out_count < max_points) {
        out_points[(*out_count) * 2] = state->start_x;
        out_points[(*out_count) * 2 + 1] = state->start_y;
        (*out_count)++;
    }
    state->cx = state->start_x;
    state->cy = state->start_y;
    return true;
}

/** @brief SVG path 命令字符→处理器 查找表（替代 12 分支 switch；大小写映射到同组处理器） */
static const struct {
    char cmd;              /**< 命令字符 */
    SvgPathHandler handler; /**< 处理器 */
} kSvgPathHandlers[] = {
    {'M', svg_path_moveto},
    {'m', svg_path_moveto},
    {'L', svg_path_lineto},
    {'l', svg_path_lineto},
    {'C', svg_path_cubic_bezier},
    {'c', svg_path_cubic_bezier},
    {'Q', svg_path_quadratic_bezier},
    {'q', svg_path_quadratic_bezier},
    {'A', svg_path_arc},
    {'a', svg_path_arc},
    {'Z', svg_path_closepath},
    {'z', svg_path_closepath},
};

static bool svg_parse_path_command(char cmd_char, const char **s, SvgParserState *state, double *out_points,
                                   int max_points, int *out_count, bool is_relative) {
    *out_count = 0;

    /* 命令查表分发（替代 12 分支 switch；未命中返回 false，与 default 分支一致） */
    for (size_t i = 0; i < lv_ARRAY_SIZE(kSvgPathHandlers); i++) {
        if (kSvgPathHandlers[i].cmd == cmd_char)
            return kSvgPathHandlers[i].handler(s, state, out_points, max_points, out_count, is_relative);
    }
    return false;
}

/**
 * @brief 解析 SVG <circle> 元素并转换为采样点
 *
 * 将圆离散为 N 个采样点以便映射到约束图。
 */
int svg_parse_circle(double cx, double cy, double r, double *out_points, int max_points) {
    int count = 0;
    int samples = 32; /* 32个采样点近似圆 */
    for (int i = 0; i < samples && count < max_points; i++) {
        double angle = 2.0 * lv_PI * (double) i / (double) samples;
        out_points[count * 2] = cx + r * cos(angle);
        out_points[count * 2 + 1] = cy + r * sin(angle);
        count++;
    }
    return count;
}

/* ==================== SVG 导入辅助 ==================== */

/** @brief 单条 path 最多采集的采样点上限（坐标对数） */
#ifndef SVG_PATH_MAX_POINTS
#define SVG_PATH_MAX_POINTS 8192
#endif

/**
 * @brief 将采样点序列导入约束图（相邻点连线段，可闭合）
 *
 * 坐标先减去 viewBox 原点 (ox, oy)（平移映射到局部坐标系）。
 * 每点按 ggb_double_to_rational 转为 1e6 精度有理数 SymbolicCoord。
 *
 * @param graph      约束图
 * @param pts        采样点数组 [x0,y0,x1,y1,...]
 * @param n          采样点数量（坐标对数）
 * @param close_loop 是否闭合（末点连回首点）
 * @param ox         viewBox 原点 x（无 viewBox 时传 0）
 * @param oy         viewBox 原点 y（无 viewBox 时传 0）
 * @return 实际导入的点数
 */
static int svg_import_samples(ConstraintGraph *graph, const double *pts, int n, bool close_loop,
                              double ox, double oy) {
    int imported = 0;
    int first_id = -1, prev_id = -1;
    for (int i = 0; i < n; i++) {
        SymbolicCoord *cx = ggb_double_to_rational(pts[i * 2] - ox);
        SymbolicCoord *cy = ggb_double_to_rational(pts[i * 2 + 1] - oy);
        if (!cx || !cy) {
            if (cx)
                symbolic_coord_destroy(cx);
            if (cy)
                symbolic_coord_destroy(cy);
            continue;
        }
        SymbolicCoord *coords[2] = {cx, cy};
        AddNodeResult res = graph_add_point(graph, coords, 2);
        if (res != ADD_NODE_OK) {
            symbolic_coord_destroy(cx);
            symbolic_coord_destroy(cy);
            continue;
        }
        int node_id = (int) (graph->next_node_id - 1);
        if (first_id < 0)
            first_id = node_id;
        if (prev_id >= 0)
            graph_add_line_segment(graph, prev_id, node_id);
        prev_id = node_id;
        imported++;
    }
    if (close_loop && first_id >= 0 && prev_id >= 0 && first_id != prev_id)
        graph_add_line_segment(graph, prev_id, first_id);
    return imported;
}

/**
 * @brief 提取 SVG 标签名（'<' 与空白/'/'/'>' 之间）
 *
 * @param tag      标签起始（'<' 之后）
 * @param tag_len  标签长度
 * @param out      输出缓冲区
 * @param out_size 输出缓冲区大小
 */
static void svg_tag_name(const char *tag, size_t tag_len, char *out, size_t out_size) {
    size_t n = 0;
    while (n < out_size - 1 && n < tag_len) {
        char c = tag[n];
        if (c == ' ' || c == '>' || c == '/' || c == '\t' || c == '\n' || c == '\r')
            break;
        out[n] = c;
        n++;
    }
    out[n] = '\0';
}

/**
 * @brief 解析 points 属性（"x1,y1 x2,y2 ..."）到采样点数组
 *
 * @param text points 属性值（NUL 结尾）
 * @param pts  输出采样点数组 [x0,y0,x1,y1,...]
 * @param max  数组容量（坐标对数）
 * @return 解析到的坐标对数
 */
static int svg_parse_points_attr(const char *text, double *pts, int max) {
    const char *s = text;
    int n = 0;
    while (n < max) {
        double x, y;
        if (!svg_parse_coord(&s, &x, &y))
            break;
        pts[n * 2] = x;
        pts[n * 2 + 1] = y;
        n++;
        if (*s == '\0')
            break;
    }
    return n;
}

/**
 * @brief 解析 SVG <path d="..."> 并导入采样点（相邻点连线段）
 *
 * 支持 M/L/C/Q/A/Z（含相对小写），贝塞尔/圆弧由文件既有的
 * svg_parse_path_command 处理器采样。所有采样点顺序导入。
 *
 * @param graph  约束图
 * @param d      d 属性值（NUL 结尾）
 * @param count  [in/out] 累计导入点数
 * @param ox     viewBox 原点 x
 * @param oy     viewBox 原点 y
 */
static void svg_import_path(ConstraintGraph *graph, const char *d, int *count, double ox, double oy) {
    SvgParserState state;
    memset(&state, 0, sizeof(state));
    double *pts = (double *) lv_malloc(sizeof(double) * 2 * (size_t) SVG_PATH_MAX_POINTS);
    if (!pts)
        return;

    int total = 0;
    const char *s = d;
    while (*s) {
        SVG_SKIP_WS(s);
        char cmd = *s;
        if (cmd == '\0')
            break;
        bool is_relative = (cmd >= 'a' && cmd <= 'z');
        char upper = is_relative ? (char) (cmd - ('a' - 'A')) : cmd;
        if (strchr("MLCQAZ", upper)) {
            s++;
            /* 同一命令可能携带多组参数（如 "L 1,1 2,2 3,3"） */
            for (;;) {
                double out_points[SVG_PATH_SAMPLES * 2 + 16];
                int cnt = 0;
                const char *before = s;
                if (!svg_parse_path_command(cmd, &s, &state, out_points,
                                            (int) lv_ARRAY_SIZE(out_points), &cnt, is_relative))
                    break;
                /* 无参数命令（Z 闭合）不消费输入，处理一次即退出，防止死循环 */
                if (s == before)
                    break;
                for (int i = 0; i < cnt && total < SVG_PATH_MAX_POINTS; i++) {
                    pts[total * 2] = out_points[i * 2];
                    pts[total * 2 + 1] = out_points[i * 2 + 1];
                    total++;
                }
                SVG_SKIP_WS(s);
                if (*s == '\0' || *s == ',' || (*s >= '0' && *s <= '9') || *s == '-' || *s == '+' || *s == '.')
                    continue;
                break;
            }
        } else {
            s++;
        }
    }

    if (total > 0)
        *count += svg_import_samples(graph, pts, total, false, ox, oy);
    lv_free((void **) &pts);
}

int interop_import_svg(lvEngine *engine, const InteropImportConfig *config) {
    if (!engine || !config)
        return lv_ERROR_INVALID_PARAM;
    if (config->input_path[0] == '\0')
        return lv_ERROR_INVALID_PARAM;
    if (!engine->main_graph) {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_STATE, lv_ERROR_INVALID_STATE, "SVG导入失败：引擎的约束图未初始化");
    }

    size_t fsize = 0;
    char *svg = (char *) lv_file_read_all(config->input_path, &fsize);
    if (!svg) {
        lv_RETURN_ERROR_VAL(lv_ERROR_IO, lv_ERROR_IO,
                            "SVG导入失败：无法读取文件'%s'（不存在、为空或读取失败）", config->input_path);
    }
    size_t len = strlen(svg);

    ConstraintGraph *graph = engine->main_graph;
    int imported = 0;
    double ox = 0, oy = 0;
    bool has_viewbox = false;

    size_t pos = 0;
    while (pos < len) {
        const char *lt = memchr(svg + pos, '<', len - pos);
        if (!lt)
            break;
        size_t lt_off = (size_t) (lt - svg);
        const char *gt = memchr(lt, '>', len - lt_off);
        if (!gt)
            break;
        size_t gt_off = (size_t) (gt - svg);
        const char *tag = lt + 1;
        size_t tag_len = gt_off - lt_off;

        bool is_close = (tag_len > 0 && tag[0] == '/');
        bool is_decl = (tag_len > 0 && (tag[0] == '?' || tag[0] == '!'));
        if (!is_close && !is_decl) {
            char name[64];
            svg_tag_name(tag, tag_len, name, sizeof(name));

            if (lv_str_eq(name, "svg")) {
                char vb[128];
                if (ggb_extract_attr(svg + lt_off, tag_len, "viewBox", vb, sizeof(vb))) {
                    double w = 0, h = 0;
                    if (sscanf(vb, "%lf %lf %lf %lf", &ox, &oy, &w, &h) == 4)
                        has_viewbox = true;
                }
            } else if (lv_str_eq(name, "path")) {
                const char *d = NULL;
                size_t d_len = 0;
                if (ggb_extract_attr_len(svg + lt_off, tag_len, "d", &d, &d_len)) {
                    char *dbuf = (char *) lv_malloc(d_len + 1);
                    if (dbuf) {
                        lv_strlcpy_n(dbuf, d_len + 1, d, (size_t) d_len);
                        svg_import_path(graph, dbuf, &imported, has_viewbox ? ox : 0, has_viewbox ? oy : 0);
                        lv_free((void **) &dbuf);
                    }
                }
            } else if (lv_str_eq(name, "circle")) {
                char buf[64];
                double cx = 0, cy = 0, r = 0;
                if (ggb_extract_attr(svg + lt_off, tag_len, "cx", buf, sizeof(buf)) && lv_parse_double(buf, &cx) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "cy", buf, sizeof(buf)) && lv_parse_double(buf, &cy) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "r", buf, sizeof(buf)) && lv_parse_double(buf, &r) == 0 &&
                    r > 0.0) {
                    double pts[64];
                    int n = svg_parse_circle(cx, cy, r, pts, (int) lv_ARRAY_SIZE(pts));
                    imported += svg_import_samples(graph, pts, n, true, has_viewbox ? ox : 0, has_viewbox ? oy : 0);
                }
            } else if (lv_str_eq(name, "line")) {
                char buf[64];
                double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
                if (ggb_extract_attr(svg + lt_off, tag_len, "x1", buf, sizeof(buf)) && lv_parse_double(buf, &x1) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "y1", buf, sizeof(buf)) && lv_parse_double(buf, &y1) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "x2", buf, sizeof(buf)) && lv_parse_double(buf, &x2) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "y2", buf, sizeof(buf)) && lv_parse_double(buf, &y2) == 0) {
                    double pts[4] = {x1, y1, x2, y2};
                    imported += svg_import_samples(graph, pts, 2, false, has_viewbox ? ox : 0, has_viewbox ? oy : 0);
                }
            } else if (lv_str_eq(name, "rect")) {
                char buf[64];
                double rx = 0, ry = 0, rw = 0, rh = 0;
                if (ggb_extract_attr(svg + lt_off, tag_len, "x", buf, sizeof(buf)) && lv_parse_double(buf, &rx) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "y", buf, sizeof(buf)) && lv_parse_double(buf, &ry) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "width", buf, sizeof(buf)) && lv_parse_double(buf, &rw) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "height", buf, sizeof(buf)) && lv_parse_double(buf, &rh) == 0 &&
                    rw > 0.0 && rh > 0.0) {
                    double pts[8] = {rx, ry, rx + rw, ry, rx + rw, ry + rh, rx, ry + rh};
                    imported += svg_import_samples(graph, pts, 4, true, has_viewbox ? ox : 0, has_viewbox ? oy : 0);
                }
            } else if (lv_str_eq(name, "polyline") || lv_str_eq(name, "polygon")) {
                const char *pv = NULL;
                size_t pv_len = 0;
                if (ggb_extract_attr_len(svg + lt_off, tag_len, "points", &pv, &pv_len)) {
                    char *pbuf = (char *) lv_malloc(pv_len + 1);
                    if (pbuf) {
                        lv_strlcpy_n(pbuf, pv_len + 1, pv, (size_t) pv_len);
                        double *pts = (double *) lv_malloc(sizeof(double) * 2 * (size_t) SVG_PATH_MAX_POINTS);
                        if (pts) {
                            int n = svg_parse_points_attr(pbuf, pts, SVG_PATH_MAX_POINTS);
                            if (n > 0)
                                imported += svg_import_samples(graph, pts, n, lv_str_eq(name, "polygon"),
                                                              has_viewbox ? ox : 0, has_viewbox ? oy : 0);
                            lv_free((void **) &pts);
                        }
                        lv_free((void **) &pbuf);
                    }
                }
            }
        }
        pos = gt_off + 1;
    }

    lv_free((void **) &svg);

    if (imported == 0) {
        lv_set_error(lv_ERROR_PARSE,
                     "SVG导入完成但未找到任何可导入的几何元素（支持：path/circle/line/rect/polyline/polygon）");
    }
    return imported;
}
