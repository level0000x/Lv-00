/**
 * @file interop_import_json.c
 * @brief GeoJSON 导入（由 interop_import.c 拆分子模块）
 *
 * @details 基于统一 lvJsonParser 解析 GeoJSON（Point / LineString /
 *          Polygon / MultiPoint / MultiLineString），导入约束图。
 *          公共入口 interop_import_geojson 定义于本模块。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include <string.h>

#include "lv/lv_file.h"

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/interop.h"
#include "lv/lv_json.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"


/* ── GeoJSON 解析辅助（基于统一 lvJsonParser，替代原手写 GJ_* 宏） ── */

#define GJ_MAX_FEATURES 4096
#define GJ_MAX_COORDS 8192

/* 解析 [[x,y],[x,y],...] 点列表，输出到 xs/ys（各至多 max 个），返回点数 */
static int gj_parse_coord_list(lvJsonParser *p, double *xs, double *ys, int max) {
    if (lv_json_peek(p) != '[')
        return 0;
    lv_json_next(p); /* 跳过 '[' */
    int n = 0;
    for (;;) {
        char c = lv_json_peek(p);
        if (c == ']') {
            lv_json_next(p);
            break;
        }
        if (c == ',') {
            lv_json_next(p);
            continue;
        }
        if (n >= max)
            break;
        double pair[2];
        size_t cnt = 0;
        if (!lv_json_parse_double_array(p, pair, 2, &cnt))
            break;
        if (cnt >= 2) {
            xs[n] = pair[0];
            ys[n] = pair[1];
        }
        n++;
        c = lv_json_peek(p);
        if (c == ',') {
            lv_json_next(p);
            continue;
        }
        if (c == ']') {
            lv_json_next(p);
            break;
        }
        break; /* 意外 token，停止 */
    }
    return n;
}

