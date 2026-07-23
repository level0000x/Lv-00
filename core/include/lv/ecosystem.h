#ifndef lv_ECOSYSTEM_H
#define lv_ECOSYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv.h"

int lv_ecosystem_init(void);
void lv_ecosystem_shutdown(void);
int lv_ecosystem_register_module(const char *name, int layer);
int lv_ecosystem_module_count(void);
const char *lv_ecosystem_module_name(int idx);

#ifdef __cplusplus
}
#endif

#endif /* lv_ECOSYSTEM_H */
