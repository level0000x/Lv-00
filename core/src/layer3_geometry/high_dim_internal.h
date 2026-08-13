/**
 * @file high_dim_internal.h
 * @brief Internal shared definitions for high-dim module.
 */

#ifndef lv_HIGH_DIM_INTERNAL_H
#define lv_HIGH_DIM_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "high_dim.h"
#include "lv_internal.h"

/* Thread-local stream context (defined in high_dim.c) */
extern lv_THREAD_LOCAL StreamContext *high_dim_stream_ctx;

#ifdef __cplusplus
}
#endif

#endif /* lv_HIGH_DIM_INTERNAL_H */
