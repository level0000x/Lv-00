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
#include "lv/lv_utils.h"
#include "lv/lv_parse_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 内部辅助：简易 JSON 解析（提取数值字段）
 * ======================================================================== */

/**
 * @brief 从 JSON 字符串中提取 double 类型字段值
 * @param json   JSON 字符串（指针不动，仅读取）
 * @param field  字段名（如 "x"、"y"）
 * @param out    输出值
 * @return 0 成功，-1 未找到字段
 */
static int json_get_double(const char *json, const char *field, double *out)
{
    if (json == NULL || field == NULL || out == NULL) return -1;

    /* 搜索 "field" : 模式 */
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", field);
    const char *pos = strstr(json, pattern);
    if (pos == NULL) return -1;

    /* 跳过字段名和冒号 */
    pos = strchr(pos, ':');
    if (pos == NULL) return -1;
    pos++;

    /* 跳过空白 */
    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') {
        pos++;
    }

    lv_parse_double(pos, out);
    return 0;
}

/**
 * @brief 从 JSON 字符串中提取 int 类型字段值
 */
static int json_get_int(const char *json, const char *field, int *out)
{
    if (json == NULL || field == NULL || out == NULL) return -1;

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", field);
    const char *pos = strstr(json, pattern);
    if (pos == NULL) return -1;

    pos = strchr(pos, ':');
    if (pos == NULL) return -1;
    pos++;

    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') {
        pos++;
    }

    lv_parse_int(pos, out);
    return 0;
}

/* ========================================================================
 * 内部辅助：解析点和多边形
 * ======================================================================== */

/** 解析单个点 */
static lvGeoSpecPoint *parse_point(const char *json)
{
    if (json == NULL) return NULL;

    lvGeoSpecPoint *pt = (lvGeoSpecPoint *)calloc(1, sizeof(lvGeoSpecPoint));
    if (pt == NULL) return NULL;

    if (json_get_double(json, "x", &pt->x) != 0) {
        pt->x = 0.0;
    }
    if (json_get_double(json, "y", &pt->y) != 0) {
        pt->y = 0.0;
    }
    return pt;
}

/** 解析多边形（含点数组） */
static lvGeoSpecPolygon *parse_polygon(const char *json)
{
    if (json == NULL) return NULL;

    lvGeoSpecPolygon *poly = (lvGeoSpecPolygon *)calloc(1, sizeof(lvGeoSpecPolygon));
    if (poly == NULL) return NULL;

    int count = 0;
    if (json_get_int(json, "count", &count) != 0 || count <= 0) {
        count = 3; /* 默认三角形 */
    }

    poly->count = count;
    poly->pts = (lvGeoSpecPoint *)calloc((size_t)count, sizeof(lvGeoSpecPoint));
    if (poly->pts == NULL) {
        free(poly);
        return NULL;
    }

    /* 解析每个点坐标（简化：从 "points" 数组顺序读取） */
    const char *pos = strstr(json, "points");
    if (pos != NULL) {
        for (int i = 0; i < count; i++) {
            pos = strchr(pos, '{');
            if (pos == NULL) break;
            json_get_double(pos, "x", &poly->pts[i].x);
            json_get_double(pos, "y", &poly->pts[i].y);
            pos++;
        }
    }

    return poly;
}

/* ========================================================================
 * 公共 API 实现
 * ======================================================================== */

int lv_geo_spec_parse(const char *json, void *out)
{
    if (json == NULL || out == NULL) {
        /* NULL 指针安全检查 */
        return -1;
    }

    /* 根据 JSON 中的 type 字段判断规范类型 */
    if (strstr(json, "point") != NULL || strstr(json, "Point") != NULL) {
        lvGeoSpecPoint *pt = parse_point(json);
        if (pt == NULL) return -1;
        *(lvGeoSpecPoint **)out = pt;
        return 0;
    }

    if (strstr(json, "polygon") != NULL || strstr(json, "Polygon") != NULL) {
        lvGeoSpecPolygon *poly = parse_polygon(json);
        if (poly == NULL) return -1;
        *(lvGeoSpecPolygon **)out = poly;
        return 1;
    }

    /* 未知类型 */
    return -1;
}

void lv_geo_spec_destroy(void *spec)
{
    if (spec == NULL) return;

    /* 尝试作为多边形释放（含点数组需要额外释放） */
    lvGeoSpecPolygon *poly = (lvGeoSpecPolygon *)spec;
    if (poly->pts != NULL && poly->count > 0) {
        free(poly->pts);
    }
    free(spec);
}
