/**
 * @file interop_export_geojson.c
 * @brief 导出 —— GeoJSON 导出
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


int interop_export_geojson(const ConstraintGraph *graph, const InteropExportConfig *config) {
    if (!graph || !config)
        return lv_ERROR_INVALID_PARAM;

    FILE *fp = fopen(config->output_path, "w");
    if (!fp)
        return lv_ERROR_IO;

    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 4096)) {
        fclose(fp);
        return lv_ERROR_OUT_OF_MEMORY;
    }

    /* R02：基于实际图数据动态生成GeoJSON，而非硬编码占位数据 */
    lv_json_buf_append_raw(&buf, "{\n");
    lv_json_buf_append_raw(&buf, "  \"type\": \"FeatureCollection\",\n");
    lv_json_buf_append_raw(&buf, "  \"features\": [\n");

    int feature_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        const GeomNode *node = graph_get_node(graph, i);
        if (!node)
            continue;

        /* 仅导出点类型节点（线段和区域的坐标较为复杂） */
        if (node->type == GEOM_POINT && node->coord_count >= 2 && node->symbolic_coords != NULL) {
            if (feature_count > 0) {
                lv_json_buf_append_raw(&buf, ",\n");
            }
            /* 获取有理数坐标值，转为double */
            double x_val = 0.0, y_val = 0.0;
            SymbolicCoord *cx = node->symbolic_coords[0];
            SymbolicCoord *cy = node->symbolic_coords[1];

            if (cx)
                x_val = symbolic_coord_to_double(cx);
            if (cy)
                y_val = symbolic_coord_to_double(cy);

            lv_json_buf_append_raw(&buf, "    {\n");
            lv_json_buf_append_raw(&buf, "      \"type\": \"Feature\",\n");
            lv_json_buf_append_raw(&buf, "      \"geometry\": {\n");
            lv_json_buf_append_fmt(&buf, "        \"type\": \"Point\",\n");
            lv_json_buf_append_fmt(&buf, "        \"coordinates\": [%.15g, %.15g]\n", x_val, y_val);
            lv_json_buf_append_raw(&buf, "      },\n");
            lv_json_buf_append_raw(&buf, "      \"properties\": {\n");
            lv_json_buf_append_fmt(&buf, "        \"id\": %d,\n", node->id);
            lv_json_buf_append_raw(&buf, "        \"type\": \"point\"\n");
            lv_json_buf_append_raw(&buf, "      }\n");
            lv_json_buf_append_raw(&buf, "    }");
            feature_count++;
        }
    }

    /* 导出线段类型节点 */
    for (int i = 0; i < graph->node_count; i++) {
        const GeomNode *node = graph_get_node(graph, i);
        if (!node)
            continue;
        if (node->type != GEOM_LINE_SEGMENT)
            continue;

        /* 查找与线段关联的 INCIDENCE 约束以获取端点 */
        int constraint_indices[lv_MAX_CONSTRAINT_INDICES];
        int c_count = graph_find_constraints_involving(graph, node->id, constraint_indices, lv_MAX_CONSTRAINT_INDICES);

        /* 收集端点坐标 */
        double endpoints[4]; /* x1, y1, x2, y2 */
        int endpoint_found = 0;
        for (int j = 0; j < c_count && endpoint_found < 2; j++) {
            const Constraint *c = graph->constraints[constraint_indices[j]];
            if (!c || c->type != INCIDENCE)
                continue;
            for (int k = 0; k < c->participant_count; k++) {
                if (c->participants[k] == node->id)
                    continue;
                const GeomNode *ep = graph_get_node(graph, c->participants[k]);
                if (!ep || ep->type != GEOM_POINT)
                    continue;
                int idx = endpoint_found * 2;
                if (ep->coord_count >= 2 && ep->symbolic_coords) {
                    endpoints[idx] = symbolic_coord_to_double(ep->symbolic_coords[0]);
                    endpoints[idx + 1] = symbolic_coord_to_double(ep->symbolic_coords[1]);
                    endpoint_found++;
                }
            }
        }

        if (endpoint_found >= 2) {
            if (feature_count > 0)
                lv_json_buf_append_raw(&buf, ",\n");
            lv_json_buf_append_raw(&buf, "    {\n");
            lv_json_buf_append_raw(&buf, "      \"type\": \"Feature\",\n");
            lv_json_buf_append_raw(&buf, "      \"geometry\": {\n");
            lv_json_buf_append_raw(&buf, "        \"type\": \"LineString\",\n");
            /* 坐标输出 %.15g：保留 double 精度（口径同 Point 分支） */
            lv_json_buf_append_fmt(&buf, "        \"coordinates\": [[%.15g, %.15g], [%.15g, %.15g]]\n", endpoints[0],
                                   endpoints[1], endpoints[2], endpoints[3]);
            lv_json_buf_append_raw(&buf, "      },\n");
            lv_json_buf_append_raw(&buf, "      \"properties\": {\n");
            lv_json_buf_append_fmt(&buf, "        \"id\": %d,\n", node->id);
            lv_json_buf_append_raw(&buf, "        \"type\": \"line_segment\"\n");
            lv_json_buf_append_raw(&buf, "      }\n");
            lv_json_buf_append_raw(&buf, "    }");
            feature_count++;
        }
    }

    lv_json_buf_append_raw(&buf, "\n  ]\n");
    lv_json_buf_append_raw(&buf, "}\n");

    char *json = lv_json_buf_finalize(&buf);
    if (json) {
        fputs(json, fp);
        lv_free(json);
    }
    fclose(fp);
    return lv_OK;
}


