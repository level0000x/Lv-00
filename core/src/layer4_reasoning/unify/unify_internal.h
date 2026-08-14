/**
 * @file unify_internal.h
 * @brief Internal shared definitions for unify module.
 */

#ifndef lv_UNIFY_INTERNAL_H
#define lv_UNIFY_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/unify.h"
#include "lv/lv_internal.h"
#include "lv/type_system.h"

/* Thread-local stream context (defined in unify.c) */
extern lv_THREAD_LOCAL StreamContext *unify_stream_ctx;

/* Internal helpers shared across split files */
bool match_ports(const ConstraintGraph *construction, const ConstraintGraph *proposition,
                 int *used_construction_ports, TypeSystem *ts);
int nodes_coords_equal(GeomNode *a, GeomNode *b);
uint64_t compute_node_coord_hash(GeomNode *node);

#ifdef __cplusplus
}
#endif

#endif /* lv_UNIFY_INTERNAL_H */
