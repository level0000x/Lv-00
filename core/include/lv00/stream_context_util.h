#ifndef LV00_STREAM_CONTEXT_UTIL_H
#define LV00_STREAM_CONTEXT_UTIL_H

#include "stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*StreamContextSetter)(StreamContext *ctx);

void stream_context_register_setter(StreamContextSetter setter);

#ifdef __cplusplus
}
#endif

#endif
