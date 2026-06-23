#ifndef LV00_GC_LANGUAGE_H
#define LV00_GC_LANGUAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/lv00.h"

int lv00_gc_parse(const char *source, void *engine);
const char *lv00_gc_error(void);
int lv00_gc_command_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GC_LANGUAGE_H */
