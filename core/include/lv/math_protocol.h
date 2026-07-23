#ifndef lv_MATH_PROTOCOL_H
#define lv_MATH_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv.h"

int lv_math_protocol_encode(void *data, char *out, size_t buf_size);
int lv_math_protocol_decode(const char *in, void *out);

#ifdef __cplusplus
}
#endif

#endif /* lv_MATH_PROTOCOL_H */
