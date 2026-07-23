/**
 * @file tikz_export.c
 * @brief TikZ/LaTeX 导出 —— 将约束图导出为 TikZ 绘图代码
 *
 * 从约束图中提取几何节点（点、线段、圆），生成 TikZ picture 描述。
 * 点渲染为 \fill circle，线段渲染为 \draw --，圆渲染为 \draw circle。
 *
 * @version 1.1.0
 */

#include "tikz_export.h"
#include "lv/constraint_graph.h"
#include "lv/symbolic_coord.h"
#include "lv/lv_utils.h"
#include "lv_internal.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* 输出缓冲区初始容量 */
#define TIKZ_BUF_INIT_CAP (64 * 1024)

/* ── 动态字符串构建器 ── */
typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} TikzBuf;

static bool tikz_buf_init(TikzBuf *b) {
    b->data = lv_malloc(TIKZ_BUF_INIT_CAP);
    if (!b->data) return false;
    b->data[0] = '\0';
    b->len = 0;
    b->cap = TIKZ_BUF_INIT_CAP;
    return true;
}

static void tikz_buf_destroy(TikzBuf *b) {
    if (b) lv_free((void **)&b->data);
}

static bool tikz_buf_append(TikzBuf *b, const char *s) {
    size_t slen = strlen(s);
    if (b->len + slen + 1 > b->cap) {
        size_t new_cap = b->cap * 2;
        if (new_cap < b->cap) return false; /* 溢出 */
        char *nd = lv_realloc(b->data, new_cap);
        if (!nd) return false;
        b->data = nd;
        b->cap = new_cap;
    }
    memcpy(b->data + b->len, s, slen + 1);
    b->len += slen;
    return true;
}

/* ── 颜色转换：将 [0,1] 浮点转为 TikZ 的 0-255 整数 ── */
static int tikz_byte(float c) {
    int v = (int)(c * 255.0f + 0.5f);
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

/* ── 核心导出 ── */

int lv_tikz_export(void *graph, char *out, size_t buf_size) {
    if (!graph || !out || buf_size == 0) return -1;

    ConstraintGraph *g = (ConstraintGraph *)graph;
    int written = 0;

    /* 写入 TikZ 头部 */
    int n = snprintf(out, buf_size,
        "%% Lv-00 TikZ Export\n"
        "\\begin{tikzpicture}[scale=1.0, x=1cm, y=1cm]\n");
    if (n < 0 || (size_t)n >= buf_size) return -1;
    written = n;

    /* 遍历所有节点，按类型导出 */
    for (int i = 0; i < g->node_count; i++) {
        GeomNode *node = g->nodes[i];
        if (!node || !node->is_active) continue;

        switch (node->type) {
        case GEOM_POINT: {
            if (node->coord_count >= 2 && node->symbolic_coords &&
                node->symbolic_coords[0] && node->symbolic_coords[1]) {
                double x = symbolic_coord_to_double(node->symbolic_coords[0]);
                double y = symbolic_coord_to_double(node->symbolic_coords[1]);
                n = snprintf(out + written, buf_size - (size_t)written,
                    "  \\fill (%.4f, %.4f) circle (2pt);\n", x, y);
                if (n < 0) return -1;
                written += n;
            }
            break;
        }
        case GEOM_LINE_SEGMENT: {
            if (node->coord_count >= 4 && node->symbolic_coords &&
                node->symbolic_coords[0] && node->symbolic_coords[1] &&
                node->symbolic_coords[2] && node->symbolic_coords[3]) {
                double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
                double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
                double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
                double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);
                n = snprintf(out + written, buf_size - (size_t)written,
                    "  \\draw (%.4f, %.4f) -- (%.4f, %.4f);\n", x1, y1, x2, y2);
                if (n < 0) return -1;
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
    n = snprintf(out + written, buf_size - (size_t)written,
        "\\end{tikzpicture}\n");
    if (n < 0) return -1;
    written += n;

    return written;
}

int lv_tikz_export_file(void *graph, const char *filename) {
    if (!graph || !filename) return -1;

    /* 先用缓冲区构建输出 */
    TikzBuf buf;
    if (!tikz_buf_init(&buf)) return -1;

    /* 写入头部 */
    tikz_buf_append(&buf,
        "% Lv-00 TikZ Export\n"
        "\\begin{tikzpicture}[scale=1.0, x=1cm, y=1cm]\n");

    ConstraintGraph *g = (ConstraintGraph *)graph;
    for (int i = 0; i < g->node_count; i++) {
        GeomNode *node = g->nodes[i];
        if (!node || !node->is_active) continue;

        switch (node->type) {
        case GEOM_POINT: {
            if (node->coord_count >= 2 && node->symbolic_coords &&
                node->symbolic_coords[0] && node->symbolic_coords[1]) {
                double x = symbolic_coord_to_double(node->symbolic_coords[0]);
                double y = symbolic_coord_to_double(node->symbolic_coords[1]);
                char line[128];
                snprintf(line, sizeof(line),
                    "  \\fill (%.4f, %.4f) circle (2pt);\n", x, y);
                tikz_buf_append(&buf, line);
            }
            break;
        }
        case GEOM_LINE_SEGMENT: {
            if (node->coord_count >= 4 && node->symbolic_coords &&
                node->symbolic_coords[0] && node->symbolic_coords[1] &&
                node->symbolic_coords[2] && node->symbolic_coords[3]) {
                double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
                double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
                double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
                double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);
                char line[128];
                snprintf(line, sizeof(line),
                    "  \\draw (%.4f, %.4f) -- (%.4f, %.4f);\n", x1, y1, x2, y2);
                tikz_buf_append(&buf, line);
            }
            break;
        }
        default:
            break;
        }
    }

    tikz_buf_append(&buf, "\\end{tikzpicture}\n");

    /* 写入文件 */
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        tikz_buf_destroy(&buf);
        return -1;
    }
    size_t written = fwrite(buf.data, 1, buf.len, fp);
    fclose(fp);
    tikz_buf_destroy(&buf);

    return (int)written;
}
