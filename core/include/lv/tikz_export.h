#ifndef lv_TIKZ_EXPORT_H
#define lv_TIKZ_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv.h"

int lv_tikz_export(void *graph, char *out, size_t buf_size);
int lv_tikz_export_file(void *graph, const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* lv_TIKZ_EXPORT_H */
