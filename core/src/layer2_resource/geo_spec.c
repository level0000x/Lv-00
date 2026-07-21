/**
 * @file geo_spec.c
 * @brief 几何规范描述解析与释放 —— Layer2 资源管理层
 *
 * 提供几何构造规范（GeoSpec）的 JSON 解析与内存释放功能。
 * 当前支持点（Lv00GeoSpecPoint）和多边形（Lv00GeoSpecPolygon）的解析。
 *
 * JSON 格式约定：
 *   点:    { "type": "point", "x": 1.0, "y": 2.0 }
 *   多边形: { "type": "polygon", "count": 3, "points": [{"x":...,"y":...}, ...] }
 *
 * @version 1.0.0
 */

#include "lv00/geo_spec.h"
#include "lv00/lv00_parse_utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ================================================================
 *  内部辅助：简易 JSON 字段提取
 * ================================================================ */

/**
 * @brief 从 JSON 字符串中提取 double 类型字段值
 *
 * 搜索 "field" : <number> 模式，使用 atof 解析数值。
 *
 * @param json   JSON 字符串（指针不动，仅读取）
 * @param field  字段名（如 "x"、"y"）
 * @param out    输出值
 * @return 0 成功，-1 未找到字段或参数无效
 */
static int json_get_double(const char *json, const char *field, double *out)
{
    char pattern[64];
    const char *pos;

    if (!json || !field || !out) return -1;

    /* 构造搜索模式 "field" */
    snprintf(pattern, sizeof(pattern), "\"%s\"", field);
    pos = strstr(json, pattern);
    if (!pos) return -1;

    /* 跳过字段名和冒号 */
    pos = strchr(pos, ':');
    if (!pos) return -1;
    pos++;

    /* 跳过空白 */
    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') {
        pos++;
    }

    /* 安全解析数值 */
    if (*pos == '\0' || *pos == '}' || *pos == ',') {
        return -1;
    }

    lv00_parse_double(pos, out);
    return 0;
}

/**
 * @brief 从 JSON 字符串中提取 int 类型字段值
 *
 * @param json   JSON 字符串
 * @param field  字段名
 * @param out    输出值
 * @return 0 成功，-1 未找到字段或参数无效
 */
static int json_get_int(const char *json, const char *field, int *out)
{
    char pattern[64];
    const char *pos;

    if (!json || !field || !out) return -1;

    snprintf(pattern, sizeof(pattern), "\"%s\"", field);
    pos = strstr(json, pattern);
    if (!pos) return -1;

    pos = strchr(pos, ':');
    if (!pos) return -1;
    pos++;

    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') {
        pos++;
    }

    if (*pos == '\0' || *pos == '}' || *pos == ',') {
        return -1;
    }

    lv00_parse_int(pos, out);
    return 0;
}

/**
 * @brief 检测 JSON 中是否包含指定类型标识
 *
 * @param json      JSON 字符串
 * @param type_name 类型名称（如 "point"、"polygon"）
 * @return 包含返回 1，不包含返回 0
 */
static int json_has_type(const char *json, const char *type_name)
{
    char pattern[64];

    if (!json || !type_name) return 0;

    /* 搜索 "type":"type_name" 或 "type" : "type_name" 等变体 */
    snprintf(pattern, sizeof(pattern), "\"type\"");
    if (!strstr(json, pattern)) return 0;

    snprintf(pattern, sizeof(pattern), "\"%s\"", type_name);
    return (strstr(json, pattern) != NULL) ? 1 : 0;
}

/* ================================================================
 *  内部辅助：解析点和多边形
 * ================================================================ */

/**
 * @brief 解析单个几何点
 *
 * @param json JSON 字符串（含 "x" 和 "y" 字段）
 * @return 分配的 Lv00GeoSpecPoint 指针，失败返回 NULL
 */
