/**
 * @file proof_navigator_internal.h
 * @brief Internal shared definitions for proof navigator module.
 */

#ifndef lv_PROOF_NAVIGATOR_INTERNAL_H
#define lv_PROOF_NAVIGATOR_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

#include "lv/proof.h"
#include "lv/lv_platform.h"
#include "lv/lv_strbuf.h"
#include "lv/stream.h"

/* ---- stream emit helper: 收敛 strbuf 四步样板 ---- */
static inline void nav_emit(StreamContext *ctx, StreamEventType event, const char *fmt, ...) {
    if (!ctx)
        return;
    va_list args;
    va_start(args, fmt);
    lvStrBuf sb = {0};
    lv_strbuf_vprintf(&sb, fmt, args);
    va_end(args);
    stream_emit_simple(ctx, event, sb.data, 0);
    lv_strbuf_destroy(&sb);
}

/* ---- breakpoint/axiom state (shared with misc sections) ---- */
typedef struct {
    int breakpoint_id;
    int current_step;
    int step_count;
    bool is_complete;
    ProofColor final_color;
} ProofBreakpointSnapshot;

#define MAX_BREAKPOINT_SNAPSHOTS 64

typedef struct ProofNavigatorState {
    ProofBreakpointSnapshot breakpoint_store[MAX_BREAKPOINT_SNAPSHOTS];
    bool axiom_locked;
    volatile int breakpoint_store_count;
    lvMutex breakpoint_mutex;
} ProofNavigatorState;

extern ProofNavigatorState s_proof_state;

#ifdef __cplusplus
}
#endif

#endif /* lv_PROOF_NAVIGATOR_INTERNAL_H */