/* 解析 geometry 对象（p 位于 '{' 处）并把坐标导入约束图 */
static void gj_import_geometry(lvJsonParser *p, ConstraintGraph *graph, int *imported_count, int *prev_node_id,
                               double *coords_x, double *coords_y) {
    if (lv_json_peek(p) != '{')
        return;
    lv_json_next(p); /* 跳过 '{' */

    bool is_point = false, is_multipoint = false;
    bool is_linestring = false, is_multilinestring = false;
    bool is_polygon = false;
    const char *coords_val = NULL;

    /* 遍历 geometry 对象字段（键序无关：先收集 type，再记录 coordinates 值位置） */
    char *key = NULL;
    while (lv_json_parse_field(p, &key)) {
        if (lv_str_eq(key, "type") && lv_json_peek(p) == '"') {
            char *t = lv_json_parse_string(p);
            if (t) {
                if (lv_str_eq(t, "Point"))
                    is_point = true;
                else if (lv_str_eq(t, "MultiPoint"))
                    is_multipoint = true;
                else if (lv_str_eq(t, "LineString"))
                    is_linestring = true;
                else if (lv_str_eq(t, "MultiLineString"))
                    is_multilinestring = true;
                else if (lv_str_eq(t, "Polygon"))
                    is_polygon = true;
                lv_free((void **) &t);
            }
        } else if (lv_str_eq(key, "coordinates")) {
            coords_val = p->data + p->pos; /* 记录值起始位置 */
            lv_json_skip_value(p);
        } else {
            lv_json_skip_value(p);
        }
        lv_free((void **) &key);
    }
    if (lv_json_peek(p) == '}')
        lv_json_next(p);

    /* 类型未知或缺少 coordinates：无导入 */
    if (!is_point && !is_multipoint && !is_linestring && !is_multilinestring && !is_polygon)
        return;
    if (!coords_val || *coords_val != '[')
        return;

    /* 解析坐标数组 */
    int coord_count = 0;
    lvJsonParser cp;
    lv_json_parser_init(&cp, coords_val, strlen(coords_val));

    if (is_point) {
        /* Point: [x, y(, z)] — 与原实现一致仅取前两个元素 */
        double pair[4];
        size_t cnt = 0;
        if (lv_json_parse_double_array(&cp, pair, 4, &cnt) && cnt >= 2) {
            coords_x[0] = pair[0];
            coords_y[0] = pair[1];
            coord_count = 1;
        }
    } else if (is_multipoint || is_linestring) {
        /* MultiPoint / LineString: [[x,y],...] */
        coord_count = gj_parse_coord_list(&cp, coords_x, coords_y, GJ_MAX_COORDS);
    } else if (is_multilinestring) {
        /* MultiLineString: [[[x,y],...], ...] — 展平所有线段 */
        if (lv_json_peek(&cp) == '[') {
            lv_json_next(&cp);
            int n = 0;
            for (;;) {
                if (lv_json_peek(&cp) == ']') {
                    lv_json_next(&cp);
                    break;
                }
                if (lv_json_peek(&cp) == ',') {
                    lv_json_next(&cp);
                    continue;
                }
                int m = gj_parse_coord_list(&cp, coords_x + n, coords_y + n, GJ_MAX_COORDS - n);
                n += m;
                if (n >= GJ_MAX_COORDS)
                    break;
                if (lv_json_peek(&cp) == ',') {
                    lv_json_next(&cp);
                    continue;
                }
                break;
            }
            coord_count = n;
        }
    } else if (is_polygon) {
        /* Polygon: [[[x,y],...], [内环...]] — 只处理外环 */
        if (lv_json_peek(&cp) == '[') {
            lv_json_next(&cp);
            coord_count = gj_parse_coord_list(&cp, coords_x, coords_y, GJ_MAX_COORDS);
            lv_json_skip_value(&cp); /* 跳过剩余内环 */
        }
    }

    /* --- 将坐标导入到约束图（与原实现逻辑一致） --- */
    if (coord_count > 0) {
        int first_node_id = -1;
        *prev_node_id = -1;

        for (int i = 0; i < coord_count; i++) {
            /* 将 double 坐标转为有理数 SymbolicCoord */
            int64_t xn = (int64_t) (coords_x[i] * (double) INTEROP_COORD_DENOM_PRECISION_GEOJSON + (coords_x[i] >= 0 ? 0.5 : -0.5));
            int64_t yn = (int64_t) (coords_y[i] * (double) INTEROP_COORD_DENOM_PRECISION_GEOJSON + (coords_y[i] >= 0 ? 0.5 : -0.5));
            SymbolicCoord *cx = symbolic_coord_create_rational(xn, INTEROP_COORD_DENOM_PRECISION_GEOJSON);
            SymbolicCoord *cy = symbolic_coord_create_rational(yn, INTEROP_COORD_DENOM_PRECISION_GEOJSON);
            if (!cx || !cy) {
                if (cx)
                    symbolic_coord_destroy(cx);
                continue;
            }
            SymbolicCoord *coords[] = {cx, cy};
            AddNodeResult res = graph_add_point(graph, coords, 2);
            if (res != ADD_NODE_OK) {
                symbolic_coord_destroy(cx);
                symbolic_coord_destroy(cy);
                continue;
            }
            int node_id = graph->next_node_id - 1;
            if (node_id < 0)
                continue;

            if (first_node_id < 0)
                first_node_id = node_id;

            if (*prev_node_id >= 0 && (is_linestring || is_multilinestring || is_polygon)) {
                graph_add_line_segment(graph, *prev_node_id, node_id);
            }

            *prev_node_id = node_id;
            (*imported_count)++;
        }

        /* 闭合多边形 */
        if (is_polygon && first_node_id >= 0 && *prev_node_id >= 0 && first_node_id != *prev_node_id) {
            graph_add_line_segment(graph, *prev_node_id, first_node_id);
        }
    }
}

