#ifndef lv_PARSER_SAFETY_H
#define lv_PARSER_SAFETY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "error_codes.h"   /* lvErrorCode */

/** Validate input before parsing. Returns lv_OK or error code. */
lvErrorCode lv_input_validate(const char *input, size_t len);

#ifdef __cplusplus
}
#endif
#endif
