/**
 * @file lv_impl_upper_internal.h
 * @brief Internal shared definitions for upper unified implementation.
 */

#ifndef lv_IMPL_UPPER_INTERNAL_H
#define lv_IMPL_UPPER_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "lv/atp_backend.h"
#include "lv/engine.h"
#include "lv/geom_evol.h"
#include "lv/meta_verify.h"
#include "lv/visual_editor.h"

#include "constraint_graph.h"

/* ---- cross-section internal APIs ---- */

int meta_verify_completeness(const ConstraintGraph *graph);
int meta_verify_soundness(const ConstraintGraph *graph);
int meta_verify_differential(const ConstraintGraph *graph_a, const ConstraintGraph *graph_b);

#ifdef __cplusplus
}
#endif

#endif /* lv_IMPL_UPPER_INTERNAL_H */
