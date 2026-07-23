#ifndef lv_STATUS_CODES_H
#define lv_STATUS_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv.h"

int lv_status_is_success(int code);
int lv_status_is_error(int code);
const char *lv_status_message(int code);

#ifdef __cplusplus
}
#endif

#endif /* lv_STATUS_CODES_H */
