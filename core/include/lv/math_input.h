#ifndef lv_MATH_INPUT_H
#define lv_MATH_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv.h"

int lv_math_input_parse(const char *input, char *normalized, size_t buf_size);
int lv_math_input_detect_format(const char *input);

#ifdef __cplusplus
}
#endif

#endif /* lv_MATH_INPUT_H */