int interop_import_geojson(lvEngine *engine, const InteropImportConfig *config) {
    if (!engine || !config)
        return lv_ERROR_INVALID_PARAM;
    if (!engine->main_graph) {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_STATE, lv_ERROR_INVALID_STATE, "GeoJSON导入失败：引擎的约束图未初始化");
    }
    if (config->input_path[0] == '\0') {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM, "GeoJSON导入失败：未指定输入文件路径");
    }

    /* --- 读取文件（lv_file_read_all：失败/空文件返回 NULL，成功时缓冲以 NUL 结尾） --- */
    size_t fsize = 0;
    char *json = (char *) lv_file_read_all(config->input_path, &fsize);
    if (!json) {
        lv_RETURN_ERROR_VAL(lv_ERROR_IO, lv_ERROR_IO, "GeoJSON导入失败：无法读取文件'%s'（不存在、为空或读取失败）", config->input_path);
    }

    /* 统一 JSON 解析器（lvJsonParser，替代原手写 GJ_* 宏） */
    lvJsonParser p;
    lv_json_parser_init(&p, json, strlen(json));

    int imported_count = 0;
    ConstraintGraph *graph = engine->main_graph;

    /* --- 解析顶层 FeatureCollection 或 Feature --- */
    if (lv_json_peek(&p) != '{') {
        lv_free((void **) &json);
        lv_RETURN_ERROR_VAL(lv_ERROR_PARSE, lv_ERROR_PARSE, "GeoJSON导入失败：根元素不是JSON对象");
    }

    /* 查找 "type" 字段来识别根类型 */
    const char *type_val = lv_json_find_key(json, "type", 4);
    if (!type_val) {
        lv_free((void **) &json);
        lv_RETURN_ERROR_VAL(lv_ERROR_PARSE, lv_ERROR_PARSE, "GeoJSON导入失败：缺少type字段");
    }

    bool is_feature_collection = false;
    if (*type_val == '"') {
        lvJsonParser tp;
        lv_json_parser_init(&tp, type_val, strlen(type_val));
        char *tstr = lv_json_parse_string(&tp);
        if (tstr) {
            /* 与原实现一致的宽松前缀比较（strncmp 17 字符，不要求结尾引号） */
            if (lv_str_startswith(tstr, "FeatureCollection"))
                is_feature_collection = true;
            lv_free((void **) &tstr);
        }
    }

    /* 定位 "features" 数组 */
    if (is_feature_collection) {
        const char *features_val = lv_json_find_key(json, "features", 8);
        if (!features_val) {
            lv_free((void **) &json);
            lv_RETURN_ERROR_VAL(lv_ERROR_PARSE, lv_ERROR_PARSE, "GeoJSON导入失败：FeatureCollection缺少features数组");
        }
        if (*features_val != '[') {
            lv_free((void **) &json);
            lv_RETURN_ERROR_VAL(lv_ERROR_PARSE, lv_ERROR_PARSE, "GeoJSON导入失败：features不是数组");
        }
        lv_json_parser_init(&p, features_val, strlen(features_val));
        lv_json_next(&p); /* 跳过 '['，进入 features 数组 */
    }

    double coords_x[GJ_MAX_COORDS];
    double coords_y[GJ_MAX_COORDS];
    int prev_node_id = -1;

    while (imported_count < GJ_MAX_FEATURES) {
        char c = lv_json_peek(&p);
        if (c == ']' || c == '\0')
            break;
        if (c == ',') {
            lv_json_next(&p);
            continue;
        }
        if (c != '{')
            break;

        /* 进入一个 feature 对象 */
        lv_json_next(&p);

        /* 遍历 feature 对象字段，处理 geometry 子对象 */
        char *key = NULL;
        while (lv_json_parse_field(&p, &key)) {
            if (lv_str_eq(key, "geometry")) {
                gj_import_geometry(&p, graph, &imported_count, &prev_node_id, coords_x, coords_y);
            } else {
                lv_json_skip_value(&p);
            }
            lv_free((void **) &key);
        }
        if (lv_json_peek(&p) == '}')
            lv_json_next(&p);
    }

    lv_free((void **) &json);

    if (imported_count == 0) {
        lv_set_error(lv_ERROR_PARSE,
                     "GeoJSON导入完成但未找到任何有效的几何数据。"
                     "支持的类型：Point, LineString, Polygon, MultiPoint, MultiLineString");
    }

    return imported_count;
}
