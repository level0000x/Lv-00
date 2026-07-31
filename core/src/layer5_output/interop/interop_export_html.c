/**
 * @file interop_export_html.c
 * @brief 导出 —— HTML 导出
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

#include "debug.h"
#include "interop_export_internal.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"


int interop_export_html(const lvEngine *engine, const InteropExportConfig *config) {
    if (!engine || !config)
        return lv_ERROR_INVALID_PARAM;
    if (!config->output_path[0])
        return lv_ERROR_INVALID_PARAM;

    ConstraintGraph *graph = engine->main_graph;
    if (!graph)
        return lv_ERROR_INVALID_PARAM;

    /* ---- 1. 生成 SVG 到临时文件 ---- */
    char svg_temp_path[INTEROP_MAX_PATH_LEN];
    {
        const char *tmp_name = tmpnam(NULL);
        if (!tmp_name)
            return lv_ERROR_IO;
        lv_strlcpy(svg_temp_path, tmp_name, sizeof(svg_temp_path));
    }

    InteropExportConfig svg_cfg;
    memset(&svg_cfg, 0, sizeof(svg_cfg));
    lv_strlcpy(svg_cfg.output_path, svg_temp_path, sizeof(svg_cfg.output_path));

    int svg_ret = interop_export_svg(graph, &svg_cfg);
    if (svg_ret != lv_OK) {
        remove(svg_temp_path);
        return svg_ret;
    }

    /* ---- 2. 读取临时 SVG 文件内容 ---- */
    char *svg_content = NULL;
    long svg_size = 0;
    {
        FILE *svg_fp = fopen(svg_temp_path, "rb");
        if (!svg_fp) {
            remove(svg_temp_path);
            return lv_ERROR_IO;
        }
        fseek(svg_fp, 0, SEEK_END);
        svg_size = ftell(svg_fp);
        fseek(svg_fp, 0, SEEK_SET);
        if (svg_size > 0) {
            svg_content = (char *) lv_malloc((size_t) svg_size + 1);
            if (svg_content) {
                size_t read_bytes = fread(svg_content, 1, (size_t) svg_size, svg_fp);
                svg_content[read_bytes] = '\0';
            }
        }
        fclose(svg_fp);
        remove(svg_temp_path);
    }

    /* ---- 3. 构建 HTML ---- */
    FILE *fp = fopen(config->output_path, "w");
    if (!fp) {
        lv_free(svg_content);
        return lv_ERROR_IO;
    }

    int node_count = graph->node_count;
    int constraint_count = graph->constraint_count;

    fprintf(fp,
            "<!DOCTYPE html>\n"
            "<html lang=\"zh-CN\">\n"
            "<head>\n"
            "  <meta charset=\"UTF-8\">\n"
            "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
            "  <title>Lv-00 几何约束图导出</title>\n"
            "  <style>\n"
            "    * { margin: 0; padding: 0; box-sizing: border-box; }\n"
            "    body {\n"
            "      background: #1a1b26;\n"
            "      color: #c0caf5;\n"
            "      font-family: 'Consolas', 'Courier New', monospace;\n"
            "      padding: 24px;\n"
            "      min-height: 100vh;\n"
            "    }\n"
            "    h1 {\n"
            "      font-size: 20px;\n"
            "      font-weight: 600;\n"
            "      color: #7aa2f7;\n"
            "      margin-bottom: 16px;\n"
            "      letter-spacing: 0.5px;\n"
            "    }\n"
            "    .stats {\n"
            "      display: flex;\n"
            "      gap: 24px;\n"
            "      margin-bottom: 20px;\n"
            "      padding: 12px 16px;\n"
            "      background: #24283b;\n"
            "      border-radius: 8px;\n"
            "      border: 1px solid #3b4261;\n"
            "      font-size: 13px;\n"
            "    }\n"
            "    .stats span { color: #9ece6a; }\n"
            "    .stats .label { color: #a9b1d6; }\n"
            "    .svg-container {\n"
            "      background: #ffffff;\n"
            "      border-radius: 8px;\n"
            "      border: 1px solid #3b4261;\n"
            "      padding: 8px;\n"
            "      overflow: auto;\n"
            "      display: inline-block;\n"
            "    }\n"
            "    .footer {\n"
            "      margin-top: 16px;\n"
            "      font-size: 11px;\n"
            "      color: #565f89;\n"
            "    }\n"
            "  </style>\n"
            "</head>\n"
            "<body>\n"
            "  <h1>Lv-00 几何约束图</h1>\n"
            "  <div class=\"stats\">\n"
            "    <div><span class=\"label\">节点</span> <span>%d</span></div>\n"
            "    <div><span class=\"label\">约束</span> <span>%d</span></div>\n"
            "    <div><span class=\"label\">引擎版本</span> <span>%s</span></div>\n"
            "  </div>\n"
            "  <div class=\"svg-container\">\n",
            node_count, constraint_count, lv_VERSION_STRING);

    /* 嵌入 SVG（跳过 XML 声明行） */
    if (svg_content) {
        char *svg_body = svg_content;
        /* 跳过可选的 <?xml ...?> 行 */
        if (svg_body[0] == '<' && svg_body[1] == '?') {
            char *nl = strchr(svg_body, '\n');
            if (nl)
                svg_body = nl + 1;
        }
        fprintf(fp, "%s\n", svg_body);
        lv_free(svg_content);
    } else {
        fprintf(fp, "    <p>SVG 生成失败</p>\n");
    }

    fprintf(fp,
            "  </div>\n"
            "  <div class=\"footer\">\n"
            "    由 Lv-00 v%s 生成\n"
            "  </div>\n"
            "</body>\n"
            "</html>\n",
            lv_VERSION_STRING);

    fclose(fp);

    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "HTML 导出完成", 0);
    }

    return lv_OK;
}

/**
 * @brief 将约束图导出为 SVG 矢量图文件
 * @param graph  约束图指针
 * @param config 导出配置（output_path 指定输出文件路径）
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效，lv_ERROR_IO 文件错误
 */
