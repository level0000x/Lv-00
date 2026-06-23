#ifndef LV00_MATH_INPUT_H
#define LV00_MATH_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/lv00.h"

int lv00_math_input_parse(const char *input, char *normalized, size_t buf_size);
int lv00_math_input_detect_format(const char *input);

#ifdef __cplusplus
}
#endif

#endif /* LV00_MATH_INPUT_H */
