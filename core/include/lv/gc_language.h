#ifndef lv_GC_LANGUAGE_H
#define lv_GC_LANGUAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "lv/lv.h"

lv_PUBLIC_API int lv_gc_parse(const char *source, void *engine);
lv_PUBLIC_API const char *lv_gc_error(void);
lv_PUBLIC_API int lv_gc_command_count(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_GC_LANGUAGE_H */