static Lv00GeoSpecPoint *parse_point(const char *json)
{
    Lv00GeoSpecPoint *pt;

    if (!json) return NULL;

    pt = (Lv00GeoSpecPoint *)calloc(1, sizeof(Lv00GeoSpecPoint));
    if (!pt) return NULL;

    /* 解析坐标，缺失时默认 (0, 0) */
    if (json_get_double(json, "x", &pt->x) != 0) {
        pt->x = 0.0;
    }
    if (json_get_double(json, "y", &pt->y) != 0) {
        pt->y = 0.0;
    }

    return pt;
}

/**
 * @brief 解析多边形（含点数组）
 *
 * @param json JSON 字符串（含 "count" 和 "points" 数组）
 * @return 分配的 Lv00GeoSpecPolygon 指针，失败返回 NULL
 */
static Lv00GeoSpecPolygon *parse_polygon(const char *json)
{
    Lv00GeoSpecPolygon *poly;
    int count;
    const char *pos;

    if (!json) return NULL;

    poly = (Lv00GeoSpecPolygon *)calloc(1, sizeof(Lv00GeoSpecPolygon));
    if (!poly) return NULL;

    /* 解析顶点数量，默认三角形 */
    if (json_get_int(json, "count", &count) != 0 || count <= 0) {
        count = 3;
    }

    /* 安全上限检查 */
    if (count > 1024) {
        count = 1024;
    }

    poly->count = count;
    poly->pts = (Lv00GeoSpecPoint *)calloc((size_t)count, sizeof(Lv00GeoSpecPoint));
    if (!poly->pts) {
        free(poly);
        return NULL;
    }

    /* 解析每个点坐标（从 "points" 数组顺序读取） */
    pos = strstr(json, "points");
    if (pos) {
        int i;
        for (i = 0; i < count; i++) {
            pos = strchr(pos, '{');
            if (!pos) break;
            json_get_double(pos, "x", &poly->pts[i].x);
            json_get_double(pos, "y", &poly->pts[i].y);
            pos++;
        }
    }

    return poly;
}

/* ================================================================
 *  公共 API 实现
 * ================================================================ */

/**
 * @brief 解析几何规范 JSON 字符串
 *
 * 根据 JSON 中的 type 字段判断规范类型，分配并解析对应结构。
 * 解析结果通过 out 参数返回（二级指针）。
 *
 * @param json JSON 格式的几何规范字符串
 * @param out  输出指针地址（解析结果写入 *out）
 * @return 0 成功解析为点，1 成功解析为多边形，-1 失败
 */
int lv00_geo_spec_parse(const char *json, void *out)
{
    if (!json || !out) {
        return -1;
    }

    /* 检测点类型 */
    if (json_has_type(json, "point") || strstr(json, "Point") != NULL) {
        Lv00GeoSpecPoint *pt = parse_point(json);
        if (!pt) return -1;
        *(Lv00GeoSpecPoint **)out = pt;
        return 0;
    }

    /* 检测多边形类型 */
    if (json_has_type(json, "polygon") || strstr(json, "Polygon") != NULL) {
        Lv00GeoSpecPolygon *poly = parse_polygon(json);
        if (!poly) return -1;
        *(Lv00GeoSpecPolygon **)out = poly;
        return 1;
    }

    /* 未识别的类型 */
    return -1;
}

/**
 * @brief 释放几何规范结构内存
 *
 * 自动处理含动态点数组的多边形结构。
 * spec 为 NULL 时安全返回。
 *
 * @param spec 要释放的结构指针
 */
void lv00_geo_spec_destroy(void *spec)
{
    Lv00GeoSpecPolygon *poly;

    if (!spec) return;

    /* 尝试作为多边形释放（含动态点数组需要额外释放） */
    poly = (Lv00GeoSpecPolygon *)spec;
    if (poly->pts != NULL && poly->count > 0) {
        free(poly->pts);
        poly->pts = NULL;
        poly->count = 0;
    }
    free(spec);
}
