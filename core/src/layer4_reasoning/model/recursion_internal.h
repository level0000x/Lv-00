/**
 * @file recursion_internal.h
 * @brief Internal shared definitions for recursion module.
 */

#ifndef lv_RECURSION_INTERNAL_H
#define lv_RECURSION_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/recursion.h"
#include "lv/lv_internal.h"

/* Thread-local stream context (defined in recursion.c) */
extern lv_THREAD_LOCAL StreamContext *recursion_stream_ctx;

#ifdef __cplusplus
}
#endif

#endif /* lv_RECURSION_INTERNAL_H */
