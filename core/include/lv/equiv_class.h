#ifndef lv_EQUIV_CLASS_H
#define lv_EQUIV_CLASS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "constraint_graph.h" /* ConstraintGraph, GeomNode */
#include "symbolic_coord.h"   /* TrustColor, SymbolicCoord */

/* ── Equiv source type ── */
typedef enum {
    EQUIV_SOURCE_DIRECT = 0,
    EQUIV_SOURCE_COORD_EQUAL = 1,
    EQUIV_SOURCE_CONSTRAINT = 2,
    EQUIV_SOURCE_TRANSFORM = 3,
    EQUIV_SOURCE_CONJUGATE = 4
} EquivSourceType;

/* ── Equiv proof ── */
typedef struct EquivProof {
    EquivSourceType source;
    int node_a_id;
    int node_b_id;
    int deriving_constraint_id;
    int proof_step_id;
    TrustColor trust;
} EquivProof;

/* ── Equiv class ── */
typedef struct EquivClass {
    int representative_id;
    int member_count;
    int capacity;
    int *member_ids;
    EquivProof *proofs;
    int proof_count;
    int proof_capacity;
    TrustColor min_trust;
} EquivClass;

/* ── Equiv merge result ── */
typedef enum {
    EQUIV_MERGE_OK = 0,
    EQUIV_MERGE_CONFLICT = 1,
    EQUIV_MERGE_ALREADY_SAME = 2,
    EQUIV_MERGE_ERROR = -1
} EquivMergeResult;

/* ── Equiv class manager ── */
typedef struct EquivClassManager EquivClassManager;
typedef EquivClassManager lvEquivClass;

struct EquivClassManager {
    ConstraintGraph *graph;
    /* === 并查集 === */
    int *uf_parent;
    int *uf_rank;
    int uf_capacity;
    /* === 等价类 === */
    EquivClass *classes;
    int class_count;
    int class_capacity;
    int *node_to_class;
    int node_to_class_capacity;
    /* === 全局证明日志 === */
    EquivProof *proof_log;
    int proof_log_count;
    int proof_log_capacity;
    /* === Statistics === */
    int total_merges;
    int coord_merges;
    int constraint_derives;
    int algebraic_conjugates;
    int transform_merges;
    int rejected_merges;
    void *stream_ctx;
};

/* ── API ── */
EquivClassManager *equiv_manager_create(ConstraintGraph *graph);
void equiv_manager_destroy(EquivClassManager *mgr);

int equiv_manager_find(EquivClassManager *mgr, int node_id);
bool equiv_manager_are_equivalent(EquivClassManager *mgr, int a, int b);
bool equiv_are_equivalent(const EquivClassManager *mgr, int a, int b);
EquivMergeResult equiv_merge_classes(EquivClassManager *mgr, int node_a, int node_b, EquivSourceType source,
                                     int constraint_id, TrustColor trust);

int equiv_merge_by_coord(EquivClassManager *mgr);
int equiv_derive_from_constraints(EquivClassManager *mgr);
int equiv_merge_algebraic_conjugates(EquivClassManager *mgr);
int equiv_merge_by_transform(EquivClassManager *mgr);
bool equiv_prove_merge_valid(EquivClassManager *mgr, int class_a_idx, int class_b_idx);
int equiv_merge_all(EquivClassManager *mgr);
int equiv_find(const EquivClassManager *mgr, int node_id);
const EquivClass *equiv_get_class(const EquivClassManager *mgr, int node_id);
int equiv_class_count(const EquivClassManager *mgr);
void equiv_get_statistics(const EquivClassManager *mgr, int64_t *out_total, int64_t *out_coord, int64_t *out_derive,
                          int64_t *out_conjugate, int64_t *out_transform, int64_t *out_rejected);

int equiv_manager_get_class_size(EquivClassManager *mgr, int node_id);
int equiv_manager_get_representative(EquivClassManager *mgr, int node_id);
bool equiv_manager_sync_from_graph(EquivClassManager *mgr);

/* Legacy aliases */
lvEquivClass *lv_equiv_class_create(size_t n_elements);
void lv_equiv_class_destroy(lvEquivClass *ec);
int lv_equiv_class_union(lvEquivClass *ec, int a, int b);
int lv_equiv_class_find(lvEquivClass *ec, int a);

#ifdef __cplusplus
}
#endif
#endif
