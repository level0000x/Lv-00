#ifndef LV00_PARSER_SAFETY_H
#define LV00_PARSER_SAFETY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* Forward decl */
typedef enum {
    LV00_OK = 0,
    LV00_ERROR_INVALID_ARGUMENT = 1
} Lv00ErrorCode;

/** Validate input before parsing. Returns LV00_OK or error code. */
Lv00ErrorCode lv00_input_validate(const char *input, size_t len);

#ifdef __cplusplus
}
#endif
#endif
