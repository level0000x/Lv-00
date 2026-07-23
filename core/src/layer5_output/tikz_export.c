/**
 * @file tikz_export.c
 * @brief TikZ/LaTeX 导出 —— 将约束图导出为 TikZ 绘图代码
 *
 * @details 从约束图中提取几何节点（点、线段、圆），生成 TikZ picture 描述。
 *          提供两种导出方式：
 *          - lv_tikz_export：导出到内存缓冲区
 *          - lv_tikz_export_file：导出到文件
 *          内部使用动态字符串构建器（TikzBuf）管理输出缓冲区。
 *
 * @author Lv-00 Project
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

/** 输出缓冲区初始容量（64 KB） */
#define TIKZ_BUF_INIT_CAP (64 * 1024)

/* ── 动态字符串构建器 ── */

/**
 * @brief TikZ 输出动态字符串缓冲区
 *
 * 用于在内存中逐步构建 TikZ 代码，避免频繁的 snprintf 边界检查。
 */
typedef struct {
    char  *data;   /**< 缓冲区数据指针 */
    size_t len;    /**< 当前数据长度 */
    size_t cap;    /**< 缓冲区总容量 */
} TikzBuf;

/**
 * @brief 初始化 TikZ 输出缓冲区
 *
 * @param b 缓冲区指针
 * @return true 成功，false 内存分配失败
 */
static bool tikz_buf_init(TikzBuf *b) {
    b->data = lv_malloc(TIKZ_BUF_INIT_CAP);
    if (!b->data) return false;
    b->data[0] = '\0';
    b->len = 0;
    b->cap = TIKZ_BUF_INIT_CAP;
    return true;
}

/**
 * @brief 销毁 TikZ 输出缓冲区，释放内存
 *
 * @param b 缓冲区指针
 */
static void tikz_buf_destroy(TikzBuf *b) {
    if (b) lv_free((void **)&b->data);
}

/**
 * @brief 向 TikZ 输出缓冲区追加字符串
 *
 * @details 当缓冲区容量不足时自动扩容（倍增），含溢出检测。
 *
 * @param b 缓冲区指针
 * @param s 要追加的字符串
 * @return true 成功，false 扩容失败或内存不足
 */
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

/**
 * @brief 将 [0,1] 范围的浮点颜色值转换为 [0,255] 范围的整数字节值
 *
 * @param c 浮点颜色值（范围 [0,1]）
 * @return 整数字节值（范围 [0,255]），自动钳位边界
 */
static int tikz_byte(float c) {
    int v = (int)(c * 255.0f + 0.5f);
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

/* ── 核心导出 ── */

/**
 * @brief 将约束图导出为 TikZ 代码到内存缓冲区
 *
 * @details 遍历约束图中的所有活跃节点，按类型生成对应的 TikZ 命令：
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

/**
 * @brief 将约束图导出为 TikZ 代码到文件
 *
 * @details 使用内部缓冲区（TikzBuf）在内存中构建完整的 TikZ 代码，
 *          然后一次性写入文件。相比 lv_tikz_export，此函数自动管理
 *          缓冲区大小，适合大型约束图导出。
 *
 * @param graph   约束图指针（ConstraintGraph*）
 * @param filename 目标文件路径
 * @return 成功时返回写入的字节数，失败返回 -1
 */
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
