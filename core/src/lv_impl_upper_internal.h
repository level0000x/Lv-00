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
#include "lv/lv_upper_api.h"

#include "lv/constraint_graph.h"

/* ============================================================
 * 上层统一接口 · 对象表状态设施（定义于 lv_impl_upper.c）
 *
 * 上层便捷接口（visual_editor / view_synchronizer / text_code /
 * geom_evol / atp_backend / preset 各族）以「ID 句柄 + 内部对象表」
 * 模式管理跨层对象，供外部绑定（GUI/Python/CLI）与未来接入点使用。
 * ============================================================ */

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
} UpperState;

extern UpperState s_upper_state;

/* ---- cross-section internal APIs ----
 * 实现于 layer4_reasoning/proof/meta_verify.c（与 layer8_meta_verify/meta_verify.c
 * 同名不同模块，函数以 lv_graph_meta_verify_* 前缀区分） */
int lv_graph_meta_verify_completeness(const ConstraintGraph *graph);
int lv_graph_meta_verify_soundness(const ConstraintGraph *graph);
int lv_graph_meta_verify_differential(const ConstraintGraph *graph_a, const ConstraintGraph *graph_b);

#ifdef __cplusplus
}
#endif

#endif /* lv_IMPL_UPPER_INTERNAL_H */
