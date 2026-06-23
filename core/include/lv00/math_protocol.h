#ifndef LV00_MATH_PROTOCOL_H
#define LV00_MATH_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/lv00.h"

int lv00_math_protocol_encode(void *data, char *out, size_t buf_size);
int lv00_math_protocol_decode(const char *in, void *out);

#ifdef __cplusplus
}
#endif

#endif /* LV00_MATH_PROTOCOL_H */
