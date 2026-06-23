#ifndef LV00_STATUS_CODES_H
#define LV00_STATUS_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/lv00.h"

int lv00_status_is_success(int code);
int lv00_status_is_error(int code);
const char *lv00_status_message(int code);

#ifdef __cplusplus
}
#endif

#endif /* LV00_STATUS_CODES_H */
