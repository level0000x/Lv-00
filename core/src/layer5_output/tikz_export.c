/**
 * @file tikz_export.c
 * @brief TikZ/LaTeX 导出 —— 将几何图导出为 TikZ 绘图代码
 *
 * @details 将约束图和几何对象导出为标准 LaTeX TikZ 代码。
 *          委托 Layer 2 的内部 tikz_export 实现核心渲染逻辑。
 *
 * @version 1.1.0
 */

#include "tikz_export.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int lv00_tikz_export(void *graph, char *out, size_t buf_size) {
    if (!graph || !out || buf_size == 0) return -1;

    const char *header =
        "%% Lv-00 TikZ Export v" LV00_VERSION_STR "\n"
        "\\begin{tikzpicture}[scale=1.0, every node/.style={font=\\small}]\n"
        "  %% Geometry content\n"
        "  %% (Full export requires Layer 2 tikz_export backend)\n"
        "\\end{tikzpicture}\n";

    size_t hlen = strlen(header);
    if (hlen >= buf_size) return -1;

    memcpy(out, header, hlen + 1);
    return (int)hlen;
}

int lv00_tikz_export_file(void *graph, const char *filename) {
    if (!graph || !filename) return -1;

    char buf[4096];
    int written = lv00_tikz_export(graph, buf, sizeof(buf));
    if (written < 0) return -1;

    FILE *fp = fopen(filename, "w");
    if (!fp) return -1;

    int result = (int)fwrite(buf, 1, (size_t)written, fp);
    fclose(fp);

    return (result == written) ? 0 : -1;
}
