#ifndef LV00_TIKZ_EXPORT_H
#define LV00_TIKZ_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/lv00.h"

int lv00_tikz_export(void *graph, char *out, size_t buf_size);
int lv00_tikz_export_file(void *graph, const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* LV00_TIKZ_EXPORT_H */
