#ifndef LV00_PARSER_SAFETY_H
#define LV00_PARSER_SAFETY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "error_codes.h"   /* Lv00ErrorCode */

/** Validate input before parsing. Returns LV00_OK or error code. */
Lv00ErrorCode lv00_input_validate(const char *input, size_t len);

#ifdef __cplusplus
}
#endif
#endif
