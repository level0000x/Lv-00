#ifndef lv_ECOSYSTEM_H
#define lv_ECOSYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

lv_PUBLIC_API int lv_ecosystem_init(void);
lv_PUBLIC_API void lv_ecosystem_shutdown(void);
lv_PUBLIC_API int lv_ecosystem_register_module(const char *name, int layer);
lv_PUBLIC_API int lv_ecosystem_module_count(void);
lv_PUBLIC_API const char *lv_ecosystem_module_name(int idx);

#ifdef __cplusplus
}
#endif

#endif /* lv_ECOSYSTEM_H */
