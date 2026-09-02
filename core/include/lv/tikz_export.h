#ifndef lv_TIKZ_EXPORT_H
#define lv_TIKZ_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "lv/lv.h"

/** 将 [0,1] 浮点颜色值转换为 [0,255] 整数字节 */
static inline int tikz_byte(float c) {
    int v = (int)(c * 255.0f + 0.5f);
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

lv_PUBLIC_API int lv_tikz_export(void *graph, char *out, size_t buf_size);
lv_PUBLIC_API int lv_tikz_export_file(void *graph, const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* lv_TIKZ_EXPORT_H */
