#ifndef LV00_ECOSYSTEM_H
#define LV00_ECOSYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/lv00.h"

int lv00_ecosystem_init(void);
void lv00_ecosystem_shutdown(void);
int lv00_ecosystem_register_module(const char *name, int layer);
int lv00_ecosystem_module_count(void);
const char *lv00_ecosystem_module_name(int idx);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ECOSYSTEM_H */
