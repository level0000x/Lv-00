/**
 * @file geo_spec.c
 * @brief 几何规范描述实现
 *
 * 定义几何构造的规范描述，支持 JSON 格式的解析与释放。
 * 当前支持点 (lvGeoSpecPoint) 和多边形 (lvGeoSpecPolygon) 的解析。
 *
 * @version 1.0.0
 */

#include "lv/geo_spec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_utils.h"
#include "lv_internal.h"

/* ========================================================================
 * 内部辅助：解析点和多边形
 * ======================================================================== */

/** 解析单个点 */
static lvGeoSpecPoint *parse_point(const char *json) {
    if (json == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "parse_point: json is NULL");

    lvGeoSpecPoint *pt = (lvGeoSpecPoint *) calloc(1, sizeof(lvGeoSpecPoint));
    if (pt == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "parse_point: calloc failed");

    if (!lv_json_get_double(json, "x", &pt->x)) {
        pt->x = 0.0;
    }
    if (!lv_json_get_double(json, "y", &pt->y)) {
        pt->y = 0.0;
    }
    return pt;
}

/** 解析多边形（含点数组） */
static lvGeoSpecPolygon *parse_polygon(const char *json) {
    if (json == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "parse_polygon: json is NULL");

    lvGeoSpecPolygon *poly = (lvGeoSpecPolygon *) calloc(1, sizeof(lvGeoSpecPolygon));
    if (poly == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "parse_polygon: calloc failed");

    int count = 0;
    if (!lv_json_get_int(json, "count", &count) || count <= 0) {
        count = 3; /* 默认三角形 */
    }

    poly->count = count;
    poly->pts = (lvGeoSpecPoint *) calloc((size_t) count, sizeof(lvGeoSpecPoint));
    if (poly->pts == NULL) {
        free(poly);
        return NULL;
    }

    /* 解析每个点坐标（简化：从 "points" 数组顺序读取） */
    const char *pos = strstr(json, "points");
    if (pos != NULL) {
        for (int i = 0; i < count; i++) {
            pos = strchr(pos, '{');
            if (pos == NULL)
                break;
            lv_json_get_double(pos, "x", &poly->pts[i].x);
            lv_json_get_double(pos, "y", &poly->pts[i].y);
            pos++;
        }
    }

    return poly;
}

/* ========================================================================
 * 公共 API 实现
 * ======================================================================== */

int lv_geo_spec_parse(const char *json, void *out) {
    if (json == NULL || out == NULL) {
        /* NULL 指针安全检查 */
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_geo_spec_parse: NULL param");
    }

    /* 根据 JSON 中的 type 字段判断规范类型 */
    {
        char type_buf[32];
        if (lv_json_get_string(json, "type", type_buf, sizeof(type_buf))) {
            if (strcmp(type_buf, "point") == 0 || strcmp(type_buf, "Point") == 0) {
                lvGeoSpecPoint *pt = parse_point(json);
                if (pt == NULL)
                    lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lv_geo_spec_parse: parse_point failed");
                *(lvGeoSpecPoint **) out = pt;
                return 0;
            }
            if (strcmp(type_buf, "polygon") == 0 || strcmp(type_buf, "Polygon") == 0) {
                lvGeoSpecPolygon *poly = parse_polygon(json);
                if (poly == NULL)
                    lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lv_geo_spec_parse: parse_polygon failed");
                *(lvGeoSpecPolygon **) out = poly;
                return 1;
            }
        }
    }

    /* 兼容旧格式：无 type 字段时退化到 strstr 匹配 */
    if (strstr(json, "Point") != NULL) {
        lvGeoSpecPoint *pt = parse_point(json);
        if (pt == NULL)
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lv_geo_spec_parse: parse_point failed");
        *(lvGeoSpecPoint **) out = pt;
        return 0;
    }
    if (strstr(json, "Polygon") != NULL) {
        lvGeoSpecPolygon *poly = parse_polygon(json);
        if (poly == NULL)
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lv_geo_spec_parse: parse_polygon failed");
        *(lvGeoSpecPolygon **) out = poly;
        return 1;
    }

    /* 未知类型 */
    lv_RETURN_ERROR(lv_ERROR_UNSUPPORTED, "lv_geo_spec_parse: unknown spec type");
}

void lv_geo_spec_destroy(void *spec) {
    if (spec == NULL)
        return;

    /* 尝试作为多边形释放（含点数组需要额外释放） */
    lvGeoSpecPolygon *poly = (lvGeoSpecPolygon *) spec;
    if (poly->pts != NULL && poly->count > 0) {
        free(poly->pts);
    }
    free(spec);
}
