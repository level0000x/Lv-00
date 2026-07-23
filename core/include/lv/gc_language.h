#ifndef lv_GC_LANGUAGE_H
#define lv_GC_LANGUAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv.h"

int lv_gc_parse(const char *source, void *engine);
const char *lv_gc_error(void);
int lv_gc_command_count(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_GC_LANGUAGE_H */
