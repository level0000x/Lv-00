/**
 * @file proof_navigator_internal.h
 * @brief Internal shared definitions for proof navigator module.
 */

#ifndef lv_PROOF_NAVIGATOR_INTERNAL_H
#define lv_PROOF_NAVIGATOR_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/proof.h"
#include "lv/lv_platform.h"

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

/* Defined in proof_navigator.c */
ConstraintGraph *deep_copy_graph(const ConstraintGraph *src);

#ifdef __cplusplus
}
#endif

#endif /* lv_PROOF_NAVIGATOR_INTERNAL_H */
