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

/* ---- internal tables (defined in lv_impl_upper.c) ---- */
typedef struct {
    int64_t id;
    lvGeomEvol *evol;
} EvolEntry;

#define MAX_EVOL_TABLE 256

typedef struct {
    int64_t id;
    ATPBackendSolver *solver;
} ATPBackendSlot;

#define MAX_ATP_BACKEND_TABLE 256

typedef struct {
    int64_t task_id;           /* task unique id */
    int64_t backend_id;        /* index into atp_backend_table */
    ATPResultInfo result_info;
    int8_t completed;          /* 0=pending, 1=done */
} ATPTask;

#define MAX_ATP_TASK_TABLE 512
#define MAX_VISUAL_EDITOR_TABLE 64
#define MAX_VIEW_SYNC_TABLE 64
#define MAX_TEXT_CODE_TABLE 64

typedef struct {
    int64_t upper_id;
    EvolEntry evol_table[MAX_EVOL_TABLE];
    int evol_count;
    ATPBackendSlot atp_backend_table[MAX_ATP_BACKEND_TABLE];
    int atp_backend_count;
    ATPTask atp_task_table[MAX_ATP_TASK_TABLE];
    int atp_task_count;
    lvVisualEditor *visual_editor_table[MAX_VISUAL_EDITOR_TABLE];
    int visual_editor_count;
    lvViewSynchronizer *view_sync_table[MAX_VIEW_SYNC_TABLE];
    int view_sync_count;
    lvTextCodeView *text_code_table[MAX_TEXT_CODE_TABLE];
    int text_code_count;
    lvMetaVerifier *meta_verifier;
} UpperState;

extern UpperState s_upper_state;

/* ---- cross-section internal APIs ---- */
typedef struct lvOrchestrator lvOrchestrator;
lvOrchestrator *lv_orchestrator_create(lvEngine *ctx);
int64_t lv_orchestrator_run(lvOrchestrator *orch, lvEngine *ctx, const char *input);
int64_t lv_orchestrator_get_stage(const lvOrchestrator *orch);
int64_t lv_orchestrator_get_status(const lvOrchestrator *orch);
int64_t lv_orchestrator_get_report(const lvOrchestrator *orch, char *buf, int64_t buf_size);
void lv_orchestrator_destroy(lvOrchestrator *orch);

int64_t meta_verify_consistency(lvEngine *ctx);
int meta_verify_completeness(const ConstraintGraph *graph);
int meta_verify_soundness(const ConstraintGraph *graph);
int meta_verify_differential(const ConstraintGraph *graph_a, const ConstraintGraph *graph_b);

int64_t upper_interop_export_coq(lvEngine *ctx, int64_t proof_id, char *buf, int64_t buf_size);
int64_t interop_export_lean4(lvEngine *ctx, int64_t proof_id, char *buf, int64_t buf_size);
int64_t upper_interop_export_svg(lvEngine *ctx, int64_t graph_id, char *buf, int64_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* lv_IMPL_UPPER_INTERNAL_H */
