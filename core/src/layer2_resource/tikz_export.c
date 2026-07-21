/**
 * @file tikz_export.c
 * @brief TikZ 图形导出模块 —— Layer2 资源管理层
 *
 * 提供将几何图结构导出为 TikZ（LaTeX 绑图宏包）格式的功能。
 * 支持导出到内存缓冲区和文件两种模式。
 *
 * TikZ 输出格式：
 *   \begin{tikzpicture}
 *     \coordinate (P0) at (0.00, 0.00);
 *     \coordinate (P1) at (3.00, 0.00);
 *     \draw (P0) -- (P1);
 *     \fill (P0) circle (1.5pt) node[below] {$P_0$};
 *   \end{tikzpicture}
 *
 * @version 1.0.0
 */

#include "lv00/tikz_export.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ================================================================
 *  内部常量
 * ================================================================ */

/** TikZ 环境开头 */
#define TIKZ_HEADER     "\\begin{tikzpicture}\n"

/** TikZ 环境结尾 */
#define TIKZ_FOOTER     "\\end{tikzpicture}\n"

/** 默认点标记半径 */
#define TIKZ_POINT_RADIUS   "1.5pt"

/** 默认坐标精度 */
#define TIKZ_COORD_PRECISION    2

/** 最大安全缓冲区写入检查步长 */
#define TIKZ_SAFETY_MARGIN  256

/* ================================================================
 *  内部辅助函数
 * ================================================================ */

/**
 * @brief 安全地向缓冲区追加格式化字符串
 *
 * @param out      输出缓冲区当前位置
 * @param buf_end  缓冲区末尾（含终止符空间）
 * @param fmt      printf 格式字符串
 * @return 追加后的新指针位置，缓冲区不足返回 NULL
 */
static char *tikz_append(char *out, const char *buf_end, const char *fmt, ...)
{
    va_list args;
    int written;

    if (!out || !buf_end || out >= buf_end) {
        return NULL;
    }

    va_start(args, fmt);
    written = vsnprintf(out, (size_t)(buf_end - out), fmt, args);
    va_end(args);

    if (written < 0 || (size_t)written >= (size_t)(buf_end - out)) {
        return NULL;
    }
    return out + written;
}

/**
 * @brief 生成示例 TikZ 内容
 *
 * 当 graph 参数为 NULL 时，输出一个基础示例图。
 * 示例包含三个点构成的三角形。
 *
 * @param out      输出缓冲区
 * @param buf_end  缓冲区末尾
 * @return 写入后的新位置，失败返回 NULL
 */
static char *tikz_write_demo(char *out, const char *buf_end)
{
    /* 写入 TikZ 头部 */
    out = tikz_append(out, buf_end, "%s", TIKZ_HEADER);
    if (!out) return NULL;

    /* 定义坐标点 */
    out = tikz_append(out, buf_end,
        "  \\coordinate (P0) at (0.00, 0.00);\n"
        "  \\coordinate (P1) at (3.00, 0.00);\n"
        "  \\coordinate (P2) at (1.50, 2.60);\n");
    if (!out) return NULL;

    /* 绘制边 */
    out = tikz_append(out, buf_end,
        "  \\draw (P0) -- (P1) -- (P2) -- cycle;\n");
    if (!out) return NULL;

    /* 标记点 */
    out = tikz_append(out, buf_end,
        "  \\fill (P0) circle (%s) node[below] {$P_0$};\n"
        "  \\fill (P1) circle (%s) node[below] {$P_1$};\n"
        "  \\fill (P2) circle (%s) node[above] {$P_2$};\n",
        TIKZ_POINT_RADIUS, TIKZ_POINT_RADIUS, TIKZ_POINT_RADIUS);
    if (!out) return NULL;

    /* 写入 TikZ 尾部 */
    out = tikz_append(out, buf_end, "%s", TIKZ_FOOTER);
    if (!out) return NULL;

    return out;
}

/**
 * @brief 将缓冲区内容写入文件
 *
 * @param content  以 '\0' 结尾的字符串内容
 * @param filename 目标文件路径
 * @return 0 成功，-1 失败
 */
static int write_to_file(const char *content, const char *filename)
{
    FILE *fp;
    size_t len;
    size_t written;

    if (!content || !filename) {
        return -1;
    }

    fp = fopen(filename, "w");
    if (!fp) {
        return -1;
    }

    len = strlen(content);
    written = fwrite(content, 1, len, fp);
    fclose(fp);

    return (written == len) ? 0 : -1;
}

/* ================================================================
 *  公共 API 实现
 * ================================================================ */

/**
 * @brief 将几何图结构导出为 TikZ 格式字符串
 *
 * 将 graph 参数描述的几何图转换为 TikZ 绑图代码。
 * 当前为基础实现：graph 为 NULL 时输出示例三角形；
 * graph 非 NULL 时（预留接口）输出占位信息。
 *
 * @param graph     几何图句柄（当前预留接口，传 NULL 输出示例）
 * @param out       输出缓冲区
 * @param buf_size  输出缓冲区大小（字节）
 * @return 写入的字符数（不含终止符），失败返回 -1
 */
int lv00_tikz_export(void *graph, char *out, size_t buf_size)
{
    const char *buf_end;
    char *p;

    /* 参数检查 */
    if (!out || buf_size < 64) {
        return -1;
    }

    buf_end = out + buf_size;
    p = out;
    out[0] = '\0';

    if (!graph) {
        /* graph 为 NULL 时输出示例内容 */
        p = tikz_write_demo(p, buf_end);
        if (!p) return -1;
    } else {
        /* graph 非 NULL 时：预留接口，输出基础结构 */
        p = tikz_append(p, buf_end, "%s", TIKZ_HEADER);
        if (!p) return -1;

        p = tikz_append(p, buf_end,
            "  %% 从几何图结构导出 TikZ 节点\n"
            "  %% graph=%p, layers auto-detected\n", graph);
        if (!p) return -1;

        p = tikz_append(p, buf_end, "%s", TIKZ_FOOTER);
        if (!p) return -1;
    }

    return (int)(p - out);
}

/**
 * @brief 将几何图结构导出为 TikZ 文件
 *
 * 将 TikZ 代码写入指定文件。内部调用 lv00_tikz_export 生成代码，
 * 然后写入文件。
 *
 * @param graph    几何图句柄（当前预留接口，传 NULL 输出示例）
 * @param filename 目标文件路径（如 "output.tex"）
 * @return 0 成功，-1 失败
 */
int lv00_tikz_export_file(void *graph, const char *filename)
{
    char buf[8192];
    int len;

    if (!filename) {
        return -1;
    }

    /* 先生成到缓冲区 */
    len = lv00_tikz_export(graph, buf, sizeof(buf));
    if (len < 0) {
        return -1;
    }

    /* 写入文件 */
    return write_to_file(buf, filename);
}
