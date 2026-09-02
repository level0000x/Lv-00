#ifndef lv_GEO_SPEC_H
#define lv_GEO_SPEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "lv/lv.h"

typedef struct {
    double x, y;
} lvGeoSpecPoint;

typedef struct {
    lvGeoSpecPoint *pts;
    int count;
} lvGeoSpecPolygon;

/**
 * @brief 解析 JSON 格式的几何规范数据
 * @param json 包含几何规范数据的 JSON 字符串
 * @param out 输出参数，接收解析后的几何规范结构体
 * @return 成功返回 0，失败返回非零错误码
 */
lv_PUBLIC_API int lv_geo_spec_parse(const char *json, void *out);

/**
 * @brief 释放几何规范结构体占用的内存
 * @param spec 指向待释放的几何规范结构体的指针
 */
lv_PUBLIC_API void lv_geo_spec_destroy(void *spec);

#ifdef __cplusplus
}
#endif

#endif /* lv_GEO_SPEC_H */
